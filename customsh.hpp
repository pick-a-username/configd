#ifndef CUSTOMSH_HPP
#define CUSTOMSH_HPP

#include <set>
#include <map>
#include <list>
#include <vector>
#include <string>
#include <regex>
#include <mutex>
#include <memory>
#include <exception>
#include <ostream>
#include <sstream>
#include <thread>

namespace customsh
{
  class bad_argument : public std::exception
  {
  public:
    bad_argument() { }
  };

  class locked : public std::exception
  {
  public:
    locked() { }
  };

  class not_found : public std::exception
  {
  public:
    not_found() { }
  };

  class module
  {
    module(const module&) = delete;
    module(const module&&) = delete;
  public:
    const char* prefix;
    const std::size_t prefix_size;
    constexpr module(const char* _prefix, std::size_t _prefix_size) : prefix(_prefix), prefix_size(_prefix_size) { }
    virtual void call(const std::string& query, std::ostream& cout) = 0;
  };

  using module_ptr = std::shared_ptr<module>;

  class modules
  {
    static std::vector<module_ptr> list;
    static std::vector<module_ptr>::size_type min_prefix_size;
  public:
    static void push(module_ptr mod);
    static void init();
    static module_ptr get(const std::string& query);
  };

  struct args
  {
    std::string prefix;
    std::string query;
    std::smatch args;
  };

  template<typename Object>
  using member_ptr = void (Object::*)(const args&, std::ostream&);

  template<typename Object>
  class _module : public module
  {
    Object* m_object = nullptr;
    const member_ptr<Object> m_member = nullptr;
  public:
    constexpr _module(const char* _prefix, std::size_t _prefix_size, Object* _object, member_ptr<Object> _member)
      : module(_prefix, _prefix_size)
      , m_object(_object)
      , m_member(_member)
    {
    }

    void call(const std::string& query, std::ostream& cout)
    {
      if (!m_object->lock())
        throw locked();
      args a;
      a.prefix = prefix;
      a.query = query;
      (m_object->*m_member)(a, cout);
      m_object->unlock();
    }
  };

  template<typename Object>
  class _module_regex : public module
  {
    const std::regex m_regex;
    Object* m_object = nullptr;
    const member_ptr<Object> m_member = nullptr;
  public:
    constexpr _module_regex(const char* _prefix, std::size_t _prefix_size, const char* _regex, std::size_t _regex_size, Object* _object, member_ptr<Object> _member)
      : module(_prefix, _prefix_size)
      , m_regex(_regex, _regex_size)
      , m_object(_object)
      , m_member(_member)
    {
    }

    void call(const std::string& query, std::ostream& cout)
    {
      args a;
      if (!m_object->lock())
        throw locked();
      if (!std::regex_match(query, a.args, m_regex))
        throw bad_argument();
      a.prefix = prefix;
      a.query = query;
      (m_object->*m_member)(a, cout);
      m_object->unlock();
    }
  };

  template<typename Object>
  class _module_unsafe : public module
  {
    Object* m_object = nullptr;
    const member_ptr<Object> m_member = nullptr;
  public:
    constexpr _module_unsafe(const char* _prefix, std::size_t _prefix_size, Object* _object, member_ptr<Object> _member)
      : module(_prefix, _prefix_size)
      , m_object(_object)
      , m_member(_member)
    {
    }

    void call(const std::string& query, std::ostream& cout)
    {
      args a;
      a.prefix = prefix;
      a.query = query;
      (m_object->*m_member)(a, cout);
    }
  };

  template<typename Object>
  class _module_regex_unsafe : public module
  {
    const std::regex m_regex;
    Object* m_object = nullptr;
    const member_ptr<Object> m_member = nullptr;
  public:
    constexpr _module_regex_unsafe(const char* _prefix, std::size_t _prefix_size, const char* _regex, std::size_t _regex_size, Object* _object, member_ptr<Object> _member)
      : module(_prefix, _prefix_size)
      , m_regex(_regex, _regex_size)
      , m_object(_object)
      , m_member(_member)
    {
    }

