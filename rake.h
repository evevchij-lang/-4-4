#pragma once
Model  g_rakeModel;
GLuint g_rakeShader = 0;

bool   g_rakeSwinging = false;
float  g_rakeSwingTime = 0.0f;
float  g_rakeSwingDuration = 0.25f;   // длительность одного взмаха (секунды)
bool   g_rakeHitDone = false;   // чтобы косить траву один раз за взмах

void InitRake()
{
    // Подложи сюда свой файл, например "rake.obj" или "rake.fbx"
    if (!g_rakeModel.Load("gardeners_rake.glb")) {
        OutputDebugStringA("Failed to load rake model\n");
        return;
    }

    // Можно использовать тот же шейдер, что и для деревьев,
    // если он: uProjection, uView, uModel, uTex, uLightDir.
    // Но лучше сделать отдельный, попроще.
    //g_rakeShader = CreateShaderProgram("rake.vert", "rake.frag");
    g_rakeShader = CreateShaderProgram("rake.vert", "rake.frag");
}

//void DrawRakeViewModel(const glm::mat4& proj, const glm::mat4& view)
//{
//    if (g_currentTool != TOOL_RAKE) return;
//    if (!g_rakeShader || g_rakeModel.meshes.empty()) return;
//
//    glUseProgram(g_rakeShader);
//
//    glUniformMatrix4fv(glGetUniformLocation(g_rakeShader, "uProjection"), 1, GL_FALSE, &proj[0][0]);
//    glUniformMatrix4fv(glGetUniformLocation(g_rakeShader, "uView"), 1, GL_FALSE, &view[0][0]);
//
//    glm::vec3 lightDir = glm::normalize(glm::vec3(0.4f, 1.0f, 0.2f));
//    glUniform3fv(glGetUniformLocation(g_rakeShader, "uLightDir"), 1, &lightDir[0]);
//
//    // --- Локальное положение граблей в координатах камеры ---
//    // x: вправо, y: вверх, z: ВПЕРЁД по направлению взгляда
//    glm::vec3 localOffset(0.2f, -0.3f, 0.6f); // подгони по вкусу
//
//    glm::mat4 local(1.0f);
//    local = glm::translate(local, localOffset);
//
//    // Анимация взмаха: от себя, дуга до 90°
//    if (g_rakeSwinging)
//    {
//        float t = g_rakeSwingTime / g_rakeSwingDuration; // 0..1
//        float swing = sin(t * 3.1415926f);               // 0..1..0
//        float angle = swing * glm::radians(90.0f);
//
//        // локальная ось X (1,0,0) — вправо; поворот -angle наклоняет от игрока вперёд
//        local = glm::rotate(local, -angle, glm::vec3(-1.0f, 0.0f, 0.0f));
//    }
//
//    // масштаб граблей
//    local = glm::scale(local, glm::vec3(0.4f)); // если слишком большие/маленькие — подкрути
//
//    // --- Перевод из локальных (экранных) координат в мир ---
//    glm::vec3 R = g_cam.right;
//    glm::vec3 U = g_cam.up;
//    glm::vec3 F = g_cam.front; // ВАЖНО: БЕЗ минуса
//
//    glm::mat4 camBasis(1.0f);
//    camBasis[0] = glm::vec4(R, 0.0f);        // локальный X → right
//    camBasis[1] = glm::vec4(U, 0.0f);        // локальный Y → up
//    camBasis[2] = glm::vec4(F, 0.0f);        // локальный Z → ВПЕРЁД (как смотрим)
//    camBasis[3] = glm::vec4(g_cam.pos, 1.0f);// позиция камеры
//
//    glm::mat4 model = camBasis * local;
//
//    glUniformMatrix4fv(glGetUniformLocation(g_rakeShader, "uModel"), 1, GL_FALSE, &model[0][0]);
//
//    // Рисуем поверх мира
//    glDisable(GL_DEPTH_TEST);
//    g_rakeModel.Draw(g_rakeShader);
//    glEnable(GL_DEPTH_TEST);
//}

