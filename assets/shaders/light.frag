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
uniform int u_end_true;     // 1 = final verdadero (100% huevos)
uniform vec2 u_light;  // px de vista, y hacia abajo
uniform float u_amb_dark;
uniform float u_lantern_rad;
uniform int u_pl_count;
uniform vec4 u_pl_pos[12];
uniform vec4 u_pl_col[12];
uniform int u_pause;   // 1 = menú de pausa
uniform int u_sel;     // ítem seleccionado 0..7
uniform int u_has_save; // 1 = hay partida guardada (CONTINUAR habilitado)
uniform int u_foliage;  // 1 = brisa / vegetación animada habilitada
uniform int u_hero_sel; // 1 = página ELIGE TU HEROE
uniform int u_hero_cur; // héroe seleccionado 0..2
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
uniform int u_hp;              // vida actual
uniform int u_max_hp;          // vida máxima (1..3)
uniform int u_dash_up;         // 1 = Dash II desbloqueado
uniform vec4 u_hud_fx;         // (damage_flash, dash_charges, dash_charges_max, milestone_flash)
uniform vec4 u_map_grid;       // (w, h, cur_cx, cur_cy)
uniform int u_map_vis;         // bitmask de chunks visitados (bits 0..31)
uniform int u_map_vis_hi;      // bitmask de chunks visitados (bits 32..63)
uniform int u_map_doors;       // bitmask de puertas/muros cerrados (bits 0..31)
uniform int u_map_doors_hi;    // bitmask de puertas/muros cerrados (bits 32..63)
uniform int u_map_eggs;        // bitmask de huevos pendientes (bits 0..31)
uniform int u_map_eggs_hi;     // bitmask de huevos pendientes (bits 32..63)
uniform int u_map_pct;         // porcentaje explorado 0..100
uniform int u_seals_mask;      // bitmask de Sellos Elementales rotos (0=Piedra, 1=Sombra, 2=Agua)
uniform int u_seals_total;     // total de sellos en el mapa (3 en world, 0 en otros)
uniform int u_map_open;        // 1 = overlay de Mapa Expandido abierto (M / Tab)
uniform vec2 u_map_target;     // (cx, cy) destino seleccionado para Viaje Rápido (-1 = ninguno)
uniform vec4 u_boss_hud;       // (hp, max_hp, kind, hurt_flash) barra de vida del boss activo
uniform vec4 u_boss_burst;     // (x_vista, y_vista, t: 1=inicio -> 0=fin, flash: 0..1) rayos catódicos al vencer boss

// Glifo 5×5, bit 0 = izquierda, fila 0 = arriba. bits = filas empaquetadas de a 5.
float glyph5(int bits, vec2 p) {
  ivec2 c = ivec2(floor(p));
  if (c.x < 0 || c.y < 0 || c.x > 4 || c.y > 4) return 0.0;
  return float((bits >> (c.y * 5 + c.x)) & 1);
}

bool mask_bit(int lo, int hi, int bit) {
  if (bit < 0 || bit >= 64) return false;
  uint word = (bit < 32) ? uint(lo) : uint(hi);
  int shift = (bit < 32) ? bit : (bit - 32);
  return ((word >> uint(shift)) & 1u) != 0u;
}

// Dígitos 0-9 (mismos glifos que el menú), '/' y '%'.
const int DIG[10] = int[](15324974, 14815428, 32584238, 16267791, 8682793,
                          16268351, 15252526, 2167071, 15252014, 15235630);
const int GSLASH = 1118480;
const int GPERCENT = 27070835;

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
    vec3 true_tint = mix(vec3(1.0, 0.88, 0.36), vec3(0.48, 0.96, 1.0), 0.35 + 0.35 * sin(u_time * 3.0 + ip.x * 0.08));
    if (inner_b) pcol = vec3(0.30, 0.23, 0.12);
    if (outer_b) pcol = (u_end_true > 0) ? true_tint : vec3(0.85, 0.72, 0.36);
    float d_tl = abs(ip.x - 39.0) + abs(ip.y - 19.0);
    float d_tr = abs(ip.x - 280.0) + abs(ip.y - 19.0);
    float d_bl = abs(ip.x - 39.0) + abs(ip.y - 160.0);
    float d_br = abs(ip.x - 280.0) + abs(ip.y - 160.0);
    if (min(min(d_tl, d_tr), min(d_bl, d_br)) <= 1.0) pcol = (u_end_true > 0) ? true_tint : vec3(0.95, 0.85, 0.48);
    if (ip.y == 87.0 && ip.x >= 44.0 && ip.x <= 275.0) pcol = vec3(0.42, 0.34, 0.18);
    if (abs(ip.x - 160.0) + abs(ip.y - 87.0) <= 2.0) pcol = vec3(0.90, 0.76, 0.38);
    float sel_y = 95.0 + float(clamp(u_end_sel, 0, 3)) * 13.0;
    if (ip.x >= 44.0 && ip.x <= 275.0 && ip.y >= sel_y - 2.0 && ip.y <= sel_y + 6.0)
      pcol = mix(pcol, vec3(0.22, 0.19, 0.10), 0.55);
  }
  int len0 = 0;
  for (int k = 0; k < 32; ++k) {
    int g0 = txt_at(0, k);
    if (g0 < 47 && g0 != 37) len0 = k + 1;
  }
  float ts = (len0 <= 18) ? 2.0 : 1.0;
  float tx0 = (len0 <= 18) ? (160.0 - float(max(len0, 1)) * 6.0 + 1.0)
                           : (160.0 - float(max(len0, 1)) * 3.0 + 0.5);
  float ty0 = (len0 <= 18) ? 22.0 : 25.0;
  float title_g = txt_glyph(0, px - vec2(tx0, ty0), ts);
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

