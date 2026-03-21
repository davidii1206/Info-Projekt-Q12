#pragma once
#include "Scene.h"

class MainMenuScene : public IScene {
public:
    const char* Name() const override { return "MainMenuScene"; }
    void OnEnter(SceneContext& ctx) override;
    void OnExit(SceneContext& ctx)  override;
    void FrameUpdate(SceneContext& ctx, float dt) override;
    void FixedUpdate(SceneContext& ctx, float dt) override;
};
