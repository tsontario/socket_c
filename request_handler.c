#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
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
    printf("error parsing HTTP request\n");
    close(client_sock);
    return 1;
  }

  int response_code;
  if ((response_code = serve_response(client_sock, &request, &response)) != 0)
  {
    if (response_code == 404)
    {
      serve_404_page(client_sock, &request, &response);
    } else if (response_code >= 400)
    {
      write_http_error(client_sock, response_code);
    } else {
      printf("error in serve response");
    }
    close(client_sock);
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
    return 500;
  }

  // Get the HTTP start line and parse into req
  if (parse_start_line(req_fd, req) == -1)
  {
    printf("error parsing start line\n");
    return 422;
  }

  if (parse_headers(req_fd, req) == -1)
  {
    printf("error parsing headers\n");
    return 422;
  }

  // All that remains is to read the body
  if (parse_body(req_fd, req) == -1)
  {
    printf("error parsing request body\n");
    return 422;
  }

  printf("> REQUEST:\n>\t%s %s %s\n", req->verb, req->path, req->version);
  print_headers(req->headers, ">\t");
  printf(">\n");
  printf(">%s\n\n", req->body);

  return 0;
}

// serve_response serves the request specified by req, using resp 
int serve_response(int client_sock, http_req* req, http_resp* resp)
{
  printf("< RESPONSE:\n");
  char* local_path = (char*)malloc(strlen(WEB_DIR) + strlen(req->path) + 1); // +1 for \0 byte
  strncat(local_path, WEB_DIR, strlen(WEB_DIR));
  strncat(local_path, req->path, strlen(req->path));
  char EMPTY_PATH[2] = "/";
  if ((strncmp(req->path, EMPTY_PATH, 2)) == 0)
  {
    // Per assignment specification, / returns a 404
    return 404;
  }

  // Check path exists
  struct stat st;
  if ((stat(local_path, &st)) == -1)
  {
    // For simplicity we assume any failure is ENOENT and we'll return 404
    perror("error calling stat on request path");
    free(local_path);
    return 404;
  }
  resp->body_fd = fopen(local_path, "r");
  if (resp->body_fd == NULL)
  {
    perror("error opening requested file");
    free(local_path);
    return 500;
  }
  free(local_path);

  // We are ready to send
  // Currently, we only make a Content-Type and Content-Length header for the response.
  // For expedience, I've done this straight inline. A better approach will be to
  // have an interface for adding entries to the header list, and feed it a list of generator functions
  // as needed
  resp->status_code = 200;
  header_list* header = (header_list*)malloc(sizeof(header_list));
  resp->headers = header;
  header->entry = (header_entry*)malloc(sizeof(header_entry));
  header->entry->key = "Content-Type";
  header->entry->value = get_content_type(req->path);
  if ((header->entry->value) == NULL)
  {
    printf("Expected Content-Type but got NULL");
    return 422;
  }
  header = (header_list*)malloc(sizeof(header_list));
  resp->headers->next = header;
  header->entry = (header_entry*)malloc(sizeof(header_entry));
  header->entry->key = "Content-Length";
  header->entry->value = get_content_length(resp->body_fd);

  printf("<\tHTTP/1.1 200 OK\n");
  print_headers(resp->headers, "<\t");

  // Construct response status line and header lines, then write to the client socket
  char* buf = (char*)malloc(sizeof(char)*BUF_SIZE);
  memset(buf, 0, BUF_SIZE);
  if ((sprintf(buf, "HTTP/1.1 200 OK\n%s: %s\n%s: %s\n\n",
    resp->headers->entry->key, resp->headers->entry->value,
    resp->headers->next->entry->key, resp->headers->next->entry->value
  )) < 0)
  {
    perror("error writing response status line and headers");
    return 1;
  }
  if ((write(client_sock, buf, strlen(buf))) == -1)
  {
    perror("writing to client socket");
    return 1;
  }

  // Write the body of the response into the client socket
  ssize_t num_bytes;
  while ((num_bytes = fread(buf, sizeof(char), BUF_SIZE, resp->body_fd)) != 0)
  {
    if (num_bytes == -1){
      perror("error reading response file");
      return 1;
    }
    write(client_sock, buf, num_bytes);
    if (strncmp(get_content_type(req->path), "text/html", strlen("text/html")) == 0)
    {
      printf("%s", buf);
    }
  }
  printf("\n< **END OF MESSAGE**\n");
  return 0;
}

// write_http_error can be called when processing a given request fails
// before beginning to write the response. It simply returns the first
// line of the HTTP response and closes the connection
void write_http_error(int client_socket, int status_code)
{
  char buf[BUF_SIZE];
  sprintf(buf, "HTTP/1.1 %d ERROR", status_code);
  printf("<\t%s\n", buf);
  write(client_socket, buf, strlen(buf));
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
  if ((strncmp(extension, ".html", sizeof(char) * 6)) == 0)
  {
    content_type = "text/html";
  } else if ((strncmp(extension, ".jpg", sizeof(char) * 5)) == 0)
  {
    content_type = "image/jpeg";
  } else if ((strncmp(extension, ".js", sizeof(char) * 4)) == 0)
  {
    content_type = "application/javascript";
  } else if ((strncmp(extension, ".jpeg", sizeof(char) * 6)) == 0)
  {
    content_type = "image/jpeg";
  } else if ((strncmp(extension, ".mp3", sizeof(char) * 5)) == 0)
  {
    content_type = "audio/mpeg";
  } else if ((strncmp(extension, ".mp4", sizeof(char) * 5)) == 0)
  {
    content_type = "video/mp4";
  } else if ((strncmp(extension, ".ico", sizeof(char) * 5)) == 0) {
    content_type = "image/x-icon";
  } else {
    content_type = NULL;
  }
  return content_type;
}

// get_content_length returns the length (in bytes) of file
char* get_content_length(FILE* file)
{
  char* buf = (char*)malloc(sizeof(char)*BUF_SIZE);
  fseek(file, 0, SEEK_END);
  long content_length = ftell(file);
  fseek(file, -content_length, SEEK_END);
  sprintf(buf, "%ld", content_length);
  return buf;
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

void serve_404_page(int client_sock, http_req* request, http_resp* response)
{
  char buf[BUF_SIZE];
  sprintf(buf, "HTTP/1.1 404 NOTFOUND\nContent-Type: text/html\n\n<html><body><h1>404 Not Found</h1></body></html>\n");
  write(client_sock, &buf, strlen(buf));
}