// Página principal de PAUSA: caja oscura con borde dorado, título (fila 0), opciones (filas 1..8),
// barra de volumen en fila 4 junto a VOL, ítem apagado en fila 2 si u_has_save == 0, versión en fila 11.
vec3 pause_page(vec3 c, vec2 px) {
  vec2 ip = floor(px);
  if (ip.x >= 44.0 && ip.x <= 275.0 && ip.y >= 14.0 && ip.y <= 165.0) {
    bool outer_b = (ip.x == 44.0 || ip.x == 275.0 || ip.y == 14.0 || ip.y == 165.0);
    bool inner_b = (ip.x == 46.0 || ip.x == 273.0 || ip.y == 16.0 || ip.y == 163.0) &&
                   (ip.x >= 46.0 && ip.x <= 273.0 && ip.y >= 16.0 && ip.y <= 163.0);
    c = mix(c, vec3(0.08, 0.08, 0.12), 0.93);
    if (inner_b) c = vec3(0.32, 0.25, 0.13);
    if (outer_b) c = vec3(0.85, 0.75, 0.40);
    float d_tl = abs(ip.x - 49.0) + abs(ip.y - 19.0);
    float d_tr = abs(ip.x - 270.0) + abs(ip.y - 19.0);
    float d_bl = abs(ip.x - 49.0) + abs(ip.y - 160.0);
    float d_br = abs(ip.x - 270.0) + abs(ip.y - 160.0);
    if (min(min(d_tl, d_tr), min(d_bl, d_br)) <= 1.0) c = vec3(0.96, 0.86, 0.48);
    if (ip.y == 40.0 && ip.x >= 52.0 && ip.x <= 267.0) c = vec3(0.38, 0.31, 0.16);
  }

  // Fila 0: título ("PAUSA") centrado a escala 3.0.
  int len0 = 0;
  for (int k = 0; k < 16; ++k) {
    int g0 = txt_at(0, k);
    if (g0 < 47 && g0 != 37) len0 = k + 1;
  }
  float title_x = max(52.0, 160.0 - float(max(len0, 1)) * 9.0 + 1.5);
  c = mix(c, vec3(1.0, 0.92, 0.50), txt_glyph(0, px - vec2(title_x, 22.0), 3.0));

  // Filas 1..9: opciones de juego (escala 2.0, paso 11 px).
  for (int r = 1; r <= 9; ++r) {
    float ry = 43.0 + float(r - 1) * 11.0;
    int c0 = txt_at(r, 0);
    if (c0 >= 47) continue;
    bool is_sel = (r - 1 == u_sel);
    if (is_sel && ip.x >= 50.0 && ip.x <= 269.0 && ip.y >= ry - 1.0 && ip.y <= ry + 10.0)
      c = mix(c, vec3(0.22, 0.19, 0.10), 0.48);
    bool has_lead = (c0 == 36 || c0 == 37);
    float tx0 = has_lead ? 54.0 : 74.0;
    float g = txt_glyph(r, px - vec2(tx0, ry), 2.0);
    if (is_sel && c0 != 36)
      g = max(g, glyph5(FONT[36], (px - vec2(has_lead ? 54.0 : 58.0, ry)) / 2.0));
    vec3 col = is_sel ? vec3(1.0, 0.95, 0.55) : vec3(0.76, 0.78, 0.84);
    if (r == 2 && u_has_save == 0) col *= 0.45;
    c = mix(c, col, g);
  }

  // Barra de volumen en fila 4 junto a VOL.
  {
    int end_c = 0;
    for (int k = 0; k < 18; ++k) {
      int g4 = txt_at(4, k);
      if (g4 < 47 && g4 != 37) end_c = k + 1;
    }
    int c4 = txt_at(4, 0);
    float tx4 = (c4 == 36 || c4 == 37) ? 54.0 : 74.0;
    float bx = max(172.0, tx4 + float(end_c) * 12.0 + 8.0);
    float bw = max(32.0, 265.0 - bx);
    float by = 79.0, bh = 4.0;
    if (px.x >= bx - 1.0 && px.x <= bx + bw + 1.0 && px.y >= by - 1.0 && px.y <= by + bh + 1.0) {
      if (px.x >= bx && px.x <= bx + bw && px.y >= by && px.y <= by + bh) {
        float f = (px.x - bx) / bw;
        if (u_mute > 0) c = vec3(0.25, 0.22, 0.22);
        else c = (f <= u_vol) ? vec3(0.85, 0.75, 0.35) : vec3(0.20, 0.20, 0.24);
      } else {
        c = vec3(0.35, 0.30, 0.18);
      }
    }
  }

  // Versión en fila 11 abajo a la derecha.
  c = mix(c, vec3(0.50, 0.50, 0.56), txt_glyph(11, px - vec2(212.0, 152.0), 1.0));
  return c;
}

