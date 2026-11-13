#pragma once
#include "audio/sfx.hpp"
namespace ew::audio {
// Intenta abrir un dispositivo (miniaudio). false = modo silencioso (headless,
// --nosound, EW_NOSOUND, o sin hardware). Nunca aborta.
bool device_start(Mixer& mixer);
void device_stop();
// Encola un beep; no-op si el dispositivo no arrancó (el mixer igual se puede usar).
void device_play(SfxId id);
}  // namespace ew::audio
