#pragma once

#define MAX_CONNS 20
#define BUF_SIZE 8096

// forward declare recursive structure
typedef struct header_list header_list;

// header_entry represents a single HTTP header entry
typedef struct {
  char* key;
  char* value;
} header_entry;

// header_list is a linked list for storing headers of an HTTP request
struct header_list {
  header_entry* entry;
  header_list* next;
};

// http_req stores information of an HTTP request
typedef struct {
  char* verb;
  char* path;
  char* version;
  header_list* headers;
  char* body;
} http_req;

// handle_conn is the entry point of a forked server process, dedicated to
// communicating with a specific TCP connection.
int handle_conn(int client_sock);

// parse_http_req parses the HTTP request stored in buffer and stores
// the parsed results in req. A non-zero error code is returned in the
// event of a parse failure.
int parse_http_req(char* buffer, size_t buf_len, http_req* req);

void print_headers(header_list* headers, char* prefix);
