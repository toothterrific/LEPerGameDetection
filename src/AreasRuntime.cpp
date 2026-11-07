#include "AreasRuntime.hpp"
#include "Common.hpp"
#include "Log.hpp"
#include "Patterns.hpp"
#include "GameContext.hpp"
#include "MapNames.hpp"

namespace mele {

    static constexpr const char* PATTERN_GOBJOBJECTS_LE1 =
        "48 8B 05 ?? ?? ?? ?? 4D 8B 0C ?? 4D 85 C9";
    static constexpr const char* PATTERN_TLKTABLE_LE1 =
        "48 8b 0d e7 a9 a2 00 e8 d2 74 42 ff 48 8b f8";

    static constexpr UDWORD RVA_TLKLOOKUP_LE1 = 0xb24470u;
    static constexpr UDWORD RVA_GETMAPLABELS_LE1 = 0xad2f60u;

    static constexpr size_t OFFU_Name = 0x48;
    static constexpr size_t OFFU_Outer = 0x40;
    static constexpr size_t OFFU_Class = 0x50;

    static constexpr size_t OFFWI_AreaMovie = 0x0B44;
    static constexpr size_t OFFWI_ParentMovie = 0x0B4C;

    struct GameTArray {
        void* Data = nullptr;
        unsigned int Count = 0;
        unsigned int Max = 0;
    };

    struct GameFString {
        wchar_t* Data = nullptr;
        unsigned int Count = 0;
        unsigned int Max = 0;
    };

    typedef wchar_t* (*tTlkLookup)(void* Table, GameFString* Out, int StrRef, int bParse);
    typedef void* (*tGetMapLabels)(GameFString* OutLabels, GameFString* OutParent);

    static uintptr_t SafeReadPtr(uintptr_t Address)
    {
        __try {
            return *reinterpret_cast<uintptr_t*>(Address);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return 0;
        }
    }

    static unsigned long long SafeReadU64(uintptr_t Address)
    {
        __try {
            return *reinterpret_cast<unsigned long long*>(Address);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return 0;
        }
    }

