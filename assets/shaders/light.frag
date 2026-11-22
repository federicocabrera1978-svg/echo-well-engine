#version 330 core
// Light pass: ambiente + luz puntual (sigue al player, con leve parpadeo) sobre el G-buffer.
// Con u_fade > 0: pantalla de final pixel-art (o fallback "META" + "R" si u_end == 0).
// HUD (u_items_total > 0): huevo + "n/total" arriba a la izquierda, también sobre la meta.
in vec2 v_uv;
out vec4 o_color;
uniform sampler2D u_albedo;
uniform sampler2D u_normal;
uniform float u_time;
uniform float u_fade;  // 0..1
uniform int u_end;     // 1 = pantalla de final interactiva (usa u_txt)
uniform int u_end_sel; // opción seleccionada 0..3
uniform int u_end_has_next; // 1 = hay siguiente nivel
uniform vec2 u_light;  // px de vista, y hacia abajo
uniform int u_pause;   // 1 = menú de pausa
uniform int u_sel;     // ítem seleccionado 0..4
uniform int u_has_save; // 1 = hay partida guardada (CONTINUAR habilitado)
uniform int u_dlg;      // 1 = caja de diálogo abierta (usa u_txt)
uniform int u_dlg_more; // 1 = hay páginas siguientes en el diálogo
uniform int u_ctl;      // 1 = página CONTROLES del menú
uniform int u_ctl_sel;  // fila seleccionada (1..10 en u_txt)
uniform int u_ctl_wait; // 1 = esperando tecla/botón
uniform int u_txt[96];  // 12 filas × 32 columnas, 4 glifos de 6 bits por int (63 = nada)
uniform float u_vol;   // 0..1 ajuste (aunque esté mute)
uniform int u_mute;
uniform int u_items;        // collectibles agarrados
uniform int u_items_total;  // 0 = sin HUD
uniform float u_items_flash; // 0..1 destello al agarrar
uniform int u_abilities;       // bitmask de habilidades desbloqueadas
uniform float u_ability_flash; // 0..1 destello al desbloquear
uniform vec4 u_map_grid;       // (w, h, cur_cx, cur_cy)
uniform int u_map_vis;         // bitmask de chunks visitados
uniform vec4 u_boss_burst;     // (x_vista, y_vista, t: 1=inicio -> 0=fin, 0) rayos catódicos al vencer boss

// Glifo 5×5, bit 0 = izquierda, fila 0 = arriba. bits = filas empaquetadas de a 5.
float glyph5(int bits, vec2 p) {
  ivec2 c = ivec2(floor(p));
  if (c.x < 0 || c.y < 0 || c.x > 4 || c.y > 4) return 0.0;
  return float((bits >> (c.y * 5 + c.x)) & 1);
}

// Dígitos 0-9 (mismos glifos que el menú) y '/'.
const int DIG[10] = int[](15324974, 14815428, 32584238, 16267791, 8682793,
                          16268351, 15252526, 2167071, 15252014, 15235630);
const int GSLASH = 1118480;

// Fuente completa (menu_text.hpp: A-Z, 0-9, '>', ' ', '-', '?', '/', '<', '.', ':', '!', ',', ''' ).
const int FONT[47] = int[](18415150, 16301615, 31491134, 16303663, 32554047, 1096767, 15266878, 18415153, 32641183, 6594844, 9604265, 32539681, 18405233, 18667121, 15255086, 1097263, 23385646, 9616943, 16267326, 4329631, 15255089, 4539953, 18732593, 18157905, 4329809, 32575775, 15324974, 14815428, 32584238, 16267791, 8682793, 16268351, 15252526, 2167071, 15252014, 15235630, 1154145, 0, 31744, 4207150, 1118480, 8521864, 4194304, 131200, 4198532, 2228224, 132);

int txt_at(int r, int c) {
  int i = r * 32 + c;
  return (u_txt[i / 4] >> ((i - (i / 4) * 4) * 6)) & 63;
}

// Glifo de la grilla de texto en (row, px relativo al origen de la fila) a escala s.
float txt_glyph(int row, vec2 rel, float s) {
  if (rel.x < 0.0 || rel.y < 0.0 || rel.y >= 5.0 * s) return 0.0;
  int col = int(floor(rel.x / (6.0 * s)));
  if (col >= 32) return 0.0;
  int g = txt_at(row, col);
  if (g >= 47) return 0.0;
  return glyph5(FONT[g], (rel - vec2(float(col) * 6.0 * s, 0.0)) / s);
}

