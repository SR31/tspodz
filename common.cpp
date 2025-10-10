#include "common.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

#include <fstream>
#include <vector>
#include <iostream>
#include <iterator>

bool send_all(int fd, const void* buf, size_t len) {
    const char* data = static_cast<const char*>(buf);
    size_t total_sent = 0;
    while (total_sent < len) {
        ssize_t s = send(fd, data + total_sent, len - total_sent, 0);
        if (s < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (s == 0) return false;
        total_sent += static_cast<size_t>(s);
    }
    return true;
}

bool recv_all(int fd, void* buf, size_t len) {
    char* data = static_cast<char*>(buf);
    size_t total_recv = 0;
    while (total_recv < len) {
        ssize_t r = recv(fd, data + total_recv, len - total_recv, 0);
        if (r < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (r == 0) {
            // connection closed
            return false;
        }
        total_recv += static_cast<size_t>(r);
    }
    return true;
}

uint64_t hton64(uint64_t x) {
    uint32_t hi = htonl(static_cast<uint32_t>(x >> 32));
    uint32_t lo = htonl(static_cast<uint32_t>(x & 0xFFFFFFFFu));
    return (static_cast<uint64_t>(lo) << 32) | static_cast<uint64_t>(hi);
}

uint64_t ntoh64(uint64_t x) {
    uint32_t hi = ntohl(static_cast<uint32_t>(x >> 32));
    uint32_t lo = ntohl(static_cast<uint32_t>(x & 0xFFFFFFFFu));
    return (static_cast<uint64_t>(lo) << 32) | static_cast<uint64_t>(hi);
}

std::string int_to_roman(int num) {
    if (num <= 0 || num > 3999) return "N/A";
    static const std::vector<std::pair<int, std::string>> table = {
        {1000,"M"}, {900,"CM"}, {500,"D"}, {400,"CD"},
        {100,"C"}, {90,"XC"}, {50,"L"}, {40,"XL"},
        {10,"X"}, {9,"IX"}, {5,"V"}, {4,"IV"}, {1,"I"}
    };
    std::string res;
    for (const auto &p : table) {
        while (num >= p.first) {
            res += p.second;
            num -= p.first;
        }
    }
    return res;
}

bool read_file(const std::string& path, std::string& out) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return false;
    out.assign((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    return true;
}

bool write_file(const std::string& path, const std::string& data) {
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) return false;
    ofs.write(data.data(), static_cast<std::streamsize>(data.size()));
    return ofs.good();
}