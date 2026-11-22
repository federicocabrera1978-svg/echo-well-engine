#version 330 core
// Geometry pass: tiles COL/VIS/FG del vecindario 3×3 (textura RGB8UI 120×69) + marcadores
// (player, entidades, cuerpos Verlet) + superficie de agua. Coordenadas de mundo en px, y hacia abajo.
// Detrás de todo, el fondo procedural de cada chunk (backdrop, sección BACKDROP del .ewmap).
// Salidas extra: o_albedo.a = respuesta a la luz del player (1 = normal, 0 = fondo) y
// o_normal.a = 1 - emisión (luz propia del fondo: gotas, luciérnagas, antorchas...).
in vec2 v_uv;
layout(location = 0) out vec4 o_albedo;
layout(location = 1) out vec4 o_normal;
uniform usampler2D u_tiles;
uniform sampler2D u_atlas;  // 128×128 RGBA8: 16×16 tiles de 8×8, id = VIS; A = altura
uniform sampler2D u_sprites; // 128×128 RGBA8: 8×8 celdas de 16×16, A = relieve (>0.94 auto-emisión)
uniform float u_time;
uniform vec2 u_cam;    // esquina superior izquierda de la vista (px mundo)
uniform vec2 u_chunk;  // origen del vecindario 3×3 de chunks (120×69 tiles, px mundo)
uniform int u_marker_count;
uniform vec4 u_markers[48];        // x, y, radio, forma
uniform vec3 u_marker_colors[48];
uniform int u_sprite_count;
uniform vec4 u_sprite_rect[16];    // (x, y, col_signed, row_signed)
uniform vec4 u_sprite_tint[16];    // (r, g, b, emit_boost)
uniform int u_water_count;         // cuerpos de agua (máx. 4)
uniform int u_water_cols[4];
uniform vec4 u_water_rect[4];      // x, y reposo, w, y fondo
uniform float u_water[256];        // superficie por columna (px mundo), stride 64 por cuerpo
uniform int u_mover_count;         // plataformas móviles (máx. 8)
uniform vec4 u_mover_rect[8];      // x, y, w, h (px mundo, esquina sup. izq.)
uniform float u_mover_fam[8];      // familia de atlas 1..15
uniform int u_hazard_count;        // franjas de pinchos pixel-art (máx. 32)
uniform vec4 u_hazards[32];        // (x0, y0, w_px, dir: +1 suelo, -1 techo)
uniform int u_boss_count;          // bosses pixel-art (máx. 4)
uniform vec4 u_boss_rect[4];       // (x_center, y_feet, w, h)
uniform vec4 u_boss_state[4];      // (kind: 0=coloso1, 1=coloso2, 2=wyrm; dir; phase; hits)

// ---------------------------------------------------------------------------------------------
// Fondos (backdrops) procedurales por chunk: tema = u_bg_theme[vecindario 3×3] (ew::map::BackdropTheme).
// Todo en px enteros (pixel art, sin texturas). Capa "cerca" pegada al mundo y capa "lejos" con
// parallax 0.35 respecto del origen del chunk. Devuelve albedo + altura (para normales) y emisión.
uniform int u_bg_theme[9];
uniform int u_bg_seed[9];

float hash12(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}
float vnoise(vec2 p) {
  vec2 i = floor(p), f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  return mix(mix(hash12(i), hash12(i + vec2(1, 0)), f.x), mix(hash12(i + vec2(0, 1)), hash12(i + vec2(1, 1)), f.x), f.y);
}
// Ladrillos con traba: x = hash del ladrillo, y = distancia al borde (px), z = fila.
vec3 bricks(vec2 p, vec2 sz, float seed) {
  float row = floor(p.y / sz.y);
  vec2 q = vec2(p.x + mod(row, 2.0) * sz.x * 0.5, p.y);
  vec2 cell = floor(q / sz);
  vec2 f = q - cell * sz;
  float e = min(min(f.x, sz.x - 1.0 - f.x), min(f.y, sz.y - 1.0 - f.y));
  return vec3(hash12(cell + seed * 1.37), e, row);
}
float q4(float x) { return floor(clamp(x, 0.0, 0.999) * 4.0) / 4.0; }  // 4 tonos (look pixel art)
// Dither ordenado 4×4 (Bayer) para degradés sin banding suave.
float bayer4(vec2 p) {
  ivec2 i = ivec2(mod(p, 4.0));
  int m[16] = int[](0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5);
  return (float(m[i.y * 4 + i.x]) + 0.5) / 16.0;
}
float bevel(float e) { return e < 1.0 ? 0.0 : (e < 2.0 ? 0.6 : 1.0); }

// Partícula de 1 px en una grilla de celdas `cell`, que se mueve con `vel` (px/s).
float motes(vec2 p, float cell, vec2 vel, float seed, float density) {
  vec2 pp = p - vel * u_time;
  vec2 c = floor(pp / cell);
  float h = hash12(c + seed);
  if (h > density) return 0.0;
  vec2 pos = c * cell + floor(vec2(hash12(c + seed + 3.1), hash12(c + seed + 7.7)) * (cell - 2.0)) + 1.0;
  pos.x += floor(sin(u_time * 1.3 + h * 40.0) * 2.0);
  vec2 d = abs(floor(pp) - pos);
  float tw = 0.6 + 0.4 * sin(u_time * 3.0 + h * 60.0);
  return (d.x < 0.5 && d.y < 0.5) ? tw : 0.0;
}

// q = px dentro del chunk (0..320, 0..184), f = q con parallax (capa lejana), s = seed.
// Devuelve vec4(albedo, altura 0..1); emit = 0..1 (luz propia, no depende del player).
vec4 bg_cueva(vec2 q, vec2 f, float s, out float emit) {
  emit = 0.0;
  vec3 b = bricks(q + vec2(floor(vnoise(q * 0.05 + s) * 4.0), 0.0), vec2(24.0, 12.0), s);
  float n = vnoise(q * 0.25 + s);
  vec3 c = vec3(0.15, 0.17, 0.23) * (0.7 + 0.3 * q4(b.x)) * (0.85 + 0.15 * n);
  if (b.y < 1.0) c = vec3(0.05, 0.05, 0.08);
  return vec4(c, bevel(b.y) * (0.8 + 0.2 * n));
}

