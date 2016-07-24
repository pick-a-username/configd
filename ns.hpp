#ifndef NS_HPP
#define NS_HPP

#include <string.h>
#include <errno.h>
#include <string>
#include <exception>

namespace ns
{
  struct exception : public std::exception
  {
    int code;
    exception(int _code) : code(_code) { }
    const char* what() const noexcept { return strerror(code); }
  };

  class net
  {
  public:
    net(const char* netns);
    net(const std::string& netns);
    virtual ~net();
  };
}

#endif
