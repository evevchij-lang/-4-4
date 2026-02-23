// main.cpp
// ЮВИ РАЗВЕРТКА!!!!!!!!!!!!!!!!!!!!!!!!!
//vUV = vec2(aUV.x, 1.0 - aUV.y);

#include <windows.h>
#include "glad/glad.h"        

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION

#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <map>
#include <queue>   // std::queue
#include <utility> // std::pair (обычно и так тянется, но пусть будет)
//отладка
float g_rakeSpinAngle = 0;

// Параметры игрока (глобальные)
float g_eyeHeight = 1.7f;    // высота глаз над землёй
float g_playerRadius = 0.6f;    // радиус для коллизии
float g_playerVelY = 0.0f;    // вертикальная скорость
bool  g_onGround = false;   // стоит ли игрок на земле

GLuint g_cutShader = 0;

const float G_GRAVITY = -25.0f;  // гравитация
const float G_JUMP_VEL = 10.0f;   // сила прыжка
GLuint LoadTexture2D(const char* path);
//bool underwater;

bool g_lmbDown = false;
bool g_lmbPressed = false; // true ровно 1 кадр


#include "modelwork.h"

Model g_treeModel;
GLuint g_treeInstanceVBO = 0;
GLsizei g_treeInstanceCount = 0;
std::vector<TreeInstance> g_treeInstances;
GLuint g_treeShader = 0;
std::vector<bool> g_treeRemoved;

// ==== cut animation debug/spawn ====
Model g_treeCutAnimModel;
bool  g_treeCutAnimLoaded = false;

struct CutAnimInstance
{
    bool active = false;
    glm::vec3 pos{ 0,0,0 };
    glm::vec3 rot{ 0,0,0 };   // radians (x,y,z)
    float scale = 1.0f;

    float t = 0.0f;         // время клипа
    float duration = 1.25f; // ДЛИТЕЛЬНОСТЬ ИЗ test_cut.glb
} g_cutAnim;







float g_time = 0.0f; // глобальное время, накапливается в главном цикле

//Выбор инструмента
enum Tool {
    TOOL_NONE = 0,
    TOOL_RAKE = 1,
    TOOL_SHOVEL = 2,
    TOOL_CHAINSAW_TEST = 3
};
int g_currentTool;

// ===== глобальные переменные окна/рендера =====

HDC g_hDC = nullptr;
HGLRC g_hRC = nullptr;
HWND g_hWnd = nullptr;
bool g_running = true;

// размеры окна
int g_winWidth = 1280;
int g_winHeight = 720;

// мышь
bool g_mouseCaptured = false;
POINT g_centerPos;
float g_mouseSensitivity = 0.12f;

// тайминг
LARGE_INTEGER g_freq;
LARGE_INTEGER g_prevTime;

// шейдер
GLuint g_shader = 0;

GLuint g_postShader = 0;

  

// ===== УТИЛИТЫ =====

void CheckGL(const char* where)
{
#ifdef _DEBUG
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        char buf[256];
        sprintf_s(buf, "GL error 0x%X at %s\n", err, where);
        OutputDebugStringA(buf);
    }
#endif
}

std::string LoadTextFile(const char* path)
{
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

GLuint CreateShaderProgram(const char* vsPath, const char* fsPath)
{
    auto vsSrc = LoadTextFile(vsPath);
    auto fsSrc = LoadTextFile(fsPath);

    auto compile = [](GLenum type, const std::string& src)->GLuint {
        GLuint s = glCreateShader(type);
        const char* c = src.c_str();
        glShaderSource(s, 1, &c, nullptr);
        glCompileShader(s);
        GLint ok = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[1024];
            glGetShaderInfoLog(s, 1024, nullptr, log);
            OutputDebugStringA(log);
        }
        return s;
        };

    GLuint vs = compile(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fsSrc);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(prog, 1024, nullptr, log);
        OutputDebugStringA(log);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}



// ===== CAMERA =====

#include "camera.h"

//struct Camera {
//    glm::vec3 pos;
//    float yaw;
//    float pitch;
//    glm::vec3 front;
//    glm::vec3 up;
//    glm::vec3 right;
//
//    Camera(glm::vec3 startPos)
//        : pos(startPos), yaw(-90.0f), pitch(0.0f)
//    {
//        updateVectors();
//    }
//
//    void updateVectors()
//    {
//        glm::vec3 f;
//        f.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
//        f.y = sin(glm::radians(pitch));
//        f.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
//        front = glm::normalize(f);
//        right = glm::normalize(glm::cross(front, glm::vec3(0, 1, 0)));
//        up = glm::normalize(glm::cross(right, front));
//    }
//
//    glm::mat4 getView() const
//    {
//        return glm::lookAt(pos, pos + front, up);
//    }
//};

Camera g_cam(glm::vec3(0.0f, 10.0f, 20.0f));
//

// ===== TERRAIN =====
#include "env_globals.h"
#include "water.h"
#include "sky.h"

struct Terrain {
    GLuint vao = 0, vbo = 0, ebo = 0;
    int vertsPerSide = 0;
    float size = 0.0f;
    float maxHeight = 0.0f;
    int      width;    // кол-во вершин по X
    int      height;   // кол-во вершин по Z
    GLuint texture = 0;
    std::vector<float> heights;
    std::vector<float> material;

    // heightmap
    int hmW = 0, hmH = 0;
    std::vector<float> hmData; // [0..1]

    bool hasHeightmap() const { return !hmData.empty(); }

    float sampleHeightmap(float xNorm, float zNorm) const
    {
        if (!hasHeightmap()) return 0.0f;
        xNorm = glm::clamp(xNorm, 0.0f, 1.0f);
        zNorm = glm::clamp(zNorm, 0.0f, 1.0f);

        float fx = xNorm * (hmW - 1);
        float fz = zNorm * (hmH - 1);
        int x0 = (int)fx;
        int z0 = (int)fz;
        int x1 = glm::min(x0 + 1, hmW - 1);
        int z1 = glm::min(z0 + 1, hmH - 1);
        float tx = fx - x0;
        float tz = fz - z0;

        auto H = [&](int x, int z) {
            return hmData[z * hmW + x] * maxHeight;
            };

        float h0 = H(x0, z0) * (1 - tx) + H(x1, z0) * tx;
        float h1 = H(x0, z1) * (1 - tx) + H(x1, z1) * tx;
        return h0 * (1 - tz) + h1 * tz;
    }

    // процедурная функция если нет heightmap
    float func(float x, float z) const
    {
        return std::sin(x * 0.08f) * std::cos(z * 0.08f) * maxHeight * 0.5f;
    }

    float getHeight(float worldX, float worldZ) const
    {
        float half = size * 0.5f;
        if (worldX < -half || worldX > half || worldZ < -half || worldZ > half)
            return 0.0f;

        // Если есть актуальная сетка высот — пользуем её
        if (!heights.empty() && width > 1 && height > 1)
        {
            float cell = size / float(width - 1);

            float fx = (worldX + half) / cell;
            float fz = (worldZ + half) / cell;

            int x0 = (int)floorf(fx);
            int z0 = (int)floorf(fz);
            int x1 = x0 + 1;
            int z1 = z0 + 1;

            x0 = std::max(0, std::min(width - 1, x0));
            x1 = std::max(0, std::min(width - 1, x1));
            z0 = std::max(0, std::min(height - 1, z0));
            z1 = std::max(0, std::min(height - 1, z1));

            float tx = fx - x0;
            float tz = fz - z0;

            auto H = [&](int x, int z) {
                return heights[z * width + x];
                };

            float h00 = H(x0, z0);
            float h10 = H(x1, z0);
            float h01 = H(x0, z1);
            float h11 = H(x1, z1);

            float h0 = h00 + (h10 - h00) * tx;
            float h1 = h01 + (h11 - h01) * tx;
            return h0 + (h1 - h0) * tz;
        }

        // fallback, если heights почему-то пуст
        if (hasHeightmap()) {
            float xn = (worldX + half) / size;
            float zn = (worldZ + half) / size;
            return sampleHeightmap(xn, zn);
        }
        else {
            return func(worldX, worldZ);
        }
    }

