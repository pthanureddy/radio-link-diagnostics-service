#pragma once

#include <chrono>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace radio_link {

class UdpRuntime {
public:
    UdpRuntime();
    ~UdpRuntime();
    UdpRuntime(const UdpRuntime&) = delete;
    UdpRuntime& operator=(const UdpRuntime&) = delete;
};

class UdpReceiver {
public:
    explicit UdpReceiver(std::uint16_t port, std::string bind_address = "127.0.0.1");
    ~UdpReceiver();
    UdpReceiver(const UdpReceiver&) = delete;
    UdpReceiver& operator=(const UdpReceiver&) = delete;

    std::vector<std::uint8_t> receive(std::chrono::milliseconds timeout);
    std::uint16_t local_port() const;

private:
    std::intptr_t socket_{-1};
    std::uint16_t local_port_{};
};

class UdpSender {
public:
    UdpSender(std::string host, std::uint16_t port);
    ~UdpSender();
    UdpSender(const UdpSender&) = delete;
    UdpSender& operator=(const UdpSender&) = delete;

    void send(std::span<const std::uint8_t> payload) const;

private:
    std::intptr_t socket_{-1};
    std::vector<std::uint8_t> destination_;
};

std::uint64_t monotonic_milliseconds();

}  // namespace radio_link

