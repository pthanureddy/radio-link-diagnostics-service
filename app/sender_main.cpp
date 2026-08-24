#include "radio_link/frame.hpp"
#include "radio_link/udp_socket.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

int main(int argc, char** argv) {
    try {
        std::string host = "127.0.0.1";
        std::uint16_t port = 9400;
        int count = 1;
        std::string scenario = "nominal";
        for (int index = 1; index < argc; ++index) {
            const std::string argument = argv[index];
            if (argument == "--host" && index + 1 < argc) host = argv[++index];
            else if (argument == "--port" && index + 1 < argc) port = static_cast<std::uint16_t>(std::stoi(argv[++index]));
            else if (argument == "--count" && index + 1 < argc) count = std::stoi(argv[++index]);
            else if (argument == "--scenario" && index + 1 < argc) scenario = argv[++index];
            else throw std::invalid_argument("usage: radio_link_sender [--host IPv4] [--port N] [--count N] [--scenario nominal|degraded|critical]");
        }
        if (count <= 0) throw std::invalid_argument("count must be positive");

        std::int16_t rssi = -700;
        std::int16_t snr = 180;
        std::uint16_t evm = 70;
        if (scenario == "degraded") {
            rssi = -900; snr = 70; evm = 180;
        } else if (scenario == "critical") {
            rssi = -1100; snr = 10; evm = 350;
        } else if (scenario != "nominal") {
            throw std::invalid_argument("scenario must be nominal, degraded, or critical");
        }

        radio_link::UdpRuntime runtime;
        radio_link::UdpSender sender(host, port);
        for (int index = 0; index < count; ++index) {
            radio_link::RadioLinkFrame frame{
                1001U,
                static_cast<std::uint32_t>(index + 1),
                radio_link::monotonic_milliseconds(),
                3'500'000'000ULL,
                20'000'000U,
                rssi,
                snr,
                evm,
                0U};
            sender.send(radio_link::encode_frame(frame));
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "radio_link_sender: " << exception.what() << '\n';
        return 2;
    }
}
