#pragma once
// Fuente única del tono narrativo: la Orden de la Vigía encerró al Wyrm bajo Tres Sellos Elementales
// (Piedra, Sombra y Agua) y dispersó las 8 nidadas de dragón. Frases cortas, segunda persona.
// Aquí viven los textos compartidos por código (final, título del mundo, diálogos de victoria y epílogo)
// y el linter `story_text_valid` que usa `tests/story_test.cpp` para vigilar todos los diálogos.
#include "game/menu_text.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace ew::game {

// Pantalla de final (end_screen.hpp) y título del mundo unificado (campaign.hpp).
inline constexpr std::string_view kEndTitleVictory = "VICTORIA: EL WYRM CAYO";
inline constexpr std::string_view kEndTitleTrue = "FINAL VERDADERO";
inline constexpr std::string_view kEndSubtitleTrue = "NIDADA A SALVO: TODOS LOS HUEVOS";
inline constexpr std::string_view kWorldTitle = "EL CALABOZO DEL WYRM";

// Diálogos de victoria sobre cada boss (boss_victory.hpp): formato `pág|pág`, líneas separadas por `/`.
inline constexpr std::string_view kVictoryWatcherSpeaker = "PRIMER SELLO: PIEDRA";
inline constexpr std::string_view kVictoryWatcherText =
    "CAE EL COLOSO DE LA ORDEN/Y CEDE EL SELLO DE PIEDRA.|UN ARCO DEL PORTICO SE ABRE./BAJA AHORA A LAS CATACUMBAS.";
inline constexpr std::string_view kVictoryGuardian2Speaker = "SEGUNDO SELLO: SOMBRA";
inline constexpr std::string_view kVictoryGuardian2Text =
    "CAE EL GUARDIAN DE LA CRIPTA/Y SE QUIEBRA EL SELLO SOMBRIO.|DOS ARCOS DEL PORTICO CEDEN:/AUN RESTA EL SELLO DEL AGUA.";
inline constexpr std::string_view kVictoryWyrmSpeaker = "CORONA DEL WYRM";
inline constexpr std::string_view kVictoryWyrmText =
    "LA FURIA DEL WYRM SE APAGA:/ROTAS QUEDAN SUS CADENAS.|AVANZA AL NIDO PRIMORDIAL/Y ENTREGA TU LLAMA VIVA!";
inline constexpr std::string_view kVictorySentinelSpeaker = "TERCER SELLO: AGUA";
inline constexpr std::string_view kVictorySentinelText =
    "EL CENTINELA SUMERGIDO CAE/Y SE ROMPE EL SELLO DEL AGUA.|LOS TRES SELLOS HAN CEDIDO:/LA GUARIDA DEL WYRM SE ABRE!";

// Epílogo narrativo al consagrar el Nido Primordial (meta final de campaña).
inline constexpr std::string_view kEpilogueSpeaker = "NIDO PRIMORDIAL";
inline constexpr std::string_view kEpilogueStandardText =
    "LOS TRES SELLOS Y LAS CADENAS/DEL WYRM CAEN HECHOS CENIZA.|TU LLAMA ENCIENDE EL NIDO/Y EL CALOR REGRESA AL POZO.|PERO EN LA PIEDRA PROFUNDA/AUN DUERMEN HUEVOS SIN LUZ.";
inline constexpr std::string_view kEpilogueTrueText =
    "LAS OCHO NIDADAS REUNIDAS/LATEN JUNTO A TU LLAMA VIVA.|CASCARAS DE ORO SE QUIEBRAN:/ALAS JOVENES CRUZAN EL POZO.|EL INVIERNO DE LA VIGIA CEDE/Y EL FUEGO VUELVE AL MUNDO!";

// Límites de un diálogo (la caja ocupa 32 columnas con 1 de margen por lado).
inline constexpr std::size_t kStoryLineMax = 30, kStoryLinesPerPage = 2, kStoryPagesMax = 3, kStorySpeakerMax = 32;

