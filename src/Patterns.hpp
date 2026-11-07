#pragma once

#include <cstdint>
#include <windows.h>

namespace mele {

    struct Pattern {
        const char* Bytes = nullptr;
        size_t Length = 0;
    };

    constexpr size_t PatternLength(const char* In)
    {
        size_t n = 0;
        while (*In) {
            ++n; ++In;
        }
        return n;
    }

    void* FindCodePattern(const char* PatternText, size_t SearchStart = 0);
    void* ResolveRipRelative(void* PatternHit, size_t InstructionLength);
    bool GetMainModuleTextSpan(void*& OutStart, size_t& OutSize);

}
