#version 330 core
// Geometry pass: tiles COL/VIS del vecindario 3×3 (textura RG8UI 120×69) + marcadores
// (player, entidades, cuerpos Verlet) + superficie de agua. Coordenadas de mundo en px, y hacia abajo.
in vec2 v_uv;
layout(location = 0) out vec4 o_albedo;
layout(location = 1) out vec4 o_normal;
uniform usampler2D u_tiles;
uniform sampler2D u_atlas;  // 128×128 RGBA8: 16×16 tiles de 8×8, id = VIS; A = altura
uniform float u_time;
uniform vec2 u_cam;    // esquina superior izquierda de la vista (px mundo)
uniform vec2 u_chunk;  // origen del vecindario 3×3 de chunks (120×69 tiles, px mundo)
uniform int u_marker_count;
uniform vec4 u_markers[48];        // x, y, radio, forma
uniform vec3 u_marker_colors[48];
uniform int u_water_count;         // cuerpos de agua (máx. 4)
uniform int u_water_cols[4];
uniform vec4 u_water_rect[4];      // x, y reposo, w, y fondo
uniform float u_water[256];        // superficie por columna (px mundo), stride 64 por cuerpo

// Texel del atlas para el tile id `vis` en coord local l (0..7).
vec4 atlas_texel(uint vis, ivec2 l) {
  ivec2 o = ivec2(int(vis % 16u), int(vis / 16u)) * 8;
  return texelFetch(u_atlas, o + clamp(l, ivec2(0), ivec2(7)), 0);
}
float atlas_h(uint vis, ivec2 l) { return atlas_texel(vis, l).a; }

void main() {
  vec2 world = u_cam + vec2(v_uv.x * 320.0, (1.0 - v_uv.y) * 180.0);
  vec2 local = world - u_chunk;
  ivec2 t = ivec2(floor(local / 8.0));
  uvec2 cv = uvec2(0u);
  bool inside = t.x >= 0 && t.y >= 0 && t.x < 120 && t.y < 69;
  if (inside) cv = texelFetch(u_tiles, ivec2(t.x, t.y), 0).rg;
  bool solid = cv.r != 0u;
  vec3 albedo = vec3(0.05, 0.06, 0.10);
  vec3 n = vec3(0.0, 0.0, 1.0);
  float depth = 1.0;
  ivec2 l = ivec2(floor(local)) - t * 8;
  if (solid) {
    albedo = atlas_texel(cv.g, l).rgb;
    // Normal desde la altura del atlas (diferencias centrales) + bisel de borde.
    float dx = atlas_h(cv.g, l + ivec2(1, 0)) - atlas_h(cv.g, l - ivec2(1, 0));
    float dy = atlas_h(cv.g, l + ivec2(0, 1)) - atlas_h(cv.g, l - ivec2(0, 1));
    n = normalize(vec3(-dx, dy, 1.0));
    if (l.x == 0) n = normalize(vec3(-1, 0, 1)); else if (l.x == 7) n = normalize(vec3(1, 0, 1));
    if (l.y == 0) n = normalize(vec3(0, 1, 1)); else if (l.y == 7) n = normalize(vec3(0, -1, 1));
    depth = 0.5;
  } else if (cv.g != 0u) {
    albedo = atlas_texel(cv.g, l).rgb * 0.35;  // decorado de fondo (VIS sin COL)
  }
  // Agua: columna por columna, desde la superficie Hooke hasta el fondo.
  for (int b = 0; b < u_water_count; ++b) {
    int nc = u_water_cols[b];
    vec4 wr = u_water_rect[b];
    int base = b * 64;
    float cw = wr.z / float(nc);
    float fx = (world.x - wr.x) / cw;
    if (fx >= 0.0 && fx < float(nc) && world.y < wr.w) {
      int i = int(fx);
      float surf = u_water[base + i];
      if (world.y >= surf - 0.5) {
        float d = world.y - surf;
        vec3 wc = d < 1.0 ? vec3(0.55, 0.85, 1.0) : vec3(0.10, 0.28, 0.48);
        albedo = mix(albedo, wc, d < 1.0 ? 1.0 : 0.75);
        if (d < 2.0) {
          float sl = u_water[base + min(i + 1, nc - 1)] - u_water[base + max(i - 1, 0)];
          n = normalize(vec3(-sl * 0.5, 1.0, 0.8));
        }
        depth = min(depth, 0.4);
      }
    }
  }
  // Marcadores
  for (int i = 0; i < u_marker_count; ++i) {
    vec4 m = u_markers[i];
    vec2 d = world - m.xy;
    bool hit = m.w < 0.5 ? dot(d, d) <= m.z * m.z : max(abs(d.x), abs(d.y)) <= m.z;
    if (hit) {
      albedo = u_marker_colors[i];
      n = m.w < 0.5 && m.z > 1.5 ? normalize(vec3(d.x / m.z, -d.y / m.z, 1.0)) : vec3(0.0, 0.0, 1.0);
      depth = 0.3;
    }
  }
  o_albedo = vec4(albedo, 1.0);
  o_normal = vec4(n * 0.5 + 0.5, 1.0);
  gl_FragDepth = depth;
}
