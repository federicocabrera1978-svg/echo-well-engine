#version 330 core
// Light pass: ambiente + luz puntual (sigue al player, con leve parpadeo) sobre el G-buffer.
// Con u_fade > 0: pantalla de meta (oscurece, viñeta dorada, "META" + pista "R").
// HUD (u_items_total > 0): huevo + "n/total" arriba a la izquierda, también sobre la meta.
in vec2 v_uv;
out vec4 o_color;
uniform sampler2D u_albedo;
uniform sampler2D u_normal;
uniform float u_time;
uniform float u_fade;  // 0..1
uniform vec2 u_light;  // px de vista, y hacia abajo
uniform int u_pause;   // 1 = menú de pausa
uniform int u_sel;     // ítem seleccionado 0..4
uniform int u_has_save; // 1 = hay partida guardada (CONTINUAR habilitado)
uniform int u_ctl;      // 1 = página CONTROLES del menú
uniform int u_ctl_sel;  // fila seleccionada (1..10 en u_txt)
uniform int u_ctl_wait; // 1 = esperando tecla/botón
uniform int u_txt[96];  // 12 filas × 32 columnas, 4 glifos de 6 bits por int (63 = nada)
uniform float u_vol;   // 0..1 ajuste (aunque esté mute)
uniform int u_mute;
uniform int u_items;        // collectibles agarrados
uniform int u_items_total;  // 0 = sin HUD
uniform float u_items_flash; // 0..1 destello al agarrar

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

// Fuente completa (menu_text.hpp: A-Z, 0-9, '>', ' ', '-', '?', '/', '<', '.').
const int FONT[43] = int[](18415150, 16301615, 31491134, 16303663, 32554047, 1096767, 15266878, 18415153, 32641183, 6594844, 9604265, 32539681, 18405233, 18667121, 15255086, 1097263, 23385646, 9616943, 16267326, 4329631, 15255089, 4539953, 18732593, 18157905, 4329809, 32575775, 15324974, 14815428, 32584238, 16267791, 8682793, 16268351, 15252526, 2167071, 15252014, 15235630, 1154145, 0, 31744, 4207150, 1118480, 8521864, 4194304);

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
  if (g >= 43) return 0.0;
  return glyph5(FONT[g], (rel - vec2(float(col) * 6.0 * s, 0.0)) / s);
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

void main() {
  vec3 albedo = texture(u_albedo, v_uv).rgb;
  vec3 n = texture(u_normal, v_uv).xyz * 2.0 - 1.0;
  vec2 px = vec2(v_uv.x * 320.0, (1.0 - v_uv.y) * 180.0);
  vec2 lp = u_light + vec2(0.0, -10.0 + sin(u_time * 3.0));
  vec3 L = vec3((lp - px) * vec2(1.0, -1.0), 24.0);  // a espacio y-arriba de las normales
  float d = length(L);
  float att = 1.0 / (1.0 + 0.0008 * d * d);
  float diff = max(dot(n, normalize(L)), 0.0);
  vec3 c = albedo * (vec3(0.12) + vec3(1.0, 0.85, 0.6) * diff * att * 2.6);

  if (u_fade > 0.0) {
    float f = clamp(u_fade, 0.0, 1.0);
    // Oscurece la escena y tiñe hacia un fondo cálido.
    vec2 mid = (px - vec2(160.0, 90.0)) / vec2(160.0, 90.0);
    float vig = smoothstep(0.4, 1.3, length(mid));
    c = mix(c, vec3(0.08, 0.07, 0.05), f * 0.7);
    c *= 1.0 - 0.55 * f;
    c = mix(c, c * vec3(1.05, 0.85, 0.45), vig * f);

    // "META" (escala 4 px) y "R" más chica abajo, aparecen al avanzar el fade.
    float a = smoothstep(0.2, 0.7, f) * (0.8 + 0.2 * sin(u_time * 4.0));
    const int GM = 22730741, GE = 32554047, GT = 4329631, GA = 18415150, GR = 9616943;
    float s = 4.0;
    vec2 o = vec2(160.0 - 12.0 * s, 52.0);  // 4 letras × (5+1) / 2
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

  if (u_items_total > 0) c = hud_items(c, px);

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
