#ifndef SH_HPP
#define SH_HPP

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <memory>
#include <initializer_list>
#include <exception>
#include <ext/stdio_filebuf.h>
#include <iostream>

namespace sh
{
  class stream : public std::iostream
  {
    int m_fd = -1;
    std::shared_ptr<__gnu_cxx::stdio_filebuf<char>> m_filebuf;
  public:
    stream()
    {
    }
    virtual ~stream()
    {
      close();
    }
    inline void fd(int fd, std::ios_base::openmode mode)
    {
      close();
      if (fd >= 0)
      {
        m_fd = fd;
        m_filebuf = std::make_shared<__gnu_cxx::stdio_filebuf<char>>(fd, mode);
      }
      rdbuf(m_filebuf.get());
    }
    inline void close()
    {
      if (m_fd >= 0)
      {
        ::close(m_fd);
        m_fd = -1;
        m_filebuf = nullptr;
      }
    }
  };

  class sh
  {
    inline const char* toChars(const char* str) const { return str; }
    template<typename T>
    inline const char* toChars(const T& str) const { return str.c_str(); }

    stream m_in;
    stream m_out;
    stream m_err;
    int m_pid = 0;

    sh(sh const&) = delete;
    sh(sh&&) = delete;
    sh& operator=(sh const&) = delete;
    sh& operator=(sh &&) = delete;

  public:
    template<typename... Args>
    sh(Args... args)
    {
      union
      {
        struct
        {
          int r;
          int w;
        };
        int fds[2];
      } fifo_in, fifo_out, fifo_err;
      if (::pipe(fifo_in.fds) < 0)
        throw std::exception();
      if (::pipe(fifo_out.fds) < 0)
      {
        ::close(fifo_in.r);
        ::close(fifo_in.w);
        throw std::exception();
      }
      if (::pipe(fifo_err.fds) < 0)
      {
        ::close(fifo_in.r);
        ::close(fifo_in.w);
        ::close(fifo_out.r);
        ::close(fifo_out.w);
        throw std::exception();
      }
      m_pid = ::vfork();
      if (m_pid < 0)
      {
        throw std::exception();
      }
      else if (m_pid)
      {
        ::close(fifo_in.r);
        ::close(fifo_out.w);
        ::close(fifo_err.w);
        m_in.fd(fifo_in.w, std::ios::out);
        m_out.fd(fifo_out.r, std::ios::in);
        m_err.fd(fifo_err.r, std::ios::in);
      }
      else
      {
        const char* argv[] = { toChars(args)..., nullptr };
        ::dup2(fifo_in.r, STDIN_FILENO);
        ::dup2(fifo_out.w, STDOUT_FILENO);
        ::dup2(fifo_err.w, STDERR_FILENO);
        for (auto i(sysconf(_SC_OPEN_MAX)); i >= 3; -- i)
          ::close(i);
        ::_exit(::execvp(argv[0], (char* const*)argv));
      }
    }
    virtual ~sh()
    {
      retcode();
    }
    inline std::ostream& in()  { return m_in; }
    inline std::istream& out() { return m_out; }
    inline std::istream& err() { return m_err; }
    inline void kill(int sig)
    {
    }
    inline int wait()
    {
      if (m_pid <= 0)
      {
        throw std::exception();
      }
      union wait pstat;
      int pid;
      sigset_t nmask;
      ::sigemptyset(&nmask);
      ::sigaddset(&nmask, SIGINT);
      ::sigaddset(&nmask, SIGQUIT);
      ::sigaddset(&nmask, SIGHUP);
      sigset_t omask;
      ::sigprocmask(SIG_BLOCK, &nmask, &omask);
      do
      {
        pid = ::waitpid(m_pid, &pstat.w_status, 0);
      }
      while (pid < 0 && errno == EINTR);
      ::sigprocmask(SIG_SETMASK, &omask, NULL);
      return pid < 0 ? -1 : pstat.__wait_terminated.__w_retcode;
    }
    inline int retcode()
    {
      if (m_pid <= 0)
      {
        throw std::exception();
      }
      m_in.close();
      m_out.close();
      m_err.close();
      return wait();
    }
  };
}

#endif
