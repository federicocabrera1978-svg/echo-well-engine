#include "render/deferred.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

namespace ew::render {
using namespace ew::gl;
namespace {
bool read_file(const std::string& p, std::string& out) {
  std::ifstream f(p, std::ios::binary);
  if (!f) return false;
  std::stringstream ss; ss << f.rdbuf(); out = ss.str();
  return true;
}
GLuint compile(GLenum type, const std::string& src, std::string& err) {
  GLuint s = CreateShader(type);
  const char* c = src.c_str();
  ShaderSource(s, 1, &c, nullptr);
  CompileShader(s);
  GLint ok = 0; GetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) { char log[1024]; GetShaderInfoLog(s, sizeof log, nullptr, log); err = log; DeleteShader(s); return 0; }
  return s;
}
GLuint program(const std::string& dir, const char* vs, const char* fs, std::string& err) {
  std::string vsrc, fsrc;
  if (!read_file(dir + "/" + vs, vsrc) || !read_file(dir + "/" + fs, fsrc)) {
    err = "no se pudo leer shader en " + dir; return 0;
  }
  GLuint v = compile(GL_VERTEX_SHADER, vsrc, err); if (!v) { err = std::string(vs) + ": " + err; return 0; }
  GLuint f = compile(GL_FRAGMENT_SHADER, fsrc, err); if (!f) { err = std::string(fs) + ": " + err; DeleteShader(v); return 0; }
  GLuint p = CreateProgram(); AttachShader(p, v); AttachShader(p, f); LinkProgram(p);
  DeleteShader(v); DeleteShader(f);
  GLint ok = 0; GetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) { char log[1024]; GetProgramInfoLog(p, sizeof log, nullptr, log); err = log; DeleteProgram(p); return 0; }
  return p;
}
GLuint tex(GLint ifmt, GLenum fmt, GLenum type) {
  GLuint t; glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
  glTexImage2D(GL_TEXTURE_2D, 0, ifmt, kInternalW, kInternalH, 0, fmt, type, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  return t;
}
}  // namespace

bool DeferredRenderer::init(const std::string& dir, std::string& err) {
  albedo_ = tex(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
  normal_ = tex(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
  depth_  = tex(GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT);
  GenFramebuffers(1, &gbuf_fbo_); BindFramebuffer(GL_FRAMEBUFFER, gbuf_fbo_);
  FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, albedo_, 0);
  FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, normal_, 0);
  FramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_, 0);
  const GLenum bufs[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
  DrawBuffers(2, bufs);
  if (CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) { err = "G-buffer incompleto"; return false; }

  lit_tex_ = tex(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
  GenFramebuffers(1, &lit_fbo_); BindFramebuffer(GL_FRAMEBUFFER, lit_fbo_);
  FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, lit_tex_, 0);
  if (CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) { err = "lit target incompleto"; return false; }
  BindFramebuffer(GL_FRAMEBUFFER, 0);

  GenVertexArrays(1, &vao_);  // triángulo fullscreen vía gl_VertexID, sin VBO
  if (!(geom_prog_ = program(dir, "fullscreen.vert", "gbuffer_map.frag", err))) return false;
  if (!(light_prog_ = program(dir, "fullscreen.vert", "light.frag", err))) return false;
  glGenTextures(1, &tiles_tex_); glBindTexture(GL_TEXTURE_2D, tiles_tex_);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8UI, kTilesW, kTilesH, 0, GL_RG_INTEGER, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  UseProgram(geom_prog_);
  Uniform1i(GetUniformLocation(geom_prog_, "u_tiles"), 0);
  UseProgram(light_prog_);
  Uniform1i(GetUniformLocation(light_prog_, "u_albedo"), 0);
  Uniform1i(GetUniformLocation(light_prog_, "u_normal"), 1);
  return true;
}

