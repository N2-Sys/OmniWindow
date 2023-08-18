#include <stdexcept>
#include <cstring>

#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

class NetworkStream {
  int fd;

public:
  NetworkStream() {
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
      throw std::runtime_error("socket");
  }

  void connect(const char *ip, uint16_t port) {
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1)
      throw std::runtime_error("inet_pton");
    addr.sin_port = htons(port);

    if (::connect(fd, (sockaddr *)&addr, sizeof(addr)) < 0)
      throw std::runtime_error("connect");
  }

  void writeAll(const void *buf, size_t n) {
    while (n > 0) {
      ssize_t r = ::write(fd, buf, n);
      if (r < 0)
        throw std::runtime_error("write");
      buf = static_cast<const char *>(buf) + r;
      n -= r;
    }
  }

  char readChar() {
    char c;
    ssize_t r = ::read(fd, &c, 1);
    if (r <= 0)
      throw std::runtime_error("read");
    return c;
  }

  void close() {
    ::close(fd);
    fd = -1;
  }

  ~NetworkStream() {
    if (fd >= 0) {
      ::close(fd);
      fd = -1;
    }
  }
};
