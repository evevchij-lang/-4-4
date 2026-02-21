#pragma once

GLuint g_skySphereVAO = 0;
GLuint g_skySphereVBO = 0;
GLuint g_skySphereEBO = 0;
GLuint g_skySphereShader = 0;
GLsizei g_skySphereIndexCount = 0;

//GLuint skyVAO, skyVBO;
//GLuint g_skyShader;


void InitSkySphere()
{
    if (g_skySphereVAO)
        return;

    // Сборка шейдера
    g_skySphereShader = CreateShaderProgram("sky_sphere.vert", "sky_sphere.frag");
    if (!g_skySphereShader)
    {
        OutputDebugStringA("Failed to create sky sphere shader\n");
        return;
    }

    const int stacks = 32;   // по вертикали
    const int slices = 64;   // по горизонтали
    const float radius = 500.0f; // радиус - гораздо больше карты, но на самом деле не критично

    std::vector<glm::vec3> vertices;
    std::vector<unsigned int> indices;

    for (int i = 0; i <= stacks; ++i)
    {
        float v = (float)i / (float)stacks;         // 0..1
        float phi = v * glm::pi<float>();           // 0..PI

        for (int j = 0; j <= slices; ++j)
        {
            float u = (float)j / (float)slices;     // 0..1
            float theta = u * glm::two_pi<float>(); // 0..2PI

            float x = sin(phi) * cos(theta);
            float y = cos(phi);
            float z = sin(phi) * sin(theta);

            vertices.push_back(glm::vec3(x, y, z) * radius);
        }
    }

    // Индексы (треугольники)
    for (int i = 0; i < stacks; ++i)
    {
        for (int j = 0; j < slices; ++j)
        {
            int row1 = i * (slices + 1);
            int row2 = (i + 1) * (slices + 1);

            int a = row1 + j;
            int b = row1 + j + 1;
            int c = row2 + j;
            int d = row2 + j + 1;

            indices.push_back(a);
            indices.push_back(c);
            indices.push_back(b);

            indices.push_back(b);
            indices.push_back(c);
            indices.push_back(d);
        }
    }

    g_skySphereIndexCount = static_cast<GLsizei>(indices.size());

    glGenVertexArrays(1, &g_skySphereVAO);
    glGenBuffers(1, &g_skySphereVBO);
    glGenBuffers(1, &g_skySphereEBO);

    glBindVertexArray(g_skySphereVAO);

    glBindBuffer(GL_ARRAY_BUFFER, g_skySphereVBO);
    glBufferData(GL_ARRAY_BUFFER,
        vertices.size() * sizeof(glm::vec3),
        vertices.data(),
        GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_skySphereEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        indices.size() * sizeof(unsigned int),
        indices.data(),
        GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

    glBindVertexArray(0);
}


void DrawSkySphere(const glm::mat4& proj, const glm::mat4& view)
{
    if (!g_skySphereShader || !g_skySphereVAO)
        return;

    // Небо всегда сзади всего:
    glDepthMask(GL_FALSE);       // не писать глубину
    glDisable(GL_CULL_FACE);     // мы "внутри" сферы, чтобы не мучаться с ориентацией
    // Можно оставить DEPTH_TEST включённым или отключить — проще отключить:
    glDisable(GL_DEPTH_TEST);

    glUseProgram(g_skySphereShader);

    glUniformMatrix4fv(glGetUniformLocation(g_skySphereShader, "uProjection"),
        1, GL_FALSE, &proj[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(g_skySphereShader, "uView"),
        1, GL_FALSE, &view[0][0]);

    // Подбери цвета под вкус
    glm::vec3 topColor = glm::vec3(0.02f, 0.20f, 0.55f); // тёмно-синий
    glm::vec3 horizonColor = glm::vec3(0.35f, 0.55f, 0.95f); // светло-голубой
    glm::vec3 bottomColor = glm::vec3(0.7f, 0.8f, 1.0f);    // почти бело-голубой

    glUniform3fv(glGetUniformLocation(g_skySphereShader, "uTopColor"), 1, &topColor[0]);
    glUniform3fv(glGetUniformLocation(g_skySphereShader, "uHorizonColor"), 1, &horizonColor[0]);
    glUniform3fv(glGetUniformLocation(g_skySphereShader, "uBottomColor"), 1, &bottomColor[0]);

    // Солнце – направление, цвет, размеры
    glm::vec3 sunDir = glm::normalize(glm::vec3(0.3f, 0.6f, 0.2f)); // чуть выше горизонта
    glUniform3fv(glGetUniformLocation(g_skySphereShader, "uSunDir"), 1, &sunDir[0]);

    glm::vec3 sunColor(1.0f, 0.97f, 0.9f);
    glUniform3fv(glGetUniformLocation(g_skySphereShader, "uSunColor"), 1, &sunColor[0]);

    glUniform1f(glGetUniformLocation(g_skySphereShader, "uSunSize"), glm::radians(1.5f)); // ~1.5°
    glUniform1f(glGetUniformLocation(g_skySphereShader, "uSunGlow"), glm::radians(8.0f)); // мягкий ореол

    glUniform1i(glGetUniformLocation(g_skySphereShader, "uUnderwater"), underwater ? 1 : 0);
    glUniform3fv(glGetUniformLocation(g_skySphereShader, "uFogColor"), 1, &fogColorUnder[0]); // если есть




    glBindVertexArray(g_skySphereVAO);
    glDrawElements(GL_TRIANGLES, g_skySphereIndexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    // Вернуть состояние
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
}

