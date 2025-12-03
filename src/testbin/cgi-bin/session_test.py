#!/usr/bin/python3
import os
import http.cookies

# 1. Read existing cookies from the Environment Variable
cookie_string = os.environ.get("HTTP_COOKIE")
query_string = os.environ.get("QUERY_STRING", "")

c = http.cookies.SimpleCookie()

if cookie_string:
    c.load(cookie_string)

# 2. Get the current counter (default to 0 if new visitor)
try:
    counter = int(c['visit_count'].value)
except (KeyError, ValueError):
    counter = 0

# 3. Increment
counter += 1

# 4. Prepare the New Cookie header
c['visit_count'] = counter
c['visit_count']['path'] = '/'
c['visit_count']['max-age'] = 3600

# ----------------------------------
# OUTPUT SECTION (Header + Body)
# ----------------------------------

# Send the Set-Cookie header
print(c.output()) 
print("Content-Type: text/html")
print("Status: 200 OK")
print("") # End of Headers

print("<html><body>")
print("<h1>CGI Session & Query Test</h1>")
print(f"<h2>You have visited this page <b>{counter}</b> times.</h2>")
print("<p>Refresh to verify session logic. Add ?foo=bar to URL to verify Query String logic.</p>")

print("<h3>Debug Info:</h3>")
print("<ul>")
print(f"<li><b>Raw Cookie Header:</b> {cookie_string}</li>")
print(f"<li><b>Query String:</b> {query_string}</li>")
print("</ul>")
print("</body></html>")
