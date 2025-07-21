#include <iostream>
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
  char in_buffer[buffer_size]{};

  int client_fd;
  Rel_data data;

  public:
  explicit Rel(const int fd, const bool is_replica = false) : client_fd(fd)
  {
    data.is_replica = is_replica;
  }

  void operator()()
  {
    while (true)
    {
      if (data.client_is_replica)
      {
        if (top_offset() != data.local_offset)
        {
          std::this_thread::yield();
          continue;
        }
        if (command_queue().empty())
        {
          std::this_thread::yield();
          continue;
        }
        response = command_queue().front().first;
        long n = send(client_fd, response.c_str(), response.length(), MSG_NOSIGNAL);
        if (n == -1)
        {
          slave_disconnected();
          remove_command();
          break;
        }
        command_queue().front().second--;
        data.local_offset++;
        remove_command();
        continue;
      }
      const long n = recv(client_fd, in_buffer, buffer_size, 0);
      if (n == 0)
      {
        break;
      }

      in_stream.str({in_buffer, static_cast<size_t>(n)});
      in_stream.clear();

      size_t prevg = 0;
      while (in_stream.peek() != EOF)
      {
        RESP_data cmd = parse(in_stream);

        response = process_command(cmd, data);
        // will be 2^64-1 on eof, but that doesn't matter as substr will copy only to the end of a string
        const size_t currg = in_stream.tellg();

        if (data.repeat)
        {
          data.repeat = false;
          add_command(in_stream.str().substr(prevg, currg - prevg));
          master_repl_offset()++;
        }
        if (data.is_replica)
        {
          continue;
        }
        send(client_fd, response.c_str(), response.length(), 0);
        if (data.send_rdb)
        {
          data.send_rdb = false;
          std::vector<unsigned char> data = make_rdb();
          send(client_fd, data.data(), data.size(), 0);
          std::cout << "Sent database to replica\n";
        }
        prevg = currg;
      }
    }

    close(client_fd);
    std::string str = "Client";
    if (data.client_is_replica)
    {
      str = "Replica";
    }
    if (data.is_replica)
    {
      str = "Master";
    }
    std::cout << str + " disconnected\n";
  }
};

int main(const int argc, char **argv) {
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
  constexpr int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    std::cerr << "setsockopt failed\n";
    return 1;
  }
  
  sockaddr_in server_addr{};
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(std::stoi(config_key_vals()["port"]));
  
  if (bind(server_fd, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) != 0) {
    std::cerr << "Failed to bind to port " + config_key_vals()["port"] + "\n";
    return 1;
  }

  if (constexpr int connection_backlog = 5; listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return 1;
  }

  std::vector<std::thread> rels;

  if (is_slave())
  {
    const int master_fd = socket(AF_INET, SOCK_STREAM, 0);
    const std::string str = config_key_vals()["replicaof"];
    const std::string madd = str.substr(0, str.find(' '));
    const std::string mp = str.substr(str.rfind(' ') + 1);

    sockaddr_in master_addr{};
    master_addr.sin_family = AF_INET;
    inet_pton(AF_INET, madd.c_str(), &master_addr.sin_addr);
    master_addr.sin_port = htons(std::stoi(mp));

    if (connect(master_fd, reinterpret_cast<sockaddr*>(&master_addr), sizeof(master_addr)) != 0)
    {
      const int err = errno;
      std::cerr << "Failed to connect to master:\n" << strerror(err) << "\n";
      return 1;
    }
    send_handshake(master_fd);
    std::cout << "Sent handshake to master\n";
    rels.emplace_back(Rel(master_fd, true));
  }
  else
  {
    read_rdb();
  }


  std::thread rel_adder([server_fd, &rels]()
  {
    while (true)
    {
      sockaddr_in client_addr{};
      int client_addr_len = sizeof(client_addr);
      std::cout << "Waiting for a client to connect...\n";

      const int client_fd = accept(server_fd, reinterpret_cast<sockaddr*>(&client_addr), reinterpret_cast<socklen_t*>(&client_addr_len));
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
