#pragma once

#include <string>
#include <unordered_map>

namespace mele {

    const std::unordered_map<std::string, const char*>& MapDictionaryLE1();
    const std::unordered_map<std::string, const char*>& MapDictionaryLE2();
    const std::unordered_map<std::string, const char*>& MapDictionaryLE3();

    const char* LookupMapName(int GameIndex, const std::string& PackageName);

}
