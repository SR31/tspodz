#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>

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
        if (r == 0) return 0; // connection closed
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

std::string int_to_roman(int num) {
    if (num <= 0 || num > 3999) return "N/A";
    static const std::vector<std::pair<int, std::string>> table = {
        {1000,"M"}, {900,"CM"}, {500,"D"}, {400,"CD"},
        {100,"C"}, {90,"XC"}, {50,"L"}, {40,"XL"},
        {10,"X"}, {9,"IX"}, {5,"V"}, {4,"IV"}, {1,"I"}
    };
    std::string res;
    for (auto &p : table) {
        while (num >= p.first) {
            res += p.second;
            num -= p.first;
        }
    }
    return res;
}

int handle_connection(int client_fd) {
    // 1) прочитать 8 байт длины
    uint64_t net_len = 0;
    ssize_t r = recv_all(client_fd, &net_len, sizeof(net_len));
    if (r <= 0) {
        std::cerr << "Ошибка чтения размера от клиента или соединение закрыто\n";
        return -1;
    }
    uint64_t data_len = ntoh64(net_len);
    if (data_len == 0) {
        std::cerr << "Получена длина 0\n";
        return -1;
    }
    if (data_len > (1ULL<<30)) { // ограничение ~1GB
        std::cerr << "Слишком большой размер: " << data_len << "\n";
        return -1;
    }

    // 2) прочитать данные
    std::string data;
    data.resize(data_len);
    r = recv_all(client_fd, &data[0], (size_t)data_len);
    if (r <= 0) {
        std::cerr << "Ошибка чтения данных: " << strerror(errno) << "\n";
        return -1;
    }

    // 3) парсим числа
    std::istringstream iss(data);
    std::string token;
    std::ostringstream out;
    while (iss >> token) {
        // пробуем распарсить целое (поддерживаем +/-, но римские только для положительных)
        bool ok = true;
        long val = 0;
        try {
            size_t idx = 0;
            val = std::stol(token, &idx, 10);
            if (idx != token.size()) ok = false;
        } catch (...) {
            ok = false;
        }
        if (!ok) {
            out << token << " -> N/A\n";
        } else {
            out << token << " -> " << int_to_roman((int)val) << "\n";
        }
    }

    std::string reply = out.str();
    uint64_t reply_len = reply.size();
    uint64_t net_reply_len = hton64(reply_len);
    if (send_all(client_fd, &net_reply_len, sizeof(net_reply_len)) < 0) {
        std::cerr << "Ошибка отправки размера ответа: " << strerror(errno) << "\n";
        return -1;
    }
    if (reply_len > 0) {
        if (send_all(client_fd, reply.data(), reply_len) < 0) {
            std::cerr << "Ошибка отправки данных ответа: " << strerror(errno) << "\n";
            return -1;
        }
    }
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <port>\n";
        return 1;
    }
    int port = 0;
    try {
        port = std::stoi(argv[1]);
    } catch (...) {
        std::cerr << "Невалидный порт\n";
        return 1;
    }
    if (port <= 0 || port > 65535) {
        std::cerr << "Порт вне диапазона\n";
        return 1;
    }

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        std::cerr << "socket() failed: " << strerror(errno) << "\n";
        return 1;
    }

    int yes = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        // не критично
    }

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)port);

    if (bind(listen_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "bind() failed: " << strerror(errno) << "\n";
        close(listen_fd);
        return 1;
    }

    if (listen(listen_fd, 5) < 0) {
        std::cerr << "listen() failed: " << strerror(errno) << "\n";
        close(listen_fd);
        return 1;
    }

    std::cout << "Сервер запущен на порту " << port << "\n";

    while (true) {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(listen_fd, (sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            std::cerr << "accept() failed: " << strerror(errno) << "\n";
            break;
        }
        char ipbuf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ipbuf, sizeof(ipbuf));
        std::cout << "Соединение от " << ipbuf << ":" << ntohs(client_addr.sin_port) << "\n";

        if (handle_connection(client_fd) != 0) {
            std::cerr << "Ошибка при обработке соединения\n";
        } else {
            std::cout << "Запрос обработан успешно\n";
        }
        close(client_fd);
    }

    close(listen_fd);
    return 0;
}