float bayer4(vec2 p) {
  ivec2 i = ivec2(mod(p, 4.0));
  int m[16] = int[](0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5);
  return (float(m[i.y * 4 + i.x]) + 0.5) / 16.0;
}

// Pantalla de final pixel-art: viñeta con dither 4×4, marco doble dorado, stats y opciones.
vec3 end_screen_page(vec3 c, vec2 px, float f) {
  vec2 ip = floor(px);
  vec2 mid = (px - vec2(160.0, 90.0)) / vec2(160.0, 90.0);
  float vig = smoothstep(0.35, 1.30, length(mid));
  float dither = step(bayer4(ip), clamp(vig * f * 0.85, 0.0, 1.0));
  c *= 1.0 - 0.58 * f;
  c = mix(c, vec3(0.06, 0.05, 0.04), f * (0.55 + 0.30 * dither));
  c = mix(c, c * vec3(1.06, 0.86, 0.45), vig * f);

  float a = smoothstep(0.20, 0.78, f);
  if (a <= 0.0) return c;
  vec3 pcol = c;
  if (ip.x >= 34.0 && ip.x <= 285.0 && ip.y >= 14.0 && ip.y <= 165.0) {
    bool outer_b = (ip.x == 34.0 || ip.x == 285.0 || ip.y == 14.0 || ip.y == 165.0);
    bool inner_b = (ip.x == 36.0 || ip.x == 283.0 || ip.y == 16.0 || ip.y == 163.0) &&
                   (ip.x >= 36.0 && ip.x <= 283.0 && ip.y >= 16.0 && ip.y <= 163.0);
    pcol = mix(c, vec3(0.08, 0.08, 0.11), 0.94);
    if (inner_b) pcol = vec3(0.30, 0.23, 0.12);
    if (outer_b) pcol = vec3(0.85, 0.72, 0.36);
    float d_tl = abs(ip.x - 39.0) + abs(ip.y - 19.0);
    float d_tr = abs(ip.x - 280.0) + abs(ip.y - 19.0);
    float d_bl = abs(ip.x - 39.0) + abs(ip.y - 160.0);
    float d_br = abs(ip.x - 280.0) + abs(ip.y - 160.0);
    if (min(min(d_tl, d_tr), min(d_bl, d_br)) <= 1.0) pcol = vec3(0.95, 0.85, 0.48);
    if (ip.y == 87.0 && ip.x >= 44.0 && ip.x <= 275.0) pcol = vec3(0.42, 0.34, 0.18);
    if (abs(ip.x - 160.0) + abs(ip.y - 87.0) <= 2.0) pcol = vec3(0.90, 0.76, 0.38);
    float sel_y = 95.0 + float(clamp(u_end_sel, 0, 3)) * 13.0;
    if (ip.x >= 44.0 && ip.x <= 275.0 && ip.y >= sel_y - 2.0 && ip.y <= sel_y + 6.0)
      pcol = mix(pcol, vec3(0.22, 0.19, 0.10), 0.55);
  }
  float title_g = txt_glyph(0, px - vec2(65.0, 22.0), 2.0);
  pcol = mix(pcol, vec3(1.0, 0.90, 0.45) * (0.88 + 0.12 * sin(u_time * 3.5)), title_g);
  pcol = mix(pcol, vec3(0.82, 0.74, 0.55), txt_glyph(1, px - vec2(46.0, 39.0), 1.0));
  for (int r = 0; r < 3; ++r) {
    float g = txt_glyph(2 + r, px - vec2(46.0, 51.0 + float(r) * 10.0), 1.0);
    pcol = mix(pcol, vec3(0.78, 0.80, 0.85), g);
  }
  pcol = mix(pcol, vec3(0.96, 0.88, 0.52), txt_glyph(5, px - vec2(46.0, 79.0), 1.0));
  for (int r = 0; r < 4; ++r) {
    float g = txt_glyph(6 + r, px - vec2(58.0, 95.0 + float(r) * 13.0), 1.0);
    vec3 col = (r == u_end_sel) ? vec3(1.0, 0.95, 0.55) : vec3(0.75, 0.75, 0.80);
    if (r == 0 && u_end_has_next == 0) col *= 0.45;
    pcol = mix(pcol, col, g);
  }
  pcol = mix(pcol, vec3(0.55, 0.55, 0.62), txt_glyph(11, px - vec2(46.0, 152.0), 1.0));
  return mix(c, pcol, a);
}

