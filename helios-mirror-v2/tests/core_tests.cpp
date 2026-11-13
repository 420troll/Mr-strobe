#include "helios_output.hpp"
#include "mirror_core.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace {

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
}

class MockApi final : public trollfx::IHeliosApi {
public:
    explicit MockApi(int count) : count(count) {}
    int open_devices_only_usb() override { ++open_calls; return count; }
    int close_devices() override { ++close_calls; return 1; }
    int get_status(unsigned int) override { return ready.load() ? 1 : 0; }
    int write_frame(unsigned int device, int pps, std::uint8_t flags,
                    trollfx::HeliosPoint* points, int point_count) override {
        std::lock_guard<std::mutex> lock(mutex);
        devices.push_back(device);
        rates.push_back(pps);
        frame_flags.push_back(flags);
        frames.emplace_back(points, points + point_count);
        ++write_calls;
        if (fail_next.exchange(false)) return -7;
        return 1;
    }
    int stop(unsigned int device) override { stops.push_back(device); return 1; }
    int get_name(unsigned int, char* name) override {
        std::strcpy(name, "Mock Helios");
        return 1;
    }

    int count;
    std::atomic<bool> ready{true};
    std::atomic<bool> fail_next{false};
    std::atomic<int> write_calls{0};
    int open_calls = 0;
    int close_calls = 0;
    std::mutex mutex;
    std::vector<unsigned int> devices;
    std::vector<int> rates;
    std::vector<std::uint8_t> frame_flags;
    std::vector<std::vector<trollfx::HeliosPoint>> frames;
    std::vector<unsigned int> stops;
};

bool wait_for(const std::atomic<int>& value, int expected) {
    for (int i = 0; i < 500; ++i) {
        if (value.load() >= expected) return true;
        std::this_thread::sleep_for(1ms);
    }
    return false;
}

trollfx::MirrorConfig live_config() {
    trollfx::MirrorConfig config;
    config.armed = true;
    for (auto& output : config.outputs) {
        output.enabled = true;
        output.blackout = false;
    }
    return config;
}

void test_conversion() {
    check(trollfx::ead_coordinate_to_helios(INT16_MIN) == 0, "X minimum maps to zero");
    check(trollfx::ead_coordinate_to_helios(INT16_MAX) == 4095, "X maximum maps to 4095");
    check(trollfx::ead_coordinate_to_helios(0) == 2048, "X zero maps to center");
    check(trollfx::ead_color_to_helios(-1) == 0, "negative color blanks");
    check(trollfx::ead_color_to_helios(16384) == 128, "signed full-scale color is reduced to 8-bit");
    check(trollfx::ead_color_to_helios(INT16_MAX) == 255, "color maximum maps to 255");

    trollfx::EadPoint point{INT16_MIN, INT16_MAX, INT16_MAX, 16384, 128, 25700, 7, 8};
    trollfx::OutputConfig output;
    output.blackout = false;
    output.transform.flip_x = true;
    output.transform.scale_y = 2.0;
    output.transform.offset_y = 1000;
    auto converted = trollfx::convert_point(point, output, true);
    check(converted.x == 4095, "flip X");
    check(converted.y == 4095, "scale/offset clips high");
    check(converted.r == 255 && converted.g == 128 && converted.i == 200,
          "RGBI conversion");
    converted = trollfx::convert_point(point, output, false);
    check(converted.r == 0 && converted.g == 0 && converted.b == 0 && converted.i == 0,
          "global disarm blanks all channels");
}

void test_discovery() {
    for (int devices = 0; devices <= 2; ++devices) {
        auto api = std::make_unique<MockApi>(devices);
        auto* raw = api.get();
        trollfx::DualHeliosRouter router(std::move(api));
        check(router.open() == devices, "fixture count is preserved");
        check(raw->open_calls == 1, "one SDK open lifecycle");
        router.close();
        check(raw->close_calls == 1, "one SDK close lifecycle");
    }
}

