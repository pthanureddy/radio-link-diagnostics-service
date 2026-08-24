#include "radio_link/frame.hpp"

#include "radio_link/crc32.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace radio_link {
namespace {

constexpr std::array<std::uint8_t, 4> magic{'R', 'L', 'D', 'S'};
constexpr std::uint8_t version = 1;

template <typename T>
void append_unsigned(std::vector<std::uint8_t>& output, T value) {
    static_assert(std::is_unsigned_v<T>);
    for (int shift = static_cast<int>(sizeof(T) - 1) * 8; shift >= 0; shift -= 8) {
        output.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
    }
}

template <typename T>
T read_unsigned(std::span<const std::uint8_t> bytes, std::size_t& offset) {
    static_assert(std::is_unsigned_v<T>);
    T value{};
    for (std::size_t index = 0; index < sizeof(T); ++index) {
        value = static_cast<T>((value << 8U) | bytes[offset++]);
    }
    return value;
}

bool metrics_valid(const RadioLinkFrame& frame) {
    return frame.center_frequency_hz > 0 && frame.bandwidth_hz > 0
        && frame.evm_x1000 <= 1000;
}

}  // namespace

std::vector<std::uint8_t> encode_frame(const RadioLinkFrame& frame) {
    if (!metrics_valid(frame)) {
        throw std::invalid_argument("radio-link metrics are outside the frame contract");
    }

    std::vector<std::uint8_t> bytes;
    bytes.reserve(frame_size);
    bytes.insert(bytes.end(), magic.begin(), magic.end());
    bytes.push_back(version);
    bytes.push_back(frame.flags);
    append_unsigned(bytes, static_cast<std::uint16_t>(0));
    append_unsigned(bytes, frame.sensor_id);
    append_unsigned(bytes, frame.sequence);
    append_unsigned(bytes, frame.monotonic_ms);
    append_unsigned(bytes, frame.center_frequency_hz);
    append_unsigned(bytes, frame.bandwidth_hz);
    append_unsigned(bytes, static_cast<std::uint16_t>(frame.rssi_dbm_x10));
    append_unsigned(bytes, static_cast<std::uint16_t>(frame.snr_db_x10));
    append_unsigned(bytes, frame.evm_x1000);
    append_unsigned(bytes, static_cast<std::uint16_t>(0));
    append_unsigned(bytes, crc32(bytes));
    return bytes;
}

DecodeResult decode_frame(std::span<const std::uint8_t> bytes) {
    if (bytes.size() != frame_size) {
        return {{}, DecodeError::invalid_size, "expected exactly 48 bytes"};
    }
    if (!std::equal(magic.begin(), magic.end(), bytes.begin())) {
        return {{}, DecodeError::invalid_magic, "frame magic is not RLDS"};
    }
    if (bytes[4] != version) {
        return {{}, DecodeError::unsupported_version, "only frame version 1 is supported"};
    }

    std::size_t checksum_offset = frame_size - sizeof(std::uint32_t);
    std::size_t checksum_reader = checksum_offset;
    const auto expected_checksum = read_unsigned<std::uint32_t>(bytes, checksum_reader);
    if (crc32(bytes.first(checksum_offset)) != expected_checksum) {
        return {{}, DecodeError::checksum_mismatch, "CRC-32 validation failed"};
    }

    std::size_t offset = 5;
    RadioLinkFrame frame;
    frame.flags = bytes[offset++];
    (void)read_unsigned<std::uint16_t>(bytes, offset);
    frame.sensor_id = read_unsigned<std::uint32_t>(bytes, offset);
    frame.sequence = read_unsigned<std::uint32_t>(bytes, offset);
    frame.monotonic_ms = read_unsigned<std::uint64_t>(bytes, offset);
    frame.center_frequency_hz = read_unsigned<std::uint64_t>(bytes, offset);
    frame.bandwidth_hz = read_unsigned<std::uint32_t>(bytes, offset);
    frame.rssi_dbm_x10 = static_cast<std::int16_t>(read_unsigned<std::uint16_t>(bytes, offset));
    frame.snr_db_x10 = static_cast<std::int16_t>(read_unsigned<std::uint16_t>(bytes, offset));
    frame.evm_x1000 = read_unsigned<std::uint16_t>(bytes, offset);

    if (!metrics_valid(frame)) {
        return {{}, DecodeError::invalid_metric, "frequency, bandwidth, or EVM is invalid"};
    }
    return {frame, DecodeError::none, {}};
}

std::string to_string(DecodeError error) {
    switch (error) {
        case DecodeError::none: return "none";
        case DecodeError::invalid_size: return "invalid_size";
        case DecodeError::invalid_magic: return "invalid_magic";
        case DecodeError::unsupported_version: return "unsupported_version";
        case DecodeError::checksum_mismatch: return "checksum_mismatch";
        case DecodeError::invalid_metric: return "invalid_metric";
    }
    return "unknown";
}

}  // namespace radio_link
