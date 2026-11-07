#pragma once

#include <string>
#include <windows.h>

namespace mele {

    struct Config {
        std::string ClientId;
        bool bEnabled = true;
        bool bDebugConsole = false;
        bool bShowRawPackages = false;
        bool bStateNumber = true;
    };

    std::wstring GetModDirectory();

    Config LoadConfig(HMODULE ThisModule, const char* CompiledDefaultClientId);

}
