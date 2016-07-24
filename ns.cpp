#include <stdio.h>
#include <sched.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>

#ifndef CLONE_NEWNET
#define CLONE_NEWNET 0x40000000
#endif

#ifndef CLONE_NEWNS
#define CLONE_NEWNS  0x00020000
#endif

#include "ns.hpp"

namespace ns
{
  static inline void switch_ns(const char* path, int nstype, int flags)
  {
    int netns = open(path, O_RDONLY | O_CLOEXEC);
    if (netns < 0)
    {
      throw exception(errno);
    }
    if (setns(netns, nstype) < 0)
    {
      int _errno = errno;
      close(netns);
      throw exception(_errno);
    }
    close(netns);
    if (unshare(flags) < 0)
    {
      throw exception(errno);
    }
  }

  net::net(const char* netns)
  {
    if (netns[0] == '/')
    {
      switch_ns(netns, CLONE_NEWNET, CLONE_NEWNS);
    }
    else
    {
      switch_ns(std::string("/run/netns/").append(netns).c_str(), CLONE_NEWNET, CLONE_NEWNS);
    }
  }

  net::net(const std::string& netns)
  {
    if (netns.size() > 0)
    {
      if (netns.at(0) == '/')
      {
        switch_ns(netns.c_str(), CLONE_NEWNET, CLONE_NEWNS);
      }
      else
      {
        switch_ns(std::string("/run/netns/").append(netns).c_str(), CLONE_NEWNET, CLONE_NEWNS);
      }
    }
  }

  net::~net()
  {
    switch_ns("/proc/1/ns/net", CLONE_NEWNET, CLONE_NEWNS);
  }
}