void DrawRakeViewModel(const glm::mat4& proj, const glm::mat4& view)
{
    if (g_currentTool != TOOL_RAKE) return;
    if (!g_rakeShader || g_rakeModel.meshes.empty()) return;

    glUseProgram(g_rakeShader);

    glUniformMatrix4fv(glGetUniformLocation(g_rakeShader, "uProjection"), 1, GL_FALSE, &proj[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(g_rakeShader, "uView"), 1, GL_FALSE, &view[0][0]);

    glm::vec3 lightDir = glm::normalize(glm::vec3(0.4f, 1.0f, 0.2f));
    glUniform3fv(glGetUniformLocation(g_rakeShader, "uLightDir"), 1, &lightDir[0]);

    // локальное положение граблей в системе камеры
    glm::mat4 local(1.0f);

    // где в руках (под экраном справа-спереди)
    //Х - слва центра справа в ракух, У - выше нижу Z- впереди сзади
    /*const glm::vec3 offset(0.8f, -1.0f, -0.45f);*/
    const glm::vec3 offset(0.8f, -1.0f, -3.0f);

    // pivot в модели: точка у черенка (в МОДЕЛЬНЫХ координатах)
    // подбери по модели; начнём с лёгкого значения:
    const glm::vec3 pivot(0.0f, 0.0f, 0.0f);

    // переносим лопату к рукам
    local = glm::translate(local, offset);

    // === АНИМАЦИЯ ВЗМАХА ===
    float t = 0.0f;
    if (g_rakeSwinging)
        t = glm::clamp(g_rakeSwingTime / g_rakeSwingDuration, 0.0f, 1.0f);

    if (t > 0.0f)
    {
        // плавная кривая 0..1..0
        float swing = sin(t * 3.1415926f);

        // старт: сильно опущена (черенок вверх), конец: почти вперёд
        /*const float startDeg = 70.0f;
        const float endDeg = -10.0f;
        float angle = glm::radians(startDeg + (endDeg - startDeg) * swing);*/
          // 0..1..0
         float angle = swing * glm::radians(90.0f);

        // поворачиваем вокруг pivot: T(pivot) * R * T(-pivot)
        local = glm::translate(local, pivot);
        local = glm::rotate(local, -angle, glm::vec3(1.0f, 0.0f, 0.0f)); // ось X — от себя/к себе
        local = glm::translate(local, -pivot);
    }

    // масштаб
    local = glm::scale(local, glm::vec3(0.4f));

    // === В МИРОВЫЕ КООРДИНАТЫ ЧЕРЕЗ КАМЕРУ ===

    glm::mat4 camMatrix = glm::inverse(view);   // корректная матрица камеры
    glm::mat4 model = camMatrix * local;

    glUniformMatrix4fv(glGetUniformLocation(g_rakeShader, "uModel"), 1, GL_FALSE, &model[0][0]);

    glDisable(GL_DEPTH_TEST);
    g_rakeModel.Draw(g_rakeShader);
    glEnable(GL_DEPTH_TEST);
}

//void DrawRakeViewModel(const glm::mat4& proj, const glm::mat4& view)
//{
//    if (g_currentTool != TOOL_RAKE)
//        return;
//    if (!g_rakeShader || g_rakeModel.meshes.empty())
//        return;
//
//    glUseProgram(g_rakeShader);
//
//    glUniformMatrix4fv(glGetUniformLocation(g_rakeShader, "uProjection"), 1, GL_FALSE, &proj[0][0]);
//    glUniformMatrix4fv(glGetUniformLocation(g_rakeShader, "uView"), 1, GL_FALSE, &view[0][0]);
//
//    glm::vec3 lightDir = glm::normalize(glm::vec3(0.4f, 1.0f, 0.2f));
//    glUniform3fv(glGetUniformLocation(g_rakeShader, "uLightDir"), 1, &lightDir[0]);
//
//    glm::mat4 local(1.0f);
//
//    // позиция у края экрана
//    const glm::vec3 offset(0.8f, -1.0f, -3.0f);
//    //const glm::vec3 offset(0.5f, -0.3f, 0.6f);
//    local = glm::translate(local, offset);
//
//    // вращаем вокруг локальной вертикальной оси (Y)
//    local = glm::rotate(local, g_rakeSpinAngle, glm::vec3(0.0f, 1.0f, 0.0f));
//
//    // масштаб как раньше
//    local = glm::scale(local, glm::vec3(0.4f));
//
//    glm::mat4 camMatrix = glm::inverse(view);
//    glm::mat4 model = camMatrix * local;
//
//    glUniformMatrix4fv(glGetUniformLocation(g_rakeShader, "uModel"), 1, GL_FALSE, &model[0][0]);
//
//    // === НАСТРОЙКИ ДЛЯ ВЬЮМОДЕЛИ ===
//
//   // depth-тест ВКЛЮЧЁН, записываем глубину (само-окклюзия работает)
//    glEnable(GL_DEPTH_TEST);
//    glDepthMask(GL_TRUE);
//    glDepthFunc(GL_LEQUAL);
//
//    // инструмент рендерим в ближнем диапазоне глубины [0 .. 0.1]
//    glDepthRange(0.0, 0.1);
//
//    // отсечение задних граней
//    glEnable(GL_CULL_FACE);
//    glCullFace(GL_BACK);
//    glFrontFace(GL_CCW);
//
//    g_rakeModel.Draw(g_rakeShader);
//
//    // === ВОЗВРАЩАЕМ СОСТОЯНИЕ ===
//
//    glDisable(GL_CULL_FACE);
//    glDepthRange(0.0, 1.0);
//    // depth-test оставляем включенным — мир уже нарисован раньше
//
//}
