#include "conn.hpp"
#include "perf.hpp"
#include <cstring>
#include <iomanip>
#include <iostream>
#include <log/log.hpp>
#include <sched/sched.hpp>
#include <sstream>
#define LTM_DESC
#include <tomcrypt.h>

#if 0
template <typename Iter>
static void dump(Iter b, Iter e)
{
  std::cout << std::setbase(16);
  int cnt{0};
  std::for_each(b, e, [&cnt](auto x) {
    if (cnt++ % 16 == 0)
      std::cout << std::endl;
    std::cout << std::setw(3) << (unsigned)(unsigned char)x;
  });
  std::cout << std::setbase(10) << std::endl;
}
#else
template <typename Iter>
static void dump(Iter, Iter)
{
}
#endif

namespace Net
{
  namespace Internal
  {
    struct Conn
    {
      rsa_key rsaKey{};
      chacha_state chachaRecv{};
      chacha_state chachaSend{};
    };
  } // namespace Internal

  Conn::Conn(Sched &sched, const RsaPublicKey &publicKey, const std::string &host, int port)
    : sched(sched), internal(std::make_unique<Internal::Conn>())
  {
    importKey(publicKey.data(), publicKey.size());
    uv_tcp_init(&sched.loop, &socket);
    struct sockaddr_in dest;
    uv_ip4_addr(host.c_str(), port, &dest);
    connect.data = this;
    socket.data = this;

    uv_tcp_connect(
      &connect, &socket, (const struct sockaddr *)&dest, [](uv_connect_t *req, int status) {
        Conn *conn = static_cast<Conn *>(req->data);
        if (status < 0)
        {
          LOG("connect failed:", status);
          if (conn->onDisconn)
            conn->onDisconn();
          return;
        }

        conn->readStart();
        conn->sendRandKey();
      });
  }

  Conn::Conn(Sched &sched,
             const RsaPrivateKey &privateKey,
             std::function<int(uv_stream_t &)> &&accept)
    : sched(sched), internal(std::make_unique<Internal::Conn>())
  {
    importKey(privateKey.data(), privateKey.size());
    uv_tcp_init(&sched.loop, &socket);
    const auto r = accept((uv_stream_t &)socket);
    if (r < 0)
    {
      LOG(this, "accept error:", r);
      if (onDisconn)
        onDisconn();
      return;
    }
    readStart();
  }

  Conn::~Conn()
  {
    auto done{false};
    uv_shutdown_t req;
    req.data = &done;
    uv_shutdown(&req, (uv_stream_t *)&socket, [](uv_shutdown_t *req, int status) {
      if (status < 0)
        LOG("shutdown failed");
      auto done = static_cast<bool *>(req->data);
      *done = true;
    });
    while (!done)
      sched.get().process();
  }

  auto Conn::readStart() -> void
  {
    socket.data = this;
    const auto err = uv_read_start((uv_stream_t *)&socket,
                                   [](uv_handle_t *handle, size_t suggested_size, uv_buf_t *buff) {
                                     auto conn = static_cast<Conn *>(handle->data);
                                     conn->inBuff.resize(4096);
                                     buff->base = conn->inBuff.data();
                                     buff->len = std::min(conn->inBuff.size(), suggested_size);
                                   },
                                   [](uv_stream_t *stream, ssize_t nread, const uv_buf_t *buff) {
                                     auto conn = static_cast<Conn *>(stream->data);
                                     conn->onRead(nread, buff->base);
                                   });
    if (err < 0)
      LOG(this, "uv_read_start error:", err);
  }

  auto Conn::importKey(const unsigned char *key, int keySize) -> void
  {
    static struct RngHash
    {
      RngHash()
      {
        ltc_mp = ltm_desc;
        if (register_prng(&sprng_desc) == -1)
        {
          LOG(this, "Error registering sprng");
          return;
        }
        if (register_hash(&sha1_desc) == -1)
        {
          LOG(this, "Error registering sha1");
          return;
        }
      }
    } rngHash;

    const auto err = rsa_import(key, keySize, &internal->rsaKey);
    if (err != CRYPT_OK)
    {
      std::cerr << "import rsa key error:" << error_to_string(err);
      return;
    }
  }

  auto Conn::sendRandKey() -> void
  {
    const auto hash_idx = find_hash("sha1");
    const auto prng_idx = find_prng("sprng");
    key.emplace();
    rng_get_bytes(key->data(), key->size(), nullptr);
    outBuff.resize(1024);
    unsigned long l1 = outBuff.size();
    const auto err = rsa_encrypt_key(key->data(),
                                     key->size(),
                                     (unsigned char *)outBuff.data(),
                                     &l1,
                                     (unsigned char *)"irl",
                                     3,
                                     nullptr,
                                     prng_idx,
                                     hash_idx,
                                     &internal->rsaKey);
    if (err != CRYPT_OK)
    {
      LOG(this, "rsa_encrypt_key %s", error_to_string(err));
      return;
    }
    outBuff.resize(l1);

    req.data = this;
    uv_buf_t buffs[2];
    const int32_t sz = outBuff.size();
    buffs[0].base = (char *)&sz;
    buffs[0].len = sizeof(sz);
    LOG(this, "message size", sz);
    buffs[1].base = outBuff.data();
    buffs[1].len = outBuff.size();
    uv_write(&req, (uv_stream_t *)&socket, buffs, 2, [](uv_write_t *req, int status) {
      LOG("key is sent", req->data, status);
      Conn *conn = static_cast<Conn *>(req->data);
      if (conn->onConn)
        conn->onConn();
    });
    setupChacha();
  }

