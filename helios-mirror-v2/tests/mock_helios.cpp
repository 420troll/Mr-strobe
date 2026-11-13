#include "mirror_core.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <cstring>

extern "C" __declspec(dllexport) int OpenDevicesOnlyUsb() { return 2; }
extern "C" __declspec(dllexport) int CloseDevices() { return 1; }
extern "C" __declspec(dllexport) int GetStatus(unsigned int) { return 1; }
extern "C" __declspec(dllexport) int WriteFrame(
    unsigned int, int, std::uint8_t, trollfx::HeliosPoint*, int count) {
    return count > 0 ? 1 : -1;
}
extern "C" __declspec(dllexport) int Stop(unsigned int) { return 1; }
extern "C" __declspec(dllexport) int GetName(unsigned int, char* name) {
    std::strcpy(name, "ABI Mock Helios");
    return 1;
}

BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) { return TRUE; }