    void loadHeightmap(const char* path)
    {
        int ch = 0;
        unsigned char* data = stbi_load(path, &hmW, &hmH, &ch, 1);
        if (!data) {
            OutputDebugStringA("Failed to load heightmap\n");
            return;
        }
        hmData.resize(hmW * hmH);
        for (int i = 0; i < hmW * hmH; ++i) {
            hmData[i] = data[i] / 255.0f;
        }
        stbi_image_free(data);
    }

    void build(int n, float worldSize, float h)
    {
        vertsPerSide = n;
        size = worldSize;
        maxHeight = h;

        width = n;
        height = n;

        heights.assign(width * height, 0.0f);
        material.assign(width * height, 0.0f); // 0 = земля

        std::vector<float> vertices;        // pos3 + normal3 + uv2 + mat1
        std::vector<unsigned int> indices;

        float step = size / float(n - 1);
        float half = size * 0.5f;

        auto Hworld = [&](float wx, float wz)
            {
                if (hasHeightmap()) {
                    float xn = (wx + half) / size;
                    float zn = (wz + half) / size;
                    return sampleHeightmap(xn, zn);
                }
                else {
                    return std::sin(wx * 0.08f) * std::cos(wz * 0.08f) * maxHeight * 0.5f;
                }
            };

        // заполняем heights
        for (int z = 0; z < height; ++z)
        {
            for (int x = 0; x < width; ++x)
            {
                float wx = -half + x * step;
                float wz = -half + z * step;
                heights[z * width + x] = Hworld(wx, wz);
            }
        }

        auto getH = [&](int x, int z)
            {
                x = std::max(0, std::min(width - 1, x));
                z = std::max(0, std::min(height - 1, z));
                return heights[z * width + x];
            };

        vertices.reserve(width * height * 9);

        // позиции, нормали, uv, material
        for (int z = 0; z < height; ++z)
        {
            for (int x = 0; x < width; ++x)
            {
                float wx = -half + x * step;
                float wz = -half + z * step;
                float wy = getH(x, z);

                float hl = getH(x - 1, z);
                float hr = getH(x + 1, z);
                float hd = getH(x, z - 1);
                float hu = getH(x, z + 1);
                glm::vec3 normal(hl - hr, 2.0f, hd - hu);
                normal = glm::normalize(normal);

                float u = (float)x / (float)(n - 1) * 16.0f;
                float v = (float)z / (float)(n - 1) * 16.0f;

                vertices.push_back(wx);
                vertices.push_back(wy);
                vertices.push_back(wz);

                vertices.push_back(normal.x);
                vertices.push_back(normal.y);
                vertices.push_back(normal.z);

                vertices.push_back(u);
                vertices.push_back(v);

                vertices.push_back(0.0f); // material = земля
            }
        }

        // индексы
        for (int z = 0; z < n - 1; ++z)
        {
            for (int x = 0; x < n - 1; ++x)
            {
                unsigned int i0 = z * n + x;
                unsigned int i1 = z * n + x + 1;
                unsigned int i2 = (z + 1) * n + x;
                unsigned int i3 = (z + 1) * n + x + 1;

                indices.push_back(i0); indices.push_back(i2); indices.push_back(i1);
                indices.push_back(i1); indices.push_back(i2); indices.push_back(i3);
            }
        }

        // буферы
        if (!vao) glGenVertexArrays(1, &vao);
        if (!vbo) glGenBuffers(1, &vbo);
        if (!ebo) glGenBuffers(1, &ebo);

        glBindVertexArray(vao);

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER,
            vertices.size() * sizeof(float),
            vertices.data(),
            GL_DYNAMIC_DRAW); // динамический, будем обновлять

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            indices.size() * sizeof(unsigned int),
            indices.data(),
            GL_STATIC_DRAW);

        GLsizei stride = 9 * sizeof(float);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));

        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float)));

        glBindVertexArray(0);
    }



    void draw() const
    {
        glBindVertexArray(vao);
        int quadCount = (vertsPerSide - 1) * (vertsPerSide - 1);
        glDrawElements(GL_TRIANGLES, quadCount * 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    void Dig(const glm::vec3& center, float radius)
    {
        if (width <= 1 || height <= 1 || heights.empty())
            return;

        float half = size * 0.5f;
        float cell = size / float(width - 1);

        // шаг по высоте сетки
        const float gridStep = 1.5f;

        // текущая высота в точке удара
        float baseH = getHeight(center.x, center.z);

        // целевой уровень — ближайший МЕНЬШИЙ уровень сетки
        // (чуть вычитаем, чтобы повторный вызов уходил на следующий "этаж")
        float targetH = std::floor((baseH - 0.001f) / gridStep) * gridStep;

        // радиусы: внутреннее плоское дно и внешние склоны
        float rFlat = radius * 0.5f;
        float rOuter = radius;

        int ixMin = std::max(0, int(std::floor((center.x - rOuter + half) / cell)));
        int ixMax = std::min(width - 1, int(std::ceil((center.x + rOuter + half) / cell)));
        int izMin = std::max(0, int(std::floor((center.z - rOuter + half) / cell)));
        int izMax = std::min(height - 1, int(std::ceil((center.z + rOuter + half) / cell)));

        for (int z = izMin; z <= izMax; ++z)
        {
            for (int x = ixMin; x <= ixMax; ++x)
            {
                int idx = z * width + x;
                float vx = -half + x * cell;
                float vz = -half + z * cell;

                float dx = vx - center.x;
                float dz = vz - center.z;
                float dist = std::sqrt(dx * dx + dz * dz);

                if (dist <= rFlat)
                {
                    // жёсткое плоское дно
                    heights[idx] = std::min(heights[idx], targetH);
                    if (!material.empty())
                        material[idx] = 1.0f; // чистый песок
                }
                else if (dist <= rOuter)
                {
                    // плавный склон между baseH (снаружи) и targetH (у края дна)
                    float t = (dist - rFlat) / (rOuter - rFlat); // 0..1
                    float desiredH = targetH + (baseH - targetH) * t;

                    heights[idx] = std::min(heights[idx], desiredH);

                    // песка меньше на склоне
                    if (!material.empty())
                    {
                        float sand = 1.0f - t;
                        material[idx] = std::max(material[idx], sand);
                    }
                }
                // вне rOuter ничего не трогаем
            }
        }

        RebuildVertices();
        RebuildWaterMask();
        UploadWaterMaskFromTerrain();
    }
    void UploadWaterMaskFromTerrain()
    {
        if (waterW <= 0 || waterH <= 0 ||
            waterMask.empty())
            return;

        if (!g_waterMaskTex)
            glGenTextures(1, &g_waterMaskTex);

        glBindTexture(GL_TEXTURE_2D, g_waterMaskTex);

        glTexImage2D(
            GL_TEXTURE_2D, 0,
            GL_R8,                           // один канал (маска)
            waterW,
            waterH,
            0,
            GL_RED, GL_UNSIGNED_BYTE,
            waterMask.data()
        );

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void RebuildVertices()
    {
        if (!vbo || width <= 0 || height <= 0 || heights.empty())
            return;

        std::vector<float> verts(width * height * 9);

        float half = size * 0.5f;
        float cell = size / float(width - 1);

        // позиции, uv, material
        for (int z = 0; z < height; ++z)
        {
            for (int x = 0; x < width; ++x)
            {
                int idx = z * width + x;
                float wx = -half + x * cell;
                float wz = -half + z * cell;
                float wy = heights[idx];

                int o = idx * 9;
                verts[o + 0] = wx;
                verts[o + 1] = wy;
                verts[o + 2] = wz;

                // временная нормаль — позже пересчитаем
                verts[o + 3] = 0.0f;
                verts[o + 4] = 1.0f;
                verts[o + 5] = 0.0f;

                verts[o + 6] = (float)x / (width - 1) * 16.0f;
                verts[o + 7] = (float)z / (height - 1) * 16.0f;

                float m = material.empty() ? 0.0f : material[idx];
                verts[o + 8] = m;
            }
        }

        // нормали
        for (int z = 1; z < height - 1; ++z)
        {
            for (int x = 1; x < width - 1; ++x)
            {
                float hL = heights[z * width + (x - 1)];
                float hR = heights[z * width + (x + 1)];
                float hD = heights[(z - 1) * width + x];
                float hU = heights[(z + 1) * width + x];

                glm::vec3 n = glm::normalize(glm::vec3(
                    hL - hR,
                    2.0f * cell,
                    hD - hU
                ));

                int o = (z * width + x) * 9;
                verts[o + 3] = n.x;
                verts[o + 4] = n.y;
                verts[o + 5] = n.z;
            }
        }

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0,
            verts.size() * sizeof(float),
            verts.data());
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void RebuildWaterMask()
    {
        // карта воды имеет тот же размер, что и сетка высот
        waterW = width;
        waterH = height;

        if (waterW <= 0 || waterH <= 0 || heights.empty())
            return;

        waterMask.assign(waterW * waterH, 0);

        std::queue< std::pair<int, int> > q;

        // вспомогательная лямбда в духе TryPushCell
        auto TryPush = [&](int x, int z)
            {
                if (x < 0 || x >= waterW || z < 0 || z >= waterH)
                    return;

                int idx = z * waterW + x;
                if (waterMask[idx])     // уже помечен
                    return;

                float h = heights[idx]; // текущая высота террейна
                if (h < g_waterHeight)  // ниже уровня моря — потенциал воды
                {
                    q.push(std::make_pair(x, z));
                }
            };

        // стартуем с краёв карты (океан вокруг)
        for (int x = 0; x < waterW; ++x)
        {
            TryPush(x, 0);           // верхняя кромка
            TryPush(x, waterH - 1);  // нижняя
        }
        for (int z = 0; z < waterH; ++z)
        {
            TryPush(0, z);    // левая
            TryPush(waterW - 1, z);    // правая
        }

        // flood fill
        while (!q.empty())
        {
            std::pair<int, int> p = q.front();
            q.pop();
            int x = p.first;
            int z = p.second;

            int idx = z * waterW + x;
            if (waterMask[idx])
                continue;

            waterMask[idx] = 255;

            // 4-соседа
            TryPush(x + 1, z);
            TryPush(x - 1, z);
            TryPush(x, z + 1);
            TryPush(x, z - 1);
        }
    }


    void DrawWater(const glm::mat4& proj, const glm::mat4& view)
    {
        if (!g_waterShader || !g_waterVAO) return;

        glDepthRange(0.0, 1.0);

        glUseProgram(g_waterShader);

        // подводный режим
        glUniform1i(glGetUniformLocation(g_waterShader, "uUnderwater"),underwater ? 1 : 0);

        if (underwater) {
            glDisable(GL_CULL_FACE);
            glUniform1f(glGetUniformLocation(g_waterShader, "uAlpha"), 0.85f);
        }
        else {
            glUniform1f(glGetUniformLocation(g_waterShader, "uAlpha"), 0.95f);
        }

        glUniformMatrix4fv(glGetUniformLocation(g_waterShader, "uProjection"),
            1, GL_FALSE, &proj[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(g_waterShader, "uView"),
            1, GL_FALSE, &view[0][0]);
        glUniform1f(glGetUniformLocation(g_waterShader, "uTime"), g_time);

        // цвет воды (можно позже вынести в параметры)
        glUniform3f(glGetUniformLocation(g_waterShader, "uWaterColor"),
            0.0f, 0.4f, 1.0f);

        // === НОВОЕ: маска воды и размер террейна ===
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_waterMaskTex);
        glUniform1i(glGetUniformLocation(g_waterShader, "uWaterMask"), 0);

        glUniform1f(glGetUniformLocation(g_waterShader, "uTerrainSize"), size);


        // новые uniforms
        glm::vec3 skyColor(0.55f, 0.72f, 0.95f); // под цвет твоего неба
        glUniform3fv(glGetUniformLocation(g_waterShader, "uSkyColor"),
            1, &skyColor[0]);

        glm::vec3 lightDir = glm::normalize(glm::vec3(0.4f, 1.0f, 0.2f));
        glUniform3fv(glGetUniformLocation(g_waterShader, "uLightDir"),
            1, &lightDir[0]);

        glUniform3fv(glGetUniformLocation(g_waterShader, "uCamPos"),
            1, &g_cam.pos[0]);
        glUniform3fv(glGetUniformLocation(g_waterShader, "uFogColor"),
            1, &fogColorUnder[0]);   // или твой глобальный
        glUniform1f(glGetUniformLocation(g_waterShader, "uFogDensity"),
            fogDensityUnder);        // тот же, что для подводного тумана


        // рендер стейты
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDisable(GL_CULL_FACE);

        glBindVertexArray(g_waterVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

};

GLuint g_terrainGrassTex = 0;
GLuint g_terrainSandTex = 0;
Terrain g_terrain;


#include "grass.h"
#include "rake.h"
#include "shovel.h"
#include "chainsaw_test.h"

void RemoveGrassInRadius(const glm::vec3& center, float radius);
bool IsTreeBlockingDig(const glm::vec3& center, float holeRadius);
//FBO++


void InitSceneFBO(int w, int h);
GLuint g_sceneFBO = 0;
GLuint g_sceneColorTex = 0;
GLuint g_sceneDepthRBO = 0;
//FBO--

//создаём VAO/VBO++
GLuint g_screenVAO = 0, g_screenVBO = 0;
void InitScreenQuad();
//создаём VAO/VBO--

struct Grass {
    GLuint vao = 0;
    GLuint vboQuad = 0;
    GLuint vboInstances = 0;
    GLuint tex = 0;
    GLsizei instanceCount = 0;
};

Grass g_grass;
//GLuint g_grassShader = 0;

void InitGrass();
void DrawGrass(const glm::mat4& proj, const glm::mat4& view);


// ===== ИНИЦИАЛИЗАЦИЯ OPENGL КОНТЕКСТА =====

HGLRC CreateGLContext(HDC hdc)
{
    // обычный контекст
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cDepthBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int pf = ChoosePixelFormat(hdc, &pfd);
    SetPixelFormat(hdc, pf, &pfd);

    HGLRC temp = wglCreateContext(hdc);
    wglMakeCurrent(hdc, temp);

    // Загружаем функции WGL для создания современного контекста, потом:
    // здесь по-хорошему wglCreateContextAttribsARB, но чтобы не раздувать:
    // просто остаёмся на temp-контексте (многие драйверы дают 3.3+ и так).
    // Если нужно строго core 3.3 — можем расписать отдельным блоком.

    if (!gladLoadGL()) {
        OutputDebugStringA("Failed to load GL with GLAD\n");
    }

    return temp;
}

// ===== ОБНОВЛЕНИЕ КАМЕРЫ И МЫШИ =====

void CaptureMouse(bool capture)
{
    g_mouseCaptured = capture;
    if (capture) {
        RECT rc;
        GetClientRect(g_hWnd, &rc);
        POINT c = { (rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2 };
        ClientToScreen(g_hWnd, &c);
        g_centerPos = c;
        SetCursorPos(c.x, c.y);
        ShowCursor(FALSE);
    }
    else {
        ShowCursor(TRUE);
    }
}

void ProcessMouse()
{
    if (!g_mouseCaptured) return;

    POINT p;
    GetCursorPos(&p);

    int dx = p.x - g_centerPos.x;
    int dy = p.y - g_centerPos.y;

    if (dx != 0 || dy != 0) {
        g_cam.yaw += dx * g_mouseSensitivity;
        g_cam.pitch -= dy * g_mouseSensitivity;

        if (g_cam.pitch > 89.0f)  g_cam.pitch = 89.0f;
        if (g_cam.pitch < -89.0f) g_cam.pitch = -89.0f;

        g_cam.updateVectors();

        SetCursorPos(g_centerPos.x, g_centerPos.y);
    }
}

void TryStartCut();

void UpdateCamera(float dt)
{

    //if (GetAsyncKeyState(VK_ESCAPE) & 0x0001)
    //{
    //    g_running = false;  // или как у тебя называется флаг цикла
    //    return;
    //}
    
    ProcessMouse();

    if (g_cutAnim.active)
    {
        float step = 0.05f;
        float rotStep = glm::radians(2.0f);

        if (GetAsyncKeyState(VK_LEFT) & 0x8000) g_cutAnim.pos.x -= step;
        if (GetAsyncKeyState(VK_RIGHT) & 0x8000) g_cutAnim.pos.x += step;
        if (GetAsyncKeyState(VK_UP) & 0x8000) g_cutAnim.pos.z -= step;
        if (GetAsyncKeyState(VK_DOWN) & 0x8000) g_cutAnim.pos.z += step;

        if (GetAsyncKeyState(VK_PRIOR) & 0x8000) g_cutAnim.pos.y += step; // PageUp
        if (GetAsyncKeyState(VK_NEXT) & 0x8000) g_cutAnim.pos.y -= step; // PageDown

        if (GetAsyncKeyState(VK_NUMPAD1) & 0x8000) g_cutAnim.rot.y -= rotStep;
        if (GetAsyncKeyState(VK_NUMPAD3) & 0x8000) g_cutAnim.rot.y += rotStep;

        if (GetAsyncKeyState(VK_OEM_MINUS) & 0x8000)
            g_cutAnim.scale = glm::max(0.01f, g_cutAnim.scale - 0.01f);

        if (GetAsyncKeyState(VK_OEM_PLUS) & 0x8000)
            g_cutAnim.scale += 0.01f;
    }

    if (g_cuttingTree && g_lockPlayerDuringCut)
    {
        // но! не забывай обновлять таймер анимации
        //UpdateCut(dt);
        UpdateCutAnim(dt);
        return; // отрезаем управление движением на время пиления
    }

    

    


    auto key = [](int vk) {
        return (GetAsyncKeyState(vk) & 0x8000) != 0;
        };

    float moveSpeed = 10.0f;
    glm::vec3 move(0.0f);

    //Клавиши
    if (key('W')) move += g_cam.front;
    if (key('S')) move -= g_cam.front;
    if (key('A')) move -= g_cam.right;
    if (key('D')) move += g_cam.right;

    //Управление инструментом
    if (GetAsyncKeyState('1') & 0x0001) { // 0x0001 = нажатие (edge)
        g_currentTool = (g_currentTool == TOOL_RAKE) ? TOOL_NONE : TOOL_RAKE;
    }

    if (GetAsyncKeyState('2') & 0x0001)
        g_currentTool = (g_currentTool == TOOL_SHOVEL) ? TOOL_NONE : TOOL_SHOVEL;

    if (GetAsyncKeyState('0') & 0x0001)
        g_currentTool = (g_currentTool == TOOL_CHAINSAW_TEST) ? TOOL_NONE : TOOL_CHAINSAW_TEST;

    bool key9 = (GetAsyncKeyState('9') & 0x0001) != 0;
    if (key9)
    {
        glm::vec3 p = g_cam.pos + g_cam.front * 2.0f;
        p.y = g_terrain.getHeight(p.x, p.z); // на землю
        StartCutAnimAt(p);
    }


    //Если выбраны грабли отлов левой клавиши мыши для  удаления травы
    // Запуск анимации взмаха граблями по клику
    if (g_currentTool == TOOL_RAKE &&
        (GetAsyncKeyState(VK_LBUTTON) & 0x8000) &&
        !g_rakeSwinging)
    {
        g_rakeSwinging = true;
        g_rakeSwingTime = 0.0f;
        g_rakeHitDone = false;
    }

    // Лопата: копаем ямку ЛКМ, радиус 0.5м, глубина 0.5м, дистанция до 3м
    if (g_currentTool == TOOL_SHOVEL &&
        (GetAsyncKeyState(VK_LBUTTON) & 0x8000) &&
        !g_shovelSwinging)
    {
        g_shovelSwinging = true;
        g_shovelSwingTime = 0.0f;
        g_shovelHitDone = false;
    }

    // 2) ЛКМ "одноразовое нажатие" (edge) — сработает ровно 1 кадр
    bool lmbPressed = (GetAsyncKeyState(VK_LBUTTON) & 0x0001) != 0;

    // 3) Старт распила ТОЛЬКО если в руках бензопила
    if (g_currentTool == TOOL_CHAINSAW_TEST && lmbPressed && !g_cuttingTree)
    {
        // ВАЖНО: сюда нужно передать позиции деревьев
        TryStartCut();   // <- замени g_treePositions на твой реальный массив/вектор
    }


    move.y = 0.0f;
    if (glm::length(move) > 0.0f)
        move = glm::normalize(move) * moveSpeed * dt;

    glm::vec3 newPos = g_cam.pos + move;

    // коллизии с деревьями по XZ
    ResolveTreeCollisions(newPos);

    // высота земли под новой позицией
    float groundY = g_terrain.getHeight(newPos.x, newPos.z);
    float feetY = g_cam.pos.y - g_eyeHeight;

    // проверка "стоял ли на земле"
    if (fabs(feetY - groundY) < 0.05f || feetY < groundY + 0.02f)
    {
        g_onGround = true;
        feetY = groundY;
        g_playerVelY = 0.0f;
    }
    else {
        g_onGround = false;
    }

    // прыжок
    if (g_onGround && key(VK_SPACE)) {
        g_playerVelY = G_JUMP_VEL;
        g_onGround = false;
    }

    // гравитация
    g_playerVelY += G_GRAVITY * dt;
    feetY += g_playerVelY * dt;

    // не провалиться
    groundY = g_terrain.getHeight(newPos.x, newPos.z);
    if (feetY < groundY) {
        feetY = groundY;
        g_playerVelY = 0.0f;
        g_onGround = true;
    }

    newPos.y = feetY + g_eyeHeight;
    g_cam.pos = newPos;

    // Обновление анимации граблей
    if (g_rakeSwinging)
    {
        g_rakeSwingTime += dt;
        float t = g_rakeSwingTime / g_rakeSwingDuration;

        // момент удара в середине анимации
        if (!g_rakeHitDone && t > 0.4f)
        {
            glm::vec3 hit;
            if (RaycastTerrain(g_cam.pos, g_cam.front, 6.0f, hit)) {
                RemoveGrassAt(hit, 2.5f);  // квадрат примерно 1x1 м
            }
            g_rakeHitDone = true;
        }

        if (t >= 1.0f)
        {
            g_rakeSwinging = false;
            g_rakeSwingTime = 0.0f;
            g_rakeHitDone = false;
        }
    }

    // лопата
    if (g_shovelSwinging)
    {
        g_shovelSwingTime += dt;

        // момент удара — примерно середина
        if (!g_shovelHitDone && g_shovelSwingTime >= g_shovelSwingDuration * 0.45f)
        {
            g_shovelHitDone = true;

            glm::vec3 hit;
            float holeRadius = 2.0f;
            if (RaycastTerrain(g_cam.pos, g_cam.front, 3.0f, hit))
            {
                if (!IsTreeBlockingDig(hit, holeRadius))
                {
                    RemoveGrassInRadius(hit, holeRadius);
                    g_terrain.Dig(hit, holeRadius);
                }
            }
        }

        if (g_shovelSwingTime >= g_shovelSwingDuration)
        {
            g_shovelSwingTime = g_shovelSwingDuration;
            g_shovelSwinging = false;
            g_shovelHitDone = false;
        }
    }

    if (g_currentTool == TOOL_RAKE)
    {
        bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

        if (lmb)
        {
            // скорость вращения, 90 градусов в секунду, крути как хочешь
            float speed = glm::radians(90.0f);
            g_rakeSpinAngle += speed * dt;

            // чтобы число не разрасталось
            if (g_rakeSpinAngle > glm::two_pi<float>())
                g_rakeSpinAngle -= glm::two_pi<float>();
        }
        // если кнопка не нажата — ничего не трогаем, угол сохраняется
    }

}

bool IsCameraUnderwater()
{
    float half = g_terrain.size * 0.5f;
    float xn = (g_cam.pos.x + half) / g_terrain.size;
    float zn = (g_cam.pos.z + half) / g_terrain.size;

    int x = int(xn * (waterW - 1));
    int z = int(zn * (waterH - 1));
    if (x < 0 || x >= waterW || z < 0 || z >= waterH) return false;

    int idx = z * waterW + x;
    float h = g_terrain.heights[idx]; // текущая высота террейна в этой клетке

    bool hasWaterHere = (waterMask[idx] != 0);
    return hasWaterHere && (g_cam.pos.y < g_waterHeight - 0.05f);
}

// ===== РЕНДЕР =====

void Render()
{
    // 1) Рисуем МИР в FBO
    glBindFramebuffer(GL_FRAMEBUFFER, g_sceneFBO);
    
    glViewport(0, 0, g_winWidth, g_winHeight);
   
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    if(!underwater)
        glClearColor(0.5f, 0.7f, 1.0f, 1.0f);
    else
        glClearColor(0.29f, 0.44f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 proj = glm::perspective(glm::radians(60.0f),
        float(g_winWidth) / float(g_winHeight),
        0.1f, 500.0f);
    glm::mat4 view = g_cam.getView();
    glm::mat4 model = glm::mat4(1.0f);

    DrawSkySphere(proj, view);

    // === Для подводы ===
    underwater = IsCameraUnderwater();
    

    // Раздаём туман всем шейдерам МИРА
    for (auto sh : { g_shader, g_treeShader, g_grassShader, g_waterShader })
    {
        glUseProgram(sh);

        glUniform3fv(glGetUniformLocation(sh, "uCamPos"), 1, &g_cam.pos[0]);
        glUniform1i(glGetUniformLocation(sh, "uUnderwater"), underwater ? 1 : 0);

        if (underwater)
        {
            glUniform3fv(glGetUniformLocation(sh, "uFogColor"), 1, &fogColorUnder[0]);
            glUniform1f(glGetUniformLocation(sh, "uFogDensity"), fogDensityUnder);
        }
        else
        {
            glUniform3fv(glGetUniformLocation(sh, "uFogColor"), 1, &fogColorTop[0]);
            glUniform1f(glGetUniformLocation(sh, "uFogDensity"), fogDensityTop);
        }
    }

   

    // === ТЕРРЕЙН ===
    glUseProgram(g_shader);

    glm::vec3 lightDir = glm::normalize(glm::vec3(0.4f, 1.0f, 0.2f));
    glUniform3fv(glGetUniformLocation(g_shader, "uLightDir"), 1, &lightDir[0]);
    glUniformMatrix4fv(glGetUniformLocation(g_shader, "uProjection"), 1, GL_FALSE, &proj[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(g_shader, "uView"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(g_shader, "uModel"), 1, GL_FALSE, &model[0][0]);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_terrainGrassTex);
    glUniform1i(glGetUniformLocation(g_shader, "uTexGrass"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_terrainSandTex);
    glUniform1i(glGetUniformLocation(g_shader, "uTexSand"), 1);

    

    g_terrain.draw();

    // === ДЕРЕВЬЯ ===
    DrawTreeObjects(proj, view);

    // === ТРАВА ===
    DrawGrass(proj, view);

    // === ВОДА ===
    g_terrain.DrawWater(proj, view);

    //распиливание дерева
    DrawCutAnim(proj, view);

    // 2) Пост-обработка: рисуем FBO на ЭКРАН
    glBindFramebuffer(GL_FRAMEBUFFER, 0); // backbuffer
    glViewport(0, 0, g_winWidth, g_winHeight);

    glDisable(GL_DEPTH_TEST);   // depth нам не нужен для полноэкранного квадрата
    glDisable(GL_BLEND);        // ВАЖНО: без смешивания с прошлым кадром

    glUseProgram(g_postShader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_sceneColorTex);
    glUniform1i(glGetUniformLocation(g_postShader, "uSceneTex"), 0);
    glUniform1i(glGetUniformLocation(g_postShader, "uUnderwater"), underwater ? 1 : 0);
    glUniform1f(glGetUniformLocation(g_postShader, "uTime"), g_time);

    glBindVertexArray(g_screenVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);



    // 3) Вьюмодели (грабли/лопата) — поверх постобработки
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glClear(GL_DEPTH_BUFFER_BIT);   // очищаем глубину в ДЕФОЛТНОМ буфере,
    // чтобы инструменты не конфликтовали со сценой

    DrawRakeViewModel(proj, view);
    DrawShovelViewModel(proj, view);
    if (!g_cutAnim.active)
     DrawChainsawTestViewModel(proj, view);


    SwapBuffers(g_hDC);
}



// ===== ОКНО / WNDPROC =====

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (g_cuttingTree && g_lockPlayerDuringCut)
    {
        // не менять позицию/скорость игрока от ввода
        // (оставь гравитацию/прилипание к земле как у тебя устроено)
        return 0;
    }
    switch (msg) {      

    case VK_EXECUTE:
        g_running = false;
        PostQuitMessage(0);
        return 0;
    case WM_DESTROY:
        g_running = false;
        PostQuitMessage(0);
        return 0;
    case WM_SIZE:
        g_winWidth = LOWORD(lParam);
        g_winHeight = HIWORD(lParam);
        return 0;
    case WM_LBUTTONDOWN:
        CaptureMouse(true);
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            if (g_mouseCaptured) CaptureMouse(false);
            else { g_running = false; PostQuitMessage(0); }
        }
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

GLuint LoadTexture2D(const char* path)
{
    int w, h, chan;
    stbi_set_flip_vertically_on_load(1); // чтобы не было вверх ногами
    unsigned char* data = stbi_load(path, &w, &h, &chan, 4); // принудительно RGBA
    if (!data) {
        std::string msg = std::string("Failed to load texture: ") + path + "\n";
        OutputDebugStringA(msg.c_str());
        return 0;
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h,
        0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    // параметры фильтрации/повторения
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    stbi_image_free(data);
    return tex;
}



// ===== MAIN / WinMain =====

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int)
{
    srand((unsigned)time(nullptr));
    g_currentTool = TOOL_NONE;

    // Регистрируем класс окна
    WNDCLASS wc = {};
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"GLTerrainWindow";
    RegisterClass(&wc);

    // Создаём окно
    g_hWnd = CreateWindowW(
        L"GLTerrainWindow", L"OpenGL Terrain",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        g_winWidth, g_winHeight,
        nullptr, nullptr, hInst, nullptr);

    g_hDC = GetDC(g_hWnd);

    // Создаём контекст OpenGL
    g_hRC = CreateGLContext(g_hDC);
    wglMakeCurrent(g_hDC, g_hRC);

    InitSceneFBO(g_winWidth, g_winHeight);
    InitScreenQuad();

    // Загружаем шейдеры
    g_shader = CreateShaderProgram("terrain.vert", "terrain.frag");
    g_postShader = CreateShaderProgram("screen_post.vert", "screen_post.frag");
    g_cutShader = CreateShaderProgram("cut_anim.vert", "cut_anim.frag");

    // Загружаем heightmap (если есть) и строим террейн
    g_terrain.loadHeightmap("heightmap.png");   // можно закомментить, если файла нет
    g_terrain.build(1024, 1024.0f, 50.0f);       // (кол-во вершин, размер, макс. высота) РАЗМЕР КАРТЫ
    g_terrain.RebuildWaterMask();
    g_terrain.UploadWaterMaskFromTerrain();
    // грузим текстуру травы
    g_terrain.texture = LoadTexture2D("Detal2048tropic.png");  // или .jpg как назовёшь
    InitSkySphere();
    InitGrass();
    InitTreeObjects();
    InitRake();
    InitShovel();
    InitChainsawTest();
    InitWater();

    if (!g_treeCutAnimLoaded)
    {
        g_treeCutAnimLoaded = g_treeCutAnimModel.Load("test_cut.glb"); // путь поправь под свой assets
        if (!g_treeCutAnimLoaded)
            OutputDebugStringA("FAILED: test_cut.glb\n");
    }
    g_treeRemoved.assign(g_treeInstances.size(), false);

    // Настраиваем таймер
    QueryPerformanceFrequency(&g_freq);
    QueryPerformanceCounter(&g_prevTime);

    // Главный цикл
    MSG msg;
    while (g_running) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        double dt = double(now.QuadPart - g_prevTime.QuadPart) / double(g_freq.QuadPart);
        g_prevTime = now;

        UpdateCamera((float)dt);
        UpdateChainsawTest((float)dt);

        Render();
        g_time += dt;
    }

    // Чистим ресурсы
    wglMakeCurrent(nullptr, nullptr);
    if (g_hRC) wglDeleteContext(g_hRC);
    if (g_hDC) ReleaseDC(g_hWnd, g_hDC);

    return 0;
}




void InitGrass()
{
    // шейдеры травы
    g_grassShader = CreateShaderProgram("grass.vert", "grass.frag");
    g_grassTex = LoadTexture2D("grass_billboard.png"); // твоя текстура травы (RGBA)
    g_terrainGrassTex = LoadTexture2D("Detal2048tropic.png"); // или твоя трава
    g_terrainSandTex = LoadTexture2D("sandphoto.png");
    GLuint g_terrainGrassTex = 0;
    GLuint g_terrainSandTex = 0;
    if (!g_grassShader || !g_grassTex) return;

    // один вертикальный квад (две вершины по X, 0..1 по Y)
    float quad[] = {
        //  x     y     u     v
        -0.5f, 0.0f,  0.0f, 0.0f,
         0.5f, 0.0f,  1.0f, 0.0f,
        -0.5f, 1.0f,  0.0f, 1.0f,
         0.5f, 1.0f,  1.0f, 1.0f
    };

    if (!g_grassVAO) glGenVertexArrays(1, &g_grassVAO);
    if (!g_grassVBOQuad) glGenBuffers(1, &g_grassVBOQuad);
    if (!g_grassVBOInstances) glGenBuffers(1, &g_grassVBOInstances);

    glBindVertexArray(g_grassVAO);

    glBindBuffer(GL_ARRAY_BUFFER, g_grassVBOQuad);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

    // layout 0: позиция в кваде
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    // layout 1: UV
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    // ---------- генерируем инстансы ----------
    g_grassInstances.clear();
    float half = g_terrain.size * 0.5f;

    const int targetCount = 400000; // плотность травы
    int tries = 0;
    while ((int)g_grassInstances.size() < targetCount && tries < targetCount * 10)
    {
        ++tries;
        float rx = (float)rand() / RAND_MAX;
        float rz = (float)rand() / RAND_MAX;

        float x = -half + rx * g_terrain.size;
        float z = -half + rz * g_terrain.size;
        float y = g_terrain.getHeight(x, z);

        // фильтр по высоте (не на скалах)
        if (y < 3.0f || y > 35.0f)
            continue;

        GrassInstance gi;
        gi.pos = glm::vec3(x, y, z);
        gi.scale = 0.6f + ((float)rand() / RAND_MAX) * 0.8f;
        gi.alive = true;


        g_grassInstances.push_back(gi);
    }

    // грузим живые инстансы в буфер
    std::vector<glm::vec4> data;
    data.reserve(g_grassInstances.size());
    for (auto& gi : g_grassInstances)
        if (gi.alive)
            data.emplace_back(gi.pos, gi.scale);

    g_grassAliveCount = (GLsizei)data.size();

    int t = 0;

    glBindBuffer(GL_ARRAY_BUFFER, g_grassVBOInstances);
    glBufferData(GL_ARRAY_BUFFER,
        data.size() * sizeof(glm::vec4),
        data.data(),
        GL_DYNAMIC_DRAW);

    // layout 2: vec4 (pos.xyz + scale) как инстанс-атрибут
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void*)0);
    glVertexAttribDivisor(2, 1);

    glBindVertexArray(0);

}

void DrawGrass(const glm::mat4& proj, const glm::mat4& view)
{
    if (!g_grassVAO || !g_grassShader || !g_grassTex || g_grassAliveCount == 0)
        return;

    glUseProgram(g_grassShader);

    glUniformMatrix4fv(glGetUniformLocation(g_grassShader, "uProjection"), 1, GL_FALSE, &proj[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(g_grassShader, "uView"), 1, GL_FALSE, &view[0][0]);
    glUniform1f(glGetUniformLocation(g_grassShader, "uTime"), g_time);

    glm::vec3 camRight = g_cam.right;
    glm::vec3 camUp = g_cam.up;

    glUniform3fv(glGetUniformLocation(g_grassShader, "uCameraRight"), 1, &camRight[0]);
    glUniform3fv(glGetUniformLocation(g_grassShader, "uCameraUp"), 1, &camUp[0]);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_grassTex);
    glUniform1i(glGetUniformLocation(g_grassShader, "uGrassTex"), 0);

    // для травы: alpha cutout, обычно без блендинга достаточно
    glDisable(GL_CULL_FACE);

    glBindVertexArray(g_grassVAO);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, g_grassAliveCount);
    glBindVertexArray(0);

    glEnable(GL_CULL_FACE);
}

bool LoadOBJ(const char* path, Mesh& outMesh)
{
    std::ifstream file(path);
    if (!file) {
        std::string msg = std::string("Failed to open OBJ: ") + path + "\n";
        OutputDebugStringA(msg.c_str());
        return false;
    }

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texcoords;

    struct VertexKey {
        int vi, ti, ni;
        bool operator<(const VertexKey& o) const {
            if (vi != o.vi) return vi < o.vi;
            if (ti != o.ti) return ti < o.ti;
            return ni < o.ni;
        }
    };

    std::map<VertexKey, unsigned int> vertMap;
    std::vector<float> vertexData;
    std::vector<unsigned int> indices;

    std::string line;
    while (std::getline(file, line)) {
        if (line.size() < 2) continue;
        if (line[0] == 'v' && line[1] == ' ') {
            std::istringstream ss(line.substr(2));
            glm::vec3 v; ss >> v.x >> v.y >> v.z;
            positions.push_back(v);
        }
        else if (line[0] == 'v' && line[1] == 't') {
            std::istringstream ss(line.substr(3));
            glm::vec2 t; ss >> t.x >> t.y;
            texcoords.push_back(t);
        }
        else if (line[0] == 'v' && line[1] == 'n') {
            std::istringstream ss(line.substr(3));
            glm::vec3 n; ss >> n.x >> n.y >> n.z;
            normals.push_back(n);
        }
        else if (line[0] == 'f' && line[1] == ' ') {
            std::istringstream ss(line.substr(2));
            std::string comps[3];
            ss >> comps[0] >> comps[1] >> comps[2];
            for (int k = 0; k < 3; ++k) {
                VertexKey key = { -1,-1,-1 };
                std::string& c = comps[k];
                int s1 = (int)c.find('/');
                int s2 = (int)c.find('/', s1 + 1);

                key.vi = std::stoi(c.substr(0, s1)) - 1;
                if (s2 > s1 + 1)
                    key.ti = std::stoi(c.substr(s1 + 1, s2 - s1 - 1)) - 1;
                else
                    key.ti = -1;
                if (s2 != (int)std::string::npos)
                    key.ni = std::stoi(c.substr(s2 + 1)) - 1;

                auto it = vertMap.find(key);
                if (it == vertMap.end()) {
                    glm::vec3 pos = positions[key.vi];
                    glm::vec3 nor(0, 1, 0);
                    glm::vec2 uv(0, 0);

                    if (key.ni >= 0 && key.ni < (int)normals.size())
                        nor = normals[key.ni];
                    if (key.ti >= 0 && key.ti < (int)texcoords.size())
                        uv = texcoords[key.ti];

                    unsigned int newIndex = (unsigned int)(vertexData.size() / 8);
                    vertMap[key] = newIndex;

                    vertexData.push_back(pos.x);
                    vertexData.push_back(pos.y);
                    vertexData.push_back(pos.z);
                    vertexData.push_back(nor.x);
                    vertexData.push_back(nor.y);
                    vertexData.push_back(nor.z);
                    vertexData.push_back(uv.x);
                    vertexData.push_back(uv.y);

                    indices.push_back(newIndex);
                }
                else {
                    indices.push_back(it->second);
                }
            }
        }
    }

    if (vertexData.empty() || indices.empty()) {
        OutputDebugStringA("OBJ has no geometry or faces\n");
        return false;
    }

    if (!outMesh.vao) glGenVertexArrays(1, &outMesh.vao);
    if (!outMesh.vbo) glGenBuffers(1, &outMesh.vbo);
    if (!outMesh.ebo) glGenBuffers(1, &outMesh.ebo);

    glBindVertexArray(outMesh.vao);

    glBindBuffer(GL_ARRAY_BUFFER, outMesh.vbo);
    glBufferData(GL_ARRAY_BUFFER,
        vertexData.size() * sizeof(float),
        vertexData.data(),
        GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, outMesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        indices.size() * sizeof(unsigned int),
        indices.data(),
        GL_STATIC_DRAW);

    GLsizei stride = 8 * sizeof(float);
    glEnableVertexAttribArray(0); // pos
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1); // normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2); // uv
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));

    glBindVertexArray(0);

    outMesh.indexCount = (GLsizei)indices.size();
    return true;
}


void InitTreeObjects()
{
    if (!g_treeModel.Load("spruce2\\untitled.obj")) { // или "tree.obj"
        OutputDebugStringA("Failed to load tree model\n");
        return;
    }

    g_treeShader = CreateShaderProgram("tree_mesh.vert", "tree_mesh.frag");

    g_treeInstances.clear();

    const int treeCount = 2000;               // Сколько деревьев хотим
    float half = g_terrain.size * 0.5f;

    int tries = 0;
    while ((int)g_treeInstances.size() < treeCount && tries < treeCount * 10)
    {
        ++tries;

        float rx = (float)rand() / RAND_MAX;
        float rz = (float)rand() / RAND_MAX;

        float wx = -half + rx * g_terrain.size;
        float wz = -half + rz * g_terrain.size;
        float wy = g_terrain.getHeight(wx, wz);

        // ограничения по высоте, чтобы не росли на вершинах/в ямах
        if (wy < 3.0f || wy > 45.0f)
            continue;

        // наклон
        float eps = 1.0f;
        float hL = g_terrain.getHeight(wx - eps, wz);
        float hR = g_terrain.getHeight(wx + eps, wz);
        float hD = g_terrain.getHeight(wx, wz - eps);
        float hU = g_terrain.getHeight(wx, wz + eps);
        glm::vec3 n(hL - hR, 2.0f, hD - hU);
        n = glm::normalize(n);
        if (n.y < 0.9f) // только почти ровные места
            continue;

        TreeInstance inst;
        inst.pos = glm::vec3(wx, wy, wz);
        inst.scale = 2.5f + ((float)rand() / RAND_MAX) * 3.5f; // разные высоты
        inst.radius = 0.4f * inst.scale;                     // для коллизии

        g_treeInstances.push_back(inst);
    }

    g_treeInstanceCount = (GLsizei)g_treeInstances.size();

    std::string msg = "Placed " + std::to_string(g_treeInstanceCount) + " trees.\n";
    OutputDebugStringA(msg.c_str());

    // готовим матрицы инстансов
    std::vector<glm::mat4> models;
    models.reserve(g_treeInstanceCount);

    for (const auto& inst : g_treeInstances)
    {
        glm::mat4 m(1.0f);
        m = glm::translate(m, inst.pos);
        m = glm::scale(m, glm::vec3(inst.scale));
        models.push_back(m);
    }

    // VBO под матрицы
    if (!g_treeInstanceVBO)
        glGenBuffers(1, &g_treeInstanceVBO);

    glBindBuffer(GL_ARRAY_BUFFER, g_treeInstanceVBO);
    glBufferData(GL_ARRAY_BUFFER,
        sizeof(glm::mat4) * models.size(),
        models.data(),
        GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // привязываем этот VBO как инстанс-атрибут для всех мешей модели
    for (auto& mesh : g_treeModel.meshes)
    {
        glBindVertexArray(mesh.vao);
        glBindBuffer(GL_ARRAY_BUFFER, g_treeInstanceVBO);

        std::size_t vec4Size = sizeof(glm::vec4);

        // layout locations 3,4,5,6 под матрицу
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)0);

        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(1 * vec4Size));

        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(2 * vec4Size));

        glEnableVertexAttribArray(6);
        glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(3 * vec4Size));

        glVertexAttribDivisor(3, 1);
        glVertexAttribDivisor(4, 1);
        glVertexAttribDivisor(5, 1);
        glVertexAttribDivisor(6, 1);

        glBindVertexArray(0);
    }
}

