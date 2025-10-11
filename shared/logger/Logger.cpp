#include "Logger.h"
#include <cstdarg> // va_list, va_start, va_end (가변 인자 처리)

#if defined (__ANDROID__)
  #include <android/log.h>
#elif defined(__APPLE__)
  #include <TargetConditionals.h>
  #if TARGET_OS_IOS
  // TODO : include iOS logging backend
  #endif
#endif

void Logger::verbose(const std::string& tag, const char* fmt, ...) {
#if defined (__ANDROID__)
  va_list args;
  va_start(args, fmt);
  __android_log_vprint(ANDROID_LOG_VERBOSE, tag.c_str(), fmt, args);
  va_end(args);
#elif defined(__APPLE__) && TARGET_OS_IOS
  // TODO : implement iOS logging backend
#endif
};

void Logger::debug(const std::string& tag, const char* fmt, ...) {
#if defined (__ANDROID__)
  va_list args;
  va_start(args, fmt);
  __android_log_vprint(ANDROID_LOG_DEBUG, tag.c_str(), fmt, args);
  va_end(args);
#elif defined(__APPLE__) && TARGET_OS_IOS
  // TODO : implement iOS logging backend
#endif
};

void Logger::info(const std::string& tag, const char* fmt, ...) {
#if defined (__ANDROID__)
  va_list args;
  va_start(args, fmt);
  __android_log_vprint(ANDROID_LOG_INFO, tag.c_str(), fmt, args);
  va_end(args);
#elif defined(__APPLE__) && TARGET_OS_IOS
  // TODO : implement iOS logging backend
#endif
};

void Logger::warn(const std::string& tag, const char* fmt, ...) {
#if defined (__ANDROID__)
  va_list args;
  va_start(args, fmt);
  __android_log_vprint(ANDROID_LOG_WARN, tag.c_str(), fmt, args);
  va_end(args);
#elif defined(__APPLE__) && TARGET_OS_IOS
  // TODO : implement iOS logging backend
#endif
};

void Logger::error(const std::string& tag, const char* fmt, ...) {
#if defined (__ANDROID__)
  va_list args;
  va_start(args, fmt);
  __android_log_vprint(ANDROID_LOG_ERROR, tag.c_str(), fmt, args);
  va_end(args);
#elif defined(__APPLE__) && TARGET_OS_IOS
  // TODO : implement iOS logging backend
#endif
};

