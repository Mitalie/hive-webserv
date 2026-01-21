#pragma once

#include <string>
#include <map>

class MimeTypes
{
public:
	/*
		Returns the MIME type for a given filename (e.g., "image/png" for "file.png").
		Defaults to "application/octet-stream" if unknown.
	*/
	static std::string getType(const std::string &filename);

private:
	MimeTypes();
	~MimeTypes();

	MimeTypes(const MimeTypes &) = delete;
	MimeTypes &operator=(const MimeTypes &) = delete;

	std::map<std::string, std::string> extensionToType;

	static MimeTypes instance;
};
