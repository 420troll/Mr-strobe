#include "etherdream_abi.hpp"
#include "helios_output.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>

namespace trollfx {
namespace {

std::filesystem::path module_directory() {
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&module_directory), &self);
    std::wstring path(32768, L'\0');
    const DWORD size = GetModuleFileNameW(self, path.data(),
                                          static_cast<DWORD>(path.size()));
    path.resize(size);
    return std::filesystem::path(path).parent_path();
}

std::string trim(std::string value) {
    const auto not_space = [](unsigned char c) { return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

bool boolean(const std::string& value, bool fallback) {
    if (value == "1" || value == "true" || value == "yes" || value == "on") return true;
    if (value == "0" || value == "false" || value == "no" || value == "off") return false;
    return fallback;
}

int integer(const std::string& value, int fallback) {
    try { return std::stoi(value); } catch (...) { return fallback; }
}

double real(const std::string& value, double fallback) {
    try { return std::stod(value); } catch (...) { return fallback; }
}

MirrorConfig load_config(const std::filesystem::path& path) {
    MirrorConfig result;
    // Every reload starts safe. Arming is never inherited from a previous file.
    result.armed = false;
    result.outputs[0].blackout = true;
    result.outputs[1].blackout = true;

    std::ifstream input(path);
    std::string section;
    std::string line;
    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        if (line.front() == '[' && line.back() == ']') {
            section = trim(line.substr(1, line.size() - 2));
            continue;
        }
        const auto separator = line.find('=');
        if (separator == std::string::npos) continue;
        const auto key = trim(line.substr(0, separator));
        const auto value = trim(line.substr(separator + 1));
        if ((section.empty() || section == "mirror") && key == "armed") {
            result.armed = boolean(value, false);
            continue;
        }
        int index = section == "output0" ? 0 : (section == "output1" ? 1 : -1);
        if (index < 0) continue;
        auto& output = result.outputs[index];
        if (key == "enabled") output.enabled = boolean(value, true);
        else if (key == "blackout") output.blackout = boolean(value, true);
        else if (key == "flip_x") output.transform.flip_x = boolean(value, false);
        else if (key == "flip_y") output.transform.flip_y = boolean(value, false);
        else if (key == "scale_x") output.transform.scale_x = std::clamp(real(value, 1.0), 0.0, 2.0);
        else if (key == "scale_y") output.transform.scale_y = std::clamp(real(value, 1.0), 0.0, 2.0);
        else if (key == "offset_x") output.transform.offset_x = std::clamp(integer(value, 0), -4095, 4095);
        else if (key == "offset_y") output.transform.offset_y = std::clamp(integer(value, 0), -4095, 4095);
    }
    return result;
}

class Bridge {
public:
    Bridge()
        : directory_(module_directory()), config_path_(directory_ / L"mirror.ini"),
          log_path_(directory_ / L"helios-mirror-v2.log"),
          router_(make_dynamic_helios_api(
              (directory_ / L"HeliosLaserDAC.dll").wstring())) {}

    void log(const std::string& operation, const std::string& details = {}) {
        std::lock_guard<std::mutex> lock(log_mutex_);
        std::ofstream output(log_path_, std::ios::app);
        if (!output) return;
        const auto now = std::chrono::system_clock::now();
        const auto time = std::chrono::system_clock::to_time_t(now);
        std::tm local{};
        localtime_s(&local, &time);
        output << std::put_time(&local, "%Y-%m-%dT%H:%M:%S") << " "
               << operation;
        if (!details.empty()) output << " " << details;
        output << "\n";
    }

    MirrorConfig config() {
        std::lock_guard<std::mutex> lock(config_mutex_);
        const auto now = std::chrono::steady_clock::now();
        if (last_config_read_.time_since_epoch().count() == 0 ||
            now - last_config_read_ >= std::chrono::milliseconds(500)) {
            cached_config_ = load_config(config_path_);
            last_config_read_ = now;
        }
        return cached_config_;
    }

    DualHeliosRouter& router() { return router_; }

private:
    std::filesystem::path directory_;
    std::filesystem::path config_path_;
    std::filesystem::path log_path_;
    std::mutex log_mutex_;
    std::mutex config_mutex_;
    std::chrono::steady_clock::time_point last_config_read_{};
    MirrorConfig cached_config_{};
    DualHeliosRouter router_;
};

Bridge& bridge() {
    // Avoid joining worker threads from a static destructor under the Windows
    // loader lock. Maxwell is expected to call CloseDevice explicitly.
    static Bridge* instance = new Bridge();
    return *instance;
}

bool valid_card(const int* card) { return card && *card == 0; }

}  // namespace
}  // namespace trollfx

