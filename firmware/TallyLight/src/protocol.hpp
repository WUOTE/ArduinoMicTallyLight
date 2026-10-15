#pragma once

#include "controller.hpp"
#include <stddef.h>
#include <stdint.h>

namespace tally {

enum class LineEvent { None, Ready, Invalid };
class SerialLine {
 public:
  LineEvent feed(char c, uint32_t now);
  bool expire(uint32_t now);
  const char* text() const { return buffer_; }

 private:
  char buffer_[64] = {};
  size_t size_ = 0;
  bool discarding_ = false;
  uint32_t lastByte_ = 0;
};

enum class CommandKind { Invalid, SetMode, Status, Ping };
struct Command {
  CommandKind kind = CommandKind::Invalid;
  Mode mode = Mode::Off;
  char id[17] = {};
};
// USB accepts bare commands or "@request_id command" (ID: 1..16 ASCII letters/digits/_/-).
Command parseCommand(const char* line);

enum class HttpMethod { Get, Post, Other };
enum class HttpPath { Root, Status, Tally, Other };
class HttpRequest {
 public:
  static constexpr size_t kMaxBody = 96;
  void reset(uint32_t now, const char* token = "");
  void feed(char c);
  void expire(uint32_t now);
  bool done() const { return done_; }
  int error() const { return error_; }
  HttpMethod method() const { return method_; }
  HttpPath path() const { return path_; }
  bool authorized() const { return authorized_; }
  const char* body() const { return body_; }
  bool jsonContentType() const { return jsonContentType_; }

 private:
  void endLine();
  void fail(int code);
  char line_[192] = {};
  char body_[kMaxBody + 1] = {};
  size_t lineSize_ = 0;
  size_t headerBytes_ = 0;
  size_t bodySize_ = 0;
  size_t contentLength_ = 0;
  const char* token_ = "";
  uint32_t started_ = 0;
  HttpMethod method_ = HttpMethod::Other;
  HttpPath path_ = HttpPath::Other;
  int error_ = 0;
  bool firstLine_ = true;
  bool headersDone_ = false;
  bool gotLength_ = false;
  bool gotAuthorization_ = false;
  bool gotContentType_ = false;
  bool authorized_ = true;
  bool jsonContentType_ = false;
  bool sawCr_ = false;
  bool done_ = false;
};

// Deliberately small JSON schema: exactly {"mode":"hot"}, with JSON whitespace.
// Rejects duplicate/unknown fields, trailing bytes, escapes and non-string values.
bool parseModeJson(const char* json, Mode& mode);
const char* httpReason(int status);

}  // namespace tally
