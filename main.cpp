#include <iostream>
#include <thread>
#include <vector>
#include <sstream>
#include <algorithm>
#include <map>
#include <queue>
#include <condition_variable>
#include <atomic>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <sys/un.h>

#include "customsh.hpp"
#include "daemonized.hpp"
#include "ns.hpp"
#include "sh.hpp"

#define WORKER_COUNT 10

struct request
{
  int fd = -1;
  std::vector<char> query;
  customsh::module_ptr module;

  request() = delete;
  request(const request&) = delete;
  request(const request&&) = delete;
  request(int _fd) : fd(_fd) { query.reserve(2048); }
  ~request()
  {
    if (fd >= 0)
    {
      close(fd);
    }
  }
  inline bool load(char* buff, std::size_t size)
  {
    query.insert(query.end(), buff, buff + size);
    if (!query_size && query.size() > sizeof(uint32_t))
    {
      query_size = *(uint32_t*)query.data() + sizeof(uint32_t);
      query.reserve(query_size + 1);
    }
    if (query_size >= query.size())
    {
      if (query.size() == query_size)
        query.emplace_back(0);
      return true;
    }
    return false;
  }
private:
  uint32_t query_size = 0;
};

using request_ptr = std::shared_ptr<request>;

class request_queue
{
  std::atomic<bool>& m_running;
  std::mutex m_mutex;
  std::queue<request_ptr> m_queue;
  std::condition_variable m_not_empty;
public:
  request_queue(std::atomic<bool>& running) : m_running(running) { }
  void put(request_ptr req)
  {
    std::unique_lock<std::mutex>(m_mutex);
    m_queue.emplace(req);
    m_not_empty.notify_one();
  }
  request_ptr get()
  {
    std::unique_lock<std::mutex> locker(m_mutex);
    while (m_running && m_queue.size() == 0)
    {
      m_not_empty.wait(locker);
    }
    request_ptr req;
    if (m_running)
    {
      req = m_queue.front();
      m_queue.pop();
    }
    return req;
  }
};

static std::atomic<bool> running(true);
static request_queue requests(running);
static std::map<int, request_ptr> buffers;

static void termination(int)
{
  running = false;
}

static void worker()
{
  while (running)
  {
    std::stringstream cout;
    auto req(requests.get());
    if (running); else break;
    try
    {
      if (!req->module)
      {
        req->module = customsh::modules::get(req->query.data() + sizeof(uint32_t));
      }
      info() << "call" << req->query;
      req->module->call(req->query.data() + sizeof(uint32_t) + req->module->prefix_size, cout);
      uint32_t msg_ret(0);
      uint32_t msg_size(cout.str().size());
      const char* msg(cout.str().c_str());
      write(req->fd, &msg_ret, sizeof(msg_ret));
      write(req->fd, &msg_size, sizeof(msg_size));
      write(req->fd, msg, msg_size);
      fsync(req->fd);
    }
    catch (const customsh::bad_argument& ex)
    {
      error() << "bad_argument";
    }
    catch (const customsh::not_found& ex)
    {
      error() << "not_found" << req->query;
    }
    catch (const customsh::locked& ex)
    {
      requests.put(req);
    }
  }
}

static int open_listener_unix(const char* path)
{
  int fd(socket(AF_UNIX, SOCK_STREAM, 0));
  if (fd >= 0)
  {
    sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, path, sizeof(addr.sun_path));
    if (path[0] == '@')
    {
      addr.sun_path[0] = 0;
    }
    socklen_t addr_len(sizeof(addr.sun_family) + strlen(path));
    if (bind(fd, (struct sockaddr*)&addr, addr_len) < 0)
    {
      close(fd);
      fd = -1;
    }
    else
    {
      listen(fd, 20);
    }
  }
  return fd;
}

static void set_non_blocking(int fd)
{
  int flags(fcntl(fd, F_GETFL, 0));
  if (flags >= 0)
  {
    flags |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);
  }
}

static void set_blocking(int fd)
{
  int flags(fcntl(fd, F_GETFL, 0));
  if (flags >= 0)
  {
    flags &= ~O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);
  }
}

static void loop_event(const std::set<int>& listen_fd)
{
  if (listen_fd.size() <= 0)
  {
    return;
  }

  std::vector<epoll_event> events(1024);
  auto epoll_fd(epoll_create1(0));
  epoll_event event;

  for (auto fd : listen_fd)
  {
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    set_non_blocking(event.data.fd);
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, event.data.fd, &event))
    {
      /// perror ("epoll_ctl");
      running = false;
    }
  }

  while (running)
  {
    auto n(epoll_wait(epoll_fd, events.data(), events.size(), 500));
    for (decltype(n) i(0); i < n; ++ i)
    {
      auto& current(events.at(i));
      if (false
        ||  (current.events & (EPOLLERR | EPOLLHUP))
        || !(current.events & EPOLLIN)
      )
      {
        /// fprintf (stderr, "epoll error\n");
        close(current.data.fd);
        buffers.erase(current.data.fd);
      }
      else if (listen_fd.count(current.data.fd))
      {
        while (true)
        {
          event.data.fd = accept(current.data.fd, nullptr, nullptr);
          if (event.data.fd == -1)
          {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
              /* We have processed all incoming connections. */
              break;
            }
            else
            {
              /// perror ("accept");
              break;
            }
          }
          event.events = EPOLLIN;
          set_non_blocking(event.data.fd);
          if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, event.data.fd, &event))
          {
            /// perror ("epoll_ctl");
            running = false;
          }
        }
      }
      else
      {
        while (true)
        {
          char buffer[512];
          auto buffer_size(read(current.data.fd, buffer, sizeof buffer));
          if (buffer_size <= 0)
          {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
              break;
            close(current.data.fd);
            buffers.erase(current.data.fd);
            break;
          }
          request_ptr req;
          try
          {
            req = buffers.at(current.data.fd);
          }
          catch (const std::out_of_range& ex)
          {
            req = std::make_shared<request>(int(current.data.fd));
            buffers[current.data.fd] = req;
          }
          if (req)
          {
            if (req->load(buffer, buffer_size))
            {
              epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current.data.fd, nullptr);
              set_blocking(current.data.fd);
              buffers.erase(current.data.fd);
              requests.put(req);
              break;
            }
          }
        }
      }
    }
  }
}

int main()
{
  if (daemonized(termination))
    return 0;

  std::set<int> listen_fd;

  customsh::modules::init();

  std::vector<std::thread> workers;
  workers.reserve(WORKER_COUNT);
  for (decltype(workers.capacity()) i(0); i < workers.capacity(); ++ i)
    workers.emplace_back(worker);

  {
    ns::net ns("test");
    listen_fd.emplace(open_listener_unix("@custom_sh"));
  }
  {
    listen_fd.emplace(open_listener_unix("@custom_sh"));
  }

  if (listen_fd.size() <= 0)
  {
    running = false;
  }
  else
  {
    debug() << "listen_fd" << listen_fd;
    loop_event(listen_fd);
  }

  std::for_each(workers.begin(), workers.end(), std::mem_fn(&std::thread::join));

  return 0;
}