void DrawTreeObjects(const glm::mat4& proj, const glm::mat4& view)
{
    if (g_treeModel.meshes.empty() || !g_treeShader || g_treeInstanceCount == 0)
        return;

    glUseProgram(g_treeShader);

    glUniformMatrix4fv(glGetUniformLocation(g_treeShader, "uProjection"), 1, GL_FALSE, &proj[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(g_treeShader, "uView"), 1, GL_FALSE, &view[0][0]);

    glm::vec3 lightDir = glm::normalize(glm::vec3(0.4f, 1.0f, 0.2f));
    glUniform3fv(glGetUniformLocation(g_treeShader, "uLightDir"), 1, &lightDir[0]);
    //Ограничить дальность леса
    glUniform3fv(glGetUniformLocation(g_treeShader, "uCamPos"), 1, &g_cam.pos[0]);
    glUniform1f(glGetUniformLocation(g_treeShader, "uMaxDist"), 250.0f); // по вкусу

    // мягкая альфа, двухсторонние листья
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);

    g_treeModel.DrawInstanced(g_treeShader, g_treeInstanceCount);

    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
}

void ResolveTreeCollisions(glm::vec3& pos)
{
    int i = -1;
    for (const auto& inst : g_treeInstances)
    {   
        i++;
        if (i < g_treeRemoved.size() && g_treeRemoved[i])
        {            
            continue;
        }
        
        glm::vec2 p(pos.x, pos.z);
        glm::vec2 c(inst.pos.x, inst.pos.z);
        glm::vec2 d = p - c;
        float dist = glm::length(d);
        float minDist = g_playerRadius + inst.radius;

        if (dist < minDist && dist > 0.0001f)
        {
            glm::vec2 dir = d / dist;
            float push = minDist - dist;
            p += dir * push;
            pos.x = p.x;
            pos.z = p.y;
        }
    }
}