// Página CONTROLES: caja ancha, título, 10 filas (acciones, POR DEFECTO, VOLVER) y ayuda.
vec3 controls_page(vec3 c, vec2 px) {
  vec2 mid = px - vec2(160.0, 98.0);
  if (abs(mid.x) < 112.0 && abs(mid.y) < 80.0) {
    float border = (abs(mid.x) > 109.0 || abs(mid.y) > 77.0) ? 1.0 : 0.0;
    c = mix(c, vec3(0.10, 0.10, 0.13), 0.92);
    c = mix(c, vec3(0.85, 0.75, 0.40), border);
  }
  c = mix(c, vec3(1.0, 0.92, 0.50), txt_glyph(0, px - vec2(106.0, 24.0), 2.0));
  float ry = px.y - 44.0;
  if (ry >= 0.0 && ry < 110.0) {
    int r = int(floor(ry / 11.0));
    float g = txt_glyph(r + 1, vec2(px.x - 56.0, ry - float(r) * 11.0), 1.0);
    vec3 col = vec3(0.75, 0.75, 0.80);
    if (r + 1 == u_ctl_sel) {
      col = vec3(1.0, 0.95, 0.55);
      if (u_ctl_wait > 0) col = mix(col, vec3(1.0, 0.55, 0.35), 0.5 + 0.5 * sin(u_time * 8.0));
    }
    c = mix(c, col, g);
  }
  c = mix(c, vec3(0.55, 0.55, 0.62), txt_glyph(11, px - vec2(56.0, 162.0), 1.0));
  return c;
}

// Caja inferior pixel-art para diálogos con NPCs, estelas dracónicas y ecos de huevos.
vec3 dialogue_box(vec3 c, vec2 px) {
  vec2 ip = floor(px);
  if (ip.x >= 18.0 && ip.x <= 301.0 && ip.y >= 118.0 && ip.y <= 173.0) {
    bool outer_b = (ip.x == 18.0 || ip.x == 301.0 || ip.y == 118.0 || ip.y == 173.0);
    bool inner_b = (ip.x == 20.0 || ip.x == 299.0 || ip.y == 120.0 || ip.y == 171.0) &&
                   (ip.x >= 20.0 && ip.x <= 299.0 && ip.y >= 120.0 && ip.y <= 171.0);
    c = mix(c, vec3(0.06, 0.06, 0.10), 0.94);
    if (inner_b) c = vec3(0.32, 0.24, 0.14);
    if (outer_b) c = vec3(0.86, 0.68, 0.32);
    float d_tl = abs(ip.x - 22.0) + abs(ip.y - 122.0);
    float d_tr = abs(ip.x - 297.0) + abs(ip.y - 122.0);
    float d_bl = abs(ip.x - 22.0) + abs(ip.y - 169.0);
    float d_br = abs(ip.x - 297.0) + abs(ip.y - 169.0);
    if (min(min(d_tl, d_tr), min(d_bl, d_br)) <= 1.0) c = vec3(0.96, 0.85, 0.45);
    if (ip.y == 131.0 && ip.x >= 24.0 && ip.x <= 295.0) c = vec3(0.32, 0.24, 0.14);
    if (u_dlg_more > 0) {
      float blink = step(0.0, sin(u_time * 6.0));
      vec2 ap = ip - vec2(290.0, 164.0);
      if (ap.y >= 0.0 && ap.y <= 2.0 && abs(ap.x) <= 2.0 - ap.y && blink > 0.5)
        c = vec3(1.0, 0.88, 0.42);
    }
  } else if (ip.x >= 16.0 && ip.x <= 304.0 && ip.y >= 116.0 && ip.y <= 176.0) {
    if (bayer4(ip) > 0.45) c *= 0.55;
  }
  c = mix(c, vec3(1.0, 0.88, 0.42), txt_glyph(0, px - vec2(26.0, 123.0), 1.0));
  c = mix(c, vec3(0.92, 0.90, 0.84), txt_glyph(1, px - vec2(26.0, 136.0), 1.0));
  c = mix(c, vec3(0.92, 0.90, 0.84), txt_glyph(2, px - vec2(26.0, 146.0), 1.0));
  c = mix(c, vec3(0.62, 0.60, 0.68), txt_glyph(3, px - vec2(26.0, 162.0), 1.0));
  return c;
}

