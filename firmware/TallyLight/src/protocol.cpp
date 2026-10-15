#include "protocol.hpp"

#include <string.h>

namespace tally {
namespace {
bool whitespace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
void skipSpace(const char*& p) { while (whitespace(*p)) ++p; }
char lower(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c; }
bool equalI(const char* a, const char* b) {
  while (*a && *b && lower(*a) == lower(*b)) { ++a; ++b; }
  return *a == *b;
}
bool tokenChar(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || c == '_' || c == '-';
}
bool readJsonString(const char*& p, char* out, size_t capacity) {
  skipSpace(p);
  if (*p++ != '"') return false;
  size_t n = 0;
  while (*p && *p != '"') {
    if (!tokenChar(*p) || n + 1 >= capacity) return false;
    out[n++] = *p++;
  }
  out[n] = 0;
  if (*p != '"') return false;
  ++p;
  return true;
}
}  // namespace

LineEvent SerialLine::feed(char c, uint32_t now) {
  lastByte_ = now;
  if (c == '\n') {
    const bool invalid = discarding_;
    buffer_[size_] = 0;
    size_ = 0;
    discarding_ = false;
    return invalid ? LineEvent::Invalid : LineEvent::Ready;
  }
  if (!discarding_) {
    if (size_ + 1 >= sizeof(buffer_) || (static_cast<unsigned char>(c) < 32 && c != '\r' && c != '\t') ||
        static_cast<unsigned char>(c) > 126) {
      discarding_ = true;
    } else {
      buffer_[size_++] = c;
    }
  }
  return LineEvent::None;
}

bool SerialLine::expire(uint32_t now) {
  if (size_ && !discarding_ && now - lastByte_ >= 1000) {
    discarding_ = true;  // Discard the suffix through LF; never execute a truncated command.
    return true;
  }
  return false;
}

Command parseCommand(const char* line) {
  Command command;
  skipSpace(line);
  if (*line == '@') {
    ++line;
    size_t n = 0;
    while (tokenChar(*line) && n < sizeof(command.id) - 1) command.id[n++] = *line++;
    if (!n || (*line != ' ' && *line != '\t')) {
      command.id[0] = 0;
      return command;
    }
    skipSpace(line);
  }
  char word[16] = {};
  size_t n = 0;
  while (*line && !whitespace(*line)) {
    if (n + 1 >= sizeof(word)) return command;
    word[n++] = *line++;
  }
  skipSpace(line);
  if (*line) return command;
  if (parseMode(word, command.mode)) command.kind = CommandKind::SetMode;
  else if (equalI(word, "status")) command.kind = CommandKind::Status;
  else if (equalI(word, "ping")) command.kind = CommandKind::Ping;
  return command;
}

void HttpRequest::reset(uint32_t now, const char* token) {
  *this = HttpRequest();
  started_ = now;
  token_ = token;
  authorized_ = !*token;
}

void HttpRequest::fail(int code) { error_ = code; done_ = true; }

void HttpRequest::feed(char c) {
  if (done_) return;
  if (headersDone_) {
    if (c == 0) { fail(400); return; }
    body_[bodySize_++] = c;
    body_[bodySize_] = 0;
    if (bodySize_ == contentLength_) done_ = true;
    return;
  }
  if (++headerBytes_ > 1024) { fail(431); return; }
  if (sawCr_) {
    sawCr_ = false;
    if (c != '\n') { fail(400); return; }
    line_[lineSize_] = 0;
    endLine();
    lineSize_ = 0;
    return;
  }
  if (c == '\r') { sawCr_ = true; return; }
  if (static_cast<unsigned char>(c) < 32 && c != '\t') { fail(400); return; }
  if (static_cast<unsigned char>(c) > 126) { fail(400); return; }
  if (lineSize_ + 1 >= sizeof(line_)) { fail(431); return; }
  line_[lineSize_++] = c;
}

void HttpRequest::endLine() {
  if (firstLine_) {
    firstLine_ = false;
    char* path = strchr(line_, ' ');
    if (!path) { fail(400); return; }
    *path++ = 0;
    char* version = strchr(path, ' ');
    if (!version) { fail(400); return; }
    *version++ = 0;
    if (strcmp(version, "HTTP/1.1") && strcmp(version, "HTTP/1.0")) { fail(400); return; }
    method_ = !strcmp(line_, "GET") ? HttpMethod::Get :
              (!strcmp(line_, "POST") ? HttpMethod::Post : HttpMethod::Other);
    path_ = !strcmp(path, "/") ? HttpPath::Root :
            (!strcmp(path, "/status") ? HttpPath::Status :
            (!strcmp(path, "/tally") ? HttpPath::Tally : HttpPath::Other));
    return;
  }
  if (!lineSize_) {
    if (method_ == HttpMethod::Post && !gotLength_) { fail(411); return; }
    if (method_ != HttpMethod::Post && contentLength_) { fail(400); return; }
    headersDone_ = true;
    if (!contentLength_) done_ = true;
    return;
  }
  char* value = strchr(line_, ':');
  if (!value || value == line_) { fail(400); return; }
  *value++ = 0;
  // Strict names prevent whitespace-smuggling and obsolete folded headers.
  for (const char* p = line_; *p; ++p) {
    if (!tokenChar(*p)) { fail(400); return; }
  }
  while (*value == ' ' || *value == '\t') ++value;
  char* end = value + strlen(value);
  while (end > value && (end[-1] == ' ' || end[-1] == '\t')) *--end = 0;
  if (equalI(line_, "Content-Length")) {
    if (gotLength_ || !*value) { fail(400); return; }
    gotLength_ = true;
    for (const char* p = value; *p; ++p) {
      if (*p < '0' || *p > '9') { fail(400); return; }
      contentLength_ = contentLength_ * 10 + static_cast<size_t>(*p - '0');
      if (contentLength_ > kMaxBody) { fail(413); return; }
    }
  } else if (equalI(line_, "Transfer-Encoding")) {
    fail(400);  // No chunking or ambiguous framing on this small one-request server.
  } else if (equalI(line_, "Expect")) {
    fail(417);
  } else if (equalI(line_, "Authorization")) {
    if (gotAuthorization_) { fail(400); return; }
    gotAuthorization_ = true;
    if (*token_) authorized_ = !strncmp(value, "Bearer ", 7) && !strcmp(value + 7, token_);
  } else if (equalI(line_, "Content-Type")) {
    if (gotContentType_) { fail(400); return; }
    gotContentType_ = true;
    char* parameters = strchr(value, ';');
    if (parameters) {
      *parameters = 0;
      while (parameters > value && parameters[-1] == ' ') *--parameters = 0;
    }
    jsonContentType_ = equalI(value, "application/json");
  }
}

void HttpRequest::expire(uint32_t now) {
  if (!done_ && now - started_ >= 1500) fail(408);  // Total deadline, not reset by trickling bytes.
}

bool parseModeJson(const char* json, Mode& mode) {
  skipSpace(json);
  if (*json++ != '{') return false;
  char key[8], value[16];
  if (!readJsonString(json, key, sizeof(key)) || strcmp(key, "mode")) return false;
  skipSpace(json);
  if (*json++ != ':') return false;
  if (!readJsonString(json, value, sizeof(value))) return false;
  skipSpace(json);
  if (*json++ != '}') return false;
  skipSpace(json);
  return !*json && parseMode(value, mode);
}

const char* httpReason(int status) {
  switch (status) {
    case 200: return "OK";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 408: return "Request Timeout";
    case 411: return "Length Required";
    case 413: return "Payload Too Large";
    case 415: return "Unsupported Media Type";
    case 417: return "Expectation Failed";
    case 431: return "Request Header Fields Too Large";
    default: return "Internal Server Error";
  }
}

}  // namespace tally
