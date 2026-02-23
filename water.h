#pragma once
GLuint g_waterVAO = 0;
GLuint g_waterVBO = 0;
GLuint g_waterShader = 0;

float g_waterHeight = 2.0f;        // уровень воды по Y Ч подгони под рельеф
float g_waterSize = 1024.0f;      // размер квадрата воды (не меньше размера террейна)

//дл€ раскопок чтобы вода не по€вл€лась по среди террейна
std::vector<uint8_t> waterMask; // 0 = сухо, 1 = вода
int waterW, waterH;             // как hmW, hmH
GLuint g_waterMaskTex = 0;

bool underwater;

//glm::vec3 fogColorUnder;
//glm::vec3 fogColorTop;
//float fogDensityUnder;
//float fogDensityTop;


//дл€ подводы 
glm::vec3 fogColorTop = glm::vec3(0.6f, 0.7f, 0.9f);
glm::vec3 fogColorUnder = glm::vec3(0.04f, 0.18f, 0.22f);
float fogDensityTop = 0.001f;
float fogDensityUnder = 0.8f;

void InitWater()
{
    g_waterShader = CreateShaderProgram("water.vert", "water.frag");
    if (!g_waterShader) {
        // сюда подгрузи шейдеры water_vert / water_frag, см. ниже
        return;
    }

    if (!g_waterVAO)
    {
        float s = g_waterSize * 0.5f;

        // ѕлоскость XY: позиции + UV
        float verts[] = {
            //  x      y                z       u  v
            -s, g_waterHeight, -s,     0.0f, 0.0f,
             s, g_waterHeight, -s,     1.0f, 0.0f,
             s, g_waterHeight,  s,     1.0f, 1.0f,
            -s, g_waterHeight,  s,     0.0f, 1.0f
        };

        unsigned int idx[] = { 0,1,2,  0,2,3 };

        GLuint ebo;
        glGenVertexArrays(1, &g_waterVAO);
        glGenBuffers(1, &g_waterVBO);
        glGenBuffers(1, &ebo);

        glBindVertexArray(g_waterVAO);

        glBindBuffer(GL_ARRAY_BUFFER, g_waterVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);

        GLsizei stride = 5 * sizeof(float);
        glEnableVertexAttribArray(0); // position
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(1); // uv
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));

        glBindVertexArray(0);
    }
}







