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
#include <stdbool.h>

#include "request_handler.h"
#include "parse.h"

/* 
  Handle_conn handles the incoming connection represented by client_sock
  It expects the incoming stream to be structured as an HTTP request, erroring out
  if this assumption is violated
*/
int handle_conn(int client_sock)
{
  int bytes_read;
  http_req request;
  char* buffer = (char*)malloc(BUF_SIZE + 1);
  if (buffer == NULL)
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

// parse verb, path, headers, and body of the incoming HTTP request and place in the http_req pointed to by req
int parse_http_req(char* buf, size_t buf_len, http_req* req)
{
  // Treating the buffer like as a FILE* let's us use cleaner semantics for reading the request
  FILE* req_fd = fmemopen(buf, buf_len, "r");
  if (req_fd == NULL)
  {
    perror("fmemopen parse_http_req");
    return 1;
  }

  // Get the HTTP start line and parse into req
  if (parse_start_line(req_fd, req) == -1)
  {
    printf("error parsing start line\n");
    return 1;
  }

  if (parse_headers(req_fd, req) == -1)
  {
    printf("error parsing headers\n");
    return 1;
  }

  // All that remains is to read the body
  if (parse_body(req_fd, req) == -1)
  {
    printf("error parsing request body\n");
    return 1;
  }

  printf("> New Request:\n>\t%s %s %s\n", req->verb, req->path, req->version);
  print_headers(req->headers, ">\t");
  printf(">\n");
  printf(">\t%s\n\n", req->body);

  return 0;
}

// print_headers is a utility for easily printing out all headers of a header_list*
void print_headers(header_list* headers, char* prefix)
{
  if (prefix == NULL)
  {
    prefix = "";
  }
  header_list* current = headers;
  while (current != NULL)
  {
    printf("%s%s: %s\n", prefix, current->entry->key, current->entry->value);
    current = current->next;
  }
}
