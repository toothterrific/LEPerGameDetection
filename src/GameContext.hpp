#pragma once

#include <string>

namespace mele {

    enum class GameId {
        Unknown = 0,
        LE1,
        LE2,
        LE3,
    };

    GameId DetectGame();

    class GameContext {
    public:
        static GameContext& Instance();

        bool Initialize();

        GameId GetGame() const { return m_Game; }
        const char* GameTag() const;
        const char* GameTitle() const;
        const char* LargeImageKey() const;

        bool IsReady() const { return m_Ready; }
        bool IsExitRequested() const;
        void* ProcessEventAddress() const { return m_ProcessEvent; }

        void** WorldSlot() const { return m_GWorld; }
        unsigned char** NamePoolsRaw() const { return m_NamePools; }

        std::string GetWorldPackageName();
        std::string ResolveNameByValue(unsigned long long NameValue60);

    private:
        GameContext() = default;

        GameId m_Game = GameId::Unknown;
        bool m_Ready = false;

        void** m_GWorld = nullptr;
        unsigned char** m_NamePools = nullptr;
        unsigned char* m_ProcessEvent = nullptr;
        unsigned char* m_ExitFlag = nullptr;

        std::string ResolveNameString(unsigned long long NameValue60);
    };

}
