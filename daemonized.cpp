#include <exception>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "daemonized.hpp"

bool daemonized(sighandler_t handle)
{
  chdir("/");
  int pid = fork();
  if (pid < 0)
    throw std::exception();
  if (pid)
    return true;
  close(STDIN_FILENO);
  int dev_null = open("/dev/null", O_RDWR);
  if (dev_null >= 0)
  {
    dup2(dev_null, STDIN_FILENO);
    dup2(dev_null, STDOUT_FILENO);
    dup2(dev_null, STDERR_FILENO);
    close(dev_null);
  }
  else
  {
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
  }

  if(setsid() == -1)
    throw std::exception();

  if (signal(SIGTERM, handle) == SIG_IGN)
  {
    signal(SIGTERM, SIG_IGN);
  }
  signal(SIGINT, SIG_IGN);
  signal(SIGHUP, SIG_IGN);
  return false;
}
