#!/usr/bin/python3
import os, sys, http.cookies

def main():
	try:
		# Session Logic
		cookie = http.cookies.SimpleCookie(os.environ.get("HTTP_COOKIE", ""))
		visits = int(cookie['v'].value) + 1 if 'v' in cookie else 1
		
		# Body Logic
		method = os.environ.get("REQUEST_METHOD", "GET")
		try:
			cl = int(os.environ.get("CONTENT_LENGTH", 0))
			body = sys.stdin.read(cl) if cl > 0 else ""
		except:
			body = ""

		# Headers
		sys.stdout.write(f"Set-Cookie: v={visits}; Path=/; Max-Age=3600\r\n")
		sys.stdout.write("Content-Type: text/plain\r\n")
		sys.stdout.write("Status: 200 OK\r\n")
		sys.stdout.write("\r\n")

		# Output
		sys.stdout.write(f"Method: {method}\n")
		sys.stdout.write(f"Visits: {visits}\n")
		sys.stdout.write(f"Body: {body if body else 'Empty'}\n")
		sys.stdout.flush()
	except Exception as e:
		sys.stdout.write("Status: 500\r\n\r\n" + str(e))

if __name__ == "__main__":
	main()
