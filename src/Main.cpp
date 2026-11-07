#include "Common.hpp"
#include "Log.hpp"
#include "Patterns.hpp"
#include "GameContext.hpp"
#include "DiscordIpc.hpp"
#include "Presence.hpp"
#include "MapNames.hpp"
#include "Ini.hpp"

#include <MinHook.h>
#include <string>
#include <atomic>
#include <thread>

namespace mele {

    typedef void (*tProcessEvent)(void* Context, void* Function, void* Parms, void* Result);
    static tProcessEvent g_OrigProcessEvent = nullptr;
    static void* g_HookTarget = nullptr;
    static std::atomic<bool> g_HookAlive{ false };
    static std::atomic<long long> g_LastPresenceReadTick{ 0 };
    static std::atomic<bool> g_WorkerAlive{ false };
    static std::thread g_WorkerThread;
    static Config g_Config;
    static const char* k_DefaultClientId = "0";

    static void OnGameTick()
    {
        auto* ctx = &GameContext::Instance();

        std::string packageName = ctx->GetWorldPackageName();
        PresenceField field;
        field.LargeKey = ctx->LargeImageKey();
        switch (ctx->GetGame()) {
        case GameId::LE1: field.State = "Mass Effect"; break;
        case GameId::LE2: field.State = "Mass Effect 2"; break;
        case GameId::LE3: field.State = "Mass Effect 3"; break;
        default: field.State = "Mass Effect"; break;
        }

        if (packageName.empty()) {
            field.Details = "Starting up";
        } else if (packageName == "entrymenu") {
            field.Details = "In main menu";
            field.bSmallKey = true;
            strcpy_s(field.SmallKey, sizeof(field.SmallKey), "menu");
        } else {
            const char* mapped = LookupMapName(static_cast<int>(ctx->GetGame()), packageName);
            if (mapped) {
                field.Details = mapped;
            } else {
                field.Details = g_Config.bShowRawPackages ? ("Exploring " + packageName) : "Exploring";
                LOG_DEBUG("Unmapped package '%s' (game %s)", packageName.c_str(), ctx->GameTag());
            }
        }

        PresenceState::Instance().StartSessionOnce();
        PresenceState::Instance().Publish(field);
    }

    static void OnGameTickGuarded()
    {
        __try {
            OnGameTick();
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            LOG_ERROR("Presence read faulted (guarded)");
        }
    }

    static void ProcessEventHook(void* Context, void* Function, void* Parms, void* Result)
    {
        if (g_HookAlive.load(std::memory_order_acquire)) {
            long long tick = ::GetTickCount64();
            long long last = g_LastPresenceReadTick.load(std::memory_order_acquire);
            if (tick - last >= 500) {
                if (g_LastPresenceReadTick.compare_exchange_strong(last, tick)) {
                    GameContext* ctx = &GameContext::Instance();
                    if (ctx->IsExitRequested()) {
                        PresenceState::Instance().Publish(PresenceField{});
                        g_HookAlive.store(false, std::memory_order_release);
                    } else {
                        OnGameTickGuarded();
                    }
                }
            }
        }
        if (g_OrigProcessEvent) {
            g_OrigProcessEvent(Context, Function, Parms, Result);
        }
    }

    static bool SendPresenceOrClear(const PresenceField& cur, PresenceField& lastSent, bool& bHaveAny)
    {
        auto* ipc = &DiscordIpc::Instance();
        if (cur.Details.empty()) {
            if (!bHaveAny) {
                return false;
            }
            if (ipc->SendClearActivity()) {
                bHaveAny = false;
                LOG_INFO("Presence cleared (game shutting down)");
                return true;
            }
            return false;
        }

        std::string json = BuildActivityJson(
            cur.LargeKey, cur.bSmallKey,
            cur.bSmallKey ? cur.SmallKey : nullptr,
            cur.Details, cur.State,
            PresenceState::Instance().SessionStart());
        if (ipc->SendActivityFrame(json)) {
            bHaveAny = true;
            LOG_INFO("Presence sent: details='%s' state='%s' large=%s small=%s",
                cur.Details.c_str(), cur.State.c_str(), cur.LargeKey.c_str(),
                cur.bSmallKey ? cur.SmallKey : "-");
            return true;
        }
        return false;
    }

