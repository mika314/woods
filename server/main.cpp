#include "private_key.hpp"
#include <iostream>
#include <log/log.hpp>
#include <net/conn.hpp>
#include <net/server.hpp>
#include <proto/proto.hpp>
#include <sched/sched.hpp>
#include <ser/overloaded.hpp>

int main()
{
  auto done = false;
  Sched sched;
  std::unordered_map<Net::Conn *, Woods::ClientState> clients;
  Net::Server server(sched, PrivateKey, 42069, [&clients, &done](Net::Conn *conn) {
    std::cout << "new connection: " << conn << std::endl;
    auto ret = clients.emplace(conn, Woods::ClientState{reinterpret_cast<std::uintptr_t>(conn)});
    auto &client = ret.first->second;
    conn->onRecv = [&client, conn, &clients](const char *buff, size_t sz) {
      WoodsProto proto;
      IStrm strm(buff, buff + sz);
      proto.deser(
        strm,
        overloaded{[&client, &clients, conn](const Woods::ClientState &state) {
                     auto tmp = state;
                     tmp.id = client.id;
                     if (!state.audio.empty())
                     {
                       for (auto &peer : clients)
                       {
                         if (peer.first == conn)
                           continue;
                         auto &audio = peer.second.audio;
                         audio.insert(std::end(audio), std::begin(tmp.audio), std::end(tmp.audio));
                       }
                     }
                     client.pos = tmp.pos;
                     client.rot = tmp.rot;
                   },
                   [](const Woods::PeersState &value) { LOG("Unexpected", typeid(value).name()); }});
    };
    conn->onDisconn = [conn, &clients, &done] {
      LOG("Peer", conn, "is disconnected");
      clients.erase(conn);
      if (clients.empty())
      {
        LOG("Exiting");
        done = true;
      }
    };
  });

  auto updateTimer = sched.regTimer(
    [&clients]() {
      for (auto &client : clients)
      {
        Woods::PeersState peersState;
        for (const auto &peer : clients)
        {
          if (peer.first == client.first)
            continue;
          peersState.push_back(peer.second);
        }
        WoodsProto proto;
        OStrm strm;
        proto.ser(strm, peersState);

        if (client.first->send(strm.str().data(), strm.str().size()))
        {
          for (auto &peer : clients)
          {
            if (peer.first == client.first)
              continue;
            peer.second.audio.clear();
          }
        }
      }
    },
    std::chrono::milliseconds{1000 / 100},
    true);
  while (!done)
    sched.process();
}
