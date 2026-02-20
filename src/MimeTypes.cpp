#include "MimeTypes.hpp"

#include <cstddef>
#include <fstream>
#include <sstream>
#include <string>

#include "Utils.hpp"

MimeTypes MimeTypes::instance;

MimeTypes::MimeTypes()
{
	const char *path = "/etc/mime.types";
	std::ifstream file(path);

	// If file is missing, we just silently start with an empty map (defaults to octet-stream)
	if (!file.is_open())
		return;

	std::string line;
	while (std::getline(file, line))
	{
		if (line.empty() || line[0] == '#')
			continue;

		std::stringstream ss(line);
		std::string mimeType;
		ss >> mimeType; // First word: "text/html"

		std::string extension;
		while (ss >> extension) // Next words: "html", "htm", "shtml"
			extensionToType[extension] = mimeType;
	}
}

MimeTypes::~MimeTypes()
{
}

static std::string getExtension(const std::string &filename)
{
	size_t pos = filename.rfind('.');
	if (pos == std::string::npos || pos == filename.length() - 1)
		return "";
	return toLower(filename.substr(pos + 1));
}

std::string MimeTypes::getType(const std::string &filename)
{
	std::string ext = getExtension(filename);

	if (instance.extensionToType.contains(ext))
		return instance.extensionToType[ext];
	return "application/octet-stream";
}
