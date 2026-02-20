#include "FsUtil.hpp"

#include <filesystem>

#include "IRequestManager.hpp"

bool safeIsDir(IRequestManager &manager, const std::filesystem::path &path)
{
	try
	{
		return std::filesystem::is_directory(path);
	}
	catch (std::filesystem::filesystem_error)
	{
		// onRequestError should throw, but return a default constructed value to satisfy compiler
		manager.onRequestError(500);
		return {};
	}
}

bool safeIsRegular(IRequestManager &manager, const std::filesystem::path &path)
{
	try
	{
		return std::filesystem::is_regular_file(path);
	}
	catch (std::filesystem::filesystem_error)
	{
		// onRequestError should throw, but return a default constructed value to satisfy compiler
		manager.onRequestError(500);
		return {};
	}
}
