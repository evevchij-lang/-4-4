#version 330 core
in vec2 vTex;
in vec3 vNormal;

out vec4 FragColor;

uniform sampler2D uTex;
uniform int uHasTex;
uniform vec3 uLightDir;

void main()
{
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightDir);
    float diff = max(dot(N, L), 0.0);

    vec3 base = vec3(0.7);
    if (uHasTex == 1)
        base = texture(uTex, vTex).rgb;

    vec3 color = base * (0.25 + 0.75 * diff);
    FragColor = vec4(color, 1.0);
}
