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
  o_color = vec4(c, 1.0);
}
