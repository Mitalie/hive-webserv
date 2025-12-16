#!/usr/bin/bash

# Read body based on Content-Length
if [ "$CONTENT_LENGTH" ]; then
    read -n "$CONTENT_LENGTH" POST_DATA
fi

echo "Content-Type: text/plain"
echo "Status: 200 OK"
echo "" 

echo "--- CGI Bash Test ---"
echo "Interpreter: Bash"
echo "Body: $POST_DATA"
