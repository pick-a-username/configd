#include <iostream>
#include "customsh.hpp"
#include "sh.hpp"

class Test2 : public customsh::d
{
public:
  Test2()
  {
    trace;
    {
      sh::sh sh("/bin/ls", "-la");
      while (true)
      {
        std::string line;
        std::getline(sh.out(), line);
        if (sh.out().eof())
          break;
        debug() << line;
      }
    }
  }
  ~Test2()
  {
    trace;
  }
};

volatile static Test2 instance;
