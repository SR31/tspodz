#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <cstring>
#include <cerrno>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdint>

static uint64_t hton64(uint64_t x) {
    uint64_t hi = htonl((uint32_t)(x >> 32));
    uint64_t lo = htonl((uint32_t)(x & 0xFFFFFFFFu));
    return (lo << 32) | hi;
}

static uint64_t ntoh64(uint64_t x) {
    uint64_t hi = ntohl((uint32_t)(x >> 32));
    uint64_t lo = ntohl((uint32_t)(x & 0xFFFFFFFFu));
    return (lo << 32) | hi;
}

ssize_t recv_all(int fd, void *buf, size_t len) {
    char *p = (char*)buf;
    size_t remaining = len;
    while (remaining > 0) {
        ssize_t r = recv(fd, p, remaining, 0);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) return 0; // closed
        p += r;
        remaining -= r;
    }
    return (ssize_t)len;
}

ssize_t send_all(int fd, const void *buf, size_t len) {
    const char *p = (const char*)buf;
    size_t remaining = len;
    while (remaining > 0) {
        ssize_t s = send(fd, p, remaining, 0);
        if (s < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        p += s;
        remaining -= s;
    }
    return (ssize_t)len;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <server_ip> <port> <path_to_file>\n";
        return 1;
    }
    const char* server_ip = argv[1];
    int port = 0;
    try {
        port = std::stoi(argv[2]);
    } catch (...) {
        std::cerr << "Невалидный порт\n";
        return 1;
    }
    const char* path = argv[3];

    // читаем файл полностью
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        std::cerr << "Не удалось открыть файл: " << path << "\n";
        return 1;
    }
    std::ostringstream oss;
    oss << ifs.rdbuf();
    std::string data = oss.str();
    uint64_t data_len = data.size();

    // создаём сокет
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        std::cerr << "socket() failed: " << strerror(errno) << "\n";
        return 1;
    }

    sockaddr_in srv;
    std::memset(&srv, 0, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, server_ip, &srv.sin_addr) <= 0) {
        std::cerr << "inet_pton() failed для " << server_ip << "\n";
        close(fd);
        return 1;
    }

    if (connect(fd, (sockaddr*)&srv, sizeof(srv)) < 0) {
        std::cerr << "connect() failed: " << strerror(errno) << "\n";
        close(fd);
        return 1;
    }

    // отправляем длину (8 байт сеть)
    uint64_t net_len = hton64(data_len);
    if (send_all(fd, &net_len, sizeof(net_len)) < 0) {
        std::cerr << "Ошибка отправки размера: " << strerror(errno) << "\n";
        close(fd);
        return 1;
    }
    // отправляем данные
    if (data_len > 0) {
        if (send_all(fd, data.data(), data_len) < 0) {
            std::cerr << "Ошибка отправки данных: " << strerror(errno) << "\n";
            close(fd);
            return 1;
        }
    }

    // получаем ответ длины
    uint64_t net_reply_len = 0;
    ssize_t r = recv_all(fd, &net_reply_len, sizeof(net_reply_len));
    if (r <= 0) {
        std::cerr << "Ошибка чтения размера ответа или соединение закрыто\n";
        close(fd);
        return 1;
    }
    uint64_t reply_len = ntoh64(net_reply_len);
    if (reply_len > (1ULL<<30)) {
        std::cerr << "Ответ слишком большой: " << reply_len << "\n";
        close(fd);
        return 1;
    }

    std::string reply;
    reply.resize(reply_len);
    if (reply_len > 0) {
        r = recv_all(fd, &reply[0], (size_t)reply_len);
        if (r <= 0) {
            std::cerr << "Ошибка чтения ответа: " << strerror(errno) << "\n";
            close(fd);
            return 1;
        }
    }

    // выводим ответ
    std::cout << reply;

    close(fd);
    return 0;
}
