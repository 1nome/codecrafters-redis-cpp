#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <unordered_map>
#include <chrono>

typedef std::chrono::time_point<std::chrono::system_clock> Timestamp;

extern std::unordered_map<std::string, std::string> key_vals;
extern std::unordered_map<std::string, Timestamp> key_expiry;
extern std::unordered_map<std::string, std::string> config_key_vals;

void read_rdb();

#endif //DATABASE_H
