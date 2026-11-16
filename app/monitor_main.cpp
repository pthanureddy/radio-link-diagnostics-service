#include "radio_link/frame.hpp"
#include "radio_link/cli_integer.hpp"
#include "radio_link/link_analyzer.hpp"
#include "radio_link/udp_socket.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
    try {
        std::uint16_t port = 9400;
        int count = 1;
        bool continuous = false;
        bool announce_ready = false;
        std::string bind_address = "0.0.0.0";
        for (int index = 1; index < argc; ++index) {
            const std::string argument = argv[index];
            if (argument == "--port" && index + 1 < argc) {
                port = static_cast<std::uint16_t>(radio_link::parse_cli_integer(
                    argv[++index], "--port", 0, 65535));
            } else if (argument == "--count" && index + 1 < argc) {
                count = radio_link::parse_cli_integer(
                    argv[++index], "--count", 1, std::numeric_limits<int>::max());
            } else if (argument == "--bind" && index + 1 < argc) {
                bind_address = argv[++index];
            } else if (argument == "--announce-ready") {
                announce_ready = true;
            } else if (argument == "--continuous") {
                continuous = true;
            } else {
                throw std::invalid_argument(
                    "usage: radio_link_monitor [--bind IPv4] [--port N] "
                    "[--count N | --continuous] [--announce-ready]");
            }
        }
        if (count <= 0) throw std::invalid_argument("count must be positive");

        radio_link::UdpRuntime runtime;
        radio_link::UdpReceiver receiver(port, bind_address);
        if (announce_ready) {
            std::cerr << "radio_link_monitor: ready port=" << receiver.local_port() << std::endl;
        }
        radio_link::LinkAnalyzer analyzer;
        for (int received = 0; continuous || received < count; ++received) {
            const auto bytes = receiver.receive(std::chrono::seconds(10));
            const auto decoded = radio_link::decode_frame(bytes);
            if (!decoded) {
                std::cout << "{\"state\":\"rejected\",\"decode_error\":\""
                          << radio_link::to_string(decoded.error) << "\"}" << '\n' << std::flush;
                continue;
            }
            const auto result = analyzer.analyze(
                decoded.frame, radio_link::monotonic_milliseconds());
            std::cout << radio_link::to_json(decoded.frame, result) << '\n' << std::flush;
        }
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "radio_link_monitor: " << exception.what() << '\n';
        return 2;
    }
}