// Contador de huevos: ícono (óvalo 5×7) + "n/t" a escala 1, esquina superior izquierda.
vec3 hud_items(vec3 c, vec2 px) {
  int n = clamp(u_items, 0, 9), t = clamp(u_items_total, 0, 9);
  bool all = n >= t;
  vec2 e = (px - vec2(8.5, 8.5)) / vec2(2.6, 3.4);  // huevo: un poco más ancho abajo
  e.x *= 1.0 + 0.18 * clamp(-e.y, 0.0, 1.0);
  float egg = step(dot(e, e), 1.0);
  vec3 egg_on = vec3(1.0, 0.93, 0.70), egg_off = vec3(0.35, 0.33, 0.30);
  // placa oscura detrás para que se lea sobre cualquier fondo
  if (px.x >= 3.0 && px.x <= 33.0 && px.y >= 3.0 && px.y <= 14.0) c = mix(c, vec3(0.03, 0.03, 0.05), 0.55);
  float flash = clamp(u_items_flash, 0.0, 1.0);
  c = mix(c, mix(n > 0 ? egg_on : egg_off, vec3(1.0), flash * 0.6), egg);
  float g = 0.0;
  vec2 o = vec2(15.0, 6.0);
  g = max(g, glyph5(DIG[n], px - o));
  g = max(g, glyph5(GSLASH, px - o - vec2(6.0, 0.0)));
  g = max(g, glyph5(DIG[t], px - o - vec2(12.0, 0.0)));
  vec3 tc = all ? vec3(1.0, 0.85, 0.30) : vec3(0.85, 0.85, 0.88);
  return mix(c, mix(tc, vec3(1.0), flash * 0.5), g);
}

// Íconos discretos de habilidades desbloqueadas (debajo del HUD de huevos, o arriba si no hay huevos).
vec3 hud_abilities(vec3 c, vec2 px) {
  float top = u_items_total > 0 ? 17.0 : 3.0;
  float right = (u_abilities & 2) != 0 ? 29.0 : 17.0;
  if (px.x >= 3.0 && px.x <= right && px.y >= top && px.y <= top + 11.0)
    c = mix(c, vec3(0.03, 0.03, 0.05), 0.55);
  float flash = clamp(u_ability_flash, 0.0, 1.0);
  if ((u_abilities & 1) != 0) {
    ivec2 p = ivec2(floor(px - vec2(6.0, top + 2.0)));
    if (p.x >= 0 && p.x <= 8 && p.y >= 0 && p.y <= 6) {
      int dx = abs(p.x - 4);
      bool icon = (p.y <= 3 && dx == p.y) || (p.y >= 3 && dx == (p.y - 3));
      if (icon) c = mix(vec3(0.45, 0.92, 1.0), vec3(1.0), flash * 0.65);
    }
  }
  if ((u_abilities & 2) != 0) {
    ivec2 p = ivec2(floor(px - vec2(18.0, top + 2.0)));
    if (p.x >= 0 && p.x <= 8 && p.y >= 0 && p.y <= 6) {
      int dy = abs(p.y - 3);
      bool arrow = (dy == 0 && p.x <= 6) || (p.x >= 4 && (p.x - 4) == (3 - dy));
      if (arrow) c = mix(vec3(1.0, 0.55, 0.22), vec3(1.0), flash * 0.65);
    }
  }
  return c;
}

