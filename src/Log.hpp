#pragma once

#include <string>
#include <windows.h>

namespace mele {

    enum class LogLevel {
        Error = 0,
        Info = 1,
        Debug = 2,
    };

    void LogInit(bool bDebugConsole);
    void LogShutdown();
    void LogWrite(LogLevel Level, const char* Format, ...);

    std::string LogGetLogPath();

}

#define LOG_ERROR(...) ::mele::LogWrite(::mele::LogLevel::Error, __VA_ARGS__)
#define LOG_INFO(...)  ::mele::LogWrite(::mele::LogLevel::Info, __VA_ARGS__)
#define LOG_DEBUG(...) ::mele::LogWrite(::mele::LogLevel::Debug, __VA_ARGS__)