ETHERDREAM_EXPORT int ETHERDREAM_CALL EtherDreamGetCardNum() {
    trollfx::bridge().log("EtherDreamGetCardNum", "result=1");
    return 1;
}

ETHERDREAM_EXPORT void ETHERDREAM_CALL EtherDreamGetDeviceName(
    const int* card, char* buffer, int maximum) {
    if (!buffer || maximum <= 0) return;
    buffer[0] = '\0';
    if (!trollfx::valid_card(card)) return;
    const char name[] = "TROLLfx Helios Mirror V2";
    strncpy_s(buffer, static_cast<std::size_t>(maximum), name, _TRUNCATE);
    trollfx::bridge().log("EtherDreamGetDeviceName", "card=0");
}

ETHERDREAM_EXPORT bool ETHERDREAM_CALL EtherDreamOpenDevice(const int* card) {
    if (!trollfx::valid_card(card)) return false;
    const int devices = trollfx::bridge().router().open();
    trollfx::bridge().log("EtherDreamOpenDevice",
                          "card=0 helios_devices=" + std::to_string(devices));
    // This is a virtual EtherDream device. Opening it succeeds even with zero
    // Helios DACs so diagnostic capture can prove Maxwell's call path.
    return devices >= 0;
}

ETHERDREAM_EXPORT int ETHERDREAM_CALL EtherDreamGetStatus(const int* card) {
    const int result = trollfx::valid_card(card)
                           ? trollfx::bridge().router().virtual_status()
                           : 0;
    trollfx::bridge().log("EtherDreamGetStatus", "result=" + std::to_string(result));
    return result;
}

ETHERDREAM_EXPORT bool ETHERDREAM_CALL EtherDreamWriteFrame(
    const int* card, const trollfx::EadPoint* points, int bytes,
    std::uint16_t pps, std::uint16_t repetitions) {
    if (!trollfx::valid_card(card)) return false;
    const int samples = bytes > 0 ? bytes / static_cast<int>(sizeof(trollfx::EadPoint)) : 0;
    std::ostringstream details;
    details << "card=0 bytes=" << bytes << " samples=" << samples
            << " pps=" << pps << " repetitions=" << repetitions;
    const bool result = trollfx::bridge().router().submit(
        points, bytes, pps, repetitions, trollfx::bridge().config());
    details << " accepted=" << (result ? 1 : 0);
    trollfx::bridge().log("EtherDreamWriteFrame", details.str());
    return result;
}

ETHERDREAM_EXPORT bool ETHERDREAM_CALL EtherDreamStop(const int* card) {
    if (!trollfx::valid_card(card)) return false;
    trollfx::bridge().router().stop();
    trollfx::bridge().log("EtherDreamStop", "card=0");
    return true;
}

ETHERDREAM_EXPORT bool ETHERDREAM_CALL EtherDreamCloseDevice(const int* card) {
    if (!trollfx::valid_card(card)) return false;
    trollfx::bridge().router().close();
    trollfx::bridge().log("EtherDreamCloseDevice", "card=0");
    return true;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(instance);
    return TRUE;
}
