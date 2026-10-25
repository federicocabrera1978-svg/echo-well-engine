#include "render/gl_loader.hpp"

namespace ew::gl {
#define EW_GL_DEF(type, name) type name = nullptr;
EW_GL_FUNCS(EW_GL_DEF)
#undef EW_GL_DEF

const char* load(ProcLoader loader) {
#define EW_GL_LOAD(type, name) \
  name = reinterpret_cast<type>(loader("gl" #name)); \
  if (!name) return "gl" #name;
  EW_GL_FUNCS(EW_GL_LOAD)
#undef EW_GL_LOAD
  return nullptr;
}
}  // namespace ew::gl
