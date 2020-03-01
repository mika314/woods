#include "peer.hpp"
#include "frame_size.hpp"
#include <net/perf.hpp>
#include <AudioStreamPlayer3D.hpp>

using namespace godot;

static int err;

Peer::Peer(uint64_t id, Node *aNode)
  : node(static_cast<RigidBody *>(aNode->duplicate())), dec(opus_decoder_create(48000, 2, &err))
{
  if (err != OPUS_OK)
    Godot::print(opus_strerror(err));
  node->set_name(std::to_string(id).c_str());
  node->set_visible(true);
  auto player = static_cast<AudioStreamPlayer3D *>(node->get_node("AudioStreamPlayer3D"));
  playback = static_cast<AudioStreamGeneratorPlayback *>(player->get_stream_playback().ptr());
}

Peer::~Peer()
{
  opus_decoder_destroy(dec);
}


RigidBody *Peer::getNode()
{
  return node;
}

void Peer::setState(const Woods::ClientState &state)
{
  Perf perf(__func__);
  Vector3 translation;
  translation.x = state.pos.x;
  translation.y = state.pos.y;
  translation.z = state.pos.z;

  node->set_translation(translation);
  Vector3 rotation;
  rotation.x = state.rot.x;
  rotation.y = state.rot.y;
  rotation.z = state.rot.z;
  node->set_rotation(rotation);
  for (const auto &audioFrame : state.audio)
  {
    std::array<opus_int16, FrameSize * 2> pcm;
    auto lenOrErr =
      opus_decode(dec, audioFrame.audio.data(), audioFrame.audio.size(), pcm.data(), FrameSize, 0);
    if (lenOrErr < 0)
    {
      Godot::print(opus_strerror(err));
      return;
    }
    audio.insert(std::end(audio), std::begin(pcm), std::end(pcm));

    auto max = *std::max_element(std::begin(pcm), std::end(pcm));
    auto s = node->get_scale();
    s.y = std::max(0.0f, 0.1f * logf(1.0f * max / 0x7fff + 0.0001f) + 1.0f);
    node->set_scale(s);
  }
}

void Peer::process()
{
  if (audio.empty())
    return;
  Perf perf(__func__);
  size_t cnt = 0u;
  for (auto to_fill = playback->get_frames_available(); to_fill > 0 && cnt < audio.size() / 2;
       --to_fill, ++cnt)
  {
    Vector2 v;
    v.x = 1.0f * audio[2 * cnt] / 0x7fff;
    v.y = 1.0f * audio[2 * cnt + 1] / 0x7fff;
    playback->push_frame(v);
  }
  audio.erase(std::begin(audio), std::begin(audio) + cnt * 2);
}
