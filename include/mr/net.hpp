#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "mr/config.hpp"

#ifdef _WIN32
#  define NOMINMAX
#  ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#    define _WINSOCK_DEPRECATED_NO_WARNINGS
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <netdb.h>
#  include <sys/socket.h>
#  include <unistd.h>
#endif

namespace mr {

#ifdef _WIN32
using socket_native = SOCKET;
#else
using socket_native = int;
#endif

/// RAII network library initializer (WSAStartup on Windows). Call once before
/// any networking.
class NetInit {
 public:
  NetInit();
  ~NetInit();
  NetInit(const NetInit&) = delete;
  NetInit& operator=(const NetInit&) = delete;
  [[nodiscard]] bool ok() const noexcept { return ok_; }
  bool ok_ = false;
};

/// A move-only TCP stream socket.
class TcpSocket {
 public:
  TcpSocket() = default;
  explicit TcpSocket(socket_native fd) : fd_(fd) {}
  ~TcpSocket();
  TcpSocket(TcpSocket&& o) noexcept;
  TcpSocket& operator=(TcpSocket&& o) noexcept;
  TcpSocket(const TcpSocket&) = delete;
  TcpSocket& operator=(const TcpSocket&) = delete;

  void close() noexcept;
  [[nodiscard]] bool valid() const noexcept { return fd_ != invalid_fd(); }

  /// Read exactly n bytes, writing into out. Returns false on EOF/error. Blocks
  /// until all bytes arrive; there is no timeout by design (tests run to
  /// natural completion).
  bool read_exact(void* out, std::size_t n);
  /// Write exactly n bytes. Returns false on error.
  bool write_all(const void* data, std::size_t n);

  // Read a length-prefixed frame: 4-byte little-endian length then payload.
  bool read_frame(std::vector<std::uint8_t>& payload);
  bool write_frame(const void* payload, std::size_t n);

  [[nodiscard]] bool peer_address(std::string& addr) const;

  [[nodiscard]] socket_native native() const noexcept { return fd_; }

 private:
  static socket_native invalid_fd() noexcept;
  socket_native fd_ = invalid_fd();
};

/// A listening TCP socket (Windows-first).
class TcpListener {
 public:
  TcpListener() = default;
  ~TcpListener();
  TcpListener(TcpListener&& o) noexcept;
  TcpListener& operator=(TcpListener&& o) noexcept;
  TcpListener(const TcpListener&) = delete;
  TcpListener& operator=(const TcpListener&) = delete;

  /// Bind and listen on the given loopback port; returns success.
  bool listen(const std::string& host, std::uint16_t port);
  /// Accept an incoming connection (blocks).
  TcpSocket accept();

  [[nodiscard]] bool valid() const noexcept { return fd_ != invalid_fd(); }
  [[nodiscard]] std::uint16_t port() const noexcept { return port_; }

 private:
  static socket_native invalid_fd() noexcept;
  socket_native fd_ = invalid_fd();
  std::uint16_t port_ = 0;
};

/// Connect to a TCP endpoint.
TcpSocket tcp_connect(const std::string& host, std::uint16_t port);

} // namespace mr