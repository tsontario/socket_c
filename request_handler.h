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

typedef struct {
  char* protocol_version;
  int status_code;
  char* status_text;
  header_list* headers;
  char* body;
  FILE* body_fd;
} http_resp;

// handle_conn is the entry point of a forked server process, dedicated to
// communicating with a specific TCP connection.
int handle_conn(int client_sock);

// parse_http_req parses the HTTP request stored in buffer and stores
// the parsed results in req. A non-zero error code is returned in the
// event of a parse failure.
int parse_http_req(char* buffer, size_t buf_len, http_req* req);

// serve_response attempts to create a valid HTTP response for the request
// encapsulated in req. The return value is the HTTP status code to be used in the response
int serve_response(int client_sock, http_req* req, http_resp* resp);

// write_http_error can be called when processing a given request fails
// before beginning to write the response. It simply returns the first
// line of the HTTP response and closes the connection
void write_http_error(int client_sock, int status_code);

void print_headers(header_list* headers, char* prefix);

char* get_content_type(char* path);

char* get_content_length(FILE* file);