// Página ELIGE TU HEROE: marco elegante + 3 recuadros de retrato traslúcidos al costado izquierdo
// donde se ven nítidos los 3 sprites animados (preview_hero_sel en x=70, y=62+h*26), resaltando recuadro y fila (u_hero_cur).
vec3 hero_select_page(vec3 c, vec2 px) {
  vec2 ip = floor(px);
  bool in_frame = (ip.x >= 34.0 && ip.x <= 285.0 && ip.y >= 12.0 && ip.y <= 165.0);

  // Detectar si estamos dentro de alguno de los 3 recuadros de retrato a la izquierda (centrados en x=70, y=62+h*26).
  int port_idx = -1;
  bool port_border = false;
  if (ip.x >= 56.0 && ip.x <= 84.0) {
    for (int i = 0; i < 3; ++i) {
      float by0 = 50.0 + float(i) * 26.0;
      float by1 = by0 + 24.0;
      if (ip.y >= by0 && ip.y <= by1) {
        port_idx = i;
        port_border = (ip.x == 56.0 || ip.x == 84.0 || ip.y == by0 || ip.y == by1);
        break;
      }
    }
  }

  if (!in_frame) {
    c = mix(c, vec3(0.04, 0.04, 0.06), 0.72);
  } else if (port_idx >= 0) {
    bool sel_p = (port_idx == u_hero_cur);
    if (port_border) {
      float pulse = 0.88 + 0.12 * sin(u_time * 5.0);
      c = sel_p ? vec3(1.0, 0.88, 0.38) * pulse : vec3(0.32, 0.34, 0.44);
    } else {
      // Ventana limpia/traslúcida: conserva nítido el sprite del mundo renderizado en c.
      c = sel_p ? clamp(c * 1.05 + vec3(0.02, 0.02, 0.01), 0.0, 1.0)
                : mix(c, vec3(0.06, 0.07, 0.10), 0.26);
    }
  } else {
    bool outer_b = (ip.x == 34.0 || ip.x == 285.0 || ip.y == 12.0 || ip.y == 165.0);
    bool inner_b = (ip.x == 36.0 || ip.x == 283.0 || ip.y == 14.0 || ip.y == 163.0) &&
                   (ip.x >= 36.0 && ip.x <= 283.0 && ip.y >= 14.0 && ip.y <= 163.0);
    c = mix(c, vec3(0.08, 0.08, 0.12), 0.93);
    if (inner_b) c = vec3(0.30, 0.24, 0.13);
    if (outer_b) c = vec3(0.86, 0.74, 0.38);
    float d_tl = abs(ip.x - 39.0) + abs(ip.y - 17.0);
    float d_tr = abs(ip.x - 280.0) + abs(ip.y - 17.0);
    float d_bl = abs(ip.x - 39.0) + abs(ip.y - 160.0);
    float d_br = abs(ip.x - 280.0) + abs(ip.y - 160.0);
    if (min(min(d_tl, d_tr), min(d_bl, d_br)) <= 1.0) c = vec3(0.96, 0.86, 0.48);
    if (ip.y == 35.0 && ip.x >= 42.0 && ip.x <= 277.0) c = vec3(0.38, 0.31, 0.16);
    if (ip.x == 90.0 && ip.y >= 46.0 && ip.y <= 130.0) c = vec3(0.28, 0.24, 0.15);
    if (ip.y == 138.0 && ip.x >= 42.0 && ip.x <= 277.0) c = vec3(0.28, 0.24, 0.15);
  }

  // Fila 0: título ("ELIGE TU HEROE") a escala 2.0.
  int len0 = 0;
  for (int k = 0; k < 24; ++k) {
    int g0 = txt_at(0, k);
    if (g0 < 47 && g0 != 37) len0 = k + 1;
  }
  float title_x = max(42.0, 160.0 - float(max(len0, 1)) * 6.0 + 1.0);
  c = mix(c, vec3(1.0, 0.90, 0.46), txt_glyph(0, px - vec2(title_x, 19.0), 2.0));

  // Filas 1..6: 3 héroes (nombre a escala 2.0 + descripción a escala 1.0) alineados con los retratos.
  for (int h = 0; h < 3; ++h) {
    int r_name = 1 + h * 2;
    int r_desc = 2 + h * 2;
    float hy0 = 50.0 + float(h) * 26.0;
    bool is_sel = (h == u_hero_cur);
    if (is_sel && ip.x >= 93.0 && ip.x <= 278.0 && ip.y >= hy0 && ip.y <= hy0 + 23.0)
      c = mix(c, vec3(0.22, 0.18, 0.09), 0.52);

    float gn = txt_glyph(r_name, px - vec2(96.0, hy0 + 2.0), 2.0);
    vec3 ncol = is_sel ? vec3(1.0, 0.95, 0.55) : vec3(0.76, 0.78, 0.85);
    c = mix(c, ncol, gn);

    float gd = txt_glyph(r_desc, px - vec2(96.0, hy0 + 14.0), 1.0);
    vec3 dcol = is_sel ? vec3(0.92, 0.86, 0.64) : vec3(0.58, 0.60, 0.68);
    c = mix(c, dcol, gd);
  }

  // Fila 11: ayuda al pie centrada.
  int len11 = 0;
  for (int k = 0; k < 32; ++k) {
    int g11 = txt_at(11, k);
    if (g11 < 47 && g11 != 37) len11 = k + 1;
  }
  float foot_x = max(42.0, 160.0 - float(max(len11, 1)) * 3.0 + 0.5);
  c = mix(c, vec3(0.58, 0.60, 0.68), txt_glyph(11, px - vec2(foot_x, 148.0), 1.0));
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

// Corazones de vida junto al HUD de huevos (1..3 corazones 7×6).
vec3 hud_hearts(vec3 c, vec2 px) {
  int mhp = clamp(u_max_hp, 1, 3);
  float x0 = (u_items_total > 0) ? 36.0 : 3.0;
  if (px.x >= x0 && px.x <= x0 + float(mhp) * 9.0 + 4.0 && px.y >= 3.0 && px.y <= 14.0)
    c = mix(c, vec3(0.03, 0.03, 0.05), 0.55);
  for (int i = 0; i < 3; ++i) {
    if (i >= mhp) break;
    ivec2 p = ivec2(floor(px - vec2(x0 + 3.0 + float(i) * 9.0, 6.0)));
    if (p.x >= 0 && p.x <= 6 && p.y >= 0 && p.y <= 5) {
      bool heart = (p.y == 0 && (p.x == 1 || p.x == 2 || p.x == 4 || p.x == 5))
                || (p.y == 1 || p.y == 2)
                || (p.y == 3 && p.x >= 1 && p.x <= 5)
                || (p.y == 4 && p.x >= 2 && p.x <= 4)
                || (p.y == 5 && p.x == 3);
      if (heart) {
        if (i < u_hp) {
          vec3 hc = (p.x == 1 && p.y == 1) ? vec3(1.0, 0.78, 0.84) : vec3(0.98, 0.28, 0.42);
          c = mix(hc, vec3(1.0, 0.94, 0.55), clamp(u_hud_fx.w, 0.0, 1.0) * 0.75);
        } else {
          c = mix(vec3(0.24, 0.14, 0.18), vec3(0.88, 0.22, 0.30), clamp(u_hud_fx.x, 0.0, 1.0) * 0.75);
        }
      }
    }
  }
  return c;
}

// Íconos discretos de habilidades desbloqueadas (debajo del HUD de huevos, o arriba si no hay huevos).
vec3 hud_abilities(vec3 c, vec2 px) {
  float top = u_items_total > 0 ? 17.0 : 3.0;
  int slots = ((u_abilities & 1) != 0 ? 1 : 0)
            + ((u_abilities & 2) != 0 ? 1 : 0)
            + ((u_abilities & 4) != 0 ? 1 : 0)
            + ((u_abilities & 8) != 0 ? 1 : 0);
  float right = max(5.0 + float(slots) * 12.0, 17.0);
  if (px.x >= 3.0 && px.x <= right && px.y >= top && px.y <= top + 11.0)
    c = mix(c, vec3(0.03, 0.03, 0.05), 0.55);
  float flash = clamp(u_ability_flash, 0.0, 1.0);
  float ax = 6.0;
  if ((u_abilities & 1) != 0) {
    ivec2 p = ivec2(floor(px - vec2(ax, top + 2.0)));
    if (p.x >= 0 && p.x <= 8 && p.y >= 0 && p.y <= 6) {
      int dx = abs(p.x - 4);
      bool icon = (p.y <= 3 && dx == p.y) || (p.y >= 3 && dx == (p.y - 3));
      if (icon) c = mix(vec3(0.45, 0.92, 1.0), vec3(1.0), flash * 0.65);
    }
    ax += 12.0;
  }
  if ((u_abilities & 2) != 0) {
    ivec2 p = ivec2(floor(px - vec2(ax, top + 2.0)));
    if (p.x >= 0 && p.x <= 8 && p.y >= 0 && p.y <= 6) {
      int dy = abs(p.y - 3);
      bool arrow = (dy == 0 && p.x <= 6) || (p.x >= 4 && (p.x - 4) == (3 - dy));
      if (arrow) {
        vec3 dc = (u_dash_up > 0)
                    ? mix(vec3(1.0, 0.85, 0.25), vec3(0.45, 0.98, 1.0), float(p.x >= 5))
                    : vec3(1.0, 0.55, 0.22);
        c = mix(dc, vec3(1.0), max(flash * 0.65, clamp(u_hud_fx.w, 0.0, 1.0) * 0.65));
      }
    }
    int dmax = clamp(int(u_hud_fx.z + 0.5), 0, 2);
    int dcur = clamp(int(u_hud_fx.y + 0.5), 0, dmax);
    if (dmax > 0) {
      ivec2 dp = ivec2(floor(px - vec2(ax, top + 9.0)));
      if (dp.y >= 0 && dp.y <= 1) {
        if (dmax == 1 && dp.x >= 2 && dp.x <= 5) {
          c = (dcur >= 1) ? vec3(1.0, 0.72, 0.28) : vec3(0.24, 0.20, 0.26);
        } else if (dmax == 2) {
          if (dp.x >= 1 && dp.x <= 3) c = (dcur >= 1) ? vec3(1.0, 0.88, 0.34) : vec3(0.24, 0.20, 0.26);
          if (dp.x >= 5 && dp.x <= 7) c = (dcur >= 2) ? vec3(0.45, 0.98, 1.0) : vec3(0.24, 0.20, 0.26);
        }
      }
    }
    ax += 12.0;
  }
  if ((u_abilities & 4) != 0) {
    ivec2 p = ivec2(floor(px - vec2(ax, top + 2.0)));
    if (p.x >= 0 && p.x <= 8 && p.y >= 0 && p.y <= 6) {
      bool claw = (p.x == 1) ||
                  (p.y == 3 && p.x >= 3 && p.x <= 6) ||
                  ((p.y == 1 || p.y == 5) && p.x >= 3 && p.x <= 4) ||
                  ((p.y == 2 || p.y == 4) && p.x == 5);
      if (claw) c = mix(vec3(0.32, 0.95, 0.58), vec3(1.0), flash * 0.65);
    }
    ax += 12.0;
  }
  if ((u_abilities & 8) != 0) {
    ivec2 p = ivec2(floor(px - vec2(ax, top + 2.0)));
    if (p.x >= 0 && p.x <= 8 && p.y >= 0 && p.y <= 6) {
      bool body = (p.y >= 2 && p.y <= 5 && p.x >= 3 && p.x <= 5);
      bool lid  = (p.y == 1 && p.x >= 3 && p.x <= 5);
      bool halo = (p.y == 6 && p.x >= 2 && p.x <= 6);
      if (body || lid || halo) c = mix(vec3(1.0, 0.75, 0.22), vec3(1.0), flash * 0.65);
    }
    ax += 12.0;
  }
  return c;
}

// Barra de vida de Boss en combate (franja superior central y=3..14, fuera de top_half y=16..89).
vec3 hud_boss_bar(vec3 c, vec2 px) {
  int mhp = clamp(int(u_boss_hud.y + 0.5), 0, 4);
  if (mhp <= 0) return c;
  int bhp = clamp(int(u_boss_hud.x + 0.5), 0, mhp);
  int kind = clamp(int(u_boss_hud.z + 0.5), 0, 3);
  float flash = clamp(u_boss_hud.w, 0.0, 1.0);
  float pip_w = 9.0, gap = 2.0;
  float total_w = 13.0 + float(mhp) * pip_w + float(mhp - 1) * gap;
  float x0 = floor(160.0 - total_w * 0.5);
  if (px.x >= x0 - 3.0 && px.x <= x0 + total_w + 3.0 && px.y >= 3.0 && px.y <= 14.0) {
    c = mix(c, vec3(0.03, 0.03, 0.05), 0.64);
    vec3 kcol = (kind == 2) ? vec3(1.0, 0.44, 0.22)
              : (kind == 3) ? vec3(0.28, 0.94, 0.80)
              : (kind == 1) ? vec3(0.76, 0.42, 0.98)
                            : vec3(0.96, 0.72, 0.26);
    ivec2 ic = ivec2(floor(px - vec2(x0, 5.0)));
    if (ic.x >= 0 && ic.x <= 8 && ic.y >= 0 && ic.y <= 6) {
      bool crown = (ic.y == 6 && ic.x >= 1 && ic.x <= 7) ||
                   (ic.y >= 3 && ic.y <= 5 && ic.x >= 1 && ic.x <= 7 && !(ic.y == 4 && (ic.x == 3 || ic.x == 5))) ||
                   (ic.y == 2 && (ic.x == 1 || ic.x == 4 || ic.x == 7)) ||
                   (ic.y == 1 && (ic.x == 1 || ic.x == 4 || ic.x == 7));
      if (crown) c = mix(kcol, vec3(1.0), flash * 0.7);
    }
    float sx = x0 + 12.0;
    for (int i = 0; i < 4; ++i) {
      if (i >= mhp) break;
      float px0 = sx + float(i) * (pip_w + gap);
      ivec2 sp = ivec2(floor(px - vec2(px0, 6.0)));
      if (sp.x >= 0 && sp.x < int(pip_w) && sp.y >= 0 && sp.y <= 4) {
        bool border = (sp.x == 0 || sp.x == int(pip_w) - 1 || sp.y == 0 || sp.y == 4);
        if (i < bhp) {
          vec3 fill = (sp.y == 1) ? mix(kcol, vec3(1.0), 0.42) : kcol;
          if (border) fill *= 0.75;
          c = mix(fill, vec3(1.0), flash * 0.75);
        } else {
          c = border ? vec3(0.28, 0.22, 0.26) : vec3(0.12, 0.10, 0.14);
        }
      }
    }
  }
  return c;
}

// Emblemas de los Tres Sellos Elementales (Piedra, Sombra, Agua) en franja superior y=3..14.
vec3 hud_seals(vec3 c, vec2 px) {
  int total = clamp(u_seals_total, 0, 3);
  if (total <= 0) return c;
  int mhp = clamp(u_max_hp, 1, 3);
  float hearts_x0 = (u_items_total > 0) ? 36.0 : 3.0;
  float x0 = hearts_x0 + float(mhp) * 9.0 + 7.0;
  float w = float(total) * 8.0 + 4.0;
  if (px.x >= x0 && px.x <= x0 + w && px.y >= 3.0 && px.y <= 14.0) {
    c = mix(c, vec3(0.03, 0.03, 0.05), 0.55);
    for (int i = 0; i < 3; ++i) {
      if (i >= total) break;
      ivec2 p = ivec2(floor(px - vec2(x0 + 3.0 + float(i) * 8.0, 6.0)));
      if (p.x >= 0 && p.x <= 4 && p.y >= 0 && p.y <= 4) {
        int manh = abs(p.x - 2) + abs(p.y - 2);
        if (manh <= 2) {
          bool on = ((u_seals_mask >> i) & 1) != 0;
          vec3 col = (i == 0) ? vec3(0.98, 0.76, 0.28)
                   : (i == 1) ? vec3(0.76, 0.44, 0.98)
                              : vec3(0.28, 0.94, 0.84);
          if (on) {
            c = (manh == 0) ? mix(col, vec3(1.0), 0.65) : col;
          } else {
            c = (manh == 2) ? vec3(0.28, 0.26, 0.34) : vec3(0.12, 0.12, 0.16);
          }
        }
      }
    }
  }
  return c;
}

// Overlay de Mapa Expandido (M / Tab): grilla ampliada 8x5 + zona actual + huevos + Tres Sellos.
vec3 world_map_page(vec3 c, vec2 px) {
  c = mix(c, vec3(0.03, 0.04, 0.06), 0.84);
  vec2 ip = floor(px);
  if (ip.x >= 20.0 && ip.x <= 299.0 && ip.y >= 8.0 && ip.y <= 171.0) {
    bool outer = (ip.x == 20.0 || ip.x == 299.0 || ip.y == 8.0 || ip.y == 171.0);
    bool inner = (ip.x == 22.0 || ip.x == 297.0 || ip.y == 10.0 || ip.y == 169.0);
    c = vec3(0.06, 0.07, 0.11);
    if (inner) c = vec3(0.24, 0.30, 0.42);
    if (outer) c = vec3(0.84, 0.68, 0.32);
  }
  c = mix(c, vec3(1.0, 0.88, 0.42), txt_glyph(0, px - vec2(28.0, 15.0), 1.0));
  c = mix(c, vec3(0.68, 0.88, 0.98), txt_glyph(1, px - vec2(28.0, 25.0), 1.0));

  int w = clamp(int(u_map_grid.x + 0.5), 1, 8), h = clamp(int(u_map_grid.y + 0.5), 1, 6);
  int cur_x = int(floor(u_map_grid.z + 0.5)), cur_y = int(floor(u_map_grid.w + 0.5));
  int tgt_x = int(floor(u_map_target.x + 0.5)), tgt_y = int(floor(u_map_target.y + 0.5));
  float cw = 18.0, ch = 12.0, pad = 2.0;
  float gw = float(w) * (cw + pad) - pad;
  float gh = float(h) * (ch + pad) - pad;
  vec2 org = vec2(floor(160.0 - gw * 0.5), floor(76.0 - gh * 0.5));
  vec2 rel = px - org;
  if (rel.x >= 0.0 && rel.y >= 0.0 && rel.x < gw && rel.y < gh) {
    int gx = int(floor(rel.x / (cw + pad)));
    int gy = int(floor(rel.y / (ch + pad)));
    vec2 cell = rel - vec2(float(gx) * (cw + pad), float(gy) * (ch + pad));
    if (gx < w && gy < h && cell.x < cw && cell.y < ch) {
      int bit = gy * w + gx;
      bool vis = mask_bit(u_map_vis, u_map_vis_hi, bit);
      bool cur = (gx == cur_x && gy == cur_y);
      bool tgt = (u_map_target.x >= -0.5 && gx == tgt_x && gy == tgt_y);
      bool bdr = (cell.x < 1.0 || cell.x >= cw - 1.0 || cell.y < 1.0 || cell.y >= ch - 1.0);
      if (vis || cur) {
        if (cur) {
          float p = 0.85 + 0.15 * sin(u_time * 5.0);
          c = bdr ? vec3(1.0, 0.96, 0.65) : vec3(0.95, 0.78, 0.28) * p;
        } else {
          c = bdr ? vec3(0.46, 0.64, 0.82) : vec3(0.22, 0.34, 0.48);
        }
        if (tgt && bdr) {
          float tp = 0.75 + 0.25 * sin(u_time * 7.0);
          c = vec3(0.32, 0.98, 0.82) * tp;
        }
        if (mask_bit(u_map_doors, u_map_doors_hi, bit) &&
            cell.x >= cw - 5.0 && cell.x <= cw - 2.0 && cell.y >= 2.0 && cell.y <= 5.0) {
          c = vec3(0.98, 0.24, 0.34);
        }
        if (mask_bit(u_map_eggs, u_map_eggs_hi, bit) &&
            cell.x >= 2.0 && cell.x <= 5.0 && cell.y >= ch - 5.0 && cell.y <= ch - 2.0) {
          c = cur ? vec3(1.0, 0.99, 0.90) : vec3(1.0, 0.88, 0.30);
        }
      } else {
        c = bdr ? vec3(0.12, 0.14, 0.20) : vec3(0.08, 0.09, 0.13);
      }
    }
  }

  c = mix(c, vec3(0.38, 0.96, 0.80), txt_glyph(8,  px - vec2(28.0, 121.0), 1.0));
  c = mix(c, vec3(0.86, 0.90, 0.96), txt_glyph(9,  px - vec2(28.0, 133.0), 1.0));
  c = mix(c, vec3(0.96, 0.82, 0.42), txt_glyph(10, px - vec2(28.0, 144.0), 1.0));
  c = mix(c, vec3(0.58, 0.62, 0.72), txt_glyph(11, px - vec2(28.0, 158.0), 1.0));
  return c;
}

// Mini-mapa exploratorio Metroidvania en esquina superior derecha:
// oculta salas no visitadas, marca puertas cerradas / huevos pendientes y muestra % explorado.
vec3 hud_minimap(vec3 c, vec2 px) {
  int w = clamp(int(u_map_grid.x + 0.5), 1, 8), h = clamp(int(u_map_grid.y + 0.5), 1, 6);
  int cur_x = int(floor(u_map_grid.z + 0.5)), cur_y = int(floor(u_map_grid.w + 0.5));
  if (w * h <= 1) return c;
  float box_w = max(float(w) * 6.0 + 3.0, 29.0);
  float box_h = float(h) * 5.0 + 11.0;
  vec2 org = vec2(317.0 - box_w, 3.0);
  if (px.x >= org.x && px.x < org.x + box_w && px.y >= org.y && px.y < org.y + box_h) {
    c = mix(c, vec3(0.03, 0.03, 0.05), 0.62);
    vec2 grid_org = org + vec2(box_w - float(w) * 6.0 - 1.0, 2.0);
    vec2 rel = px - grid_org;
    if (rel.x >= 0.0 && rel.y >= 0.0) {
      int gx = int(floor(rel.x / 6.0)), gy = int(floor(rel.y / 5.0));
      vec2 cell = rel - vec2(float(gx) * 6.0, float(gy) * 5.0);
      if (gx < w && gy < h && cell.x < 5.0 && cell.y < 4.0) {
        int bit = gy * w + gx;
        bool vis = mask_bit(u_map_vis, u_map_vis_hi, bit);
        bool cur = (gx == cur_x && gy == cur_y);
        if (vis || cur) {
          if (cur) {
            float p = 0.85 + 0.15 * sin(u_time * 5.0);
            c = vec3(1.0, 0.88, 0.36) * p;
          } else {
            c = vec3(0.30, 0.45, 0.60);
          }
          ivec2 cp = ivec2(floor(cell));
          if (mask_bit(u_map_doors, u_map_doors_hi, bit) && cp.x >= 3 && cp.y <= 1) {
            c = vec3(0.96, 0.24, 0.34);
          }
          if (mask_bit(u_map_eggs, u_map_eggs_hi, bit) && cp.x <= 1 && cp.y >= 2) {
            c = cur ? vec3(1.0, 0.99, 0.88) : vec3(1.0, 0.86, 0.28);
          }
        }
      }
    }
    int pct = clamp(u_map_pct, 0, 100);
    float tx = org.x + box_w - 7.0;
    float ty = org.y + float(h) * 5.0 + 4.0;
    float g = glyph5(GPERCENT, px - vec2(tx, ty));
    g = max(g, glyph5(DIG[pct % 10], px - vec2(tx - 6.0, ty)));
    if (pct >= 10) g = max(g, glyph5(DIG[(pct / 10) % 10], px - vec2(tx - 12.0, ty)));
    if (pct >= 100) g = max(g, glyph5(DIG[pct / 100], px - vec2(tx - 18.0, ty)));
    if (g > 0.0) {
      vec3 tc = (pct == 100) ? vec3(1.0, 0.88, 0.40) : vec3(0.78, 0.88, 0.96);
      c = mix(c, tc, g);
    }
  }
  return c;
}

// Efecto de rayos catódicos (CRT) emanando del boss al ser derrotado: haces radiales con dispersión
// cromática RGB de fósforo, anillos periódicos con scanlines, haz horizontal y destello blanco-cian.
vec3 crt_boss_rays(vec3 c, vec2 px, vec4 burst) {
  float bt = clamp(burst.z, 0.0, 1.0);
  float flash = clamp(burst.w, 0.0, 1.0);
  if (bt <= 0.001 && flash <= 0.001) return c;
  float scan = 0.55 + 0.45 * step(0.5, fract(px.y * 0.5 + u_time * 15.0));
  if (bt > 0.001) {
    float u = 1.0 - bt;
    float env = mix(0.65, 1.0, smoothstep(0.0, 0.05, u)) * (1.0 - smoothstep(0.78, 1.0, u));
    vec2 d = px - burst.xy;
    float r = length(d);
    float ang = atan(d.y, d.x);
    float wave_r = 24.0 + min(1.0, u * 3.0) * 320.0;
    float reach = smoothstep(wave_r, max(1.0, wave_r - 36.0), r) * exp(-r * 0.006);
    // 1) Haces catódicos radiales con separación de cañones RGB
    float spin = u * 34.0;
    float bR = max(0.0, cos((ang - 0.048) * 12.0 - spin + r * 0.08));
    float bG = max(0.0, cos(ang * 12.0 - spin + r * 0.08));
    float bB = max(0.0, cos((ang + 0.048) * 12.0 - spin + r * 0.08));
    vec3 rays = vec3(pow(bR, 5.0), pow(bG, 5.0), pow(bB, 5.0));
    c += rays * vec3(1.0, 0.92, 1.15) * reach * env * scan * 1.35;
    // 2) Anillos concéntricos periódicos de fósforo CRT + haz horizontal de deflexión catódica
    float w1 = fract(u * 2.5);
    float w2 = fract(u * 2.5 + 0.5);
    float w1_env = sin(w1 * 3.14159265) * env;
    float w2_env = sin(w2 * 3.14159265) * env * smoothstep(0.15, 0.22, u);
    float ring1 = exp(-abs(r - (8.0 + w1 * 170.0)) * 0.30);
    float ring2 = exp(-abs(r - (8.0 + w2 * 155.0)) * 0.34);
    c += (ring1 * vec3(0.45, 0.96, 1.0) * w1_env + ring2 * vec3(1.0, 0.42, 0.88) * w2_env) * scan * 1.05;
    float beam_pulse = 0.78 + 0.22 * sin(u * 38.0);
    float beam_env = pow(bt, 0.55) * (1.0 - smoothstep(0.84, 1.0, u)) * beam_pulse;
    float hbeam = exp(-abs(d.y) * 0.52) * exp(-abs(d.x) * 0.0055) * beam_env;
    c += vec3(0.65, 0.98, 1.0) * hbeam * scan * 1.15;
    // 3) Destello del núcleo catódico en el centro del boss
    float core = exp(-r * 0.11) * pow(bt, 0.6);
    c = mix(c, vec3(0.85, 0.98, 1.0), clamp(core * 1.25, 0.0, 1.0));
  }
  if (flash > 0.001) {
    // 4) Destello suave de pantalla en blanco-cian catódico (inicio + pulso secundario)
    vec3 flash_col = mix(vec3(0.58, 0.95, 1.0), vec3(0.95, 0.99, 1.0), flash);
    float flash_amt = clamp(flash * (0.50 + 0.10 * scan), 0.0, 0.65);
    c = mix(c, flash_col, flash_amt);
  }
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
  float att = 1.0 / (1.0 + (d * d) / max(u_lantern_rad * u_lantern_rad * 0.14, 400.0));
  float diff = max(dot(n, normalize(L)), 0.0);
  // Fondo: ambiente frío más alto (se lee la silueta de la sala) escalado por oscuridad del bioma.
  float dark = clamp(u_amb_dark, 0.0, 1.0);
  vec3 fg_amb = vec3(mix(0.16, 0.055, dark));
  vec3 bg_amb = mix(vec3(0.36, 0.38, 0.46), vec3(0.22, 0.25, 0.34), dark);
  vec3 amb = mix(bg_amb, fg_amb, lr);
  vec3 lit = amb + vec3(1.0, 0.85, 0.6) * diff * att * 2.6 * mix(0.45, 1.0, lr);
  vec3 halo = vec3(1.0, 0.84, 0.56) * exp(-d * 0.032) * 0.09;

  // Luces puntuales multipunto: 0=Torch, 1=Crystal, 2=Lava, 3=Shrine.
  int pl_n = min(u_pl_count, 12);
  vec3 core_emissive = vec3(0.0);
  for (int i = 0; i < 12; ++i) {
    if (i >= pl_n) break;
    vec4 plp = u_pl_pos[i];
    vec4 plc = u_pl_col[i];
    vec2 ppos = plp.xy;
    float rad = plp.z;
    int kind = int(floor(plp.w + 0.5));
    float ph = plc.w;

    float flick = 1.0;
    if (kind == 0) {
      flick = 0.86 + 0.09 * sin(u_time * 9.5 + ph * 3.1) + 0.05 * sin(u_time * 17.3 + ph * 5.7);
    } else if (kind == 1) {
      flick = 0.85 + 0.15 * sin(u_time * 2.1 + ph);
    } else if (kind == 2) {
      flick = 0.84 + 0.16 * sin(u_time * 1.45 + ph * 1.7 + 0.4 * sin(u_time * 0.7 + ph));
    } else {
      flick = 0.92 + 0.08 * sin(u_time * 3.2 + ph);
    }

    float pd = length(ppos - px);
    if (pd < rad) {
      float falloff = pow(clamp(1.0 - pd / max(rad, 1.0), 0.0, 1.0), 1.65);
      vec3 pL = normalize(vec3((ppos - px) * vec2(1.0, -1.0), 22.0));
      float pdiff = max(dot(n, pL), 0.0);
      lit += plc.rgb * pdiff * falloff * flick * 1.9 * mix(0.55, 1.0, lr);
      halo += plc.rgb * exp(-pd * 0.12) * falloff * flick * 0.10;
    }

    if (lr < 0.5 && pd < 10.0) {
      vec2 dpx = floor(px) - floor(ppos);
      if (kind == 0) {
        float sway = step(0.15, sin(u_time * 9.5 + ph * 3.1));
        if (dpx.x >= 0.0 && dpx.x <= 1.0 && dpx.y >= 1.0 && dpx.y <= 2.0) {
          core_emissive = max(core_emissive, dpx.y == 1.0 ? vec3(0.42, 0.28, 0.16) : vec3(0.24, 0.16, 0.11));
        } else if (dpx.y == 0.0 && dpx.x >= 0.0 && dpx.x <= 1.0) {
          core_emissive = max(core_emissive, vec3(1.0, 0.94, 0.62) * flick);
        } else if (dpx.y == -1.0 && dpx.x >= 0.0 && dpx.x <= 1.0) {
          core_emissive = max(core_emissive, vec3(1.0, 0.62, 0.18) * flick);
        } else if (dpx.y == -2.0 && dpx.x == sway && sin(u_time * 14.0 + ph * 5.0) > -0.35) {
          core_emissive = max(core_emissive, vec3(1.0, 0.42, 0.10) * flick);
        }
      } else if (kind == 1 && rad > 38.0) {
        if (dpx.x >= 0.0 && dpx.x <= 1.0 && dpx.y >= -1.0 && dpx.y <= 1.0 &&
            !(dpx.x == 1.0 && dpx.y == -1.0)) {
          bool glint = (dpx.x == 0.0 && dpx.y == -1.0) || (dpx.x == 1.0 && dpx.y == 0.0);
          vec3 ccol = glint ? mix(plc.rgb, vec3(1.0), 0.65) : plc.rgb * 0.92;
          core_emissive = max(core_emissive, ccol * (0.85 + 0.35 * flick));
        }
      } else if (kind == 2 && rad > 48.0) {
        if (abs(dpx.x) <= 2.0 && dpx.y >= 1.0 && dpx.y <= 3.0) {
          bool hot = abs(dpx.x) <= 1.0 && dpx.y == 2.0;
          vec3 lcol = hot ? vec3(1.0, 0.72, 0.22) : vec3(0.95, 0.32, 0.08);
          core_emissive = max(core_emissive, lcol * (0.85 + 0.30 * flick));
        }
        float spark_y = -floor(mod(u_time * 4.5 + ph * 3.0, 4.0));
        if (dpx.x == 0.0 && dpx.y == spark_y) {
          core_emissive = max(core_emissive, vec3(1.0, 0.64, 0.18) * flick);
        }
      }
    }
  }

  vec3 c = albedo * lit + halo + core_emissive;
  c += albedo * emit * 1.6;
  c = clamp(c, 0.0, 1.0);
  if (u_boss_burst.z > 0.001 || u_boss_burst.w > 0.001) c = crt_boss_rays(c, px, u_boss_burst);

  if (u_fade > 0.0 && u_hero_sel == 0) {
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

  if (u_hud_fx.x > 0.01 && u_pause == 0 && u_end == 0 && u_hero_sel == 0) {
    vec2 mid = (px - vec2(160.0, 90.0)) / vec2(160.0, 90.0);
    float edge = smoothstep(0.55, 1.32, length(mid));
    c = mix(c, vec3(0.92, 0.14, 0.22), clamp(edge * u_hud_fx.x * 0.65, 0.0, 0.65));
  }

  if (u_items_total > 0 && u_hero_sel == 0) c = hud_items(c, px);
  if (u_max_hp > 0 && u_hero_sel == 0) c = hud_hearts(c, px);
  if (u_seals_total > 0 && u_pause == 0 && u_end == 0 && u_hero_sel == 0) c = hud_seals(c, px);
  if (u_abilities != 0 && u_hero_sel == 0) c = hud_abilities(c, px);
  if (u_boss_hud.y > 0.5 && u_pause == 0 && u_end == 0 && u_hero_sel == 0) c = hud_boss_bar(c, px);
  if (u_map_grid.x * u_map_grid.y > 1 && u_pause == 0 && u_end == 0 && u_hero_sel == 0) c = hud_minimap(c, px);
  if (u_dlg > 0 && u_pause == 0 && u_end == 0 && u_hero_sel == 0) c = dialogue_box(c, px);

  if (u_map_open > 0 && u_pause == 0 && u_end == 0 && u_hero_sel == 0 && u_dlg == 0) {
    c = world_map_page(c, px);
  } else if (u_hero_sel > 0) {
    c = hero_select_page(c, px);
  } else if (u_pause > 0 && u_ctl > 0) {
    c = mix(c, vec3(0.05, 0.05, 0.07), 0.72);
    c = controls_page(c, px);
  } else if (u_pause > 0) {
    c = mix(c, vec3(0.05, 0.05, 0.07), 0.72);
    c = pause_page(c, px);
  }
  o_color = vec4(c, 1.0);
}
