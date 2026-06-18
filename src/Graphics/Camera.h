/**
 * @file Camera.h
 * @brief FPS-style camera for scene navigation.
 */
#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

/**
 * @enum ProjectionMode
 * @brief Selects between perspective and orthographic projection.
 */
enum class ProjectionMode {
    Perspective,
    Orthographic
};

/**
 * @class Camera
 * @brief An FPS-style camera for scene navigation.
 * 
 * Supports both perspective and orthographic projection, allowing
 * seamless switching between 1st-person, RTS commander, and building modes.
 */
class Camera {
public:
    /**
     * @brief Constructs a new Camera at the origin looking forward.
     */
    Camera() : m_Position(0.0f, 0.0f, 0.0f), m_Yaw(-90.0f), m_Pitch(0.0f) {
        UpdateVectors();
    }

    /**
     * @brief Updates the camera state.
     * @param deltaTime Time elapsed since the last frame.
     */
    void Update(float deltaTime) {}

    /**
     * @brief Rotates the camera based on mouse movement.
     * @param xOffset Horizontal mouse movement.
     * @param yOffset Vertical mouse movement.
     */
    void Rotate(float xOffset, float yOffset) {
        m_Yaw += xOffset * m_Sensitivity;
        m_Pitch -= yOffset * m_Sensitivity;
        if (m_Pitch > 89.0f) m_Pitch = 89.0f;
        if (m_Pitch < -89.0f) m_Pitch = -89.0f;
        UpdateVectors();
    }

    /**
     * @brief Moves the camera forward.
     * @param deltaTime Time elapsed since the last frame.
     */
    void MoveForward(float deltaTime) { m_Position += m_Front * m_MovementSpeed * deltaTime; }

    /**
     * @brief Moves the camera backward.
     * @param deltaTime Time elapsed since the last frame.
     */
    void MoveBackward(float deltaTime) { m_Position -= m_Front * m_MovementSpeed * deltaTime; }

    /**
     * @brief Moves the camera to the left.
     * @param deltaTime Time elapsed since the last frame.
     */
    void MoveLeft(float deltaTime) { m_Position -= m_Right * m_MovementSpeed * deltaTime; }

    /**
     * @brief Moves the camera to the right.
     * @param deltaTime Time elapsed since the last frame.
     */
    void MoveRight(float deltaTime) { m_Position += m_Right * m_MovementSpeed * deltaTime; }

    /**
     * @brief Moves the camera up.
     * @param deltaTime Time elapsed since the last frame.
     */
    void MoveUp(float deltaTime) { m_Position += m_WorldUp * m_MovementSpeed * deltaTime; }

    /**
     * @brief Moves the camera down.
     * @param deltaTime Time elapsed since the last frame.
     */
    void MoveDown(float deltaTime) { m_Position -= m_WorldUp * m_MovementSpeed * deltaTime; }

    /**
     * @brief Recalculates the front, right, and up vectors based on yaw and pitch.
     */
    void UpdateVectors() {
        glm::vec3 front;
        front.x = cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
        front.y = sin(glm::radians(m_Pitch));
        front.z = sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
        m_Front = glm::normalize(front);
        m_Right = glm::normalize(glm::cross(m_Front, m_WorldUp));
        m_Up = glm::normalize(glm::cross(m_Right, m_Front));
    }

    /**
     * @brief Gets the view matrix.
     * @return glm::mat4 representing the view transformation.
     */
    glm::mat4 GetViewMatrix() const {
        return glm::lookAt(m_Position, m_Position + m_Front, m_Up);
    }

    /**
     * @brief Gets the projection matrix (perspective or orthographic).
     * @param aspect The aspect ratio of the viewport.
     * @return glm::mat4 representing the projection.
     */
    glm::mat4 GetProjectionMatrix(float aspect) const {
        if (aspect <= 0.0f) aspect = 1.0f;
        float near = glm::max(m_Near, 0.01f);
        float far = glm::max(m_Far, near + 1.0f);

        if (m_ProjectionMode == ProjectionMode::Orthographic) {
            float halfH = m_OrthoSize;
            float halfW = halfH * aspect;
            glm::mat4 proj = glm::ortho(-halfW, halfW, -halfH, halfH, near, far);
            // Convert from OpenGL [-1,1] to reverse-Z [0,1] (1=near, 0=far)
            // to match the perspective path and SDL GPU conventions.
            proj[2][2] =  1.0f / (far - near);
            proj[3][2] =  far / (far - near);
            return proj;
        }

        glm::mat4 proj = glm::perspective(glm::radians(m_FOV), aspect, near, far);
        proj[2][2] = near / (far - near);
        proj[3][2] = (far * near) / (far - near);
        return proj;
    }

    /** @brief Sets the projection mode. */
    void SetProjectionMode(ProjectionMode mode) { m_ProjectionMode = mode; }
    /** @brief Returns the current projection mode. */
    ProjectionMode GetProjectionMode() const { return m_ProjectionMode; }

    glm::vec3 m_Position; /**< Current position in world space. */
    glm::vec3 m_Front;    /**< Forward direction vector. */
    glm::vec3 m_Up;       /**< Local up direction vector. */
    glm::vec3 m_Right;    /**< Local right direction vector. */
    glm::vec3 m_WorldUp = {0.0f, 1.0f, 0.0f}; /**< World up vector (constant). */

    float m_Yaw;          /**< Horizontal rotation angle in degrees. */
    float m_Pitch;        /**< Vertical rotation angle in degrees. */
    float m_FOV = 90.0f;  /**< Field of view in degrees (perspective only). */
    float m_Near = 0.5f;  /**< Near clipping plane distance. */
    float m_Far = 20000.0f; /**< Far clipping plane distance. */

    float m_MovementSpeed = 25.0f; /**< Movement speed units per second. */
    float m_Sensitivity = 0.1f;    /**< Mouse sensitivity factor. */

    ProjectionMode m_ProjectionMode = ProjectionMode::Perspective; /**< Current projection. */
    float m_OrthoSize = 30.f; /**< Half-height of orthographic frustum (building mode zoom). */
};
