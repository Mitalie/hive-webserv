#pragma once

/*
	A wrapper to manage Unix file descriptor ownership.

	Prevents accidental copying, closes the file on destruction, and converts
	to int for code that expects a raw file descriptor.
*/
class UnixFD
{
public:
	// Avoid implicit construction to avoid unintentionally taking ownership

	explicit UnixFD(int fd = -1);
	UnixFD(const UnixFD &) = delete;
	UnixFD(UnixFD &&);
	UnixFD &operator=(const UnixFD &) = delete;
	UnixFD &operator=(UnixFD &&);
	~UnixFD();

	// Implicit conversion for passing to Unix functions
	operator int() const;

private:
	int fd;
};
