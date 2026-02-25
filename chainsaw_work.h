#pragma once

#include <windows.h>
#include "glad/glad.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "modelwork.h"

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
}

inline void UpdateChainsawWork(float dt)
{
    // активна ли пила в руках
    bool activeNow = (g_currentTool == TOOL_CHAINSAW_TEST);

    // при переключении на пилу — сбросить анимацию, чтобы всегда “оживала”
    if (activeNow && !g_chainsawWorkWasActive)
        g_chainsawWorkModel.ResetAnimation();

    g_chainsawWorkWasActive = activeNow;

    // Анимацию крутим только когда инструмент выбран
    // (можно крутить всегда — не принципиально)
    if (activeNow && g_chainsawWorkLoaded)
        g_chainsawWorkModel.UpdateAnimation(dt);
}

inline void DrawChainsawWorkViewModel(const glm::mat4& proj, const glm::mat4& view)
{
    if (g_currentTool != TOOL_CHAINSAW_TEST) return;
    if (!g_chainsawWorkLoaded) return;
    if (!g_cutShader) return;

    glUseProgram(g_cutShader);

    // обязательные uniform из cut_anim.vert/frag
    glUniformMatrix4fv(glGetUniformLocation(g_cutShader, "uProjection"), 1, GL_FALSE, &proj[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(g_cutShader, "uView"), 1, GL_FALSE, &view[0][0]);

    // чтобы туман не влиял на вьюмодель:
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

    // освещение (просто чтобы было не “плоско”)
    GLint locLight = glGetUniformLocation(g_cutShader, "uLightDir");
    if (locLight >= 0)
    {
        glm::vec3 lightDir = glm::normalize(glm::vec3(0.4f, 1.0f, 0.2f));
        glUniform3fv(locLight, 1, &lightDir[0]);
    }

    // uCamPos нужен только для тумана (который мы выключили), но поставим безопасно:
    GLint locCam = glGetUniformLocation(g_cutShader, "uCamPos");
    if (locCam >= 0)
    {
        glm::vec3 camPos(0.0f);
        glUniform3fv(locCam, 1, &camPos[0]);
    }

    // -----------------------------
    // Viewmodel transform (как shovel/rake)
    // -----------------------------
    glm::mat4 local(1.0f);
    local = glm::translate(local, g_chainsawOffset);

    local = glm::rotate(local, g_chainsawRot.x, glm::vec3(1, 0, 0));
    local = glm::rotate(local, g_chainsawRot.y, glm::vec3(0, 1, 0));
    local = glm::rotate(local, g_chainsawRot.z, glm::vec3(0, 0, 1));

    local = glm::scale(local, glm::vec3(g_chainsawScale));

    glm::mat4 camMatrix = glm::inverse(view);
    glm::mat4 world = camMatrix * local;

    // состояния глубины как у инструментов
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);

    glDepthRange(0.0, 0.1);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // ВАЖНО: рисуем именно “анимированно”, как в катсцене распила
    g_chainsawWorkModel.DrawWithAnimation(g_cutShader, world);

    glDisable(GL_CULL_FACE);
    glDepthRange(0.0, 1.0);
}