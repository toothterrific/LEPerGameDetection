#include "Ini.hpp"
#include "Common.hpp"

#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

namespace mele {

    static std::string TrimAscii(std::string In)
    {
        size_t b = 0;
        while (b < In.size() && (In[b] == ' ' || In[b] == '\t' || In[b] == '\r' || In[b] == '\n')) ++b;
        size_t e = In.size();
        while (e > b && (In[e - 1] == ' ' || In[e - 1] == '\t' || In[e - 1] == '\r' || In[e - 1] == '\n')) --e;
        return In.substr(b, e - b);
    }

    static bool ParseBool(const std::string& In)
    {
        std::string s = ToLowerAscii(In);
        return s == "1" || s == "true" || s == "yes" || s == "on";
    }

    static bool IsDigits(const std::string& In)
    {
        if (In.empty()) {
            return false;
        }
        for (char c : In) {
            if (c < '0' || c > '9') {
                return false;
            }
        }
        return true;
    }

    GameId DetectGame()
    {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        std::string lower = ToLowerAscii(WideToUtf8(::PathFindFileNameW(path)));

        if (lower == "masseffect1.exe") return GameId::LE1;
        if (lower == "masseffect2.exe") return GameId::LE2;
        if (lower == "masseffect3.exe") return GameId::LE3;
        return GameId::Unknown;
    }

    Config LoadConfig(void* ThisModuleModule, const char* CompiledDefaultLEId,
        const char* CompiledDefaultLE2Id, const char* CompiledDefaultLE3Id)
    {
        Config cfg;
        cfg.ClientIdLE1 = CompiledDefaultLEId;
        cfg.ClientIdLE2 = CompiledDefaultLE2Id;
        cfg.ClientIdLE3 = CompiledDefaultLE3Id;

        wchar_t modulePath[MAX_PATH];
        if (!GetModuleFileNameW(static_cast<HMODULE>(ThisModuleModule), modulePath, MAX_PATH)) {
            return cfg;
        }
        ::PathRemoveFileSpecW(modulePath);
        std::wstring iniPath = std::wstring(modulePath) + L"\\MELEPerGamePresence.ini";

        wchar_t buffer[256];
        const wchar_t* file = iniPath.c_str();

        if (GetPrivateProfileStringW(L"presence", L"client_id_le1", L"", buffer, 256, file) > 0) {
            std::string v = TrimAscii(WideToUtf8(buffer));
            if (IsDigits(v)) {
                cfg.ClientIdLE1 = v;
            }
        }
        if (GetPrivateProfileStringW(L"presence", L"client_id_le2", L"", buffer, 256, file) > 0) {
            std::string v = TrimAscii(WideToUtf8(buffer));
            if (IsDigits(v)) {
                cfg.ClientIdLE2 = v;
            }
        }
        if (GetPrivateProfileStringW(L"presence", L"client_id_le3", L"", buffer, 256, file) > 0) {
            std::string v = TrimAscii(WideToUtf8(buffer));
            if (IsDigits(v)) {
                cfg.ClientIdLE3 = v;
            }
        }
        if (GetPrivateProfileStringW(L"presence", L"enabled", L"", buffer, 256, file) > 0) {
            cfg.bEnabled = ParseBool(WideToUtf8(buffer));
        }
        if (GetPrivateProfileStringW(L"presence", L"debug", L"", buffer, 256, file) > 0) {
            cfg.bDebugConsole = ParseBool(WideToUtf8(buffer));
        }
        if (GetPrivateProfileStringW(L"presence", L"show_timer", L"", buffer, 256, file) > 0) {
            cfg.bShowTimer = ParseBool(WideToUtf8(buffer));
        }

        return cfg;
    }

}
