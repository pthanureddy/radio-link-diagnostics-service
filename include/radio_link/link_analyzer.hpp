#pragma once

#include "radio_link/frame.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace radio_link {

enum class LinkState { nominal, degraded, critical, rejected };

struct LinkPolicy {
    std::uint64_t minimum_frequency_hz{100'000'000};
    std::uint64_t maximum_frequency_hz{6'000'000'000};
    std::uint32_t maximum_bandwidth_hz{200'000'000};
    std::int16_t nominal_rssi_dbm_x10{-850};
    std::int16_t critical_rssi_dbm_x10{-1000};
    std::int16_t nominal_snr_db_x10{100};
    std::int16_t critical_snr_db_x10{30};
    std::uint16_t nominal_evm_x1000{125};
    std::uint16_t critical_evm_x1000{250};
    std::uint64_t maximum_age_ms{2'000};
};

struct DiagnosticResult {
    std::uint32_t sensor_id{};
    std::uint32_t sequence{};
    LinkState state{LinkState::rejected};
    std::uint32_t missing_frames{};
    bool duplicate{};
    bool out_of_order{};
    std::vector<std::string> reasons;
};

class LinkAnalyzer {
public:
    explicit LinkAnalyzer(LinkPolicy policy = {});

    DiagnosticResult analyze(const RadioLinkFrame& frame, std::uint64_t arrival_monotonic_ms);
    void reset();

private:
    struct SequenceState {
        std::uint32_t highest_sequence{};
        bool initialized{};
    };

    LinkPolicy policy_;
    std::unordered_map<std::uint32_t, SequenceState> sequences_;
};

std::string to_string(LinkState state);
std::string to_json(const RadioLinkFrame& frame, const DiagnosticResult& result);

}  // namespace radio_link

