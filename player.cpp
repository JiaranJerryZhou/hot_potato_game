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

  // accept previous player
  int status_prev;
  int socket_fd_prev;
  struct addrinfo host_info_prev;
  struct addrinfo *host_info_list_prev;
  memset(&host_info_prev, 0, sizeof(host_info_prev));

  host_info_prev.ai_family = AF_UNSPEC;
  host_info_prev.ai_socktype = SOCK_STREAM;
  host_info_prev.ai_flags = AI_PASSIVE;

  char buffer_prev[512];
  recv(socket_fd, buffer_prev, sizeof(buffer_prev), 0);
  string prev_info(buffer_prev);
  int pos = prev_info.find(' ');
  const char *hostname_prev = "0.0.0.0";
  const char *port_prev = prev_info.substr(pos).c_str();

  status_prev = getaddrinfo(hostname_prev, port_prev, &host_info_prev,
                            &host_info_list_prev);
  if (status_prev != 0) {
    cerr << "Error: cannot get address info for next player" << endl;
    cerr << "  (" << hostname_prev << "," << port_prev << ")" << endl;
    return -1;
  } // if

  socket_fd_prev =
      socket(host_info_list_prev->ai_family, host_info_list_prev->ai_socktype,
             host_info_list_prev->ai_protocol);
  if (socket_fd_prev == -1) {
    cerr << "Error: cannot create socket" << endl;
    cerr << "  (" << hostname_prev << "," << port_prev << ")" << endl;
    return -1;
  } // if

  cout << "Connecting to " << hostname_prev << " on port " << port_prev << "..."
       << endl;

  // bind and listen
  int yes = 1;
  status_prev =
      setsockopt(socket_fd_prev, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
  status_prev = bind(socket_fd_prev, host_info_list_prev->ai_addr,
                     host_info_list_prev->ai_addrlen);
  if (status_prev == -1) {
    cerr << "Error: cannot bind socket" << endl;
    cerr << "  (" << hostname_prev << "," << port_prev << ")" << endl;
    return -1;
  } // if

  status_prev = listen(socket_fd_prev, 10);
  if (status_prev == -1) {
    cerr << "Error: cannot listen on socket" << endl;
    cerr << "  (" << hostname_prev << "," << port_prev << ")" << endl;
    return -1;
  } // if

  cout << "Waiting for connection on port " << port_prev << endl;
  struct sockaddr_storage socket_addr;
  socklen_t socket_addr_len = sizeof(socket_addr);
  int client_connection_fd;
  client_connection_fd =
      accept(socket_fd_prev, (struct sockaddr *)&socket_addr, &socket_addr_len);
  if (client_connection_fd == -1) {
    cerr << "Error: cannot accept connection on socket" << endl;
    return -1;
  } // if

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
  char buffer_next[512];
  recv(socket_fd, buffer_next, sizeof(buffer_next), 0);
  string next_info(buffer_next);
  pos = next_info.find(' ');
  const char *hostname_next = next_info.substr(0, pos).c_str();
  const char *port_next = next_info.substr(pos).c_str();
  status_next = getaddrinfo(hostname_next, port_next, &host_info_next,
                            &host_info_list_next);
  if (status_next != 0) {
    cerr << "Error: cannot get address info for next player" << endl;
    cerr << "  (" << hostname_next << "," << port_next << ")" << endl;
    return -1;
  } // if

  socket_fd_next =
      socket(host_info_list_next->ai_family, host_info_list_next->ai_socktype,
             host_info_list_next->ai_protocol);
  if (socket_fd_next == -1) {
    cerr << "Error: cannot create socket" << endl;
    cerr << "  (" << hostname_next << "," << port_next << ")" << endl;
    return -1;
  } // if

  cout << "Connecting to " << hostname_next << " on port " << port_next << "..."
       << endl;

  status_next = connect(socket_fd_next, host_info_list_next->ai_addr,
                        host_info_list_next->ai_addrlen);
  if (status_next == -1) {
    cerr << "Error: cannot connect to socket" << endl;
    cerr << "  (" << hostname_next << "," << port_next << ")" << endl;
    return -1;
  } // if

  // end game
  freeaddrinfo(host_info_list);
  close(socket_fd);
  freeaddrinfo(host_info_list_next);
  close(socket_fd_next);
  freeaddrinfo(host_info_list_prev);
  close(socket_fd_prev);
}
