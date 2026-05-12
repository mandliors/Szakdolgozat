#version 330

uniform vec4 color;

out vec4 fragmentColor;

void main()
{
    vec2 circCoord = gl_PointCoord - vec2(0.5);
    float distSq = dot(circCoord, circCoord);

    if (distSq > 0.25)
        discard;

    fragmentColor = color;
}