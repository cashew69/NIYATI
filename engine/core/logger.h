#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdarg.h>

enum LogLevel {
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
    LOG_DEBUG
};

#ifdef __cplusplus
extern "C" {
#endif

void Logger_Init(const char* filename);
void Logger_Log(LogLevel level, const char* format, ...);
void Logger_Cleanup();

extern FILE* gpFile; // Legacy compatibility pointer


// Helper macros for easy use
#define LOG_I(...) Logger_Log(LOG_INFO, __VA_ARGS__)
#define LOG_W(...) Logger_Log(LOG_WARNING, __VA_ARGS__)
#define LOG_E(...) Logger_Log(LOG_ERROR, __VA_ARGS__)
#define LOG_D(...) Logger_Log(LOG_DEBUG, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif // LOGGER_H
