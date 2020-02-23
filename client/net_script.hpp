#pragma once
#include <Godot.hpp>
#include <RigidBody.hpp>
#include <Spatial.hpp>
#include <net/conn.hpp>
#include <proto/proto.hpp>
#include <sched/sched.hpp>
#include <opus/opus.h>
#include <AudioEffectRecord.hpp>

class NetScript : public godot::Spatial
{
  GODOT_CLASS(NetScript, Spatial)
public:
  static void _register_methods();
  NetScript();
  ~NetScript();
  void _init();
  void _ready();
  void _process(float delta);

  void operator()(const Woods::PeersState &);
  void operator()(const Woods::ClientState &);

private:
  Sched sched;
  std::unique_ptr<Net::Conn> conn;
  bool isConnected{false};
  std::vector<godot::RigidBody *> peers;
  OpusEncoder *enc{nullptr};
  godot::AudioEffectRecord *effect{nullptr};
  float currentTime{0};
  std::vector<opus_int16> audioBuff;
};
