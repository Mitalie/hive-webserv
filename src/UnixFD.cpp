#include "UnixFD.hpp"

#include <utility>

#include <unistd.h>

UnixFD::UnixFD(int fd)
	: fd(fd)
{
}

UnixFD::UnixFD(UnixFD &&other)
	: fd(other.fd)
{
	other.fd = -1;
}

UnixFD &UnixFD::operator=(UnixFD &&other)
{
	// Destructor of tmp cleans up old this->fd
	// In case of self-assignment, tmp steals fd and swap puts it back
	UnixFD tmp(std::move(other));
	std::swap(fd, tmp.fd);
	return *this;
}

UnixFD::~UnixFD()
{
	if (fd >= 0)
		close(fd);
}

UnixFD::operator int() const
{
	return fd;
}
