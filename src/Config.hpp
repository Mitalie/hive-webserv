#ifndef WEBSERV_CONFIG_HPP
#define WEBSERV_CONFIG_HPP

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <cstddef>

enum class ServerDirective {
	Listen,
	ServerName,
	ErrorPage,
	ClientMaxBodySize,
	Location,
	Unknown
};

static const std::unordered_map<std::string, ServerDirective> serverDirectiveMap = {
	{"listen", ServerDirective::Listen},
	{"server_name", ServerDirective::ServerName},
	{"error_page", ServerDirective::ErrorPage},
	{"client_max_body_size", ServerDirective::ClientMaxBodySize},
	{"location", ServerDirective::Location}
};

struct RouteConfig
{
	std::string path;									// Location path (e.g., "/images")
	std::vector<std::string> allowedMethods;			// Allowed HTTP methods (GET, POST, etc.)
	std::string root;									// Filesystem root for this route
	std::string index;									// Default file for directory requests
	bool autoindex;										// Directory listing enabled/disabled
	int redirectCode;                                   // Redirect status code (0 if none)
	std::string redirect;								// Redirect URL (empty if none)
	std::map<std::string, std::string> cgiInterpreters; // File extension -> interpreter path
	std::string uploadStore;							// Directory for uploaded files
};

struct hostPort
{
	std::string host; // Host to listen on
	std::string port; // Port to listen on
	bool operator<(const hostPort &other) const
	{
		return std::tie(host, port) < std::tie(other.host, other.port);
	}
};

struct ServerConfig
{
	hostPort listener;					   // Port to listen on
	std::vector<std::string> serverNames;  // Server names (virtual hosts)
	std::map<int, std::string> errorPages; // Error code to file path
	size_t clientMaxBodySize;			   // Max body size in bytes
	std::vector<RouteConfig> routes;	   // All location blocks
	void handleListen(class Tokenizer &tokenizer);
	void handleServerName(class Tokenizer &tokenizer);
	void handleErrorPage(class Tokenizer &tokenizer);
	void handleClientMaxBodySize(class Tokenizer &tokenizer);
};

typedef std::map<hostPort, std::vector<ServerConfig>> PortServerMap;

class ConfigParser
{
public:
	PortServerMap parse(const std::string &filename);
private:
	ServerConfig parseServer(class Tokenizer &tokenizer);
	RouteConfig parseLocation(class Tokenizer &tokenizer, const std::string &locationPath);
	// Add more helpers as needed
};


#endif // WEBSERV_CONFIG_HPP