vec4 bg_pozo(vec2 q, vec2 f, float s, out float emit) {
  emit = 0.0;
  vec3 b = bricks(q, vec2(16.0, 8.0), s);
  float u = q.x / 320.0 * 2.0 - 1.0;
  float cyl = 1.0 - 0.65 * u * u;  // pared curva del pozo: más clara al centro
  vec3 c = vec3(0.12, 0.19, 0.21) * (0.75 + 0.25 * q4(b.x)) * cyl;
  float h = bevel(b.y);
  if (b.y < 1.0) c = vec3(0.03, 0.06, 0.07);
  // musgo abajo
  float moss = vnoise(q * vec2(0.2, 0.4) + s) + (q.y - 120.0) / 80.0;
  if (moss > 0.9 && b.y >= 1.0) c = mix(c, vec3(0.10, 0.22, 0.10), 0.7);
  // haz de luz desde la boca del pozo (capa lejana, dither)
  float half_w = 36.0 + f.y * 0.18;
  float band = 1.0 - smoothstep(half_w - 10.0, half_w, abs(f.x - 160.0 - 12.0 * sin(s)));
  float fall = clamp(1.0 - f.y / 184.0, 0.0, 1.0);
  if (band * fall * 0.8 > bayer4(q)) { c = mix(c, vec3(0.45, 0.62, 0.62), 0.22); emit = 0.12 * fall; }
  // gotas que caen (1×2 px)
  for (int i = 0; i < 4; ++i) {
    float x = floor(20.0 + hash12(vec2(float(i), s)) * 280.0);
    float y = mod(u_time * 70.0 + hash12(vec2(s, float(i))) * 400.0, 260.0) - 20.0;
    if (abs(q.x - x) < 0.5 && q.y >= y && q.y < y + 2.0) { c = vec3(0.55, 0.85, 0.95); emit = 0.8; }
  }
  return vec4(c, h);
}

vec4 bg_raices(vec2 q, vec2 f, float s, out float emit) {
  emit = 0.0;
  float n = vnoise(q * 0.12 + s), n2 = vnoise(q * 0.5 + s * 3.0);
  vec3 c = vec3(0.13, 0.10, 0.07) * (0.7 + 0.3 * q4(n)) * (0.9 + 0.1 * q4(n2));
  float h = 0.3 + 0.3 * n2;
  if (hash12(floor(q / 3.0) + s) > 0.985) { c *= 1.6; h = 0.9; }  // piedritas
  // troncos lejanos (silueta oscura con parallax)
  float tx = mod(f.x + s * 37.0, 110.0);
  if (abs(tx - 55.0) < 9.0 + 3.0 * sin(f.y * 0.04 + s)) c = mix(c, vec3(0.05, 0.06, 0.04), 0.75);
  // raíces colgantes: una por banda de 10 px, largo y ondulación al azar
  float k = floor(q.x / 10.0);
  float hk = hash12(vec2(k, s));
  float len = 30.0 + hk * 120.0;
  float cx = k * 10.0 + 5.0 + floor(sin(q.y * 0.06 + hk * 20.0) * 3.0);
  float wdt = mix(2.5, 0.8, q.y / len);
  if (q.y < len && abs(q.x - cx) < wdt) { c = vec3(0.22, 0.15, 0.08) * (0.8 + 0.2 * hk); h = 1.0; }
  // esporas que flotan
  float m = motes(q, 22.0, vec2(4.0, -6.0), s, 0.12);
  if (m > 0.0) { c = vec3(0.75, 0.90, 0.45); emit = 0.7 * m; }
  return vec4(c, h);
}

vec4 bg_grieta(vec2 q, vec2 f, float s, out float emit) {
  emit = 0.0;
  float n = vnoise(q * 0.08 + s);
  vec3 c = vec3(0.16, 0.08, 0.10) * (0.7 + 0.3 * q4(n));
  float h = 0.4 + 0.4 * n;
  // grietas: bordes de Voronoi (F2 - F1)
  vec2 g = q / 44.0, gi = floor(g);
  float d1 = 9.0, d2 = 9.0;
  for (int y = -1; y <= 1; ++y)
    for (int x = -1; x <= 1; ++x) {
      vec2 o = gi + vec2(x, y);
      vec2 pt = o + vec2(hash12(o + s), hash12(o + s + 5.3));
      float d = length(g - pt);
      if (d < d1) { d2 = d1; d1 = d; } else if (d < d2) d2 = d;
    }
  float edge = (d2 - d1) * 44.0;
  if (edge < 1.6) {
    c = vec3(0.04, 0.02, 0.03); h = 0.0;
    // brasas tenues y entrecortadas (no compiten con los pinchos rojos del primer plano)
    if (edge < 0.6 && vnoise(q * 0.15 + s) > 0.45) {
      float pulse = 0.5 + 0.5 * sin(u_time * 1.7 + hash12(gi + s) * 6.28);
      c = vec3(0.55, 0.26, 0.10); emit = 0.10 + 0.12 * pulse;
    }
  }
  // estalactitas lejanas
  float k = floor(f.x / 14.0);
  float L = 10.0 + hash12(vec2(k, s + 2.0)) * 50.0;
  float dx = abs(f.x - k * 14.0 - 7.0);
  if (f.y < L * (1.0 - dx / 7.0)) { c = vec3(0.07, 0.035, 0.045); h = 0.2; emit = 0.0; }
  return vec4(c, h);
}

float pipe_h(float d, float r) { return d < r ? sqrt(1.0 - (d * d) / (r * r)) : -1.0; }
vec4 bg_cisterna(vec2 q, vec2 f, float s, out float emit) {
  emit = 0.0;
  vec3 b = bricks(q, vec2(32.0, 16.0), s);
  vec3 c = vec3(0.12, 0.15, 0.20) * (0.75 + 0.25 * q4(b.x));
  float h = bevel(b.y);
  if (b.y < 1.0) c = vec3(0.04, 0.05, 0.07);
  // marcas del nivel del agua (bajo / alto) y verdín por debajo
  if (q.y > 128.0) c = mix(c, vec3(0.07, 0.14, 0.12), 0.45);
  if (abs(q.y - 128.0) < 1.0 || abs(q.y - 104.0) < 0.6) c = vec3(0.20, 0.27, 0.24);
  // caño horizontal con bridas y caño vertical
  float px = 72.0 + mod(s * 53.0, 176.0);
  float hp = pipe_h(abs(q.y - 44.0), 5.0);
  if (hp >= 0.0) {
    bool fl = mod(q.x + 8.0, 64.0) < 4.0;
    c = vec3(0.20, 0.22, 0.24) * (0.6 + 0.4 * hp) * (fl ? 1.25 : 1.0); h = hp;
  }
  float hv = pipe_h(abs(q.x - px), 4.0);
  if (hv >= 0.0 && q.y < 44.0) { c = vec3(0.19, 0.21, 0.23) * (0.6 + 0.4 * hv); h = hv; }
  // gota desde una brida
  float dx = floor(px + 32.0);
  float y = mod(u_time * 60.0 + s * 17.0, 160.0) + 50.0;
  if (abs(q.x - dx) < 0.5 && q.y >= y && q.y < y + 2.0) { c = vec3(0.55, 0.85, 0.95); emit = 0.8; }
  return vec4(c, h);
}

