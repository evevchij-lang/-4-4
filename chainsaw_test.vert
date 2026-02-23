#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aTex;

uniform mat4 uProjection;
uniform mat4 uView;
uniform mat4 uModel;
uniform mat4 uNode;

uniform int   uIsChain;
uniform float uTime;
uniform float uChainSpeed;   // например 6.0
uniform float uChainScale;   // например 1.0 (можно 0.5..2.0)

out vec2 vTex;
out vec3 vNormal;

void main()
{
    mat4 M = uModel * uNode;

    vec4 worldPos = M * vec4(aPos, 1.0);
    vNormal = mat3(transpose(inverse(M))) * aNormal;

    // ВАЖНО: UV скроллим только для цепи
    vec2 uv = vec2(aTex.x, 1.0 - aTex.y);

    if (uIsChain == 1)
    {
        // прокрутка по U (можешь поменять на V если надо)
        uv.x += uTime * uChainSpeed;
        uv *= uChainScale;
    }

    vTex = uv;

    gl_Position = uProjection * uView * worldPos;
}
