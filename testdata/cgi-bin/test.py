#!/usr/bin/env python3
#
# A simple CGI script for our webserv test harness.
# It reads environment variables and stdin and prints them as HTML.
#
import os
import sys

# --- 1. Send HTTP Headers ---
# This is mandatory! The blank line separates headers from body.
print("Content-Type: text/html")
print("Status: 200 OK") # Our parser checks for this!
print("\r\n") # The \r\n\r\n separator

# --- 2. Send The HTML Body ---
print("<html>")
print("	<head>")
print("		<title>Webserv CGI Test</title>")
print("		<style>body { font-family: sans-serif; }</style>")
print("	</head>")
print("	<body>")
print("		<h1>Hello from CgiHandler!</h1>")
print("		<hr>")

# --- 3. Access Environment Variables (Set by CgiHandler) ---
print("		<h2>Environment Variables:</h2>")
print("		<pre>")
for key, value in os.environ.items():
	# Only print ones we care about for the test
	if key.startswith("HTTP_") or key.startswith("REQUEST_") or \
	   key.startswith("CONTENT_") or key.startswith("SCRIPT_") or \
	   key.startswith("QUERY_"):
		print(f"<strong>{key}:</strong> {value}")
print("		</pre>")

# --- 4. Access stdin (Set by CgiHandler for POST) ---
print("		<h2>POST Data (from stdin):</h2>")
print("		<pre>")
try:
	# Read the Content-Length number of bytes from stdin
	content_length = int(os.environ.get("CONTENT_LENGTH", 0))
	if content_length > 0:
		post_data = sys.stdin.read(content_length)
		print(f"Read {content_length} bytes: {post_data}")
	else:
		print("No POST data (Content-Length was 0 or missing).")
except Exception as e:
	print(f"Error reading stdin: {e}")
print("		</pre>")

print("	</body>")
print("</html>")
