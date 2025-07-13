#ifndef REPLICATION_H
#define REPLICATION_H
#include <string>

extern std::string master_replid;
extern size_t master_repl_offset;

bool is_slave();

void send_handshake(int master_fd);

#endif //REPLICATION_H
