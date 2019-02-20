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
#include <time.h>
#include <unistd.h>
#include <vector>

#include "potato.h"

using namespace std;

int main(int argc, char *argv[]) {
  if (argc != 4) {
    cout << "not enough arguments" << endl;
    return 0;
  }
  int status;
  int socket_fd;
  struct addrinfo host_info;
  struct addrinfo *host_info_list;
  const char *hostname = "0.0.0.0";
  const char *port = argv[1];
  int num_players = stoi(argv[2]);
  int num_hops = stoi(argv[3]);
  vector<string> player_ports;
  vector<string> player_addr;
  memset(&host_info, 0, sizeof(host_info));

  host_info.ai_family = AF_UNSPEC;
  host_info.ai_socktype = SOCK_STREAM;
  host_info.ai_flags = AI_PASSIVE;

  status = getaddrinfo(hostname, port, &host_info, &host_info_list);
  if (status != 0) {
    cerr << "Error: cannot get address info for host" << endl;
    cerr << "  (" << hostname << "," << port << ")" << endl;
    return -1;
  } // if

  socket_fd = socket(host_info_list->ai_family, host_info_list->ai_socktype,
                     host_info_list->ai_protocol);
  if (socket_fd == -1) {
    cerr << "Error: cannot create socket" << endl;
    cerr << "  (" << hostname << "," << port << ")" << endl;
    return -1;
  } // if

  // ringmaster initialized
  cout << "Potato Ringmaster" << endl;
  cout << "Players = " << num_players << endl;
  cout << "Hops = " << num_hops << endl;

  // bind and listen
  int yes = 1;
  status = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
  status = bind(socket_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
  if (status == -1) {
    cerr << "Error: cannot bind socket" << endl;
    cerr << "  (" << hostname << "," << port << ")" << endl;
    return -1;
  } // if

  status = listen(socket_fd, num_players);
  if (status == -1) {
    cerr << "Error: cannot listen on socket" << endl;
    cerr << "  (" << hostname << "," << port << ")" << endl;
    return -1;
  } // if

  // create the ring
  int curr_players = 0;
  int *client_connection_fd = new int[num_players];
  struct sockaddr_in socket_addr;
  socklen_t socket_addr_len = sizeof(socket_addr);
  while (curr_players < num_players) {
    // cout << "Waiting for connection on port " << port << endl;
    client_connection_fd[curr_players] =
        accept(socket_fd, (struct sockaddr *)&socket_addr, &socket_addr_len);
    if (client_connection_fd[curr_players] == -1) {
      cerr << "Error: cannot accept connection on socket" << endl;
      return -1;
    } // if
    char addr_buf[512];
    recv(client_connection_fd[curr_players], addr_buf, sizeof(addr_buf), 0);
    string player_address(addr_buf);
    player_addr.push_back(player_address);
    char port_buf[512];
    recv(client_connection_fd[curr_players], port_buf, sizeof(port_buf), 0);
    string player_port(port_buf);
    player_ports.push_back(player_port);

    cout << "Player " << curr_players << " is ready to play" << endl;

    curr_players++;
  }

  // send each player his next player's hostname and port
  // send them their id
  for (int i = 0; i < num_players; i++) {
    string addr_next;
    string port_next;
    if (i == num_players - 1) {

      addr_next = player_addr[0];
      port_next = player_ports[0];
    } else {
      addr_next = player_addr[i + 1];
      port_next = player_ports[i + 1];
    }
    cout << "player " << i << ": " << addr_next << " " << port_next << endl;
    const char *addr_msg = addr_next.c_str();
    const char *port_msg = port_next.c_str();
    const char *id_msg = to_string(i).c_str();
    send(client_connection_fd[i], addr_msg, 512, 0);
    send(client_connection_fd[i], port_msg, 512, 0);
    send(client_connection_fd[i], id_msg, 512, 0);
  }
  cout << "sent each player his next player's hostname and port" << endl;

  if (num_hops > 0) {
    // randomly choose a player to send potato
    srand((unsigned int)time(NULL));
    int random = rand() % num_players;
    cout << "Ready to start the game, sending potato to player " << random
         << endl;
    potato my_potato;
    my_potato.target = num_hops;
    my_potato.hop = 0;
    my_potato.id[0] = random;
    send(client_connection_fd[random], "potato", 512, 0);
    send(client_connection_fd[random], &my_potato, sizeof(my_potato), 0);

    // receive the potato after the game ends
    // select
    fd_set master;
    int fdmax = 0;
    for (int i = 0; i < num_players; i++) {
      if (client_connection_fd[i] > fdmax) {
        fdmax = client_connection_fd[i];
      }
      FD_SET(client_connection_fd[i], &master);
    }
    fd_set read_fds;
    int nbytes;
    while (true) {
      read_fds = master;
      if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) {
        perror("select");
        return -1;
      }
      // check if any port returns the potato
      for (int i = 0; i < fdmax; i++) {
        if (FD_ISSET(i, &read_fds)) {
          // handle potato or signal
          if ((nbytes = recv(i, &my_potato, sizeof(potato), 0)) <= 0) {
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
            // we got the potato
            cout << "Trace of potato: ";
            for (int i = 0; i < num_hops - 1; i++) {
              cout << my_potato.id[i] << ", ";
            }
            cout << my_potato.id[num_hops - 1] << endl;
            // close all players sockets
            for (int i = 0; i < num_players; i++) {
              send(client_connection_fd[i], "gameover", 512, 0);
              close(client_connection_fd[i]);
            }
            // close the game
            freeaddrinfo(host_info_list);
            close(socket_fd);
            delete[] client_connection_fd;
            return 0;
          }
        }
      }
    }
  }
  // close the game
  freeaddrinfo(host_info_list);
  close(socket_fd);
  delete[] client_connection_fd;
  return 0;
}
