#pragma once
#include "rsa_key.hpp"
#include <functional>
#include <memory>
#include <unordered_map>
#include <uv.h>

class Sched;
namespace Net
{
  class Conn;
  class Server
  {
  public:
    Server(Sched &,
           const RsaPrivateKey &privateKey,
           const int port,
           std::function<void(Conn *)> &&onConn);
    ~Server();
    Server(const Server &) = delete;
    Server &operator=(const Server &) = delete;

  private:
    Sched *sched;
    uv_tcp_t server;
    const RsaPrivateKey *privateKey;
    std::function<void(Conn *)> onConn;
    std::unordered_map<Conn *, std::unique_ptr<Conn>> conns;

    auto onNewConn(const int status) -> void;
  };
} // namespace Net
