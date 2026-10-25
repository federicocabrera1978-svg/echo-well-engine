#version 330 core
// Geometry pass stub: escena procedural (piso + bloques) hasta enganchar MapLoader.
in vec2 v_uv;
layout(location = 0) out vec4 o_albedo;
layout(location = 1) out vec4 o_normal;
uniform float u_time;
void main() {
  ivec2 px = ivec2(v_uv * vec2(320.0, 180.0));
  ivec2 tile = px / 8;
  bool solid = tile.y < 3 || (tile.y == 6 && tile.x > 10 && tile.x < 18) || tile.x == 0 || tile.x == 39;
  vec3 albedo = solid ? vec3(0.32, 0.30, 0.38) : vec3(0.05, 0.06, 0.10);
  vec3 n = vec3(0.0, 0.0, 1.0);
  if (solid) {
    ivec2 l = px % 8;  // bisel por tile para que se note la luz
    if (l.x == 0) n = normalize(vec3(-1, 0, 1)); else if (l.x == 7) n = normalize(vec3(1, 0, 1));
    if (l.y == 7) n = normalize(vec3(0, 1, 1)); else if (l.y == 0) n = normalize(vec3(0, -1, 1));
  }
  o_albedo = vec4(albedo, 1.0);
  o_normal = vec4(n * 0.5 + 0.5, 1.0);
  gl_FragDepth = solid ? 0.5 : 1.0;
}
