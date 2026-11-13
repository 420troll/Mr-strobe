#include "mirror_core.hpp"

#include <algorithm>
#include <cmath>

namespace trollfx {

std::uint16_t ead_coordinate_to_helios(std::int16_t value) {
    // EAD coordinates span signed 16-bit full scale. Map both endpoints exactly
    // onto the Helios 12-bit range rather than shifting signed values.
    const auto unsigned_value = static_cast<std::uint32_t>(
        static_cast<std::int32_t>(value) - INT16_MIN);
    return static_cast<std::uint16_t>((unsigned_value * 4095u + 32767u) / 65535u);
}

std::uint8_t ead_color_to_helios(std::int16_t value) {
    // j4c's EtherDream converter shifts each signed channel left once into the
    // protocol's unsigned 16-bit channel. Therefore the meaningful EAD range
    // is 0..32767. Scale that full range onto Helios 8-bit color.
    const auto clamped = static_cast<std::uint32_t>(
        std::clamp<int>(value, 0, INT16_MAX));
    return static_cast<std::uint8_t>((clamped * 255u + 16383u) / 32767u);
}

std::uint16_t apply_axis_transform(std::uint16_t value, bool flip,
                                   double scale, int offset) {
    const double safe_scale = std::clamp(scale, 0.0, 2.0);
    double centered = static_cast<double>(value) - 2047.5;
    if (flip) centered = -centered;
    const auto transformed = static_cast<int>(
        std::lround(2047.5 + centered * safe_scale + offset));
    return static_cast<std::uint16_t>(std::clamp(transformed, 0, 4095));
}

HeliosPoint convert_point(const EadPoint& source, const OutputConfig& config,
                          bool globally_armed) {
    HeliosPoint result{};
    const auto base_x = ead_coordinate_to_helios(source.x);
    const auto base_y = ead_coordinate_to_helios(source.y);
    result.x = apply_axis_transform(base_x, config.transform.flip_x,
                                    config.transform.scale_x,
                                    config.transform.offset_x);
    result.y = apply_axis_transform(base_y, config.transform.flip_y,
                                    config.transform.scale_y,
                                    config.transform.offset_y);

    if (globally_armed && config.enabled && !config.blackout) {
        result.r = ead_color_to_helios(source.r);
        result.g = ead_color_to_helios(source.g);
        result.b = ead_color_to_helios(source.b);
        result.i = ead_color_to_helios(source.i);
    }
    return result;
}

std::vector<HeliosPoint> convert_frame(const EadPoint* source, std::size_t count,
                                       const OutputConfig& config,
                                       bool globally_armed) {
    std::vector<HeliosPoint> output;
    output.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        output.push_back(convert_point(source[i], config, globally_armed));
    }
    return output;
}

}  // namespace trollfx
