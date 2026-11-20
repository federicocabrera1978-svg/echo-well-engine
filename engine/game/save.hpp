#pragma once
// Guardado de partida: formato binario versionado y validado (bounds + CRC-32), sin GL.
// Layout (little-endian):
//   "EWSV" u16 version u16 reservado(0) u32 payload_size | payload | u32 CRC-32 de todo lo anterior
// Payload v1:
//   u8 map_len, char map[map_len] (1..64, ASCII imprimible)
//   i16 checkpoint (-1 = spawn, si no índice de entidad < entity_count)
//   u8 flags (bit0 gravedad invertida, bit1 mute; el resto debe ser 0)
//   f32 volume (0..1, finito)
//   u16 entity_count (≤ kMaxEntities), u8 active[(entity_count + 7) / 8] (bit i = entidad i activa)
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ew::game {

class Sim;

inline constexpr uint16_t kSaveVersion = 1;
inline constexpr std::size_t kSaveMaxMapName = 64;
inline constexpr std::size_t kSaveMaxEntities = 4096;
inline constexpr std::size_t kSaveMaxBytes = 4096;  // un save válido nunca pasa de esto

enum class SaveError { None, TooSmall, TooBig, BadMagic, BadVersion, BadSize, BadCrc, BadField, Mismatch, Io };
const char* save_error_str(SaveError e);

struct SaveData {
  std::string map;                 // nombre del mapa (STRTAB offset 1)
  int checkpoint = -1;             // índice en Scene::entities(), -1 = spawn
  bool flipped = false;            // gravedad al tomar el checkpoint
  bool muted = false;
  float volume = 0.8f;
  std::vector<uint8_t> active;     // Entity::active por entidad (0/1)
};

std::vector<uint8_t> encode_save(const SaveData& d);
// Valida todo (magic, versión, tamaños exactos, campos, CRC). Nunca lee fuera de [data, data+size).
SaveError decode_save(const uint8_t* data, std::size_t size, SaveData& out);

// Estado de la partida → SaveData (volumen/mute los completa quien llama).
SaveData capture_save(const Sim& sim);
// Aplica un SaveData a la sim: valida contra el mapa cargado (nombre, cantidad de entidades,
// checkpoint de tipo "checkpoint"), restaura entidades activas (puertas, switches, huevos,
// boss vencido), checkpoint + gravedad, y reaparece ahí. Si no valida, no toca nada.
SaveError apply_save(Sim& sim, const SaveData& d);

// Archivo: escribe atómico (tmp + rename) y lee con tope de tamaño.
SaveError write_save_file(const std::string& path, const SaveData& d);
SaveError read_save_file(const std::string& path, SaveData& out);

// Directorio de datos del usuario para el save (se crea si no existe):
//   EW_SAVE_DIR si está definido; Windows %APPDATA%\EchoWell; macOS ~/Library/Application
//   Support/EchoWell; resto $XDG_DATA_HOME/echo-well o ~/.local/share/echo-well.
// Si no se puede crear/escribir, prueba `fallback_dir` (p. ej. junto al binario); si tampoco,
// devuelve "" (sin guardado).
std::string save_dir(const std::string& fallback_dir = "");
inline constexpr const char* kSaveFileName = "save.ewsv";

}  // namespace ew::game
