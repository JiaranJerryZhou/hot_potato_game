#include "potato.h"
#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

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

  char addr_buf[512] = "";
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
      cerr << "Error: cannot get address info for curr player" << endl;
      cerr << "  (" << addr_buf << ", " << port_prev << ")" << endl;
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
  send(socket_fd, port_buf, sizeof(port_buf), 0);

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
  // receive itself id
  char addr_next[512];
  char port_next[512];
  char id[512];
  recv(socket_fd, addr_next, sizeof(addr_next), 0);
  recv(socket_fd, port_next, sizeof(port_next), 0);
  recv(socket_fd, id, sizeof(id), 0);
  status_next =
      getaddrinfo(addr_next, port_next, &host_info_next, &host_info_list_next);
  if (status_next != 0) {
    cerr << "Error: cannot get address info for next player" << endl;
    cerr << "  (" << addr_next << ", " << port_next << ")" << endl;
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

  // send its id to next and prev player
  char id_prev[512];
  char id_next[512];
  send(socket_fd_next, id, 512, 0);
  recv(client_connection_fd, id_prev, 512, 0);
  send(client_connection_fd, id, 512, 0);
  recv(socket_fd_next, id_next, 512, 0);

  cout << id_next << endl;
  cout << id_prev << endl;

  // pass potatos!
  // create a set of readfds
  fd_set master;
  FD_SET(socket_fd, &master);
  FD_SET(socket_fd_next, &master);
  FD_SET(client_connection_fd, &master);
  fd_set read_fds;
  int fdmax = max(max(socket_fd, socket_fd_next), client_connection_fd);
  int nbytes;
  char buf[512];
  potato my_potato;
  while (true) {
    read_fds = master;
    if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) {
      perror("select");
      return -1;
    }
    // run through existing connections looking for data to read
    for (int i = 0; i <= fdmax; i++) {
      if (FD_ISSET(i, &read_fds)) {
        // handle potato or signal
        if ((nbytes = recv(i, buf, sizeof(buf), 0)) <= 0) {
          // got error or connection closed by client
          if (nbytes == 0) {
            // connection closed
            printf("selectserver: socket %d hung up\n", i);
          } else {
            perror("recv");
          }
          close(i);           // bye!
          FD_CLR(i, &master); // remove from master set
        } else {
          // we got some data from elsewhere
          cout << buf << endl;
          if (strcmp(buf, "gameover") == 0) {
            // game over
            freeaddrinfo(host_info_list);
            close(socket_fd);
            freeaddrinfo(host_info_list_next);
            close(socket_fd_next);
            freeaddrinfo(host_info_list_prev);
            close(socket_fd_prev);
            close(client_connection_fd);
            return 0;
          } else if (strcmp(buf, "potato") == 0) {
            // potato arrives
            recv(i, &my_potato, sizeof(my_potato), 0);
            my_potato.id[my_potato.hop] = stoi(id);
            my_potato.hop++;
            // game over
            if (my_potato.hop == my_potato.target) {
              cout << "I'm it" << endl;
              send(socket_fd, &my_potato, sizeof(my_potato), 0);
            }
            // send to next player
            else {
              int pass_to = rand() % 2;
              // pass to prev player
              if (pass_to == 0) {
                cout << "Sending potato to " << id_prev << endl;
                send(client_connection_fd, "potato", 512, 0);
                send(client_connection_fd, &my_potato, sizeof(potato), 0);
              }
              // send to next player
              else if (pass_to == 1) {
                cout << "Sending potato to " << id_next << endl;
                send(socket_fd_next, "potato", 512, 0);
                send(socket_fd_next, &my_potato, sizeof(potato), 0);
              } else {
                perror("no player to send");
                return -1;
              }
            }

          } else {
            perror("unrecognized signal");
            return -1;
          }
        }
      }
    }
  }

  // end game
  freeaddrinfo(host_info_list);
  close(socket_fd);
  freeaddrinfo(host_info_list_next);
  close(socket_fd_next);
  freeaddrinfo(host_info_list_prev);
  close(socket_fd_prev);
  return 0;
}