    static DWORD WINAPI WorkerThread(LPVOID)
    {
        auto* ipc = &DiscordIpc::Instance();
        PresenceField lastSent;
        bool bHaveAny = false;
        bool bSentIdleOnce = false;
        long long lastIoAttempt = 0;

        while (g_WorkerAlive.load()) {
            ::Sleep(300);
            if (!g_Config.bEnabled) {
                continue;
            }

            PresenceField cur;
            PresenceState::Instance().GetLatest(&cur);
            long long now = ::GetTickCount64();

            const bool dirty = cur.Differs(lastSent);

            if (dirty) {
                if (ipc->IsConnected()) {
                    if (SendPresenceOrClear(cur, lastSent, bHaveAny)) {
                        lastSent = cur;
                        bSentIdleOnce = bHaveAny;
                        lastIoAttempt = now;
                        ipc->DrainIncoming();
                        continue;
                    }
                }
                if (ipc->TryConnect() && SendPresenceOrClear(cur, lastSent, bHaveAny)) {
                    lastSent = cur;
                    bSentIdleOnce = bHaveAny;
                    lastIoAttempt = now;
                    ipc->DrainIncoming();
                    continue;
                }
                lastIoAttempt = now;
            } else if (bHaveAny && (now - lastIoAttempt) >= 15000) {
                if (ipc->IsConnected() && ipc->SendPing()) {
                    ipc->DrainIncoming();
                } else if (ipc->TryConnect()) {
                    ipc->DrainIncoming();
                }
                lastIoAttempt = now;
            } else if (!bHaveAny && !bSentIdleOnce && (now - lastIoAttempt) >= 5000) {
                bSentIdleOnce = true;
            } else if (!ipc->IsConnected() && (now - lastIoAttempt) >= 15000) {
                lastIoAttempt = now;
            }
        }
        return 0;
    }

    static void UninstallHookIfAny()
    {
        if (g_OrigProcessEvent) {
            if (g_HookTarget) {
                ::MH_DisableHook(g_HookTarget);
                ::MH_RemoveHook(g_HookTarget);
            }
            ::MH_Uninitialize();
            g_OrigProcessEvent = nullptr;
            g_HookTarget = nullptr;
        }
    }

    static DWORD WINAPI ModThread(LPVOID Module)
    {
        Config cfg = LoadConfig(static_cast<HMODULE>(Module), k_DefaultClientId);
        g_Config = cfg;
        if (!cfg.bEnabled) {
            return 0;
        }
        LogInit(cfg.bDebugConsole);
        LOG_INFO("=== MELE Discord RPC starting (built %s %s) ===", __DATE__, __TIME__);
        LOG_INFO("Config: client_id=%s enabled=%d debug=%d raw_packages=%d game_number=%d",
            cfg.ClientId.c_str(), cfg.bEnabled ? 1 : 0, cfg.bDebugConsole ? 1 : 0,
            cfg.bShowRawPackages ? 1 : 0, cfg.bStateNumber ? 1 : 0);

        if (!GameContext::Instance().Initialize()) {
            LOG_ERROR("Initialization failed; mod stays idle");
            return 0;
        }

        DiscordIpc::Instance().Configure(cfg.ClientId);

        if (::MH_Initialize() != MH_OK) {
            LOG_ERROR("MH_Initialize failed");
            return 0;
        }

        g_HookTarget = GameContext::Instance().ProcessEventAddress();
        if (!g_HookTarget) {
            LOG_ERROR("ProcessEvent address missing");
            return 0;
        }
        if (::MH_CreateHook(g_HookTarget, &ProcessEventHook,
            reinterpret_cast<LPVOID*>(&g_OrigProcessEvent)) != MH_OK) {
            LOG_ERROR("MH_CreateHook(ProcessEvent) failed");
            return 0;
        }
        if (::MH_EnableHook(g_HookTarget) != MH_OK) {
            LOG_ERROR("MH_EnableHook(ProcessEvent) failed");
            return 0;
        }
        LOG_INFO("ProcessEvent hook installed at %p", g_HookTarget);

        g_HookAlive.store(true, std::memory_order_release);
        g_WorkerAlive.store(true);
        g_WorkerThread = std::thread(WorkerThread, nullptr);
        LOG_INFO("Mod fully started");
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
