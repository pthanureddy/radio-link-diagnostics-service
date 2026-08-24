#include "radio_link/frame.hpp"
#include "radio_link/link_analyzer.hpp"
#include "radio_link/udp_socket.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
    try {
        std::uint16_t port = 9400;
        int count = 1;
        bool continuous = false;
        for (int index = 1; index < argc; ++index) {
            const std::string argument = argv[index];
            if (argument == "--port" && index + 1 < argc) {
                port = static_cast<std::uint16_t>(std::stoi(argv[++index]));
            } else if (argument == "--count" && index + 1 < argc) {
                count = std::stoi(argv[++index]);
            } else if (argument == "--continuous") {
                continuous = true;
            } else {
                throw std::invalid_argument(
                    "usage: radio_link_monitor [--port N] [--count N | --continuous]");
            }
        }
        if (count <= 0) throw std::invalid_argument("count must be positive");

        radio_link::UdpRuntime runtime;
        radio_link::UdpReceiver receiver(port, "0.0.0.0");
        radio_link::LinkAnalyzer analyzer;
        for (int received = 0; continuous || received < count; ++received) {
            const auto bytes = receiver.receive(std::chrono::seconds(10));
            const auto decoded = radio_link::decode_frame(bytes);
            if (!decoded) {
                std::cout << "{\"state\":\"rejected\",\"decode_error\":\""
                          << radio_link::to_string(decoded.error) << "\"}" << '\n';
                continue;
            }
            const auto result = analyzer.analyze(
                decoded.frame, radio_link::monotonic_milliseconds());
            std::cout << radio_link::to_json(decoded.frame, result) << '\n';
        }
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "radio_link_monitor: " << exception.what() << '\n';
        return 2;
    }
}
