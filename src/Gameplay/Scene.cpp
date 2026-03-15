#include "Scene.h"

SceneManager::~SceneManager() {
    delete m_Current;
    delete m_Next;
}

void SceneManager::RequestTransition(IScene* next) {
    delete m_Next; // discard any already-queued transition
    m_Next = next;
}

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

void SceneManager::FrameUpdate(SceneContext& ctx, float dt) {
    if (m_Current) m_Current->FrameUpdate(ctx, dt);
}

void SceneManager::FixedUpdate(SceneContext& ctx, float dt) {
    if (m_Current) m_Current->FixedUpdate(ctx, dt);
}

const char* SceneManager::GetName() const {
    return m_Current ? m_Current->Name() : "None";
}
