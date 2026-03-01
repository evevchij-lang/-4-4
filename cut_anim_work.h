#pragma once

#include <windows.h>
#include "glad/glad.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <fstream>
#include <iomanip>

#include "modelwork.h"

// ==== ЭТО УЖЕ ОПРЕДЕЛЕНО В main.cpp ====
// struct CutAnimInstance { bool active; glm::vec3 pos; glm::vec3 rot; float scale; float t; float duration; ... };
// CutAnimInstance g_cutAnim;
// Model g_treeCutAnimModel;
// bool g_treeCutAnimLoaded;
// GLuint g_cutShader;
// bool g_cuttingTree;
// bool g_lockPlayerDuringCut;

struct CutAnimInstance; // тип определён в main.cpp

// ресурсы/состояния из main.cpp
extern CutAnimInstance g_cutAnim;

glm::vec3 g_cutCamOffset(0.0f, 1.2f, 2.5f); // будет сохраняться на Num5
float g_cutCamYaw = 0.0f;                  // опционально
float g_cutCamPitch = 0.0f;                // опционально
bool g_useCutFixedCamera = true;

extern Model  g_treeCutAnimModel;
extern bool   g_treeCutAnimLoaded;
extern GLuint g_cutShader;

extern bool   g_cuttingTree;
extern bool   g_lockPlayerDuringCut;

// путь к glb катсцены распила
static const char* g_cutAnimPath = "test_cut.glb";

// ------------------------------
// Debug/edit options (как у пилы)
// ------------------------------
static bool g_cutAnimEditMode = true;              // В РЕЛИЗЕ false
//static bool debugAnim = false;                     // твой флаг: если true — лупаем анимацию
static const char* g_cutAnimCfgFile = "cut_anim.cfg";

// ---- сохранить текущие параметры распила ----
inline void SaveCutAnimCfg()
{
    std::ofstream out(g_cutAnimCfgFile);
    if (!out.is_open()) return;

    out << std::fixed << std::setprecision(6);

    // 1) сохраняем OFFSET камеры от дерева
    out << g_cutCamOffset.x << " "
        << g_cutCamOffset.y << " "
        << g_cutCamOffset.z << "\n";

    // 2) сохраняем поворот сцены распила
    out << g_cutAnim.rot.x << " "
        << g_cutAnim.rot.y << " "
        << g_cutAnim.rot.z << "\n";

    // 3) масштаб
    out << g_cutAnim.scale << "\n";

    // 4) длительность
    out << g_cutAnim.duration << "\n";

    // 5) сохраняем направление камеры
    out << g_cutCamYaw << " "
        << g_cutCamPitch << "\n";

    out.close();
}

// ---- загрузить параметры распила если файл существует ----
inline void LoadCutAnimCfg()
{
    std::ifstream in(g_cutAnimCfgFile);
    if (!in.is_open()) return;

    // 1) offset камеры
    in >> g_cutCamOffset.x
        >> g_cutCamOffset.y
        >> g_cutCamOffset.z;

    // 2) rot сцены
    in >> g_cutAnim.rot.x
        >> g_cutAnim.rot.y
        >> g_cutAnim.rot.z;

    // 3) scale
    in >> g_cutAnim.scale;

    // 4) duration
    in >> g_cutAnim.duration;

    // 5) yaw/pitch камеры
    in >> g_cutCamYaw
        >> g_cutCamPitch;

    in.close();
}

// ---- матрица мира для катсцены (использует поля из g_cutAnim) ----
inline glm::mat4 BuildCutWorldMatrix()
{
    glm::mat4 M(1.0f);

    M = glm::translate(M, g_cutAnim.pos);

    // rot хранится в радианах
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

    // Подхватим сохранённые параметры, если есть
    LoadCutAnimCfg();

    g_treeCutAnimModel.ResetAnimation();

    g_cutAnim.active = false;
    g_cutAnim.t = 0.0f;

    g_cuttingTree = false;
}

