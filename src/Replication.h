#ifndef REPLICATION_H
#define REPLICATION_H
#include <string>
#include <vector>
#include <queue>

struct PropagatedCmd
{
    std::string cmd;
    int remaining;
    bool expect_response;
    unsigned int timeout;
};

extern std::string master_replid;
size_t& master_repl_offset();

std::deque<PropagatedCmd>& command_queue();
int& slave_count();
size_t& top_offset();
void slave_disconnected();
void add_command(const std::string& command, bool expect_response = false, unsigned int timeout = 0);
void remove_command();

void replica_acked();
void reset_acks();
int n_acks();
bool& send_getack();

bool is_slave();

std::string send_handshake(int master_fd);
std::vector<unsigned char> make_rdb();

#endif //REPLICATION_H
