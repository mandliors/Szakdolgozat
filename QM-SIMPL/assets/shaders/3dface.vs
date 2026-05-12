#version 330

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 norm;

uniform mat4 MVP;
uniform mat4 M;
uniform mat4 Minv;

uniform vec3 camPos;

out vec4 position;
out vec3 normal;
out vec3 view;

void main()
{
	gl_Position = MVP * vec4(pos, 1);

	position = M * vec4(pos, 1);
	normal = (vec4(norm, 0) * Minv).xyz;
	view = camPos - position.xyz / position.w;
}