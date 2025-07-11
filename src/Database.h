#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <unordered_map>
#include <chrono>

typedef std::chrono::time_point<std::chrono::system_clock> Timestamp;

extern std::unordered_map<std::string, std::pair<std::string, Timestamp>> key_vals;

#endif //DATABASE_H
