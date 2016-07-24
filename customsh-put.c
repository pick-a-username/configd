#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

static char buffer_cmd[4096];

static int exec_customsh_cmd(const char* sun_path, const char* cmd, int cmd_len)
{
  struct sockaddr_un addr;
  socklen_t addr_len;
  memset(&addr, 0, sizeof(struct sockaddr_un));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, sun_path, sizeof(addr.sun_path) - 1);
  addr_len = sizeof(addr.sun_family) + strlen(sun_path);
  if (sun_path[0] == '@')
  {
    addr.sun_path[0] = 0;
  }

  int sock = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sock < 0)
  {
    perror("socket");
    return 2;
  }

  if (connect(sock, (struct sockaddr*)&addr, addr_len) == -1)
  {
    perror("connect");
    return 3;
  }

  int recv_len = 0, ret = 0, buff_len = 0;

  if (send(sock, &cmd_len, sizeof(cmd_len), 0) == -1)
  {
    perror("send");
    return 4;
  }

  if (send(sock, cmd, cmd_len, 0) == -1)
  {
    perror("send");
    return 4;
  }

  recv_len = recv(sock, &ret, sizeof(ret), 0);
  if (recv_len < 0)
  {
    perror("recv");
    return 5;
  }
  else if (recv_len == 0)
  {
    printf("Server closed connection\n");
    return 6;
  }
  else if (recv_len < 4)
  {
    perror("package error");
    return 5;
  }

  recv_len = recv(sock, &buff_len, sizeof(buff_len), 0);
  if (recv_len < 0)
  {
    perror("recv");
    return 5;
  }
  else if (recv_len == 0)
  {
    printf("Server closed connection\n");
    return 6;
  }
  else if (recv_len < 4)
  {
    printf("package error");
    return 5;
  }

  char* buff = (char*)malloc(buff_len + 1);
  if (buff != NULL)
  {
    recv(sock, buff, buff_len, 0);
    printf("ret(%d)>\n%*s\n", ret, buff_len, buff);
    free(buff);
  }

  close(sock);

  return ret;
}

int main(int argc, char** argv)
{
  if (argc == 2)
  {
    while (feof(stdin) == 0)
    {
      char* cmd = fgets(buffer_cmd, sizeof(buffer_cmd) - 1, stdin);
      if (cmd == NULL)
      {
        break;
      }

      int cmd_len = strlen(cmd);
      while (cmd_len > 0 && cmd[cmd_len - 1] == '\n')
      {
        -- cmd_len;
      }
      cmd[cmd_len] = 0;

      exec_customsh_cmd(argv[1], cmd, cmd_len);
    }
    return 0;
  }
  else if (argc == 3)
  {
    return exec_customsh_cmd(argv[1], argv[2], strlen(argv[2]));
  }
  fprintf(stderr, "Using: %s UNIX_SOCKET [CMD]\n", argv[0]);
  return 1;
}

