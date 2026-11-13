#pragma once

#include <cstddef>
#include <cstdint>

#if defined(_WIN32)
#define ETHERDREAM_CALL __stdcall
#define ETHERDREAM_EXPORT extern "C" __declspec(dllexport)
#else
#define ETHERDREAM_CALL
#define ETHERDREAM_EXPORT extern "C"
#endif

namespace trollfx {

struct EadPoint {
    std::int16_t x;
    std::int16_t y;
    std::int16_t r;
    std::int16_t g;
    std::int16_t b;
    std::int16_t i;
    std::int16_t al;
    std::int16_t ar;
};

static_assert(sizeof(EadPoint) == 16, "EtherDream EAD_Pnt_s must be 16 bytes");
static_assert(offsetof(EadPoint, ar) == 14, "Unexpected EtherDream point layout");

constexpr int kEtherDreamReady = 1;
constexpr int kEtherDreamBusy = 2;

}  // namespace trollfx