vec4 bg_nido(vec2 q, vec2 f, float s, out float emit) {
  emit = 0.0;
  vec3 b = bricks(q, vec2(12.0, 6.0), s);
  vec3 c = vec3(0.20, 0.14, 0.09) * (0.75 + 0.25 * q4(b.x));
  float h = bevel(b.y) * 0.8;
  if (b.y < 1.0) c = vec3(0.07, 0.05, 0.03);
  // nichos con arco cada 64 px; algunos con huevo
  float k = floor((q.x + 8.0) / 64.0);
  vec2 nc = vec2(k * 64.0 + 24.0, 84.0);
  vec2 d = q - nc;
  bool in_rect = abs(d.x) < 13.0 && d.y > -6.0 && d.y < 22.0;
  bool in_arch = d.y <= -6.0 && length(vec2(d.x, d.y + 6.0)) < 13.0;
  if (in_rect || in_arch) {
    c = vec3(0.06, 0.04, 0.03); h = 0.1;
    float he = hash12(vec2(k, s + 9.0));
    vec2 e = (q - vec2(nc.x, nc.y + 13.0)) / vec2(5.0, 7.0);
    e.x *= 1.0 + 0.15 * clamp(-e.y, 0.0, 1.0);
    if (he > 0.3 && dot(e, e) < 1.0) {
      // huevo con dibujo (franjas / puntos según el nicho)
      vec3 shell = mix(vec3(0.55, 0.47, 0.33), vec3(0.42, 0.50, 0.55), step(0.65, he));
      float pat = he > 0.65 ? step(0.5, fract((q.y - nc.y) / 4.0)) : step(0.8, hash12(floor(q / 2.0)));
      c = shell * (0.8 + 0.2 * pat); h = sqrt(max(0.0, 1.0 - dot(e, e)));
    }
  } else if ((abs(d.x) < 15.0 && abs(d.x) >= 13.0 && d.y > -6.0 && d.y < 22.0) ||
             (d.y <= -6.0 && abs(length(vec2(d.x, d.y + 6.0)) - 14.0) < 1.0)) {
    c = vec3(0.30, 0.22, 0.12); h = 1.0;  // marco del nicho
  }
  // luciérnagas
  float m = motes(q, 30.0, vec2(3.0 * sin(u_time * 0.3 + s), -2.0), s, 0.4);
  if (m > 0.0) { c = vec3(1.0, 0.90, 0.45); emit = 0.9 * m; }
  return vec4(c, h);
}

vec4 bg_engranajes(vec2 q, vec2 f, float s, out float emit) {
  emit = 0.0;
  // paneles remachados
  vec2 cell = floor(q / vec2(40.0, 24.0)), pf = q - cell * vec2(40.0, 24.0);
  vec3 c = vec3(0.14, 0.10, 0.07) * (0.8 + 0.2 * q4(hash12(cell + s)));
  float h = 0.6;
  if (pf.x < 1.0 || pf.y < 1.0) { c = vec3(0.05, 0.04, 0.03); h = 0.0; }
  vec2 rv = min(pf, vec2(40.0, 24.0) - pf);
  if (abs(rv.x - 4.0) < 1.0 && abs(rv.y - 4.0) < 1.0) { c = vec3(0.24, 0.18, 0.12); h = 1.0; }
  // engranajes lejanos que giran
  for (int i = 0; i < 3; ++i) {
    float fi = float(i);
    vec2 ctr = vec2(50.0 + fi * 110.0 + hash12(vec2(fi, s)) * 30.0, 40.0 + hash12(vec2(s, fi)) * 100.0);
    float R = 26.0 + hash12(vec2(fi + 4.0, s)) * 22.0;
    float teeth = floor(R / 4.0);
    float dir = mod(fi, 2.0) < 1.0 ? 1.0 : -1.0;
    vec2 d = f - ctr;
    float r = length(d);
    float a = atan(d.y, d.x) + dir * u_time * (12.0 / R);
    float tooth = step(0.5, fract(a * teeth / 6.2831853)) * 4.0;
    float spokes = abs(fract(a * 3.0 / 6.2831853 + 0.5) - 0.5) * r;  // 6 rayos
    bool body = r < R + tooth && (r < R * 0.32 || r > R * 0.72 || spokes < 3.0) && r > 3.0;
    if (body) { c = vec3(0.075, 0.06, 0.05) * (r > R - 1.0 && tooth > 0.0 ? 1.4 : 1.0); h = 0.3; }
  }
  // cadenas que bajan
  for (int i = 0; i < 2; ++i) {
    float x = 30.0 + float(i) * 250.0 + floor(mod(s * 11.0, 20.0));
    float y = q.y + u_time * 8.0 * (i == 0 ? 1.0 : -1.0);
    float ly = mod(y, 8.0);
    vec2 d = vec2(q.x - x, ly - 4.0);
    bool flat_link = mod(floor(y / 8.0), 2.0) < 1.0;
    float ring = flat_link ? abs(length(d / vec2(3.0, 4.0)) - 1.0) : (abs(d.x) < 0.6 && abs(d.y) < 4.0 ? 0.0 : 9.0);
    if (ring < 0.3) { c = vec3(0.26, 0.20, 0.13); h = 1.0; }
  }
  return vec4(c, h);
}

vec4 bg_invertido(vec2 q, vec2 f, float s, out float emit) {
  emit = 0.0;
  float m = abs(q.y - 92.0);  // espejado techo / piso
  vec3 b = bricks(vec2(q.x, m), vec2(20.0, 10.0), s);
  vec3 c = vec3(0.12, 0.10, 0.20) * (0.75 + 0.25 * q4(b.x));
  float h = bevel(b.y) * 0.7;
  if (b.y < 1.0) c = vec3(0.04, 0.03, 0.08);
  // franja central más oscura con estrellas (el "vacío" entre los dos pisos)
  if (m < 34.0) {
    c = vec3(0.05, 0.04, 0.10); h = 0.0;
    if (hash12(floor(f) + s) > 0.997) { c = vec3(0.8, 0.75, 1.0); emit = 0.6; }
  }
  // columnas espejadas cada 48 px (fuste + capitel), desde techo y piso hacia el medio
  float k = mod(q.x + mod(s * 13.0, 48.0), 48.0) - 24.0;
  if (m >= 34.0 && abs(k) < 6.0) { c = vec3(0.18, 0.15, 0.28) * (0.8 + 0.2 * (1.0 - abs(k) / 6.0)); h = 1.0 - abs(k) / 6.0; }
  if (m >= 34.0 && m < 40.0 && abs(k) < 9.0) { c = vec3(0.22, 0.19, 0.33); h = 1.0; }
  // polvo que "cae" hacia arriba
  float p = motes(q, 18.0, vec2(0.0, -18.0), s, 0.3);
  if (p > 0.0) { c = vec3(0.80, 0.70, 1.0); emit = 0.7 * p; }
  return vec4(c, h);
}

