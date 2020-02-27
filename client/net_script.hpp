#pragma once
#include "peer.hpp"
#include <AudioEffectRecord.hpp>
#include <Godot.hpp>
#include <RigidBody.hpp>
#include <Spatial.hpp>
#include <cstdint>
#include <net/conn.hpp>
#include <opus/opus.h>
#include <proto/proto.hpp>
#include <sched/sched.hpp>

class NetScript : public godot::Spatial
{
  GODOT_CLASS(NetScript, Spatial)
public:
  static void _register_methods();
  NetScript();
  ~NetScript();
  void _init();
  void _ready();
  void _physics_process(float delta);

  void operator()(const Woods::PeersState &);
  void operator()(const Woods::ClientState &);

private:
  Sched sched;
  std::unique_ptr<Net::Conn> conn;
  bool isConnected{false};
  std::unordered_map<uint64_t, Peer> peers;
  OpusEncoder *enc{nullptr};
  float currentTime{-1.0f};
  std::vector<opus_int16> audioBuff;
  Woods::ClientState state;
};
