/**
 * @file Scene.cpp
 * @brief Implementation of scene management.
 */

#include "Scene.h"

/**
 * @brief Destructor that cleans up current and queued scenes.
 */
SceneManager::~SceneManager() {
    delete m_Current;
    delete m_Next;
}

/**
 * @brief Queues a scene transition.
 * @param next Pointer to the next scene.
 */
void SceneManager::RequestTransition(IScene* next) {
    delete m_Next; // discard any already-queued transition
    m_Next = next;
}

/**
 * @brief Flushes any pending scene transitions.
 * @param ctx The current scene context.
 */
void SceneManager::Flush(SceneContext& ctx) {
    if (!m_Next) return;
    if (m_Current) {
        m_Current->OnExit(ctx);
        delete m_Current;
    }
    m_Current = m_Next;
    m_Next    = nullptr;
    m_Current->OnEnter(ctx);
}

/**
 * @brief Updates the current scene's frame logic.
 * @param ctx The current scene context.
 * @param dt Delta time.
 */
void SceneManager::FrameUpdate(SceneContext& ctx, float dt) {
    if (m_Current) m_Current->FrameUpdate(ctx, dt);
}

/**
 * @brief Updates the current scene's fixed logic.
 * @param ctx The current scene context.
 * @param dt Fixed delta time.
 */
void SceneManager::FixedUpdate(SceneContext& ctx, float dt) {
    if (m_Current) m_Current->FixedUpdate(ctx, dt);
}

/**
 * @brief Renders the current scene.
 * @param ctx The current scene context.
 * @param renderer Pointer to the renderer.
 */
void SceneManager::Render(SceneContext& ctx, Renderer* renderer) {
    if (m_Current) m_Current->Render(ctx, renderer);
}

/**
 * @brief Gets the name of the current scene.
 * @return The name of the current scene.
 */
const char* SceneManager::GetName() const {
    return m_Current ? m_Current->Name() : "None";
}
