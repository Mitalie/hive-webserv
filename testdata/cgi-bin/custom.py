#!/usr/bin/env python3
import sys

# Script sends a custom Status.
# The Server MUST output "HTTP/1.1 418 I'm a teapot".
sys.stdout.write("Status: 418 I'm a teapot\r\n")
sys.stdout.write("Content-Type: text/plain\r\n\r\n")
sys.stdout.write("Short and stout")
