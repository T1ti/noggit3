// This file is part of Noggit3, licensed under GNU General Public License (version 3).
#version 330 core

in vec4 position;
in vec3 normal;
in vec4 vertex_color;
in vec2 texcoord;

out vec3 f_position;
out vec2 f_texcoord;
out vec4 f_vertex_color;

uniform mat4 model_view;
uniform mat4 projection;
uniform mat4 transform;

void main()
{
  vec4 pos = transform * position;

  gl_Position = projection * model_view * pos;

  f_position = pos.xyz;
  f_texcoord = texcoord;
  f_vertex_color = vertex_color;
}
