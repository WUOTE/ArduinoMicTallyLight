/*
  Arduino Yellowtec Mic Tally Light - Local WiFi Server Implementation

  This version includes an HTTP server ONLY.
  If you need websocket connectivity, uncomment the WS code
  It uses an arduino_secrets.h file for credentials.
*/

#include "arduino_secrets.h"
#include <WiFiS3.h>
// #include <WebSocketsServer.h> // WebSocket library commented out

// =============================================================================
// WiFi Configuration
// =============================================================================
char ssid[] = SECRET_SSID;
char pass[] = SECRET_OPTIONAL_PASS;

// =============================================================================
// Pin and Hardware Configuration
// =============================================================================
const int dirPin = 13;    // Direction control
const int pwmPin = 11;    // Speed/Intensity control (PWM)
const int brakePin = 8;   // Brake control

// =============================================================================
// Tally Light Breathing Effect Lookup Table
// =============================================================================
unsigned long previousMillis = 0;

// milliseconds
const long interval = 12; // Milliseconds for breathing loop

/*
Instead of constantly calculating required light's intensity at any given point
a pre-generated lookup table is used.

The lowest intensity used is 20/255 because at lower levels
Mika mic tally light starts flickering unpleasantly.
My experiments show that 20 is the lowest usable value.
Changing the lowest value to 15, 20 and 25 still gives good results
while providing smaller change in luminocity. If you want to do just that --
use `Lookup table calculation\MyDistFunction.ipynb` and
`Lookup table calculation\MicLookupTable.xlsx`.

*/
const static int tallyTable[] = {
    20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  21,  21,  21,  21,  21,
    21,  21,  21,  22,  22,  22,  22,  22,  23,  23,  23,  23,  24,  24,  24,
    25,  25,  25,  26,  26,  27,  27,  28,  28,  29,  30,  30,  31,  32,  33,
    33,  34,  35,  36,  37,  38,  39,  40,  41,  43,  44,  45,  47,  48,  50,
    51,  53,  55,  56,  58,  60,  62,  64,  66,  68,  70,  73,  75,  77,  80,
    82,  85,  88,  90,  93,  96,  99,  102, 105, 108, 111, 114, 117, 121, 124,
    127, 131, 134, 138, 141, 144, 148, 151, 155, 159, 162, 166, 169, 173, 177,
    180, 183, 187, 190, 194, 197, 200, 204, 207, 210, 213, 216, 219, 222, 224,
    227, 230, 232, 235, 237, 239, 241, 243, 244, 246, 248, 249, 250, 251, 252,
    253, 254, 254, 255, 255, 255, 255, 255, 255, 255, 254, 254, 254, 254, 253,
    253, 252, 252, 251, 251, 250, 250, 249, 248, 248, 247, 246, 245, 244, 243,
    242, 241, 240, 239, 238, 237, 236, 234, 233, 232, 231, 229, 228, 226, 225,
    224, 222, 221, 219, 218, 216, 215, 213, 211, 209, 208, 206, 205, 203, 201,
    199, 198, 196, 194, 192, 190, 188, 187, 185, 183, 181, 179, 178, 176, 173,
    172, 170, 168, 166, 164, 162, 160, 159, 157, 154, 153, 151, 149, 147, 145,
    143, 142, 140, 138, 136, 134, 132, 130, 129, 127, 125, 123, 122, 120, 118,
    116, 114, 113, 111, 109, 108, 106, 105, 103, 101, 100, 98,  97,  95,  94,
    92,  91,  89,  88,  86,  85,  84,  82,  81,  79,  78,  77,  75,  74,  73,
    72,  71,  69,  68,  67,  66,  65,  64,  63,  62,  61,  60,  59,  58,  57,
    56,  55,  54,  53,  52,  51,  50,  50,  49,  48,  47,  46,  46,  45,  44,
    43,  43,  42,  41,  41,  40,  40,  39,  38,  38,  37,  37,  36,  36,  35,
    35,  34,  34,  34,  33,  33,  32,  32,  31,  31,  31,  30,  30,  30,  29,
    29,  28,  28,  28,  27,  27,  27,  27,  26,  26,  26,  26,  25,  25,  25,
    25,  25,  24,  24,  24,  24,  24,  24,  23,  23,  23,  23,  23,  23,  23,
    22,  22,  22,  22,  22,  22,  22,  22,  22,  22,  21,  21,  21,  21,  21,
    21,  21,  21,  21,  21,  21,  21,  21,  21,  20,  20,  20,  20,  20,  20,
    20,  20,  20,  20,  20,  20,  20,  20,  20,  20};
