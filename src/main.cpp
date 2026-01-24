#include "resp.h"
#include "commands.h"

#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>

#define MAX_BUFFER_SIZE 512

CommandExecutor executor{};

void handleClient(int epoll_fd, int client_fd) {
  std::vector<u8> buffer(MAX_BUFFER_SIZE);
  int numBytesRead = read(client_fd, buffer.data(), buffer.size());
  
  if (numBytesRead <= 0) {
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
    close(client_fd);
    std::cout << "Client disconnected\n";
  }
  
  buffer.resize(numBytesRead); // prevent parser from trying to handle null elements at end of buffer
  std::cout << "Client connected, reading... \n";

  RespParser parser(buffer);
  auto client_input = parser.parse();
  
  Resp response;
  if (!client_input || !parser.bufferEmpty())
    response = Resp::error("ERR invalid protocol");
  else
    response = executor.execute(*client_input);

  std::string res_str {response.encode()};
  send(client_fd, res_str.c_str(), res_str.length(), 0);
}

void connectClient(int epoll_fd, int server_fd) {
  struct sockaddr_in client_addr;
  int client_addr_len = sizeof(client_addr);
  int client_fd = accept(server_fd, (sockaddr*) &client_addr, (socklen_t*) &client_addr_len);
  
  if (client_fd < 0) {
    std::cerr << "accept failed\n";
    return;
  }
  
  int ret = fcntl(client_fd, F_SETFL, O_NONBLOCK);
  if (ret < 0) {
    std::cerr << "fcntl failed\n";
    close(client_fd);
    return;
  }

  struct epoll_event client_event;
  client_event.data.fd = client_fd;
  client_event.events = EPOLLIN;
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_event);

  std::cout << "Established connection with new client\n";
}

int main(int argc, char **argv) {
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  
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
  server_addr.sin_port = htons(6379);
  
  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
    std::cerr << "Failed to bind to port 6379\n";
    return 1;
  }
  
  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return 1;
  }
  
  
  std::cout << "Waiting for a client to connect...\n";

  int epoll_fd = epoll_create(1);
  if (epoll_fd < 0) {
    std::cerr << "epoll_create failed\n";
    return 1;
  }

  struct epoll_event server_event;
  server_event.data.fd = server_fd;
  server_event.events = EPOLLIN;
  fcntl(server_fd, F_SETFL, O_NONBLOCK);
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &server_event);

  while (true) {
    struct epoll_event events[64] {};
    int num_ready = epoll_wait(epoll_fd, events, sizeof(events) / sizeof(epoll_event), -1);
    
    if (num_ready == -1) {
      std::cerr << "epoll error.\n";
      break;
    } else if (num_ready == 0) {
      std::cerr << "epoll timed out.\n";
      break;
    }

    for (int i{0}; i < num_ready; ++i) {
      if (events[i].data.fd == server_fd) { // we can accept a new client connection request
        connectClient(epoll_fd, server_fd);
      }
      else { // read() from the client and send() a response
        handleClient(epoll_fd, events[i].data.fd);
      }
    }

  }
  close(epoll_fd);
  close(server_fd);
  return 0;
}
