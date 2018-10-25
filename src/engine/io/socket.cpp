#include <engine/io/socket.hpp>

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <string>
#include <system_error>

#include <engine/task/task.hpp>
#include <logging/log.hpp>

#include <build_config.hpp>
#include <engine/io/error.hpp>
#include <engine/task/task_context.hpp>
#include <utils/check_syscall.hpp>

namespace engine {
namespace io {
namespace {

// MAC_COMPAT
Socket MakeSocket(const Addr& addr) {
  return Socket(utils::CheckSyscall(::socket(addr.Family(),
#ifdef SOCK_NONBLOCK
                                             addr.Type() | SOCK_NONBLOCK,
#else
                                             addr.Type(),
#endif
                                             addr.Protocol()),
                                    "creating socket, addr=", addr));
}

template <typename... Context>
Addr& MemoizeAddr(Addr& addr, decltype(&::getpeername) getter,
                  const Socket& socket, const Context&... context) {
  if (addr.Domain() == AddrDomain::kInvalid) {
    AddrStorage buf;
    auto len = buf.Size();
    utils::CheckSyscall(getter(socket.Fd(), buf.Data(), &len),
                        utils::impl::ToString(context...));
    assert(len <= buf.Size());
    addr = Addr(buf, 0, 0);
  }
  return addr;
}

}  // namespace

ConnectTimeout::ConnectTimeout()
    : IoError("connection establishment timed out") {}

Socket::Socket(int fd) : fd_control_(impl::FdControl::Adopt(fd)) {}

bool Socket::IsOpen() const { return !!fd_control_; }

void Socket::WaitReadable(Deadline deadline) {
  assert(IsOpen());
  fd_control_->Read().Wait(std::move(deadline));
}

void Socket::WaitWriteable(Deadline deadline) {
  assert(IsOpen());
  fd_control_->Write().Wait(std::move(deadline));
}

size_t Socket::RecvSome(void* buf, size_t len, Deadline deadline) {
  if (!IsOpen()) {
    throw IoError("Attempt to Recv from closed socket");
  }
  auto& dir = fd_control_->Read();
  impl::Direction::Lock lock(dir);
  return dir.PerformIo(lock, &::read, buf, len, impl::TransferMode::kPartial,
                       std::move(deadline), "Recv from ", peername_);
}

size_t Socket::RecvAll(void* buf, size_t len, Deadline deadline) {
  if (!IsOpen()) {
    throw IoError("Attempt to RecvAll from closed socket");
  }
  auto& dir = fd_control_->Read();
  impl::Direction::Lock lock(dir);
  return dir.PerformIo(lock, &::read, buf, len, impl::TransferMode::kWhole,
                       std::move(deadline), "RecvAll from ", peername_);
}

size_t Socket::SendAll(const void* buf, size_t len, Deadline deadline) {
  if (!IsOpen()) {
    throw IoError("Attempt to Send to closed socket");
  }
  auto& dir = fd_control_->Write();
  impl::Direction::Lock lock(dir);

// MAC_COMPAT
#ifdef MSG_NOSIGNAL
  static const auto send_func = [](int fd, const void* buf, size_t len) {
    return ::send(fd, buf, len, MSG_NOSIGNAL);
  };
#else
  static const auto send_func = &::write;
#endif

  return dir.PerformIo(lock, send_func, const_cast<void*>(buf), len,
                       impl::TransferMode::kWhole, std::move(deadline),
                       "Send to ", peername_);
}

Socket Socket::Accept(Deadline deadline) {
  if (!IsOpen()) {
    throw IoError("Attempt to Accept from closed socket");
  }
  current_task::CancellationPoint();

  auto& dir = fd_control_->Read();
  impl::Direction::Lock lock(dir);
  for (;;) {
    AddrStorage buf;
    auto len = buf.Size();

// MAC_COMPAT
#ifdef HAVE_ACCEPT4
    int fd = ::accept4(dir.Fd(), buf.Data(), &len, SOCK_NONBLOCK);
#else
    int fd = ::accept(dir.Fd(), buf.Data(), &len);
#endif

    assert(len <= buf.Size());
    if (fd != -1) {
      auto peersock = Socket(fd);
      peersock.peername_ = Addr(buf, 0, 0);
      return peersock;
    }

    switch (errno) {
      case EAGAIN:
#if EAGAIN != EWOULDBLOCK
      case EWOULDBLOCK:
#endif
        WaitReadable(deadline);
        if (current_task::GetCurrentTaskContext()->GetWakeupSource() ==
            engine::impl::TaskContext::WakeupSource::kDeadlineTimer) {
          throw ConnectTimeout();
        }
        break;

      case ECONNABORTED:  // DOA connection
      case EINTR:         // signal interrupt
      // TCP/IP network errors
      case ENETDOWN:
      case EPROTO:
      case ENOPROTOOPT:
      case EHOSTDOWN:
      case ENONET:
      case EHOSTUNREACH:
      case EOPNOTSUPP:
      case ENETUNREACH:
        // retry accept()
        break;

      default:
        utils::CheckSyscall(-1, "accepting a connection");
    }
  }
}

void Socket::Close() { fd_control_.reset(); }

int Socket::Fd() const { return fd_control_ ? fd_control_->Fd() : kInvalidFd; }

const Addr& Socket::Getpeername() {
  assert(IsOpen());
  return MemoizeAddr(peername_, &::getpeername, *this,
                     "getting peer name, fd=", Fd());
}

const Addr& Socket::Getsockname() {
  assert(IsOpen());
  return MemoizeAddr(sockname_, &::getsockname, *this,
                     "getting socket name, fd=", Fd());
}

int Socket::Release() && noexcept {
  const int fd = Fd();
  if (IsOpen()) fd_control_.release()->Invalidate();
  return fd;
}

Socket Connect(Addr addr, Deadline deadline) {
  current_task::CancellationPoint();

  auto socket = MakeSocket(addr);

  int err_value = ::connect(socket.Fd(), addr.Sockaddr(), addr.Addrlen());
  if (!err_value) {
    return socket;
  }
  err_value = errno;
  if (err_value == EINPROGRESS) {
    socket.WaitWriteable(deadline);
    if (current_task::GetCurrentTaskContext()->GetWakeupSource() ==
        engine::impl::TaskContext::WakeupSource::kDeadlineTimer) {
      throw ConnectTimeout();
    }
    err_value = socket.GetOption(SOL_SOCKET, SO_ERROR);
  }

  if (err_value) {
    throw std::system_error(
        err_value, std::system_category(),
        utils::impl::ToString("Error while establishing connection, fd=",
                              socket.Fd(), ", addr=", addr));
  }
  return socket;
}

Socket Listen(Addr addr, int backlog) {
  current_task::CancellationPoint();

  auto socket = MakeSocket(addr);

  socket.SetOption(SOL_SOCKET, SO_REUSEADDR, 1);

// MAC_COMPAT
#ifdef SO_REUSEPORT
  socket.SetOption(SOL_SOCKET, SO_REUSEPORT, 1);
#else
  LOG_ERROR() << "SO_REUSEPORT is not defined, you may experience problems "
                 "with multithreaded listeners";
#endif

  utils::CheckSyscall(::bind(socket.Fd(), addr.Sockaddr(), addr.Addrlen()),
                      "binding a socket, addr=", addr);
  utils::CheckSyscall(::listen(socket.Fd(), backlog),
                      "listening on a socket, addr=", addr,
                      ", backlog=", backlog);
  return socket;
}

int Socket::GetOption(int layer, int optname) const {
  assert(IsOpen());
  int value = -1;
  socklen_t value_len = sizeof(value);
  utils::CheckSyscall(::getsockopt(Fd(), layer, optname, &value, &value_len),
                      "getting socket option ", layer, ',', optname, "on fd ",
                      Fd());
  assert(value_len == sizeof(value));
  return value;
}

void Socket::SetOption(int layer, int optname, int optval) {
  assert(IsOpen());
  utils::CheckSyscall(
      ::setsockopt(Fd(), layer, optname, &optval, sizeof(optval)),
      "setting socket option ", layer, ',', optname, " to ", optval, " on fd ",
      Fd());
}

}  // namespace io
}  // namespace engine
