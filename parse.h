#pragma once
#include "request_handler.h"

// parse_start_line is a helper function for parsing the first line
// of an HTTP request, which contains the verb, path, and HTTP version.
// The parsed entries are stored in http_req.
int parse_start_line(FILE*, http_req*);

// parse_headers is a helper function for parsing the header entries of
// an HTTP request. The parsed entries are stored in req->headers
int parse_headers(FILE*, http_req*);

int parse_body(FILE*, http_req*);