void test_dual_routing_and_ownership() {
    auto api = std::make_unique<MockApi>(2);
    auto* raw = api.get();
    raw->ready.store(false);
    trollfx::DualHeliosRouter router(std::move(api));
    check(router.open() == 2, "two devices opened");
    auto config = live_config();
    config.outputs[1].transform.flip_x = true;
    config.outputs[1].transform.offset_y = 10;
    trollfx::EadPoint source[2] = {
        {INT16_MIN, 0, 1285, 2570, 3855, 5140, 0, 0},
        {INT16_MAX, INT16_MAX, 6425, 7710, 8995, 10280, 0, 0}};
    check(router.submit(source, sizeof(source), 30000, 1, config), "frame accepted");
    source[0] = {};
    source[1] = {};
    raw->ready.store(true);
    check(wait_for(raw->write_calls, 2), "both device workers write");
    {
        std::lock_guard<std::mutex> lock(raw->mutex);
        check(raw->frames.size() == 2, "two immutable frame copies captured");
        std::size_t primary = raw->devices[0] == 0 ? 0 : 1;
        std::size_t secondary = 1 - primary;
        check(raw->frames[primary][0].x == 0, "primary keeps X alignment");
        check(raw->frames[secondary][0].x == 4095, "secondary transform is independent");
        check(raw->frames[primary][0].r == 10, "input ownership deep-copy");
        check(raw->frames[secondary][1].y == 4095, "offset clips without reordering");
    }
    router.close();
}

void test_ordering_stop_and_recovery() {
    auto api = std::make_unique<MockApi>(1);
    auto* raw = api.get();
    raw->ready.store(false);
    trollfx::DualHeliosRouter router(std::move(api));
    router.open();
    auto config = live_config();
    trollfx::EadPoint point{0, 0, 128, 0, 0, INT16_MAX, 0, 0};
    router.submit(&point, sizeof(point), 20000, 1, config);
    point.r = 257;
    router.submit(&point, sizeof(point), 20000, 1, config);
    point.r = 385;
    router.submit(&point, sizeof(point), 20000, 1, config);
    raw->ready.store(true);
    check(wait_for(raw->write_calls, 1), "latest bounded frame is written");
    std::this_thread::sleep_for(20ms);
    {
        std::lock_guard<std::mutex> lock(raw->mutex);
        check(raw->frames.back()[0].r == 3, "newest frame wins while busy");
    }
    check(router.stats().overwritten_frames[0] >= 1, "overwrite diagnostics count drops");

    raw->ready.store(false);
    point.r = 9;
    router.submit(&point, sizeof(point), 20000, 1, config);
    router.stop();
    const int before = raw->write_calls.load();
    raw->ready.store(true);
    std::this_thread::sleep_for(20ms);
    check(raw->write_calls.load() == before, "stop cancels queued frame without stale replay");

    raw->fail_next.store(true);
    point.r = 10;
    router.submit(&point, sizeof(point), 20000, 1, config);
    check(wait_for(raw->write_calls, before + 1), "failure attempt occurs");
    point.r = 11;
    router.submit(&point, sizeof(point), 20000, 1, config);
    check(wait_for(raw->write_calls, before + 2), "worker recovers after failure");
    check(router.stats().failed_frames[0] >= 1, "failure diagnostics increment");
    router.close();
}

void test_validation() {
    auto api = std::make_unique<MockApi>(1);
    trollfx::DualHeliosRouter router(std::move(api));
    router.open();
    auto config = live_config();
    trollfx::EadPoint point{};
    check(!router.submit(nullptr, sizeof(point), 30000, 1, config), "null input rejected");
    check(!router.submit(&point, sizeof(point) - 1, 30000, 1, config), "misaligned byte length rejected");
    check(!router.submit(&point, sizeof(point), 0, 1, config), "zero PPS rejected");
    check(router.stats().invalid_frames == 3, "invalid frame diagnostics");
    router.close();
}

}  // namespace

int main() {
    test_conversion();
    test_discovery();
    test_dual_routing_and_ownership();
    test_ordering_stop_and_recovery();
    test_validation();
    std::cout << "All core tests passed\n";
    return 0;
}
