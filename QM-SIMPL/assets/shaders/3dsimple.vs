#version 330

layout(location = 0) in vec3 pos;

uniform mat4 MVP;

void main()
{
	gl_Position = MVP * vec4(pos, 1);
	gl_Position.z -= 0.0001 * gl_Position.w;
}