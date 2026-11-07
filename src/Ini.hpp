#pragma once

#include <string>
#include <windows.h>

namespace mele {

    enum class GameId {
        Unknown = 0,
        LE1,
        LE2,
        LE3,
    };

    GameId DetectGame();

    struct Config {
        std::string ClientIdLE1;
        std::string ClientIdLE2;
        std::string ClientIdLE3;
        bool bEnabled = true;
        bool bDebugConsole = false;
        bool bShowTimer = true;
    };

    Config LoadConfig(void* ThisModuleModule, const char* CompiledDefaultLEId,
        const char* CompiledDefaultLE2Id, const char* CompiledDefaultLE3Id);

}
