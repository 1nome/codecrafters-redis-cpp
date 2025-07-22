#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <unordered_map>
#include <chrono>
#include <mutex>

typedef std::chrono::time_point<std::chrono::system_clock> Timestamp;

std::unordered_map<std::string, std::string>& key_vals();
std::unordered_map<std::string, Timestamp>& key_expiry();
std::unordered_map<std::string, std::string>& config_key_vals();

struct Stream_entry
{
    std::string id;
    std::unordered_map<std::string, std::string> key_vals;
};

void stream_add(const std::string& stream_key, const Stream_entry& se);
bool stream_exists(const std::string& stream_key);

void read_rdb(std::basic_istream<char>* s = nullptr);

#endif //DATABASE_H