// ---- Start ----
inline void StartCutAnimAt(const glm::vec3& treePos, bool applyCamera)
{
    g_cutAnim.pos = treePos;
    g_cutAnim.t = 0.0f;
    g_cutAnim.active = true;
    g_cuttingTree = true;

    g_treeCutAnimModel.ResetAnimation();

    //// Камеру применяем ТОЛЬКО если явно просили
    //if (applyCamera && g_useCutFixedCamera && g_lockPlayerDuringCut)
    //{
    //    g_cam.pos = treePos + g_cutCamOffset;
    //    g_cam.yaw = g_cutCamYaw;
    //    g_cam.pitch = g_cutCamPitch;
    //    g_cam.updateVectors();
    //}
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
    // --- управление параметрами (только в edit mode) ---
    // Старые клавиши, как ты просил:
    // стрелки: x/z, PageUp/PageDown: y, NumPad1/3: rot.y, NumPad4/6: rot.x, NumPad7/9: rot.z, +/- scale
    // NumPad5: сохранить
    if (g_cutAnimEditMode)
    {
        const float step = 0.05f;
        const float rotStep = glm::radians(2.0f);

        if (g_cutAnim.active)
        {
            const float step = 0.1f;

            if (GetAsyncKeyState(VK_LEFT) & 0x8000) g_cam.pos.x -= step;
            if (GetAsyncKeyState(VK_RIGHT) & 0x8000) g_cam.pos.x += step;

            if (GetAsyncKeyState(VK_UP) & 0x8000) g_cam.pos.z -= step;
            if (GetAsyncKeyState(VK_DOWN) & 0x8000) g_cam.pos.z += step;

            if (GetAsyncKeyState(VK_PRIOR) & 0x8000) g_cam.pos.y += step;
            if (GetAsyncKeyState(VK_NEXT) & 0x8000) g_cam.pos.y -= step;
        }

        // вращение во всех плоскостях
        if (GetAsyncKeyState(VK_NUMPAD1) & 0x8000) g_cutAnim.rot.y -= rotStep;
        if (GetAsyncKeyState(VK_NUMPAD3) & 0x8000) g_cutAnim.rot.y += rotStep;

        if (GetAsyncKeyState(VK_NUMPAD4) & 0x8000) g_cutAnim.rot.x -= rotStep;
        if (GetAsyncKeyState(VK_NUMPAD6) & 0x8000) g_cutAnim.rot.x += rotStep;

        if (GetAsyncKeyState(VK_NUMPAD7) & 0x8000) g_cutAnim.rot.z -= rotStep;
        if (GetAsyncKeyState(VK_NUMPAD9) & 0x8000) g_cutAnim.rot.z += rotStep;

        // масштаб
        if (GetAsyncKeyState(VK_OEM_MINUS) & 0x8000)
            g_cutAnim.scale = glm::max(0.01f, g_cutAnim.scale - 0.01f);

        if (GetAsyncKeyState(VK_OEM_PLUS) & 0x8000)
            g_cutAnim.scale += 0.01f;

        // сохранить конфиг по NumPad5 (edge)
        if (GetAsyncKeyState(VK_NUMPAD5) & 0x0001)
        {
            // дерево/сцена в world
            const glm::vec3 treePos = g_cutAnim.pos;

            // сохраняем не world pos камеры, а offset от дерева
            g_cutCamOffset = g_cam.pos - treePos;

            // если хочешь сохранять взгляд
            g_cutCamYaw = g_cam.yaw;
            g_cutCamPitch = g_cam.pitch;

            //SaveCutConfigToFile(); // твоя функция (или просто запиши в файл)
            SaveCutAnimCfg();
        }
            

        // удобный toggle лупа на NumPad8 (edge) — можно убрать если не надо
        if (GetAsyncKeyState(VK_NUMPAD8) & 0x0001)
            debugAnim = !debugAnim;
    }

    // --- обычный апдейт анимации ---
    if (!g_cutAnim.active) return;
    if (!g_treeCutAnimLoaded) return;

    g_cutAnim.t += dt;

    g_treeCutAnimModel.UpdateAnimation(dt);

    // автостоп по длительности (или луп если debugAnim)
    if (g_cutAnim.duration > 0.0f && g_cutAnim.t >= g_cutAnim.duration)
    {
        if (!debugAnim)
        {
            StopCutAnim();
        }
        else
        {
            g_cutAnim.t = 0.0f;
            g_treeCutAnimModel.ResetAnimation();
        }
    }
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

inline void ApplyCutCamera(const glm::vec3& treePos)
{
    g_cam.pos = treePos + g_cutCamOffset;
    g_cam.yaw = g_cutCamYaw;
    g_cam.pitch = g_cutCamPitch;
    g_cam.updateVectors();
}