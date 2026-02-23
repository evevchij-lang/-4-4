#pragma once

// ==== STL ====

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

#include <cmath>
#include <cstdlib>

// ==== OpenGL ====
#include <GL/gl.h> // или твой glew/glad include (оставь как у тебя в проекте)

// ==== GLM ====
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
//#include <glm/gtx/quaternion.hpp>   // slerp
#include <glm/gtc/matrix_transform.hpp>

// ==== Assimp ====
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/material.h>

// ==== stb_image / твои хелперы ====
/*
  Тут предполагается, что в проекте уже есть:
  - LoadTexture2D(...)
  - stbi_load / stbi_load_from_memory / stbi_image_free
  - OutputDebugStringA
*/

// =======================================================
// TEXTURES
// =======================================================

inline glm::mat4 AiToGlm(const aiMatrix4x4& m)
{
  /*  return glm::mat4(
        m.a1, m.a2, m.a3, m.a4,
        m.b1, m.b2, m.b3, m.b4,
        m.c1, m.c2, m.c3, m.c4,
        m.d1, m.d2, m.d3, m.d4
    );*/
    glm::mat4 r;
    // GLM: r[col][row]
    r[0][0] = m.a1; r[0][1] = m.b1; r[0][2] = m.c1; r[0][3] = m.d1;
    r[1][0] = m.a2; r[1][1] = m.b2; r[1][2] = m.c2; r[1][3] = m.d2;
    r[2][0] = m.a3; r[2][1] = m.b3; r[2][2] = m.c3; r[2][3] = m.d3;
    r[3][0] = m.a4; r[3][1] = m.b4; r[3][2] = m.c4; r[3][3] = m.d4;
    return r;
}

struct TextureInfo {
    GLuint id = 0;
    std::string type; // "texture_diffuse"
    std::string path;
};

static TextureInfo LoadTextureFromFile(const std::string& filename,
    const std::string& directory,
    const std::string& typeName,
    std::vector<TextureInfo>& loaded)
{
    std::string fullPath = directory + "/" + filename;

    for (auto& t : loaded) {
        if (t.path == fullPath && t.type == typeName)
            return t;
    }

    TextureInfo tex;
    tex.id = LoadTexture2D(fullPath.c_str());
    tex.type = typeName;
    tex.path = fullPath;

    loaded.push_back(tex);
    return tex;
}

static TextureInfo LoadTexture_Assimp(
    const aiScene* scene,
    const aiMaterial* material,
    aiTextureType type,
    unsigned int index,
    const std::string& directory,
    const std::string& typeName,
    std::vector<TextureInfo>& loaded
)
{
    aiString str;
    if (material->GetTexture(type, index, &str) != AI_SUCCESS)
        return { 0, "", "" };

    std::string texPath = str.C_Str();

    // cache by (path,type)
    for (const auto& t : loaded) {
        if (t.path == texPath && t.type == typeName)
            return t;
    }

    TextureInfo tex{};
    tex.type = typeName;
    tex.path = texPath;

    // Embedded "*N"
    if (!texPath.empty() && texPath[0] == '*')
    {
        int texIndex = std::atoi(texPath.c_str() + 1);
        if (texIndex >= 0 && texIndex < (int)scene->mNumTextures)
        {
            const aiTexture* aitex = scene->mTextures[texIndex];

            GLuint id = 0;
            glGenTextures(1, &id);
            glBindTexture(GL_TEXTURE_2D, id);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            int w = 0, h = 0, ch = 0;

            if (aitex->mHeight == 0)
            {
                // compressed (PNG/JPEG) in memory
                const unsigned char* data = reinterpret_cast<const unsigned char*>(aitex->pcData);
                int size = (int)aitex->mWidth;

                unsigned char* img = stbi_load_from_memory(data, size, &w, &h, &ch, 4);
                if (img)
                {
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
                    glGenerateMipmap(GL_TEXTURE_2D);
                    stbi_image_free(img);
                    tex.id = id;
                }
                else
                {
                    glDeleteTextures(1, &id);
                }
            }
            else
            {
                // raw BGRA8888
                w = (int)aitex->mWidth;
                h = (int)aitex->mHeight;

                std::vector<unsigned char> pixels(w * h * 4);
                const unsigned char* src = reinterpret_cast<const unsigned char*>(aitex->pcData);

                for (int i = 0; i < w * h; ++i)
                {
                    pixels[i * 4 + 0] = src[i * 4 + 2];
                    pixels[i * 4 + 1] = src[i * 4 + 1];
                    pixels[i * 4 + 2] = src[i * 4 + 0];
                    pixels[i * 4 + 3] = src[i * 4 + 3];
                }

                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                    GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                glGenerateMipmap(GL_TEXTURE_2D);

                tex.id = id;
            }

            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }
    else
    {
        // external file
        std::string filename = texPath;
        if (!directory.empty())
            filename = directory + "/" + filename;

        int w, h, ch;
        unsigned char* data = stbi_load(filename.c_str(), &w, &h, &ch, 4);
        if (data)
        {
            GLuint id;
            glGenTextures(1, &id);
            glBindTexture(GL_TEXTURE_2D, id);

            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            stbi_image_free(data);
            glBindTexture(GL_TEXTURE_2D, 0);

            tex.id = id;
        }
    }

    if (tex.id != 0)
        loaded.push_back(tex);

    return tex;
}

// =======================================================
// MESH
// =======================================================

struct Mesh {
    GLuint vao = 0, vbo = 0, ebo = 0;
    GLsizei indexCount = 0;
    std::vector<TextureInfo> textures;
    std::string name;

    // Для анимации по нодам:
    int nodeIndex = -1;

    void Draw(GLuint shader) const
    {
        GLuint texId = 0;
        if (!textures.empty() && textures[0].id != 0)
            texId = textures[0].id;

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texId);

        GLint locTex = glGetUniformLocation(shader, "uTex");
        if (locTex >= 0) glUniform1i(locTex, 0);

        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void DrawInstanced(GLuint shader, GLsizei instanceCount) const
    {
        if (!textures.empty()) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, textures[0].id);
            GLint loc = glGetUniformLocation(shader, "uTex");
            if (loc >= 0) glUniform1i(loc, 0);
        }

        glBindVertexArray(vao);
        glDrawElementsInstanced(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0, instanceCount);
        glBindVertexArray(0);

        glActiveTexture(GL_TEXTURE0);
    }
};

