#include "helios_output.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace trollfx {
namespace {

constexpr int kMaxPoints = 0xFFF;
constexpr std::uint8_t kSingleMode = 1u << 1;
constexpr std::uint8_t kDontBlock = 1u << 2;

struct Packet {
    std::vector<HeliosPoint> points;
    int pps = 30000;
    std::uint8_t flags = kSingleMode | kDontBlock;
    std::uint16_t repetitions = 1;
    std::uint64_t generation = 0;
};

struct AtomicStats {
    std::atomic<std::uint64_t> accepted{0};
    std::atomic<std::uint64_t> invalid{0};
    std::array<std::atomic<std::uint64_t>, 2> overwritten{};
    std::array<std::atomic<std::uint64_t>, 2> written{};
    std::array<std::atomic<std::uint64_t>, 2> failed{};
};

class Worker {
public:
    Worker(IHeliosApi& api, unsigned int device, AtomicStats& stats,
           std::atomic<int>& pending)
        : api_(api), device_(device), stats_(stats), pending_(pending),
          thread_(&Worker::run, this) {}

    ~Worker() { shutdown(); }

    void submit(Packet packet) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (packet_) {
                stats_.overwritten[device_].fetch_add(1);
            } else {
                pending_.fetch_add(1);
            }
            packet_ = std::move(packet);
        }
        cv_.notify_one();
    }

    void cancel(std::uint64_t generation) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            generation_ = generation;
            if (packet_) {
                packet_.reset();
                pending_.fetch_sub(1);
            }
        }
        cv_.notify_all();
        api_.stop(device_);
    }

    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (done_) return;
            done_ = true;
            if (packet_) {
                packet_.reset();
                pending_.fetch_sub(1);
            }
        }
        cv_.notify_all();
        if (thread_.joinable()) thread_.join();
    }

private:
    void run() {
        for (;;) {
            Packet packet;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [&] { return done_ || packet_.has_value(); });
                if (done_) return;
                packet = std::move(*packet_);
                packet_.reset();
            }

            bool cancelled = false;
            const std::uint32_t play_count =
                packet.repetitions == UINT16_MAX ? 1u : packet.repetitions;
            for (std::uint32_t play = 0; play < play_count && !cancelled; ++play) {
                int status = 0;
                for (int attempt = 0; attempt < 100; ++attempt) {
                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        if (done_ || packet.generation != generation_) {
                            cancelled = true;
                            break;
                        }
                        // Bound latency: abandon finite repetitions or a frame
                        // waiting on busy hardware when a newer frame arrives.
                        if (packet_) {
                            stats_.overwritten[device_].fetch_add(1);
                            cancelled = true;
                            break;
                        }
                    }
                    status = api_.get_status(device_);
                    if (status == 1 || status < 0) break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                if (cancelled) break;
                if (status != 1) {
                    stats_.failed[device_].fetch_add(1);
                    break;
                }
                const int result = api_.write_frame(device_, packet.pps,
                    packet.flags, packet.points.data(),
                    static_cast<int>(packet.points.size()));
                if (result > 0) {
                    stats_.written[device_].fetch_add(1);
                } else {
                    stats_.failed[device_].fetch_add(1);
                    break;
                }
            }
            pending_.fetch_sub(1);
        }
    }

    IHeliosApi& api_;
    unsigned int device_;
    AtomicStats& stats_;
    std::atomic<int>& pending_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::optional<Packet> packet_;
    bool done_ = false;
    std::uint64_t generation_ = 0;
    std::thread thread_;
};

#if defined(_WIN32)
class DynamicHeliosApi final : public IHeliosApi {
public:
    explicit DynamicHeliosApi(const std::wstring& path) {
        module_ = LoadLibraryExW(path.c_str(), nullptr,
                                 LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
                                     LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!module_) return;
        open_ = load<OpenFn>("OpenDevicesOnlyUsb");
        close_ = load<CloseFn>("CloseDevices");
        status_ = load<StatusFn>("GetStatus");
        write_ = load<WriteFn>("WriteFrame");
        stop_ = load<StopFn>("Stop");
        name_ = load<NameFn>("GetName");
        if (!open_ || !close_ || !status_ || !write_ || !stop_ || !name_) {
            FreeLibrary(module_);
            module_ = nullptr;
        }
    }

    ~DynamicHeliosApi() override {
        if (module_) FreeLibrary(module_);
    }
    int open_devices_only_usb() override { return open_ ? open_() : -1; }
    int close_devices() override { return close_ ? close_() : -1; }
    int get_status(unsigned int d) override { return status_ ? status_(d) : -1; }
    int write_frame(unsigned int d, int pps, std::uint8_t flags,
                    HeliosPoint* points, int count) override {
        return write_ ? write_(d, pps, flags, points, count) : -1;
    }
    int stop(unsigned int d) override { return stop_ ? stop_(d) : -1; }
    int get_name(unsigned int d, char* name) override {
        return name_ ? name_(d, name) : -1;
    }

private:
    template <typename T> T load(const char* symbol) {
        return reinterpret_cast<T>(GetProcAddress(module_, symbol));
    }
    using OpenFn = int (*)();
    using CloseFn = int (*)();
    using StatusFn = int (*)(unsigned int);
    using WriteFn = int (*)(unsigned int, int, std::uint8_t, HeliosPoint*, int);
    using StopFn = int (*)(unsigned int);
    using NameFn = int (*)(unsigned int, char*);
    HMODULE module_ = nullptr;
    OpenFn open_ = nullptr;
    CloseFn close_ = nullptr;
    StatusFn status_ = nullptr;
    WriteFn write_ = nullptr;
    StopFn stop_ = nullptr;
    NameFn name_ = nullptr;
};
#endif

}  // namespace

