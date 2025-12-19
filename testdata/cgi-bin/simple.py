#!/usr/bin/env python3
import sys

# Script sends ONLY Content-Type.
# The Server MUST inject "HTTP/1.1 200 OK" for this to be valid.
sys.stdout.write("Content-Type: text/plain\r\n\r\n")
sys.stdout.write("Default Status Test")
