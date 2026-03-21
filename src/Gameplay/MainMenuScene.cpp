#include "MainMenuScene.h"
#include "GameScene.h"
#include "../Networking/NetworkManager.h"

void MainMenuScene::OnEnter(SceneContext& ctx) {
    ctx.serverRegistry.clear();
    ctx.clientRegistry.clear();
}

void MainMenuScene::OnExit(SceneContext& ctx) {}

void MainMenuScene::FrameUpdate(SceneContext& ctx, float dt) {
    // Transition automatically when the network becomes active.
    // The Host/Join action itself comes from NetworkDebugUI.
    if (ctx.network.IsConnected()) {
        ctx.scenes.RequestTransition(new GameScene());
    }
}

void MainMenuScene::FixedUpdate(SceneContext& ctx, float dt) {}
