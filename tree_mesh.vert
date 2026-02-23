#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTex;

// матрица экземпляра (из VBO)
layout (location = 3) in mat4 aInstanceModel;

uniform mat4 uProjection;
uniform mat4 uView;

out vec3 vNormal;
out vec3 vWorldPos;
out vec2 vTex;

void main()
{
    vec4 worldPos = aInstanceModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;

    // нормали с учётом масштаба/поворота
    vNormal = mat3(transpose(inverse(aInstanceModel))) * aNormal;

    vTex = aTex; // переворот делаем во frag
    gl_Position = uProjection * uView * worldPos;
}