const int sizeOftallyTable = sizeof(tallyTable) / sizeof(tallyTable[0]);

// =============================================================================
// Network Configuration
// =============================================================================
int wifiStatus = WL_IDLE_STATUS;
WiFiServer httpServer(80);
// WebSocketsServer webSocket = WebSocketsServer(81); // WebSocket library commented out

// =============================================================================
// Global State
// =============================================================================
String tallyMode = "off";
String currentUSBMode = "off";

// USB Serial Control Variables
const byte numChars = 32;
char receivedChars[numChars];
boolean newData = false;

// Forward declarations
void setTallyMode(String newMode);
String parseJsonMode(String json);
// void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length); // WebSocket library commented out

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  Serial.begin(115200);
  delay(1500);

  pinMode(dirPin, OUTPUT);
  pinMode(pwmPin, OUTPUT);
  pinMode(brakePin, OUTPUT);
  setTallyMode("off");
  delay(100);

  Serial.println("Tally Light Local Server Initializing...");

  connectToWiFi();
  
  httpServer.begin();
  // webSocket.begin(); // WebSocket library commented out
  // webSocket.onEvent(webSocketEvent); // WebSocket library commented out

  Serial.println("<Arduino is ready>");
  Serial.print("HTTP server started at http://");
  Serial.println(WiFi.localIP());
  // Serial.print("WebSocket server started at ws://"); // WebSocket library commented out
  // Serial.print(WiFi.localIP()); // WebSocket library commented out
  // Serial.println(":81/"); // WebSocket library commented out
}

// =============================================================================
// MAIN LOOP
// =============================================================================
void loop() {
  // webSocket.loop(); // WebSocket library commented out
  handleNewClients();
  handleTallyLight();
  handleUSBSerial();
}

/* // WebSocket library commented out
// =============================================================================
// WebSocket Event Handler
// =============================================================================
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.print("[WebSocket] Client #");
      Serial.print(num);
      Serial.println(" disconnected.");
      break;
    case WStype_CONNECTED: {
      Serial.print("[WebSocket] Client #");
      Serial.print(num);
      Serial.println(" connected.");
      webSocket.sendTXT(num, "{\"status\":\"connected\"}");
      break;
    }
    case WStype_TEXT: {
      Serial.print("[WebSocket] Got text from #");
      Serial.print(num);
      Serial.print(": ");
      Serial.println((char*)payload);
      String message = (const char *)payload;
      String mode = parseJsonMode(message);
      if (mode != "") {
        setTallyMode(mode);
        String response = "{\"status\":\"success\",\"mode\":\"" + tallyMode + "\"}";
        webSocket.sendTXT(num, response);
      } else {
        webSocket.sendTXT(num, "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
      }
      break;
    }
    case WStype_BIN:
    case WStype_ERROR:
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
      break;
  }
}
*/

// =============================================================================
// Tally Light Logic
// =============================================================================
void handleTallyLight() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis;

    // Prioritize USB mode if set, otherwise use network mode
    String activeMode = (currentUSBMode != "off") ? currentUSBMode : tallyMode;

    if (activeMode == "idle" || activeMode == "hot")
    {
      static int currentIndex = 0;

      // Read intensity from lookup table
      int lightIntensity = tallyTable[currentIndex];

      // Set light intensity
      analogWrite(pwmPin, lightIntensity);

      // Increment index and wrap around
      currentIndex = (currentIndex + 1) % sizeOftallyTable;
    }
  }
}

void setTallyMode(String newMode) {
  Serial.print("Setting mode: ");
  Serial.println(newMode);

  if (newMode == "off") {
    digitalWrite(dirPin, HIGH);
    digitalWrite(brakePin, HIGH);
    analogWrite(pwmPin, 0);
  } else if (newMode == "idle") {
    digitalWrite(dirPin, HIGH);
    digitalWrite(brakePin, LOW);
  } else if (newMode == "hot") {
    digitalWrite(dirPin, LOW);
    digitalWrite(brakePin, LOW);
  } else if (newMode == "red") {
    digitalWrite(dirPin, LOW);
    digitalWrite(brakePin, LOW);
    analogWrite(pwmPin, 255);
  } else if (newMode == "white") {
    digitalWrite(dirPin, HIGH);
    digitalWrite(brakePin, LOW);
    analogWrite(pwmPin, 255);
  } else {
    Serial.println("Invalid mode ignored.");
    return;
  }
  tallyMode = newMode;
}

