#include "net_script.hpp"
#include "public_key.hpp"
#include <AudioServer.hpp>
#include <array>

void NetScript::_register_methods()
{
  register_method("_process", &NetScript::_process);
  register_method("_ready", &NetScript::_ready);
}

static int err;

using namespace godot;

NetScript::NetScript() : enc(opus_encoder_create(48000, 2, OPUS_APPLICATION_VOIP, &err))
{
  if (err != OPUS_OK)
    Godot::print(opus_strerror(err));
}

NetScript::~NetScript()
{
  opus_encoder_destroy(enc);
}

void NetScript::_init() {}

void NetScript::_ready()
{
  Godot::print("network native script is ready");
}

void NetScript::_process(float delta)
{
  if (!conn)
  {
    conn = std::make_unique<Net::Conn>(sched, PublicKey, "localhost", 42069);
    conn->onConn = [this]() {
      Godot::print("on connected");
      isConnected = true;
      auto audioServer = AudioServer::get_singleton();
      auto idx = audioServer->get_bus_index("Record");
      effect = static_cast<AudioEffectRecord *>(audioServer->get_bus_effect(idx, 0).ptr());
      effect->set_recording_active(true);
    };
    conn->onRecv = [this](const char *buff, size_t sz) {
      WoodsProto proto;
      IStrm strm(buff, buff + sz);
      proto.deser(strm, *this);
    };
    conn->onDisconn = [this]() {
      Godot::print("on disconnect");
      isConnected = false;
      if (effect)
        effect->set_recording_active(false);
    };
  }
  sched.processNoWait();
  if (!isConnected)
    return;

  auto player = static_cast<RigidBody *>(get_node("PlayerRigidBody"));
  Woods::ClientState state;
  auto translation = player->get_translation();
  state.pos.x = translation.x;
  state.pos.y = translation.y;
  state.pos.z = translation.z;
  auto rotation = player->get_rotation();
  state.rot.x = rotation.x;
  state.rot.y = rotation.y;
  state.rot.z = rotation.z;
  currentTime += delta;
  if (currentTime < 0.1)
  {
    OStrm strm;
    WoodsProto proto;
    proto.ser(strm, state);
    conn->send(strm.str().data(), strm.str().size());
    return;
  }
  currentTime -= 0.1;
  auto recording = effect->get_recording();
  if (!recording.ptr())
  {
    OStrm strm;
    WoodsProto proto;
    proto.ser(strm, state);
    conn->send(strm.str().data(), strm.str().size());
    return;
  }
  auto data = recording->get_data();
  effect->set_recording_active(false);
  effect->set_recording_active(true);
  std::ostringstream logStrm;
  logStrm << "Recording: ";
  switch (recording->get_format())
  {
  case AudioStreamSample::FORMAT_8_BITS: logStrm << "8 "; break;
  case AudioStreamSample::FORMAT_16_BITS: logStrm << "16 "; break;
  case AudioStreamSample::FORMAT_IMA_ADPCM: logStrm << "IMA_ADPCM "; break;
  }
  logStrm << recording->get_mix_rate() << " " << (recording->is_stereo() ? "stereo" : "mono") << " "
       << 1.0f * data.size() / sizeof(opus_int16) / 2 / recording->get_mix_rate();
  audioBuff.insert(std::end(audioBuff),
                   (opus_int16 *)data.read().ptr(),
                   (opus_int16 *)(data.read().ptr() + data.size()));
  const auto FrameSize = 960;
  std::array<uint8_t, FrameSize * sizeof(opus_int16) * 2> buff;
  while (audioBuff.size() > FrameSize * 2)
  {
    auto lenOrErr =
      opus_encode(enc, (opus_int16 *)audioBuff.data(), FrameSize, buff.data(), buff.size());
    audioBuff.erase(std::begin(audioBuff), std::begin(audioBuff) + FrameSize * 2);
    if (lenOrErr < 0)
    {
      Godot::print(opus_strerror(err));
      return;
    }
    state.audio.insert(std::end(state.audio), buff.data(), buff.data() + lenOrErr);
    logStrm << " " << lenOrErr;
  }
  Godot::print(logStrm.str().c_str());

  OStrm strm;
  WoodsProto proto;
  proto.ser(strm, state);
  conn->send(strm.str().data(), strm.str().size());
  return;
}

void NetScript::operator()(const Woods::PeersState &peers)
{
  auto player = static_cast<RigidBody *>(get_node("Player"));
  while (peers.size() != this->peers.size())
  {
    if (peers.size() > this->peers.size())
    {
      auto newPeer = static_cast<RigidBody *>(player->duplicate());
      newPeer->set_visible(true);
      add_child(newPeer);
      this->peers.push_back(newPeer);
      Godot::print("peer entered");
    }
    if (peers.size() < this->peers.size())
    {
      this->peers.back()->queue_free();
      this->peers.pop_back();
      Godot::print("peer left");
    }
  }
  size_t idx = 0;
  for (const auto &peer : peers)
  {
    auto thisPeer = this->peers[idx];
    Vector3 translation;
    translation.x = peer.pos.x;
    translation.y = peer.pos.y;
    translation.z = peer.pos.z;

    thisPeer->set_translation(translation);
    Vector3 rotation;
    rotation.x = peer.rot.x;
    rotation.y = peer.rot.y;
    rotation.z = peer.rot.z;
    thisPeer->set_rotation(rotation);
    ++idx;
  }
}

void NetScript::operator()(const Woods::ClientState &)
{
  Godot::print("Not expected: ClientState");
}
