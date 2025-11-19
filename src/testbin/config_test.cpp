#include "../Config.hpp"
#include <iostream>
#include <cstddef>          // size_t
#include <exception>        // std::runtime_error
#include <map>              // std::map

int main() {
    ConfigParser parser;
    try {
        PortServerMap config = parser.parse("config/webserv.conf");
        std::cout << "Config parsed successfully!\n";
        std::cout << "Number of listeners: " << config.size() << std::endl;
        if (config.empty()) {
            std::cout << "No listeners found in config.\n";
        }
        for (const auto& entry : config) {
            std::cout << "Listener: " << entry.first.host << ":" << entry.first.port << "\n";
            std::cout << "  Number of servers: " << entry.second.size() << std::endl;
            if (entry.second.empty()) {
                std::cout << "  No servers found for this listener.\n";
            }
            for (const ServerConfig& server : entry.second) {
                std::cout << "  Server names: ";
                if (server.serverNames.empty()) std::cout << "(none)";
                for (size_t i = 0; i < server.serverNames.size(); ++i) {
                    std::cout << server.serverNames[i];
                    if (i + 1 < server.serverNames.size()) std::cout << ", ";
                }
                std::cout << "\n  Error pages: ";
                if (server.errorPages.empty()) std::cout << "(none)";
                for (std::map<int, std::string>::const_iterator it = server.errorPages.begin(); it != server.errorPages.end(); ++it) {
                    std::cout << it->first << "->" << it->second << " ";
                }
                std::cout << "\n  Max body size: " << server.clientMaxBodySize << " bytes\n";
                std::cout << "  Number of routes: " << server.routes.size() << std::endl;
                if (server.routes.empty()) {
                    std::cout << "    No routes found for this server.\n";
                }
                for (size_t r = 0; r < server.routes.size(); ++r) {
                    const RouteConfig& route = server.routes[r];
                    std::cout << "    Location: " << route.path << "\n";
                    if (!route.root.empty())
                        std::cout << "      Root: " << route.root << "\n";
                    if (!route.allowedMethods.empty()) {
                        std::cout << "      Methods: ";
                        for (size_t m = 0; m < route.allowedMethods.size(); ++m) {
                            std::cout << route.allowedMethods[m];
                            if (m + 1 < route.allowedMethods.size()) std::cout << ", ";
                        }
                        std::cout << "\n";
                    }
                    if (!route.index.empty())
                        std::cout << "      Index: " << route.index << "\n";
                    std::cout << "      Autoindex: " << (route.autoindex ? "on" : "off") << "\n";
                    if (!route.redirect.empty())
                        std::cout << "      Redirect: " << route.redirect << "\n";
                    if (!route.uploadStore.empty())
                        std::cout << "      Upload store: " << route.uploadStore << "\n";
                    if (!route.cgiInterpreters.empty()) {
                        std::cout << "      CGI Interpreters: ";
                        for (std::map<std::string, std::string>::const_iterator cit = route.cgiInterpreters.begin(); cit != route.cgiInterpreters.end(); ++cit) {
                            std::cout << cit->first << "->" << cit->second << " ";
                        }
                        std::cout << "\n";
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing config: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
