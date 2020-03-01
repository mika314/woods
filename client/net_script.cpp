#include "net_script.hpp"
#include "frame_size.hpp"
#include "public_key.hpp"
#include <AudioServer.hpp>
#include <AudioStreamGeneratorPlayback.hpp>
#include <AudioStreamPlayer3D.hpp>
#include <CSGBox.hpp>
#include <array>
#include <net/perf.hpp>

void NetScript::_register_methods()
{
  register_method("_physics_process", &NetScript::_physics_process);
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

void NetScript::_physics_process(float delta)
{
  if (!conn)
  {
    conn = std::make_unique<Net::Conn>(sched, PublicKey, "localhost", 42069);
    conn->onConn = [this]() {
      Godot::print("on connected");
      isConnected = true;
      auto audioServer = AudioServer::get_singleton();
      auto idx = audioServer->get_bus_index("Record");
      auto effect = static_cast<AudioEffectRecord *>(audioServer->get_bus_effect(idx, 0).ptr());
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
      auto audioServer = AudioServer::get_singleton();
      auto idx = audioServer->get_bus_index("Record");
      if (auto effect = static_cast<AudioEffectRecord *>(audioServer->get_bus_effect(idx, 0).ptr()))
        effect->set_recording_active(false);
    };
  }
  sched.processNoWait();
  for (auto &peer : peers)
    peer.second.process();
  auto audioServer = AudioServer::get_singleton();
  auto idx = audioServer->get_bus_index("Record");
  auto effect = static_cast<AudioEffectRecord *>(audioServer->get_bus_effect(idx, 0).ptr());
  if (!effect)
    return;
  if (!isConnected)
    return;

  auto player = static_cast<RigidBody *>(get_node("PlayerRigidBody"));
  auto translation = player->get_translation();
  state.pos.x = translation.x;
  state.pos.y = translation.y;
  state.pos.z = translation.z;
  auto rotation = player->get_rotation();
  state.rot.x = rotation.x;
  state.rot.y = rotation.y;
  state.rot.z = rotation.z;
  currentTime += delta;
  const auto FrameDuarion = 0.1f;
  if (currentTime < FrameDuarion)
  {
    return;
  }
  currentTime -= FrameDuarion;
  auto recording = [&effect]() {
    Perf perf("getRecording");
    auto ret = effect->get_recording();
    effect->set_recording_active(false);
    effect->set_recording_active(true);
    return ret;
  }();
  if (!recording.ptr() || !effect->is_recording_active())
  {
    OStrm strm;
    WoodsProto proto;
    proto.ser(strm, state);
    if (conn->send(strm.str().data(), strm.str().size()))
      state.audio.clear();
    return;
  }

  std::ostringstream logStrm;
  logStrm << "Recording: ";
  switch (recording->get_format())
  {
  case AudioStreamSample::FORMAT_8_BITS: logStrm << "8 "; break;
  case AudioStreamSample::FORMAT_16_BITS: logStrm << "16 "; break;
  case AudioStreamSample::FORMAT_IMA_ADPCM: logStrm << "IMA_ADPCM "; break;
  }
  auto data = recording->get_data();

  logStrm << recording->get_mix_rate() << " " << (recording->is_stereo() ? "stereo" : "mono") << " "
          << 1.0f * data.size() / sizeof(opus_int16) / 2 / recording->get_mix_rate();
  auto volume = static_cast<CSGBox *>(get_node("PlayerRigidBody/Volume"));
  if (data.size() <= 0)
  {
    Godot::print("data empty");
    OStrm strm;
    WoodsProto proto;
    proto.ser(strm, state);
    if (conn->send(strm.str().data(), strm.str().size()))
      state.audio.clear();
    auto s = volume->get_scale();
    s.y = 0;
    volume->set_scale(s);
    return;
  }

  auto max = *std::max_element((opus_int16 *)data.read().ptr(),
                               (opus_int16 *)(data.read().ptr() + data.size()));

  auto s = volume->get_scale();
  s.y = std::max(0.0f, 0.1f * logf(1.0f * max / 0x7fff + 0.0001f) + 1.0f);
  volume->set_scale(s);

  audioBuff.insert(std::end(audioBuff),
                   (opus_int16 *)data.read().ptr(),
                   (opus_int16 *)(data.read().ptr() + data.size()));
  std::array<uint8_t, FrameSize * sizeof(opus_int16) * 2> buff;
  while (audioBuff.size() > FrameSize * 2)
  {
    auto lenOrErr =
      opus_encode(enc, audioBuff.data(), FrameSize, buff.data(), buff.size());
    if (lenOrErr < 0)
    {
      Godot::print(opus_strerror(err));
      OStrm strm;
      WoodsProto proto;
      proto.ser(strm, state);
      if (conn->send(strm.str().data(), strm.str().size()))
        state.audio.clear();
      return;
    }
    static auto id = 0;
    state.audio.emplace_back(id++, std::begin(buff), std::begin(buff) + lenOrErr);
    audioBuff.erase(std::begin(audioBuff), std::begin(audioBuff) + FrameSize * 2);

  }
  // Godot::print(logStrm.str().c_str());

  OStrm strm;
  WoodsProto proto;
  proto.ser(strm, state);
  if (conn->send(strm.str().data(), strm.str().size()))
    state.audio.clear();
  return;
}

void NetScript::operator()(const Woods::PeersState &netPeers)
{
  auto dummyPlayer = get_node("Player");

  for (const auto &netPeer : netPeers)
  {
    auto iter = peers.find(netPeer.id);
    if (iter == std::end(peers))
    {
      std::ostringstream strm;
      strm << "peer entered: " << netPeer.id;
      Godot::print(strm.str().c_str());
      iter = this->peers.try_emplace(netPeer.id, netPeer.id, dummyPlayer).first;
      add_child(iter->second.getNode());
    }
    auto &thisPeer = iter->second;
    thisPeer.setState(netPeer);
  }
}

void NetScript::operator()(const Woods::ClientState &)
{
  Godot::print("Not expected: ClientState");
}