    void call(const std::string& query, std::ostream& cout)
    {
      args a;
      if (!std::regex_match(query, a.args, m_regex))
        throw bad_argument();
      a.prefix = prefix;
      a.query = query;
      (m_object->*m_member)(a, cout);
    }
  };

  class d
  {
    d(const d&) = delete;
    d(const d&&) = delete;
    std::mutex m_lock;
  public:
    constexpr d() { }
    inline bool lock() { return m_lock.try_lock(); }
    inline void unlock() { m_lock.unlock(); }
  protected:

    template<std::size_t prefix_size, typename Object>
    inline void bind(const char (&prefix)[prefix_size], member_ptr<Object> _member)
    {
      modules::push(std::make_shared<_module<Object>>(prefix, prefix_size, static_cast<Object*>(this), _member));
    }

    template<std::size_t prefix_size, std::size_t regex_size, typename Object>
    inline void bind(const char (&prefix)[prefix_size], const char (&regex)[regex_size], member_ptr<Object> _member)
    {
      modules::push(std::make_shared<_module_regex<Object>>(prefix, prefix_size, regex, regex_size, static_cast<Object*>(this), _member));
    }

    template<std::size_t prefix_size, typename Object>
    inline void bind_unsafe(const char (&prefix)[prefix_size], member_ptr<Object> _member)
    {
      modules::push(std::make_shared<_module_unsafe<Object>>(prefix, prefix_size, static_cast<Object*>(this), _member));
    }

    template<std::size_t prefix_size, std::size_t regex_size, typename Object>
    inline void bind_unsafe(const char (&prefix)[prefix_size], const char (&regex)[regex_size], member_ptr<Object> _member)
    {
      modules::push(std::make_shared<_module_regex_unsafe<Object>>(prefix, prefix_size, regex, regex_size, static_cast<Object*>(this), _member));
    }
  };

  class log
  {
    std::string m_prefix_buffer;
    const std::string& m_prefix;
    std::string m_log_buffer;
    std::string& m_log;
    bool m_space = true;
    bool m_quote = true;
    int m_tab = 0;
    bool m_isDepth = false;
  public:
    log(const char* file, const char* line, const char* func)
      : m_prefix(m_prefix_buffer)
      , m_log(m_log_buffer)
    {
      m_log.reserve(4096);
      std::stringstream pid;
      pid << "0x" << std::hex << std::this_thread::get_id();
      m_prefix_buffer
        .append(pid.str())
        .append(1, '|')
        .append(file)
        .append(1, ':')
        .append(line)
        .append(1, '(')
        .append(func)
        .append(1, ')')
        ;
    }
    log(const log& log)
      : m_prefix(log.m_prefix)
      , m_log(log.m_log)
      , m_tab(log.m_tab)
      , m_isDepth(true)
    {
    }
    virtual ~log()
    {
      if (!m_isDepth)
        flush();
    }
    inline log& space()
    {
      m_space = true;
      return *this;
    }
    inline log& nospace()
    {
      m_space = false;
      return *this;
    }
    inline log& quote()
    {
      m_quote = true;
      return *this;
    }
    inline log& noquote()
    {
      m_quote = false;
      return *this;
    }
    inline log& tab()
    {
      ++ m_tab;
      return *this;
    }
    inline log& notab()
    {
      -- m_tab;
      return *this;
    }

    inline log& operator << (short value) { return to_string(value); }
    inline log& operator << (unsigned short value) { return to_string(value); }
    inline log& operator << (int value) { return to_string(value); }
    inline log& operator << (unsigned int value) { return to_string(value); }
    inline log& operator << (long int value) { return to_string(value); }
    inline log& operator << (unsigned long int value) { return to_string(value); }
    inline log& operator << (long long int value) { return to_string(value); }
    inline log& operator << (unsigned long long int value) { return to_string(value); }
    inline log& operator << (float value) { return to_string(value); }
    inline log& operator << (double value) { return to_string(value); }
    inline log& operator << (long double value) { return to_string(value); }