// =======================================================
// ANIMATION (node TRS)
// =======================================================

struct NodeTRS
{
    glm::vec3 t{ 0,0,0 };
    glm::quat r;        // identity
    glm::vec3 s{ 1,1,1 };

    NodeTRS() : r(1.0f, 0.0f, 0.0f, 0.0f) {}
};

struct AnimChannel
{
    int nodeIndex = -1;

    std::vector<double> tTimes;
    std::vector<glm::vec3> tValues;

    std::vector<double> rTimes;
    std::vector<glm::quat> rValues;

    std::vector<double> sTimes;
    std::vector<glm::vec3> sValues;
};

struct AnimClip
{
    double durationTicks = 0.0;
    double ticksPerSecond = 25.0;
    std::vector<AnimChannel> channels;
};

// =======================================================
// MODEL
// =======================================================

struct Model {
    std::vector<Mesh> meshes;
    std::string directory;
    std::vector<TextureInfo> loadedTextures;

    // Ноды (для анимации)
    std::vector<std::string> nodeNames;
    std::vector<int> nodeParent;
    std::vector<glm::mat4> nodeBaseLocal;  // base local from aiNode->mTransformation
    std::vector<glm::mat4> nodeAnimLocal;  // animated local (base * TRS from clip)
    std::vector<glm::mat4> nodeGlobal;     // final global

    AnimClip clip;
    bool hasAnimation = false;
    double animTimeTicks = 0.0;

    bool Load(const std::string& path);

    // обычный статический draw (как раньше)
    void Draw(GLuint shader) const;
    void DrawInstanced(GLuint shader, GLsizei instanceCount) const;

    // анимация нод
    void ResetAnimation() { animTimeTicks = 0.0; }
    void UpdateAnimation(float dt);
    void DrawWithAnimation(GLuint shader, const glm::mat4& world) const;
};

// =======================================================
// TREE SYSTEM (как у тебя было)
// =======================================================

struct TreeInstance {
    glm::vec3 pos;
    float     scale;
    float     radius;
};

extern Model g_treeModel;
extern GLuint g_treeInstanceVBO;
extern GLsizei g_treeInstanceCount;
extern std::vector<TreeInstance> g_treeInstances;
extern GLuint g_treeShader;

// g_treeRemoved живёт в main.cpp — объявляем как extern
extern std::vector<bool> g_treeRemoved;

bool LoadOBJ(const char* path, Mesh& outMesh);
void InitTreeObjects();
void DrawTreeObjects(const glm::mat4& proj, const glm::mat4& view);
void ResolveTreeCollisions(glm::vec3& pos);

// =======================================================
// HELPERS
// =======================================================