bool IsTreeBlockingDig(const glm::vec3& center, float holeRadius)
{
    const float extra = 0.5f; // небольшой запас
    float r = holeRadius + extra;
    int i = -1;

    for (const auto& t : g_treeInstances)
    {
        i++;
        if (i < g_treeRemoved.size() && g_treeRemoved[i]) {

            continue;
        }
        
        float dx = t.pos.x - center.x;
        float dz = t.pos.z - center.z;
        float dist2 = dx * dx + dz * dz;
        float blockR = (t.radius > 0.0f ? t.radius : 0.8f); // примерный радиус ствола
        float limit = r + blockR;
        if (dist2 < limit * limit)
            return true; // слишком близко к дереву — не копаем
    }
    return false;
}

void RemoveGrassInRadius(const glm::vec3& center, float radius)
{
    float r2 = radius * radius;

    for (auto& g : g_grassInstances)
    {
        if (g.alive <= 0.0f)
            continue;

        float dx = g.pos.x - center.x;
        float dz = g.pos.z - center.z;
        float dist2 = dx * dx + dz * dz;

        if (dist2 < r2)
        {
            g.alive = 0.0f;
        }
    }

    // Обновляем instance-буфер под инстансинг
    // (подставь свою функцию/код, как ты уже делал для удаления травы граблями)
    std::vector<glm::vec4> instanceData;
    instanceData.reserve(g_grassInstances.size());
    for (auto& g : g_grassInstances)
    {
        if (g.alive > 0.0f)
            instanceData.push_back(glm::vec4(g.pos, 1.0f));
    }

    glBindBuffer(GL_ARRAY_BUFFER, g_grassVBOInstances);
    glBufferData(GL_ARRAY_BUFFER,
        instanceData.size() * sizeof(glm::vec4),
        instanceData.data(),
        GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    g_grassAliveCount = (int)instanceData.size();
}

void InitSceneFBO(int w, int h)
{
    if (g_sceneFBO) {
        glDeleteFramebuffers(1, &g_sceneFBO);
        glDeleteTextures(1, &g_sceneColorTex);
        glDeleteRenderbuffers(1, &g_sceneDepthRBO);
    }

    glGenFramebuffers(1, &g_sceneFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, g_sceneFBO);

    // цвет
    glGenTextures(1, &g_sceneColorTex);
    glBindTexture(GL_TEXTURE_2D, g_sceneColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D, g_sceneColorTex, 0);

    // depth
    glGenRenderbuffers(1, &g_sceneDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, g_sceneDepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
        GL_RENDERBUFFER, g_sceneDepthRBO);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        OutputDebugStringA("Scene FBO NOT complete!\n");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void InitScreenQuad()
{
    float verts[] = {
        // pos   // uv
        -1.f, -1.f,  0.f, 0.f,
         1.f, -1.f,  1.f, 0.f,
        -1.f,  1.f,  0.f, 1.f,
         1.f,  1.f,  1.f, 1.f,
    };

    glGenVertexArrays(1, &g_screenVAO);
    glGenBuffers(1, &g_screenVBO);
    glBindVertexArray(g_screenVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_screenVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);
}

int FindNearestTree(const glm::vec3& playerPosXZ, float maxDist,
    const std::vector<glm::vec3>& treePositions)
{
    int best = -1;
    float bestD2 = maxDist * maxDist;

    for (int i = 0; i < (int)treePositions.size(); ++i)
    {
        glm::vec3 d = treePositions[i] - playerPosXZ;
        d.y = 0.0f;
        float d2 = glm::dot(d, d);
        if (d2 < bestD2)
        {
            bestD2 = d2;
            best = i;
        }
    }
    return best;
}

static int FindNearestTreeIndexXZ(const glm::vec3& p, float maxDist)
{
    int best = -1;
    float best2 = maxDist * maxDist;

    for (int i = 0; i < (int)g_treeInstances.size(); ++i)
    {
        const auto& t = g_treeInstances[i];
        float dx = t.pos.x - p.x;
        float dz = t.pos.z - p.z;
        float d2 = dx * dx + dz * dz;

        if (d2 < best2)
        {
            best2 = d2;
            best = i;
        }
    }
    return best;
}

static void SnapPlayerToTreeFront(int treeIdx)
{
    const auto& t = g_treeInstances[treeIdx];

    // ВАЖНО: "ЮГ" у тебя может быть +Z или -Z — если окажется с другой стороны, просто поменяй знак.
    glm::vec3 south(0.0f, 0.0f, 1.0f);

    float trunkR = (t.radius > 0.0f ? t.radius : 0.8f);
    float standDist = trunkR + 0.9f; // подберёшь, но это уже нормально “впритык”

    glm::vec3 target = glm::vec3(t.pos.x, 0.0f, t.pos.z) + south * standDist;

    float ground = g_terrain.getHeight(target.x, target.z);
    g_cam.pos = glm::vec3(target.x, ground + g_eyeHeight, target.z);

    // Повернуть камеру лицом к дереву
    glm::vec3 toTree = glm::vec3(t.pos.x - g_cam.pos.x, 0.0f, t.pos.z - g_cam.pos.z);
    if (glm::length(toTree) > 0.0001f)
    {
        toTree = glm::normalize(toTree);
        g_cam.yaw = glm::degrees(atan2(toTree.z, toTree.x));
    }
    g_cam.pitch = 0.0f;
    g_cam.updateVectors();
}

void RebuildTreeInstanceBuffer()
{
    std::vector<glm::mat4> mats;
    mats.reserve(g_treeInstances.size());

    for (size_t i = 0; i < g_treeInstances.size(); ++i)
    {
        if (i < g_treeRemoved.size() && g_treeRemoved[i]) {

            continue;
        }

        const auto& t = g_treeInstances[i];

        glm::mat4 M(1.0f);
        M = glm::translate(M, t.pos);
        M = glm::scale(M, glm::vec3(t.scale));
        mats.push_back(M);
    }

    g_treeInstanceCount = (GLsizei)mats.size();

    glBindBuffer(GL_ARRAY_BUFFER, g_treeInstanceVBO);
    glBufferData(GL_ARRAY_BUFFER,
        mats.size() * sizeof(glm::mat4),
        mats.empty() ? nullptr : mats.data(),
        GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void TryStartCut()
{
    if (g_cuttingTree) return;

    // Игрок
    glm::vec3 p = g_cam.pos;
    p.y = 0.0f;

    // 1) ищем дерево рядом (радиус подстрой)
    int idx = FindNearestTreeIndexXZ(p, 5.0f);
    if (idx < 0) return;
    if (g_treeRemoved[idx]) return;
    g_treeRemoved[idx] = true;
    RebuildTreeInstanceBuffer();

    // 3) заспавнить анимацию на позиции дерева
    StartCutAnimAt(g_treeInstances[idx].pos);

    const auto& t = g_treeInstances[idx];

    // 2) доп.порог: чтобы не “пилило” дерево через полкарты
    float trunkR = (t.radius > 0.0f ? t.radius : 0.8f);
    float interact = trunkR + 1.3f;

    float dx = t.pos.x - p.x;
    float dz = t.pos.z - p.z;
    if (dx * dx + dz * dz > interact * interact)
        return;

    // 3) снеп
    SnapPlayerToTreeFront(idx);

    // 4) запуск “анимации распила”
    g_targetTreeIndex = idx;
    g_cuttingTree = true;
    g_cutTime = 0.0f;
}