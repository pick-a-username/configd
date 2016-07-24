#include <iostream>
#include "customsh.hpp"

class Test3 : public customsh::d
{
public:
  Test3()
  {
    trace;
  }
  ~Test3()
  {
    trace;
  }
};

volatile static Test3 instance;
