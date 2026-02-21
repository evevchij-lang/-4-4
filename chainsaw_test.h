#pragma once

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <cctype>

#include "stb_image.h"   // БЕЗ STB_IMAGE_IMPLEMENTATION !

extern GLuint CreateShaderProgram(const char* vsPath, const char* fsPath);
void StartCutAnimAt(const glm::vec3& worldPos);
extern int g_currentTool;

// === Runtime tuning (позиция/поворот оружия) ===
static glm::vec3 g_toolOffset = glm::vec3(0.55f, -1.45f, -3.2f);
static glm::vec3 g_toolRotDeg = glm::vec3(-10.0f, 12.0f, 0.0f);
static float     g_toolScale = 0.55f;

static bool g_toolDebugPrint = false;

// --- chainsaw cut state ---
bool  g_cuttingTree = false;
float g_cutTime = 0.0f;
const float g_cutDuration = 0.55f;   // длительность одного "пропила"
int   g_targetTreeIndex = -1;        // индекс дерева, которое пилим
bool  g_lockPlayerDuringCut = true;  // чтобы не двигался во время анимации

inline void DebugMoveTool(float dt)
{
    const float moveSpeed = 2.5f * dt;
    const float rotSpeed = 90.0f * dt;
    const float scaleSpd = 1.0f * dt;

    auto down = [](int vkey) { return (GetAsyncKeyState(vkey) & 0x8000) != 0; };

    // === ПЕРЕМЕЩЕНИЕ ===
    if (down(VK_LEFT))  g_toolOffset.x -= moveSpeed;
    if (down(VK_RIGHT)) g_toolOffset.x += moveSpeed;

    if (down(VK_UP))    g_toolOffset.y += moveSpeed;
    if (down(VK_DOWN))  g_toolOffset.y -= moveSpeed;

    if (down(VK_PRIOR)) g_toolOffset.z += moveSpeed; // PgUp
    if (down(VK_NEXT))  g_toolOffset.z -= moveSpeed; // PgDn

    // === ВРАЩЕНИЕ ===
    if (down(VK_NUMPAD4)) g_toolRotDeg.y -= rotSpeed;
    if (down(VK_NUMPAD6)) g_toolRotDeg.y += rotSpeed;

    if (down(VK_NUMPAD8)) g_toolRotDeg.x -= rotSpeed;
    if (down(VK_NUMPAD2)) g_toolRotDeg.x += rotSpeed;

    if (down(VK_NUMPAD7)) g_toolRotDeg.z -= rotSpeed;
    if (down(VK_NUMPAD9)) g_toolRotDeg.z += rotSpeed;

    // === SCALE ===
    if (down(VK_ADD))      g_toolScale += scaleSpd;
    if (down(VK_SUBTRACT)) g_toolScale -= scaleSpd;

    // === PRINT В DEBUG (нажми NUMPAD5) ===
    if (GetAsyncKeyState(VK_NUMPAD5) & 1)
    {
        char buf[512];
        sprintf_s(buf,
            "\nOFFSET  = %.4f, %.4f, %.4f\nROT     = %.4f, %.4f, %.4f\nSCALE   = %.4f\n",
            g_toolOffset.x, g_toolOffset.y, g_toolOffset.z,
            g_toolRotDeg.x, g_toolRotDeg.y, g_toolRotDeg.z,
            g_toolScale);

        OutputDebugStringA(buf);
    }
}



// ====== helpers ======
static glm::mat4 CST_AiToGlm(const aiMatrix4x4& m)
{
    glm::mat4 r;
    r[0][0] = m.a1; r[1][0] = m.a2; r[2][0] = m.a3; r[3][0] = m.a4;
    r[0][1] = m.b1; r[1][1] = m.b2; r[2][1] = m.b3; r[3][1] = m.b4;
    r[0][2] = m.c1; r[1][2] = m.c2; r[2][2] = m.c3; r[3][2] = m.c4;
    r[0][3] = m.d1; r[1][3] = m.d2; r[2][3] = m.d3; r[3][3] = m.d4;
    return r;
}

