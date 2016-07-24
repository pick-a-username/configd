#ifndef DAEMONIZED_HPP
#define DAEMONIZED_HPP

#include <functional>
#include <signal.h>

bool daemonized(sighandler_t handle);

#endif

