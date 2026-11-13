#version 330 core
// Light pass: ambiente + luz puntual (sigue al player, con leve parpadeo) sobre el G-buffer.
// Con u_fade > 0: pantalla de meta (oscurece, viñeta dorada, "META" + pista "R").
in vec2 v_uv;
out vec4 o_color;
uniform sampler2D u_albedo;
uniform sampler2D u_normal;
uniform float u_time;
uniform float u_fade;  // 0..1
uniform vec2 u_light;  // px de vista, y hacia abajo
uniform int u_pause;   // 1 = menú de pausa
uniform int u_sel;     // ítem seleccionado 0..3
uniform float u_vol;   // 0..1 ajuste (aunque esté mute)
uniform int u_mute;

// Glifo 5×5, bit 0 = izquierda, fila 0 = arriba. bits = filas empaquetadas de a 5.
float glyph5(int bits, vec2 p) {
  ivec2 c = ivec2(floor(p));
  if (c.x < 0 || c.y < 0 || c.x > 4 || c.y > 4) return 0.0;
  return float((bits >> (c.y * 5 + c.x)) & 1);
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

  if (u_pause > 0) {
    // Oscurece y dibuja PAUSA + 4 ítems (SEGUIR, REINICIO, VOL/MUTE, SALIR).
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

    int rows[4 * 8];
    int rlen[4];
    // SEGUIR
    rows[0]=18; rows[1]=4; rows[2]=6; rows[3]=20; rows[4]=8; rows[5]=17; rows[6]=0; rows[7]=0; rlen[0]=6;
    // REINICIO
    rows[8]=17; rows[9]=4; rows[10]=8; rows[11]=13; rows[12]=8; rows[13]=2; rows[14]=8; rows[15]=14; rlen[1]=8;
    // VOL xx  o  MUTE
    if (u_mute > 0) {
      rows[16]=12; rows[17]=20; rows[18]=19; rows[19]=4; rows[20]=0; rows[21]=0; rows[22]=0; rows[23]=0; rlen[2]=4;
    } else {
      int pct = int(clamp(u_vol, 0.0, 1.0) * 100.0 + 0.5);
      rows[16]=21; rows[17]=14; rows[18]=11;  // VOL
      if (pct >= 100) {
        rows[19]=27; rows[20]=26; rows[21]=26; rlen[2]=6;  // VOL100
      } else {
        rows[19]=26 + pct / 10; rows[20]=26 + pct % 10; rlen[2]=5;
      }
    }
    // SALIR
    rows[24]=18; rows[25]=0; rows[26]=11; rows[27]=8; rows[28]=17; rows[29]=0; rows[30]=0; rows[31]=0; rlen[3]=5;

    float is_ = 2.0;
    for (int r = 0; r < 4; ++r) {
      vec2 o = vec2(112.0, 68.0 + float(r) * 18.0);
      float g = 0.0;
      if (r == u_sel)
        g = max(g, glyph5(G[36], (px - vec2(o.x - 14.0, o.y)) / is_));
      for (int i = 0; i < rlen[r]; ++i)
        g = max(g, glyph5(G[rows[r * 8 + i]], (px - o - vec2(float(i) * 6.0 * is_, 0.0)) / is_));
      vec3 col = (r == u_sel) ? vec3(1.0, 0.95, 0.55) : vec3(0.75, 0.75, 0.80);
      c = mix(c, col, g);
    }
    // Barrita de volumen bajo el ítem VOL.
    {
      float bx = 112.0, by = 68.0 + 2.0 * 18.0 + 12.0, bw = 80.0, bh = 3.0;
      if (px.x >= bx && px.x <= bx + bw && px.y >= by && px.y <= by + bh) {
        float f = (px.x - bx) / bw;
        if (u_mute > 0) c = vec3(0.25, 0.22, 0.22);
        else c = (f <= u_vol) ? vec3(0.85, 0.75, 0.35) : vec3(0.22, 0.22, 0.25);
      }
    }
  }
  o_color = vec4(c, 1.0);
}
