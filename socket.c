#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef MAX_CONNS
#define MAX_CONNS 20
#endif

int main()
{
  struct sockaddr_in server_addr, remote_addr;
  socklen_t remote_socklen;
  int sock_id;

  sock_id = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_id == -1)
  {
    perror("error creating socket");
    return 1;
  }
  
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  server_addr.sin_port = htons(8989);

  if (bind(sock_id, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1)
  {
    perror("error binding socket");
    return 1;
  }
  
  if (listen(sock_id, MAX_CONNS) == -1)
  {
    perror("error listening on socket");
    return 1;
  }

  while(1)
  {
    int incoming_sock; 
    if ((incoming_sock = accept(sock_id, (struct sockaddr*)&remote_addr, &remote_socklen)) == -1)
    {
      perror("error accepting connection");
      continue;
    }
    char* message = "<h1>HELLO WORLD</h1";
    send(incoming_sock, message, sizeof(message), 0);
    close(incoming_sock);

  }
}