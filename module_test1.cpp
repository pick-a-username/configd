#include <iostream>
#include "customsh.hpp"

class Test1 : public customsh::d
{
public:
  Test1()
  {
    trace;

    bind("this is a prefix ", &Test1::func1);
    bind("this is a ", &Test1::func2);
    bind("a-this is a ", &Test1::func3);
  }
  ~Test1()
  {
    trace;
  }
  void func1(const customsh::args& args, std::ostream& cout)
  {
    cout << __FILE__ << ':' << __LINE__ << '|' << __PRETTY_FUNCTION__ << "| [" << args.prefix << "] [" << args.query << ']';
  }
  void func2(const customsh::args& args, std::ostream& cout)
  {
    cout << __FILE__ << ':' << __LINE__ << '|' << __PRETTY_FUNCTION__ << "| [" << args.prefix << "] [" << args.query << ']';
  }
  void func3(const customsh::args& args, std::ostream& cout)
  {
    cout << __FILE__ << ':' << __LINE__ << '|' << __PRETTY_FUNCTION__ << "| [" << args.prefix << "] [" << args.query << ']';
  }
};

volatile static Test1 instance;

