#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <thread>
#include <vector>

#include "Command.h"
#include "Resp.h"
#include "Args.h"
#include "Database.h"
#include "Replication.h"

constexpr int buffer_size = 4096;

class Rel
{
  std::string response;
  std::stringstream in_stream{};
  char in_buffer[buffer_size];

  int client_fd;

  public:
  Rel(const int fd) : client_fd(fd)
  {
  }

  void operator()()
  {
    while (true)
    {
      int n = recv(client_fd, in_buffer, buffer_size, 0);
      if (n == 0)
      {
        break;
      }

      in_stream.str(in_buffer);
      in_stream.clear();

      RESP_data cmd = parse(in_stream);

      response = process_command(cmd);

      send(client_fd, response.c_str(), response.length(), 0);
    }

    close(client_fd);
    std::cout << "Client disconnected\n";
  }
};

int main(int argc, char **argv) {
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  if (!process_args(argc, argv))
  {
    std::cerr << "Failed to apply at least one argument\n";
  }
  
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
   std::cerr << "Failed to create server socket\n";
   return 1;
  }
  
  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    std::cerr << "setsockopt failed\n";
    return 1;
  }
  
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(std::stoi(config_key_vals["port"]));
  
  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
    std::cerr << "Failed to bind to port " + config_key_vals["port"] + "\n";
    return 1;
  }
  
  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return 1;
  }

  if (is_slave())
  {
    int master_fd = socket(AF_INET, SOCK_STREAM, 0);
    const std::string str = config_key_vals["replicaof"];
    const std::string madd = str.substr(0, str.find(' '));
    const std::string mp = str.substr(str.rfind(' ') + 1);

    sockaddr_in master_addr{};
    master_addr.sin_family = AF_INET;
    inet_pton(AF_INET, madd.c_str(), &master_addr.sin_addr);
    master_addr.sin_port = htons(std::stoi(mp));

    if (connect(master_fd, (sockaddr *) &master_addr, sizeof(master_addr)) != 0)
    {
      const int err = errno;
      std::cerr << "Failed to connect to master:\n" << strerror(err) << "\n";
      return 1;
    }
    send_handshake(master_fd);
    std::cout << "Sent handshake\n";
  }
  else
  {
    read_rdb();
  }

  std::vector<std::thread> rels;

  std::thread rel_adder([server_fd, &rels]()
  {
    while (true)
    {
      sockaddr_in client_addr{};
      int client_addr_len = sizeof(client_addr);
      std::cout << "Waiting for a client to connect...\n";

      int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
      std::cout << "Client connected\n";

      rels.emplace_back(Rel(client_fd));

      // some ending condition
    }
  });

  rel_adder.join();
  // also need a relation manager/ender

  close(server_fd);
  std::cout << "Server terminated\n";

  return 0;
}
