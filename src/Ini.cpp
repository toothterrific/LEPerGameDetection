#include "Ini.hpp"
#include "Common.hpp"

#include <shlwapi.h>

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

    std::wstring GetModDirectory()
    {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        ::PathRemoveFileSpecW(path);
        return path;
    }

    Config LoadConfig(HMODULE ThisModule, const char* CompiledDefaultClientId)
    {
        Config cfg;
        cfg.ClientId = CompiledDefaultClientId;

        wchar_t modulePath[MAX_PATH];
        if (!GetModuleFileNameW(ThisModule, modulePath, MAX_PATH)) {
            return cfg;
        }
        ::PathRemoveFileSpecW(modulePath);
        std::wstring iniPath = std::wstring(modulePath) + L"\\MELEDiscordRPC.ini";

        wchar_t buffer[256];
        const wchar_t* file = iniPath.c_str();

        if (GetPrivateProfileStringW(L"presence", L"client_id", L"", buffer, 256, file) > 0) {
            std::string v = ToLowerAscii(WideToUtf8(buffer));
            if (!v.empty() && v.find_first_not_of("0123456789") == std::string::npos) {
                cfg.ClientId = v;
            }
        }
        if (GetPrivateProfileStringW(L"presence", L"enabled", L"", buffer, 256, file) > 0) {
            cfg.bEnabled = ParseBool(WideToUtf8(buffer));
        }
        if (GetPrivateProfileStringW(L"presence", L"debug", L"", buffer, 256, file) > 0) {
            cfg.bDebugConsole = ParseBool(WideToUtf8(buffer));
        }
        if (GetPrivateProfileStringW(L"presence", L"show_raw_packages", L"", buffer, 256, file) > 0) {
            cfg.bShowRawPackages = ParseBool(WideToUtf8(buffer));
        }
        if (GetPrivateProfileStringW(L"presence", L"show_game_number", L"", buffer, 256, file) > 0) {
            cfg.bStateNumber = ParseBool(WideToUtf8(buffer));
        }

        return cfg;
    }

}
