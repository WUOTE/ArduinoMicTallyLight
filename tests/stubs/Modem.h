#pragma once
#include <string>
#define _SOFTRESETWIFI "reset"
#define PROMPT(command) command
#define CMD(command) command
struct ModemClass {
  bool beginned = false;
  size_t timeoutMs = 10000;
  void begin(int baud, int retry);
  void timeout(size_t value) { timeoutMs = value; }
  bool write(const std::string&, std::string&, const char*, ...);
};
extern ModemClass modem;
