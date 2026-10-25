#version 330 core
// Light pass: ambiente + una luz puntual animada sobre el G-buffer.
in vec2 v_uv;
out vec4 o_color;
uniform sampler2D u_albedo;
uniform sampler2D u_normal;
uniform float u_time;
void main() {
  vec3 albedo = texture(u_albedo, v_uv).rgb;
  vec3 n = texture(u_normal, v_uv).xyz * 2.0 - 1.0;
  vec2 px = v_uv * vec2(320.0, 180.0);
  vec2 lp = vec2(160.0 + 110.0 * sin(u_time * 0.7), 90.0 + 25.0 * cos(u_time * 1.1));
  vec3 L = vec3(lp - px, 24.0);
  float d = length(L);
  float att = 1.0 / (1.0 + 0.0008 * d * d);
  float diff = max(dot(n, normalize(L)), 0.0);
  vec3 c = albedo * (vec3(0.08) + vec3(1.0, 0.85, 0.6) * diff * att * 2.2);
  o_color = vec4(c, 1.0);
}
