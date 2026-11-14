#include "etherdream_abi.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <iostream>

namespace {
template <typename T> T ordinal(HMODULE module, int number) {
    return reinterpret_cast<T>(GetProcAddress(module, MAKEINTRESOURCEA(number)));
}

bool require(bool value, const char* message) {
    if (!value) std::cerr << "ABI smoke failure: " << message << "\n";
    return value;
}
}  // namespace

int main() {
    HMODULE module = LoadLibraryW(L"EtherDream.dll");
    if (!module) {
        std::cerr << "ABI smoke failure: LoadLibrary error " << GetLastError() << "\n";
        return 1;
    }
    using GetCardNum = int(__stdcall*)();
    using GetName = void(__stdcall*)(const int*, char*, int);
    using Open = bool(__stdcall*)(const int*);
    using Status = int(__stdcall*)(const int*);
    using Write = bool(__stdcall*)(const int*, const trollfx::EadPoint*, int,
                                   std::uint16_t, std::uint16_t);
    using Stop = bool(__stdcall*)(const int*);
    using Close = bool(__stdcall*)(const int*);
    auto get_cards = ordinal<GetCardNum>(module, 19);
    auto get_name = ordinal<GetName>(module, 20);
    auto open = ordinal<Open>(module, 23);
    auto status = ordinal<Status>(module, 21);
    auto write = ordinal<Write>(module, 27);
    auto stop = ordinal<Stop>(module, 25);
    auto close = ordinal<Close>(module, 18);
    bool ok = require(get_cards && get_name && open && status && write && stop && close,
                      "one or more ordinal exports are missing");
    int card = 0;
    char name[64]{};
    trollfx::EadPoint points[2]{};
    if (ok) {
        ok &= require(get_cards() == 1, "virtual card count");
        get_name(&card, name, sizeof(name));
        ok &= require(name[0] != '\0', "device name");
        ok &= require(open(&card), "open");
        ok &= require(status(&card) == trollfx::kEtherDreamReady, "ready status");
        ok &= require(write(&card, points, sizeof(points), 30000, 1), "write");
        ok &= require(stop(&card), "stop");
        ok &= require(close(&card), "close");
    }
    FreeLibrary(module);
    if (ok) std::cout << "EtherDream ordinal ABI smoke test passed\n";
    return ok ? 0 : 1;
}