// =============================================================================
// HTTP Server Logic
// =============================================================================
void handleNewClients() {
  WiFiClient client = httpServer.available();
  if (client) {
    handleHttpClient(client);
  }
}

void handleHttpClient(WiFiClient client) {
  Serial.println("[HTTP] New client.");
  String request = "";
  while (client.connected()) {
    if (client.available()) {
      request = client.readStringUntil('\n');
      client.flush();
      break;
    }
  }

  Serial.print("[HTTP] Request: ");
  Serial.println(request);

  if (request.indexOf("GET / ") != -1) {
    String html = "<h1>Tally Light Status</h1><p>Mode: " + tallyMode + "</p>";
    sendHttpResponse(client, "200 OK", "text/html", html);
  } else if (request.indexOf("GET /status") != -1) {
    String json = "{\"mode\":\"" + tallyMode + "\",\"uptime\":\"" + String(millis()) + "\"}";
    sendHttpResponse(client, "200 OK", "application/json", json);
  } else if (request.indexOf("POST /tally") != -1) {
    String body = "";
    while(client.available()) { body += (char)client.read(); }
    int jsonStart = body.indexOf('{');
    String mode = (jsonStart != -1) ? parseJsonMode(body.substring(jsonStart)) : "";
    if (mode != "") {
      setTallyMode(mode);
      sendHttpResponse(client, "200 OK", "application/json", "{\"status\":\"success\",\"mode\":\"" + tallyMode + "\"}");
    } else {
      sendHttpResponse(client, "400 Bad Request", "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
    }
  } else {
    sendHttpResponse(client, "404 Not Found", "text/html", "<h1>404 Not Found</h1>");
  }

  client.stop();
  Serial.println("[HTTP] client disconnected.");
}

void sendHttpResponse(WiFiClient client, String code, String contentType, String content) {
  client.println("HTTP/1.1 " + code);
  client.println("Content-Type: " + contentType);
  client.println("Connection: close");
  client.println("Access-Control-Allow-Origin: *");
  client.print("Content-Length: ");
  client.println(content.length());
  client.println();
  client.println(content);
}

String parseJsonMode(String json) {
  int modeIndex = json.indexOf("\"mode\"");
  if (modeIndex == -1) return "";
  int colonIndex = json.indexOf(':', modeIndex);
  int quote1Index = json.indexOf('"', colonIndex);
  int quote2Index = json.indexOf('"', quote1Index + 1);
  if (colonIndex == -1 || quote1Index == -1 || quote2Index == -1) return "";
  return json.substring(quote1Index + 1, quote2Index);
}

// =============================================================================
// USB Serial Control (Fallback)
// =============================================================================
// an array to store the received data
void handleUSBSerial() {
  recvWithEndMarker();
  if (newData) {
    currentUSBMode.trim();
    setTallyMode(currentUSBMode);
    newData = false;
  }
}

// USB Serial Command Reception
void recvWithEndMarker() {
  static byte ndx = 0;
  char endMarker = '\n';
  char rc;
  while (Serial.available() > 0 && newData == false) {
    rc = Serial.read();
    if (rc != endMarker) {
      receivedChars[ndx] = rc;
      ndx++;
      if (ndx >= numChars) { ndx = numChars - 1; }
    } else {
      receivedChars[ndx] = '\0';
      ndx = 0;
      currentUSBMode = receivedChars;
      newData = true;
    }
  }
}

// =============================================================================
// WiFi & Network Utilities
// =============================================================================
void connectToWiFi() {
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    while (true);
  }
  while (wifiStatus != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    wifiStatus = WiFi.begin(ssid, pass);
    unsigned long start = millis();
    while(millis() - start < 10000 && wifiStatus != WL_CONNECTED) {
      delay(100);
      wifiStatus = WiFi.status();
    }
  }
  Serial.println("Connected to WiFi! Waiting for IP address...");

  // Wait for a valid IP address
  while (WiFi.localIP() == IPAddress(0,0,0,0)) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  
  printWiFiStatus();
}

void printWiFiStatus() {
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
  long rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI): ");
  Serial.print(rssi);
  Serial.println(" dBm");
}