#ifndef REPLICATION_H
#define REPLICATION_H
#include <string>
#include <vector>
#include <queue>

extern std::string master_replid;
size_t& master_repl_offset();

std::deque<std::pair<std::string, int>>& command_queue();
int& slave_count();
size_t& top_offset();
void slave_disconnected();
void add_command(const std::string& command);
void remove_command();

bool is_slave();

std::string send_handshake(int master_fd);
std::vector<unsigned char> make_rdb();

#endif //REPLICATION_H