vec4 bg_arena(vec2 q, vec2 f, float s, out float emit) {
  emit = 0.0;
  vec3 b = bricks(q, vec2(20.0, 10.0), s);
  vec3 c = vec3(0.17, 0.07, 0.07) * (0.75 + 0.25 * q4(b.x));
  float h = bevel(b.y);
  if (b.y < 1.0) c = vec3(0.06, 0.02, 0.02);
  // arcada lejana de coliseo (dos pisos)
  for (int tier = 0; tier < 2; ++tier) {
    float per = tier == 0 ? 56.0 : 40.0, rad = tier == 0 ? 20.0 : 13.0, top = tier == 0 ? 84.0 : 28.0;
    float k = mod(f.x + per * 0.5, per) - per * 0.5;
    float bottom = tier == 0 ? 150.0 : 56.0;
    bool hole = abs(k) < rad && f.y < bottom && (f.y > top || length(vec2(k, f.y - top)) < rad);
    if (hole) { c = vec3(0.05, 0.02, 0.03); h = 0.0; }
  }
  // sello de la bestia en el centro: anillo + ojo
  vec2 d = q - vec2(160.0, 70.0);
  float r = length(d);
  float pulse = 0.5 + 0.5 * sin(u_time * 0.9);
  if (abs(r - 30.0) < 1.5 || abs(r - 25.0) < 0.7) { c = vec3(0.45, 0.30, 0.10); h = 1.0; emit = 0.08 + 0.08 * pulse; }
  vec2 e = d / vec2(16.0, 7.0);
  float lid = abs(d.y) - 7.0 * (1.0 - (d.x * d.x) / 256.0);
  if (lid < 0.0 && abs(d.x) < 16.0) {
    c = vec3(0.30, 0.18, 0.08); h = 0.8;
    if (r < 4.5) { c = vec3(0.95, 0.40, 0.15); emit = 0.25 + 0.3 * pulse; }
  }
  // antorchas a los costados (soporte + llama que titila)
  for (int i = 0; i < 2; ++i) {
    vec2 tp = vec2(i == 0 ? 44.0 : 276.0, 96.0);
    vec2 t = q - tp;
    if (abs(t.x) < 1.5 && t.y > 0.0 && t.y < 10.0) { c = vec3(0.22, 0.14, 0.08); h = 1.0; }
    float fl = vnoise(vec2(q.x * 0.6, q.y * 0.4 + u_time * 9.0) + float(i) * 10.0);
    vec2 ft = vec2(t.x, t.y + 4.0) / vec2(3.0, 6.0);
    if (dot(ft, ft) < 0.6 + 0.6 * fl && t.y < 0.0) { c = mix(vec3(1.0, 0.45, 0.12), vec3(1.0, 0.85, 0.4), fl); emit = 1.0; }
  }
  return vec4(c, h);
}

// 10: Cripta Dracónica — arcos góticos de pizarra, nichos funerarios y braseros violetas
vec4 bg_cripta(vec2 q, vec2 f, float s, out float emit) {
  emit = 0.0;
  float h = 0.0;
  float col_x = mod(q.x, 64.0);
  float mor = (mod(q.y, 12.0) < 1.0 || mod(q.x + floor(q.y / 12.0) * 10.0, 20.0) < 1.0) ? 0.0 : 1.0;
  vec3 c = mix(vec3(0.04, 0.04, 0.06), vec3(0.08, 0.08, 0.12), mor * (0.65 + 0.35 * vnoise(q * 0.08)));
  // arcos apuntados cada 64 px
  float ax = abs(col_x - 32.0);
  float arch_y = 34.0 + sqrt(max(0.0, 26.0 * 26.0 - ax * ax)) * 0.85;
  if (ax > 21.0 && q.y > 32.0) {
    c = vec3(0.11, 0.10, 0.15);
    h = 0.55;
  } else if (q.y < arch_y && q.y > 26.0 && ax < 18.0) {
    // hueco profundo del nicho con reliquia tenue
    c = vec3(0.03, 0.03, 0.05);
    if (abs(ax - 0.0) < 3.5 && q.y > 72.0 && q.y < 86.0) {
      c = vec3(0.16, 0.14, 0.20);
      h = 0.4;
    }
  }
  // estandartes rasgados + fuego espectral tenue en columnas alternas
  float banner_x = abs(mod(q.x + 32.0, 128.0) - 64.0);
  if (banner_x < 4.5 && q.y > 42.0 && q.y < 76.0 - mod(banner_x, 3.0) * 2.0) {
    c = vec3(0.16, 0.06, 0.09);
    h = 0.6;
  }
  for (int i = 0; i < 2; ++i) {
    vec2 bp = vec2(i == 0 ? 64.0 : 256.0, 108.0);
    vec2 d = q - bp;
    if (abs(d.x) < 4.0 && d.y >= 0.0 && d.y < 5.0) { c = vec3(0.14, 0.13, 0.17); h = 0.8; }
    float fl = vnoise(vec2(q.x * 0.5, q.y * 0.4 + u_time * 7.5) + float(i) * 5.3);
    if (abs(d.x) < 2.5 && d.y < 0.0 && d.y > -5.5 - fl * 2.0) {
      c = mix(vec3(0.32, 0.16, 0.48), vec3(0.55, 0.40, 0.72), fl);
      emit = 0.55;
    }
  }
  return vec4(c, h);
}

// 11: Guarida del Wyrm — estrato volcánico oscuro, costillar fósil y vetas de brasas
vec4 bg_guarida(vec2 q, vec2 f, float s, out float emit) {
  emit = 0.0;
  float h = 0.0;
  float strata = sin(q.y * 0.11 + vnoise(q * 0.04) * 3.0);
  vec3 c = mix(vec3(0.05, 0.03, 0.04), vec3(0.10, 0.05, 0.05), smoothstep(-0.3, 0.6, strata));
  // grietas incandescentes en la roca basáltica (muy tenues para preservar contraste)
  float crack = vnoise(vec2(q.x * 0.09, q.y * 0.14));
  if (abs(crack - 0.5) < 0.022 && q.y > 92.0) {
    c = mix(vec3(0.45, 0.12, 0.04), vec3(0.70, 0.28, 0.06), 0.5 + 0.5 * sin(u_time * 3.2 + q.x * 0.1));
    emit = 0.45;
  }
  // costillar gigante de dragón al fondo
  vec2 rc = q - vec2(160.0, 126.0);
  if (abs(rc.x) < 76.0) {
    float rib_id = floor((rc.x + 76.0) / 18.0);
    float rx = mod(rc.x + 76.0, 18.0) - 9.0;
    float arch = -abs(rc.x) * 0.18 + 64.0;
    float rib_y = 122.0 - arch + rx * rx * 0.18;
    if (abs(q.y - rib_y) < 2.2 && q.y < 126.0) {
      c = vec3(0.17, 0.14, 0.12);
      h = 0.65;
    }
    if (mod(rib_id, 2.0) < 0.5 && abs(q.y - 104.0) < 3.0 && abs(rx) < 5.0) {
      c = vec3(0.14, 0.11, 0.10);
      h = 0.5;
    }
  }
  return vec4(c, h);
}

