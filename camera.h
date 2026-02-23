#pragma once
// camera.h
#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Camera {
    glm::vec3 pos;
    float yaw;
    float pitch;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;

    Camera(glm::vec3 startPos)
        : pos(startPos), yaw(-90.0f), pitch(0.0f)
    {
        updateVectors();
    }

    void updateVectors()
    {
        glm::vec3 f;
        f.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        f.y = sin(glm::radians(pitch));
        f.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(f);
        right = glm::normalize(glm::cross(front, glm::vec3(0, 1, 0)));
        up = glm::normalize(glm::cross(right, front));
    }

    glm::mat4 getView() const
    {
        return glm::lookAt(pos, pos + front, up);
    }
};

extern Camera g_cam;