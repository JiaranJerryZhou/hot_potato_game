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
  int client_connection_fd[num_players];
  struct sockaddr_in socket_addr[num_players];
  socklen_t socket_addr_len = sizeof(socket_addr[0]);
  while (curr_players < num_players - 1) {
    // cout << "Waiting for connection on port " << port << endl;
    client_connection_fd[curr_players] =
        accept(socket_fd, (struct sockaddr *)&socket_addr[num_players],
               &socket_addr_len);
    if (client_connection_fd[curr_players] == -1) {
      cerr << "Error: cannot accept connection on socket" << endl;
      return -1;
    } // if

    cout << "Player " << curr_players << " is ready to play" << endl;
    curr_players++;
  }

  // send each player himself next player's hostname and port
  for (int i = 0; i < num_players; i++) {
    struct in_addr in_addr;
    unsigned long addr;
    unsigned short port;
    in_addr = socket_addr[i].sin_addr;
    addr = in_addr.s_addr;
    port = socket_addr[i].sin_port;
    string message = to_string(addr) + " " + to_string(port);
    const char *msg = message.c_str();
    send(client_connection_fd[i], msg, strlen(msg), 0);
  }

  // send each player his next player's hostname and port
  for (int i = 0; i < num_players; i++) {
    struct in_addr in_addr;
    unsigned long addr;
    unsigned short port;
    if (i == num_players - 1) {
      in_addr = socket_addr[0].sin_addr;
      addr = in_addr.s_addr;
      port = socket_addr[0].sin_port;
    } else {
      in_addr = socket_addr[i + 1].sin_addr;
      addr = in_addr.s_addr;
      port = socket_addr[i + 1].sin_port;
    }
    string message = to_string(addr) + " " + to_string(port);
    const char *msg = message.c_str();
    send(client_connection_fd[i], msg, strlen(msg), 0);
  }

  if (num_hops > 0) {
    // randomly choose a player to send potato
    srand((unsigned int)time(NULL));
    int random = rand() % num_players;
    cout << "Ready to start the game, sending potato to player " << random
         << endl;
    potato my_potato;
    my_potato.hop = num_hops;
    my_potato.id[0] = random;
    send(client_connection_fd[random], &my_potato, sizeof(&my_potato), 0);

    // receive the potato after the game ends
  }

  // close the game
  freeaddrinfo(host_info_list);
  close(socket_fd);
  return 0;
}
