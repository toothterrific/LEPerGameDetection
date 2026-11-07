#include "Common.hpp"
#include "Log.hpp"
#include "Ini.hpp"
#include "DiscordIpc.hpp"

#include <atomic>
#include <string>
#include <thread>

namespace mele {

    static const char* k_DefaultClientIdLE1 = "1568689342417739806";
    static const char* k_DefaultClientIdLE2 = "1568722277154160791";
    static const char* k_DefaultClientIdLE3 = "1568722427175895060";

    static const char* k_LargeKeyLE1 = "le1";
    static const char* k_LargeKeyLE2 = "le2";
    static const char* k_LargeKeyLE3 = "le3";

    static Config g_Config;
    static GameId g_Game = GameId::Unknown;
    static long long g_SessionStartMs = 0;

    static std::atomic<bool> g_WorkerAlive{ false };
    static std::thread g_WorkerThread;

    static void PushStaticPresence()
    {
        auto* ipc = &DiscordIpc::Instance();
        const char* largeKey = nullptr;
        switch (g_Game) {
        case GameId::LE1: largeKey = k_LargeKeyLE1; break;
        case GameId::LE2: largeKey = k_LargeKeyLE2; break;
        case GameId::LE3: largeKey = k_LargeKeyLE3; break;
        default: return;
        }

        std::string json = BuildActivityJson(
            largeKey, false, nullptr,
            "", "",
            g_Config.bShowTimer ? g_SessionStartMs : 0);

        if (ipc->SendActivityFrame(json)) {
            LOG_INFO("Presence pushed (game=%d large=%s timer=%d)",
                static_cast<int>(g_Game), largeKey, g_Config.bShowTimer ? 1 : 0);
        }
    }

    static DWORD WINAPI WorkerThread(LPVOID)
    {
        auto* ipc = &DiscordIpc::Instance();

        constexpr long long k_MinReconnectMs = 5000;
        constexpr long long k_HealthProbeMs = 30000;

        long long lastConnectAttemptMs = 0;
        long long lastHealthProbeMs = 0;

        if (ipc->TryConnect()) {
            lastConnectAttemptMs = NowTickMs();
            PushStaticPresence();
        }

        while (g_WorkerAlive.load()) {
            ::Sleep(400);
            long long now = NowTickMs();

            if (!ipc->IsConnected()) {
                if ((now - lastConnectAttemptMs) >= k_MinReconnectMs) {
                    lastConnectAttemptMs = now;
                    if (ipc->TryConnect()) {
                        PushStaticPresence();
                    }
                }
                continue;
            }

            if ((now - lastHealthProbeMs) >= k_HealthProbeMs) {
                lastHealthProbeMs = now;
                if (!ipc->ProbeHealth()) {
                    lastConnectAttemptMs = 0;
                }
            }

            ipc->DrainIncoming();
        }
        return 0;
    }

    static DWORD WINAPI ModThread(LPVOID Module)
    {
        HMODULE thisModule = static_cast<HMODULE>(Module);
        Config cfg = LoadConfig(thisModule, k_DefaultClientIdLE1, k_DefaultClientIdLE2, k_DefaultClientIdLE3);
        g_Config = cfg;
        if (!cfg.bEnabled) {
            return 0;
        }

        LogInit(cfg.bDebugConsole);
        LOG_INFO("=== MELE Discord RPC v2 starting (built %s %s) ===", __DATE__, __TIME__);

        g_Game = DetectGame();
        if (g_Game == GameId::Unknown) {
            LOG_ERROR("Unsupported process (expected MassEffect1/2/3.exe); staying idle");
            LogShutdown();
            return 0;
        }

        const char* clientId = "";
        const char* gameName = "";
        switch (g_Game) {
        case GameId::LE1: clientId = cfg.ClientIdLE1.c_str(); gameName = "Mass Effect 1"; break;
        case GameId::LE2: clientId = cfg.ClientIdLE2.c_str(); gameName = "Mass Effect 2"; break;
        case GameId::LE3: clientId = cfg.ClientIdLE3.c_str(); gameName = "Mass Effect 3"; break;
        default: break;
        }

        LOG_INFO("Detected game: %s", gameName);
        LOG_INFO("Config: client_id=%s enabled=%d debug=%d timer=%d",
            clientId, cfg.bEnabled ? 1 : 0, cfg.bDebugConsole ? 1 : 0, cfg.bShowTimer ? 1 : 0);

        if (!clientId || clientId[0] == '\0') {
            LOG_ERROR("No client id configured for this game; staying idle");
            LogShutdown();
            return 0;
        }

        DiscordIpc::Instance().Configure(clientId);
        g_SessionStartMs = NowEpochMs();

        g_WorkerAlive.store(true);
        g_WorkerThread = std::thread(WorkerThread, nullptr);
        LOG_INFO("Mod started");
        return 0;
    }

} // namespace mele

BOOL WINAPI DllMain(HINSTANCE hInstDLL, DWORD reason, LPVOID)
{
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hInstDLL);
        if (HANDLE hThread = ::CreateThread(nullptr, 0, &::mele::ModThread, hInstDLL, 0, nullptr)) {
            ::CloseHandle(hThread);
        }
        break;
    case DLL_PROCESS_DETACH:
        break;
    default:
        break;
    }
    return TRUE;
}
