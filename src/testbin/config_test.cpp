#include <cstddef>	 // size_t
#include <exception> // std::runtime_error
#include <iostream>
#include <map>		 // std::map

#include "Config.hpp"

int main()
{
	try
	{
		PortServerMap config = parseConfig("config/webserv.conf");
		std::cout << "Config parsed successfully!\n";
		std::cout << "Number of listeners: " << config.size() << std::endl;
		if (config.empty())
		{
			std::cout << "No listeners found in config.\n";
		}
		for (const auto &entry : config)
		{
			std::cout << "\033[1;36mListener: " << entry.first.host << ":" << entry.first.port << "\033[0m\n";
			std::cout << "  \033[1;34mNumber of servers: \033[0m" << entry.second.size() << std::endl;
			if (entry.second.empty())
			{
				std::cout << "  No servers found for this listener.\n";
			}
			for (const ServerConfig &server : entry.second)
			{
				std::cout << "  \033[1;32mServer names: \033[0m";
				if (server.serverNames.empty())
					std::cout << "(none)";
				for (size_t i = 0; i < server.serverNames.size(); ++i)
				{
					std::cout << server.serverNames[i];
					if (i + 1 < server.serverNames.size())
						std::cout << ", ";
				}
				std::cout << "\n  \033[1;33mError pages: \033[0m";
				if (server.errorPages.empty())
					std::cout << "(none)";
				for (std::map<int, std::string>::const_iterator it = server.errorPages.begin(); it != server.errorPages.end(); ++it)
				{
					std::cout << it->first << "->" << it->second << " ";
				}
				std::cout << "\n  \033[1;35mMax body size: \033[0m" << server.clientMaxBodySize << " bytes\n";
				std::cout << "  \033[1;34mNumber of routes: \033[0m" << server.routes.size() << std::endl;
				if (server.routes.empty())
				{
					std::cout << "    No routes found for this server.\n";
				}
				for (size_t r = 0; r < server.routes.size(); ++r)
				{
					const RouteConfig &route = server.routes[r];
					std::cout << "\n    \033[1;36mLocation: \033[0m" << route.path << "\n";
					if (!route.root.empty())
						std::cout << "      \033[1;32mRoot: \033[0m" << route.root << "\n";
					if (!route.allowedMethods.empty())
					{
						std::cout << "      \033[1;34mMethods: \033[0m";
						for (size_t m = 0; m < route.allowedMethods.size(); ++m)
						{
							std::cout << route.allowedMethods[m];
							if (m + 1 < route.allowedMethods.size())
								std::cout << ", ";
						}
						std::cout << "\n";
					}
					if (!route.index.empty())
						std::cout << "      \033[1;33mIndex: \033[0m" << route.index << "\n";
					std::cout << "      \033[1;35mAutoindex: \033[0m" << (route.autoindex ? "on" : "off") << "\n";
					if (!route.redirect.empty())
						std::cout << "      \033[1;31mRedirect: \033[0m" << route.redirect << "\n";
					if (!route.uploadStore.empty())
						std::cout << "      \033[1;32mUpload store: \033[0m" << route.uploadStore << "\n";
					if (!route.cgiInterpreters.empty())
					{
						std::cout << "      \033[1;34mCGI Interpreters: \033[0m";
						for (std::map<std::string, std::string>::const_iterator cit = route.cgiInterpreters.begin(); cit != route.cgiInterpreters.end(); ++cit)
						{
							std::cout << cit->first << "->" << cit->second << " ";
						}
						std::cout << "\n";
					}
					std::cout << std::endl;
				}
			}
		}
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error parsing config: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
