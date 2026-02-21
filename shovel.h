#pragma once
// добавляем для лопаты
bool  g_shovelSwinging = false;
float g_shovelSwingTime = 0.0f;  // 0..1
bool  g_shovelHitDone = false; // чтобы копнуть один раз за взмах
Model  g_shovelModel;
GLuint g_shovelShader = 0;
const float g_shovelSwingDuration = 0.5f;


void InitShovel()
{
    // Подложи сюда свой файл, например "rake.obj" или "rake.fbx"
    if (!g_shovelModel.Load("shovel_low_poly_gltf\\untitled.obj")) {
        OutputDebugStringA("Failed to load rake model\n");
        return;
    }

    // Можно использовать тот же шейдер, что и для деревьев,
    // если он: uProjection, uView, uModel, uTex, uLightDir.
    // Но лучше сделать отдельный, попроще.
    g_shovelShader = CreateShaderProgram("shovel.vert", "shovel.frag");
}

void DrawShovelViewModel(const glm::mat4& proj, const glm::mat4& view)
{
    if (g_currentTool != TOOL_SHOVEL && !g_shovelSwinging)
        return;
    if (!g_shovelShader || g_shovelModel.meshes.empty())
        return;

    glUseProgram(g_shovelShader);

    glUniformMatrix4fv(glGetUniformLocation(g_shovelShader, "uProjection"), 1, GL_FALSE, &proj[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(g_shovelShader, "uView"), 1, GL_FALSE, &view[0][0]);

    glm::vec3 lightDir = glm::normalize(glm::vec3(0.4f, 1.0f, 0.2f));
    glUniform3fv(glGetUniformLocation(g_shovelShader, "uLightDir"), 1, &lightDir[0]);

    // === ЛОКАЛЬНАЯ ПОЗИЦИЯ ЛОПАТЫ (в системе камеры) ===

    glm::mat4 local(1.0f);

    // где в руках (под экраном справа-спереди)
    //Х - слва центра справа в ракух, У - выше нижу - впереди сзади
    const glm::vec3 offset(0.35f, -0.35f, -0.45f);

    // pivot в модели: точка у черенка (в МОДЕЛЬНЫХ координатах)
    // подбери по модели; начнём с лёгкого значения:
    const glm::vec3 pivot(0.0f, 0.0f, 0.0f);

    // переносим лопату к рукам
    local = glm::translate(local, offset);

    // === АНИМАЦИЯ ВЗМАХА ===
    float t = 0.0f;
    if (g_shovelSwinging)
        t = glm::clamp(g_shovelSwingTime / g_shovelSwingDuration, 0.0f, 1.0f);

    if (t > 0.0f)
    {
        // плавная кривая 0..1..0
        float swing = sin(t * 3.1415926f);

        // старт: сильно опущена (черенок вверх), конец: почти вперёд
       /* const float startDeg = 70.0f;
        const float endDeg = -10.0f;
        float angle = glm::radians(startDeg + (endDeg - startDeg) * swing);*/
        float angle = swing * glm::radians(22.5f);

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

    glUniformMatrix4fv(glGetUniformLocation(g_shovelShader, "uModel"), 1, GL_FALSE, &model[0][0]);

    // depth-тест ВКЛЮЧЁН, записываем глубину (само-окклюзия работает)
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);

    // инструмент рендерим в ближнем диапазоне глубины [0 .. 0.1]
    glDepthRange(0.0, 0.1);

    // отсечение задних граней
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    g_shovelModel.Draw(g_shovelShader);

    // === ВОЗВРАЩАЕМ СОСТОЯНИЕ ===

    glDisable(GL_CULL_FACE);
    glDepthRange(0.0, 1.0);
    // depth-test оставляем включенным — мир уже нарисован раньше
}

