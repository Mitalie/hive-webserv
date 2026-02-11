#include "Signals.hpp"

#include <csignal>

static volatile std::sig_atomic_t exitSignalCaught = 0;

extern "C" void exitSignalHandler(int sig)
{
	std::signal(sig, exitSignalHandler); // Probably unnecessary, but not guaranteed by standard
	exitSignalCaught = 1;
}

void setupSignals()
{
	std::signal(SIGINT, exitSignalHandler);
	std::signal(SIGTERM, exitSignalHandler);
}

bool gotExitSignal()
{
	// We expect to exit, so don't bother resetting the flag
	return exitSignalCaught != 0;
}