static int FindKeyIndex(const std::vector<double>& times, double t)
{
    if (times.empty()) return -1;
    int i = 0;
    while (i + 1 < (int)times.size() && times[i + 1] <= t) ++i;
    return i;
}

static glm::vec3 LerpVec3(const glm::vec3& a, const glm::vec3& b, float k)
{
    return a + (b - a) * k;
}

// =======================================================
// Model::Load
// =======================================================

inline bool Model::Load(const std::string& path)
{
    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(
        path,
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_JoinIdenticalVertices |
        aiProcess_ImproveCacheLocality |
        aiProcess_SortByPType |
        aiProcess_OptimizeMeshes |
        aiProcess_OptimizeGraph |
        aiProcess_FlipUVs
    );

    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
        OutputDebugStringA(("ASSIMP error: " + std::string(importer.GetErrorString()) + "\n").c_str());
        return false;
    }

    directory = path.substr(0, path.find_last_of("/\\"));
    meshes.clear();
    loadedTextures.clear();

    // ноды/анимация
    nodeNames.clear();
    nodeParent.clear();
    nodeBaseLocal.clear();
    nodeAnimLocal.clear();
    nodeGlobal.clear();
    clip.channels.clear();
    hasAnimation = false;
    animTimeTicks = 0.0;

    std::unordered_map<std::string, int> nodeIndexByName;

    std::function<void(aiNode*, int)> processNode;
    processNode = [&](aiNode* node, int parentIndex)
        {
            int myIndex = (int)nodeNames.size();
            nodeNames.push_back(node->mName.C_Str());
            nodeIndexByName[nodeNames.back()] = myIndex;

            nodeParent.push_back(parentIndex);
            nodeBaseLocal.push_back(AiToGlm(node->mTransformation));
            nodeAnimLocal.push_back(nodeBaseLocal.back());
            nodeGlobal.push_back(glm::mat4(1.0f));

            // meshes of this node
            for (unsigned int i = 0; i < node->mNumMeshes; ++i)
            {
                aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];

                const std::string meshName = mesh->mName.C_Str();
                const bool isSaw = (meshName == "Mesh0");

                float minU = 0.0f, minV = 0.0f;
                float maxU = 0.0f, maxV = 0.0f;
                bool hasUV = mesh->HasTextureCoords(0);

                if (isSaw && hasUV)
                {
                    minU = minV = 1e9f;
                    maxU = maxV = -1e9f;

                    for (unsigned int v = 0; v < mesh->mNumVertices; ++v)
                    {
                        const aiVector3D uv = mesh->mTextureCoords[0][v];
                        minU = std::min(minU, (float)uv.x);
                        minV = std::min(minV, (float)uv.y);
                        maxU = std::max(maxU, (float)uv.x);
                        maxV = std::max(maxV, (float)uv.y);
                    }
                }

                std::vector<float> vertices;
                std::vector<unsigned int> indices;
                std::vector<TextureInfo> textures;

                vertices.reserve(mesh->mNumVertices * 8);

                for (unsigned int v = 0; v < mesh->mNumVertices; ++v)
                {
                    aiVector3D pos = mesh->mVertices[v];
                    aiVector3D nor = mesh->HasNormals() ? mesh->mNormals[v] : aiVector3D(0, 1, 0);
                    aiVector3D uv = mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][v] : aiVector3D(0, 0, 0);

                    float u = (float)uv.x;
                    float vv = (float)uv.y;

                    //// ✅ ONLY for saw: normalize UV into 0..1
                    //if (isSaw && mesh->HasTextureCoords(0))
                    //{
                    //    float offU = -std::floor(minU);
                    //    float offV = -std::floor(minV);

                    //    u += offU;
                    //    vv += offV;
                    //}

                    // POSITION (3)
                    vertices.push_back(pos.x);
                    vertices.push_back(pos.y);
                    vertices.push_back(pos.z);

                    // NORMAL (3)
                    vertices.push_back(nor.x);
                    vertices.push_back(nor.y);
                    vertices.push_back(nor.z);

                    // UV (2)
                    vertices.push_back(u);
                    vertices.push_back(vv);
                }

                for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
                    const aiFace& face = mesh->mFaces[f];
                    for (unsigned int j = 0; j < face.mNumIndices; ++j)
                        indices.push_back(face.mIndices[j]);
                }

                if (mesh->mMaterialIndex >= 0)
                {
                    aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

                    // ✅ For glTF/GLB prefer BASE_COLOR
                    TextureInfo texBase = LoadTexture_Assimp(scene, material,
                        (aiTextureType)aiTextureType_BASE_COLOR,
                        0, directory,
                        "texture_diffuse", textures);

                    if (texBase.id == 0)
                    {
                        // fallback
                        LoadTexture_Assimp(scene, material,
                            aiTextureType_DIFFUSE,
                            0, directory,
                            "texture_diffuse", textures);
                    }
                }

                // optional material debug
                /*
                if (isSaw)
                {
                    OutputDebugStringA("==== Mesh0 material debug ====\n");
                    char b[128];
                    sprintf_s(b, "materialIndex=%d texturesCount=%d\n",
                        mesh->mMaterialIndex, (int)textures.size());
                    OutputDebugStringA(b);

                    for (auto& t : textures)
                    {
                        std::string s = "type=" + t.type + " path=" + t.path + " id=" + std::to_string(t.id) + "\n";
                        OutputDebugStringA(s.c_str());
                    }
                }
                */

                Mesh out;
                out.name = meshName;
                out.textures = textures;
                out.indexCount = (GLsizei)indices.size();
                out.nodeIndex = myIndex;

                glGenVertexArrays(1, &out.vao);
                glGenBuffers(1, &out.vbo);
                glGenBuffers(1, &out.ebo);

                glBindVertexArray(out.vao);

                glBindBuffer(GL_ARRAY_BUFFER, out.vbo);
                glBufferData(GL_ARRAY_BUFFER,
                    vertices.size() * sizeof(float),
                    vertices.data(),
                    GL_STATIC_DRAW);

                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, out.ebo);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                    indices.size() * sizeof(unsigned int),
                    indices.data(),
                    GL_STATIC_DRAW);

                GLsizei stride = 8 * sizeof(float);
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
                glEnableVertexAttribArray(2);
                glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));

                glBindVertexArray(0);

                meshes.push_back(out);
            }

            for (unsigned int i = 0; i < node->mNumChildren; ++i)
                processNode(node->mChildren[i], myIndex);
        };

    processNode(scene->mRootNode, -1);

    // ==== Load animation (only first clip) ====
    hasAnimation = (scene->mNumAnimations > 0);
    if (hasAnimation)
    {
        aiAnimation* a = scene->mAnimations[0];
        clip.durationTicks = a->mDuration;
        clip.ticksPerSecond = (a->mTicksPerSecond != 0.0 ? a->mTicksPerSecond : 25.0);

        clip.channels.clear();
        clip.channels.reserve(a->mNumChannels);

        for (unsigned int c = 0; c < a->mNumChannels; ++c)
        {
            aiNodeAnim* ch = a->mChannels[c];
            auto it = nodeIndexByName.find(ch->mNodeName.C_Str());
            if (it == nodeIndexByName.end()) continue;

            AnimChannel out;
            out.nodeIndex = it->second;

            // T
            out.tTimes.reserve(ch->mNumPositionKeys);
            out.tValues.reserve(ch->mNumPositionKeys);
            for (unsigned int k = 0; k < ch->mNumPositionKeys; ++k)
            {
                out.tTimes.push_back(ch->mPositionKeys[k].mTime);
                auto v = ch->mPositionKeys[k].mValue;
                out.tValues.push_back(glm::vec3(v.x, v.y, v.z));
            }

            // R
            out.rTimes.reserve(ch->mNumRotationKeys);
            out.rValues.reserve(ch->mNumRotationKeys);
            for (unsigned int k = 0; k < ch->mNumRotationKeys; ++k)
            {
                out.rTimes.push_back(ch->mRotationKeys[k].mTime);
                auto q = ch->mRotationKeys[k].mValue;
                out.rValues.push_back(glm::quat((float)q.w, (float)q.x, (float)q.y, (float)q.z));
            }

            // S
            out.sTimes.reserve(ch->mNumScalingKeys);
            out.sValues.reserve(ch->mNumScalingKeys);
            for (unsigned int k = 0; k < ch->mNumScalingKeys; ++k)
            {
                out.sTimes.push_back(ch->mScalingKeys[k].mTime);
                auto v = ch->mScalingKeys[k].mValue;
                out.sValues.push_back(glm::vec3(v.x, v.y, v.z));
            }

            clip.channels.push_back(std::move(out));
        }
    }

    return !meshes.empty();
}