// Restos de la opción "ascensión y pureza" que no deben reaparecer en ningún texto.
inline const char* const kStoryForbidden[] = {
    "PUREZA",    "PURO",       "ASCENSION", "IMPEDIMENTO", "ALLI UNA VEZ",     "DEMUESTR",
    "PERTENECES", "NO DESISTAS", "TAREA NO ES VANA", "LUZ DE TU PUREZA", "REINO DE LO ALTO",
};
inline constexpr std::size_t kStoryForbiddenCount = sizeof(kStoryForbidden) / sizeof(kStoryForbidden[0]);

namespace detail {
// Normaliza a mayúsculas ASCII (la misma regla que DialogueBox: solo a-z se sube).
inline std::string story_upper(std::string_view s) {
  std::string out(s);
  for (auto& c : out)
    if (c >= 'a' && c <= 'z') c = char(c - 'a' + 'A');
  return out;
}
inline bool story_set_why(std::string* why, const std::string& msg) {
  if (why) *why = msg;
  return false;
}
}  // namespace detail

// Primer término prohibido contenido en `text` (tras normalizar), o nullptr si está limpio.
inline const char* story_find_forbidden(std::string_view text) {
  const std::string up = detail::story_upper(text);
  for (std::size_t i = 0; i < kStoryForbiddenCount; ++i)
    if (up.find(kStoryForbidden[i]) != std::string::npos) return kStoryForbidden[i];
  return nullptr;
}

// Valida un hablante: ≤32 glifos, solo glifos de la fuente y sin términos prohibidos.
inline bool story_speaker_valid(std::string_view speaker, std::string* why = nullptr) {
  if (speaker.empty()) return detail::story_set_why(why, "hablante vacio");
  if (speaker.size() > kStorySpeakerMax) return detail::story_set_why(why, "hablante > 32 glifos");
  for (char c : detail::story_upper(speaker))
    if (glyph_index(c) == kGlyphNone) return detail::story_set_why(why, std::string("glifo invalido en hablante: ") + c);
  if (const char* f = story_find_forbidden(speaker)) return detail::story_set_why(why, std::string("prohibido: ") + f);
  return true;
}

// Valida el texto de un diálogo: solo glifos de la fuente (`kGlyphChars`; minúsculas aceptadas porque se
// normalizan), ≤30 glifos por línea, ≤2 líneas por página, ≤3 páginas y sin términos prohibidos.
inline bool story_text_valid(std::string_view text, std::string* why = nullptr) {
  using detail::story_set_why;
  if (text.empty()) return story_set_why(why, "texto vacio");
  const std::string up = detail::story_upper(text);
  if (const char* f = story_find_forbidden(up)) return story_set_why(why, std::string("prohibido: ") + f);
  std::size_t pages = 0, pos = 0;
  while (pos <= up.size()) {
    const std::size_t pipe = up.find('|', pos);
    const std::string_view page = std::string_view(up).substr(pos, pipe == std::string::npos ? std::string::npos : pipe - pos);
    ++pages;
    std::size_t lines = 0, lpos = 0;
    while (lpos <= page.size()) {
      const std::size_t slash = page.find('/', lpos);
      const std::string_view line = page.substr(lpos, slash == std::string_view::npos ? std::string_view::npos : slash - lpos);
      ++lines;
      if (line.size() > kStoryLineMax)
        return story_set_why(why, "linea > 30 glifos (" + std::to_string(line.size()) + "): " + std::string(line));
      for (char c : line)
        if (glyph_index(c) == kGlyphNone) return story_set_why(why, std::string("glifo invalido: ") + c + " en: " + std::string(line));
      if (slash == std::string_view::npos) break;
      lpos = slash + 1;
    }
    if (lines > kStoryLinesPerPage) return story_set_why(why, "pagina con mas de 2 lineas");
    if (pipe == std::string::npos) break;
    pos = pipe + 1;
  }
  if (pages > kStoryPagesMax) return story_set_why(why, "mas de 3 paginas");
  return true;
}

}  // namespace ew::game
