#include "common.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <iostream>
#include <sstream>
#include <cstring>
#include <cerrno>
#include <signal.h>

static std::string process_numbers(const std::string& data) {
    std::istringstream iss(data);
    std::ostringstream out;
    std::string token;
    while (iss >> token) {
        try {
            size_t idx = 0;
            long v = std::stol(token, &idx, 10);
            if (idx != token.size()) throw std::invalid_argument("trailing");
            out << token << " -> " << int_to_roman(static_cast<int>(v)) << "\n";
        } catch (...) {
            out << token << " -> N/A\n";
        }
    }
    return out.str();
}

static void handle_client(int client_fd, const char* client_ip, uint16_t client_port) {
    while (true) {
        uint8_t req_type = 0;
        if (recv_all(client_fd, &req_type, 1) < 0) {
            break;
        }

        if (req_type == REQ_PROCESS) {
            uint64_t net_len = 0;
            if (recv_all(client_fd, &net_len, sizeof(net_len)) < 0) {
                std::cerr << "Ошибка чтения длины\n";
                break;
            }

            uint64_t len = ntoh64(net_len);
            std::string data;
            data.resize(len);
            if (len > 0) {
                if (recv_all(client_fd, &data[0], static_cast<size_t>(len)) < 0) {
                    std::cerr << "Ошибка чтения данных\n";
                    break;
                }
            }
            std::cout << "Получены данные от " << client_ip << ":" << client_port << "\n";

            std::string result = process_numbers(data);

            uint64_t rlen = result.size();
            uint64_t net_rlen = hton64(rlen);
            if (send_all(client_fd, &net_rlen, sizeof(net_rlen)) < 0 ||
                (rlen > 0 && send_all(client_fd, result.data(), rlen) < 0)) {
                std::cerr << "Ошибка отправки результата клиенту\n";
                break;
            }
            std::cout << "Результат обработан и отправлен клиенту\n";

        } else if (req_type == REQ_UPLOAD) {
            uint64_t net_len = 0;
            if (recv_all(client_fd, &net_len, sizeof(net_len)) < 0) {
                std::cerr << "Ошибка чтения длины данных\n";
                break;
            }

            uint64_t len = ntoh64(net_len);
            std::string data;
            data.resize(len);
            if (len > 0) {
                if (recv_all(client_fd, &data[0], static_cast<size_t>(len)) < 0) {
                    std::cerr << "Ошибка чтения данных\n";
                    break;
                }
            }

            const char* out_name = "output";
            if (write_file(out_name, data)) {
                std::cout << "Сохранён файл от клиента как " << out_name << " (" << len << " байт)\n";
                uint8_t status = 0;
                if (send_all(client_fd, &status, 1) < 0) {
                    std::cerr << "Ошибка отправки статуса клиенту\n";
                    break;
                }
            } else {
                std::cerr << "Ошибка записи файла на сервере\n";
                uint8_t status = 1;
                send_all(client_fd, &status, 1);
                break;
            }

        } else {
            std::cerr << "Неизвестный тип запроса: " << static_cast<int>(req_type) << "\n";
            break;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Использование: " << argv[0] << " <PORT>\n";
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
        perror("socket");
        return 1;
    }

    int yes = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, static_cast<socklen_t>(sizeof(yes))) < 0) {
        perror("setsockopt");
        close(listen_fd);
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("bind");
        close(listen_fd);
        return 1;
    }

    if (listen(listen_fd, 5) < 0) {
        perror("listen");
        close(listen_fd);
        return 1;
    }

    std::cout << "Сервер запущен на порту " << port << "\n";

    for (;;) {
        sockaddr_in cli{};
        socklen_t clilen = sizeof(cli);
        int client_fd = accept(listen_fd, reinterpret_cast<sockaddr*>(&cli), &clilen);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            break;
        }
        char ipbuf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cli.sin_addr, ipbuf, sizeof(ipbuf));
        uint16_t cport = ntohs(cli.sin_port);
        std::cout << "Клиент подключился с " << ipbuf << ":" << cport << "\n";

        handle_client(client_fd, ipbuf, cport);

        close(client_fd);
        std::cout << "Соединение с клиентом завершено\n";
    }

    close(listen_fd);
    return 0;
}