// =======================================================
// Draw (static)
// =======================================================

inline void Model::Draw(GLuint shader) const
{
    for (const auto& m : meshes)
        m.Draw(shader);
}

inline void Model::DrawInstanced(GLuint shader, GLsizei instanceCount) const
{
    // ВАЖНО:
    // НЕ надо тут проверять g_treeRemoved — это логика инстансов, а не мешей.
    for (const auto& m : meshes)
        m.DrawInstanced(shader, instanceCount);
}

// =======================================================
// UpdateAnimation (node TRS)
// =======================================================

inline void Model::UpdateAnimation(float dt)
{
    if (!hasAnimation) return;

    animTimeTicks += (double)dt * clip.ticksPerSecond;
    if (clip.durationTicks > 0.0)
        animTimeTicks = std::fmod(animTimeTicks, clip.durationTicks);

    // по умолчанию — base pose
    for (size_t i = 0; i < nodeBaseLocal.size(); ++i)
        nodeAnimLocal[i] = nodeBaseLocal[i];

    // применяем каналы: мы перезаписываем локалку на base*TRS
    for (const auto& ch : clip.channels)
    {
        int ni = ch.nodeIndex;
        if (ni < 0 || ni >= (int)nodeAnimLocal.size()) continue;

        glm::vec3 T(0, 0, 0);
        glm::quat R(1, 0, 0, 0);
        glm::vec3 S(1, 1, 1);

        // T
        if (!ch.tTimes.empty())
        {
            int k = FindKeyIndex(ch.tTimes, animTimeTicks);
            int k2 = std::min(k + 1, (int)ch.tTimes.size() - 1);
            if (k >= 0)
            {
                double t0 = ch.tTimes[k], t1 = ch.tTimes[k2];
                float f = (t1 > t0) ? (float)((animTimeTicks - t0) / (t1 - t0)) : 0.0f;
                T = LerpVec3(ch.tValues[k], ch.tValues[k2], f);
            }
        }

        // R
        if (!ch.rTimes.empty())
        {
            int k = FindKeyIndex(ch.rTimes, animTimeTicks);
            int k2 = std::min(k + 1, (int)ch.rTimes.size() - 1);
            if (k >= 0)
            {
                double t0 = ch.rTimes[k], t1 = ch.rTimes[k2];
                float f = (t1 > t0) ? (float)((animTimeTicks - t0) / (t1 - t0)) : 0.0f;
                glm::quat q0 = ch.rValues[k];
                glm::quat q1 = ch.rValues[k2];

                // чтобы не крутило через "длинный путь"
                if (glm::dot(q0, q1) < 0.0f) q1 = -q1;

                R = glm::normalize(glm::quat(
                    q0.w + (q1.w - q0.w) * f,
                    q0.x + (q1.x - q0.x) * f,
                    q0.y + (q1.y - q0.y) * f,
                    q0.z + (q1.z - q0.z) * f
                ));
            }
        }

        // S
        if (!ch.sTimes.empty())
        {
            int k = FindKeyIndex(ch.sTimes, animTimeTicks);
            int k2 = std::min(k + 1, (int)ch.sTimes.size() - 1);
            if (k >= 0)
            {
                double t0 = ch.sTimes[k], t1 = ch.sTimes[k2];
                float f = (t1 > t0) ? (float)((animTimeTicks - t0) / (t1 - t0)) : 0.0f;
                S = LerpVec3(ch.sValues[k], ch.sValues[k2], f);
            }
        }

        glm::mat4 TRS(1.0f);
        TRS = glm::translate(TRS, T);
        TRS *= glm::mat4_cast(R);
        TRS = glm::scale(TRS, S);

        //nodeAnimLocal[ni] = nodeBaseLocal[ni] * TRS;
        nodeAnimLocal[ni] = TRS;
    }

    // пересчитать global
    for (int i = 0; i < (int)nodeAnimLocal.size(); ++i)
    {
        int p = nodeParent[i];
        nodeGlobal[i] = (p >= 0) ? (nodeGlobal[p] * nodeAnimLocal[i]) : nodeAnimLocal[i];
    }
}

// =======================================================
// DrawWithAnimation
// =======================================================

inline void Model::DrawWithAnimation(GLuint shader, const glm::mat4& world) const
{
    GLint loc = glGetUniformLocation(shader, "uModel");

    for (const auto& m : meshes)
    {
        glm::mat4 G = nodeGlobal[m.nodeIndex];
        if (!std::isfinite(G[3][0]) || !std::isfinite(G[3][1]) || !std::isfinite(G[3][2])) {
            OutputDebugStringA("NaN/INF in nodeGlobal\n");
        }

        glm::mat4 M = world;

        if (hasAnimation && m.nodeIndex >= 0 && m.nodeIndex < (int)nodeGlobal.size())
            M = world * nodeGlobal[m.nodeIndex];

        if (loc >= 0)
            glUniformMatrix4fv(loc, 1, GL_FALSE, &M[0][0]);

        m.Draw(shader);
    }
}