// 12: Santuario del Huevo — vitrales ojivales tenues, pilares de mármol y runas arcanas
vec4 bg_santuario(vec2 q, vec2 f, float s, out float emit) {
  emit = 0.0;
  float h = 0.0;
  vec3 c = vec3(0.04, 0.05, 0.08);
  float panel = mod(q.x, 80.0);
  if (panel < 10.0 || panel > 70.0) {
    // pilastras acanaladas
    float flute = sin(panel * 1.4);
    c = mix(vec3(0.08, 0.09, 0.13), vec3(0.13, 0.15, 0.21), 0.5 + 0.5 * flute);
    h = 0.7;
  } else {
    // rosetón y vitral ojival central
    vec2 w = vec2(panel - 40.0, q.y - 68.0);
    float r = length(w);
    if (abs(w.x) < 16.0 && q.y > 40.0 && q.y < 118.0) {
      float tint = step(0.0, sin(w.x * 0.35) * cos(w.y * 0.25));
      c = mix(vec3(0.07, 0.14, 0.18), vec3(0.12, 0.10, 0.22), tint);
      if (abs(r - 12.0) < 1.4 || abs(w.x) < 1.0) {
        c = vec3(0.05, 0.06, 0.09);
        h = 0.5;
      } else {
        emit = 0.18;
      }
    }
  }
  // runas flotantes muy sutiles
  float rn = vnoise(floor(q / 6.0) * 1.37 + vec2(0.0, floor(u_time * 0.8)));
  if (rn > 0.91 && mod(q.x, 6.0) > 1.5 && mod(q.y, 6.0) > 1.5 && panel >= 12.0 && panel <= 68.0) {
    c = vec3(0.22, 0.42, 0.52);
    emit = 0.35;
  }
  return vec4(c, h);
}

// 13: Abismo Arcano — estalactitas de obsidiana, bruma violácea y ojos lejanos en la penumbra
vec4 bg_abismo(vec2 q, vec2 f, float s, out float emit) {
  emit = 0.0;
  float h = 0.0;
  float fog = vnoise(vec2(q.x * 0.03 + u_time * 0.15, q.y * 0.05));
  vec3 c = mix(vec3(0.02, 0.02, 0.05), vec3(0.06, 0.04, 0.10), fog);
  // siluetas de agujas de obsidiana en fondo y techo
  float spire_top = 18.0 + 22.0 * abs(sin(q.x * 0.045));
  float spire_bot = 162.0 - 24.0 * abs(cos(q.x * 0.037 + 1.1));
  if (q.y < spire_top || q.y > spire_bot) {
    c = vec3(0.04, 0.04, 0.07);
    h = 0.45;
  }
  // grietas arcanas distantes + pares de ojos dracónicos parpadeando
  for (int i = 0; i < 3; ++i) {
    vec2 ep = vec2(58.0 + float(i) * 98.0, 52.0 + float((i * 2) % 3) * 24.0);
    float blink = step(0.12, fract(u_time * 0.22 + float(i) * 0.37));
    if (blink > 0.5 && abs(q.y - ep.y) < 1.2 && (abs(q.x - (ep.x - 3.0)) < 1.5 || abs(q.x - (ep.x + 3.0)) < 1.5)) {
      c = vec3(0.65, 0.28, 0.16);
      emit = 0.7;
    }
  }
  return vec4(c, h);
}

vec4 bg_theme_px(int theme, vec2 q, vec2 f, float s, out float emit) {
  emit = 0.0;
  if (theme == 1) return bg_pozo(q, f, s, emit);
  if (theme == 2) return bg_raices(q, f, s, emit);
  if (theme == 3) return bg_grieta(q, f, s, emit);
  if (theme == 4) return bg_cisterna(q, f, s, emit);
  if (theme == 5) return bg_nido(q, f, s, emit);
  if (theme == 6) return bg_engranajes(q, f, s, emit);
  if (theme == 7) return bg_invertido(q, f, s, emit);
  if (theme == 8) return bg_arena(q, f, s, emit);
  if (theme == 9) return vec4(0.05, 0.06, 0.10, 0.0);  // vacio: el fondo liso de antes
  if (theme == 10) return bg_cripta(q, f, s, emit);
  if (theme == 11) return bg_guarida(q, f, s, emit);
  if (theme == 12) return bg_santuario(q, f, s, emit);
  if (theme == 13) return bg_abismo(q, f, s, emit);
  return bg_cueva(q, f, s, emit);
}

// Fondo en px de mundo: elige tema por chunk, calcula normal por diferencias de altura.
// lr (respuesta a la luz) = 0 → el light pass lo trata como fondo (menos luz del player).
vec3 backdrop(vec2 world, vec2 local, out vec3 n, out float emit, out float lr) {
  vec2 csz = vec2(320.0, 184.0);
  ivec2 c = clamp(ivec2(floor(local / csz)), ivec2(0), ivec2(2));
  int idx = c.y * 3 + c.x;
  int theme = u_bg_theme[idx];
  float s = float(u_bg_seed[idx] % 997) + 1.0;
  vec2 corg = u_chunk + vec2(c) * csz;   // origen del chunk en px de mundo
  vec2 q = floor(world - corg);
  vec2 f = floor(q + (u_cam - corg) * 0.35);  // capa lejana: se mueve más lento que el mundo
  float e1, e2, e3;
  vec4 a = bg_theme_px(theme, q, f, s, e1);
  vec4 ax = bg_theme_px(theme, q + vec2(1.0, 0.0), f + vec2(1.0, 0.0), s, e2);
  vec4 ay = bg_theme_px(theme, q + vec2(0.0, 1.0), f + vec2(0.0, 1.0), s, e3);
  n = normalize(vec3(-(ax.a - a.a) * 1.5, (ay.a - a.a) * 1.5, 1.0));
  emit = e1;
  lr = theme == 9 ? 1.0 : 0.0;
  return a.rgb;
}

// Texel del atlas para el tile id `vis` en coord local l (0..7).
vec4 atlas_texel(uint vis, ivec2 l) {
  ivec2 o = ivec2(int(vis % 16u), int(vis / 16u)) * 8;
  return texelFetch(u_atlas, o + clamp(l, ivec2(0), ivec2(7)), 0);
}
float atlas_h(uint vis, ivec2 l) { return atlas_texel(vis, l).a; }
vec4 sprite_texel(ivec2 cell, ivec2 l) {
  if (l.x < 0 || l.y < 0 || l.x > 15 || l.y > 15) return vec4(0.0);
  return texelFetch(u_sprites, cell * 16 + l, 0);
}
// Bisel de borde solo en lados sin vecino (máscara 4-bit: N=1 E=2 S=4 W=8).
void autotile_bevel(inout vec3 n, uint vis, ivec2 l) {
  uint m = vis & 15u;
  if (l.x == 0 && (m & 8u) == 0u) n = normalize(vec3(-1.0, 0.0, 1.0));
  else if (l.x == 7 && (m & 2u) == 0u) n = normalize(vec3(1.0, 0.0, 1.0));
  if (l.y == 0 && (m & 1u) == 0u) n = normalize(vec3(0.0, 1.0, 1.0));
  else if (l.y == 7 && (m & 4u) == 0u) n = normalize(vec3(0.0, -1.0, 1.0));
}

