#include "firmware.hpp"
#include "controller.hpp"
#include "protocol.hpp"
#include "../config.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#if TALLY_ENABLE_WIFI
#include <WiFiS3.h>
#include <Modem.h>
#endif

namespace tally {
namespace {
constexpr uint8_t kDirectionPin = 13;
constexpr uint8_t kPwmPin = 11;
constexpr uint8_t kBrakePin = 8;
constexpr uint32_t kFrameMs = 10;
Settings settings() {
  Settings s;
  s.periodMs = TALLY_BREATH_PERIOD_MS;
  s.redFloor = TALLY_RED_FLOOR;
  s.whiteFloor = TALLY_WHITE_FLOOR;
  return s;
}
Controller controller(settings());
SerialLine serialLine;
Output applied{Color::White, 0};
uint32_t lastFrame = 0;
uint32_t lastService = 0;
uint32_t maxServiceGap = 0;
const char* wifiState = "disabled";
char ipAddress[16] = "0.0.0.0";
char serialTx[1024];
size_t txHead = 0;
size_t txTail = 0;
size_t txSize = 0;
uint32_t droppedReplies = 0;

void render(bool force = false) {
  const uint32_t now = millis();
  if (!force && now - lastFrame < kFrameMs) return;
  lastFrame = now;
  const Output next = controller.output();
  if (!next.pwm) {
    if (applied.pwm || force) {
      analogWrite(kPwmPin, 0);
      digitalWrite(kBrakePin, HIGH);
    }
    applied.pwm = 0;
    return;
  }
  const bool colorChanged = next.color != applied.color;
  if (colorChanged) {
    // Disable the bridge before reversing polarity; never switch direction under power.
    analogWrite(kPwmPin, 0);
    digitalWrite(kBrakePin, HIGH);
    const bool high = (next.color == Color::Red) == static_cast<bool>(TALLY_RED_DIRECTION_HIGH);
    digitalWrite(kDirectionPin, high ? HIGH : LOW);
    applied.color = next.color;
  }
  if (!applied.pwm || colorChanged || force) digitalWrite(kBrakePin, LOW);
  if (next.pwm != applied.pwm || colorChanged || force) analogWrite(kPwmPin, next.pwm);
  applied = next;
}

void enqueueSerial(const char* text) {
  const size_t length = strlen(text);
  if (length + 1 > sizeof(serialTx) - txSize) { ++droppedReplies; return; }
  for (size_t i = 0; i <= length; ++i) {
    serialTx[txHead] = i == length ? '\n' : text[i];
    txHead = (txHead + 1) % sizeof(serialTx);
  }
  txSize += length + 1;
}

void flushSerial() {
  if (!Serial) { txHead = txTail = txSize = 0; return; }
  const int available = Serial.availableForWrite();
  if (available <= 0 || !txSize) return;
  size_t count = txSize < 64 ? txSize : 64;
  if (count > static_cast<size_t>(available)) count = static_cast<size_t>(available);
  if (count > sizeof(serialTx) - txTail) count = sizeof(serialTx) - txTail;
  const size_t sent = Serial.write(reinterpret_cast<const uint8_t*>(serialTx + txTail), count);
  txTail = (txTail + sent) % sizeof(serialTx);
  txSize -= sent;
}

void statusJson(char* out, size_t capacity, const char* id, bool changed) {
  // Newlib-nano printf configurations need not support %llu. Format this integer explicitly.
  char uptime[21];
  char* digit = uptime + sizeof(uptime) - 1;
  *digit = 0;
  uint64_t remaining = controller.uptimeMs();
  do { *--digit = static_cast<char>('0' + remaining % 10); remaining /= 10; } while (remaining);
  snprintf(out, capacity,
           "{\"ok\":true,\"id\":\"%s\",\"mode\":\"%s\",\"changed\":%s,"
           "\"source\":\"%s\",\"revision\":%lu,\"pwm\":%u,\"pwm_max\":4095,"
           "\"uptime_ms\":%s,\"wifi\":\"%s\",\"ip\":\"%s\","
           "\"max_service_gap_ms\":%lu,\"dropped_replies\":%lu}",
           id, modeName(controller.mode()), changed ? "true" : "false",
           sourceName(controller.source()), static_cast<unsigned long>(controller.revision()),
           static_cast<unsigned>(applied.pwm), digit,
           wifiState, ipAddress, static_cast<unsigned long>(maxServiceGap),
           static_cast<unsigned long>(droppedReplies));
}

void serialError(const char* error, const char* id = "") {
  char response[128];
  snprintf(response, sizeof(response), "{\"ok\":false,\"id\":\"%s\",\"error\":\"%s\"}", id, error);
  enqueueSerial(response);
}

void pollUsb() {
  if (serialLine.expire(millis())) serialError("line_timeout");
  for (unsigned budget = 64; budget && Serial.available() > 0; --budget) {
    const int byte = Serial.read();
    if (byte < 0) break;
    const LineEvent event = serialLine.feed(static_cast<char>(byte), millis());
    if (event == LineEvent::Invalid) { serialError("invalid_line"); continue; }
    if (event != LineEvent::Ready) continue;
    if (!*serialLine.text()) continue;  // Empty LF is also a transport resynchronization marker.
    const Command command = parseCommand(serialLine.text());
    if (command.kind == CommandKind::Invalid) { serialError("invalid_command", command.id); continue; }
    bool changed = false;
    if (command.kind == CommandKind::SetMode) changed = controller.setMode(command.mode, Source::Usb, millis());
    controller.tick(millis());
    render(true);  // Apply first, acknowledge second. Queries also report an up-to-date output.
    char response[384];
    statusJson(response, sizeof(response), command.id, changed);
    enqueueSerial(response);
  }
}

void serviceLightAndUsb() {
  const uint32_t now = millis();
  const uint32_t gap = now - lastService;
  if (gap > maxServiceGap) maxServiceGap = gap;
  lastService = now;
  controller.tick(now);
  pollUsb();
  render();
  flushSerial();
}

#if TALLY_ENABLE_WIFI
enum class NetworkStep { Init, Reset, Connect, Wait, StartServer, GetIp, Online, CloseClient, StopServer, Backoff, Disabled };
enum class ClientStep { Read, Write, Close };
NetworkStep networkStep = NetworkStep::Disabled;
ClientStep clientStep = ClientStep::Read;
WiFiServer server(80);
WiFiClient client;
HttpRequest request;
uint32_t lastNetworkPoll = 0;
uint32_t lastStatusPoll = 0;
uint32_t attemptStarted = 0;
uint32_t responseStarted = 0;
uint32_t lastResetAttempt = 0;
unsigned resetAttempts = 0;
char httpResponse[768];
size_t responseSize = 0;
size_t responseSent = 0;

void prepareResponse() {
  int status = request.error();
  bool changed = false;
  if (!status && !request.authorized()) status = 401;
  if (!status && request.path() == HttpPath::Other) status = 404;
  if (!status) {
    const bool get = request.method() == HttpMethod::Get &&
                     (request.path() == HttpPath::Root || request.path() == HttpPath::Status);
    const bool post = request.method() == HttpMethod::Post && request.path() == HttpPath::Tally;
    if (!get && !post) status = 405;
    else if (post) {
      Mode mode;
      if (!request.jsonContentType()) status = 415;
      else if (!parseModeJson(request.body(), mode)) status = 400;
      else changed = controller.setMode(mode, Source::Http, millis());
    }
  }
  controller.tick(millis());
  render(true);
  char body[384];
  if (!status) {
    status = 200;
    statusJson(body, sizeof(body), "", changed);
  } else {
    snprintf(body, sizeof(body), "{\"ok\":false,\"error\":\"%s\"}", httpReason(status));
  }
  const char* extra = status == 401 ? "WWW-Authenticate: Bearer\r\n" : "";
  if (status == 405) extra = request.path() == HttpPath::Tally ? "Allow: POST\r\n" : "Allow: GET\r\n";
  const int length = snprintf(httpResponse, sizeof(httpResponse),
    "HTTP/1.1 %d %s\r\nContent-Type: application/json\r\nContent-Length: %u\r\n"
    "Connection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\n%s\r\n%s",
    status, httpReason(status), static_cast<unsigned>(strlen(body)), extra, body);
  responseSize = length > 0 && static_cast<size_t>(length) < sizeof(httpResponse) ? static_cast<size_t>(length) : 0;
  responseSent = 0;
  responseStarted = millis();
  clientStep = responseSize ? ClientStep::Write : ClientStep::Close;
}

void pollHttp() {
  if (!client) {
    client = server.accept();  // One modem request, no wait for a complete HTTP request.
    if (client) {
      request.reset(millis(), TALLY_HTTP_TOKEN);
      clientStep = ClientStep::Read;
    }
    return;
  }
  if (clientStep == ClientStep::Close) { client.stop(); return; }
  if (clientStep == ClientStep::Write) {
    if (millis() - responseStarted >= 1500) { clientStep = ClientStep::Close; return; }
    const size_t remaining = responseSize - responseSent;
    const size_t count = remaining < 128 ? remaining : 128;
    const size_t sent = client.write(reinterpret_cast<const uint8_t*>(httpResponse + responseSent), count);
    responseSent += sent;
    if (!sent || responseSent == responseSize) clientStep = ClientStep::Close;
    return;
  }
  request.expire(millis());
  if (request.done()) { prepareResponse(); return; }
  // read(buffer, size) makes at most one modem receive request; no connected()/available() loop.
  uint8_t bytes[64];
  const int count = client.read(bytes, sizeof(bytes));
  request.expire(millis());  // A modem read may itself have crossed the request deadline.
  for (int i = 0; i < count; ++i) request.feed(static_cast<char>(bytes[i]));
  if (request.done()) prepareResponse();
}

void pollNetwork() {
  const uint32_t now = millis();
  if (networkStep == NetworkStep::Disabled || now - lastNetworkPoll < 5) return;
  lastNetworkPoll = now;
  switch (networkStep) {
    case NetworkStep::Init:
      if (now < 250) return;  // USB is usable immediately after boot.
      // WiFiS3's default begin retries three 500 ms resets and restores a 10 s RPC timeout.
      // Initialize UART without those retries, then do bounded handshakes on separate passes.
      // These are public Modem APIs in the pinned Renesas core 1.6.0.
      modem.begin(115200, 0);
      modem.timeout(TALLY_MODEM_TIMEOUT_MS);
      WiFi.setTimeout(0);  // Initiate association; poll status separately below.
      lastResetAttempt = now - 1000;
      networkStep = NetworkStep::Reset;
      break;
    case NetworkStep::Reset: {
      if (now - lastResetAttempt < 1000) return;
      lastResetAttempt = now;
      ++resetAttempts;
      std::string reply;
      modem.beginned = modem.write(PROMPT(_SOFTRESETWIFI), reply, "%s", CMD(_SOFTRESETWIFI));
      if (modem.beginned) {
        networkStep = NetworkStep::Connect;
        wifiState = "connecting";
      } else if (resetAttempts >= 3) {
        networkStep = NetworkStep::Disabled;
        wifiState = "unavailable";
      }
      break;
    }
    case NetworkStep::Connect:
      attemptStarted = now;
      wifiState = "connecting";
      // With timeout=0 the return value is not the eventual connection result.
      WiFi.begin(TALLY_WIFI_SSID, *TALLY_WIFI_PASSWORD ? TALLY_WIFI_PASSWORD : nullptr);
      lastStatusPoll = now;
      networkStep = NetworkStep::Wait;
      break;
    case NetworkStep::Wait:
      if (now - lastStatusPoll < 1000) return;
      lastStatusPoll = now;
      if (WiFi.status() == WL_CONNECTED) networkStep = NetworkStep::StartServer;
      else if (now - attemptStarted >= 15000) {
        attemptStarted = now;
        wifiState = "offline";
        networkStep = NetworkStep::Backoff;
      }
      break;
    case NetworkStep::StartServer:
      server.begin();
      lastStatusPoll = now - 250;
      networkStep = server ? NetworkStep::GetIp : NetworkStep::StopServer;
      break;
    case NetworkStep::GetIp: {
      if (now - lastStatusPoll < 250) return;
      lastStatusPoll = now;
      const IPAddress ip = WiFi.localIP();
      if (ip == IPAddress(0, 0, 0, 0)) {
        // Association can precede DHCP. Retry without blocking the application.
        if (now - attemptStarted >= 15000) networkStep = NetworkStep::StopServer;
        break;
      }
      snprintf(ipAddress, sizeof(ipAddress), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
      wifiState = "online";
      lastStatusPoll = now;
      networkStep = NetworkStep::Online;
      break;
    }
    case NetworkStep::Online:
      if (now - lastStatusPoll >= 1000) {
        lastStatusPoll = now;
        if (WiFi.status() != WL_CONNECTED) {
          wifiState = "offline";
          strcpy(ipAddress, "0.0.0.0");
          networkStep = NetworkStep::CloseClient;
        }
      } else pollHttp();
      break;
    case NetworkStep::CloseClient:
      if (client) client.stop();
      networkStep = NetworkStep::StopServer;
      break;
    case NetworkStep::StopServer:
      server.end();
      wifiState = "offline";
      strcpy(ipAddress, "0.0.0.0");
      attemptStarted = now;
      networkStep = NetworkStep::Backoff;
      break;
    case NetworkStep::Backoff:
      if (now - attemptStarted >= 15000) networkStep = NetworkStep::Connect;
      break;
    case NetworkStep::Disabled: break;
  }
}
#endif
}  // namespace

void setupFirmware() {
  // The Renesas core's pinMode(OUTPUT) clears the output latch: set levels AFTER it.
  // Hold enable low first, then brake while analogWrite starts its PWM timer.
  // The timer briefly starts at its default duty before applying the requested zero.
  pinMode(kPwmPin, OUTPUT);
  digitalWrite(kPwmPin, LOW);
  pinMode(kBrakePin, OUTPUT);
  digitalWrite(kBrakePin, HIGH);
  pinMode(kDirectionPin, OUTPUT);
  digitalWrite(kDirectionPin, TALLY_RED_DIRECTION_HIGH ? LOW : HIGH);
  analogWriteResolution(12);
  analogWrite(kPwmPin, 0);  // Keep the core's default ~490 Hz carrier.
  Serial.begin(115200);
  enqueueSerial("{\"event\":\"ready\",\"protocol\":1}");
#if TALLY_ENABLE_WIFI
  if (*TALLY_WIFI_SSID) {
    wifiState = "starting";
    networkStep = NetworkStep::Init;
  }
#endif
}

void loopFirmware() {
  serviceLightAndUsb();
#if TALLY_ENABLE_WIFI
  pollNetwork();
#endif
  serviceLightAndUsb();
}

}  // namespace tally
