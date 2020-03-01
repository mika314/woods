#pragma once
#include <ser/macro.hpp>
#include <ser/proto.hpp>

namespace Woods
{
  struct Vec3
  {
    float x{};
    float y{};
    float z{};
#define SER_PROPERTY_LIST \
  SER_PROPERTY(x);        \
  SER_PROPERTY(y);        \
  SER_PROPERTY(z);
    SER_DEFINE_PROPERTIES()
#undef SER_PROPERTY_LIST
  };

  struct AudioFrame
  {
    AudioFrame() = default;
    template <typename Iter>
    AudioFrame(int id, Iter beg , Iter end) : id(id), audio(beg, end)
    {
    }
    int id{-1};
    std::vector<unsigned char> audio;
#define SER_PROPERTY_LIST \
  SER_PROPERTY(id);       \
  SER_PROPERTY(audio);
    SER_DEFINE_PROPERTIES()
#undef SER_PROPERTY_LIST
  };

  struct ClientState
  {
    ClientState(uint64_t id = 0) : id(id) {}
    uint64_t id{};
    Vec3 pos;
    Vec3 rot;
    std::vector<AudioFrame> audio;
#define SER_PROPERTY_LIST \
  SER_PROPERTY(id);       \
  SER_PROPERTY(pos);      \
  SER_PROPERTY(rot);      \
  SER_PROPERTY(audio);
    SER_DEFINE_PROPERTIES()
#undef SER_PROPERTY_LIST
  };

  using PeersState = std::vector<ClientState>;
} // namespace Woods

using WoodsProto = Proto<Woods::ClientState, Woods::PeersState>;
