#include "server.hpp"
#include "conn.hpp"
#include <log/log.hpp>
#include <sched/sched.hpp>

namespace Net
{
  Server::Server(Sched &sched,
                 const RsaPrivateKey &privateKey,
                 const int port,
                 std::function<void(Conn *)> &&onConn)
    : sched(&sched), privateKey(&privateKey), onConn(std::move(onConn))
  {
    uv_tcp_init(&sched.loop, &server);
    server.data = this;
    struct sockaddr_in addr;
    uv_ip4_addr("0.0.0.0", port, &addr);

    uv_tcp_bind(&server, (const struct sockaddr *)&addr, 0);
    const auto r = uv_listen((uv_stream_t *)&server, 5, [](uv_stream_t *server, int status) {
      static_cast<Server *>(server->data)->onNewConn(status);
    });
    if (r)
    {
      LOG("Listen error %s\n", uv_strerror(r));
      return;
    }
  }

  Server::~Server() {}

  void Server::onNewConn(const int status)
  {
    LOG(__func__, status);
    if (status < 0)
    {
      LOG(__func__, status);
      return;
    }
    auto conn = std::make_unique<Conn>(*sched, *privateKey, [this](uv_stream_t &client) {
      return uv_accept((uv_stream_t *)&server, &client);
    });
    auto ptr = conn.get();
    conn->onConn = [this, ptr]() { onConn(ptr); };
    conn->onDisconn = [this, ptr]() { conns.erase(ptr); };
    conns[ptr] = std::move(conn);
  }
} // namespace Net