  auto Conn::decryptKey() -> void
  {
    const auto hash_idx = find_hash("sha1");
    key.emplace();
    unsigned long l1 = sizeof(*key);
    int stat{0};
    const auto err = rsa_decrypt_key((unsigned char *)packet.data(),
                                     packet.size(),
                                     key->data(),
                                     &l1,
                                     (unsigned char *)"irl",
                                     3,
                                     hash_idx,
                                     &stat,
                                     &internal->rsaKey);
    if (err != CRYPT_OK)
    {
      LOG(this, "rsa_decrypt_key:", error_to_string(err));
      disconn();
      return;
    }
    if (stat == 0)
    {
      LOG(this, "rsa_decrypt_key: invalid key");
      disconn();
      return;
    }
    LOG(this, "key is: ", std::string(std::begin(*key), std::end(*key)));
  }

  auto Conn::disconn() -> void
  {
    if (uv_is_closing((uv_handle_t *)&socket))
      return;
    uv_close((uv_handle_t *)&socket, [](uv_handle_t *ctx) {
      Conn *conn = static_cast<Conn *>(ctx->data);
      if (conn->onDisconn)
        conn->onDisconn();
    });
  }

  auto Conn::onRead(int nread, const char *encBuff) -> void
  {
    Perf perf(__func__);
    if (nread == UV_EOF || nread < 0)
    {
      if (onDisconn)
        onDisconn();
      return;
    }

    char buff[4096];
    auto isDecoded{false};
    dump(encBuff, encBuff + nread);
    if (key)
    {
      auto err = chacha_crypt(
        &internal->chachaRecv, (const unsigned char *)encBuff, nread, (unsigned char *)buff);
      if (err != CRYPT_OK)
      {
        LOG(this, "Error:", error_to_string(err));
        disconn();
        return;
      }
      isDecoded = true;
    }
    else
      std::copy(encBuff, encBuff + nread, buff);
    dump(buff, buff + nread);

    int idx = 0;
    while (idx < nread)
    {
      if (remining == 0)
      {
        if (nread - idx < static_cast<int>(sizeof(int32_t)))
        {
          LOG(this, "Other side of connection is misbehaving:", nread - idx, "<", sizeof(int32_t));
          disconn();
          return;
        }
        if (key && !isDecoded)
        {
          const auto err = chacha_crypt(&internal->chachaRecv,
                                        (const unsigned char *)(buff + idx),
                                        nread - idx,
                                        (unsigned char *)(buff + idx));
          if (err != CRYPT_OK)
          {
            LOG(this, "Error:", error_to_string(err));
            disconn();
            return;
          }
        }
        int32_t sz = *(int32_t *)(&buff[idx]);
        idx += sizeof(sz);
        if (sz > 2 * 1024 * 1024)
        {
          LOG(this, "Packed is too big:", sz);
          disconn();
          return;
        }
        if (sz < 0)
        {
          LOG(this, "Packed has negative size:", sz);
          disconn();
          return;
        }
        remining = sz;
        packet.clear();
      }
      const auto tmpSz = std::min(remining, nread - idx);
      if (tmpSz == 0)
        break;
      const auto tmpIdx = packet.size();
      packet.resize(packet.size() + tmpSz);
      std::copy(buff + idx, buff + idx + tmpSz, packet.data() + tmpIdx);
      idx += tmpSz;
      remining -= tmpSz;
      if (remining == 0)
      {
        if (!key)
        {
          decryptKey();
          setupChacha();
          if (onConn)
            onConn();
        }
        else if (onRecv)
          onRecv(packet.data(), packet.size());
      }
    }
  }

  auto Conn::setupChacha() -> void
  {
    assert(key);
    auto setup = [&](chacha_state &chacha) {
      {
        const auto err = chacha_setup(&chacha, key->data(), key->size(), 20);
        if (err != CRYPT_OK)
        {
          LOG("Setup chacha error:", error_to_string(err));
          disconn();
          return;
        }
      }
      {
        const std::array<unsigned char, 12> nonce = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
        const auto err = chacha_ivctr32(&chacha, nonce.data(), nonce.size(), 0);
        if (err != CRYPT_OK)
        {
          LOG("Setup chacha error:", error_to_string(err));
          disconn();
          return;
        }
      }
    };
    setup(internal->chachaSend);
    setup(internal->chachaRecv);
  }

  auto Conn::send(const char *buff, size_t size) -> bool
  {
    if (isSending)
      return false;
    Perf perf(__func__);
    const int32_t sz = size;
    tmpBuff.resize(sizeof(sz) + size);
    std::copy((const char *)&sz, (const char *)&sz + sizeof(sz), std::begin(tmpBuff));
    std::copy(buff, buff + size, std::begin(tmpBuff) + sizeof(sz));
    dump(std::begin(tmpBuff), std::end(tmpBuff));
    outBuff.resize(tmpBuff.size());
    const auto err = chacha_crypt(&internal->chachaSend,
                                  (const unsigned char *)tmpBuff.data(),
                                  tmpBuff.size(),
                                  (unsigned char *)outBuff.data());
    if (err != CRYPT_OK)
    {
      LOG(this, "Error:", error_to_string(err));
      disconn();
      return true;
    }
    uv_buf_t buffs[1];
    buffs[0].base = outBuff.data();
    buffs[0].len = outBuff.size();
    req.data = this;
    dump(std::begin(outBuff), std::end(outBuff));
    isSending = true;
    uv_write(&req, (uv_stream_t *)&socket, buffs, 1, [](uv_write_t *req, int status) {
      auto conn = static_cast<Conn *>(req->data);
      if (status != 0)
      {
        LOG("Sent error:", status);
        conn->disconn();
      }
      conn->isSending = false;
    });
    return true;
  }
} // namespace Net