static std::string CST_ToLower(std::string s)
{
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

static GLuint CST_TexFromMemory(const unsigned char* bytes, int len)
{
    int w = 0, h = 0, comp = 0;
    stbi_set_flip_vertically_on_load(1);
    unsigned char* data = stbi_load_from_memory(bytes, len, &w, &h, &comp, 4);
    if (!data) return 0;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    stbi_image_free(data);
    return tex;
}

static GLuint CST_LoadAnyBaseColor(const aiScene* scene, const aiMaterial* mat)
{
    if (!scene || !mat) return 0;

    aiString path;

    // glTF BaseColor
    if (mat->GetTexture(aiTextureType_BASE_COLOR, 0, &path) == AI_SUCCESS ||
        mat->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS)
    {
        const char* p = path.C_Str();
        if (!p || !p[0]) return 0;

        // embedded "*0"
        if (p[0] == '*')
        {
            int idx = atoi(p + 1);
            if (idx >= 0 && idx < (int)scene->mNumTextures)
            {
                const aiTexture* t = scene->mTextures[idx];
                if (!t) return 0;

                // compressed image case: mHeight==0, mWidth = byte size
                if (t->mHeight == 0 && t->mWidth > 0)
                    return CST_TexFromMemory((const unsigned char*)t->pcData, (int)t->mWidth);

                // raw pixels case (rare)
                // pcData is aiTexel*
                std::vector<unsigned char> bytes;
                bytes.resize((size_t)t->mWidth * (size_t)t->mHeight * 4);
                for (unsigned y = 0; y < t->mHeight; ++y)
                    for (unsigned x = 0; x < t->mWidth; ++x)
                    {
                        const aiTexel& px = t->pcData[y * t->mWidth + x];
                        size_t o = ((size_t)y * t->mWidth + x) * 4;
                        bytes[o + 0] = px.r;
                        bytes[o + 1] = px.g;
                        bytes[o + 2] = px.b;
                        bytes[o + 3] = px.a;
                    }
                return CST_TexFromMemory(bytes.data(), (int)bytes.size());
            }
            return 0;
        }

        // external file (если вдруг будет)
        // тут можешь дернуть свой LoadTexture2D если есть
        return 0;
    }

    return 0;
}

// ====== vertex ======
struct CST_Vertex
{
    glm::vec3 pos;
    glm::vec3 nrm;
    glm::vec2 uv;
};

// ====== mesh ======
struct CST_Mesh
{
    GLuint vao = 0, vbo = 0, ebo = 0;
    GLsizei indexCount = 0;
    glm::vec3 localCenter = glm::vec3(0.0f);
    std::string meshName;
    std::vector<CST_Vertex> baseVerts; // базовые вершины
    std::vector<CST_Vertex> workVerts; // рабочие вершины для morph
    std::vector<std::vector<glm::vec3>> morphPosDeltas; // [morphTarget][vertex] delta
    std::vector<std::vector<glm::vec3>> morphNrmDeltas; // [target][v] 
    bool isChain = false;

    std::string nodeName;
    glm::mat4 bindNode = glm::mat4(1.0f);

    GLuint baseTex = 0;

    void Draw() const
    {
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
};

struct ChainsawTest
{
    Assimp::Importer importer;
    const aiScene* scene = nullptr;

    std::vector<CST_Mesh> meshes;

    // для “процедурной анимации”
    float t = 0.0f;

    bool Load(const char* path)
    {
        scene = importer.ReadFile(path,
            aiProcess_Triangulate |
            aiProcess_GenSmoothNormals |
            aiProcess_FlipUVs);

        if (!scene || !scene->mRootNode) return false;

        meshes.clear();

        std::function<void(aiNode*, aiMatrix4x4)> walk;
        walk = [&](aiNode* node, aiMatrix4x4 parent)
            {
                aiMatrix4x4 global = parent * node->mTransformation;

                for (unsigned mi = 0; mi < node->mNumMeshes; ++mi)
                {
                    aiMesh* m = scene->mMeshes[node->mMeshes[mi]];
                    if (!m) continue;

                    std::vector<CST_Vertex> verts;
                    std::vector<unsigned> idx;

                    verts.resize(m->mNumVertices);
                    glm::vec3 bmin(1e9f), bmax(-1e9f);
                    for (unsigned k = 0; k < m->mNumVertices; ++k)
                    {

                        const aiVector3D& p = m->mVertices[k];
                        aiVector3D n = m->HasNormals() ? m->mNormals[k] : aiVector3D(0, 1, 0);

                        aiVector3D uv(0, 0, 0);
                        if (m->HasTextureCoords(0) && m->mTextureCoords[0])
                            uv = m->mTextureCoords[0][k];

                        CST_Vertex v;
                        v.pos = glm::vec3(p.x, p.y, p.z);
                        
                        v.nrm = glm::vec3(n.x, n.y, n.z);
                        v.uv = glm::vec2(uv.x, uv.y);

                        bmin = glm::min(bmin, v.pos);
                        bmax = glm::max(bmax, v.pos);

                        verts[k] = v;
                    }

                    idx.reserve(m->mNumFaces * 3);
                    for (unsigned f = 0; f < m->mNumFaces; ++f)
                        for (unsigned j = 0; j < m->mFaces[f].mNumIndices; ++j)
                            idx.push_back(m->mFaces[f].mIndices[j]);

                    CST_Mesh out;
                    out.indexCount = (GLsizei)idx.size();
                    out.nodeName = node->mName.C_Str();
                    out.bindNode = CST_AiToGlm(global);
                    out.localCenter = (bmin + bmax) * 0.5f;

                    out.meshName = m->mName.C_Str(); // ВАЖНО: имя МЕША (для morph channel)
                    out.baseVerts = verts;            // базовые вершины
                    out.workVerts = verts;            // рабочие вершины (будем менять позиции)

                    // ===== morph targets (если есть) =====
                    if (m->mNumAnimMeshes > 0)
                    {
                        out.morphPosDeltas.resize(m->mNumAnimMeshes);
                        out.morphNrmDeltas.resize(m->mNumAnimMeshes);

                        for (unsigned ti = 0; ti < m->mNumAnimMeshes; ++ti)
                        {
                            aiAnimMesh* am = m->mAnimMeshes[ti];

                            out.morphPosDeltas[ti].resize(m->mNumVertices, glm::vec3(0));
                            out.morphNrmDeltas[ti].resize(m->mNumVertices, glm::vec3(0));

                            bool hasMorphNormals = (am->mNormals != nullptr) && m->HasNormals();

                            for (unsigned vi = 0; vi < m->mNumVertices; ++vi)
                            {
                                // pos delta
                                const aiVector3D& tp = am->mVertices[vi];
                                const aiVector3D& bp = m->mVertices[vi];
                                out.morphPosDeltas[ti][vi] = glm::vec3(tp.x - bp.x, tp.y - bp.y, tp.z - bp.z);

                                // normal delta (если есть)
                                if (hasMorphNormals)
                                {
                                    const aiVector3D& tn = am->mNormals[vi];
                                    const aiVector3D& bn = m->mNormals[vi];
                                    out.morphNrmDeltas[ti][vi] = glm::vec3(tn.x - bn.x, tn.y - bn.y, tn.z - bn.z);
                                }
                            }
                        }

                    }

                    out.isChain = (m->mNumAnimMeshes > 0);

                    // texture from material (embedded)
                    int matIndex = (int)m->mMaterialIndex;
                    if (matIndex >= 0 && matIndex < (int)scene->mNumMaterials)
                        out.baseTex = CST_LoadAnyBaseColor(scene, scene->mMaterials[matIndex]);
                    
                    glGenVertexArrays(1, &out.vao);
                    glGenBuffers(1, &out.vbo);
                    glGenBuffers(1, &out.ebo);

                    glBindVertexArray(out.vao);

                    glBindBuffer(GL_ARRAY_BUFFER, out.vbo);
                    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(CST_Vertex), verts.data(), GL_DYNAMIC_DRAW);


                    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, out.ebo);
                    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned), idx.data(), GL_STATIC_DRAW);

                    GLsizei stride = sizeof(CST_Vertex);

                    glEnableVertexAttribArray(0);
                    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(CST_Vertex, pos));

                    glEnableVertexAttribArray(1);
                    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(CST_Vertex, nrm));

                    glEnableVertexAttribArray(2);
                    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(CST_Vertex, uv));

                    glBindVertexArray(0);

                    meshes.push_back(out);
                }

                for (unsigned c = 0; c < node->mNumChildren; ++c)
                    walk(node->mChildren[c], global);
            };

        walk(scene->mRootNode, aiMatrix4x4());
        return !meshes.empty();
    }

    void Update(float dt)
    {

        if (!scene || !scene->HasAnimations()) return;

        t += dt;

        aiAnimation* anim = scene->mAnimations[0];
        double tps = anim->mTicksPerSecond != 0.0 ? anim->mTicksPerSecond : 25.0;
        double time = fmod(t * tps, anim->mDuration);

        // === 1) если в анимации нет morph-каналов — выходим
        if (anim->mNumMorphMeshChannels == 0) return;

        // === 2) пробегаем по morph-каналам (в твоём файле он 1 и он для цепи)
        for (unsigned mc = 0; mc < anim->mNumMorphMeshChannels; ++mc)
        {
            aiMeshMorphAnim* morph = anim->mMorphMeshChannels[mc];
            if (!morph || morph->mNumKeys == 0) continue;

            // morph->mName обычно совпадает с именем меша (у тебя "Cube.002_0")
            // найдём наш CST_Mesh по имени (ты сохранишь meshName при Load)
            CST_Mesh* dst = nullptr;
            for (auto& m : meshes)
            {
                if (m.meshName == morph->mName.C_Str())
                {
                    dst = &m;
                    break;
                }
            }
            if (!dst) continue;

            // Если у меша нет морфов — нечего обновлять
            if (dst->morphPosDeltas.empty()) continue;

            // === 3) найти пару ключей вокруг time и интерполировать веса
            unsigned k0 = 0;
            while (k0 + 1 < morph->mNumKeys && time >= morph->mKeys[k0 + 1].mTime)
                ++k0;

            unsigned k1 = (k0 + 1 < morph->mNumKeys) ? (k0 + 1) : k0;

            double t0 = morph->mKeys[k0].mTime;
            double t1 = morph->mKeys[k1].mTime;
            float f = 0.0f;
            if (k0 != k1 && (t1 - t0) > 1e-8)
                f = (float)((time - t0) / (t1 - t0));

            // В Assimp morph key хранит:
            // - mNumValuesAndWeights
            // - массив mValues (индексы morph targets)
            // - массив mWeights (веса)
            // (в glTF это как раз "weights")
            std::vector<float> w(dst->morphPosDeltas.size(), 0.0f);

            auto applyKey = [&](const aiMeshMorphKey& key, float kf)
                {
                    for (unsigned i = 0; i < key.mNumValuesAndWeights; ++i)
                    {
                        unsigned targetIndex = key.mValues[i];
                        if (targetIndex < w.size())
                            w[targetIndex] += (float)key.mWeights[i] * kf;
                    }
                };

            if (k0 == k1)
            {
                applyKey(morph->mKeys[k0], 1.0f);
            }
            else
            {
                applyKey(morph->mKeys[k0], 1.0f - f);
                applyKey(morph->mKeys[k1], f);
            }

            // === 4) пересчитать позиции вершин: base + сумма(weight * delta)
            // (CPU morphing)
            dst->workVerts = dst->baseVerts;

            for (size_t vi = 0; vi < dst->workVerts.size(); ++vi)
            {
                glm::vec3 p = dst->baseVerts[vi].pos;
                glm::vec3 n = dst->baseVerts[vi].nrm;

                for (size_t ti = 0; ti < w.size(); ++ti)
                {
                    float ww = w[ti];
                    if (ww == 0.0f) continue;

                    p += dst->morphPosDeltas[ti][vi] * ww;

                    if (!dst->morphNrmDeltas.empty())
                        n += dst->morphNrmDeltas[ti][vi] * ww;
                }

                dst->workVerts[vi].pos = p;
                dst->workVerts[vi].nrm = glm::normalize(n);
            }

            // === 5) залить обновлённые вершины в VBO
            glBindBuffer(GL_ARRAY_BUFFER, dst->vbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0,
                (GLsizeiptr)(dst->workVerts.size() * sizeof(CST_Vertex)),
                dst->workVerts.data());
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
    }



};

