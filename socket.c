#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_CONNS 20
#define BUF_SIZE 8096

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
    int bytes_read;
    char* buffer = (char*)malloc(BUF_SIZE + 1);
     
    if ((incoming_sock = accept(sock_id, (struct sockaddr*)&remote_addr, &remote_socklen)) == -1)
    {
      perror("error accepting connection");
      continue;
    }
    if ((bytes_read = recv(incoming_sock, buffer, BUF_SIZE, 0)) == -1)
    {
      perror("error reading incoming request");
      return 1;
    }
    buffer[bytes_read] = '\0';
    printf("%s", buffer);
    
    close(incoming_sock);
  }
}