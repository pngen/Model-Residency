#include "mr/net.hpp"

#include <cstring>

#include "mr/core/error.hpp"

#ifdef _WIN32
#else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
#endif

namespace mr {
namespace {

constexpr std::uint32_t kMaxFrame = 64u * 1024u * 1024u;

socket_native invalid() noexcept {
#ifdef _WIN32
  return INVALID_SOCKET;
#else
  return -1;
#endif
}

} // namespace

NetInit::NetInit() {
#ifdef _WIN32
  WSADATA data;
  ok_ = (WSAStartup(MAKEWORD(2, 2), &data) == 0);
#else
  ok_ = true;
#endif
}
NetInit::~NetInit() {
#ifdef _WIN32
  if (ok_) { WSACleanup(); }
#endif
}

socket_native TcpSocket::invalid_fd() noexcept { return invalid(); }
socket_native TcpListener::invalid_fd() noexcept { return invalid(); }

TcpSocket::~TcpSocket() { close(); }
TcpSocket::TcpSocket(TcpSocket&& o) noexcept : fd_(o.fd_) { o.fd_ = invalid(); }
TcpSocket& TcpSocket::operator=(TcpSocket&& o) noexcept {
  if (this != &o) { close(); fd_ = o.fd_; o.fd_ = invalid(); }
  return *this;
}
void TcpSocket::close() noexcept {
  if (fd_ != invalid()) {
#ifdef _WIN32
    ::closesocket(fd_);
#else
    ::close(fd_);
#endif
    fd_ = invalid();
  }
}

bool TcpSocket::read_exact(void* out, std::size_t n) {
  auto* p = static_cast<unsigned char*>(out);
  std::size_t got = 0;
  while (got < n) {
#ifdef _WIN32
    const int r = ::recv(fd_, reinterpret_cast<char*>(p + got), static_cast<int>(n - got), 0);
#else
    const ssize_t r = ::recv(fd_, p + got, n - got, 0);
#endif
    if (r <= 0) { return false; }
    got += static_cast<std::size_t>(r);
  }
  return true;
}

bool TcpSocket::write_all(const void* data, std::size_t n) {
  const auto* p = static_cast<const unsigned char*>(data);
  std::size_t sent = 0;
  while (sent < n) {
#ifdef _WIN32
    const int r = ::send(fd_, reinterpret_cast<const char*>(p + sent), static_cast<int>(n - sent), 0);
#else
    const ssize_t r = ::send(fd_, p + sent, n - sent, 0);
#endif
    if (r <= 0) { return false; }
    sent += static_cast<std::size_t>(r);
  }
  return true;
}

bool TcpSocket::read_frame(std::vector<std::uint8_t>& payload) {
  std::uint8_t lenraw[4];
  if (!read_exact(lenraw, 4)) { return false; }
  const std::uint32_t len = static_cast<std::uint32_t>(lenraw[0]) |
                            (static_cast<std::uint32_t>(lenraw[1]) << 8) |
                            (static_cast<std::uint32_t>(lenraw[2]) << 16) |
                            (static_cast<std::uint32_t>(lenraw[3]) << 24);
  if (len > kMaxFrame) { return false; }
  payload.resize(len);
  if (len > 0 && !read_exact(payload.data(), len)) { return false; }
  return true;
}

bool TcpSocket::write_frame(const void* data, std::size_t n) {
  if (n > kMaxFrame) { return false; }
  const std::uint32_t len = static_cast<std::uint32_t>(n);
  std::uint8_t lenraw[4];
  lenraw[0] = static_cast<std::uint8_t>(len & 0xFF);
  lenraw[1] = static_cast<std::uint8_t>((len >> 8) & 0xFF);
  lenraw[2] = static_cast<std::uint8_t>((len >> 16) & 0xFF);
  lenraw[3] = static_cast<std::uint8_t>((len >> 24) & 0xFF);
  if (!write_all(lenraw, 4)) { return false; }
  if (n > 0 && !write_all(data, n)) { return false; }
  return true;
}

bool TcpSocket::peer_address(std::string& addr) const {
#ifdef _WIN32
  sockaddr_in addr_in{};
  int len = sizeof(addr_in);
  if (getpeername(fd_, reinterpret_cast<sockaddr*>(&addr_in), &len) != 0) { return false; }
  char buf[INET_ADDRSTRLEN] = {0};
  if (inet_ntop(AF_INET, &addr_in.sin_addr, buf, sizeof(buf)) == nullptr) { return false; }
  addr = buf;
  return true;
#else
  sockaddr_in addr_in{};
  socklen_t len = sizeof(addr_in);
  if (getpeername(fd_, reinterpret_cast<sockaddr*>(&addr_in), &len) != 0) { return false; }
  char buf[INET_ADDRSTRLEN] = {0};
  if (inet_ntop(AF_INET, &addr_in.sin_addr, buf, sizeof(buf)) == nullptr) { return false; }
  addr = buf;
  return true;
#endif
}

TcpListener::~TcpListener() {
  if (fd_ != invalid()) {
#ifdef _WIN32
    ::closesocket(fd_);
#else
    ::close(fd_);
#endif
    fd_ = invalid();
  }
}
TcpListener::TcpListener(TcpListener&& o) noexcept : fd_(o.fd_), port_(o.port_) { o.fd_ = invalid(); }
TcpListener& TcpListener::operator=(TcpListener&& o) noexcept {
  if (this != &o) {
    if (fd_ != invalid()) {
#ifdef _WIN32
      ::closesocket(fd_);
#else
      ::close(fd_);
#endif
    }
    fd_ = o.fd_; port_ = o.port_; o.fd_ = invalid();
  }
  return *this;
}

bool TcpListener::listen(const std::string& host, std::uint16_t port) {
  if (fd_ != invalid()) { return false; }
#ifdef _WIN32
  fd_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (fd_ == INVALID_SOCKET) { return false; }
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = inet_addr(host.c_str());
  if (::bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) { return false; }
  if (::listen(fd_, 16) != 0) { return false; }
#else
  fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd_ == -1) { return false; }
  int one = 1;
  setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = inet_addr(host.c_str());
  if (::bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) { return false; }
  if (::listen(fd_, 16) != 0) { return false; }
#endif
  port_ = port;
  return true;
}

TcpSocket TcpListener::accept() {
#ifdef _WIN32
  sockaddr_in addr{};
  int len = sizeof(addr);
  const SOCKET c = ::accept(fd_, reinterpret_cast<sockaddr*>(&addr), &len);
  return TcpSocket(c == INVALID_SOCKET ? invalid() : c);
#else
  sockaddr_in addr{};
  socklen_t len = sizeof(addr);
  const int c = ::accept(fd_, reinterpret_cast<sockaddr*>(&addr), &len);
  return TcpSocket(c == -1 ? invalid() : c);
#endif
}

TcpSocket tcp_connect(const std::string& host, std::uint16_t port) {
#ifdef _WIN32
  const SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s == INVALID_SOCKET) { return TcpSocket(); }
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = inet_addr(host.c_str());
  if (::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    ::closesocket(s);
    return TcpSocket();
  }
  return TcpSocket(s);
#else
  const int s = ::socket(AF_INET, SOCK_STREAM, 0);
  if (s == -1) { return TcpSocket(); }
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = inet_addr(host.c_str());
  if (::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    ::close(s);
    return TcpSocket();
  }
  return TcpSocket(s);
#endif
}

} // namespace mr
