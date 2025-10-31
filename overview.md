# Webserv

A simple(?) single-process, single-threaded HTTP 1.1 server in C++ using non-blocking I/O.

This document outlines the core requirements and working principles.

## Non-blocking I/O

Outside of reading the configuration file, all I/O must be non-blocking so that the server doesn't freeze waiting for a single client or single CGI child process but instead continues serving other clients.
This means that all code paths that perform I/O must be structured in a way that allows returning to a "main loop" or similar and resuming later.

Simply attempting to perform I/O on each possible file descriptor and seeing whether it succeeds would be rather inefficient and is in fact disallowed in the project assignment.
Instead we must use a kernel interface like `poll(2)` to query which file descriptors might be able to make progress, and to block/sleep if none of them are.

When transferring data between two potentially blocking file descriptors, just one of them not being ready might block any further progress on that transfer.
If the buffer is full the server has no room for reading more data from the source until it writes some of the buffered data to the destination.
If the buffer is empty the server has nothing to write to the destination until it reads more data from the source.
In these cases the other file descriptor should be removed from the poll set as it can't be worked on anyway until buffer has room or data again, and any attempt would waste CPU time in checking the situation instead of serving other clients or sleeping.

## Accepting connections

Server must support listening on multiple ports if so configured.
It creates a socket for each with `socket(2)`, binds it to the specified port with `bind(2)`, and sets it to listening mode with `listen(2)`.
Binding to specific IP addresses could also be implemented, but is not required by the project assignment.

When a client connects to the listening socket, `accept(2)` returns a socket file descriptor for this particular connection.
To comply with the non-blocking requirement, the listening socket FD must be put into the poll set and `accept(2)` called only when the listening socket is "readable".

## Parsing requests

For each connection that has been accepted, we must `read(2)` or `recv(2)` incoming data from it until the entire header section of a HTTP request has been received, and parse the text-form HTTP 1.1 header into a data structure that can be consulted by further processing.

We likely want to separate socket handling from parsing, but if we do so, the parsing logic needs to indicate to the socket handling logic whether the header has been completed.
Any leftover data following the end of the header must be retained by the connection logic, as it might contain the start of a request body or the start of a next request using the same connection.

To comply with the non-blocking requirement, the connection FD must be put into the poll set and `read(2)` or `recv(2)` called only when the socket is readable.
Short reads must be handled correctly.
The parsing logic must either buffer the incomplete header data or leave it with the socket handling logic until the end of the header section is found.
Partially parsing the imcomplete data is possible but not necessary.

Parallel processing of pipelined requests (RFC 9112 section 9.3.2) is optional in the standard and not mentioned in the project assignment, so to keep implementation simple we can handle one request at a time (per connection) and only start parsing the next one after the current request has been completely processed and responded to.

## Handling requests

Based on the parsed header data and the server configuration, we must decide how to handle to each request:

* Serve the contents of a file from the filesystem.
* Execute a CGI program, forward the request to it, and forward its output to the client as the response.
* Accept the request body as a file upload and store it in the filesystem.
  * TODO: can we implement this with a CGI program, or does it need to be a feature of the server itself?
* Respond with an error page.

At minimum, we must consider the request path, the request method, and the `Host` header (if present, to allow selecting one of configured virtual hosts).

If a request specifies `Transfer-Encoding: chunked`, the server must decode the request body before passing it to CGI program or storing it as an uploaded file.
Support for other transfer encodings is not necessary.

In all cases the server must determine appropriate response headers and `write(2)` or `send(2)` them into the connection, followed by the response body.
To comply with the non-blocking requirement, the connection FD must be put into the poll set and `write(2)` or `send(2)` called only when the socket is writable.
Short writes must be handled correctly.

## File requests

The server translates the URL path to a filesystem path as specified in the configuration, reads the file, and sends its content as the response body.
Length of the file should be queried from the file system and sent as the `Content-Length` header.
The server should also deduce an appropriate `Content-Type` header, perhaps based on the file extension.

File reading can be done with the same non-blocking logic as socket transfers, but at least on Linux regular files are always considered ready even if operating on it would block briefly for hardware access.
The project assignment is somewhat contradictory on this - on one hand it says poll or equivalent must be used for **client-server communication**, on the other hand it says **any read or write** without going through poll or equivalent is forbidden.

The entire file does not need to be read into memory at once - we should limit the amount of buffered data to prevent large files from bloating server memory usage.

## CGI programs

Server configuration determines how CGI requests are identified.
Project assignment requires file extension based detection.
Other mechanisms could be implemented but are not required.
For file extension based detection, we can probably assume the files with the specified extension(s) are scripts, and require the configuration to specify an interpreter program for each CGI file extension.

To execute a CGI program, the server must set up `pipe(2)` for the program's standard input and output, `fork(2)` a child process, prepare environment variables and arguments based on the request as specified in the CGI specification, and `execve(2)` the configured program.
The server must avoid exposing any unnecessary file descriptors (e.g. sockets) to the executed program.

The server must write the request body (if any) into the standard input pipe of the CGI program, and read the response from the standard output pipe of the CGI program.
To comply with the non-blocking requirement, the pipe FDs must be put into the poll set and `read(2)` or `write(2)` called only when the pipe is readable or writable.
Short reads and writes must be handled correctly.

A CGI program provides response headers at the start of its output.
CGI response headers are similar but slightly different from HTTP response headers, so the server must parse them and perform some additional processing before sending the response headers to the client.

The server may either buffer the entire response buffer in memory to be able to specify a `Content-Length` header, or use `chunked` transfer encoding to forward output as it arrives to prevent large responses from bloating server memory usage.
Using neither and instead closing the connection at the end of the response body is allowed by HTTP standard but not recommended as it is indistinguishable from response interrupted by a failure.

Temporary files could be used instead of pipes for the CGI program's standard input and output, which would allow buffering large bodies on filesystem instead of server memory, but managing temporary files adds extra complexity.

## File uploads

TODO: can we implement this with a CGI program, or does it need to be a feature of the server itself?
