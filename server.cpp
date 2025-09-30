#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>

using namespace std;

// Функция преобразования числа в римскую запись
string toRoman(int num) {
    string roman = "";
    int values[] = {1000, 900, 500, 400, 100, 90, 50, 40, 10, 9, 5, 4, 1};
    string symbols[] = {"M", "CM", "D", "CD", "C", "XC", "L", "XL", "X", "IX", "V", "IV", "I"};
    
    for (int i = 0; i < 13; i++) {
        while (num >= values[i]) {
            roman += symbols[i];
            num -= values[i];
        }
    }
    return roman;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: server <порт>" << endl;
        return 1;
    }
    
    int port = stoi(argv[1]);
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    
    // Создаем сокет
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation: X");
        exit(EXIT_FAILURE);
    }
    
    // Устанавливаем параметры сокета
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("Socket configuration: X");
        exit(EXIT_FAILURE);
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    // Привязываем сокет
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Socket binding: X");
        exit(EXIT_FAILURE);
    }
    
    // Слушаем входящие подключения
    if (listen(server_fd, 3) < 0) {
        perror("Listening: X");
        exit(EXIT_FAILURE);
    }
    
    while(true) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accepting: X");
            exit(EXIT_FAILURE);
        }
        
        char buffer[1024] = {0};
        int valread;
        
        // Читаем данные от клиента
        if ((valread = read(new_socket, buffer, 1024)) < 0) {
            perror("Receiving data: X");
            exit(EXIT_FAILURE);
        }
        
        try {
            int number = stoi(buffer);
            string result = toRoman(number);
            
            // Отправляем результат обратно
            send(new_socket, result.c_str(), result.length(), 0);
        } catch (const exception& e) {
            string error = "Cannot handle this number: " + string(e.what());
            send(new_socket, error.c_str(), error.length(), 0);
        }
        
        close(new_socket);
    }
    
    return 0;
}