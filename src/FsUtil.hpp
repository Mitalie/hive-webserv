#pragma once

#include <filesystem>

#include "IRequestManager.hpp"

bool safeIsDir(IRequestManager &manager, const std::filesystem::path &path);
bool safeIsRegular(IRequestManager &manager, const std::filesystem::path &path);
