#pragma once
#include "audio/sfx.hpp"
namespace ew::audio {
// Intenta abrir un dispositivo (miniaudio). false = modo silencioso (headless,
// --nosound, EW_NOSOUND, o sin hardware). Nunca aborta.
bool device_start(Mixer& mixer);
void device_stop();
// Encola un beep; no-op si el dispositivo no arrancó (el mixer igual se puede usar).
void device_play(SfxId id);
// Fija la zona musical activa del mixer (thread-safe; no-op sin dispositivo).
void device_set_music_zone(MusicZone z);
}  // namespace ew::audio
