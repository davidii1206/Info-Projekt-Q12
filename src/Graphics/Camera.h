#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

/**
 * @class Camera
 * @brief An FPS-style camera for testing purposes.
 */
class Camera {
public:
    Camera() : m_Position(0.0f, 0.0f, 0.0f), m_Yaw(-90.0f), m_Pitch(0.0f) {
        UpdateVectors();
    }

    void Update(float deltaTime) {}

    void Rotate(float xOffset, float yOffset) {
        m_Yaw += xOffset * m_Sensitivity;
        m_Pitch -= yOffset * m_Sensitivity; // SDL yrel is positive downwards, so moving up is negative yrel. Pitch should increase to look up.
        if (m_Pitch > 89.0f) m_Pitch = 89.0f;
        if (m_Pitch < -89.0f) m_Pitch = -89.0f;
        UpdateVectors();
    }

    void MoveForward(float deltaTime) { m_Position += m_Front * m_MovementSpeed * deltaTime; }
    void MoveBackward(float deltaTime) { m_Position -= m_Front * m_MovementSpeed * deltaTime; }
    void MoveLeft(float deltaTime) { m_Position -= m_Right * m_MovementSpeed * deltaTime; }
    void MoveRight(float deltaTime) { m_Position += m_Right * m_MovementSpeed * deltaTime; }
    void MoveUp(float deltaTime) { m_Position += m_WorldUp * m_MovementSpeed * deltaTime; }
    void MoveDown(float deltaTime) { m_Position -= m_WorldUp * m_MovementSpeed * deltaTime; }

    void UpdateVectors() {
        glm::vec3 front;
        front.x = cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
        front.y = sin(glm::radians(m_Pitch));
        front.z = sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
        m_Front = glm::normalize(front);
        m_Right = glm::normalize(glm::cross(m_Front, m_WorldUp));
        m_Up = glm::normalize(glm::cross(m_Right, m_Front));
    }

    glm::mat4 GetViewMatrix() const {
        return glm::lookAt(m_Position, m_Position + m_Front, m_Up);
    }

    glm::mat4 GetProjectionMatrix(float aspect) const {
        if (aspect <= 0.0f) aspect = 1.0f;
        float near = glm::max(m_Near, 0.01f);
        float far = glm::max(m_Far, near + 1.0f);
        
        glm::mat4 proj = glm::perspective(glm::radians(m_FOV), aspect, near, far);
        
        proj[2][2] = near / (far - near);
        proj[3][2] = (far * near) / (far - near);
        
        return proj;
    }

    glm::vec3 m_Position;
    glm::vec3 m_Front;
    glm::vec3 m_Up;
    glm::vec3 m_Right;
    glm::vec3 m_WorldUp = {0.0f, 1.0f, 0.0f};

    float m_Yaw;
    float m_Pitch;
    float m_FOV = 90.0f; 
    float m_Near = 0.5f;
    float m_Far = 20000.0f;

    float m_MovementSpeed = 10.0f;
    float m_Sensitivity = 0.1f;
};
