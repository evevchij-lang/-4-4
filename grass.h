#pragma once
struct GrassInstance {
    glm::vec3 pos;   // центр пучка на земле
    float scale;     // размер
    bool  alive;     // можно убрать граблями
};

std::vector<GrassInstance> g_grassInstances;
GLuint g_grassVAO = 0;
GLuint g_grassVBOQuad = 0;
GLuint g_grassVBOInstances = 0;
GLuint g_grassShader = 0;
GLuint g_grassTex = 0;
GLsizei g_grassAliveCount = 0;

bool RaycastTerrain(const glm::vec3& origin,
    const glm::vec3& dir,
    float maxDist,
    glm::vec3& hitPos)
{
    float t = 0.0f;
    const float step = 0.5f;

    while (t < maxDist)
    {
        glm::vec3 p = origin + dir * t;
        float h = g_terrain.getHeight(p.x, p.z);

        if (p.y <= h + 0.1f) {
            hitPos = glm::vec3(p.x, h, p.z);
            return true;
        }

        t += step;
    }
    return false;
}

void RemoveGrassAt(const glm::vec3& center, float radius)
{
    float r2 = radius * radius;

    for (auto& gi : g_grassInstances)
    {
        if (!gi.alive) continue;
        glm::vec2 d(gi.pos.x - center.x, gi.pos.z - center.z);
        if (glm::dot(d, d) <= r2)
            gi.alive = false;
    }

    // пересобираем буфер только из живых
    std::vector<glm::vec4> data;
    data.reserve(g_grassInstances.size());
    for (auto& gi : g_grassInstances)
        if (gi.alive)
            data.emplace_back(gi.pos, gi.scale);

    g_grassAliveCount = (GLsizei)data.size();

    glBindBuffer(GL_ARRAY_BUFFER, g_grassVBOInstances);
    glBufferData(GL_ARRAY_BUFFER,
        data.size() * sizeof(glm::vec4),
        data.data(),
        GL_DYNAMIC_DRAW);
}



