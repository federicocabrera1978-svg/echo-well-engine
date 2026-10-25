#include "render/deferred.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

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
  if (!(geom_prog_ = program(dir, "fullscreen.vert", "gbuffer_stub.frag", err))) return false;
  if (!(light_prog_ = program(dir, "fullscreen.vert", "light.frag", err))) return false;
  UseProgram(light_prog_);
  Uniform1i(GetUniformLocation(light_prog_, "u_albedo"), 0);
  Uniform1i(GetUniformLocation(light_prog_, "u_normal"), 1);
  return true;
}

void DeferredRenderer::render(float t, int fb_w, int fb_h) {
  BindVertexArray(vao_);
  // 1) Geometry pass (stub procedural) -> G-buffer
  BindFramebuffer(GL_FRAMEBUFFER, gbuf_fbo_);
  glViewport(0, 0, kInternalW, kInternalH);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  UseProgram(geom_prog_);
  Uniform1f(GetUniformLocation(geom_prog_, "u_time"), t);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  // 2) Light pass -> lit target
  BindFramebuffer(GL_FRAMEBUFFER, lit_fbo_);
  UseProgram(light_prog_);
  Uniform1f(GetUniformLocation(light_prog_, "u_time"), t);
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

void DeferredRenderer::shutdown() {
  if (geom_prog_) DeleteProgram(geom_prog_);
  if (light_prog_) DeleteProgram(light_prog_);
  if (vao_) DeleteVertexArrays(1, &vao_);
  if (gbuf_fbo_) DeleteFramebuffers(1, &gbuf_fbo_);
  if (lit_fbo_) DeleteFramebuffers(1, &lit_fbo_);
  GLuint texs[4] = {albedo_, normal_, depth_, lit_tex_};
  glDeleteTextures(4, texs);
  *this = {};
}
}  // namespace ew::render