inline void UpdateCut(float dt)
{
    if (!g_cuttingTree) return;
    g_cutTime += dt;
    if (g_cutTime >= g_cutDuration)
    {
        //вот тут удаляем дерево
        if (g_targetTreeIndex >= 0)
        {
            g_treeRemoved[g_targetTreeIndex] = true;

            StartCutAnimAt(g_treeInstances[g_targetTreeIndex].pos);
        }

        g_cuttingTree = false;
        g_cutTime = 0.0f;
        g_targetTreeIndex = -1;
    }
}

// ===== globals =====
static ChainsawTest g_chainsawTest;
static GLuint g_chainsawShader = 0;

inline void InitChainsawTest()
{
    g_chainsawShader = CreateShaderProgram("chainsaw_test.vert", "chainsaw_test.frag");

    if (!g_chainsawTest.Load("chainsaw.glb"))
        OutputDebugStringA("ChainsawTest: load failed\n");
    else
        OutputDebugStringA("ChainsawTest: load OK\n");
}

inline void UpdateChainsawTest(float dt)
{
    DebugMoveTool(dt);
    if (g_currentTool != 3) return;
    g_chainsawTest.Update(dt);
}

inline void DrawChainsawTestViewModel(const glm::mat4& proj, const glm::mat4& view)
{
    if (g_currentTool != 3) return;
    if (!g_chainsawShader || g_chainsawTest.meshes.empty()) return;

    glUseProgram(g_chainsawShader);

    glUniformMatrix4fv(glGetUniformLocation(g_chainsawShader, "uProjection"), 1, GL_FALSE, &proj[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(g_chainsawShader, "uView"), 1, GL_FALSE, &view[0][0]);

    glm::vec3 lightDir = glm::normalize(glm::vec3(0.4f, 1.0f, 0.2f));
    glUniform3fv(glGetUniformLocation(g_chainsawShader, "uLightDir"), 1, &lightDir[0]);

    // viewmodel transform + sway
    glm::mat4 local(1.0f);

    float swayX = sinf(g_chainsawTest.t * 2.0f) * 0.03f;
    float swayY = cosf(g_chainsawTest.t * 2.3f) * 0.02f;

    local = glm::translate(local, g_toolOffset + glm::vec3(1.7799, -1.5826, -2.4089));

    local = glm::rotate(local, glm::radians((float)207.6271), glm::vec3(1, 0, 0));
    local = glm::rotate(local, glm::radians((float)68.5759), glm::vec3(0, 1, 0));
    local = glm::rotate(local, glm::radians((float)9.0054), glm::vec3(0, 0, 1));

    local = glm::scale(local, glm::vec3(0.94));

    //local = glm::translate(local, g_toolOffset + glm::vec3(swayX, swayY, 0.0f));

    //local = glm::rotate(local, glm::radians(g_toolRotDeg.x), glm::vec3(1, 0, 0));
    //local = glm::rotate(local, glm::radians(g_toolRotDeg.y), glm::vec3(0, 1, 0));
    //local = glm::rotate(local, glm::radians(g_toolRotDeg.z), glm::vec3(0, 0, 1));

    //local = glm::scale(local, glm::vec3(g_toolScale));


    //OFFSET = 1.7799, -1.5826, -2.4089
    //    ROT = 207.6271, 68.5759, 9.0054
    //    SCALE = 0.5745


    if (g_cuttingTree)
    {
        float t = glm::clamp(g_cutTime / g_cutDuration, 0.0f, 1.0f);

        // 0..1..0 (вперёд -> назад)
        float pass = sin(t * 3.1415926f);

        // ВАЖНО: знак зависит от того, куда у вас "вперёд" у вьюмодели.
        // Обычно Z отрицательный = "в экран". Подберите знак.
        float zPush = -0.35f * pass;  // глубина "врезания"
        float yBob = 0.04f * sin(t * 6.28318f); // легкая дрожь

        local = glm::translate(local, glm::vec3(0.0f, yBob, zPush));

        // небольшой поворот вокруг Y чтобы "вести" пилу
        float yawWobble = glm::radians(6.0f) * (pass - 0.5f);
        local = glm::rotate(local, yawWobble, glm::vec3(0, 1, 0));
    }


    glm::mat4 camMatrix = glm::inverse(view);
    glm::mat4 model = camMatrix * local;

    glUniformMatrix4fv(glGetUniformLocation(g_chainsawShader, "uModel"), 1, GL_FALSE, &model[0][0]);

    // viewmodel depth
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glDepthRange(0.0, 0.1);

    GLint locNode = glGetUniformLocation(g_chainsawShader, "uNode");
    GLint locHasTex = glGetUniformLocation(g_chainsawShader, "uHasTex");
    GLint locTex = glGetUniformLocation(g_chainsawShader, "uTex");
    GLint locIsChain = glGetUniformLocation(g_chainsawShader, "uIsChain");
    GLint locTime = glGetUniformLocation(g_chainsawShader, "uTime");
    GLint locSpeed = glGetUniformLocation(g_chainsawShader, "uChainSpeed");

    for (const auto& mesh : g_chainsawTest.meshes)
    {
        // texture
        if (mesh.baseTex)
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, mesh.baseTex);
            if (locTex >= 0) glUniform1i(locTex, 0);
            if (locHasTex >= 0) glUniform1i(locHasTex, 1);
        }
        else
        {
            if (locHasTex >= 0) glUniform1i(locHasTex, 0);
        }

        // node
        glm::mat4 nodeM = mesh.bindNode;
        if (locNode >= 0)
            glUniformMatrix4fv(locNode, 1, GL_FALSE, &nodeM[0][0]);

        // chain flag + time
        std::string low = CST_ToLower(mesh.nodeName);
        int isChain = mesh.isChain ? 1 : 0;

        if (locIsChain >= 0) glUniform1i(locIsChain, isChain);
        if (locTime >= 0)    glUniform1f(locTime, g_chainsawTest.t);
        if (locSpeed >= 0)   glUniform1f(locSpeed, 6.0f);  // скорость "бега" UV, подгони

        mesh.Draw();
    }

    glDepthRange(0.0, 1.0);
}

