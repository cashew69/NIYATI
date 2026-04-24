#include "logger.h"
#include <time.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

// Global log file pointer to maintain compatibility with existing code during transition
FILE* gpFile = NULL;

void Logger_Init(const char* filename) {
    gpFile = fopen(filename, "w");
    if (!gpFile) {
        printf("[Logger] Failed to open log file %s\n", filename);
        return;
    }
    LOG_I("Logger initialized. Outputting to %s", filename);
}

void Logger_Log(LogLevel level, const char* format, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    const char* levelStr = "INFO";
    switch (level) {
        case LOG_WARNING: levelStr = "WARNING"; break;
        case LOG_ERROR:   levelStr = "ERROR";   break;
        case LOG_DEBUG:   levelStr = "DEBUG";   break;
        default: break;
    }

    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char timeStr[20];
    if (t) {
        strftime(timeStr, sizeof(timeStr), "%H:%M:%S", t);
    } else {
        strcpy(timeStr, "00:00:00");
    }

    // Prepare final message
    char finalMessage[4096];
    snprintf(finalMessage, sizeof(finalMessage), "[%s] [%s] %s\n", timeStr, levelStr, buffer);

    // Console output
#ifdef _WIN32
    OutputDebugStringA(finalMessage);
    printf("%s", finalMessage);
#else
    // Color coding for Linux terminal
    const char* color = "\033[0m"; // Reset
    if (level == LOG_WARNING) color = "\033[1;33m"; // Yellow
    if (level == LOG_ERROR)   color = "\033[1;31m"; // Red
    if (level == LOG_DEBUG)   color = "\033[1;36m"; // Cyan
    
    printf("%s%s\033[0m", color, finalMessage);
#endif

    // File output
    if (gpFile) {
        fprintf(gpFile, "%s", finalMessage);
        fflush(gpFile);
    }
}

void Logger_Cleanup() {
    if (gpFile) {
        LOG_I("Logger shutting down.");
        fclose(gpFile);
        gpFile = NULL;
    }
}
