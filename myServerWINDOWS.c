#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "request_handler.h"

/*
  myServerWINDOWS is for compatibility with Windows systems. It foregoes the use of forking
  new client connections since that is not supported with the same API as Linux/OSX. To compile for windows,
  run `make windows`, which build the binary `myServerWINDOWS`
*/

// main instantiates a new TCP/HTTP server. Requests are handled by forking the main server
// and having the child process take care of a given, individual, connection
int main(int argc, char** argv)
{
  in_addr_t HOST = htonl(INADDR_ANY); // Bind to all available interfaces
  int PORT = 8989;

  int opt;
  while ((opt = getopt(argc, argv, "p::")) != -1)  {
    switch(opt)
    {
      case 'p':
        PORT = atoi(optarg);
        break;
    }
  }

  // Server variables
  struct sockaddr_in server_addr, remote_addr;
  socklen_t remote_socklen;
  int server_fd;

  printf("\x1b[39;1mSetting up local http server (binding to all inet interfaces) on port \x1b[32;1m%d\x1b[39m\n",PORT);

  // Create the socket
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1)
  {
    perror("error creating socket");
    return 1;
  }
  // set SO_REUSEADDR so we can rebind to the same port quickly in the event of a restart
  const int SET = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &SET, sizeof(int)) < 0)
  {
    printf("setsockopt(SO_REUSEADDR) failed\n");
    return 1;
  }
  // Configure to listen on HOST:PORT
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = HOST;
  server_addr.sin_port = htons(PORT);

  // Bind socket to server_addr
  if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1)
  {
    perror("error binding socket");
    return 1;
  }

  // Set the server to passively wait for connections, allowing MAX_CONNS requests to queue
  if (listen(server_fd, MAX_CONNS) == -1)
  {
    perror("error listening on socket");
    return 1;
  }

  printf("Socket successfully bound, awaiting incoming connections...\n");

/*
    *Main server loop*
    
    The server forks on each new connection: the parent process immediately closes the client socket and
    begins waiting for a new connection. The child process the takes responsibility for servicing the request.
    We expect each incoming READ to be an HTTP request, otherwise we return an appropriate HTTP error.
*/
  while(1)
  {
    int client_sock;
    pid_t child_process;

    if ((client_sock = accept(server_fd, (struct sockaddr*)&remote_addr, &remote_socklen)) == -1)
    {
      // Log the error but don't kill the server (the problem could be intermittent)
      perror("error accepting connection");
      continue;
    }
		// We are in the forked child process. Handle the new connection in this subprocess from here on out
		handle_conn(client_sock);
  }
}