    static bool SafeOuterMostName(uintptr_t ObjectPtr, unsigned long long* OutName60)
    {
        __try {
            uintptr_t obj = ObjectPtr;
            for (int hop = 0; hop < 64 && obj; ++hop) {
                uintptr_t outer = *reinterpret_cast<uintptr_t*>(obj + OFFU_Outer);
                if (!outer) {
                    break;
                }
                obj = outer;
            }
            if (!obj) {
                return false;
            }
            *OutName60 = *reinterpret_cast<unsigned long long*>(obj + OFFU_Name);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    static bool SafeNameEqualsAscii(const unsigned char** Pools, unsigned long long Name60,
        const char* Ascii, size_t AsciiLen)
    {
        __try {
            if (!Pools) return false;
            unsigned int low32 = static_cast<unsigned int>(Name60 & 0xFFFFFFFFull);
            unsigned int chunkValue = (low32 >> 29) & 0x7u;
            unsigned int offsetValue = low32 & 0x1FFFFFFFu;
            if (chunkValue >= 8) return false;
            const unsigned char* pool = Pools[chunkValue];
            if (!pool) return false;
            const unsigned char* entry = pool + offsetValue;
            unsigned int packed = *reinterpret_cast<const unsigned int*>(entry);
            unsigned int flags = (packed >> 29) & 0x7u;
            unsigned int length = (packed >> 20) & 0x1FFu;
            if (flags & 0x2u) return false;
            if (length != AsciiLen) return false;
            return memcmp(entry + 12, Ascii, AsciiLen) == 0;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    struct ScanCtx {
        const unsigned char** Pools = nullptr;
        unsigned char* ObjData = nullptr;
        unsigned int ObjCount = 0;
        unsigned char* WorldObj = nullptr;
        unsigned long long WorldTopName60 = 0;
    };

    static bool TryScanWorldInfoSafe(ScanCtx* Ctx, unsigned char** OutWorldInfo)
    {
        __try {
            if (!Ctx->Pools || !Ctx->ObjData || !Ctx->WorldObj || Ctx->ObjCount == 0 || Ctx->ObjCount > 0x1000000u) {
                return false;
            }
            for (unsigned int i = 0; i < Ctx->ObjCount; ++i) {
                unsigned char* obj = *reinterpret_cast<unsigned char**>(Ctx->ObjData + static_cast<size_t>(i) * 8);
                if (!obj) continue;
                uintptr_t cls = SafeReadPtr(reinterpret_cast<uintptr_t>(obj) + OFFU_Class);
                if (!cls) continue;
                unsigned long long clsName60 = SafeReadU64(cls + OFFU_Name);
                if (!SafeNameEqualsAscii(Ctx->Pools, clsName60, "BioWorldInfo", 12)) continue;

                unsigned long long topName60 = 0;
                if (!SafeOuterMostName(reinterpret_cast<uintptr_t>(obj), &topName60)) continue;
                if (topName60 == Ctx->WorldTopName60) {
                    *OutWorldInfo = obj;
                    return true;
                }
            }
            return false;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    struct GObjView {
        unsigned char* Data = nullptr;
        unsigned int Count = 0;
        bool Valid = false;
    };

    static bool ReadGObjectsViewSafe(void** GObjObjectsGlobal, GObjView* Out)
    {
        __try {
            if (!GObjObjectsGlobal) return false;
            void* tarr = *GObjObjectsGlobal;
            if (!tarr) return false;
            GameTArray* t = static_cast<GameTArray*>(tarr);
            if (!t->Data || t->Count == 0 || t->Count > 0x1000000u) return false;
            Out->Data = static_cast<unsigned char*>(t->Data);
            Out->Count = t->Count;
            Out->Valid = true;
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    struct MovieRaw {
        uintptr_t Movie = 0;
        uintptr_t Parent = 0;
        bool Valid = false;
    };

    static bool ReadWorldMoviesSafe(uintptr_t WorldInfoPtr, MovieRaw* Out)
    {
        __try {
            Out->Movie = *reinterpret_cast<uintptr_t*>(WorldInfoPtr + OFFWI_AreaMovie);
            Out->Parent = *reinterpret_cast<uintptr_t*>(WorldInfoPtr + OFFWI_ParentMovie);
            Out->Valid = Out->Movie != 0;
            return Out->Valid;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            Out->Valid = false;
            return false;
        }
    }

    static bool CallTlkLookupSafe(void* Fn, void* Table, int StrRef, GameFString* Out)
    {
        __try {
            reinterpret_cast<tTlkLookup>(Fn)(Table, Out, StrRef, 0);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            Out->Data = nullptr;
            Out->Count = 0;
            return false;
        }
    }

    static bool CallGetMapLabelsSafe(void* Fn, GameFString* OutLabels, GameFString* OutParent)
    {
        __try {
            reinterpret_cast<tGetMapLabels>(Fn)(OutLabels, OutParent);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            OutLabels->Data = nullptr;
            OutLabels->Count = 0;
            OutParent->Data = nullptr;
            OutParent->Count = 0;
            return false;
        }
    }

    static std::string FStringToUtf8(const GameFString& F)
    {
        if (!F.Data || F.Count < 2) return {};
        unsigned int chars = F.Count - 1;
        if (chars > 0x800u) return {};
        const wchar_t* data = F.Data;
        if (!data) return {};
        return WideToUtf8(data, chars);
    }

    bool AreasRuntime::InitLE1()
    {
        void* gobjHit = FindCodePattern(PATTERN_GOBJOBJECTS_LE1);
        if (!gobjHit) {
            LOG_ERROR("LE1 areas: GObjObjects pattern not found");
            return false;
        }
        m_GObjObjectsGlobal = reinterpret_cast<void**>(ResolveRipRelative(gobjHit, 7));

        void* tlkHit = FindCodePattern(PATTERN_TLKTABLE_LE1);
        if (tlkHit) {
            m_TlkTableGlobal = reinterpret_cast<void**>(ResolveRipRelative(tlkHit, 7));
        } else {
            LOG_ERROR("LE1 areas: GTlkTable pattern not found");
        }

        HMODULE mod = ::GetModuleHandleW(nullptr);
        if (mod) {
            m_TlkLookupFn = reinterpret_cast<unsigned char*>(mod) + RVA_TLKLOOKUP_LE1;
            m_GetMapLabelsFn = reinterpret_cast<unsigned char*>(mod) + RVA_GETMAPLABELS_LE1;
        }

        LOG_INFO("LE1 areas: gobj=%p tlkTable=%p tlkFn=%p labelsFn=%p",
            reinterpret_cast<void*>(m_GObjObjectsGlobal), reinterpret_cast<void*>(m_TlkTableGlobal),
            m_TlkLookupFn, m_GetMapLabelsFn);
        return m_GObjObjectsGlobal != nullptr;
    }

    AreasRuntime& AreasRuntime::Instance()
    {
        static AreasRuntime instance;
        return instance;
    }

    bool AreasRuntime::Initialize(GameId Which)
    {
        if (Which == GameId::LE1) {
            return InitLE1();
        }
        return true;
    }

    std::string AreasRuntime::TlkLookupLE1(int StrRef)
    {
        if (!m_TlkLookupFn || !m_TlkTableGlobal || StrRef <= -2147000000) {
            return {};
        }
        void* table = reinterpret_cast<void*>(SafeReadPtr(reinterpret_cast<uintptr_t>(m_TlkTableGlobal)));
        if (!table) {
            return {};
        }
        GameFString out {};
        if (!CallTlkLookupSafe(m_TlkLookupFn, table, StrRef, &out)) {
            return {};
        }
        return FStringToUtf8(out);
    }

    std::string AreasRuntime::ResolveLE1(const std::string& PackageName, bool bPackageChanged)
    {
        if (!m_GObjObjectsGlobal || PackageName.empty()) {
            return {};
        }
        if (PackageName == "entrymenu") {
            return {};
        }
        if (PackageName == "BIOG_UIWorld") {
            return "In a cinematic";
        }

        auto* ctx = &GameContext::Instance();

        GObjView objects;
        if (!ReadGObjectsViewSafe(m_GObjObjectsGlobal, &objects)) {
            return {};
        }

        uintptr_t world = SafeReadPtr(reinterpret_cast<uintptr_t>(ctx->WorldSlot()));
        if (!world) {
            return {};
        }

        unsigned long long worldTopName60 = 0;
        if (!SafeOuterMostName(world, &worldTopName60)) {
            return {};
        }

        unsigned char** poolsRaw = ctx->NamePoolsRaw();
        const unsigned char** pools = const_cast<const unsigned char**>(poolsRaw);

        if (!m_WorldInfoScanned || !m_WorldInfo || m_WorldInfoScanObjCount != static_cast<int>(objects.Count)) {
            ScanCtx scan;
            scan.Pools = pools;
            scan.ObjData = objects.Data;
            scan.ObjCount = objects.Count;
            scan.WorldObj = reinterpret_cast<unsigned char*>(world);
            scan.WorldTopName60 = worldTopName60;

            unsigned char* found = nullptr;
            if (TryScanWorldInfoSafe(&scan, &found)) {
                m_WorldInfo = found;
                m_WorldInfoScanObjCount = static_cast<int>(objects.Count);
                m_WorldInfoScanned = true;
                LOG_INFO("LE1 areas: BioWorldInfo at %p (objects=%u)", reinterpret_cast<void*>(found), objects.Count);
            } else {
                m_WorldInfoScanObjCount = static_cast<int>(objects.Count);
                m_WorldInfo = nullptr;
                m_WorldInfoScanned = false;
                LOG_DEBUG("LE1 areas: BioWorldInfo not present now (%s)", PackageName.c_str());
                return {};
            }
        }

        MovieRaw raw;
        if (!ReadWorldMoviesSafe(reinterpret_cast<uintptr_t>(m_WorldInfo), &raw)) {
            LOG_DEBUG("LE1 areas: area movie absent (%s)", PackageName.c_str());
            return {};
        }

        unsigned long long movieName60 = SafeReadU64(raw.Movie + OFFU_Name);
        unsigned long long parentName60 = raw.Parent ? SafeReadU64(raw.Parent + OFFU_Name) : 0;

        std::string movieName;
        std::string parentName;
        if (movieName60) {
            char hex[64];
            snprintf(hex, sizeof(hex), "name60=%llu", static_cast<unsigned long long>(movieName60));
            (void)hex;
            bool ok = SafeNameEqualsAscii(pools, movieName60, "AreaMap_AreaMap", 15);
            (void)ok;
        }
        {
            auto* resolverNames = ctx;
            (void)resolverNames;
        }

        movieName = ctx->ResolveNameByValue(movieName60);
        parentName = ctx->ResolveNameByValue(parentName60);

        if (movieName != m_LastAreaMovieName || !m_LastAreaMovieResolved) {
            LOG_INFO("LE1 areas: movie='%s' parent='%s' pkg=%s",
                movieName.c_str(), parentName.c_str(), PackageName.c_str());
            m_LastAreaMovieName = movieName;
            m_LastAreaMovieResolved = true;
        }

        if (m_GetMapLabelsFn && bPackageChanged) {
            GameFString outL {};
            GameFString outP {};
            if (CallGetMapLabelsSafe(m_GetMapLabelsFn, &outL, &outP)) {
                std::string l = FStringToUtf8(outL);
                std::string p = FStringToUtf8(outP);
                LOG_INFO("LE1 areas: save labels='%s' parent='%s' pkg=%s",
                    l.c_str(), p.c_str(), PackageName.c_str());
            } else {
                LOG_DEBUG("LE1 areas: labels call faulted (%s)", PackageName.c_str());
            }
        }

        std::string movieSuffix;
        if (movieName.rfind("AreaMap_AreaMap_", 0) == 0 && movieName.size() > 16) {
            movieSuffix = movieName.substr(16);
        } else if (movieName.rfind("AreaMap_", 0) == 0 && movieName.size() > 8) {
            movieSuffix = movieName.substr(8);
        }
        if (movieSuffix.empty()) {
            return {};
        }

        bool allDigits = !movieSuffix.empty();
        for (char ch : movieSuffix) {
            if (ch < '0' || ch > '9') {
                allDigits = false;
                break;
            }
        }
        if (allDigits) {
            int strRef = atoi(movieSuffix.c_str());
            std::string tlk = TlkLookupLE1(strRef);
            LOG_INFO("LE1 areas: numeric suffix %s -> tlk='%s'", movieSuffix.c_str(), tlk.c_str());
            return tlk;
        }

        LOG_DEBUG("LE1 areas: suffix '%s' not auto-resolved (%s)", movieSuffix.c_str(), PackageName.c_str());
        return {};
    }

    std::string AreasRuntime::ResolveArea(GameId Which, const std::string& PackageName, bool bPackageChanged)
    {
        if (Which == GameId::LE1) {
            return ResolveLE1(PackageName, bPackageChanged);
        }
        return {};
    }

}