void main() {
  vec2 world = u_cam + vec2(v_uv.x * 320.0, (1.0 - v_uv.y) * 180.0);
  vec2 local = world - u_chunk;
  ivec2 t = ivec2(floor(local / 8.0));
  uvec3 cv = uvec3(0u);
  bool inside = t.x >= 0 && t.y >= 0 && t.x < 120 && t.y < 69;
  if (inside) cv = texelFetch(u_tiles, ivec2(t.x, t.y), 0).rgb;
  bool solid = cv.r != 0u;
  vec3 n = vec3(0.0, 0.0, 1.0);
  float emit = 0.0, lr = 1.0;
  vec3 albedo = vec3(0.05, 0.06, 0.10);
  float depth = 1.0;
  ivec2 l = ivec2(floor(local)) - t * 8;
  if (!solid && cv.g == 0u) albedo = backdrop(world, local, n, emit, lr);
  if (solid) {
    albedo = atlas_texel(cv.g, l).rgb;
    // Normal desde la altura del atlas (diferencias centrales) + bisel de borde.
    float dx = atlas_h(cv.g, l + ivec2(1, 0)) - atlas_h(cv.g, l - ivec2(1, 0));
    float dy = atlas_h(cv.g, l + ivec2(0, 1)) - atlas_h(cv.g, l - ivec2(0, 1));
    n = normalize(vec3(-dx, dy, 1.0));
    autotile_bevel(n, cv.g, l);
    depth = 0.5;
  } else if (cv.g != 0u) {
    albedo = atlas_texel(cv.g, l).rgb * 0.35;  // decorado de fondo (VIS sin COL)
  }
  if (solid || cv.g != 0u) { emit = 0.0; lr = 1.0; }
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
        emit = 0.0; lr = 1.0;
      }
    }
  }
  // Plataformas móviles: tiles del atlas, autotile 4 vecinos dentro del rect, depth 0.35.
  for (int i = 0; i < u_mover_count; ++i) {
    vec4 mr = u_mover_rect[i];
    vec2 rel = world - mr.xy;
    if (rel.x >= 0.0 && rel.y >= 0.0 && rel.x < mr.z && rel.y < mr.w) {
      int nw = max(int(floor(mr.z / 8.0 + 0.5)), 1);
      int nh = max(int(floor(mr.w / 8.0 + 0.5)), 1);
      ivec2 cell = ivec2(clamp(int(floor(rel.x / 8.0)), 0, nw - 1),
                         clamp(int(floor(rel.y / 8.0)), 0, nh - 1));
      ivec2 ml = ivec2(floor(rel)) - cell * 8;
      uint mask = 0u;
      if (cell.y > 0) mask |= 1u;
      if (cell.x < nw - 1) mask |= 2u;
      if (cell.y < nh - 1) mask |= 4u;
      if (cell.x > 0) mask |= 8u;
      uint vis = uint(u_mover_fam[i]) * 16u + mask;
      albedo = atlas_texel(vis, ml).rgb;
      float mdx = atlas_h(vis, ml + ivec2(1, 0)) - atlas_h(vis, ml - ivec2(1, 0));
      float mdy = atlas_h(vis, ml + ivec2(0, 1)) - atlas_h(vis, ml - ivec2(0, 1));
      n = normalize(vec3(-mdx, mdy, 1.0));
      autotile_bevel(n, vis, ml);
      depth = 0.35;
      emit = 0.0; lr = 1.0;
    }
  }
  // Pinchos pixel-art afilados por tramo horizontal (META 4): acero oscuro y punta carmesí
  for (int i = 0; i < u_hazard_count; ++i) {
    vec4 hz = u_hazards[i];
    if (world.x >= hz.x && world.x < hz.x + hz.z &&
        world.y >= hz.y && world.y < hz.y + 8.0) {
      float lx = mod(world.x - hz.x, 4.0);
      float ly = world.y - hz.y;
      // py = altura desde la base (0 = base apoyada, 7 = punta libre)
      float py = hz.w > 0.0 ? (7.5 - ly) : ly;
      float half_w = clamp((7.0 - py) * 0.29 + 0.35, 0.35, 2.0);
      float dx = abs(lx - 2.0);
      if (dx <= half_w && py >= 0.0 && py < 7.6) {
        bool tip = py > 4.9;
        bool edge = dx > half_w - 0.55 || py < 1.1;
        albedo = tip ? vec3(0.86, 0.14, 0.18)
                     : (edge ? vec3(0.18, 0.20, 0.25)
                             : (lx < 2.0 ? vec3(0.62, 0.68, 0.78) : vec3(0.36, 0.40, 0.48)));
        n = normalize(vec3((lx < 2.0 ? -0.55 : 0.55), hz.w > 0.0 ? -0.25 : 0.25, 1.0));
        depth = 0.34;
        emit = tip ? 0.25 : 0.0;
        lr = 1.0;
      }
    }
  }
  // Marcadores (actores)
  for (int i = 0; i < u_marker_count; ++i) {
    vec4 m = u_markers[i];
    vec2 d = world - m.xy;
    bool hit = m.w < 0.5 ? dot(d, d) <= m.z * m.z : max(abs(d.x), abs(d.y)) <= m.z;
    if (hit) {
      albedo = u_marker_colors[i];
      n = m.w < 0.5 && m.z > 1.5 ? normalize(vec3(d.x / m.z, -d.y / m.z, 1.0)) : vec3(0.0, 0.0, 1.0);
      depth = 0.3;
      emit = 0.0; lr = 1.0;
    }
  }
  // Bosses pixel-art (Coloso del Sello 24×16 y Wyrm Ancestral 32×22) con corona dorada de pureza.
  for (int bi = 0; bi < u_boss_count; ++bi) {
    vec4 br = u_boss_rect[bi];
    vec4 bs = u_boss_state[bi];
    int kind = int(floor(bs.x + 0.5));
    float dir = bs.y >= 0.0 ? 1.0 : -1.0;
    int phase = int(floor(bs.z + 0.5));
    float hits = bs.w;
    float tell = (phase == 1 || phase == 3) ? 1.0 : 0.0;
    float fx = floor(world.x - br.x) * dir;
    float fy = floor(world.y - (br.y - br.w));
    if (kind < 2) {
      // Coloso Acorazado (24×16 px): kind 0 = Carcelero Carmesí (Nivel 1), kind 1 = Arconte Abisal (Nivel 2)
      if (abs(fx) <= 13.0 && fy >= -2.0 && fy < 16.0) {
        float stride = (phase == 0 || phase == 3) ? floor(sin(u_time * 12.0 + sign(fx) * 1.57) * 1.5) : 0.0;
        vec3 arm_hi = kind == 1 ? vec3(0.42, 0.24, 0.58) : vec3(0.56, 0.16, 0.20);
        vec3 arm_lo = kind == 1 ? vec3(0.18, 0.11, 0.28) : vec3(0.24, 0.08, 0.11);
        vec3 trim   = kind == 1 ? vec3(0.75, 0.56, 0.95) : vec3(0.78, 0.48, 0.22);
        vec3 glow_c = kind == 1 ? vec3(0.95, 0.38, 0.88) : vec3(1.0, 0.48, 0.14);
        if (tell > 0.5) { arm_hi += vec3(0.22, 0.12, 0.08); glow_c = vec3(1.0, 0.85, 0.28); }
        bool drawn = false;
        vec3 col = arm_lo;
        float e = 0.0;
        vec2 norm_xy = vec2(fx * 0.07 * dir, (fy - 8.0) * 0.08);
        // 1) Corona dorada de la Pureza (punto débil superior: fy -1..2, abs(fx) <= 4)
        bool prong = (fy <= 0.0 && (abs(fx) < 0.8 || abs(abs(fx) - 3.0) < 0.9));
        bool band  = (fy >= 0.0 && fy <= 2.0 && abs(fx) <= 3.5);
        if ((fy >= -1.0 && prong) || band) {
          drawn = true;
          bool gem = (abs(fx) < 1.0 && fy == 1.0);
          col = gem ? vec3(0.45, 0.95, 1.0) : vec3(1.0, 0.84, 0.24);
          e = gem ? 0.85 : 0.52;
          norm_xy = vec2(0.0, -0.45);
        }
        // 2) Cuernos curvos laterales (abs(fx) 6..11, fy 0..5)
        else if (abs(fx) >= 6.0 && abs(fx) <= 11.0 && fy >= 0.0 && fy <= 5.0 &&
                 abs(fy - (5.0 - (abs(fx) - 6.0) * 0.85)) <= 1.3) {
          drawn = true;
          col = mix(vec3(0.62, 0.54, 0.44), vec3(0.88, 0.80, 0.66), (5.0 - fy) * 0.2);
          norm_xy = vec2(sign(fx) * 0.45 * dir, -0.35);
        }
        // 3) Yelmo, hombreras segmentadas y torso acorazado (fy 2..12)
        else if (fy >= 2.0 && fy <= 12.0 && abs(fx) <= (fy < 6.0 ? 6.5 : 10.5)) {
          drawn = true;
          bool edge = abs(fx) >= (fy < 6.0 ? 5.5 : 9.5) || fy == 2.0 || fy == 12.0;
          bool pauldron = fy >= 5.0 && fy <= 9.0 && abs(fx) >= 6.0;
          bool visor = (fy >= 4.0 && fy <= 5.0 && fx >= -2.0 && fx <= 5.0);
          bool eye = (fy == 4.0 && (abs(fx - 1.0) < 0.8 || abs(fx - 4.0) < 0.8));
          bool core = (fy >= 7.0 && fy <= 10.0 && abs(fx - 0.5) <= 2.5);
          if (eye) {
            col = vec3(1.0, 0.92, 0.35);
            e = 0.85;
          } else if (visor) {
            col = vec3(0.06, 0.04, 0.08);
          } else if (core) {
            float pulse = 0.65 + 0.35 * sin(u_time * (6.0 + hits * 3.0));
            bool inner = (fy >= 8.0 && fy <= 9.0 && abs(fx - 0.5) <= 1.2);
            col = inner ? vec3(1.0, 0.90, 0.45) : glow_c * pulse;
            e = inner ? 0.80 : (0.45 + 0.35 * tell);
          } else if (pauldron) {
            bool rib = mod(floor(abs(fx) + fy), 3.0) < 1.0;
            col = rib ? trim : arm_hi;
          } else {
            col = edge ? arm_lo : mix(arm_lo, arm_hi, 0.65);
          }
        }
        // 4) Guantelete / garra frontal (fx 9..13, fy 7..11)
        else if (fx >= 9.0 && fx <= 13.0 && fy >= 7.0 && fy <= 11.0) {
          drawn = true;
          col = (fx >= 12.0) ? vec3(0.92, 0.86, 0.72) : trim;
          e = (tell > 0.5 && fx >= 12.0) ? 0.45 : 0.0;
        }
        // 5) Grebas / piernas articuladas (fy 12..15)
        else if (fy >= 12.0 && fy < 16.0 && abs(abs(fx) - 4.5) <= 2.5) {
          float leg_y = fy + (fx > 0.0 ? stride : -stride) * 0.5;
          if (leg_y < 15.8) {
            drawn = true;
            col = (fy >= 14.0) ? trim * 0.75 : arm_lo;
          }
        }
        if (drawn) {
          albedo = clamp(col, 0.0, 1.0);
          n = normalize(vec3(norm_xy, 1.0));
          depth = 0.29;
          emit = e;
          lr = 1.0;
        }
      }
    } else {
      // Wyrm Ancestral (32×22 px): dragón alado multipatrón con escamas, alas membranosas y fauces ígneas
      if (fx >= -18.0 && fx <= 17.0 && fy >= -2.0 && fy < 22.0) {
        float flap = floor(sin(u_time * (phase >= 2 ? 18.0 : 7.0)) * 2.5);
        float stride = (phase == 0 || phase == 3) ? floor(sin(u_time * 11.0 + sign(fx) * 1.57) * 1.5) : 0.0;
        vec3 sc_hi = vec3(0.68, 0.14, 0.18) + vec3(0.20, 0.12, 0.04) * tell;
        vec3 sc_lo = vec3(0.28, 0.06, 0.10);
        vec3 belly = vec3(0.82, 0.56, 0.22);
        bool drawn = false;
        vec3 col = sc_lo;
        float e = 0.0;
        vec2 norm_xy = vec2(fx * 0.05 * dir, (fy - 11.0) * 0.06);
        // 1) Corona dorada del Wyrm sobre el cráneo (fx 1..8, fy -1..2)
        bool c_prong = (fy <= 0.0 && (abs(fx - 2.0) < 0.8 || abs(fx - 4.5) < 0.8 || abs(fx - 7.0) < 0.8));
        bool c_band  = (fy >= 0.0 && fy <= 2.0 && fx >= 1.5 && fx <= 7.5);
        if ((fy >= -1.0 && c_prong) || c_band) {
          drawn = true;
          bool gem = (abs(fx - 4.5) < 1.0 && fy == 1.0);
          col = gem ? vec3(0.50, 0.96, 1.0) : vec3(1.0, 0.86, 0.25);
          e = gem ? 0.90 : 0.58;
          norm_xy = vec2(0.0, -0.45);
        }
        // 2) Cuernos dracónicos barridos hacia atrás (fx -5..2, fy 0..4)
        else if (fx >= -5.0 && fx <= 2.0 && fy >= 0.0 && fy <= 4.5 &&
                 abs(fy - (1.0 + (fx + 5.0) * 0.42)) <= 1.2) {
          drawn = true;
          col = mix(vec3(0.58, 0.48, 0.36), vec3(0.92, 0.82, 0.64), (2.0 - fx) / 7.0);
          norm_xy = vec2(-0.35 * dir, -0.35);
        }
        // 3) Cabeza, hocico escamado y fauces en llamas (fx 2..16, fy 2..10)
        else if (fx >= 2.0 && fx <= 16.0 && fy >= 2.0 && fy <= 10.0 &&
                 (fy <= 7.0 || (fx <= 14.0 && fy <= (tell > 0.5 || phase == 2 ? 10.0 : 8.5)))) {
          drawn = true;
          bool eye = (fx >= 6.0 && fx <= 8.0 && fy >= 3.5 && fy <= 5.0);
          bool nostril = (fx >= 14.0 && fx <= 15.0 && fy == 4.0);
          bool maw = (fx >= 7.0 && fx <= 14.5 && fy >= 7.0 && fy <= 9.0);
          bool fang = (maw && fy == 7.0 && mod(fx, 2.0) < 1.0);
          if (eye) {
            col = vec3(1.0, 0.92, 0.32);
            e = 0.92;
          } else if (fang) {
            col = vec3(0.95, 0.92, 0.86);
          } else if (maw && (tell > 0.5 || phase >= 2)) {
            col = vec3(1.0, 0.52, 0.12);
            e = 0.88;
          } else if (nostril) {
            col = vec3(1.0, 0.45, 0.10);
            e = 0.65;
          } else {
            bool sc = mod(fx + fy, 2.0) < 1.0;
            col = sc ? sc_hi : sc_lo;
          }
          norm_xy = vec2(0.35 * dir, (fy - 5.0) * 0.1);
        }
        // 4) Alas membranosas batientes con nervaduras de brasa (fx -13..3, fy 1..13)
        else if (fx >= -13.0 && fx <= 3.0 && fy >= 1.0 + flap && fy <= 12.0 + flap &&
                 fy <= 13.0 + flap - abs(fx + 5.0) * 0.35) {
          drawn = true;
          float wy = fy - flap;
          bool rib = abs(wy - 2.0) < 1.0 || abs(fx + 12.0) < 1.0 || abs(fx + 7.0 + (wy - 2.0) * 0.4) < 0.9 ||
                     abs(fx + 2.0 + (wy - 2.0) * 0.3) < 0.9;
          col = rib ? vec3(0.76, 0.26, 0.18) : vec3(0.34, 0.08, 0.14);
          e = rib ? (0.22 + 0.38 * tell) : 0.05;
          norm_xy = vec2(-0.25 * dir, -0.30);
        }
        // 5) Torso acorazado de escamas, pecho dorado y núcleo ígneo (fx -9..8, fy 7..17)
        else if (fx >= -9.0 && fx <= 8.0 && fy >= 7.0 && fy <= 17.0 &&
                 (fx * fx) / 81.0 + ((fy - 12.0) * (fy - 12.0)) / 30.0 <= 1.05) {
          drawn = true;
          bool underbelly = (fx >= 2.0 && fy >= 9.0 && fy <= 16.0);
          bool heart = (abs(fx - 1.0) <= 2.0 && abs(fy - 12.0) <= 2.0);
          if (heart) {
            float p = 0.65 + 0.35 * sin(u_time * (7.0 + hits * 3.0));
            col = vec3(1.0, 0.58, 0.16) * p;
            e = 0.55 + 0.35 * tell;
          } else if (underbelly) {
            col = (mod(fy, 2.0) < 1.0) ? belly : belly * 0.72;
          } else {
            bool sc = mod(fx + floor(fy * 0.5) * 2.0, 3.0) < 1.0;
            col = sc ? sc_hi : sc_lo;
          }
        }
        // 6) Cola espinosa (fx -17..-8, fy 12..17)
        else if (fx >= -17.0 && fx < -8.0 &&
                 abs(fy - (14.5 + sin((fx + u_time * 8.0) * 0.45) * 1.6)) <= (18.0 + fx) * 0.28 + 0.8) {
          drawn = true;
          bool spike = mod(fx, 3.0) < 1.0 && fy < 14.5;
          col = spike ? belly : sc_hi * 0.8;
        }
        // 7) Garras / patas dracónicas (fy 17..21)
        else if (fy >= 17.0 && fy < 22.0 && (abs(fx - 4.0) <= 2.5 || abs(fx + 5.0) <= 2.5)) {
          float ly = fy + (fx > 0.0 ? stride : -stride) * 0.5;
          if (ly < 21.8) {
            drawn = true;
            col = (fy >= 20.0) ? vec3(0.92, 0.84, 0.66) : sc_lo;
          }
        }
        if (drawn) {
          albedo = clamp(col, 0.0, 1.0);
          n = normalize(vec3(norm_xy, 1.0));
          depth = 0.29;
          emit = e;
          lr = 1.0;
        }
      }
    }
  }
  // Sprites pixel-art 16×16 (protagonista, huevos de dragón, reliquias) con normales por altura.
  for (int i = 0; i < u_sprite_count; ++i) {
    vec4 sr = u_sprite_rect[i];
    vec2 rel = floor(world - sr.xy + vec2(8.0, 8.0));
    if (rel.x >= 0.0 && rel.y >= 0.0 && rel.x < 16.0 && rel.y < 16.0) {
      int raw_c = int(floor(abs(sr.z) + 0.5));
      int raw_r = int(floor(abs(sr.w) + 0.5));
      bool flip_x = sr.z < -0.5;
      bool flip_y = sr.w < -0.5;
      int col = clamp(flip_x ? raw_c - 1 : raw_c, 0, 7);
      int row = clamp(flip_y ? raw_r - 1 : raw_r, 0, 7);
      int lx = int(rel.x), ly = int(rel.y);
      ivec2 sl = ivec2(flip_x ? 15 - lx : lx, flip_y ? 15 - ly : ly);
      ivec2 cell = ivec2(col, row);
      vec4 sp = sprite_texel(cell, sl);
      if (sp.a > 0.002) {
        vec4 tint = u_sprite_tint[i];
        albedo = clamp(sp.rgb * tint.rgb, 0.0, 1.0);
        float ar = sprite_texel(cell, sl + ivec2(1, 0)).a;
        float al = sprite_texel(cell, sl - ivec2(1, 0)).a;
        float ad = sprite_texel(cell, sl + ivec2(0, 1)).a;
        float au = sprite_texel(cell, sl - ivec2(0, 1)).a;
        ar = ar > 0.002 ? ar : sp.a * 0.4;
        al = al > 0.002 ? al : sp.a * 0.4;
        ad = ad > 0.002 ? ad : sp.a * 0.4;
        au = au > 0.002 ? au : sp.a * 0.4;
        float dx = (ar - al) * (flip_x ? -1.0 : 1.0);
        float dy = (ad - au) * (flip_y ? -1.0 : 1.0);
        n = normalize(vec3(-dx * 1.8, dy * 1.8, 1.0));
        float self_emit = sp.a > 0.94 ? (sp.a - 0.94) * 12.0 : 0.0;
        emit = clamp(self_emit + tint.a, 0.0, 1.0);
        lr = 1.0;
        depth = 0.28;
      }
    }
  }
  // FG: primer plano, sobre los actores (depth 0.2). Mismo atlas que VIS; id = canal B.
  if (inside && cv.b != 0u) {
    albedo = atlas_texel(cv.b, l).rgb;
    float dx = atlas_h(cv.b, l + ivec2(1, 0)) - atlas_h(cv.b, l - ivec2(1, 0));
    float dy = atlas_h(cv.b, l + ivec2(0, 1)) - atlas_h(cv.b, l - ivec2(0, 1));
    n = normalize(vec3(-dx, dy, 1.0));
    autotile_bevel(n, cv.b, l);
    depth = 0.2;
    emit = 0.0; lr = 1.0;
  }
  o_albedo = vec4(albedo, lr);
  o_normal = vec4(n * 0.5 + 0.5, 1.0 - emit);
  gl_FragDepth = depth;
}
