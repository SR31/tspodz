#include "common.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <iostream>
#include <string>
#include <cstring>
#include <cerrno>

// Обрабатываем локально: превращаем числа в римские (этот шаг не обязателен, но оставлен для теста/отладки)
static std::string process_numbers_local(const std::string& data) {
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

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Использование: " << argv[0] << " <IP> <PORT> <INPUT FILE PATH>\n";
        return 1;
    }
    const char* server_ip = argv[1];
    int port = 0;
    try { port = std::stoi(argv[2]); } catch (...) {
        std::cerr << "Невалидный порт\n";
        return 1;
    }
    const char* input_path = argv[3];

    std::string input_data;
    if (!read_file(input_path, input_data)) {
        std::cerr << "Не удалось прочитать входной файл: " << input_path << "\n";
        return 1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    sockaddr_in srv{};
    srv.sin_family = AF_INET;
    srv.sin_port = htons(static_cast<uint16_t>(port));
    if (inet_pton(AF_INET, server_ip, &srv.sin_addr) <= 0) {
        std::cerr << "Ошибка преобразования адреса " << server_ip << "\n";
        close(sock);
        return 1;
    }

    if (connect(sock, reinterpret_cast<sockaddr*>(&srv), sizeof(srv)) < 0) {
        std::cerr << "Не удалось подключиться к серверу " << server_ip << ":" << port
                  << " - " << strerror(errno) << "\n";
        close(sock);
        return 1;
    }
    std::cout << "Соединение с сервером " << server_ip << ":" << port << "\n";

    uint8_t req = REQ_PROCESS;
    if (send_all(sock, &req, 1) < 0) {
        std::cerr << "Ошибка отправки типа запроса\n";
        close(sock);
        return 1;
    }

    uint64_t len = input_data.size();
    uint64_t net_len = hton64(len);
    if (send_all(sock, &net_len, sizeof(net_len)) < 0 ||
        (len > 0 && send_all(sock, input_data.data(), len) < 0)) {
        std::cerr << "Ошибка отправки файла на сервер\n";
        close(sock);
        return 1;
    }
    std::cout << "Файл отправлен на сервер\n";

    uint64_t net_rlen = 0;
    if (recv_all(sock, &net_rlen, sizeof(net_rlen)) < 0) {
        std::cerr << "Ошибка чтения длины результата от сервера\n";
        close(sock);
        return 1;
    }

    uint64_t rlen = ntoh64(net_rlen);
    std::string result;
    result.resize(rlen);
    if (rlen > 0) {
        if (recv_all(sock, &result[0], static_cast<size_t>(rlen)) < 0) {
            std::cerr << "Ошибка чтения результата от сервера\n";
            close(sock);
            return 1;
        }
    }

    std::cout << "Результат от сервера:\n" << result;
    const char* out_name = "output";
    if (!write_file(out_name, result)) {
        std::cerr << "Ошибка записи " << out_name << "\n";
        close(sock);
        return 1;
    }
    std::cout << "Результат сохранён локально как " << out_name << "\n";

    std::string output_data;
    if (!read_file(out_name, output_data)) {
        std::cerr << "Ошибка чтения файла с результатом\n";
        close(sock);
        return 1;
    }
    // пустой файл допустим: len==0
    req = REQ_UPLOAD;
    if (send_all(sock, &req, 1) < 0) {
        std::cerr << "Ошибка отправки типа запроса\n";
        close(sock);
        return 1;
    }

    uint64_t olen = output_data.size();
    uint64_t net_olen = hton64(olen);
    if (send_all(sock, &net_olen, sizeof(net_olen)) < 0 ||
        (olen > 0 && send_all(sock, output_data.data(), olen) < 0)) {
        std::cerr << "Ошибка отправки файла на сервер\n";
        close(sock);
        return 1;
    }
    std::cout << "Файл отправлен серверу\n";

    uint8_t status = 1;
    if (recv_all(sock, &status, 1) < 0) {
        std::cerr << "Ошибка получения статуса от сервера\n";
        close(sock);
        return 1;
    }
    if (status == 0) {
        std::cout << "Сервер успешно сохранил файл\n";
    } else {
        std::cerr << "Сервер не смог сохранить файл\n";
    }

    close(sock);
    return 0;
}
