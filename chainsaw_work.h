#pragma once
#include <fstream>
#include <iomanip>
#include <windows.h>
#include "glad/glad.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "modelwork.h"


static bool g_chainsawVMEditMode = true;   // В РЕЛИЗЕ поставить false
static const char* g_chainsawVMFile = "chainsaw_vm.cfg";

struct ChainsawVM
{
    glm::vec3 pos = glm::vec3(0.25f, -0.35f, -1.10f);
    glm::vec3 rot = glm::vec3(glm::radians(-10.0f), glm::radians(160.0f), 0.0f); // radians
    float scale = 0.25f;
};

static ChainsawVM g_chainsawVM;

// В main.cpp это уже есть (создаётся до Init* инструментов)
extern GLuint g_cutShader;

// Тоже в main.cpp
extern float g_time;
extern int g_currentTool;

// -----------------------------
// Chainsaw (work) state
// -----------------------------
static Model  g_chainsawWorkModel;
static bool   g_chainsawWorkLoaded = false;

// хотим сбрасывать анимацию при “доставании” пилы
static bool   g_chainsawWorkWasActive = false;

// Настройка “в руках” (подкрути под себя)
static glm::vec3 g_chainsawOffset = glm::vec3(0.35f, -0.33f, -0.55f);
static glm::vec3 g_chainsawRot = glm::vec3(glm::radians(-10.0f), glm::radians(160.0f), glm::radians(0.0f));
static float     g_chainsawScale = 0.35f;

// Путь к твоему GLB
static const char* g_chainsawWorkPath = "my_chainsaw_opty.glb";
inline void LoadChainsawVM();
inline void SaveChainsawVM();

inline void InitChainsawWork()
{
    // грузим твою модель с анимацией
    g_chainsawWorkLoaded = g_chainsawWorkModel.Load(g_chainsawWorkPath);
    if (!g_chainsawWorkLoaded)
    {
        OutputDebugStringA("InitChainsawWork: FAILED to load my_chainsaw.glb\n");
        return;
    }

    // стартуем с начала
    g_chainsawWorkModel.ResetAnimation();
    LoadChainsawVM();
}

inline void UpdateChainsawWork(float dt)
{
    const bool activeNow = (g_currentTool == TOOL_CHAINSAW_TEST);

    if (activeNow && g_chainsawVMEditMode)
    {
        const float step = 0.02f;
        const float rotStep = glm::radians(2.0f);

        // позиция
        if (GetAsyncKeyState(VK_LEFT) & 0x8000) g_chainsawVM.pos.x -= step;
        if (GetAsyncKeyState(VK_RIGHT) & 0x8000) g_chainsawVM.pos.x += step;
        if (GetAsyncKeyState(VK_UP) & 0x8000) g_chainsawVM.pos.z -= step;
        if (GetAsyncKeyState(VK_DOWN) & 0x8000) g_chainsawVM.pos.z += step;

        if (GetAsyncKeyState(VK_PRIOR) & 0x8000) g_chainsawVM.pos.y += step;
        if (GetAsyncKeyState(VK_NEXT) & 0x8000) g_chainsawVM.pos.y -= step;

        // вращение во всех плоскостях
        if (GetAsyncKeyState(VK_NUMPAD1) & 0x8000) g_chainsawVM.rot.y -= rotStep;
        if (GetAsyncKeyState(VK_NUMPAD3) & 0x8000) g_chainsawVM.rot.y += rotStep;

        if (GetAsyncKeyState(VK_NUMPAD4) & 0x8000) g_chainsawVM.rot.x -= rotStep;
        if (GetAsyncKeyState(VK_NUMPAD6) & 0x8000) g_chainsawVM.rot.x += rotStep;

        if (GetAsyncKeyState(VK_NUMPAD7) & 0x8000) g_chainsawVM.rot.z -= rotStep;
        if (GetAsyncKeyState(VK_NUMPAD9) & 0x8000) g_chainsawVM.rot.z += rotStep;

        // масштаб
        if (GetAsyncKeyState(VK_OEM_MINUS) & 0x8000)
            g_chainsawVM.scale = glm::max(0.01f, g_chainsawVM.scale - 0.01f);

        if (GetAsyncKeyState(VK_OEM_PLUS) & 0x8000)
            g_chainsawVM.scale += 0.01f;

        // сохранить по NumPad 5 (edge)
        if (GetAsyncKeyState(VK_NUMPAD5) & 0x0001)
        {
            SaveChainsawVM();
        }
    }

    if (activeNow && !g_chainsawWorkWasActive)
    {
        if (g_chainsawWorkLoaded)
            g_chainsawWorkModel.ResetAnimation();
    }

    g_chainsawWorkWasActive = activeNow;

    if (activeNow && g_chainsawWorkLoaded)
        g_chainsawWorkModel.UpdateAnimation(dt);
}

