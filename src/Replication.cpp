#include "Replication.h"

#include <iostream>

#include "Database.h"
#include <sys/socket.h>
#include "Resp.h"
#include <mutex>
#include <ranges>

std::string master_replid = "VeryRandomStringThatIsFortyCharactersLon";
size_t master_repl_offset_int = 0;

std::deque<std::pair<std::string, int>> command_queue_q;
int slave_count_int = 0;
size_t top_offset_int = 0;

std::mutex command_queue_lock;
std::mutex slave_count_lock;
std::mutex master_repl_offset_lock;
std::mutex top_offset_lock;

size_t& master_repl_offset()
{
    const std::lock_guard guard(master_repl_offset_lock);
    return master_repl_offset_int;
}

std::deque<std::pair<std::string, int>>& command_queue()
{
    const std::lock_guard lock(command_queue_lock);
    return command_queue_q;
}

int& slave_count()
{
    const std::lock_guard lock(slave_count_lock);
    return slave_count_int;
}

size_t& top_offset()
{
    const std::lock_guard lock(top_offset_lock);
    return top_offset_int;
}

void slave_disconnected()
{
    const std::lock_guard lock(command_queue_lock);
    const std::lock_guard lock2(slave_count_lock);
    slave_count_int--;
    if (!slave_count_int)
    {
        command_queue_q.clear();
    }
    for (auto& val : command_queue_q | std::views::values)
    {
        val--;
    }
}

void add_command(const std::string& command)
{
    const std::lock_guard lock(command_queue_lock);
    const std::lock_guard lock2(slave_count_lock);
    if (!slave_count_int)
    {
        return;
    }
    command_queue_q.emplace_back(command, slave_count_int);
}

void remove_command()
{
    const std::lock_guard lock(top_offset_lock);
    const std::lock_guard lock2(command_queue_lock);
    if (command_queue_q.front().second == 0)
    {
        command_queue_q.pop_front();
        top_offset_int++;
    }
}

int num_finished_commands()
{
    const std::lock_guard lock2(command_queue_lock);
    const std::lock_guard lock(slave_count_lock);
    if (command_queue_q.empty())
    {
        return slave_count_int;
    }
    return slave_count_int - command_queue_q.back().second;
}

bool is_slave()
{
    return config_key_vals().contains("replicaof");
}

int find(const char* s, const char c, const int start, const int end)
{
    for (int i = start; i < end; i++)
    {
        if (s[i] == c)
        {
            return i;
        }
    }
    return -1;
}

std::string send_handshake(const int master_fd)
{
    constexpr int buffer_size = 4096;
    char in_buffer[buffer_size];

    const std::string str1 = command({"PING"});
    send(master_fd, str1.c_str(), str1.size(), 0);
    recv(master_fd, in_buffer, buffer_size, 0); // pong

    const std::string str2 = command({"REPLCONF", "listening-port", config_key_vals()["port"]});
    send(master_fd, str2.c_str(), str2.size(), 0);
    recv(master_fd, in_buffer, buffer_size, 0); // ok

    const std::string str3 = command({"REPLCONF", "capa", "psync2"});
    send(master_fd, str3.c_str(), str3.size(), 0);
    recv(master_fd, in_buffer, buffer_size, 0); // ok

    const std::string str4 = command({"PSYNC", "?", "-1"});
    send(master_fd, str4.c_str(), str4.size(), 0);
    unsigned int r = recv(master_fd, in_buffer, buffer_size, 0); // fullresync

    int s = find(in_buffer, '$', 0, r);
    if (s == -1)
    {
        r = recv(master_fd, in_buffer, buffer_size, 0);
        s = 0;
    }
    std::cout << r << std::endl;
    std::cout << s << std::endl;
    s += 1;
    char* end;
    const long n = std::strtol(in_buffer + s, &end, 10);
    std::cout << n << std::endl;
    end += 2;
    std::stringstream ss;
    ss.write(end, n);
    read_rdb(&ss);

    return {end + n, static_cast<std::string::size_type>(r - n - (end - in_buffer))};
}

std::vector<unsigned char> make_rdb()
{
    // currently returns only an empty db
    return {'$', '8', '8', '\r', '\n',
    0x52, 0x45, 0x44, 0x49, 0x53, 0x30, 0x30, 0x31, 0x31, 0xfa, 0x09, 0x72, 0x65, 0x64, 0x69, 0x73, //|REDIS0011..redis|
    0x2d, 0x76, 0x65, 0x72, 0x05, 0x37, 0x2e, 0x32, 0x2e, 0x30, 0xfa, 0x0a, 0x72, 0x65, 0x64, 0x69, //|-ver.7.2.0..redi|
    0x73, 0x2d, 0x62, 0x69, 0x74, 0x73, 0xc0, 0x40, 0xfa, 0x05, 0x63, 0x74, 0x69, 0x6d, 0x65, 0xc2, //|s-bits.@..ctime.|
    0x6d, 0x08, 0xbc, 0x65, 0xfa, 0x08, 0x75, 0x73, 0x65, 0x64, 0x2d, 0x6d, 0x65, 0x6d, 0xc2, 0xb0, //|m..e..used-mem..|
    0xc4, 0x10, 0x00, 0xfa, 0x08, 0x61, 0x6f, 0x66, 0x2d, 0x62, 0x61, 0x73, 0x65, 0xc0, 0x00, 0xff, //|.....aof-base...|
    0xf0, 0x6e, 0x3b, 0xfe, 0xc0, 0xff, 0x5a, 0xa2};                                                //|.n;...Z.|
}

