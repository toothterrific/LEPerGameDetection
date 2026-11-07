#include "Patterns.hpp"
#include "Common.hpp"
#include "Log.hpp"

#include <map>

namespace mele {

    static std::map<const char*, void*> g_PatternCache;

    bool GetMainModuleTextSpan(void*& OutStart, size_t& OutSize)
    {
        HMODULE mod = ::GetModuleHandleW(nullptr);
        if (!mod) {
            return false;
        }
        auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(mod);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
            return false;
        }
        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(reinterpret_cast<uint8_t*>(mod) + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) {
            return false;
        }
        auto* sec = IMAGE_FIRST_SECTION(nt);
        for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
            auto& s = sec[i];
            if ((s.Characteristics & IMAGE_SCN_MEM_EXECUTE) && memcmp(s.Name, ".text", 5) == 0) {
                OutStart = reinterpret_cast<uint8_t*>(mod) + s.VirtualAddress;
                OutSize = s.Misc.VirtualSize;
                return true;
            }
        }
        return false;
    }

    static bool MatchesPattern(const uint8_t* Text, size_t TextSize, size_t Offset, const char* PatternText)
    {
        const size_t patternSize = PatternLength(PatternText);
        if (Offset + (patternSize / 2) > TextSize) {
            return false;
        }

        const char* p = PatternText;
        const uint8_t* cursor = Text + Offset;

        while (*p) {
            if (p[0] == '?') {
                if (p[1] != '?') return false;
                p += 2;
                cursor += 1;
                continue;
            }
            if (p[0] == ' ' || p[0] == '\t') {
                p += 1;
                continue;
            }

            auto hexVal = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
                };

            int hi = hexVal(p[0]);
            if (hi < 0) return false;
            p += 1;
            while (*p == ' ' || *p == '\t') p += 1;
            int lo = hexVal(p[0]);
            if (lo < 0) return false;
            p += 1;

            uint8_t value = static_cast<uint8_t>((hi << 4) | lo);
            if (*cursor != value) {
                return false;
            }
            cursor += 1;
        }
        return true;
    }

    void* FindCodePattern(const char* PatternText, size_t SearchStart)
    {
        auto it = g_PatternCache.find(PatternText);
        if (it != g_PatternCache.end()) {
            return it->second;
        }

        void* textStart = nullptr;
        size_t textSize = 0;
        if (!GetMainModuleTextSpan(textStart, textSize)) {
            LOG_ERROR("Could not locate main module .text section");
            return nullptr;
        }

        auto* text = static_cast<const uint8_t*>(textStart);
        const size_t patternSize = PatternLength(PatternText);
        const size_t byteCount = patternSize / 2;

        for (size_t i = SearchStart; i + byteCount <= textSize; ++i) {
            if (MatchesPattern(text, textSize, i, PatternText)) {
                void* hit = const_cast<uint8_t*>(text + i);
                g_PatternCache[PatternText] = hit;
                return hit;
            }
        }

        LOG_ERROR("Pattern not found in .text (pattern text length=%llu)", static_cast<unsigned long long>(patternSize));
        return nullptr;
    }

    void* ResolveRipRelative(void* PatternHit, size_t InstructionLength)
    {
        if (!PatternHit || InstructionLength < 7) {
            return nullptr;
        }
        auto* bytes = static_cast<const uint8_t*>(PatternHit);
        int32_t disp = 0;
        memcpy(&disp, bytes + 3, sizeof(int32_t));
        uint8_t* rip = static_cast<uint8_t*>(PatternHit) + InstructionLength;
        return rip + disp;
    }

}
