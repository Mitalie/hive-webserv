#!/bin/bash

SERVER="http://localhost:8080"
GREEN='\033[0;32m'
CYAN='\033[0;36m'
RED='\033[0;31m'
NC='\033[0m'

clear
echo -e "${CYAN}==================================================${NC}"
echo -e "${CYAN}      WEBSERV TECHNICAL REQUIREMENTS CHECK        ${NC}"
echo -e "${CYAN}==================================================${NC}"

echo -e "\n${CYAN}[1] METHODS (GET/POST/DELETE/UNKNOWN)${NC}"

CODE_GET=$(curl -s -o /dev/null -w "%{http_code}" "$SERVER/index.html")
echo -e "GET:			$([[ "$CODE_GET" == "200" ]] && echo -ne "${GREEN}PASS${NC}" || echo -ne "${RED}FAIL ($CODE_GET)${NC}")	> Requests static file via GET; 200 OK proves the server retrieves and serves basic content."

POST_RESP=$(curl -s -X POST -d "data" "$SERVER/cgi-bin/eval_demo.py" | grep "POST")
echo -e "POST:			$([[ ! -z "$POST_RESP" ]] && echo -ne "${GREEN}PASS${NC}" || echo -ne "${RED}FAIL${NC}")	> Submits data to CGI via POST; script output reflection proves the server passes request bodies to the handler."

echo "test" > www/uploads/delete_me.txt
CODE_DEL=$(curl -s -o /dev/null -w "%{http_code}" -X DELETE "$SERVER/upload/delete_me.txt")
echo -e "DELETE:			$([[ "$CODE_DEL" == "200" || "$CODE_DEL" == "204" ]] && echo -ne "${GREEN}PASS${NC}" || echo -ne "${RED}FAIL ($CODE_DEL)${NC}")	> Issues DELETE on existing file; success proves the server can execute file system removal operations."

UNKNOWN_RESP=$(printf "GIBBERISH / HTTP/1.1\r\nHost: localhost\r\n\r\n" | nc localhost 8080 | head -n 1 | grep "400\|501\|405")
echo -e "UNKNOWN:		$([[ ! -z "$UNKNOWN_RESP" ]] && echo -ne "${GREEN}PASS${NC}" || echo -ne "${RED}FAIL${NC}")	> Sends invalid method via netcat; an error response proves the server identifies and rejects non-compliant HTTP protocols."

echo -e "\n${CYAN}[2] CONFIGURATION & ROUTING${NC}"

dd if=/dev/zero of=large_test.bin bs=1M count=2 2>/dev/null
CODE413=$(curl -s -o /dev/null -w "%{http_code}" -H "Content-Type: multipart/form-data" --data-binary @large_test.bin "$SERVER/upload/")
rm -f large_test.bin
echo -e "Body Limit (413):	$([[ "$CODE413" == "413" ]] && echo -ne "${GREEN}PASS${NC}" || echo -ne "${RED}FAIL ($CODE413)${NC}")	> Sends 2MB file to 1MB limit route; 413 error proves client_max_body_size is enforced."

CODE405=$(curl -s -o /dev/null -w "%{http_code}" -X POST "$SERVER/images/")
echo -e "Limit Except (405):	$([[ "$CODE405" == "405" || "$CODE405" == "403" ]] && echo -ne "${GREEN}PASS${NC}" || echo -ne "${RED}FAIL ($CODE405)${NC}")	> Sends forbidden POST to GET-only route; 405/403 proves the server applies location-specific method restrictions."

AI_CONTENT=$(curl -s "$SERVER/upload/")
echo -e "Autoindex (ON):		$([[ "$AI_CONTENT" == *"Index"* || "$AI_CONTENT" == *"href"* ]] && echo -ne "${GREEN}PASS${NC}" || echo -ne "${RED}FAIL${NC}")	> Requests directory with autoindex enabled; HTML link listing proves the server generates directory indices."

