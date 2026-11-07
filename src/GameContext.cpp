#include "GameContext.hpp"
#include "Common.hpp"
#include "Log.hpp"
#include "Patterns.hpp"

#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

namespace mele {

    static constexpr const char* PATTERN_GWORLD_LE1 = "48 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 85 C0 0F 84 ?? ?? ?? ?? 33 D2";
    static constexpr const char* PATTERN_GWORLD_LE2 = "48 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 85 C0 0F 84 ?? ?? ?? ?? 33 D2 48 8B 4B";
    static constexpr const char* PATTERN_GWORLD_LE3 = "48 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 85 C0 0F 84 ?? ?? ?? ?? 33 D2";

    static constexpr const char* PATTERN_NAMEPOOLS_LE1 = "4C 8D 25 ?? ?? ?? ?? 33 F6 49 8B E9";
    static constexpr const char* PATTERN_NAMEPOOLS_LE2 = "4C 8D 25 ?? ?? ?? ?? 33 F6 49 8B E9";
    static constexpr const char* PATTERN_NAMEPOOLS_LE3 = "4C 8D 0D ?? ?? ?? ?? 66 66 0F 1F 84 ?? 00 00 00 00 48 8B 16";

    static constexpr const char* PATTERN_PROCESSEVENT_TAIL =
        "57 48 81 EC 90 00 00 00 48 8D 6C 24 ?? 48 C7 45 ?? FE FF FF FF";
    static constexpr unsigned char PROLOGUE_FIRST_FIVE[5] = { 0x40, 0x55, 0x41, 0x56, 0x41 };

    struct ExitFlagRva { UDWORD Le1, Le2, Le3; };
    static constexpr ExitFlagRva k_ExitFlagRva { 0x16b66f0u, 0x168a47cu, 0x17d5698u };

    GameContext& GameContext::Instance()
    {
        static GameContext instance;
        return instance;
    }

    GameId DetectGame()
    {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        const wchar_t* basename = ::PathFindFileNameW(path);
        std::string lower = ToLowerAscii(WideToUtf8(basename));

        if (lower == "masseffect1.exe") return GameId::LE1;
        if (lower == "masseffect2.exe") return GameId::LE2;
        if (lower == "masseffect3.exe") return GameId::LE3;
        return GameId::Unknown;
    }

    const char* GameContext::GameTag() const
    {
        switch (m_Game) {
        case GameId::LE1: return "LE1";
        case GameId::LE2: return "LE2";
        case GameId::LE3: return "LE3";
        default: return "??";
        }
    }

    const char* GameContext::GameTitle() const
    {
        switch (m_Game) {
        case GameId::LE1: return "Mass Effect 1";
        case GameId::LE2: return "Mass Effect 2";
        case GameId::LE3: return "Mass Effect 3";
        default: return "Mass Effect Legendary Edition";
        }
    }

    const char* GameContext::LargeImageKey() const
    {
        switch (m_Game) {
        case GameId::LE1: return "le1";
        case GameId::LE2: return "le2";
        case GameId::LE3: return "le3";
        default: return "le1";
        }
    }

    bool GameContext::Initialize()
    {
        m_Game = DetectGame();
        if (m_Game == GameId::Unknown) {
            LOG_ERROR("Unsupported process (expected MassEffect1/2/3.exe)");
            return false;
        }
        LOG_INFO("Detected game: %s (%s)", GameTitle(), GameTag());

        const char* gworldPattern = nullptr;
        const char* namePoolsPattern = nullptr;
        UDWORD exitRva = 0;
        switch (m_Game) {
        case GameId::LE1:
            gworldPattern = PATTERN_GWORLD_LE1;
            namePoolsPattern = PATTERN_NAMEPOOLS_LE1;
            exitRva = k_ExitFlagRva.Le1;
            break;
        case GameId::LE2:
            gworldPattern = PATTERN_GWORLD_LE2;
            namePoolsPattern = PATTERN_NAMEPOOLS_LE2;
            exitRva = k_ExitFlagRva.Le2;
            break;
        case GameId::LE3:
            gworldPattern = PATTERN_GWORLD_LE3;
            namePoolsPattern = PATTERN_NAMEPOOLS_LE3;
            exitRva = k_ExitFlagRva.Le3;
            break;
        default:
            return false;
        }

        void* gworldHit = FindCodePattern(gworldPattern);
        if (!gworldHit) {
            LOG_ERROR("GWorld pattern not found; mod cannot read game state");
            return false;
        }
        m_GWorld = reinterpret_cast<void**>(ResolveRipRelative(gworldHit, 7));
        LOG_INFO("GWorld: %p", reinterpret_cast<void*>(m_GWorld));

        void* poolsHit = FindCodePattern(namePoolsPattern);
        if (!poolsHit) {
            LOG_ERROR("Name-pools pattern not found; map names will be unavailable");
        } else {
            m_NamePools = reinterpret_cast<unsigned char**>(ResolveRipRelative(poolsHit, 7));
            LOG_INFO("SFXName pools: %p", reinterpret_cast<void*>(m_NamePools));
        }

        void* peHit = FindCodePattern(PATTERN_PROCESSEVENT_TAIL);
        if (!peHit) {
            LOG_ERROR("ProcessEvent pattern not found; cannot hook game tick");
            return false;
        }
        auto* peStart = static_cast<unsigned char*>(peHit) - 5;
        if (memcmp(peStart, PROLOGUE_FIRST_FIVE, 5) != 0) {
            LOG_ERROR("ProcessEvent prologue check failed on found location; aborting hook");
            return false;
        }
        m_ProcessEvent = peStart;
        LOG_INFO("ProcessEvent: %p", reinterpret_cast<void*>(m_ProcessEvent));

        HMODULE mod = ::GetModuleHandleW(nullptr);
        m_ExitFlag = reinterpret_cast<unsigned char*>(mod) + exitRva;
        LOG_INFO("GIsRequestingExit: %p", reinterpret_cast<void*>(m_ExitFlag));

        m_Ready = true;
        return true;
    }

