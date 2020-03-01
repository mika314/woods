#pragma once
#include <AudioStreamGeneratorPlayback.hpp>
#include <RigidBody.hpp>
#include <opus/opus.h>
#include <proto/proto.hpp>

class Peer
{
public:
  Peer(uint64_t id, godot::Node *);
  Peer(const Peer &) = delete;
  ~Peer();
  godot::RigidBody *getNode();
  void setState(const Woods::ClientState &);
  void process();

private:
  godot::RigidBody *node;
  godot::AudioStreamGeneratorPlayback *playback;
  std::vector<int16_t> audio;
  OpusDecoder *dec{nullptr};
};
