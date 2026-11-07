#include "Common.hpp"

namespace mele {

    long long NowEpochMs()
    {
        FILETIME ft;
        ::GetSystemTimeAsFileTime(&ft);
        unsigned long long ft64 = (static_cast<unsigned long long>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
        return static_cast<long long>((ft64 - 116444736000000000ull) / 10000ull);
    }

    std::string ToLowerAscii(std::string_view In)
    {
        std::string out;
        out.reserve(In.size());
        for (char c : In) {
            out.push_back(c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c);
        }
        return out;
    }

    std::string WideToUtf8(const wchar_t* In, size_t InCharCount)
    {
        if (!In || InCharCount == 0) {
            return {};
        }
        if (InCharCount > 0x40000000u) {
            return {};
        }
        int utf8Bytes = ::WideCharToMultiByte(CP_UTF8, 0, In, static_cast<int>(InCharCount), nullptr, 0, nullptr, nullptr);
        if (utf8Bytes <= 0) {
            return {};
        }
        std::string out(static_cast<size_t>(utf8Bytes), '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, In, static_cast<int>(InCharCount), out.data(), utf8Bytes, nullptr, nullptr);
        return out;
    }

    std::string WideToUtf8(const std::wstring& In)
    {
        return WideToUtf8(In.data(), In.size());
    }

}
