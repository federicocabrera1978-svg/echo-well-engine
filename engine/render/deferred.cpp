#include "render/deferred.hpp"
#include <cstdio>

#include <algorithm>
#include <array>
#include "render/sprite_atlas.hpp"
#include "render/tile_atlas.hpp"
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

bool DeferredRenderer::init(const std::string& dir, std::string& err, const std::string& atlas_path,
                            const std::string& sprite_atlas_path) {
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
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8UI, kTilesW, kTilesH, 0, GL_RGB_INTEGER, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  {  // atlas de tiles 128×128 RGBA8 (unidad 1)
    const auto loaded = load_tile_atlas_file(atlas_path.empty() ? nullptr : atlas_path.c_str());
    const auto& atlas = loaded.px;
    std::fprintf(stderr, "echo_engine: atlas %s%s\n",
                 loaded.source == AtlasSource::Png ? "PNG " : "procedural",
                 loaded.source == AtlasSource::Png ? atlas_path.c_str() : "");
    glGenTextures(1, &atlas_tex_); glBindTexture(GL_TEXTURE_2D, atlas_tex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kAtlasPx, kAtlasPx, 0, GL_RGBA, GL_UNSIGNED_BYTE, atlas.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  }
  {  // atlas de sprites 256×128 RGBA8 (unidad 2)
    const auto loaded_sp = load_sprite_atlas_file(sprite_atlas_path.empty() ? nullptr : sprite_atlas_path.c_str());
    const auto& sprites = loaded_sp.px;
    std::fprintf(stderr, "echo_engine: sprites %s%s\n",
                 loaded_sp.source == AtlasSource::Png ? "PNG " : "procedural",
                 loaded_sp.source == AtlasSource::Png ? sprite_atlas_path.c_str() : "");
    glGenTextures(1, &sprite_tex_); glBindTexture(GL_TEXTURE_2D, sprite_tex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kSpriteAtlasW, kSpriteAtlasH, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 sprites.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  }
  UseProgram(geom_prog_);
  Uniform1i(GetUniformLocation(geom_prog_, "u_tiles"), 0);
  Uniform1i(GetUniformLocation(geom_prog_, "u_atlas"), 1);
  Uniform1i(GetUniformLocation(geom_prog_, "u_sprites"), 2);
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
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kTilesW, kTilesH, GL_RGB_INTEGER, GL_UNSIGNED_BYTE, v.tiles);
    tiles_version_ = v.tiles_version;
  }
  ActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, atlas_tex_);
  ActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, sprite_tex_);
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
  Uniform1i(GetUniformLocation(geom_prog_, "u_sprite_count"), v.sprite_count);
  if (v.sprite_count > 0) {
    Uniform4fv(GetUniformLocation(geom_prog_, "u_sprite_rect"), v.sprite_count, v.sprites.data());
    Uniform4fv(GetUniformLocation(geom_prog_, "u_sprite_tint"), v.sprite_count, v.sprite_tint.data());
  }
  Uniform1i(GetUniformLocation(geom_prog_, "u_player_sprite_idx"), v.player_sprite_idx);
  Uniform2f(GetUniformLocation(geom_prog_, "u_player_squash"), v.player_squash_x, v.player_squash_y);
  Uniform1i(GetUniformLocation(geom_prog_, "u_water_count"), v.water_count);
  if (v.water_count > 0) {
    Uniform1iv(GetUniformLocation(geom_prog_, "u_water_cols"), v.water_count, v.water_cols.data());
    Uniform4fv(GetUniformLocation(geom_prog_, "u_water_rect"), v.water_count, v.water_rect.data());
    Uniform1fv(GetUniformLocation(geom_prog_, "u_water"), v.water_count * kMaxWaterCols, v.water_surface.data());
  }
  Uniform1i(GetUniformLocation(geom_prog_, "u_mover_count"), v.mover_count);
  if (v.mover_count > 0) {
    Uniform4fv(GetUniformLocation(geom_prog_, "u_mover_rect"), v.mover_count, v.mover_rect.data());
    Uniform1fv(GetUniformLocation(geom_prog_, "u_mover_fam"), v.mover_count, v.mover_fam.data());
  }
  Uniform1i(GetUniformLocation(geom_prog_, "u_hazard_count"), v.hazard_count);
  if (v.hazard_count > 0) {
    Uniform4fv(GetUniformLocation(geom_prog_, "u_hazards"), v.hazard_count, v.hazards.data());
  }
  Uniform1i(GetUniformLocation(geom_prog_, "u_boss_count"), v.boss_count);
  if (v.boss_count > 0) {
    Uniform4fv(GetUniformLocation(geom_prog_, "u_boss_rect"), v.boss_count, v.boss_rect.data());
    Uniform4fv(GetUniformLocation(geom_prog_, "u_boss_state"), v.boss_count, v.boss_state.data());
  }
  Uniform1iv(GetUniformLocation(geom_prog_, "u_bg_theme"), 9, v.bg_theme.data());
  Uniform1iv(GetUniformLocation(geom_prog_, "u_bg_seed"), 9, v.bg_seed.data());
  Uniform1i(GetUniformLocation(geom_prog_, "u_foliage"), v.foliage_enabled);
  Uniform1f(GetUniformLocation(geom_prog_, "u_wind_gust"), v.wind_gust);
  Uniform1i(GetUniformLocation(geom_prog_, "u_wind_imp_count"), v.wind_impulse_count);
  if (v.wind_impulse_count > 0)
    Uniform4fv(GetUniformLocation(geom_prog_, "u_wind_imp"), v.wind_impulse_count, v.wind_impulses.data());
  glDrawArrays(GL_TRIANGLES, 0, 3);
  // 2) Light pass -> lit target
  BindFramebuffer(GL_FRAMEBUFFER, lit_fbo_);
  UseProgram(light_prog_);
  Uniform1f(GetUniformLocation(light_prog_, "u_time"), t);
  Uniform1f(GetUniformLocation(light_prog_, "u_fade"), v.fade);
  Uniform1i(GetUniformLocation(light_prog_, "u_end"), v.end_screen);
  Uniform1i(GetUniformLocation(light_prog_, "u_end_sel"), v.end_sel);
  Uniform1i(GetUniformLocation(light_prog_, "u_end_has_next"), v.end_has_next);
  Uniform1i(GetUniformLocation(light_prog_, "u_end_true"), v.end_true);
  Uniform2f(GetUniformLocation(light_prog_, "u_light"), v.light_x - v.cam_x, v.light_y - v.cam_y);
  Uniform1f(GetUniformLocation(light_prog_, "u_amb_dark"), v.ambient_darkness);
  Uniform1f(GetUniformLocation(light_prog_, "u_lantern_rad"), v.lantern_radius);
  const int pl_count = std::clamp(v.point_light_count, 0, ew::game::kMaxPointLights);
  Uniform1i(GetUniformLocation(light_prog_, "u_pl_count"), pl_count);
  if (pl_count > 0) {
    std::array<float, ew::game::kMaxPointLights * 4> pl_pos = v.point_light_pos;
    for (int i = 0; i < pl_count; ++i) {
      pl_pos[std::size_t(i) * 4 + 0] -= v.cam_x;
      pl_pos[std::size_t(i) * 4 + 1] -= v.cam_y;
    }
    Uniform4fv(GetUniformLocation(light_prog_, "u_pl_pos"), pl_count, pl_pos.data());
    Uniform4fv(GetUniformLocation(light_prog_, "u_pl_col"), pl_count, v.point_light_col.data());
  }
  Uniform1i(GetUniformLocation(light_prog_, "u_pause"), v.pause);
  Uniform1i(GetUniformLocation(light_prog_, "u_sel"), v.pause_sel);
  Uniform1f(GetUniformLocation(light_prog_, "u_vol"), v.pause_vol);
  Uniform1i(GetUniformLocation(light_prog_, "u_mute"), v.pause_mute);
  Uniform1i(GetUniformLocation(light_prog_, "u_has_save"), v.pause_has_save);
  Uniform1i(GetUniformLocation(light_prog_, "u_foliage"), v.foliage_enabled);
  Uniform1i(GetUniformLocation(light_prog_, "u_hero_sel"), v.hero_select);
  Uniform1i(GetUniformLocation(light_prog_, "u_hero_cur"), v.hero_cursor);
  Uniform1i(GetUniformLocation(light_prog_, "u_dlg"), v.dlg);
  Uniform1i(GetUniformLocation(light_prog_, "u_dlg_more"), v.dlg_more);
  Uniform1i(GetUniformLocation(light_prog_, "u_ctl"), v.ctl);
  Uniform1i(GetUniformLocation(light_prog_, "u_ctl_sel"), v.ctl_sel);
  Uniform1i(GetUniformLocation(light_prog_, "u_ctl_wait"), v.ctl_wait);
  if (v.pause || v.hero_select || v.end_screen || v.dlg || v.map_open || v.toast_alpha > 0.01f)
    Uniform1iv(GetUniformLocation(light_prog_, "u_txt"), 96, v.ctl_txt.data());
  Uniform1i(GetUniformLocation(light_prog_, "u_items"), v.items);
  Uniform1i(GetUniformLocation(light_prog_, "u_items_total"), v.items_total);
  Uniform1f(GetUniformLocation(light_prog_, "u_items_flash"), v.items_flash);
  Uniform1i(GetUniformLocation(light_prog_, "u_abilities"), v.abilities);
  Uniform1f(GetUniformLocation(light_prog_, "u_ability_flash"), v.ability_flash);
  Uniform1i(GetUniformLocation(light_prog_, "u_hp"), v.hp);
  Uniform1i(GetUniformLocation(light_prog_, "u_max_hp"), v.max_hp);
  Uniform1i(GetUniformLocation(light_prog_, "u_dash_up"), v.dash_upgraded);
  Uniform4f(GetUniformLocation(light_prog_, "u_hud_fx"),
            v.damage_flash, float(v.dash_charges), float(v.dash_charges_max), v.milestone_flash);
  Uniform4f(GetUniformLocation(light_prog_, "u_map_grid"),
            float(v.map_w), float(v.map_h), float(v.cur_cx), float(v.cur_cy));
  Uniform1i(GetUniformLocation(light_prog_, "u_map_vis"), int(v.visited_mask));
  Uniform1i(GetUniformLocation(light_prog_, "u_map_vis_hi"), int(v.visited_mask_hi));
  Uniform1i(GetUniformLocation(light_prog_, "u_map_doors"), int(v.blocked_doors_lo));
  Uniform1i(GetUniformLocation(light_prog_, "u_map_doors_hi"), int(v.blocked_doors_hi));
  Uniform1i(GetUniformLocation(light_prog_, "u_map_eggs"), int(v.missing_eggs_lo));
  Uniform1i(GetUniformLocation(light_prog_, "u_map_eggs_hi"), int(v.missing_eggs_hi));
  Uniform1i(GetUniformLocation(light_prog_, "u_map_pct"), v.explored_pct);
  Uniform1i(GetUniformLocation(light_prog_, "u_seals_mask"), v.seals_mask);
  Uniform1i(GetUniformLocation(light_prog_, "u_seals_total"), v.seals_total);
  Uniform1i(GetUniformLocation(light_prog_, "u_map_open"), v.map_open);
  Uniform2f(GetUniformLocation(light_prog_, "u_map_target"), float(v.map_target_cx), float(v.map_target_cy));
  Uniform2f(GetUniformLocation(light_prog_, "u_toast"), v.toast_alpha, float(v.toast_kind));
  Uniform4f(GetUniformLocation(light_prog_, "u_boss_hud"),
            float(v.boss_hp), float(v.boss_max_hp), float(v.boss_kind), v.boss_hurt_flash);
  Uniform4f(GetUniformLocation(light_prog_, "u_boss_burst"), v.boss_burst_x, v.boss_burst_y, v.boss_burst_t, v.boss_flash);
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
  GLuint texs[7] = {albedo_, normal_, depth_, lit_tex_, tiles_tex_, atlas_tex_, sprite_tex_};
  glDeleteTextures(7, texs);
  *this = {};
}
}  // namespace ew::render
