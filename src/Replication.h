#ifndef REPLICATION_H
#define REPLICATION_H
#include <string>
#include <vector>

extern std::string master_replid;
extern size_t master_repl_offset;

extern bool send_rdb;

bool is_slave();

void send_handshake(int master_fd);
std::vector<unsigned char> make_rdb();

#endif //REPLICATION_H