class DualHeliosRouter::Impl {
public:
    explicit Impl(std::unique_ptr<IHeliosApi> source) : api(std::move(source)) {}

    std::unique_ptr<IHeliosApi> api;
    std::array<std::unique_ptr<Worker>, 2> workers;
    AtomicStats stats;
    std::atomic<int> pending{0};
    std::atomic<int> device_count{0};
    std::atomic<bool> opened{false};
    std::atomic<std::uint64_t> generation{0};
    std::mutex lifecycle;
};

DualHeliosRouter::DualHeliosRouter(std::unique_ptr<IHeliosApi> api)
    : impl_(std::make_unique<Impl>(std::move(api))) {}

DualHeliosRouter::~DualHeliosRouter() { close(); }

int DualHeliosRouter::open() {
    std::lock_guard<std::mutex> lock(impl_->lifecycle);
    if (impl_->opened.load()) return impl_->device_count.load();
    if (!impl_->api) return -1;
    const int count = impl_->api->open_devices_only_usb();
    impl_->device_count.store(std::max(count, 0));
    if (count < 0) return count;
    impl_->opened.store(true);
    for (int i = 0; i < std::min(count, 2); ++i) {
        impl_->workers[i] = std::make_unique<Worker>(
            *impl_->api, static_cast<unsigned int>(i), impl_->stats,
            impl_->pending);
    }
    return count;
}

void DualHeliosRouter::close() {
    std::lock_guard<std::mutex> lock(impl_->lifecycle);
    if (!impl_->opened.exchange(false)) return;
    for (auto& worker : impl_->workers) {
        if (worker) worker->shutdown();
        worker.reset();
    }
    impl_->pending.store(0);
    impl_->api->close_devices();
    impl_->device_count.store(0);
}

bool DualHeliosRouter::submit(const EadPoint* points, int bytes,
                              std::uint16_t pps, std::uint16_t repetitions,
                              const MirrorConfig& config) {
    if (!impl_->opened.load() || !points || bytes <= 0 ||
        bytes % static_cast<int>(sizeof(EadPoint)) != 0 || pps == 0) {
        impl_->stats.invalid.fetch_add(1);
        return false;
    }
    const int count = bytes / static_cast<int>(sizeof(EadPoint));
    if (count <= 0 || count > kMaxPoints) {
        impl_->stats.invalid.fetch_add(1);
        return false;
    }

    // The reference driver treats a zero-repetition call as a successful no-op.
    if (repetitions == 0) {
        impl_->stats.accepted.fetch_add(1);
        return true;
    }

    const auto generation = impl_->generation.load();
    const auto flags = static_cast<std::uint8_t>(
        (repetitions != UINT16_MAX ? kSingleMode : 0u) | kDontBlock);
    bool submitted = false;
    for (int i = 0; i < std::min(impl_->device_count.load(), 2); ++i) {
        if (!impl_->workers[i]) continue;
        Packet packet;
        packet.points = convert_frame(points, static_cast<std::size_t>(count),
                                      config.outputs[i], config.armed);
        packet.pps = static_cast<int>(pps);
        packet.flags = flags;
        packet.repetitions = repetitions;
        packet.generation = generation;
        impl_->workers[i]->submit(std::move(packet));
        submitted = true;
    }
    if (submitted) impl_->stats.accepted.fetch_add(1);
    else impl_->stats.failed[0].fetch_add(1);
    return submitted;
}

void DualHeliosRouter::stop() {
    const auto generation = impl_->generation.fetch_add(1) + 1;
    for (auto& worker : impl_->workers) {
        if (worker) worker->cancel(generation);
    }
}

int DualHeliosRouter::virtual_status() const {
    if (!impl_->opened.load()) return 0;
    return impl_->pending.load() == 0 ? kEtherDreamReady : kEtherDreamBusy;
}

int DualHeliosRouter::discovered_devices() const {
    return impl_->device_count.load();
}

RouterStats DualHeliosRouter::stats() const {
    RouterStats result;
    result.accepted_frames = impl_->stats.accepted.load();
    result.invalid_frames = impl_->stats.invalid.load();
    for (int i = 0; i < 2; ++i) {
        result.overwritten_frames[i] = impl_->stats.overwritten[i].load();
        result.written_frames[i] = impl_->stats.written[i].load();
        result.failed_frames[i] = impl_->stats.failed[i].load();
    }
    result.discovered_devices = impl_->device_count.load();
    return result;
}

std::unique_ptr<IHeliosApi> make_dynamic_helios_api(const std::wstring& path) {
#if defined(_WIN32)
    return std::make_unique<DynamicHeliosApi>(path);
#else
    (void)path;
    return {};
#endif
}

}  // namespace trollfx
