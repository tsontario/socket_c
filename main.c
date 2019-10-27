#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "request_handler.h"

int main(int argc, char** argv)
{
  in_addr_t HOST = htonl(INADDR_ANY); // Bind to all available interfaces
  int PORT = 8989;
  int c;

  while ((c = getopt(argc, argv, "p::")) != -1)  {
    switch(c)
    {
      case 'p':
        PORT = atoi(optarg);
        break;
    }
  }

  // Constants...
  // const char* HOST = "127.0.0.1";
  // const int PORT = 8989;
  const int SET = 1;

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
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &SET, sizeof(int)) < 0)
  {
    printf("setsockopt(SO_REUSEADDR) failed");
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
    
    For each new incoming connection, we assign a dedicated port for the TCP stream.
    We expect each incoming READ to be an HTTP request, which we handle appropriately.
    If we are unable to parse the incoming request, we return an HTTP 500 response.
*/
  while(1)
  {
    int client_sock;
    pid_t child_process;

    if ((client_sock = accept(server_fd, (struct sockaddr*)&remote_addr, &remote_socklen)) == -1)
    {
      perror("error accepting connection");
      continue;
    }

    // accept() returns a dedicated socket for the connection. We fork the process, close the new connection in the parent,
    // and let the child process handle the request, closing it only after the session is terminated
    if ((child_process = fork()) == -1)
    {
      // Log the error but don't kill the server (the problem could be intermittent)
      perror("error forking new connection");
      continue;
    }
    else if (child_process > 0)
    {
      // We are in the parent process. Close the accepted socket (it is the child process's responsibility now) and prepare to accept a new connection
      close(client_sock);
      continue;
    } else {
      // We are in the forked child process. Handle the new connection in this subprocess from here on out
      handle_conn(client_sock);
    }
  }
}
