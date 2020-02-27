#pragma once
#include "rsa_key.hpp"
#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <uv.h>
#include <vector>

class Sched;
namespace Net
{
  namespace Internal
  {
    struct Conn;
  }

  class Conn
  {
  public:
    Conn(Sched &, const RsaPublicKey &publicKey, const std::string &host, int port);
    Conn(Sched &, const RsaPrivateKey &privateKey, std::function<int(uv_stream_t &)> &&accept);
    Conn(const Conn &) = delete;
    Conn &operator=(const Conn &) = delete;
    ~Conn();
    auto send(const char *buff, size_t size) -> bool;
    std::function<void(const char *buff, size_t)> onRecv{nullptr};
    std::function<void()> onConn{nullptr};
    std::function<void()> onDisconn{nullptr};

  private:
    std::reference_wrapper<Sched> sched;
    uv_tcp_t socket{};
    uv_connect_t connect{};
    std::optional<std::array<unsigned char, 256 / 8>> key{};
    std::unique_ptr<Internal::Conn> internal{};
    std::vector<char> inBuff{};
    std::vector<char> outBuff{};
    std::vector<char> tmpBuff{};
    std::vector<char> packet{};
    int remining{0};
    uv_write_t req{};
    bool isSending{false};

    auto readStart() -> void;
    auto sendRandKey() -> void;
    auto importKey(const unsigned char *key, int keySize) -> void;
    auto decryptKey() -> void;
    auto onRead(int size, const char *buff) -> void;
    auto setupChacha() -> void;
    auto disconn() -> void;
  };
} // namespace Net
