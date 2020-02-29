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

  struct ClientState
  {
    ClientState(uint64_t id = 0) : id(id) {}
    uint64_t id{};
    Vec3 pos;
    Vec3 rot;
    using AudioFrame = std::vector<int16_t>;
    std::vector<AudioFrame> audio;
#define SER_PROPERTY_LIST \
  SER_PROPERTY(id);       \
  SER_PROPERTY(pos);      \
  SER_PROPERTY(rot);      \
  SER_PROPERTY(audio);
    SER_DEFINE_PROPERTIES()
#undef SER_PROPERTY_LIST
  };

  struct Quit
  {
#define SER_PROPERTY_LIST
    SER_DEFINE_PROPERTIES()
#undef SER_PROPERTY_LIST
  };

  using PeersState = std::vector<ClientState>;
} // namespace Woods

using WoodsProto = Proto<Woods::ClientState, Woods::PeersState, Woods::Quit>;
