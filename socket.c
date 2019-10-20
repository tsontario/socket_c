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

#include "socket.h"

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

  req->headers = (header_list*)malloc(sizeof(header_list));
  if (errno != 0)
  {
    perror("allocating header list memory");
    return 1;
  }

  header_list* current = req->headers;
  while(1)
  {
    if (getline(&line_ptr, &line_size, req_fd) == -1)
    {
      printf("error reading line from request\n");
      return 1;
    }
    if (strcmp(line_ptr, "\r\n") == 0)
    {
      current = NULL;
      break;
    }

    header_entry* entry = (header_entry*)malloc(sizeof(header_entry));
    entry->key = strdup(strtok(line_ptr, ": "));
    entry->value = strdup(strtok(NULL, "\n"));
    current->entry = entry;

    printf("Header key: %s\n", current->entry->key);
    free(line_ptr);
    line_ptr = NULL;
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
int parse_http_req(char* buffer, size_t buf_len, http_req* req)
{
  // Setting line_ptr = NULL and line_size = 0 let's getline() allocate buffer space on-the-fly without us needing to alloc ourselves.
  // Note we still must free() line_ptr on each read.
  char* line_ptr = NULL;
  size_t line_size = 0;

  // Treating the buffer like a file let's us use simple functions for reading lines
  FILE* req_fd = fmemopen(buffer, buf_len, "r");

  printf("Before start line\n");
  // Get the HTTP start line and parse into req
  if (parse_start_line(req_fd, req) == -1)
  {
    printf("error parsing start line\n");
    return 1;
  }

  printf("Processing HTTP request...\nVERB: %s\nPATH: %s\nVERSION: %s\n", req->verb, req->path, req->version);
  printf("Parsing headers...\n");
  if (parse_headers(req_fd, req) == -1)
  {
    printf("error parsing headers\n");
    return 1;
  }
  printf("Done parsing headers\n");
  header_list* headers = req->headers;
  while (headers != NULL)
  {
    printf("Key: %s; Value: %s\n", headers->entry->key, headers->entry->value);
    headers = headers->next;
  }
  return 0;
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
