#include "radio_link/link_analyzer.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace radio_link {

LinkAnalyzer::LinkAnalyzer(LinkPolicy policy) : policy_(policy) {
    if (policy_.minimum_frequency_hz >= policy_.maximum_frequency_hz) {
        throw std::invalid_argument("minimum frequency must be below maximum frequency");
    }
}

DiagnosticResult LinkAnalyzer::analyze(
    const RadioLinkFrame& frame,
    std::uint64_t arrival_monotonic_ms) {
    DiagnosticResult result;
    result.sensor_id = frame.sensor_id;
    result.sequence = frame.sequence;
    result.state = LinkState::nominal;

    if (frame.center_frequency_hz < policy_.minimum_frequency_hz
        || frame.center_frequency_hz > policy_.maximum_frequency_hz) {
        result.state = LinkState::rejected;
        result.reasons.emplace_back("center_frequency_out_of_policy");
    }
    if (frame.bandwidth_hz > policy_.maximum_bandwidth_hz) {
        result.state = LinkState::rejected;
        result.reasons.emplace_back("bandwidth_out_of_policy");
    }
    if (frame.monotonic_ms > arrival_monotonic_ms
        || arrival_monotonic_ms - frame.monotonic_ms > policy_.maximum_age_ms) {
        result.state = LinkState::rejected;
        result.reasons.emplace_back("stale_or_future_timestamp");
    }

    auto& sequence = sequences_[frame.sensor_id];
    if (sequence.initialized) {
        if (frame.sequence == sequence.highest_sequence) {
            result.duplicate = true;
            result.reasons.emplace_back("duplicate_sequence");
        } else if (frame.sequence < sequence.highest_sequence) {
            result.out_of_order = true;
            result.reasons.emplace_back("out_of_order_sequence");
        } else {
            result.missing_frames = frame.sequence - sequence.highest_sequence - 1U;
            if (result.missing_frames > 0) {
                result.reasons.emplace_back("sequence_gap");
            }
            sequence.highest_sequence = frame.sequence;
        }
    } else {
        sequence.highest_sequence = frame.sequence;
        sequence.initialized = true;
    }

    if (result.state != LinkState::rejected) {
        const bool critical = frame.rssi_dbm_x10 < policy_.critical_rssi_dbm_x10
            || frame.snr_db_x10 < policy_.critical_snr_db_x10
            || frame.evm_x1000 > policy_.critical_evm_x1000;
        const bool degraded = frame.rssi_dbm_x10 < policy_.nominal_rssi_dbm_x10
            || frame.snr_db_x10 < policy_.nominal_snr_db_x10
            || frame.evm_x1000 > policy_.nominal_evm_x1000;

        if (critical) {
            result.state = LinkState::critical;
            result.reasons.emplace_back("critical_signal_quality");
        } else if (degraded || result.duplicate || result.out_of_order || result.missing_frames > 0) {
            result.state = LinkState::degraded;
            if (degraded) {
                result.reasons.emplace_back("degraded_signal_quality");
            }
        }
    }

    if (result.reasons.empty()) {
        result.reasons.emplace_back("within_policy");
    }
    return result;
}

void LinkAnalyzer::reset() { sequences_.clear(); }

std::string to_string(LinkState state) {
    switch (state) {
        case LinkState::nominal: return "nominal";
        case LinkState::degraded: return "degraded";
        case LinkState::critical: return "critical";
        case LinkState::rejected: return "rejected";
    }
    return "unknown";
}

std::string to_json(const RadioLinkFrame& frame, const DiagnosticResult& result) {
    std::ostringstream output;
    output << "{\"sensor_id\":" << frame.sensor_id
           << ",\"sequence\":" << frame.sequence
           << ",\"center_frequency_hz\":" << frame.center_frequency_hz
           << ",\"bandwidth_hz\":" << frame.bandwidth_hz
           << ",\"rssi_dbm\":" << (static_cast<double>(frame.rssi_dbm_x10) / 10.0)
           << ",\"snr_db\":" << (static_cast<double>(frame.snr_db_x10) / 10.0)
           << ",\"evm\":" << (static_cast<double>(frame.evm_x1000) / 1000.0)
           << ",\"state\":\"" << to_string(result.state) << "\""
           << ",\"missing_frames\":" << result.missing_frames
           << ",\"duplicate\":" << (result.duplicate ? "true" : "false")
           << ",\"out_of_order\":" << (result.out_of_order ? "true" : "false")
           << ",\"reasons\":[";
    for (std::size_t index = 0; index < result.reasons.size(); ++index) {
        if (index > 0) output << ',';
        output << '\"' << result.reasons[index] << '\"';
    }
    output << "]}";
    return output.str();
}

}  // namespace radio_link
