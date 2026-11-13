// TROLLfx Helios Mirror experimental Windows x64 DLL proxy.
// All real Helios calls go through one loaded genuine DLL; no second SDK open.
// This is an ABI-specific prototype. Confirm Maxwell's import table before use.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#pragma pack(push, 1)
struct HeliosPoint {
    uint16_t x;
    uint16_t y;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t i;
};
#pragma pack(pop)
static_assert(sizeof(HeliosPoint) == 8, "Unexpected point size");
using FnOpen = int (*)();
using FnClose = int (*)();
using FnStatus = int (*)(int);
using FnWrite = int (*)(int, int, int, const HeliosPoint*, int);
using FnStop = int (*)(int);
using FnGetName = int (*)(int, char*);
struct SDK {
    HMODULE dll = nullptr;
    FnOpen open = nullptr;
    FnClose close = nullptr;
    FnStatus status = nullptr;
    FnWrite write = nullptr;
    FnStop stop = nullptr;
    FnGetName name = nullptr;
};
static SDK sdk;
static std::once_flag sdk_once;
static std::atomic<int> opened_devices{0};
static std::atomic<bool> opened{false};

struct Config {
    bool armed = false;
    bool mirror = true;
    bool flip_x = false;
    bool flip_y = false;
    double size = 1.0;
    int offset_x = 0, offset_y = 0;
};
static std::mutex config_mutex;
static Config config;
static std::chrono::steady_clock::time_point last_read{};
static std::filesystem::path config_path;

static std::filesystem::path local_path(const wchar_t* name) {
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&local_path), &self);
    wchar_t path[MAX_PATH]{};
    if (!self || !GetModuleFileNameW(self, path, MAX_PATH)) return name;
    return std::filesystem::path(path).parent_path() / name;
}

static void load_real_sdk() {
    auto libpath = local_path(L"HeliosLaserDAC-real.dll");
    sdk.dll = LoadLibraryW(libpath.c_str());
    if (!sdk.dll) return;
    auto symbol = [](const char* name) { return GetProcAddress(sdk.dll, name); };
    sdk.open = reinterpret_cast<FnOpen>(symbol("HeliosDacOpenDevices"));
    sdk.close = reinterpret_cast<FnClose>(symbol("HeliosDacCloseDevices"));
    sdk.status = reinterpret_cast<FnStatus>(symbol("HeliosDacGetStatus"));
    sdk.write = reinterpret_cast<FnWrite>(symbol("HeliosDacWriteFrame"));
    sdk.stop = reinterpret_cast<FnStop>(symbol("HeliosDacStop"));
    sdk.name = reinterpret_cast<FnGetName>(symbol("HeliosDacGetName"));
    config_path = local_path(L"mirror.ini");
}
static void init() { std::call_once(sdk_once, load_real_sdk); }
static int integer(const std::string& text, int fallback) {
    try { return std::stoi(text); } catch (...) { return fallback; }
}
static double real(const std::string& text, double fallback) {
    try { return std::stod(text); } catch (...) { return fallback; }
}
static Config get_config() {
    std::lock_guard<std::mutex> lock(config_mutex);
    const auto now = std::chrono::steady_clock::now();
    if (last_read.time_since_epoch().count() == 0 ||
        now - last_read > std::chrono::milliseconds(500)) {
        last_read = now;
        // Missing config: default to disarmed. Never inherit prior arm state.
        Config next;
        std::ifstream in(config_path);
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#' || line[0] == ';') continue;
            const size_t sep = line.find('=');
            if (sep == std::string::npos) continue;
            const std::string k = line.substr(0, sep);
            const std::string v = line.substr(sep + 1);
            if (k == "armed") next.armed = integer(v, 0) != 0;
            else if (k == "mirror_enabled") next.mirror = integer(v, 1) != 0;
            else if (k == "flip_x") next.flip_x = integer(v, 0) != 0;
            else if (k == "flip_y") next.flip_y = integer(v, 0) != 0;
            else if (k == "size") next.size = std::clamp(real(v, 1), 0.0, 1.0);
            else if (k == "offset_x") next.offset_x = std::clamp(integer(v, 0), -2048, 2048);
            else if (k == "offset_y") next.offset_y = std::clamp(integer(v, 0), -2048, 2048);
        }
        config = next;
    }
    return config;
}
static uint16_t transform(uint16_t coord, bool flip, double scale, int offset) {
    double centered = static_cast<double>(coord) - 2047.5;
    if (flip) centered = -centered;
    const int output = static_cast<int>(std::lround(2047.5 + centered * scale + offset));
    return static_cast<uint16_t>(std::clamp(output, 0, 4095));
}
static void black(HeliosPoint& pt) {
    pt.r = pt.g = pt.b = pt.i = 0;
}
extern "C" __declspec(dllexport) int HeliosDacOpenDevices() {
    init();
    if (!sdk.open || !sdk.write || !sdk.status) return -1;
    // Keep exactly one SDK lifecycle. Repeated Maxwell OpenDevices() calls
    // return current device count rather than opening twice.
    if (opened.load()) return opened_devices.load();
    const int count = sdk.open();
    if (count >= 0) {
        opened_devices.store(count);
        opened.store(true);
    }
    return count;
}
extern "C" __declspec(dllexport) int HeliosDacCloseDevices() {
    init();
    if (!sdk.close || !opened.exchange(false)) return 0;
    opened_devices.store(0);
    return sdk.close();
}
extern "C" __declspec(dllexport) int HeliosDacGetStatus(int device) {
    init();
    return sdk.status ? sdk.status(device) : -1;
}
extern "C" __declspec(dllexport) int HeliosDacStop(int device) {
    init();
    if (!sdk.stop) return -1;
    const int result = sdk.stop(device);
    if (opened_devices.load() >= 2 && device == 0) sdk.stop(1);
    return result;
}
extern "C" __declspec(dllexport) int HeliosDacGetName(int device, char* name) {
    init();
    return sdk.name ? sdk.name(device, name) : -1;
}
extern "C" __declspec(dllexport) int HeliosDacWriteFrame(
    int device, int pps, int flags, const HeliosPoint* input, int count) {
    init();
    if (!sdk.write || !input || count <= 0 || count > 65536) return -1;
    if (device < 0 || device >= opened_devices.load()) return -1;
    const Config cfg = get_config();
    // Never mutate Maxwell's input buffer.
    std::vector<HeliosPoint> primary(input, input + count);
    if (!cfg.armed) for (auto& pt : primary) black(pt);
    const int result = sdk.write(device, pps, flags, primary.data(), count);
    if (device != 0 || !cfg.mirror || opened_devices.load() < 2) return result;
    if (!sdk.status || sdk.status(1) != 1) return result;
    std::vector<HeliosPoint> secondary(input, input + count);
    for (auto& pt : secondary) {
        pt.x = transform(pt.x, cfg.flip_x, cfg.size, cfg.offset_x);
        pt.y = transform(pt.y, cfg.flip_y, cfg.size, cfg.offset_y);
        if (!cfg.armed) black(pt);
    }
    sdk.write(1, pps, flags, secondary.data(), count);
    return result;
}