    bool GameContext::IsExitRequested() const
    {
        return m_ExitFlag && *m_ExitFlag != 0;
    }

    struct NameEntryRaw {
        const unsigned char* Pool0;
        UDWORD Chunk;
        UDWORD Offset;
        const unsigned char** Pools;
    };

    static bool ReadEntrySafe(unsigned char** Pools, UDWORD Chunk, UDWORD Offset,
        unsigned char** OutEntryPtr, UDWORD* OutChunkBaseGuard)
    {
        __try {
            if (!Pools || Offset <= 0 || Chunk >= 8) {
                return false;
            }
            const unsigned char* pool = Pools[Chunk];
            if (!pool) {
                return false;
            }
            *OutEntryPtr = const_cast<unsigned char*>(pool + static_cast<size_t>(Offset));

            // Entry layout: SFXPackedIndex(4) | HashNext(8) | name data — guard read of first 12 bytes.
            volatile const unsigned char* probe = *OutEntryPtr;
            UDWORD acc = 0;
            for (int i = 0; i < 12; ++i) {
                acc += probe[i];
            }
            if (acc == 0xDEADBEEF) {
                return false;
            }
            *OutChunkBaseGuard = acc;
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    std::string GameContext::ResolveNameString(unsigned long long NameValue60)
    {
        UDWORD nameLow32 = static_cast<UDWORD>(NameValue60 & 0xFFFFFFFFull);
        INT nameNumber = static_cast<INT>(static_cast<long long>(NameValue60 >> 32));

        if (!m_NamePools) {
            return {};
        }

        const UDWORD chunk = (nameLow32 >> 29) & 0x7u;
        const UDWORD offset = nameLow32 & 0x1FFFFFFFu;

        unsigned char* entry = nullptr;
        UDWORD guard = 0;
        if (!ReadEntrySafe(m_NamePools, chunk, offset, &entry, &guard)) {
            LOG_DEBUG("Name lookup failed: chunk=%u offset=0x%x", chunk, offset);
            return {};
        }

        const UDWORD packed = *reinterpret_cast<UDWORD*>(entry);
        const UDWORD entryOffset = packed & 0xFFFFFu;
        const UDWORD entryLength = (packed >> 20) & 0x1FFu;
        const UDWORD entryFlags = (packed >> 29) & 0x7u;
        (void)entryOffset;

        if (entryLength == 0 || entryLength > 0x100) {
            LOG_DEBUG("Name entry bad length: %u", entryLength);
            return {};
        }

        bool isUnicode = (entryFlags & 0x2u) != 0;
        const unsigned char* nameData = entry + 12;

        std::string result;
        if (!isUnicode) {
            std::string ansi;
            ansi.reserve(entryLength);
            for (UDWORD i = 0; i < entryLength; ++i) {
                char c = static_cast<char>(nameData[i]);
                ansi.push_back(c);
            }
            std::swap(result, ansi);
        } else {
            std::wstring wide;
            wide.reserve(entryLength);
            for (UDWORD i = 0; i < entryLength; ++i) {
                wchar_t wc = static_cast<wchar_t>(nameData[i * 2] | (nameData[i * 2 + 1] << 8));
                wide.push_back(wc);
            }
            result = WideToUtf8(wide);
        }

        if (nameNumber > 0) {
            char suffix[16];
            snprintf(suffix, sizeof(suffix), "_%d", nameNumber - 1);
            result += suffix;
        }

        return result;
    }

    struct WorldPackageRaw {
        const void* World;
        const void* OuterMost;
        unsigned long long NameValue60;
    };

    static bool ReadWorldPackageSafe(const void* World, WorldPackageRaw* OutRaw)
    {
        __try {
            if (!World) {
                return false;
            }
            const unsigned char* obj = static_cast<const unsigned char*>(World);
            const unsigned char* nextOuter = *reinterpret_cast<unsigned char* const*>(obj + 0x40);
            while (nextOuter) {
                obj = nextOuter;
                nextOuter = *reinterpret_cast<unsigned char* const*>(obj + 0x40);
            }
            OutRaw->OuterMost = obj;
            OutRaw->NameValue60 = *reinterpret_cast<const unsigned long long*>(obj + 0x48);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    static bool TryGetCurrentWorldPackage(void** GWorld, unsigned long long* OutName60)
    {
        __try {
            if (!GWorld || !*GWorld) {
                return false;
            }
            WorldPackageRaw raw {};
            if (!ReadWorldPackageSafe(*GWorld, &raw)) {
                return false;
            }
            *OutName60 = raw.NameValue60;
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    std::string GameContext::GetWorldPackageName()
    {
        if (!m_Ready) {
            return {};
        }
        unsigned long long name60 = 0;
        if (!TryGetCurrentWorldPackage(m_GWorld, &name60)) {
            return {};
        }
        return ResolveNameString(name60);
    }

}
