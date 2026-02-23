#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNor;
layout(location=2) in vec2 aUV;

uniform mat4 uProjection;
uniform mat4 uView;
uniform mat4 uModel;

out vec2 vUV;

void main()
{
    vUV = vec2(aUV.x, 1.0 - aUV.y);
    gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
}