void DeferredRenderer::render(float t, int fb_w, int fb_h, const FrameView& v) {
  BindVertexArray(vao_);
  ActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, tiles_tex_);
  if (v.tiles && v.tiles_version != tiles_version_) {
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kTilesW, kTilesH, GL_RG_INTEGER, GL_UNSIGNED_BYTE, v.tiles);
    tiles_version_ = v.tiles_version;
  }
  // 1) Geometry pass (tiles del chunk activo + marcadores + agua) -> G-buffer
  BindFramebuffer(GL_FRAMEBUFFER, gbuf_fbo_);
  glViewport(0, 0, kInternalW, kInternalH);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  UseProgram(geom_prog_);
  Uniform1f(GetUniformLocation(geom_prog_, "u_time"), t);
  Uniform2f(GetUniformLocation(geom_prog_, "u_cam"), v.cam_x, v.cam_y);
  Uniform2f(GetUniformLocation(geom_prog_, "u_chunk"), v.chunk_x_px, v.chunk_y_px);
  Uniform1i(GetUniformLocation(geom_prog_, "u_marker_count"), v.marker_count);
  if (v.marker_count > 0) {
    Uniform4fv(GetUniformLocation(geom_prog_, "u_markers"), v.marker_count, v.markers.data());
    Uniform3fv(GetUniformLocation(geom_prog_, "u_marker_colors"), v.marker_count, v.marker_colors.data());
  }
  Uniform1i(GetUniformLocation(geom_prog_, "u_water_count"), v.water_count);
  if (v.water_count > 0) {
    Uniform1iv(GetUniformLocation(geom_prog_, "u_water_cols"), v.water_count, v.water_cols.data());
    Uniform4fv(GetUniformLocation(geom_prog_, "u_water_rect"), v.water_count, v.water_rect.data());
    Uniform1fv(GetUniformLocation(geom_prog_, "u_water"), v.water_count * kMaxWaterCols, v.water_surface.data());
  }
  glDrawArrays(GL_TRIANGLES, 0, 3);
  // 2) Light pass -> lit target
  BindFramebuffer(GL_FRAMEBUFFER, lit_fbo_);
  UseProgram(light_prog_);
  Uniform1f(GetUniformLocation(light_prog_, "u_time"), t);
  Uniform2f(GetUniformLocation(light_prog_, "u_light"), v.light_x - v.cam_x, v.light_y - v.cam_y);
  ActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, albedo_);
  ActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, normal_);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  // 3) Present: escalado entero centrado (letterbox)
  BindFramebuffer(GL_READ_FRAMEBUFFER, lit_fbo_);
  BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  glViewport(0, 0, fb_w, fb_h);
  glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
  int s = std::max(1, std::min(fb_w / kInternalW, fb_h / kInternalH));
  int w = kInternalW * s, h = kInternalH * s, x = (fb_w - w) / 2, y = (fb_h - h) / 2;
  BlitFramebuffer(0, 0, kInternalW, kInternalH, x, y, x + w, y + h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
  BindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool DeferredRenderer::dump_ppm(const std::string& path) const {
  std::vector<unsigned char> px(std::size_t(kInternalW) * kInternalH * 3);
  BindFramebuffer(GL_READ_FRAMEBUFFER, lit_fbo_);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, kInternalW, kInternalH, GL_RGB, GL_UNSIGNED_BYTE, px.data());
  BindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  std::ofstream f(path, std::ios::binary);
  if (!f) return false;
  f << "P6\n" << kInternalW << " " << kInternalH << "\n255\n";
  for (int y = kInternalH - 1; y >= 0; --y)
    f.write(reinterpret_cast<const char*>(&px[std::size_t(y) * kInternalW * 3]), kInternalW * 3);
  return bool(f);
}

void DeferredRenderer::shutdown() {
  if (geom_prog_) DeleteProgram(geom_prog_);
  if (light_prog_) DeleteProgram(light_prog_);
  if (vao_) DeleteVertexArrays(1, &vao_);
  if (gbuf_fbo_) DeleteFramebuffers(1, &gbuf_fbo_);
  if (lit_fbo_) DeleteFramebuffers(1, &lit_fbo_);
  GLuint texs[5] = {albedo_, normal_, depth_, lit_tex_, tiles_tex_};
  glDeleteTextures(5, texs);
  *this = {};
}
}  // namespace ew::render