void StartCutAnimAt(const glm::vec3& worldPos)
{
    if (!g_treeCutAnimLoaded) return;

    g_cutAnim.active = true;
    g_cutAnim.pos = worldPos;
    g_cutAnim.t = 0.0f;

    // дефолтные подгонки (потом крутишь кнопками)
    g_cutAnim.rot = glm::vec3(0.0f, 0.0f, 0.0f);
    g_cutAnim.scale = 1.0f;

    // если в твоём Model есть сброс анимации — вызови
    // g_treeCutAnimModel.ResetAnimation();
}

void UpdateCutAnim(float dt)
{
    if (!g_cutAnim.active) return;

    g_cutAnim.t += dt;

    // прогоняем анимацию в Model
    // ВАЖНО: тут зависит от modelwork.h.
    // Если у тебя UpdateAnimation(dt) крутит внутренний time сам — просто вызывай:
    g_treeCutAnimModel.UpdateAnimation(dt);

    // Если нужно вручную остановить по времени:
    if (g_cutAnim.t >= g_cutAnim.duration)
    {
        g_cutAnim.active = false; // проиграли и исчезли
    }
}

void DrawCutAnim(const glm::mat4& proj, const glm::mat4& view)
{
    if (!g_cutAnim.active) return;

    glm::mat4 M(1.0f);
    M = glm::translate(M, g_cutAnim.pos);
    M = glm::rotate(M, g_cutAnim.rot.y, glm::vec3(0, 1, 0));
    M = glm::rotate(M, g_cutAnim.rot.x, glm::vec3(1, 0, 0));
    M = glm::rotate(M, g_cutAnim.rot.z, glm::vec3(0, 0, 1));
    M = glm::scale(M, glm::vec3(g_cutAnim.scale));

    // у тебя уже есть общий Draw для Model
    // обычно это что-то типа: g_treeCutAnimModel.Draw(shader, M, proj, view)
    // Но в твоём проекте Draw(shader) берёт uModel из uniform.
    // Поэтому: выставь uProjection/uView/uModel и вызови Draw как в остальных моделях.

    glUseProgram(g_treeShader); // или какой у тебя шейдер для моделей
    glUniformMatrix4fv(glGetUniformLocation(g_treeShader, "uProjection"), 1, GL_FALSE, &proj[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(g_treeShader, "uView"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(g_treeShader, "uModel"), 1, GL_FALSE, &M[0][0]);

    g_treeCutAnimModel.Draw(g_treeShader);
}