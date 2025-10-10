#ifndef COMMON_H
#define COMMON_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <sys/types.h>

constexpr uint8_t REQ_PROCESS = 1; // отправка файла с числами для обработки
constexpr uint8_t REQ_UPLOAD  = 2; // отправка уже готового output-файла для сохранения

constexpr uint64_t MAX_DATA_SIZE = (1ULL << 30);

bool send_all(int fd, const void* buf, size_t len);
bool recv_all(int fd, void* buf, size_t len);

uint64_t hton64(uint64_t x);
uint64_t ntoh64(uint64_t x);

std::string int_to_roman(int num);

bool read_file(const std::string& path, std::string& out);
bool write_file(const std::string& path, const std::string& data);

#endif // COMMON_H