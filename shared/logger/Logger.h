#pragma once
#include <string>

class Logger {
public:
  static void verbose(const std::string& tag, const char* fmt, ...);
  static void debug(const std::string& tag, const char* fmt, ...);
  static void info(const std::string& tag, const char* fmt, ...);
  static void warn(const std::string& tag, const char* fmt, ...);
  static void error(const std::string& tag, const char* fmt, ...);
};