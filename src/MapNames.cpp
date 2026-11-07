#include "MapNames.hpp"

#include <initializer_list>

namespace mele {

    namespace {

        struct Pair { const char* key; const char* value; };

        const std::unordered_map<std::string, const char*>& BuildDict(std::initializer_list<Pair> In)
        {
            static std::unordered_map<std::string, const char*> dict;
            static bool built = false;
            if (!built) {
                for (auto& p : In) {
                    dict.emplace(p.key, p.value);
                }
                built = true;
            }
            return dict;
        }

    }

    const std::unordered_map<std::string, const char*>& MapDictionaryLE1()
    {
        return BuildDict({
            { "entrymenu", "In main menu" },
        });
    }

    const std::unordered_map<std::string, const char*>& MapDictionaryLE2()
    {
        return BuildDict({
            { "entrymenu", "In main menu" },
        });
    }

    const std::unordered_map<std::string, const char*>& MapDictionaryLE3()
    {
        return BuildDict({
            { "entrymenu", "In main menu" },
        });
    }

    const char* LookupMapName(int GameIndex, const std::string& PackageName)
    {
        const std::unordered_map<std::string, const char*>* table = nullptr;
        switch (GameIndex) {
        case 1: table = &MapDictionaryLE1(); break;
        case 2: table = &MapDictionaryLE2(); break;
        case 3: table = &MapDictionaryLE3(); break;
        default: return nullptr;
        }
        auto it = table->find(PackageName);
        if (it != table->end()) {
            return it->second;
        }
        return nullptr;
    }

}
