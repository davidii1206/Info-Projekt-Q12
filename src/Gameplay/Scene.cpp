#include "Scene.h"
#include <spdlog/spdlog.h>

SceneManager::~SceneManager() {
    if (m_Current) delete m_Current;
    if (m_Next)    delete m_Next;
}

void SceneManager::RequestTransition(IScene* next) {
    if (m_Next) delete m_Next;
    m_Next = next;
}

void SceneManager::Flush(SceneContext& ctx) {
    if (m_Next) {
        if (m_Current) {
            m_Current->OnExit(ctx);
            delete m_Current;
        }
        m_Current = m_Next;
        m_Next = nullptr;
        
        if (m_Current) {
            m_Current->OnEnter(ctx);
            spdlog::info("SceneManager: transitioned to {}", m_Current->Name());
        }
    }
}

void SceneManager::LogicUpdate(SceneContext& ctx, float dt) {
    if (m_Current) m_Current->LogicUpdate(ctx, dt);
}

void SceneManager::UIUpdate(SceneContext& ctx, float dt) {
    if (m_Current) m_Current->UIUpdate(ctx, dt);
}

void SceneManager::FixedUpdate(SceneContext& ctx, float dt) {
    if (m_Current) m_Current->FixedUpdate(ctx, dt);
}

void SceneManager::Render(SceneContext& ctx, Renderer* renderer) {
    if (m_Current) m_Current->Render(ctx, renderer);
}

const char* SceneManager::GetName() const {
    return m_Current ? m_Current->Name() : "None";
}
