#include "radio_link/crc32.hpp"

namespace radio_link {

std::uint32_t crc32(std::span<const std::uint8_t> bytes) {
    std::uint32_t value = 0xFFFFFFFFU;
    for (const auto byte : bytes) {
        value ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            const auto mask = static_cast<std::uint32_t>(-(static_cast<std::int32_t>(value & 1U)));
            value = (value >> 1U) ^ (0xEDB88320U & mask);
        }
    }
    return ~value;
}

}  // namespace radio_link