// Mini-mapa exploratorio en esquina superior derecha (y <= 14 px para mapas de hasta 2 filas).
vec3 hud_minimap(vec3 c, vec2 px) {
  int w = clamp(int(u_map_grid.x + 0.5), 1, 8), h = clamp(int(u_map_grid.y + 0.5), 1, 4);
  int cur_x = int(floor(u_map_grid.z + 0.5)), cur_y = int(floor(u_map_grid.w + 0.5));
  if (w * h <= 1) return c;
  float bw = float(w) * 5.0 + 3.0, bh = float(h) * 4.0 + 3.0;
  vec2 org = vec2(317.0 - bw, 3.0);
  if (px.x >= org.x && px.x <= org.x + bw && px.y >= org.y && px.y <= org.y + bh) {
    c = mix(c, vec3(0.03, 0.03, 0.05), 0.62);
    vec2 rel = px - org - vec2(2.0, 2.0);
    if (rel.x >= 0.0 && rel.y >= 0.0) {
      int gx = int(floor(rel.x / 5.0)), gy = int(floor(rel.y / 4.0));
      vec2 cell = rel - vec2(float(gx) * 5.0, float(gy) * 4.0);
      if (gx < w && gy < h && cell.x < 4.0 && cell.y < 3.0) {
        int bit = gy * w + gx;
        bool vis = (bit < 31) && ((uint(u_map_vis) >> uint(bit)) & 1u) != 0u;
        bool cur = (gx == cur_x && gy == cur_y);
        if (cur) {
          float p = 0.85 + 0.15 * sin(u_time * 5.0);
          c = vec3(1.0, 0.88, 0.36) * p;
        } else if (vis) {
          c = vec3(0.32, 0.44, 0.56);
        } else {
          c = vec3(0.11, 0.12, 0.17);
        }
      }
    }
  }
  return c;
}

// Efecto de rayos catódicos (CRT) emanando del boss al ser derrotado: haces radiales con dispersión
// cromática RGB de fósforo, anillos de barrido con scanlines y línea horizontal de deflexión.
vec3 crt_boss_rays(vec3 c, vec2 px, vec3 burst) {
  float bt = clamp(burst.z, 0.0, 1.0);
  if (bt <= 0.001) return c;
  float u = 1.0 - bt;
  float env = sin(u * 3.14159265);
  vec2 d = px - burst.xy;
  float r = length(d);
  float ang = atan(d.y, d.x);
  float scan = 0.55 + 0.45 * step(0.5, fract(px.y * 0.5 + u_time * 15.0));
  float wave_r = 16.0 + u * 260.0;
  float reach = smoothstep(wave_r, max(1.0, wave_r - 36.0), r) * exp(-r * 0.009);
  // 1) Haces catódicos radiales con separación de cañones RGB
  float bR = max(0.0, cos((ang - 0.045) * 12.0 - u * 22.0 + r * 0.09));
  float bG = max(0.0, cos(ang * 12.0 - u * 22.0 + r * 0.09));
  float bB = max(0.0, cos((ang + 0.045) * 12.0 - u * 22.0 + r * 0.09));
  vec3 rays = vec3(pow(bR, 5.0), pow(bG, 5.0), pow(bB, 5.0));
  c += rays * vec3(1.0, 0.92, 1.15) * reach * env * scan * 1.35;
  // 2) Anillos concéntricos de fósforo CRT + haz horizontal de deflexión catódica
  float ring1 = exp(-abs(r - u * 195.0) * 0.32);
  float ring2 = exp(-abs(r - max(0.0, u - 0.16) * 175.0) * 0.36);
  c += (ring1 * vec3(0.45, 0.96, 1.0) + ring2 * vec3(1.0, 0.42, 0.88)) * env * scan * 0.95;
  float hbeam = exp(-abs(d.y) * 0.55) * exp(-abs(d.x) * 0.008) * bt;
  c += vec3(0.65, 0.98, 1.0) * hbeam * scan * 1.1;
  // 3) Destello del núcleo catódico en el centro del boss
  float core = exp(-r * 0.12) * bt;
  c = mix(c, vec3(0.85, 0.98, 1.0), clamp(core * 1.2, 0.0, 1.0));
  return clamp(c, 0.0, 1.0);
}

