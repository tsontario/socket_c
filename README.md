- [Instructions for users](#instructions-for-users)
  - [Building](#building)
  - [Running](#running)
- [Release notes](#release-notes)
  - [Supported MIME types](#supported-mime-types)
- [Design](#design)
  - [Basic design](#basic-design)
  - [Error handling](#error-handling)
  - [Work outstanding](#work-outstanding)
  - [Assumptions](#assumptions)

This repo implements a basic HTTP web server using TCP sockets for the transport layer.

# Instructions for users

## Building

- Linux: run `make` (requires `build-essentials`)
- OSX: run `make` (requires `Xcode`)
- Windows: _this build has not been tested on a windows machine_
- Docker: from project root, run `scripts/build_container.sh` to build, and `scripts/run_container.sh` to run. Port-forwarding is set to `8989:8989`

## Running

Basic usage: `myserver [-p PORT]`

By default, `myServer` binds to all available network interfaces, and listens on port `8989`. The port number can be configured by passing in `-p` followed by a valid port number.

In addition, it is crucial that there is a directory, with path relative to `./myServer`, called `web/`, which is where `myServer` will look for server files (a future refinement would be to make this configurable).

If running `myServer` locally, you can access files via your browser (or `curl`) as follows: `http://localhost:8989/<FILE>`.

# Release notes

## Supported MIME types

Currently supported file formats/MIME types:

- `.html -> text/html`
- `.js -> application/javascript`
- `.mp3 -> audio/mpeg`
- `.jpg, .jpeg -> image/jpeg`
- `.mp4 -> video/mp4`
- `.ico -> image/x-icon`

# Design

## Basic design

The design of the server implements a forking model: a main process receives incoming events, and a new, forked, subprocess is spawned to handle each individual connection. If requests are queued beyond a certain number, they are simply dropped. Once a new connection is established, the forked process services the request by first parsing the request, generating the response, and finally writing output back to the client.

## Error handling

At any moment between establishing a connection and immediately before writing the response headers, meaningful errors are able to be sent back to the client. E.g `404` for non-existent files, `500` for internal processing errors. Note that it's impossible to send a semantically meangingful error either prior to establishing the connection and after already writing the response header to the client.

## Work outstanding

As code is forever a work in progress, the following list describes currently known deficiencies:

- `http_req` and `http_resp` structs are not properly destroyed. Since each connection is forked (and we do not use Keep-Alive connections), the risk of `ENOMEN` due to memory leaks is quite low. Regardless, it would be preferable to better-handle the lifecycle of these objects.
- Similar to the above, `char*` with `malloc` is used in numerous places where a stack-local buffer would be preferable.
- Headers (for both requests and responses) are modeled as linked lists. However, the implementation lacks useful helper methods to simplify their use. For example, constructing the response headers is done 'manually' at the moment is absolutely not a nice way to do that.
- While an attempt has been made to have robust error handling, there are still some areas that lack requisite checks. Note that the most critical operations are guarded, but there is still more work to be done here.
- Log output has a primitive form: migrating to structured (e.g. JSON) logging would be a vast improvement. In addition, all response bodies are logged, even if they are non-textual data.

## Assumptions

- Entities (e.g. client/server) communicate using HTTP/1.1 according to the conventions outlined in [RFC 7230](https://tools.ietf.org/html/rfc7230).
- The size of a request's body are no greater than 8096 bytes: excess data is simply dropped. Note this refers _only_ to the body and not the initial request-line nor headers list of the request
- TCP connections are not reused and are closed after each, atomic, HTTP request. A further improvement could be to handle TCP reuse, but that has not been implemented yet.
