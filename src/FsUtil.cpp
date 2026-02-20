#include "FsUtil.hpp"

#include <filesystem>

#include "IRequestManager.hpp"

bool safeIsDir(IRequestManager &manager, const std::filesystem::path &path)
{
	try
	{
		return std::filesystem::is_directory(path);
	}
	catch (const std::filesystem::filesystem_error &)
	{
		// onRequestError should throw, but return something to satisfy compiler
		manager.onRequestError(500);
		return false;
	}
}

bool safeIsRegular(IRequestManager &manager, const std::filesystem::path &path)
{
	try
	{
		return std::filesystem::is_regular_file(path);
	}
	catch (const std::filesystem::filesystem_error &)
	{
		// onRequestError should throw, but return something to satisfy compiler
		manager.onRequestError(500);
		return false;
	}
}
