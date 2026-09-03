#include "radio_link/crc32.hpp"
#include "radio_link/frame.hpp"
#include "radio_link/link_analyzer.hpp"
#include "radio_link/udp_socket.hpp"

#include <algorithm>
#include <chrono>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using radio_link::LinkState;

radio_link::RadioLinkFrame nominal(std::uint32_t sequence = 1, std::uint32_t sensor = 7) {
    return {sensor, sequence, 10'000, 3'500'000'000ULL, 20'000'000U, -700, 180, 70, 0};
}

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void refresh_checksum(std::vector<std::uint8_t>& bytes) {
    const auto checksum = radio_link::crc32(
        std::span(bytes.data(), bytes.size() - sizeof(std::uint32_t)));
    for (std::size_t index = 0; index < sizeof(checksum); ++index) {
        const auto shift = static_cast<unsigned>((sizeof(checksum) - index - 1U) * 8U);
        bytes[bytes.size() - sizeof(checksum) + index] =
            static_cast<std::uint8_t>((checksum >> shift) & 0xFFU);
    }
}

template <typename Function>
void require_throws(Function&& function, const std::string& message) {
    try { function(); } catch (const std::exception&) { return; }
    throw std::runtime_error(message);
}

}  // namespace

int main() {
    using Test = std::pair<std::string, std::function<void()>>;
    const std::vector<Test> tests{
        {"crc32_known_vector", [] {
            const std::string input = "123456789";
            require(radio_link::crc32(std::span(reinterpret_cast<const std::uint8_t*>(input.data()), input.size())) == 0xCBF43926U, "unexpected CRC-32");
        }},
        {"frame_round_trip", [] {
            const auto frame = nominal();
            const auto decoded = radio_link::decode_frame(radio_link::encode_frame(frame));
            require(decoded && decoded.frame == frame, "frame did not round-trip");
        }},
        {"frame_size_is_fixed", [] {
            require(radio_link::encode_frame(nominal()).size() == radio_link::frame_size, "unexpected frame size");
        }},
        {"reject_truncated_frame", [] {
            auto bytes = radio_link::encode_frame(nominal()); bytes.pop_back();
            require(radio_link::decode_frame(bytes).error == radio_link::DecodeError::invalid_size, "truncation not rejected");
        }},
        {"reject_invalid_magic", [] {
            auto bytes = radio_link::encode_frame(nominal()); bytes[0] = 0;
            require(radio_link::decode_frame(bytes).error == radio_link::DecodeError::invalid_magic, "magic not rejected");
        }},
        {"reject_unsupported_version", [] {
            auto bytes = radio_link::encode_frame(nominal()); bytes[4] = 2;
            require(radio_link::decode_frame(bytes).error == radio_link::DecodeError::unsupported_version, "version not rejected");
        }},
        {"reject_corrupted_payload", [] {
            auto bytes = radio_link::encode_frame(nominal()); bytes[20] ^= 0x10;
            require(radio_link::decode_frame(bytes).error == radio_link::DecodeError::checksum_mismatch, "corruption not rejected");
        }},
        {"reject_invalid_encode_metrics", [] {
            auto frequency = nominal(); frequency.center_frequency_hz = 0;
            require_throws([&] { (void)radio_link::encode_frame(frequency); }, "zero frequency encoded");
            auto bandwidth = nominal(); bandwidth.bandwidth_hz = 0;
            require_throws([&] { (void)radio_link::encode_frame(bandwidth); }, "zero bandwidth encoded");
            auto evm = nominal(); evm.evm_x1000 = 1001;
            require_throws([&] { (void)radio_link::encode_frame(evm); }, "out-of-range EVM encoded");
        }},
        {"reject_invalid_decoded_metrics", [] {
            auto frequency = radio_link::encode_frame(nominal());
            std::fill(frequency.begin() + 24, frequency.begin() + 32, 0);
            refresh_checksum(frequency);
            require(radio_link::decode_frame(frequency).error == radio_link::DecodeError::invalid_metric,
                    "zero frequency decoded");

            auto bandwidth = radio_link::encode_frame(nominal());
            std::fill(bandwidth.begin() + 32, bandwidth.begin() + 36, 0);
            refresh_checksum(bandwidth);
            require(radio_link::decode_frame(bandwidth).error == radio_link::DecodeError::invalid_metric,
                    "zero bandwidth decoded");

            auto evm = radio_link::encode_frame(nominal());
            evm[40] = 0x03;
            evm[41] = 0xE9;
            refresh_checksum(evm);
            require(radio_link::decode_frame(evm).error == radio_link::DecodeError::invalid_metric,
                    "out-of-range EVM decoded");
        }},
        {"nominal_quality", [] {
            radio_link::LinkAnalyzer analyzer;
            require(analyzer.analyze(nominal(), 10'100).state == LinkState::nominal, "nominal frame not nominal");
        }},
        {"degraded_quality", [] {
            auto frame = nominal(); frame.rssi_dbm_x10 = -900;
            radio_link::LinkAnalyzer analyzer;
            require(analyzer.analyze(frame, 10'100).state == LinkState::degraded, "degraded frame not degraded");
        }},
        {"critical_quality", [] {
            auto frame = nominal(); frame.snr_db_x10 = 10;
            radio_link::LinkAnalyzer analyzer;
            require(analyzer.analyze(frame, 10'100).state == LinkState::critical, "critical frame not critical");
        }},
        {"reject_frequency_outside_policy", [] {
            auto frame = nominal(); frame.center_frequency_hz = 10'000;
            radio_link::LinkAnalyzer analyzer;
            require(analyzer.analyze(frame, 10'100).state == LinkState::rejected, "frequency not rejected");
        }},
        {"reject_frequency_above_policy", [] {
            auto frame = nominal(); frame.center_frequency_hz = 6'000'000'001ULL;
            radio_link::LinkAnalyzer analyzer;
            require(analyzer.analyze(frame, 10'100).state == LinkState::rejected,
                    "frequency above policy not rejected");
        }},
        {"reject_bandwidth_above_policy", [] {
            auto frame = nominal(); frame.bandwidth_hz = 200'000'001U;
            radio_link::LinkAnalyzer analyzer;
            require(analyzer.analyze(frame, 10'100).state == LinkState::rejected,
                    "bandwidth above policy not rejected");
        }},
        {"reject_stale_frame", [] {
            radio_link::LinkAnalyzer analyzer;
            require(analyzer.analyze(nominal(), 20'000).state == LinkState::rejected, "stale frame not rejected");
        }},
        {"reject_future_frame", [] {
            auto frame = nominal(); frame.monotonic_ms = 10'101;
            radio_link::LinkAnalyzer analyzer;
            require(analyzer.analyze(frame, 10'100).state == LinkState::rejected,
                    "future timestamp not rejected");
        }},
        {"detect_sequence_gap", [] {
            radio_link::LinkAnalyzer analyzer;
            (void)analyzer.analyze(nominal(4), 10'100);
            const auto result = analyzer.analyze(nominal(7), 10'100);
            require(result.missing_frames == 2 && result.state == LinkState::degraded, "gap not detected");
        }},
        {"detect_duplicate", [] {
            radio_link::LinkAnalyzer analyzer;
            (void)analyzer.analyze(nominal(4), 10'100);
            const auto result = analyzer.analyze(nominal(4), 10'100);
            require(result.duplicate && result.state == LinkState::degraded, "duplicate not detected");
        }},
        {"detect_out_of_order", [] {
            radio_link::LinkAnalyzer analyzer;
            (void)analyzer.analyze(nominal(5), 10'100);
            const auto result = analyzer.analyze(nominal(3), 10'100);
            require(result.out_of_order && result.state == LinkState::degraded, "ordering not detected");
        }},
        {"track_sensors_independently", [] {
            radio_link::LinkAnalyzer analyzer;
            (void)analyzer.analyze(nominal(9, 1), 10'100);
            const auto result = analyzer.analyze(nominal(1, 2), 10'100);
            require(result.state == LinkState::nominal, "sensor states were mixed");
        }},
        {"reset_sequence_state", [] {
            radio_link::LinkAnalyzer analyzer;
            (void)analyzer.analyze(nominal(9), 10'100); analyzer.reset();
            require(analyzer.analyze(nominal(1), 10'100).state == LinkState::nominal, "reset failed");
        }},
        {"json_contains_diagnostics", [] {
            radio_link::LinkAnalyzer analyzer; const auto frame = nominal();
            const auto json = radio_link::to_json(frame, analyzer.analyze(frame, 10'100));
            const std::vector<std::string> required_fields{
                "\"sensor_id\":7", "\"sequence\":1", "\"center_frequency_hz\":3500000000",
                "\"bandwidth_hz\":20000000", "\"rssi_dbm\":-70", "\"snr_db\":18",
                "\"evm\":0.07", "\"state\":\"nominal\"", "\"missing_frames\":0",
                "\"duplicate\":false", "\"out_of_order\":false", "\"reasons\":[\"within_policy\"]"};
            for (const auto& field : required_fields) {
                require(json.find(field) != std::string::npos, "JSON diagnostic field missing: " + field);
            }
        }},
        {"udp_loopback_round_trip", [] {
            radio_link::UdpRuntime runtime;
            radio_link::UdpReceiver receiver(0);
            radio_link::UdpSender sender("127.0.0.1", receiver.local_port());
            const auto expected = radio_link::encode_frame(nominal());
            sender.send(expected);
            const auto actual = receiver.receive(std::chrono::seconds(2));
            require(actual == expected, "UDP payload changed");
        }},
        {"udp_timeout", [] {
            radio_link::UdpRuntime runtime;
            radio_link::UdpReceiver receiver(0);
            require_throws([&] { (void)receiver.receive(std::chrono::milliseconds(10)); }, "timeout not reported");
        }},
        {"reject_invalid_ipv4", [] {
            radio_link::UdpRuntime runtime;
            require_throws([] { radio_link::UdpSender sender("not-an-ip", 9400); }, "invalid host accepted");
        }}
    };

    int failed = 0;
    for (const auto& [name, test] : tests) {
        try {
            test();
            std::cout << "PASS " << name << '\n';
        } catch (const std::exception& exception) {
            ++failed;
            std::cerr << "FAIL " << name << ": " << exception.what() << '\n';
        }
    }
    std::cout << "RESULT " << (tests.size() - static_cast<std::size_t>(failed))
              << "/" << tests.size() << " passed\n";
    return failed == 0 ? 0 : 1;
}

