#include "UploadRequestHandler.hpp"
#include "IRequestManager.hpp"
#include "RequestHeader.hpp"
#include "Config.hpp"
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <string>
#include <span>
#include <system_error>

UploadRequestHandler::UploadRequestHandler(IRequestManager& manager, const RequestHeader& header, const RouteConfig& route)
    : manager_(manager), header_(header), route_(route), done_(false), fileOpen_(false) {
    // 1. Ensure upload directory exists
    std::error_code ec;
    if (!std::filesystem::exists(route_.uploadStore, ec)) {
        if (!std::filesystem::create_directories(route_.uploadStore, ec)) {
            sendResponse(500, "Upload directory does not exist and could not be created");
            manager_.onRequestDone();
            done_ = true;
            return;
        }
    }

    // 2. Sanitize filename (no directory traversal, no empty, no special chars)
    std::string filename = std::filesystem::path(header_.path()).filename().string();
    if (filename.empty() || filename == "." || filename == "..") filename = "upload.bin";
    // Remove dangerous characters (very basic)
    for (char& c : filename) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
            c = '_';
    }

    // 3. Unique filename logic (document.txt, document(1).txt, ...)
    std::string base = filename;
    std::string ext;
    size_t dot = filename.find_last_of('.');
    if (dot != std::string::npos && dot != 0) {
        base = filename.substr(0, dot);
        ext = filename.substr(dot);
    }
    targetPath_ = route_.uploadStore + "/" + filename;
    int count = 1;
    while (std::filesystem::exists(targetPath_)) {
        targetPath_ = route_.uploadStore + "/" + base + "(" + std::to_string(count++) + ")" + ext;
    }

    // 4. MIME type check (basic, for multipart/form-data)
    std::string contentType = header_.get("Content-Type");
    if (contentType.find("multipart/form-data") == std::string::npos) {
        sendResponse(415, "Unsupported Media Type: Only multipart/form-data supported");
        manager_.onRequestDone();
        done_ = true;
        return;
    }

    // 5. Parse boundary from Content-Type
    size_t pos = contentType.find("boundary=");
    if (pos == std::string::npos) {
        sendResponse(400, "Malformed multipart/form-data: missing boundary");
        manager_.onRequestDone();
        done_ = true;
        return;
    }
    boundary_ = "--" + contentType.substr(pos + 9);
}

UploadRequestHandler::~UploadRequestHandler() {
    if (outFile_.is_open()) outFile_.close();
}

void UploadRequestHandler::onBodyData(std::span<const char> data) {
    if (done_) return;

    // Buffer to accumulate multipart data (instance member, not static)
    multipartBuffer_.append(data.data(), data.size());

    // If not open, look for start of file part
    if (!fileOpen_) {
        // Find the double CRLF after the part headers
        size_t headerEnd = multipartBuffer_.find("\r\n\r\n");
        if (headerEnd == std::string::npos)
            return; // Wait for more data
        size_t fileDataStart = headerEnd + 4;
        outFile_.open(targetPath_, std::ios::binary | std::ios::out);
        if (!outFile_) {
            sendResponse(500, "Failed to open file for upload");
            manager_.onRequestDone();
            done_ = true;
            return;
        }
        fileOpen_ = true;
        // Write any file data already received
        size_t boundaryPos = multipartBuffer_.find(boundary_, fileDataStart);
        size_t fileDataEnd = (boundaryPos != std::string::npos) ? boundaryPos - 2 : std::string::npos; // -2 for \r\n before boundary
        if (fileDataEnd != std::string::npos)
            outFile_.write(multipartBuffer_.data() + fileDataStart, fileDataEnd - fileDataStart);
        else
            outFile_.write(multipartBuffer_.data() + fileDataStart, multipartBuffer_.size() - fileDataStart);
        if (!outFile_) {
            sendResponse(500, "Write error during upload");
            manager_.onRequestDone();
            done_ = true;
            return;
        }
        // If boundary found, upload is done
        if (boundaryPos != std::string::npos) {
            outFile_.close();
            sendResponse(201, "File uploaded successfully");
            manager_.onRequestDone();
            done_ = true;
            return;
        }
        // Remove processed header portion, keep remaining data
        multipartBuffer_.erase(0, fileDataStart);
    }
    else {
        // File is open, look for boundary (end of file data)
        size_t boundaryPos = multipartBuffer_.find(boundary_);
        if (boundaryPos != std::string::npos) {
            // Ensure there are at least two bytes (\r\n) before the boundary
            if (boundaryPos < 2) {
                // Malformed multipart data: not enough data before boundary
                if (outFile_.is_open()) {
                    outFile_.close();
                }
                sendResponse(400, "Malformed multipart data");
                manager_.onRequestDone();
                done_ = true;
                return;
            }
            // Write up to boundary (excluding trailing \r\n)
            size_t fileDataEnd = boundaryPos - 2;
            outFile_.write(multipartBuffer_.data(), fileDataEnd);
            outFile_.close();
            sendResponse(201, "File uploaded successfully");
            manager_.onRequestDone();
            done_ = true;
            return;
        }
        else {
            // Keep potential partial boundary at end (boundary_.size() bytes)
            size_t safeWrite = multipartBuffer_.size() > boundary_.size() 
                ? multipartBuffer_.size() - boundary_.size() 
                : 0;
            if (safeWrite > 0) {
                outFile_.write(multipartBuffer_.data(), safeWrite);
                multipartBuffer_.erase(0, safeWrite);
            }
        }
        if (!outFile_) {
            sendResponse(500, "Write error during upload");
            manager_.onRequestDone();
            done_ = true;
            return;
        }
    }
}

void UploadRequestHandler::notifyResponseBuffer(size_t /*bufferSize*/) {
    // No buffering logic for upload
}

void UploadRequestHandler::sendResponse(int code, const std::string& message) {
    std::string statusText;
    switch (code) {
    case 201: statusText = "Created"; break;
    case 400: statusText = "Bad Request"; break;
    case 415: statusText = "Unsupported Media Type"; break;
    default: statusText = "Internal Server Error"; break;
    }
    std::string response =
        "HTTP/1.1 " + std::to_string(code) + " " + statusText + "\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: " + std::to_string(message.size()) + "\r\n"
        "Connection: close\r\n\r\n" + message;
    manager_.writeResponseData(response);
}
