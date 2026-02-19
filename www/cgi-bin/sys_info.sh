#!/bin/bash
echo -ne "Status: 200 OK\r\n"
echo -ne "Content-Type: text/plain\r\n"
echo -ne "\r\n"
echo "--- System Info (Bash CGI) ---"
echo "Interpreter: /bin/bash"
echo "Timestamp: $(date)"
