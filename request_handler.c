#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>

#include "request_handler.h"
#include "parse.h"

// WEB_DIR is the (relative to project root) directory that contains the files visible to the webserver4
// TODO paths that contain `../` in one form or another should serve a 422 Unprocessable Entity error.
char* WEB_DIR = "web";

/* 
  Handle_conn handles the incoming connection represented by client_sock
  It expects the incoming stream to be structured as an HTTP request, erroring out
  if this assumption is violated
*/
int handle_conn(int client_sock)
{
  int bytes_read;
  http_req request;
  http_resp response;
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
  if ((parse_http_req(buffer, bytes_read, &request)) == -1)
  {
    printf("Something bad happened\n");
    return 1;
  }

  if ((serve_request(client_sock, &request, &response)) == -1)
  {
    // TODO serve 500
    return 1;
  }
  
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
  printf(">\n%s\n\n", req->body);

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

int serve_request(int client_sock, http_req* req, http_resp* resp)
{
  char* local_path = (char*)malloc(strlen(WEB_DIR) + strlen(req->path) + 1); // +1 for \0 byte
  local_path = strncat(WEB_DIR, req->path, strlen(req->path));
  // Check path exists
  struct stat st;
  if ((stat(local_path, &st)) == -1)
  {
    // For simplicity we assume any failure is ENOENT and we'll return 404
    perror("error calling stat on request path");
    free(local_path);
    // set 404 resp
    return 1;
  }
  resp->body_fd = fopen(local_path, "r");
  if (resp->body_fd == NULL)
  {
    perror("error opening requested file");
    free(local_path);
    // set 500 resp
    return 1;
  }
  free(local_path);
  // We are ready to send
  resp->status_code = 200;
  resp->headers = (header_list*)malloc(sizeof(header_list));
  resp->headers->entry = (header_entry*)malloc(sizeof(header_entry));
  resp->headers->entry->key = "Content-Type";
  resp->headers->entry->value = get_content_type(req->path);
  if ((resp->headers->entry->value) == NULL)
  {
    // Bad request
    return 1;
  }
  char* buf[BUF_SIZE];
  
  FILE* resp_body_buf = fmemopen(buf, BUF_SIZE, "w+");
  if (resp_body_buf == NULL)
  {
    perror("fmemopen for response body buffer");
  }
  dprintf(client_sock, "HTTP/1.1 200 OK\n%s: %s\n\n", resp->headers->entry->key, resp->headers->entry->value);  // Slight hack: we're only using content-type header for now
  ssize_t num_bytes;
  while ((num_bytes == read(resp->body_fd->_fileno, &buf, sizeof(buf))) != 0)
  {
    write(client_sock, &buf, num_bytes);
  }
  return 0;
}

// Callers are responsible for freeing the returned char* returned by get_content_type
char* get_content_type(char* path)
{
  char* content_type = (char*)malloc(sizeof(char) * 256); // We don't really expect the header to be this large 
  const char* extension = strrchr(path, '.');
  if (!extension)
  {
    return NULL;
  }
  if ((strncmp(extension, "html", sizeof(char) * 4)) == 0)
  {
    content_type = "text/html";
  } else if ((strncmp(extension, "jpg", sizeof(char) * 3))== 0)
  {
    content_type = "image/jpeg";
  } else if ((strncmp(extension, "jpeg", sizeof(char) * 4))== 0)
  {
    content_type = "image/jpeg";
  } else if ((strncmp(extension, "mp3", sizeof(char) * 3))== 0)
  {
    content_type = "audio/mpeg";
  } else if ((strncmp(extension, "mp4", sizeof(char) * 3))== 0)
  {
    content_type = "video/mp4";
  } else {
    content_type = NULL;
  }
  return content_type;
}