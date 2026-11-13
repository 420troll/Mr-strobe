#pragma once

#include "etherdream_abi.hpp"

#include <cstdint>
#include <vector>

namespace trollfx {

struct HeliosPoint {
    std::uint16_t x;
    std::uint16_t y;
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
    std::uint8_t i;
};

static_assert(sizeof(HeliosPoint) == 8, "HeliosPoint ABI must be 8 bytes");

struct AxisTransform {
    bool flip_x = false;
    bool flip_y = false;
    double scale_x = 1.0;
    double scale_y = 1.0;
    int offset_x = 0;
    int offset_y = 0;
};

struct OutputConfig {
    bool enabled = true;
    bool blackout = true;
    AxisTransform transform{};
};

struct MirrorConfig {
    bool armed = false;
    OutputConfig outputs[2]{};
};

std::uint16_t ead_coordinate_to_helios(std::int16_t value);
std::uint8_t ead_color_to_helios(std::int16_t value);
std::uint16_t apply_axis_transform(std::uint16_t value, bool flip,
                                   double scale, int offset);
HeliosPoint convert_point(const EadPoint& source, const OutputConfig& config,
                          bool globally_armed);
std::vector<HeliosPoint> convert_frame(const EadPoint* source, std::size_t count,
                                       const OutputConfig& config,
                                       bool globally_armed);

}  // namespace trollfx
