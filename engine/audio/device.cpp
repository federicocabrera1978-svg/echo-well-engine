#include "audio/device.hpp"
#ifdef EW_AUDIO
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <mutex>
#endif
#include <cstdio>

namespace ew::audio {
#ifdef EW_AUDIO
namespace {
std::mutex g_mx;
Mixer* g_mix = nullptr;
ma_device g_dev{};
bool g_on = false;

void data_cb(ma_device*, void* out, const void*, ma_uint32 frames) {
  auto* f = static_cast<float*>(out);
  std::lock_guard<std::mutex> lk(g_mx);
  if (g_mix) g_mix->render(f, int(frames), int(g_dev.sampleRate ? g_dev.sampleRate : kRate));
  else for (ma_uint32 i = 0; i < frames; ++i) f[i] = 0;
}
}  // namespace

bool device_start(Mixer& mixer) {
  g_mix = &mixer;
  ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
  cfg.playback.format = ma_format_f32;
  cfg.playback.channels = 1;
  cfg.sampleRate = kRate;
  cfg.dataCallback = data_cb;
  cfg.periodSizeInFrames = 512;
  if (ma_device_init(nullptr, &cfg, &g_dev) != MA_SUCCESS) {
    std::fprintf(stderr, "echo_engine: sin dispositivo de audio (SFX en silencio)\n");
    g_on = false;
    return false;
  }
  if (ma_device_start(&g_dev) != MA_SUCCESS) {
    ma_device_uninit(&g_dev);
    std::fprintf(stderr, "echo_engine: no se pudo iniciar el audio (SFX en silencio)\n");
    g_on = false;
    return false;
  }
  g_on = true;
  return true;
}

void device_stop() {
  if (g_on) {
    ma_device_uninit(&g_dev);
    g_on = false;
  }
  std::lock_guard<std::mutex> lk(g_mx);
  g_mix = nullptr;
}

void device_play(SfxId id) {
  std::lock_guard<std::mutex> lk(g_mx);
  if (g_mix) g_mix->play(id);
}
#else
bool device_start(Mixer&) { return false; }
void device_stop() {}
void device_play(SfxId) {}
#endif
}  // namespace ew::audio
