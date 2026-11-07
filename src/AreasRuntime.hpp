#pragma once

#include <string>

namespace mele {

enum class GameId;

class AreasRuntime {
public:
    static AreasRuntime& Instance();

    bool Initialize(GameId Which);

    std::string ResolveArea(GameId Which, const std::string& PackageName, bool bPackageChanged);

    std::string TlkLookupLE1(int StrRef);

private:
    AreasRuntime() = default;

    bool InitLE1();

    std::string ResolveLE1(const std::string& PackageName, bool bPackageChanged);

    void** m_GObjObjectsGlobal = nullptr;
    void** m_GObjArraySlot = nullptr;
    void** m_GObjCountSlot = nullptr;

    void** m_TlkTableGlobal = nullptr;
    void* m_TlkLookupFn = nullptr;
    void* m_GetMapLabelsFn = nullptr;

    unsigned char* m_WorldInfo = nullptr;
    int m_WorldInfoScanObjCount = -1;
    bool m_WorldInfoScanned = false;

    unsigned char* m_LastAreaMovie = nullptr;
    std::string m_LastAreaMovieName;
    bool m_LastAreaMovieResolved = false;
};

}
