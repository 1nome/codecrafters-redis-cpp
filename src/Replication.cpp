#include "Replication.h"
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
    const std::string str1 = array({"PING"});
    send(master_fd, str1.c_str(), str1.size(), 0);
}
