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

#define MAX_CONNS 20
#define BUF_SIZE 8096

typedef struct {
  char* verb;
  char* path;
  char** headers;
  char* body;
} http_req;


// parse VERB, path, headers, and body of the incoming HTTP request and place in the http_req pointed to by req
void parse_http_req(char* buffer, size_t buf_len, http_req* req)
{
  char* line_buf[BUF_SIZE];
  size_t line_buf_size = sizeof(line_buf);
  FILE* req_fd = fmemopen(buffer, buf_len, "r");
  getline(line_buf, &line_buf_size, req_fd);
  printf("contents of linebuf: %s\n", *line_buf);
  // TODO so this works and grabs the first line
  // TODO sep on ' ', then process further lines until empty line, then we have body then either EOF or end of buffer
}

int handle_conn(int client_sock)
{
  int bytes_read;
  http_req request;
  char* buffer = (char*)malloc(BUF_SIZE + 1);
  if (errno != 0)
  {
    perror("error allocating buffer");
    close(client_sock);
    return 1;
  }
  memset(buffer, 0, BUF_SIZE);  // Zero the buffer

  if ((bytes_read = recv(client_sock, buffer, BUF_SIZE, 0)) == -1)
  {
    perror("error reading incoming request");
    // Close the socket and return
    close(client_sock);
    return 1;
  }
  
  buffer[bytes_read+1] = '\0';  // Defensively ensure we have a null byte available in case request was too big (we just drop whatever didn't fit)
  parse_http_req(buffer, bytes_read, &request);

  
  if (close(client_sock) == -1)
  {
    perror("error closing socket");
    return 1;
  }
  return 0;
}

int main()
{
  // Constants...
  const char* HOST = "127.0.0.1";
  const int PORT = 8989;
  const int SET = 1;

  // Server variables
  struct sockaddr_in server_addr, remote_addr;
  socklen_t remote_socklen;
  int server_fd;

  printf("\x1b[39;1mSetting up local server: \x1b[32;1mhttp://%s:%d\x1b[39m\n", HOST, PORT);

  // Create the socket
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1)
  {
    perror("error creating socket");
    return 1;
  }
  // Configure to listen on HOST:PORT
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(HOST);
  server_addr.sin_port = htons(PORT);

  // Bind socket to server_addr
  if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1)
  {
    perror("error binding socket");
    return 1;
  }

  // Set SO_REUSEADDR option (eases local development). Should be removed when production-ready
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &SET, sizeof(int)) == -1)
  {
    perror("setting socket options to reuse local addr");
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
      handle_conn(client_sock);
      // We are in the forked subprocess: handle the connection
      // TODO put in a serve() function to clean things up
      // TODO we expect every READ will be an HTTP request -> let's read it line by line and construct what we need
      /*
        <METHOD> <PATH> HTTP/1.1
        <HEADER_KEY>: <HEADER_VAL>  (N times)
        BLANK_LINE
        <REQUEST_BODY>
      */
    }
  }
}
