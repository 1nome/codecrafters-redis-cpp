#ifndef COMMAND_H
#define COMMAND_H

#include <set>
#include <string>

#include "Replication.h"
#include "Resp.h"

struct Rel_data
{
    bool send_rdb = false;
    bool client_is_replica = false;
    bool repeat = false;
    size_t local_offset = top_offset();
    bool is_replica = false;
    bool respond = false;
    bool queue_commands = false;
    std::queue<RESP_data> transaction_queue;
    std::vector<std::string> transaction_responses;
    std::set<std::string> subscribed_channels;
    bool subscribed = false;
    bool set_nonblocking = false;
};

std::string process_command(const RESP_data& resp, Rel_data& data);

#endif //COMMAND_H
