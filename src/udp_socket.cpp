#include "radio_link/udp_socket.hpp"

#include <array>
#include <chrono>
#include <cstring>
#include <stdexcept>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_handle = SOCKET;
constexpr socket_handle invalid_socket_handle = INVALID_SOCKET;
static void close_socket(socket_handle handle) { closesocket(handle); }
#else
#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_handle = int;
constexpr socket_handle invalid_socket_handle = -1;
static void close_socket(socket_handle handle) { close(handle); }
#endif

namespace radio_link {
namespace {

socket_handle as_socket(std::intptr_t value) { return static_cast<socket_handle>(value); }

sockaddr_in make_address(const std::string& host, std::uint16_t port) {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &address.sin_addr) != 1) {
        throw std::invalid_argument("host must be an IPv4 address");
    }
    return address;
}

}  // namespace

UdpRuntime::UdpRuntime() {
#ifdef _WIN32
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        throw std::runtime_error("WSAStartup failed");
    }
#endif
}

UdpRuntime::~UdpRuntime() {
#ifdef _WIN32
    WSACleanup();
#endif
}

UdpReceiver::UdpReceiver(std::uint16_t port, std::string bind_address) {
    const auto handle = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (handle == invalid_socket_handle) {
        throw std::runtime_error("failed to create UDP receiver socket");
    }
    socket_ = static_cast<std::intptr_t>(handle);

    try {
        auto address = make_address(bind_address, port);
        if (::bind(handle, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
            throw std::runtime_error("failed to bind UDP receiver socket");
        }
        sockaddr_in local{};
#ifdef _WIN32
        int length = sizeof(local);
#else
        socklen_t length = sizeof(local);
#endif
        if (getsockname(handle, reinterpret_cast<sockaddr*>(&local), &length) != 0) {
            throw std::runtime_error("failed to read local UDP port");
        }
        local_port_ = ntohs(local.sin_port);
    } catch (...) {
        close_socket(handle);
        socket_ = -1;
        throw;
    }
}

UdpReceiver::~UdpReceiver() {
    if (socket_ != -1) close_socket(as_socket(socket_));
}

std::vector<std::uint8_t> UdpReceiver::receive(std::chrono::milliseconds timeout) {
    const auto handle = as_socket(socket_);
#ifdef _WIN32
    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(handle, &read_set);
    timeval value{};
    value.tv_sec = static_cast<long>(timeout.count() / 1000);
    value.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);
    const int ready = select(0, &read_set, nullptr, nullptr, &value);
#else
    pollfd descriptor{handle, POLLIN, 0};
    const int ready = poll(&descriptor, 1, static_cast<int>(timeout.count()));
#endif
    if (ready == 0) throw std::runtime_error("UDP receive timed out");
    if (ready < 0) throw std::runtime_error("UDP receive wait failed");

    std::array<std::uint8_t, 2048> buffer{};
    const auto received = recvfrom(
        handle,
        reinterpret_cast<char*>(buffer.data()),
        static_cast<int>(buffer.size()),
        0,
        nullptr,
        nullptr);
    if (received <= 0) throw std::runtime_error("UDP receive failed");
    return {buffer.begin(), buffer.begin() + received};
}

std::uint16_t UdpReceiver::local_port() const { return local_port_; }

UdpSender::UdpSender(std::string host, std::uint16_t port) {
    const auto handle = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (handle == invalid_socket_handle) {
        throw std::runtime_error("failed to create UDP sender socket");
    }
    socket_ = static_cast<std::intptr_t>(handle);
    const auto address = make_address(host, port);
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&address);
    destination_.assign(bytes, bytes + sizeof(address));
}

UdpSender::~UdpSender() {
    if (socket_ != -1) close_socket(as_socket(socket_));
}

void UdpSender::send(std::span<const std::uint8_t> payload) const {
    const auto* address = reinterpret_cast<const sockaddr*>(destination_.data());
    const auto sent = sendto(
        as_socket(socket_),
        reinterpret_cast<const char*>(payload.data()),
        static_cast<int>(payload.size()),
        0,
        address,
        static_cast<int>(destination_.size()));
    if (sent != static_cast<int>(payload.size())) {
        throw std::runtime_error("UDP datagram was not sent completely");
    }
}

std::uint64_t monotonic_milliseconds() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

}  // namespace radio_link
