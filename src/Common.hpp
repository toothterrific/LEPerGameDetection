#pragma once

#include <string>
#include <string_view>
#include <cstdint>
#include <windows.h>

namespace mele {

    using UQWORD = uint64_t;
    using UDWORD = uint32_t;
    using UBYTE = uint8_t;

    std::string ToLowerAscii(std::string_view In);
    std::string WideToUtf8(const wchar_t* In, size_t InCharCount);
    std::string WideToUtf8(const std::wstring& In);

    long long NowEpochMs();
    long long NowTickMs();

    constexpr const char* k_IpcPipePrefix = "\\\\?\\pipe\\discord-ipc-";

}
