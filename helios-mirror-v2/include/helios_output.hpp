#pragma once

#include "mirror_core.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace trollfx {

class IHeliosApi {
public:
    virtual ~IHeliosApi() = default;
    virtual int open_devices_only_usb() = 0;
    virtual int close_devices() = 0;
    virtual int get_status(unsigned int device) = 0;
    virtual int write_frame(unsigned int device, int pps, std::uint8_t flags,
                            HeliosPoint* points, int count) = 0;
    virtual int stop(unsigned int device) = 0;
    virtual int get_name(unsigned int device, char* name) = 0;
};

struct RouterStats {
    std::uint64_t accepted_frames = 0;
    std::uint64_t invalid_frames = 0;
    std::uint64_t overwritten_frames[2]{};
    std::uint64_t written_frames[2]{};
    std::uint64_t failed_frames[2]{};
    int discovered_devices = 0;
};

class DualHeliosRouter {
public:
    explicit DualHeliosRouter(std::unique_ptr<IHeliosApi> api);
    ~DualHeliosRouter();
    DualHeliosRouter(const DualHeliosRouter&) = delete;
    DualHeliosRouter& operator=(const DualHeliosRouter&) = delete;

    int open();
    void close();
    bool submit(const EadPoint* points, int bytes, std::uint16_t pps,
                std::uint16_t repetitions, const MirrorConfig& config);
    void stop();
    int virtual_status() const;
    int discovered_devices() const;
    RouterStats stats() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

std::unique_ptr<IHeliosApi> make_dynamic_helios_api(const std::wstring& dll_path);

}  // namespace trollfx
