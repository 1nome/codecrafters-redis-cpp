#include "Replication.h"

#include <iostream>

#include "Database.h"
#include <sys/socket.h>
#include "Resp.h"

std::string master_replid = "VeryRandomStringThatIsFortyCharactersLon";
size_t master_repl_offset = 0;

bool is_slave()
{
    return config_key_vals.contains("replicaof");
}

void send_handshake(const int master_fd)
{
    constexpr int buffer_size = 4096;
    char in_buffer[buffer_size];

    const std::string str1 = command({"PING"});
    send(master_fd, str1.c_str(), str1.size(), 0);
    recv(master_fd, in_buffer, buffer_size, 0);

    const std::string str2 = command({"REPLCONF", "listening-port", config_key_vals["port"]});
    send(master_fd, str2.c_str(), str2.size(), 0);
    recv(master_fd, in_buffer, buffer_size, 0);

    const std::string str3 = command({"REPLCONF", "capa", "psync2"});
    send(master_fd, str3.c_str(), str3.size(), 0);
    recv(master_fd, in_buffer, buffer_size, 0);

    const std::string str4 = command({"PSYNC", "?", "-1"});
    send(master_fd, str4.c_str(), str4.size(), 0);
    recv(master_fd, in_buffer, buffer_size, 0);
}