    inline log& operator << (char value)
    {
      if (m_space)
        m_log.append(1, ' ');
      switch (value)
      {
      case 0:
        m_log.append("\\x00");
        break;
      default:
        m_log.append(1, value);
      }
      return *this;
    }

    inline log& operator << (const std::string& value)
    {
      if (m_space)
        m_log.append(1, ' ');
      if (m_quote)
      {
        m_log.append(1, '"');
        m_log.append(value);
        m_log.append(1, '"');
      }
      else
      {
        m_log.append(value);
      }
      return *this;
    }

    inline log& operator << (const char* value)
    {
      if (m_space)
        m_log.append(1, ' ');
      if (m_quote)
      {
        m_log.append(1, '"');
        m_log.append(value);
        m_log.append(1, '"');
      }
      else
      {
        m_log.append(value);
      }
      return *this;
    }

    template<int N>
    inline log& operator << (const char (&value)[N])
    {
      if (m_space)
        m_log.append(1, ' ');
      if (m_quote)
      {
        m_log.append(1, '"');
        m_log.append(value, N);
        m_log.append(1, '"');
      }
      else
      {
        m_log.append(value, N);
      }
      return *this;
    }

    inline log& flush()
    {
      if (m_log.size())
      {
        std::string tab;
        tab.append(m_tab * 2, ' ');
        std::cout << m_prefix << ": " << tab << m_log << std::endl;
      }
      else
      {
        std::cout << m_prefix << std::endl;
      }
      m_log.clear();
      return *this;
    }

  private:
    template<typename T>
    inline log& to_string(T value)
    {
      if (m_space)
        m_log.append(1, ' ');
      m_log.append(std::to_string(value));
      return *this;
    }
  };

  class nolog
  {
  public:
    nolog(const char*, int, const char*) { }
    nolog(const nolog&) { }
    inline nolog &space() { return *this; }
    inline nolog &nospace() { return *this; }
    inline nolog &quote() { return *this; }
    inline nolog &noquote() { return *this; }
    template<typename T>
    inline nolog &operator<<(const T&) { return *this; }
  };

  template<typename V>
  inline log operator << (log l, const std::vector<V>& value)
  {
    (l.noquote() << "vector(").flush().tab().quote();
    for (auto& i : value)
    {
      (l << i).flush();
    }
    return l.notab() << ')';
  }

  template<typename V>
  inline log operator << (log l, const std::list<V>& value)
  {
    (l.noquote() << "list(").flush().tab().quote();
    for (auto& i : value)
    {
      (l << i).flush();
    }
    return l.notab() << ')';
  }

  template<typename V>
  inline log operator << (log l, const std::set<V>& value)
  {
    (l.noquote() << "set(").flush().tab().quote();
    for (auto& i : value)
    {
      (l << i).flush();
    }
    return l.notab() << ')';
  }

  template<typename K, typename V>
  inline log operator << (log l, const std::map<K, V>& value)
  {
    (l.noquote() << "map(").flush().tab().quote();
    for (auto& i : value)
    {
      (l << i.first << '=' << i.second).flush();
    }
    return l.notab() << ')';
  }
}

#define customsh_log_(file, line, func) customsh::log(file, #line, func)
#define customsh_log(file, line, func) customsh_log_(file, line, func)

#define debug() customsh_log(__FILE__, __LINE__, __PRETTY_FUNCTION__)
#define error() customsh_log(__FILE__, __LINE__, __PRETTY_FUNCTION__)
#define warning() customsh_log(__FILE__, __LINE__, __PRETTY_FUNCTION__)
#define info() customsh_log(__FILE__, __LINE__, __PRETTY_FUNCTION__)
#define trace customsh_log(__FILE__, __LINE__, __PRETTY_FUNCTION__)

#endif

