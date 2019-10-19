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

typedef struct header_list header_list;

typedef struct {
  char* key;
  char* value;
} header_entry;

struct header_list {
  header_entry* entry;
  header_list* next; 
};

typedef struct {
  char* verb;
  char* path;
  char* version;
  header_list* headers;
  char* body;
} http_req;

// parse_start_line reads line and parses verb, path, and http version into req.
// It assumes that the provided line conforms to the structure of the start line of an HTTP
// request as outlined here: https://developer.mozilla.org/en-US/docs/Web/HTTP/Messages
int parse_start_line(FILE* req_fd,  http_req* req)
{
  // Setting line_ptr = NULL and line_size = 0 let's getline() allocate buffer space on-the-fly without us needing to alloc ourselves.
  // Note we still must free() line_ptr on each read.
  char* line_ptr = NULL;
  size_t line_size = 0;
  if (getline(&line_ptr, &line_size, req_fd) == -1)
  {
    printf("error reading line from request\n");
    return 1;
  }
  
  req->verb = strdup(strtok(line_ptr, " "));
  req->path = strdup(strtok(NULL, " "));
  req->version = strdup(strtok(NULL, "\r\n"));
  free(line_ptr);
  
  if (req->verb == NULL || req->path == NULL || req->version == NULL)
  {
    printf("failed to parse HTTP start line");
    return 1;
  }
  return 0;
}

int parse_headers(FILE* req_fd, http_req* req)
{
  // Setting line_ptr = NULL and line_size = 0 let's getline() allocate buffer space on-the-fly without us needing to alloc ourselves.
  // Note we still must free() line_ptr on each read.
  char* line_ptr = NULL;
  size_t line_size = 0;
  char* delim = (char *)' ';
  header_list* current;

  header_list* headers;
  req->headers = (header_list*)malloc(sizeof(header_list));
  if (errno != 0)
  {
    perror("allocating header list memory");
    return 1;
  }

  current = headers;
  while(1)
  {
    if (getline(&line_ptr, &line_size, req_fd) == -1)
    {
      printf("error reading line from request\n");
      return 1;
    }
    printf("HEADER_LINE: %s\n", line_ptr);
    if (strcmp(line_ptr, "\r\n") == 0)
    {
      printf("in end\n");
      current = NULL;
      return 0;
    }
    printf("Got a HEADER\n");
    header_entry* entry = (header_entry*)malloc(sizeof(header_entry));
    entry->key = strdup(strtok(line_ptr, ": "));
    entry->value = strdup(strtok(NULL, "\n"));
    printf("Found a header: %s:%s\n", entry->key, entry->value);
    current->next = (header_list*)malloc(sizeof(header_list));
    if (errno != 0)
    {
      perror("allocating header list memory (next entry)");
      return 1;
    }
    current = current->next;
  }

  return 0;
}

// parse verb, path, headers, and body of the incoming HTTP request and place in the http_req pointed to by req
void parse_http_req(char* buffer, size_t buf_len, http_req* req)
{
  // Setting line_ptr = NULL and line_size = 0 let's getline() allocate buffer space on-the-fly without us needing to alloc ourselves.
  // Note we still must free() line_ptr on each read.
  char* line_ptr = NULL;
  size_t line_size = 0;
  char* delim = (char *)' ';

  // Treating the buffer like a file let's us use simple functions for reading lines
  FILE* req_fd = fmemopen(buffer, buf_len, "r");
  
  printf("Before start line\n");
  // Get the HTTP start line and parse into req
  if (parse_start_line(req_fd, req) == -1)
  {
    printf("error parsing start line\n");
    return;
  }

  printf("Processing HTTP request...\nVERB: %s\nPATH: %s\nVERSION: %s\n", req->verb, req->path, req->version);
  printf("Parsing headers...\n");
  if (parse_headers(req_fd, req) == -1)
  {
    printf("error parsing headers\n");
    return;
  }
  printf("Done parsing headers\n");
  header_list* headers = req->headers;
  while (headers != NULL)
  {
    printf("Key: %s; Value: %s\n", headers->entry->key, headers->entry->value);
    headers = headers->next;
  }
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