inline void DrawChainsawWorkViewModel(const glm::mat4& proj, const glm::mat4& view)
{
    if (g_currentTool != TOOL_CHAINSAW_TEST) return;
    if (!g_chainsawWorkLoaded) return;
    if (!g_cutShader) return;

    // -----------------------------
    // Save GL state (минимально нужное)
    // -----------------------------
    GLboolean wasCull = glIsEnabled(GL_CULL_FACE);
    GLboolean wasDepth = glIsEnabled(GL_DEPTH_TEST);

    GLint oldDepthFunc = GL_LESS;
    glGetIntegerv(GL_DEPTH_FUNC, &oldDepthFunc);

    GLboolean oldDepthMask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &oldDepthMask);

    GLfloat oldDepthRange[2] = { 0.0f, 1.0f };
    glGetFloatv(GL_DEPTH_RANGE, oldDepthRange);

    GLint oldFrontFace = GL_CCW;
    glGetIntegerv(GL_FRONT_FACE, &oldFrontFace);

    GLint oldCullMode = GL_BACK;
    glGetIntegerv(GL_CULL_FACE_MODE, &oldCullMode);

    // -----------------------------
    // Viewmodel GL state
    // -----------------------------
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);

    // Viewmodel “поверх” мира
    glDepthRange(0.0f, 0.1f);

    // -----------------------------
    // Shader + uniforms
    // -----------------------------
    glUseProgram(g_cutShader);

    GLint locP = glGetUniformLocation(g_cutShader, "uProjection");
    if (locP >= 0) glUniformMatrix4fv(locP, 1, GL_FALSE, &proj[0][0]);

    GLint locV = glGetUniformLocation(g_cutShader, "uView");
    if (locV >= 0) glUniformMatrix4fv(locV, 1, GL_FALSE, &view[0][0]);

    // туман выключаем на viewmodel
    GLint locFogD = glGetUniformLocation(g_cutShader, "uFogDensity");
    if (locFogD >= 0) glUniform1f(locFogD, 0.0f);

    GLint locFogC = glGetUniformLocation(g_cutShader, "uFogColor");
    if (locFogC >= 0)
    {
        glm::vec3 fog(0.0f);
        glUniform3fv(locFogC, 1, &fog[0]);
    }

    GLint locUnder = glGetUniformLocation(g_cutShader, "uUnderwater");
    if (locUnder >= 0) glUniform1i(locUnder, 0);

    GLint locLight = glGetUniformLocation(g_cutShader, "uLightDir");
    if (locLight >= 0)
    {
        glm::vec3 lightDir = glm::normalize(glm::vec3(0.4f, 1.0f, 0.2f));
        glUniform3fv(locLight, 1, &lightDir[0]);
    }

    GLint locCam = glGetUniformLocation(g_cutShader, "uCamPos");
    if (locCam >= 0)
    {
        glm::vec3 camPos(0.0f);
        glUniform3fv(locCam, 1, &camPos[0]);
    }

    // -----------------------------
    // Viewmodel transform (g_chainsawVM настраивается клавишами)
    // -----------------------------
    glm::mat4 local(1.0f);
    local = glm::translate(local, g_chainsawVM.pos);

    local = glm::rotate(local, g_chainsawVM.rot.x, glm::vec3(1, 0, 0));
    local = glm::rotate(local, g_chainsawVM.rot.y, glm::vec3(0, 1, 0));
    local = glm::rotate(local, g_chainsawVM.rot.z, glm::vec3(0, 0, 1));

    local = glm::scale(local, glm::vec3(g_chainsawVM.scale));

    glm::mat4 camMatrix = glm::inverse(view);
    glm::mat4 world = camMatrix * local;

    // -----------------------------
    // Draw (animated)
    // -----------------------------
    g_chainsawWorkModel.DrawWithAnimation(g_cutShader, world);

    // -----------------------------
    // Restore GL state
    // -----------------------------
    glDepthRange(oldDepthRange[0], oldDepthRange[1]);
    glDepthFunc(oldDepthFunc);
    glDepthMask(oldDepthMask);

    glFrontFace(oldFrontFace);
    glCullFace(oldCullMode);

    if (wasCull)  glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (wasDepth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
}

inline void SaveChainsawVM()
{
    std::ofstream out(g_chainsawVMFile);
    if (!out.is_open()) return;

    out << std::fixed << std::setprecision(6);

    out << g_chainsawVM.pos.x << " "
        << g_chainsawVM.pos.y << " "
        << g_chainsawVM.pos.z << "\n";

    out << g_chainsawVM.rot.x << " "
        << g_chainsawVM.rot.y << " "
        << g_chainsawVM.rot.z << "\n";

    out << g_chainsawVM.scale << "\n";

    out.close();
}

inline void LoadChainsawVM()
{
    std::ifstream in(g_chainsawVMFile);
    if (!in.is_open())
        return; // файла нет — используем дефолт

    in >> g_chainsawVM.pos.x
        >> g_chainsawVM.pos.y
        >> g_chainsawVM.pos.z;

    in >> g_chainsawVM.rot.x
        >> g_chainsawVM.rot.y
        >> g_chainsawVM.rot.z;

    in >> g_chainsawVM.scale;

    in.close();
}