#ifndef COMMAND_H
#define COMMAND_H

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
};

std::string process_command(const RESP_data& resp, Rel_data& data);

#endif //COMMAND_H
