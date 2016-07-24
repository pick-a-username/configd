#include <iostream>
#include <limits>
#include "customsh.hpp"

namespace customsh
{
  std::vector<module_ptr> modules::list;
  std::vector<module_ptr>::size_type modules::min_prefix_size(std::numeric_limits<std::vector<module_ptr>::size_type>::max());

  static int module_compare(module_ptr a, const std::string& query)
  {
    const auto n(std::memcmp(a->prefix, query.c_str(), std::min(a->prefix_size - 1, query.size())));
    if (n < 0) return -1;
    if (n > 0) return  1;
    return 0;
  }

  static bool module_compare_less(module_ptr a, module_ptr b)
  {
    const auto n(std::memcmp(a->prefix, b->prefix, std::min(a->prefix_size, b->prefix_size) - 1));
    return n ? n < 0 : a->prefix_size < b->prefix_size;
  }

  static bool module_compare_not_equal(module_ptr a, const std::string& query)
  {
    return !!std::memcmp(a->prefix, query.c_str(), std::min(a->prefix_size - 1, query.size()));
  }

  void modules::push(module_ptr mod)
  {
    list.emplace_back(mod);
  }

  void modules::init()
  {
    std::sort(list.begin(), list.end(), module_compare_less);
    for (auto i : list)
      if (min_prefix_size > i->prefix_size)
        min_prefix_size = i->prefix_size;
    -- min_prefix_size;
  }

  module_ptr modules::get(const std::string& query)
  {
    if (min_prefix_size > query.size())
      throw not_found();
    std::vector<module_ptr>::size_type l(0);
    std::vector<module_ptr>::size_type r(list.size());
    while (true)
    {
      auto i((l + r) / 2);
      auto m(list[i]);
      auto n(module_compare(m, query));
      if (n == 0)
      {
        for (++ i; i < list.size(); ++ i)
        {
          auto k(list[i]);
          if (module_compare_not_equal(k, query))
            break;
          m = k;
        }
        return m;
      }
      else if (i == l)
      {
        throw not_found();
      }
      else if (n < 0)
      {
        r = i;
      }
      else if (n > 0)
      {
        l = i + 1;
      }
    }
  }
}

