#!/usr/bin/python3
import time
import sys

# Send headers immediately
print("Content-Type: text/plain\r\n\r\n")
sys.stdout.flush()

print("This script will hang to test the server timeout.")
print("Please wait 30 seconds for the server to kill me...")
sys.stdout.flush()

while True:
    time.sleep(1) # Sleep loop