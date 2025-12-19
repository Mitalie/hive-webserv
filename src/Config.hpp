#ifndef WEBSERV_CONFIG_HPP
#define WEBSERV_CONFIG_HPP

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <tuple>

enum class LocationDirective {
	Redirect,
	Root,
	Index,
	Autoindex,
	UploadStore,
	Methods,
	CgiExt,
	Unknown
};

inline const std::unordered_map<std::string, LocationDirective> locationDirectiveMap = {
	{"redirect", LocationDirective::Redirect},
	{"root", LocationDirective::Root},
	{"index", LocationDirective::Index},
	{"autoindex", LocationDirective::Autoindex},
	{"upload_store", LocationDirective::UploadStore},
	{"methods", LocationDirective::Methods},
	{"cgi_ext", LocationDirective::CgiExt}
};

enum class ServerDirective {
	Listen,
	ServerName,
	ErrorPage,
	ClientMaxBodySize,
	Location,
	Unknown
};

inline const std::unordered_map<std::string, ServerDirective> serverDirectiveMap = {
	{"listen", ServerDirective::Listen},
	{"server_name", ServerDirective::ServerName},
	{"error_page", ServerDirective::ErrorPage},
	{"client_max_body_size", ServerDirective::ClientMaxBodySize},
	{"location", ServerDirective::Location}
};

struct RouteConfig
{
	RouteConfig(class Tokenizer& tokenizer);
	std::string path;									// Location path (e.g., "/images")
	std::vector<std::string> allowedMethods;			// Allowed HTTP methods (GET, POST, etc.)
	std::string root;									// Filesystem root for this route
	std::string index;									// Default file for directory requests
	bool autoindex;										// Directory listing enabled/disabled
	int redirectCode;                                   // Redirect status code (0 if none)
	std::string redirect;								// Redirect URL (empty if none)
	std::map<std::string, std::string> cgiInterpreters; // File extension -> interpreter path
	std::string uploadStore;							// Directory for uploaded files
	void handleLocationRedirect(class Tokenizer& tokenizer);
	void handleLocationRoot(class Tokenizer& tokenizer);
	void handleLocationIndex(class Tokenizer& tokenizer);
	void handleLocationAutoindex(class Tokenizer& tokenizer);
	void handleLocationUploadStore(class Tokenizer& tokenizer);
	void handleLocationMethods(class Tokenizer& tokenizer);
	void handleLocationCgiExt(class Tokenizer& tokenizer);
};

struct HostPort
{
	std::string host; // Host to listen on
	std::string port; // Port to listen on
	bool operator<(const HostPort& other) const
	{
		return std::tie(host, port) < std::tie(other.host, other.port);
	}
};

struct ServerConfig
{
	ServerConfig(class Tokenizer& tokenizer);
	HostPort listener;					   // Port to listen on
	std::vector<std::string> serverNames;  // Server names (virtual hosts)
	std::map<int, std::string> errorPages; // Error code to file path
	size_t clientMaxBodySize;			   // Max body size in bytes
	// All location blocks, sorted by descending path length for efficient prefix matching
	std::vector<RouteConfig> routes;	   // All location blocks
	void handleListen(class Tokenizer& tokenizer);
	void handleServerName(class Tokenizer& tokenizer);
	void handleErrorPage(class Tokenizer& tokenizer);
	void handleClientMaxBodySize(class Tokenizer& tokenizer);
};

typedef std::vector<ServerConfig> ListenerConfig;

typedef std::map<HostPort, ListenerConfig> PortServerMap;

PortServerMap parseConfig(const std::string& filename);


#endif // WEBSERV_CONFIG_HPP
