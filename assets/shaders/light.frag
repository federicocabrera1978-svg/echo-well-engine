#version 330 core
// Light pass: ambiente + luz puntual (sigue al player, con leve parpadeo) sobre el G-buffer.
in vec2 v_uv;
out vec4 o_color;
uniform sampler2D u_albedo;
uniform sampler2D u_normal;
uniform float u_time;
uniform vec2 u_light;  // px de vista, y hacia abajo
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
  o_color = vec4(c, 1.0);
}