CODE403_AI=$(curl -s -o /dev/null -w "%{http_code}" "$SERVER/cgi-bin/")
echo -e "Autoindex (OFF):	$([[ "$CODE403_AI" == "403" || "$CODE403_AI" == "404" ]] && echo -ne "${GREEN}PASS${NC}" || echo -ne "${RED}FAIL ($CODE403_AI)${NC}")	> Requests directory with autoindex disabled; a 403 error proves the server protects file hierarchy by default."

CODE404=$(curl -s -o /dev/null -w "%{http_code}" "$SERVER/non_existent_file")
echo -e "Error Page (404):	$([[ "$CODE404" == "404" ]] && echo -ne "${GREEN}PASS${NC}" || echo -ne "${RED}FAIL ($CODE404)${NC}")	> Requests invalid path; 404 status proves the server handles missing resources using the configured error handler."

CODE301=$(curl -s -o /dev/null -w "%{http_code}" "$SERVER/redirect/")
echo -e "Redirect (301):		$([[ "$CODE301" == "301" || "$CODE301" == "302" ]] && echo -ne "${GREEN}PASS${NC}" || echo -ne "${RED}FAIL ($CODE301)${NC}")	> Requests redirected route; 301/302 status proves the server processes the 'return' directive for URI forwarding."

echo -e "\n${CYAN}[3] PORT ISSUES (VIRTUAL HOSTING)${NC}"

VCODE=$(curl -s -o /dev/null -w "%{http_code}" -H "Host: testsite.com" "http://localhost:8081/")
echo -e "VHost (8081):		$([[ "$VCODE" == "200" ]] && echo -ne "${GREEN}PASS${NC}" || echo -ne "${RED}FAIL ($VCODE)${NC}")	> Sends specific Host header; 200 OK on shared port proves the server correctly differentiates between multiple virtual servers."

echo -e "\n${CYAN}[4] CGI & BONUSES${NC}"

curl -s -c c.txt "$SERVER/cgi-bin/eval_demo.py" > /dev/null
V2=$(curl -s -b c.txt "$SERVER/cgi-bin/eval_demo.py" | grep "Visits: 2")
echo -e "Cookies/Session:	$([[ ! -z "$V2" ]] && echo -ne "${GREEN}PASS${NC}" || echo -ne "${RED}FAIL${NC}")	> Tracks visit count via Set-Cookie; persistent state proves the server supports session tracking."

CGI_MULTI=$(curl -s "$SERVER/cgi-bin/sys_info.sh" | grep "Bash")
echo -e "Multiple CGI:		$([[ ! -z "$CGI_MULTI" ]] && echo -ne "${GREEN}PASS${NC}" || echo -ne "${RED}FAIL${NC}")	> Executes a .sh script alongside .py; output proves the server can execute varied script interpreters simultaneously."

echo -e "\n${CYAN}[5] RESILIENCE${NC}"

curl -s "$SERVER/cgi-bin/crash.py" > /dev/null
STILL_ALIVE=$(curl -s -o /dev/null -w "%{http_code}" "$SERVER/index.html")
echo -e "CGI Crash (Alive):	$([[ "$STILL_ALIVE" == "200" ]] && echo -ne "${GREEN}PASS${NC}" || echo -ne "${RED}FAIL${NC}")	> Triggers a script exit(1); subsequent server response proves the main process is isolated from CGI failures."

echo -e "Testing Timeout (Wait 5s)..."
CODE504=$(curl -s -o /dev/null -w "%{http_code}" "$SERVER/cgi-bin/infinite.py")
echo -e "Timeout (504):		$([[ "$CODE504" == "504" ]] && echo -ne "${GREEN}PASS${NC}" || echo -ne "${RED}FAIL ($CODE504)${NC}")	> Requests infinite loop script; 504 Gateway Timeout proves the server terminates hanging handlers."

echo -e "\n${CYAN}[6] PORT CONFLICT (MANUAL TEST)${NC}"
echo -e "Run: ./webserv config/webserv.conf"
echo -e "Expect: 'Error: bind: Address already in use' and clean exit."
echo -e "${CYAN}==================================================${NC}"
