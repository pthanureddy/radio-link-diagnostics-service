#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace radio_link {

struct RadioLinkFrame {
    std::uint32_t sensor_id{};
    std::uint32_t sequence{};
    std::uint64_t monotonic_ms{};
    std::uint64_t center_frequency_hz{};
    std::uint32_t bandwidth_hz{};
    std::int16_t rssi_dbm_x10{};
    std::int16_t snr_db_x10{};
    std::uint16_t evm_x1000{};
    std::uint8_t flags{};

    bool operator==(const RadioLinkFrame&) const = default;
};

enum class DecodeError {
    none,
    invalid_size,
    invalid_magic,
    unsupported_version,
    checksum_mismatch,
    invalid_metric
};

struct DecodeResult {
    RadioLinkFrame frame{};
    DecodeError error{DecodeError::none};
    std::string message;

    explicit operator bool() const { return error == DecodeError::none; }
};

inline constexpr std::size_t frame_size = 48;

std::vector<std::uint8_t> encode_frame(const RadioLinkFrame& frame);
DecodeResult decode_frame(std::span<const std::uint8_t> bytes);
std::string to_string(DecodeError error);

}  // namespace radio_link
