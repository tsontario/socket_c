#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#include "request_handler.h"
#include "parse.h"

/*
  parse.c contains code related to parsing the contents of an HTTP request into an http_req* structure.

  The parsing is done in 3 parts:
  - in parse_start_line, we read the first line of the request and return the verb, path, and HTTP version of the request
  - in parse_headers, we find all HTTP headers and store them as a linked list pointed to by req->headers
  - Finally, we read the body and store it in req->body. If no body exists, a generic stub noting this fact is placed
    in req->body, instead.
*/

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

// parse_headers is the second parse called when parsing an HTTP request.
// It iterates over the HTTP header until an empty line is found.
// Each key:value pair of headers is stored as an entry in a linked list,
// pointed to by req->headers
int parse_headers(FILE* req_fd, http_req* req)
{
  // Setting buf = NULL and line_size = 0 let's getline() allocate buffer space on-the-fly without us needing to alloc ourselves.
  // Note we still must free() line_ptr on each read.
  char* buf = NULL;
  size_t line_size = 0;

  req->headers = (header_list*)malloc(sizeof(header_list));
  if (req->headers == NULL)
  {
    perror("allocating header list memory");
    return 1;
  }

  header_list* current = req->headers;
  header_list* prev = NULL;
  while(1)
  {
    if (getline(&buf, &line_size, req_fd) == -1)
    {
      printf("error reading line from request\n");
      return 1;
    }
    if (strcmp(buf, "\r\n") == 0)
    {
      prev->next = NULL;
      break;
    }

    header_entry* entry = (header_entry*)malloc(sizeof(header_entry));
    entry->key = strdup(strtok(buf, ": "));
    entry->value = strdup(strtok(NULL, "\n"));
    current->entry = entry;

    free(buf);
    buf = NULL;
    current->next = (header_list*)malloc(sizeof(header_list));
    if (current->next == NULL)
    {
      perror("allocating header list memory (next entry)");
      return 1;
    }
    prev = current;
    current = current->next;
  }

  return 0;
}

// parse_body is the final parse method called when parsing an HTTP request
// It reads the rest of req_fd until it either runs out of space (truncating the output),
// or EOF is found. The result is stored in req->body
int parse_body(FILE* req_fd, http_req* req)
{
  // It is likely the body of the request is empty, so we need to account for this case
  // TODO centralize req mallocs and initialization
  req->body = (char*)malloc(sizeof(char) * BUF_SIZE);
  char buf[BUF_SIZE+1];
  bool first_pass = true;
  for(int i=0; i<BUF_SIZE;++i)
  {
    char next = fgetc(req_fd);
    if (feof(req_fd) != 0)
    {
      clearerr(req_fd);
      if (first_pass == true)
      { 
        strcpy(req->body, " <EMPTY REQUEST BODY>");
        return 0;
      } else {
        buf[i] = '\0';
        strncpy(req->body, buf, BUF_SIZE);
        return 0;
      }
    } else if (ferror(req_fd) != 0)
    {
      printf("error reading request body\n");
      clearerr(req_fd);
      return 1;
    }
    first_pass = false;
    buf[i] = next;
  }
  return 0;
}
