
#include "FileRequestHandler.hpp"
#include "IRequestManager.hpp"
#include "RequestHeader.hpp"
#include "Config.hpp"
#include "CallbackQueue.hpp"
#include <cstddef>
#include <span>
#include <ios>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

FileRequestHandler::FileRequestHandler(IRequestManager& manager, const RequestHeader& header, const RouteConfig& route)
    : manager_(manager), header_(header), route_(route) {
    filePath_ = route_.root + header_.path();
    // Queue callback to do file serving work from main loop
    // This avoids deep call stacks and allows proper async handling
    CallbackQueue::queueCallback([this]() { sendFile(); });
}

FileRequestHandler::~FileRequestHandler() {
    if (inFile_.is_open()) inFile_.close();
}

void FileRequestHandler::onBodyData(std::span<const char> /*data*/) {
    // Ignore body data for GET
}

void FileRequestHandler::notifyResponseBuffer(size_t /*bufferSize*/) {
    // No buffering logic for file serving
}

void FileRequestHandler::sendErrorResponse(int code, const std::string& message) {
    std::string statusText = (code == 200) ? "OK" : "Error"; // Simplified for brevity
    std::string response =
        "HTTP/1.1 " + std::to_string(code) + " " + statusText + "\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: " + std::to_string(message.size()) + "\r\n"
        "Connection: close\r\n\r\n" + message; // Body
    manager_.writeResponseData(response); // Send the response
    manager_.onRequestDone(); // Notify manager that request is done
}

/*
    - Uses a std::stringstream to format the chunk size in hexadecimal, which is the correct and portable way to do it in C++.
    - The file is read and sent in 8KB chunks, each chunk is properly formatted for HTTP chunked transfer encoding.
    - The final zero-length chunk is sent to signal the end of the response.
*/
void FileRequestHandler::sendFile() {
    inFile_.open(filePath_, std::ios::binary); // Open file in binary mode
    if (!inFile_) {
        sendErrorResponse(404, "File not found"); // File could not be opened
        return;
    }

    // Use chunked transfer encoding for streaming
    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Connection: close\r\n\r\n";
    manager_.writeResponseData(std::span<const char>(response.data(), response.size())); // Send headers

    constexpr size_t chunkSize = 8192; // 8KB
    std::vector<char> buffer(chunkSize); // Buffer for file chunks
    while (inFile_) {
        inFile_.read(buffer.data(), chunkSize); // Read a chunk
        std::streamsize bytesRead = inFile_.gcount();// Get number of bytes read
        if (bytesRead <= 0) break; // End of file or read error
        // Write chunk size in hex followed by CRLF
        std::stringstream ss;   // String stream for formatting
        ss << std::hex << bytesRead << "\r\n"; // Chunk size line
        std::string chunkHeader = ss.str(); // Convert to string
        manager_.writeResponseData(std::span<const char>(chunkHeader.data(), chunkHeader.size())); // Send chunk size
        // Write chunk data
        manager_.writeResponseData(std::span<const char>(buffer.data(), bytesRead)); // Send chunk data
        // Write CRLF after chunk
        std::string crlf = "\r\n"; // CRLF after chunk data
        manager_.writeResponseData(std::span<const char>(crlf.data(), crlf.size())); // Send CRLF
    }
    // Write final zero-length chunk
    std::string lastChunk = "0\r\n\r\n"; // Last chunk indicating end of response
    manager_.writeResponseData(std::span<const char>(lastChunk.data(), lastChunk.size())); // Send last chunk
    manager_.onRequestDone(); // Notify manager that request is done
}

