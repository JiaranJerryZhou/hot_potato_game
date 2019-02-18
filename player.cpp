#include <cstring>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

#include "potato.h"

using namespace std;

int main(int argc, char *argv[]) {
  if (argc != 3) {
    cout << "not enough arguments" << endl;
    return 0;
  }
  // master
  int status;
  int socket_fd;
  struct addrinfo host_info;
  struct addrinfo *host_info_list;
  memset(&host_info, 0, sizeof(host_info));

  host_info.ai_family = AF_UNSPEC;
  host_info.ai_socktype = SOCK_STREAM;

  // create a socket to connect to ring master
  const char *machine_name = argv[1];
  const char *port = argv[2];

  status = getaddrinfo(machine_name, port, &host_info, &host_info_list);
  if (status != 0) {
    cerr << "Error: cannot get address info for host" << endl;
    cerr << "  (" << machine_name << "," << port << ")" << endl;
    return -1;
  } // if

  socket_fd = socket(host_info_list->ai_family, host_info_list->ai_socktype,
                     host_info_list->ai_protocol);
  if (socket_fd == -1) {
    cerr << "Error: cannot create socket" << endl;
    cerr << "  (" << machine_name << "," << port << ")" << endl;
    return -1;
  } // if

  cout << "Connecting to " << machine_name << " on port " << port << "..."
       << endl;

  status =
      connect(socket_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
  if (status == -1) {
    cerr << "Error: cannot connect to socket" << endl;
    cerr << "  (" << machine_name << "," << port << ")" << endl;
    return -1;
  } // if

  // create new server
  int status_prev;
  int socket_fd_prev;
  struct addrinfo host_info_prev;
  struct addrinfo *host_info_list_prev;
  memset(&host_info_prev, 0, sizeof(host_info_prev));

  host_info_prev.ai_family = AF_UNSPEC;
  host_info_prev.ai_socktype = SOCK_STREAM;
  host_info_prev.ai_flags = AI_PASSIVE;

  char addr_buf[512];
  status = gethostname(addr_buf, sizeof(addr_buf));
  // bind a new port to create server
  srand((unsigned int)time(NULL));
  int random = rand() % 65536;
  if (random < 1024) {
    random += 1024;
  }
  int valid_port = random;
  while (true) {
    const char *port_prev = to_string(random).c_str();
    status_prev =
        getaddrinfo(addr_buf, port_prev, &host_info_prev, &host_info_list_prev);
    if (status_prev != 0) {
      cerr << "Error: cannot get address info for next player" << endl;
      cerr << "  (" << addr_buf << "," << port_prev << ")" << endl;
      return -1;
    } // if

    socket_fd_prev =
        socket(host_info_list_prev->ai_family, host_info_list_prev->ai_socktype,
               host_info_list_prev->ai_protocol);
    if (socket_fd_prev == -1) {
      cerr << "Error: cannot create socket" << endl;
      cerr << "  (" << addr_buf << "," << port_prev << ")" << endl;
      return -1;
    } // if

    // bind
    int yes = 1;
    status_prev =
        setsockopt(socket_fd_prev, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    status_prev = bind(socket_fd_prev, host_info_list_prev->ai_addr,
                       host_info_list_prev->ai_addrlen);
    if (status_prev == 0) {
      break;
    } // if
    random++;
    valid_port = random;
  }
  cout << "bind to a new port " << valid_port << endl;

  // send address and port to ringmaster
  send(socket_fd, addr_buf, sizeof(addr_buf), 0);
  const char *port_buf = to_string(valid_port).c_str();
  send(socket_fd, port_buf, sizeof(addr_buf), 0);

  // listen
  status_prev = listen(socket_fd_prev, 10);
  if (status_prev == -1) {
    cerr << "Error: cannot listen on socket" << endl;
    cerr << "  (" << addr_buf << "," << valid_port << ")" << endl;
    return -1;
  } // if

  cout << "Waiting for connection on port " << valid_port << endl;

  // connect to next player
  int status_next;
  int socket_fd_next;
  struct addrinfo host_info_next;
  struct addrinfo *host_info_list_next;
  memset(&host_info_next, 0, sizeof(host_info_next));

  host_info_next.ai_family = AF_UNSPEC;
  host_info_next.ai_socktype = SOCK_STREAM;

  // create a socket to connect to next player
  // receive next player's information from master
  char addr_next[512];
  char port_next[512];
  recv(socket_fd, addr_next, sizeof(addr_next), 0);
  recv(socket_fd, port_next, sizeof(port_next), 0);
  status_next =
      getaddrinfo(addr_next, port_next, &host_info_next, &host_info_list_next);
  if (status_next != 0) {
    cerr << "Error: cannot get address info for next player" << endl;
    cerr << "  (" << addr_next << "," << port_next << ")" << endl;
    return -1;
  } // if

  socket_fd_next =
      socket(host_info_list_next->ai_family, host_info_list_next->ai_socktype,
             host_info_list_next->ai_protocol);
  if (socket_fd_next == -1) {
    cerr << "Error: cannot create socket" << endl;
    cerr << "  (" << addr_next << "," << port_next << ")" << endl;
    return -1;
  } // if

  cout << "Connecting to next player " << addr_next << " on port " << port_next
       << "..." << endl;

  status_next = connect(socket_fd_next, host_info_list_next->ai_addr,
                        host_info_list_next->ai_addrlen);
  if (status_next == -1) {
    cerr << "Error: cannot connect to socket" << endl;
    cerr << "  (" << addr_next << "," << port_next << ")" << endl;
    return -1;
  } // if

  struct sockaddr_storage socket_addr;
  socklen_t socket_addr_len = sizeof(socket_addr);
  int client_connection_fd;
  client_connection_fd =
      accept(socket_fd_prev, (struct sockaddr *)&socket_addr, &socket_addr_len);
  if (client_connection_fd == -1) {
    cerr << "Error: cannot accept connection on socket" << endl;
    return -1;
  } // if
  cout << "connected to prev player" << endl;

  // end game
  freeaddrinfo(host_info_list);
  close(socket_fd);
  freeaddrinfo(host_info_list_next);
  close(socket_fd_next);
  freeaddrinfo(host_info_list_prev);
  close(socket_fd_prev);
}