void main() {
  vec4 A = texture(u_albedo, v_uv);
  vec4 N = texture(u_normal, v_uv);
  vec3 albedo = A.rgb;
  vec3 n = N.xyz * 2.0 - 1.0;
  float lr = A.a;          // 1 = escena, 0 = fondo (backdrop)
  float emit = 1.0 - N.a;  // luz propia del fondo
  vec2 px = vec2(v_uv.x * 320.0, (1.0 - v_uv.y) * 180.0);
  vec2 lp = u_light + vec2(0.0, -10.0 + sin(u_time * 3.0));
  vec3 L = vec3((lp - px) * vec2(1.0, -1.0), 24.0);  // a espacio y-arriba de las normales
  float d = length(L);
  float att = 1.0 / (1.0 + 0.0008 * d * d);
  float diff = max(dot(n, normalize(L)), 0.0);
  // Fondo: ambiente frío más alto (se lee la silueta de la sala) y la luz del player lo
  // roza a medias, así queda detrás del primer plano. Escena: igual que antes.
  vec3 amb = mix(vec3(0.36, 0.38, 0.46), vec3(0.12), lr);
  vec3 c = albedo * (amb + vec3(1.0, 0.85, 0.6) * diff * att * 2.6 * mix(0.45, 1.0, lr));
  c += albedo * emit * 1.6;
  if (u_boss_burst.z > 0.001) c = crt_boss_rays(c, px, u_boss_burst.xyz);

  if (u_fade > 0.0) {
    float f = clamp(u_fade, 0.0, 1.0);
    if (u_end > 0 && u_pause == 0) {
      c = end_screen_page(c, px, f);
    } else if (u_pause == 0) {
      // Fallback: oscurece la escena y dibuja "META" + "R" cuando no hay EndScreen.
      vec2 mid = (px - vec2(160.0, 90.0)) / vec2(160.0, 90.0);
      float vig = smoothstep(0.4, 1.3, length(mid));
      c = mix(c, vec3(0.08, 0.07, 0.05), f * 0.7);
      c *= 1.0 - 0.55 * f;
      c = mix(c, c * vec3(1.05, 0.85, 0.45), vig * f);

      float a = smoothstep(0.2, 0.7, f) * (0.8 + 0.2 * sin(u_time * 4.0));
      const int GM = 22730741, GE = 32554047, GT = 4329631, GA = 18415150, GR = 9616943;
      float s = 4.0;
      vec2 o = vec2(160.0 - 12.0 * s, 52.0);
      float g = 0.0;
      int letters[4] = int[](GM, GE, GT, GA);
      for (int i = 0; i < 4; ++i) {
        vec2 p = (px - o - vec2(float(i) * 6.0 * s, 0.0)) / s;
        g = max(g, glyph5(letters[i], p));
      }
      float sr = 3.0;
      vec2 orr = vec2(160.0 - 2.5 * sr, 52.0 + 6.0 * s + 6.0);
      g = max(g, glyph5(GR, (px - orr) / sr));
      c = mix(c, vec3(1.0, 0.9, 0.45), g * a);
    }
  }

  if (u_items_total > 0) c = hud_items(c, px);
  if (u_abilities != 0) c = hud_abilities(c, px);
  if (u_map_grid.x * u_map_grid.y > 1 && u_pause == 0 && u_end == 0) c = hud_minimap(c, px);
  if (u_dlg > 0 && u_pause == 0 && u_end == 0) c = dialogue_box(c, px);

  if (u_pause > 0 && u_ctl > 0) {
    c = mix(c, vec3(0.05, 0.05, 0.07), 0.72);
    c = controls_page(c, px);
  } else if (u_pause > 0) {
    // Oscurece y dibuja PAUSA + 6 ítems (SEGUIR, CONTINUAR, REINICIO, VOL/MUTE, CONTROLES, SALIR).
    c = mix(c, vec3(0.05, 0.05, 0.07), 0.72);
    // A=0 … Z=25, 0-9=26-35, '>'=36. Solo las letras usadas están llenas.
    const int G[37] = int[](
      18415150, 16301615, 31491134, 16303663, 32554047, 0, 15266878, 0,          // A-H
      32641183, 0, 0, 32539681, 18405233, 18667121, 15255086, 1097263,            // I-P
      0, 9616943, 16267326, 4329631, 15255089, 4539953, 0, 0, 0, 0,              // Q-Z
      15324974, 14815428, 32584238, 16267791, 8682793,                            // 0-4
      16268351, 15252526, 2167071, 15252014, 15235630,                            // 5-9
      1154145                                                                    // >
    );
    // caja
    vec2 mid = px - vec2(160.0, 92.0);
    if (abs(mid.x) < 86.0 && abs(mid.y) < 68.0) {
      float border = (abs(mid.x) > 83.0 || abs(mid.y) > 65.0) ? 1.0 : 0.0;
      c = mix(c, vec3(0.10, 0.10, 0.13), 0.92);
      c = mix(c, vec3(0.85, 0.75, 0.40), border);
    }
    float tg = 0.0;
    // PAUSA (escala 3)
    float ts = 3.0;
    vec2 to = vec2(160.0 - 15.0 * ts, 36.0);
    int title[5] = int[](15, 0, 20, 18, 0);  // P A U S A
    for (int i = 0; i < 5; ++i)
      tg = max(tg, glyph5(G[title[i]], (px - to - vec2(float(i) * 6.0 * ts, 0.0)) / ts));
    c = mix(c, vec3(1.0, 0.92, 0.50), tg);

    const int NR = 6, W = 9;
    int rows[NR * W];
    int rlen[NR];
    for (int i = 0; i < NR * W; ++i) rows[i] = 0;
    // SEGUIR
    rows[0]=18; rows[1]=4; rows[2]=6; rows[3]=20; rows[4]=8; rows[5]=17; rlen[0]=6;
    // CONTINUAR
    rows[9]=2; rows[10]=14; rows[11]=13; rows[12]=19; rows[13]=8; rows[14]=13; rows[15]=20; rows[16]=0; rows[17]=17; rlen[1]=9;
    // REINICIO
    rows[18]=17; rows[19]=4; rows[20]=8; rows[21]=13; rows[22]=8; rows[23]=2; rows[24]=8; rows[25]=14; rlen[2]=8;
    // VOL xx  o  MUTE
    if (u_mute > 0) {
      rows[27]=12; rows[28]=20; rows[29]=19; rows[30]=4; rlen[3]=4;
    } else {
      int pct = int(clamp(u_vol, 0.0, 1.0) * 100.0 + 0.5);
      rows[27]=21; rows[28]=14; rows[29]=11;  // VOL
      if (pct >= 100) {
        rows[30]=27; rows[31]=26; rows[32]=26; rlen[3]=6;  // VOL100
      } else {
        rows[30]=26 + pct / 10; rows[31]=26 + pct % 10; rlen[3]=5;
      }
    }
    // CONTROLES
    rows[36]=2; rows[37]=14; rows[38]=13; rows[39]=19; rows[40]=17; rows[41]=14; rows[42]=11; rows[43]=4; rows[44]=18; rlen[4]=9;
    // SALIR
    rows[45]=18; rows[46]=0; rows[47]=11; rows[48]=8; rows[49]=17; rlen[5]=5;

    float is_ = 2.0;
    for (int r = 0; r < NR; ++r) {
      vec2 o = vec2(112.0, 60.0 + float(r) * 15.0);
      float g = 0.0;
      if (r == u_sel)
        g = max(g, glyph5(G[36], (px - vec2(o.x - 14.0, o.y)) / is_));
      for (int i = 0; i < rlen[r]; ++i)
        g = max(g, glyph5(G[rows[r * W + i]], (px - o - vec2(float(i) * 6.0 * is_, 0.0)) / is_));
      vec3 col = (r == u_sel) ? vec3(1.0, 0.95, 0.55) : vec3(0.75, 0.75, 0.80);
      if (r == 1 && u_has_save == 0) col *= 0.45;  // sin partida guardada: apagado
      c = mix(c, col, g);
    }
    // Versión (fila 11 de u_txt) abajo a la derecha de la caja.
    c = mix(c, vec3(0.50, 0.50, 0.56), txt_glyph(11, px - vec2(204.0, 150.0), 1.0));
    // Barrita de volumen bajo el ítem VOL.
    {
      float bx = 112.0, by = 60.0 + 3.0 * 15.0 + 11.0, bw = 80.0, bh = 2.0;
      if (px.x >= bx && px.x <= bx + bw && px.y >= by && px.y <= by + bh) {
        float f = (px.x - bx) / bw;
        if (u_mute > 0) c = vec3(0.25, 0.22, 0.22);
        else c = (f <= u_vol) ? vec3(0.85, 0.75, 0.35) : vec3(0.22, 0.22, 0.25);
      }
    }
  }
  o_color = vec4(c, 1.0);
}
