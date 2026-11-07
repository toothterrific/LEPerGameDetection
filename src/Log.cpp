#include "Log.hpp"
#include "Common.hpp"

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <shlobj.h>

namespace mele {

    static HANDLE g_LogFile = nullptr;
    static bool g_DebugConsole = false;
    static std::wstring g_LogDir;

    std::wstring GetLogDirectory()
    {
        PWSTR appData = nullptr;
        if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &appData))) {
            return L"C:\\MELEDiscordRPC";
        }
        std::wstring dir = std::wstring(appData) + L"\\MELEDiscordRPC";
        CoTaskMemFree(appData);
        return dir;
    }

    void LogInit(bool bDebugConsole)
    {
        g_LogDir = GetLogDirectory();
        CreateDirectoryW(g_LogDir.c_str(), nullptr);

        SYSTEMTIME st;
        GetLocalTime(&st);
        wchar_t stamp[64];
        swprintf_s(stamp, L"%04u-%02u-%02u_%02u-%02u-%02u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

        std::wstring path = g_LogDir + L"\\MELEDiscordRPC_" + stamp + L".log";
        g_LogFile = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (g_LogFile != INVALID_HANDLE_VALUE && GetFileSize(g_LogFile, nullptr) == 0) {
            DWORD written = 0;
            const char BOM[] = { static_cast<char>(0xEF), static_cast<char>(0xBB), static_cast<char>(0xBF) };
            WriteFile(g_LogFile, BOM, 3, &written, nullptr);
        }

        if (bDebugConsole) {
            if (::AllocConsole()) {
                FILE* f = nullptr;
                freopen_s(&f, "CONOUT$", "w", stdout);
                SetConsoleTitleW(L"MELE Discord RPC [debug]");
            }
            g_DebugConsole = true;
        }
    }

    void LogShutdown()
    {
        if (g_LogFile && g_LogFile != INVALID_HANDLE_VALUE) {
            CloseHandle(g_LogFile);
            g_LogFile = nullptr;
        }
    }

    std::string LogGetLogPath()
    {
        return WideToUtf8(g_LogDir) + "\\MELEDiscordRPC_*.log";
    }

    static void WriteLevelPrefix(LogLevel Level, char* Out, size_t OutSize)
    {
        const char* tag = "INFO ";
        if (Level == LogLevel::Error) tag = "ERROR";
        else if (Level == LogLevel::Debug) tag = "DEBUG";
        (void)OutSize;

        SYSTEMTIME st;
        GetLocalTime(&st);
        char timebuf[32];
        snprintf(timebuf, sizeof(timebuf), "%02u:%02u:%02u.%03u [%s] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, tag);
        strcpy_s(Out, 48, timebuf);
    }

    void LogWrite(LogLevel Level, const char* Format, ...)
    {
        char prefix[48];
        WriteLevelPrefix(Level, prefix, sizeof(prefix));

        char body[1024];
        va_list args;
        va_start(args, Format);
        _vsnprintf_s(body, sizeof(body), _TRUNCATE, Format, args);
        va_end(args);

        char line[1120];
        snprintf(line, sizeof(line), "%s%s\r\n", prefix, body);

        if (g_DebugConsole || Level != LogLevel::Debug) {
            printf("%s", line);
            fflush(stdout);
        }

        if (g_LogFile && g_LogFile != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            WriteFile(g_LogFile, line, static_cast<DWORD>(strlen(line)), &written, nullptr);
        }
    }

}
