#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <spdlog/spdlog.h>

class AssetManager {
public:
    static void Init() {
        spdlog::info("AssetManager Initialized");
    }

    static void Shutdown() {
        spdlog::info("AssetManager Shutdown");
    }
};