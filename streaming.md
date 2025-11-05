# Asynchronous streaming

Buffering entire large request or response bodies would cause problematic memory usage when serving a large number of clients in parallel.
Therefore, a large request body for CGI or file upload should be streamed from the client connection into the file or the CGI input pipe,
and a large response body should be streamed from the file or the CGI output pipe into the client connection.

Alone, either a request body or a response body could be streamed by alternating between read and write phases to focus on only one file descriptor at a time.
However, a CGI program might start to output a response before reading the entire request body, and block when the output pipe buffer is full.
The CGI request handler can't wait only on write to CGI input, because the program might block on output, but also can't wait only on read from CGI output, because the program might block on input.
The request handler must poll for both of these file descriptors to ensure it can make progress in either case.

When the request handler is notified of file descriptor readiness, it can't blindly attempt to work on both input and output flow.
The project assignment requires that every I/O is preceded by readiness notification from poll or similar.
Therefore the request handler must know which file descriptor is ready whenever it is woken up to do some work.

There are two ways this could be handled.
In a parameterized approach, the code that calls the request handler's resume function must pass some parameters specifying what to do.
In a callback approach, request handler specifies different resume functions to be called when different file descriptors become ready.
I believe the callback approach will feel more intuitive and lead to cleaner code.
With the parameterized approach, the request handler either needs to inform poll wrapper or file descriptor wrapper of the parameters it wants beforehand,
or needs to map generic parameters such as file descriptors or wrapper references back to their logical meaning internal to the request handler.
Then it branches on the logical value and ends up performing same actions as it would've done directly with the callback approach.

## Callback-based readable and writable interfaces

To avoid making entire codebase work with Unix file descriptors and system calls, we wrap sockets, pipes, and files into wrapper classes.
These wrapper classes register callbacks with the polling mechanism, and any higher level logic works through these wrapper classes and register callbacks with them.
The wrapper classes implement a consistent interface regardless of their type. Filter classes could also implement this interface, e.g. for chunked transfer encoding.

```c++
class IReadable
{
	typedef std::function<void(std::string data)> ReadableCallback;
	void read(ReadableCallback callback);
	void stopReading();
	void pauseReading();
	void resumeReading();
};

class IWritable
{
	typedef std::function<std::string()> WritableCallback;
	void write(WritableCallback callback);
	void stopWriting();
	void pauseWriting();
	void resumeWriting();
};
```

The pause functions are used to indicate that the caller can't handle more incoming data or produce more outgoing data right now.
The callback should not be called after pausing, and the underlying file descriptor should not be polled as we won't do anything with it anyway.
The resume functions undo the effect of pausing and enable polling again.
Implementations of the interface may and in many cases need to buffer data.
They can continue working on the buffer while paused as long as the callback is not called and no data is lost.

## Continuous data stream into discrete requests

A single client connection can receive multiple requests before it is closed.
Some clients even queue additional requests before receiving response for the first one.
The boundary between one request and the next is not known until the first request is at least partially parsed, so we don't know how many bytes the request parsing / handling logic should be allowed to read.
Therefore we will have a Client class that reads incoming data, buffers it until it detects a boundary, and retains excess data for the next request.
It first detects the end-of-headers boundary, then parses the entire header block, and then consults the parsed header data to know how much body data belongs to current request.
Client class passes the parsed header to RequestHandler, and acts as an IReadable filter to prevent the handler from consuming data past its intended request body.

## Request handling I/O

RequestHandler comes in various implementations, depending on the type of the request.
All implementations perform the necessary setup and prepare the response headers,
and then simply stream the request body (if applicable) from client IReadable to upload or CGI IWritable,
and the response body from file IReadable, CGI IReadable, or server-generated in-memory response to client IWritable.
The streaming code can likely be shared between the different request types.

# Errors and edge cases

The above description doesn't describe what to do in case there are errors or a stream closes unexpectedly.
These situations will need to be handled properly, and may require additional callbacks.
Most errors will trigger an response, which is sent to client just like any other response if sending response to client is still possible.
However in case of broken output the related input should be either discarded or closed, and in case of broken input the related output needs to be cut short.
If the client connection is in bad state in either direction, the entire connection needs to be closed without processing any additional requests on it.
Any temporary or incomplete data needs to be cleaned up.
