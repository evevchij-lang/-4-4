#pragma once

#include <windows.h>
#include "glad/glad.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "modelwork.h"

// ==== ЭТО УЖЕ ОПРЕДЕЛЕНО В main.cpp ====
// struct CutAnimInstance { ... };
// CutAnimInstance g_cutAnim;
// Model g_treeCutAnimModel;
// bool g_treeCutAnimLoaded;
// GLuint g_cutShader;
// bool g_cuttingTree;
// bool g_lockPlayerDuringCut;

struct CutAnimInstance; // forward declaration (тип определён в main.cpp)

// ресурсы/состояния из main.cpp
extern CutAnimInstance g_cutAnim;
extern Model  g_treeCutAnimModel;
extern bool   g_treeCutAnimLoaded;
extern GLuint g_cutShader;

extern bool   g_cuttingTree;
extern bool   g_lockPlayerDuringCut;

// путь к glb катсцены распила
static const char* g_cutAnimPath = "test_cut.glb";

// ---- матрица мира для катсцены (использует поля из g_cutAnim) ----
inline glm::mat4 BuildCutWorldMatrix()
{
    glm::mat4 M(1.0f);
    M = glm::translate(M, g_cutAnim.pos);

    // rot хранится в радианах (у тебя так в main.cpp подписано)
    M = glm::rotate(M, g_cutAnim.rot.y, glm::vec3(0, 1, 0));
    M = glm::rotate(M, g_cutAnim.rot.x, glm::vec3(1, 0, 0));
    M = glm::rotate(M, g_cutAnim.rot.z, glm::vec3(0, 0, 1));

    M = glm::scale(M, glm::vec3(g_cutAnim.scale));
    return M;
}

// ---- Init ----
inline void InitCutAnim()
{
    g_treeCutAnimLoaded = g_treeCutAnimModel.Load(g_cutAnimPath);
    if (!g_treeCutAnimLoaded)
    {
        OutputDebugStringA("InitCutAnim: FAILED to load test_cut.glb\n");
        return;
    }

    g_treeCutAnimModel.ResetAnimation();

    g_cutAnim.active = false;
    g_cutAnim.t = 0.0f;

    g_cuttingTree = false;
}

// ---- Start ----
inline void StartCutAnimAt(const glm::vec3& p)
{
    if (!g_treeCutAnimLoaded) return;

    g_cutAnim.active = true;
    g_cutAnim.pos = p;
    g_cutAnim.t = 0.0f;

    // если в main.cpp duration уже выставлен — не трогаем
    // иначе можно поставить дефолт:
    // g_cutAnim.duration = 1.25f;

    g_cuttingTree = true;

    g_treeCutAnimModel.ResetAnimation();
}

// ---- Stop ----
inline void StopCutAnim()
{
    g_cutAnim.active = false;
    g_cuttingTree = false;
}

// ---- Update ----
inline void UpdateCutAnim(float dt)
{
    if (!g_cutAnim.active) return;
    if (!g_treeCutAnimLoaded) return;

    g_cutAnim.t += dt;

    g_treeCutAnimModel.UpdateAnimation(dt);

    // автостоп по времени, как у тебя задумано
    if (g_cutAnim.duration > 0.0f && g_cutAnim.t >= g_cutAnim.duration)
        StopCutAnim();
}

// ---- Draw ----
inline void DrawCutAnim(const glm::mat4& proj, const glm::mat4& view)
{
    if (!g_cutAnim.active) return;
    if (!g_treeCutAnimLoaded) return;
    if (!g_cutShader) return;

    glUseProgram(g_cutShader);

    GLint locP = glGetUniformLocation(g_cutShader, "uProjection");
    if (locP >= 0) glUniformMatrix4fv(locP, 1, GL_FALSE, &proj[0][0]);

    GLint locV = glGetUniformLocation(g_cutShader, "uView");
    if (locV >= 0) glUniformMatrix4fv(locV, 1, GL_FALSE, &view[0][0]);

    glm::mat4 world = BuildCutWorldMatrix();

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    g_treeCutAnimModel.DrawWithAnimation(g_cutShader, world);

    glDisable(GL_CULL_FACE);
}