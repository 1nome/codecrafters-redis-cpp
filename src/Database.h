#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <unordered_map>
#include <chrono>
#include <list>
#include <mutex>

typedef std::chrono::time_point<std::chrono::system_clock> Timestamp;

std::unordered_map<std::string, std::string>& key_vals();
std::unordered_map<std::string, Timestamp>& key_expiry();
std::unordered_map<std::string, std::string>& config_key_vals();

struct Stream_entry
{
    unsigned long milliseconds_time;
    unsigned int sequence_number;
    std::unordered_map<std::string, std::string> key_vals;

    std::string id_bulk() const;
};

void stream_add(const std::string& stream_key, const Stream_entry& se);
bool stream_exists(const std::string& stream_key);
const Stream_entry& stream_top(const std::string& stream_key);
bool stream_empty(const std::string& stream_key);

extern std::mutex streams_lock;
extern std::unordered_map<std::string, std::vector<Stream_entry>> streams;

extern std::unordered_map<std::string, std::list<std::string>> lists;
extern std::mutex lists_lock;

void read_rdb(std::basic_istream<char>* s = nullptr);

#endif //DATABASE_H
