#include <iostream>
#include <fstream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>

using namespace std;

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: client <IP> <PATH>" << endl;
        return 1;
    }

    string server_ip = argv[1];
    string filename = argv[2];
    
    // Читаем файл
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << filename << " is not available" << endl;
        return 1;
    }

    string file_content((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
    file.close();

    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation: X");
        return 1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(3110);
    
    if(inet_pton(AF_INET, server_ip.c_str(), &serv_addr.sin_addr) <= 0) {
        perror("Invalid IP");
        close(sock);
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection: X");
        close(sock);
        return 1;
    }

    if (send(sock, file_content.c_str(), file_content.length(), 0) < 0) {
        perror("Sending data: X");
        close(sock);
        return 1;
    }

    char buffer[1024] = {0};
    int valread;

    // Получаем ответ
    if ((valread = read(sock, buffer, 1024)) < 0) {
        perror("Receiving data: X");
        close(sock);
        return 1;
    }

    cout << "Result: " << buffer << endl;
    close(sock);
    return 0;
}