/**
 * @file ScatterSystem.cpp
 * @brief Implementation of biome-aware decorative prop scattering.
 */

#define GLM_ENABLE_EXPERIMENTAL
#include "ScatterSystem.h"
#include "Components.h"
#include "../Core/WorldManager.h"

#include <PerlinNoise.hpp>
#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>
#include <random>
#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------
// Default placeholder layer set
// ---------------------------------------------------------------------------

ScatterConfig ScatterConfig::Default(uint32_t seed) {
    ScatterConfig cfg;
    cfg.seed = seed;

    // Helper to keep the layer list readable.
    auto layer = [](const char* name, const char* modelPath, std::vector<BugClass> biomes,
                    float density, float minS, float maxS,
                    float clumpScale, float clumpThresh,
                    bool alignSlope) {
        ScatterLayer l;
        l.name = name;
        l.modelPath = modelPath;
        l.biomes = std::move(biomes);
        l.density = density;
        l.minScale = minS;
        l.maxScale = maxS;
        l.clumpNoiseScale = clumpScale;
        l.clumpThreshold = clumpThresh;
        l.alignToSlope = alignSlope;
        return l;
    };

    // -----------------------------------------------------------------------
    // Global litter — low density, all biomes. Listed first so rare items
    // scatter everywhere even inside dense biomes.
    // -----------------------------------------------------------------------
    {
        auto l = layer("rock_moss", "assets/Rock_Moss_1.glb", {}, 0.05f, 2.4f, 7.2f, 0.04f, 0.0f, true);
        l.maxSlope = 0.9f; l.yOffset = -0.05f;
        l.scaleXZJitter = 0.25f; l.scaleYJitter = 0.30f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("wood_log", "assets/WoodLog.glb", {}, 0.03f, 0.8f, 2.0f, 0.05f, 0.0f, false);
        l.maxSlope = 0.3f;
        l.scaleXZJitter = 0.15f; l.scaleYJitter = 0.20f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("stump", "assets/TreeStump.glb", {}, 0.04f, 1.0f, 3.0f, 0.05f, 0.0f, false);
        l.maxSlope = 0.35f; l.yOffset = -0.05f;
        l.scaleXZJitter = 0.20f; l.scaleYJitter = 0.25f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("stump_moss", "assets/TreeStump_Moss.glb",
            {BugClass::Snails, BugClass::MosquitosTicks, BugClass::Dragonflies, BugClass::Fireflies, BugClass::Mantis},
            0.06f, 0.8f, 2.5f, 0.05f, 0.0f, false);
        l.maxSlope = 0.35f; l.yOffset = -0.05f;
        l.scaleXZJitter = 0.20f; l.scaleYJitter = 0.25f;
        cfg.layers.push_back(l);
    }

    // -----------------------------------------------------------------------
    // Large rock formations / boulders — the space-filling feature for
    // biomes that don't have tree canopies.  Same Rock_Moss assets as the
    // global litter but at 5-12× the scale so they read as genuine boulders
    // and monoliths that break sightlines the way trees do in forest biomes.
    // Listed early so they get first pick at candidates in their biomes.
    // -----------------------------------------------------------------------
    {   // Beetles: dark angular boulders — the biome's signature landmark
        auto l = layer("boulder_beetle", "assets/Rock_Moss_3.glb", {BugClass::Beetles},
            0.18f, 3.0f, 7.0f, 0.05f, 0.0f, true);
        l.maxSlope = 0.65f; l.yOffset = -0.12f;
        l.scaleXZJitter = 0.30f; l.scaleYJitter = 0.40f;
        cfg.layers.push_back(l);
    }
    {   // Beetles secondary: small mossy rocks for variety
        auto l = layer("boulder_beetle_b", "assets/Rock_Moss_1.glb", {BugClass::Beetles},
            0.14f, 2.0f, 5.0f, 0.06f, 0.0f, true);
        l.maxSlope = 0.65f; l.yOffset = -0.10f;
        l.scaleXZJitter = 0.28f; l.scaleYJitter = 0.35f;
        cfg.layers.push_back(l);
    }
    {   // Spiders: mossy boulders — draped, lurking shapes
        auto l = layer("boulder_spider", "assets/Rock_Moss_2.glb", {BugClass::Spiders},
            0.14f, 2.5f, 6.0f, 0.06f, 0.0f, true);
        l.maxSlope = 0.60f; l.yOffset = -0.10f;
        l.scaleXZJitter = 0.28f; l.scaleYJitter = 0.35f;
        cfg.layers.push_back(l);
    }
    {   // Roaches: crumbling weathered rocks, overgrown
        auto l = layer("boulder_roach", "assets/Rock_Moss_1.glb", {BugClass::Roaches},
            0.13f, 2.5f, 5.5f, 0.06f, 0.0f, true);
        l.maxSlope = 0.60f; l.yOffset = -0.10f;
        l.scaleXZJitter = 0.30f; l.scaleYJitter = 0.35f;
        cfg.layers.push_back(l);
    }
    {   // CentipedesWorms: subterranean rocks pushing up through the floor
        auto l = layer("boulder_cent", "assets/Rock_Moss_3.glb", {BugClass::CentipedesWorms},
            0.15f, 2.5f, 6.0f, 0.06f, 0.0f, true);
        l.maxSlope = 0.60f; l.yOffset = -0.10f;
        l.scaleXZJitter = 0.28f; l.scaleYJitter = 0.38f;
        cfg.layers.push_back(l);
    }
    {   // BossArena: tall monolithic stone pillars — imposing arena landmarks
        auto l = layer("monolith_boss_a", "assets/Rock_Moss_3.glb", {BugClass::BossArena},
            0.14f, 4.0f, 10.0f, 0.05f, 0.0f, true);
        l.maxSlope = 0.55f; l.yOffset = -0.12f;
        l.scaleXZJitter = 0.20f; l.scaleYJitter = 0.50f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("monolith_boss_b", "assets/Rock_Moss_1.glb", {BugClass::BossArena},
            0.11f, 2.5f, 6.0f, 0.05f, 0.0f, true);
        l.maxSlope = 0.55f; l.yOffset = -0.10f;
        l.scaleXZJitter = 0.25f; l.scaleYJitter = 0.40f;
        cfg.layers.push_back(l);
    }
    {   // Desert: sandstone rock formations for both scorpion and ant biomes
        auto l = layer("boulder_desert_a", "assets/Rock_Moss_3.glb",
            {BugClass::Scorpions, BugClass::Ants},
            0.16f, 2.5f, 6.0f, 0.05f, 0.0f, true);
        l.maxSlope = 0.55f; l.yOffset = -0.10f;
        l.scaleXZJitter = 0.28f; l.scaleYJitter = 0.38f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("boulder_desert_b", "assets/Rock_Moss_1.glb",
            {BugClass::Scorpions, BugClass::Ants},
            0.10f, 1.8f, 4.5f, 0.06f, 0.0f, true);
        l.maxSlope = 0.55f; l.yOffset = -0.08f;
        l.scaleXZJitter = 0.30f; l.scaleYJitter = 0.35f;
        cfg.layers.push_back(l);
    }

    // -----------------------------------------------------------------------
    // Dense dark forest — Mantis (assassin order) + Snails (wet forest)
    // Wide-crown oaks via scaleBias; Mantis gets Willow_4 for visual variety.
    // -----------------------------------------------------------------------
    {
        auto l = layer("oak_dense", "assets/Willow_1.glb",
            {BugClass::Mantis, BugClass::Snails},
            0.65f, 2.5f, 5.5f, 0.07f, 0.0f, false);
        l.maxSlope = 0.25f; l.cliffBuffer = 2;
        l.scaleBias = {1.6f, 0.72f, 1.6f};
        l.scaleXZJitter = 0.22f; l.scaleYJitter = 0.30f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("oak_dense_2", "assets/Willow_3.glb",
            {BugClass::Mantis, BugClass::Snails},
            0.40f, 2.2f, 4.8f, 0.07f, 0.0f, false);
        l.maxSlope = 0.25f; l.cliffBuffer = 2;
        l.scaleBias = {1.5f, 0.68f, 1.5f};
        l.scaleXZJitter = 0.22f; l.scaleYJitter = 0.30f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("oak_dense_3", "assets/Willow_4.glb",
            {BugClass::Mantis},
            0.30f, 2.4f, 5.2f, 0.07f, 0.0f, false);
        l.maxSlope = 0.25f; l.cliffBuffer = 2;
        l.scaleXZJitter = 0.20f; l.scaleYJitter = 0.28f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("log_moss", "assets/WoodLog_Moss.glb",
            {BugClass::Mantis, BugClass::Snails},
            0.25f, 1.0f, 2.2f, 0.06f, 0.0f, false);
        l.maxSlope = 0.3f;
        l.scaleXZJitter = 0.15f; l.scaleYJitter = 0.20f;
        cfg.layers.push_back(l);
    }

    // -----------------------------------------------------------------------
    // Common deciduous trees — mixed with oaks in Mantis/Snails; primary
    // canopy for ButterfliesMoths (open meadow feel with broad crowns).
    // -----------------------------------------------------------------------
    const std::vector<BugClass> kCommonForestBiomes = {
        BugClass::Mantis, BugClass::Snails, BugClass::ButterfliesMoths
    };
    {
        auto l = layer("common_1", "assets/CommonTree_1.glb", kCommonForestBiomes,
            0.45f, 2.0f, 5.0f, 0.07f, 0.0f, false);
        l.maxSlope = 0.25f; l.cliffBuffer = 2;
        l.scaleXZJitter = 0.20f; l.scaleYJitter = 0.30f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("common_2", "assets/CommonTree_2.glb", kCommonForestBiomes,
            0.40f, 2.2f, 5.5f, 0.07f, 0.0f, false);
        l.maxSlope = 0.25f; l.cliffBuffer = 2;
        l.scaleXZJitter = 0.20f; l.scaleYJitter = 0.30f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("common_3", "assets/CommonTree_3.glb", kCommonForestBiomes,
            0.35f, 1.8f, 4.8f, 0.07f, 0.0f, false);
        l.maxSlope = 0.25f; l.cliffBuffer = 2;
        l.scaleXZJitter = 0.22f; l.scaleYJitter = 0.28f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("common_4", "assets/CommonTree_4.glb", kCommonForestBiomes,
            0.32f, 2.0f, 5.2f, 0.07f, 0.0f, false);
        l.maxSlope = 0.25f; l.cliffBuffer = 2;
        l.scaleXZJitter = 0.20f; l.scaleYJitter = 0.30f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("common_5", "assets/CommonTree_5.glb", kCommonForestBiomes,
            0.28f, 2.4f, 5.8f, 0.07f, 0.0f, false);
        l.maxSlope = 0.25f; l.cliffBuffer = 2;
        l.scaleXZJitter = 0.18f; l.scaleYJitter = 0.32f;
        cfg.layers.push_back(l);
    }

    // -----------------------------------------------------------------------
    // Pine forest — BeesWasps (stinger alliance, sunny mountain feel).
    // Narrow scaleBias makes pines clearly taller and thinner than oaks.
    // Six variants cascade so each candidate gets a different tree shape.
    // -----------------------------------------------------------------------
    {
        auto l = layer("pine_1", "assets/PineTree_1.glb",
            {BugClass::BeesWasps},
            0.50f, 2.0f, 5.0f, 0.07f, 0.0f, false);
        l.maxSlope = 0.28f; l.cliffBuffer = 2;
        l.scaleBias = {0.75f, 1.2f, 0.75f};
        l.scaleXZJitter = 0.18f; l.scaleYJitter = 0.35f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("pine_2", "assets/PineTree_2.glb",
            {BugClass::BeesWasps},
            0.45f, 2.5f, 6.0f, 0.07f, 0.0f, false);
        l.maxSlope = 0.28f; l.cliffBuffer = 2;
        l.scaleBias = {0.75f, 1.2f, 0.75f};
        l.scaleXZJitter = 0.18f; l.scaleYJitter = 0.35f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("pine_3", "assets/PineTree_3.glb",
            {BugClass::BeesWasps},
            0.40f, 2.0f, 5.5f, 0.07f, 0.0f, false);
        l.maxSlope = 0.28f; l.cliffBuffer = 2;
        l.scaleBias = {0.75f, 1.2f, 0.75f};
        l.scaleXZJitter = 0.18f; l.scaleYJitter = 0.35f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("pine_4", "assets/PineTree_4.glb",
            {BugClass::BeesWasps},
            0.35f, 1.8f, 4.5f, 0.07f, 0.0f, false);
        l.maxSlope = 0.28f; l.cliffBuffer = 2;
        l.scaleBias = {0.75f, 1.2f, 0.75f};
        l.scaleXZJitter = 0.18f; l.scaleYJitter = 0.35f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("pine_5", "assets/PineTree_5.glb",
            {BugClass::BeesWasps},
            0.30f, 2.0f, 5.0f, 0.07f, 0.0f, false);
        l.maxSlope = 0.28f; l.cliffBuffer = 2;
        l.scaleBias = {0.75f, 1.2f, 0.75f};
        l.scaleXZJitter = 0.18f; l.scaleYJitter = 0.35f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("pine_tall", "assets/PineTree_Tall_1.glb",
            {BugClass::BeesWasps},
            0.45f, 3.0f, 7.0f, 0.07f, 0.0f, false);
        l.maxSlope = 0.28f; l.cliffBuffer = 2;
        l.scaleXZJitter = 0.15f; l.scaleYJitter = 0.30f;
        cfg.layers.push_back(l);
    }

    // -----------------------------------------------------------------------
    // Light airy canopy — ButterfliesMoths (mostly open, few sparse trees).
    // Very low density so most of the biome reads as open meadow.
    // -----------------------------------------------------------------------
    {
        auto l = layer("willow_bfly", "assets/Willow_3.glb",
            {BugClass::ButterfliesMoths},
            0.55f, 2.2f, 4.4f, 0.07f, 0.0f, false);
        l.maxSlope = 0.25f; l.cliffBuffer = 2;
        l.scaleBias = {1.5f, 0.68f, 1.5f};
        l.scaleXZJitter = 0.22f; l.scaleYJitter = 0.28f;
        cfg.layers.push_back(l);
    }

    // -----------------------------------------------------------------------
    // Swamp biomes — three distinct sub-feels:
    //   MosquitosTicks: dense plague swamp, packed willows + large canopy
    //   Dragonflies: open hunting water, large willows, spacious
    //   Fireflies: eerie sparse clearing, bare dead trees among willows
    // -----------------------------------------------------------------------
    {
        auto l = layer("willow_dense", "assets/Willow_2.glb",
            {BugClass::MosquitosTicks},
            0.60f, 2.2f, 5.5f, 0.08f, 0.0f, false);
        l.maxSlope = 0.2f; l.cliffBuffer = 2;
        l.scaleXZJitter = 0.20f; l.scaleYJitter = 0.28f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("willow_4_swamp", "assets/Willow_4.glb",
            {BugClass::MosquitosTicks},
            0.25f, 2.2f, 4.8f, 0.08f, 0.0f, false);
        l.maxSlope = 0.2f; l.cliffBuffer = 2;
        l.scaleXZJitter = 0.20f; l.scaleYJitter = 0.28f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("willow_large", "assets/Willow_5.glb",
            {BugClass::MosquitosTicks, BugClass::Dragonflies},
            0.30f, 3.0f, 6.5f, 0.09f, 0.0f, false);
        l.maxSlope = 0.2f; l.cliffBuffer = 2;
        l.scaleXZJitter = 0.18f; l.scaleYJitter = 0.25f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("willow_sparse", "assets/Willow_2.glb",
            {BugClass::Dragonflies},
            0.28f, 2.2f, 5.2f, 0.08f, 0.0f, false);
        l.maxSlope = 0.2f; l.cliffBuffer = 2;
        l.scaleXZJitter = 0.20f; l.scaleYJitter = 0.28f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("willow_firefly", "assets/Willow_4.glb",
            {BugClass::Fireflies},
            0.50f, 2.2f, 4.4f, 0.08f, 0.0f, false);
        l.maxSlope = 0.22f; l.cliffBuffer = 2;
        l.scaleXZJitter = 0.22f; l.scaleYJitter = 0.30f;
        cfg.layers.push_back(l);
    }
    {   // Bare dead trees give Fireflies their eerie glow-in-the-dark look
        auto l = layer("dead_firefly", "assets/BirchTree_Dead_1.glb",
            {BugClass::Fireflies},
            0.55f, 2.0f, 4.0f, 0.07f, 0.0f, false);
        l.maxSlope = 0.25f; l.cliffBuffer = 2;
        l.scaleXZJitter = 0.18f; l.scaleYJitter = 0.28f;
        cfg.layers.push_back(l);
    }

    // -----------------------------------------------------------------------
    // CommonTree (dead variants) — used in all dark/decay biomes and boss
    // arenas for additional silhouette variety beyond BirchTree_Dead.
    // Boss arenas get trees here since no other layer targets BossArena.
    // -----------------------------------------------------------------------
    const std::vector<BugClass> kDeadBiomes = {
        BugClass::Spiders, BugClass::Roaches, BugClass::Beetles,
        BugClass::CentipedesWorms, BugClass::BossArena, BugClass::Fireflies
    };
    {
        auto l = layer("common_dead_1", "assets/CommonTree_Dead_1.glb", kDeadBiomes,
            0.50f, 1.8f, 4.2f, 0.07f, 0.0f, false);
        l.maxSlope = 0.25f; l.cliffBuffer = 2;
        l.scaleXZJitter = 0.18f; l.scaleYJitter = 0.28f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("common_dead_2", "assets/CommonTree_Dead_2.glb", kDeadBiomes,
            0.45f, 2.0f, 4.5f, 0.07f, 0.0f, false);
        l.maxSlope = 0.25f; l.cliffBuffer = 2;
        l.scaleXZJitter = 0.20f; l.scaleYJitter = 0.30f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("common_dead_3", "assets/CommonTree_Dead_3.glb", kDeadBiomes,
            0.40f, 1.6f, 4.0f, 0.07f, 0.0f, false);
        l.maxSlope = 0.25f; l.cliffBuffer = 2;
        l.scaleXZJitter = 0.18f; l.scaleYJitter = 0.28f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("common_dead_4", "assets/CommonTree_Dead_4.glb", kDeadBiomes,
            0.35f, 2.0f, 4.8f, 0.07f, 0.0f, false);
        l.maxSlope = 0.25f; l.cliffBuffer = 2;
        l.scaleXZJitter = 0.20f; l.scaleYJitter = 0.30f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("common_dead_5", "assets/CommonTree_Dead_5.glb", kDeadBiomes,
            0.30f, 1.8f, 4.5f, 0.07f, 0.0f, false);
        l.maxSlope = 0.25f; l.cliffBuffer = 2;
        l.scaleXZJitter = 0.18f; l.scaleYJitter = 0.28f;
        cfg.layers.push_back(l);
    }

    // -----------------------------------------------------------------------
    // Wet rocks — Snails + dense swamp biomes
    // -----------------------------------------------------------------------
    {
        auto l = layer("rock_moss_wet", "assets/Rock_Moss_2.glb",
            {BugClass::Snails, BugClass::MosquitosTicks, BugClass::Dragonflies},
            0.20f, 2.8f, 8.0f, 0.05f, 0.0f, true);
        l.maxSlope = 0.8f; l.yOffset = -0.05f;
        l.scaleXZJitter = 0.25f; l.scaleYJitter = 0.30f;
        cfg.layers.push_back(l);
    }

    // -----------------------------------------------------------------------
    // Birch forest — Woodlice (dense uniform), Termites (builders: more logs)
    // -----------------------------------------------------------------------
    {
        auto l = layer("birch_1", "assets/BirchTree_1.glb",
            {BugClass::Woodlice, BugClass::Termites},
            0.55f, 2.0f, 4.5f, 0.07f, 0.0f, false);
        l.maxSlope = 0.25f; l.cliffBuffer = 2;
        l.scaleXZJitter = 0.18f; l.scaleYJitter = 0.32f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("birch_2", "assets/BirchTree_2.glb",
            {BugClass::Woodlice, BugClass::Termites},
            0.50f, 1.8f, 4.0f, 0.07f, 0.0f, false);
        l.maxSlope = 0.25f; l.cliffBuffer = 2;
        l.scaleXZJitter = 0.18f; l.scaleYJitter = 0.32f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("birch_3", "assets/BirchTree_3.glb",
            {BugClass::Woodlice, BugClass::Termites},
            0.48f, 2.2f, 4.5f, 0.07f, 0.0f, false);
        l.maxSlope = 0.25f; l.cliffBuffer = 2;
        l.scaleXZJitter = 0.18f; l.scaleYJitter = 0.32f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("birch_4", "assets/BirchTree_4.glb",
            {BugClass::Woodlice},
            0.48f, 1.6f, 3.8f, 0.07f, 0.0f, false);
        l.maxSlope = 0.25f; l.cliffBuffer = 2;
        l.scaleXZJitter = 0.18f; l.scaleYJitter = 0.32f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("birch_5", "assets/BirchTree_5.glb",
            {BugClass::Woodlice, BugClass::Termites},
            0.45f, 2.0f, 4.5f, 0.07f, 0.0f, false);
        l.maxSlope = 0.25f; l.cliffBuffer = 2;
        l.scaleXZJitter = 0.18f; l.scaleYJitter = 0.32f;
        cfg.layers.push_back(l);
    }
    {   // Termites = builders; give them noticeably more fallen logs
        auto l = layer("log_birch", "assets/WoodLog.glb",
            {BugClass::Termites},
            0.30f, 0.8f, 1.8f, 0.05f, 0.0f, false);
        l.maxSlope = 0.3f;
        l.scaleXZJitter = 0.15f; l.scaleYJitter = 0.20f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("log_birch_moss", "assets/WoodLog_Moss.glb",
            {BugClass::Termites},
            0.20f, 0.8f, 1.6f, 0.05f, 0.0f, false);
        l.maxSlope = 0.3f;
        l.scaleXZJitter = 0.15f; l.scaleYJitter = 0.20f;
        cfg.layers.push_back(l);
    }

    // -----------------------------------------------------------------------
    // Dead / dark biomes — each has its own sub-character:
    //   Spiders:        dense dead trees + mushrooms (silk-draped forest)
    //   Roaches:        rotting decay — dead trees + mossy logs
    //   Beetles:        rocky charcoal — hard rocks + stout dead trees
    //   CentipedesWorms: underground feel — mushrooms dominant, sparse trees
    //   Bugs:           toxic wasteland — ALL mushroom types at high density
    // -----------------------------------------------------------------------

    // Spiders: dead tree canopy
    {
        auto l = layer("dead_spider_1", "assets/BirchTree_Dead_1.glb",
            {BugClass::Spiders},
            0.70f, 2.0f, 4.4f, 0.07f, 0.0f, false);
        l.maxSlope = 0.25f; l.cliffBuffer = 2;
        l.scaleXZJitter = 0.18f; l.scaleYJitter = 0.28f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("dead_spider_2", "assets/BirchTree_Dead_3.glb",
            {BugClass::Spiders},
            0.60f, 1.8f, 4.0f, 0.07f, 0.0f, false);
        l.maxSlope = 0.25f; l.cliffBuffer = 2;
        l.scaleXZJitter = 0.18f; l.scaleYJitter = 0.28f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("dead_spider_3", "assets/BirchTree_Dead_5.glb",
            {BugClass::Spiders},
            0.50f, 2.0f, 3.8f, 0.07f, 0.0f, false);
        l.maxSlope = 0.25f; l.cliffBuffer = 2;
        l.scaleXZJitter = 0.18f; l.scaleYJitter = 0.28f;
        cfg.layers.push_back(l);
    }

    // Roaches: rotting decay
    {
        auto l = layer("dead_roach_1", "assets/BirchTree_Dead_3.glb",
            {BugClass::Roaches},
            0.65f, 2.0f, 4.2f, 0.07f, 0.0f, false);
        l.maxSlope = 0.25f; l.cliffBuffer = 2;
        l.scaleXZJitter = 0.20f; l.scaleYJitter = 0.30f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("dead_roach_2", "assets/BirchTree_Dead_4.glb",
            {BugClass::Roaches},
            0.55f, 1.8f, 4.0f, 0.07f, 0.0f, false);
        l.maxSlope = 0.25f; l.cliffBuffer = 2;
        l.scaleXZJitter = 0.20f; l.scaleYJitter = 0.30f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("dead_roach_3", "assets/BirchTree_Dead_5.glb",
            {BugClass::Roaches},
            0.45f, 1.8f, 3.6f, 0.07f, 0.0f, false);
        l.maxSlope = 0.25f; l.cliffBuffer = 2;
        l.scaleXZJitter = 0.20f; l.scaleYJitter = 0.30f;
        cfg.layers.push_back(l);
    }
    {   // Mossy rotting logs — signature of the Roach decay biome
        auto l = layer("roach_log", "assets/WoodLog_Moss.glb",
            {BugClass::Roaches},
            0.30f, 0.8f, 1.8f, 0.05f, 0.0f, false);
        l.maxSlope = 0.3f;
        l.scaleXZJitter = 0.15f; l.scaleYJitter = 0.20f;
        cfg.layers.push_back(l);
    }

    // Beetles: rocky + sturdy dead trees
    {
        auto l = layer("dead_beetle_1", "assets/BirchTree_Dead_1.glb",
            {BugClass::Beetles},
            0.60f, 2.0f, 4.4f, 0.07f, 0.0f, false);
        l.maxSlope = 0.25f; l.cliffBuffer = 2;
        l.scaleXZJitter = 0.18f; l.scaleYJitter = 0.28f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("dead_beetle_2", "assets/BirchTree_Dead_4.glb",
            {BugClass::Beetles},
            0.50f, 1.8f, 4.0f, 0.07f, 0.0f, false);
        l.maxSlope = 0.25f; l.cliffBuffer = 2;
        l.scaleXZJitter = 0.18f; l.scaleYJitter = 0.28f;
        cfg.layers.push_back(l);
    }
    {   // Dark angular rocks are the Beetle biome's signature
        auto l = layer("rock_dark", "assets/Rock_Moss_3.glb",
            {BugClass::Beetles, BugClass::CentipedesWorms},
            0.35f, 2.4f, 7.0f, 0.05f, 0.0f, true);
        l.maxSlope = 0.8f; l.yOffset = -0.05f;
        l.scaleXZJitter = 0.25f; l.scaleYJitter = 0.30f;
        cfg.layers.push_back(l);
    }

    // CentipedesWorms: sparse dead trees, then mushrooms dominate
    {
        auto l = layer("dead_cent_1", "assets/BirchTree_Dead_2.glb",
            {BugClass::CentipedesWorms},
            0.45f, 1.8f, 3.8f, 0.07f, 0.0f, false);
        l.maxSlope = 0.25f; l.cliffBuffer = 2;
        l.scaleXZJitter = 0.18f; l.scaleYJitter = 0.28f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("dead_cent_2", "assets/BirchTree_Dead_3.glb",
            {BugClass::CentipedesWorms},
            0.38f, 2.0f, 3.6f, 0.07f, 0.0f, false);
        l.maxSlope = 0.25f; l.cliffBuffer = 2;
        l.scaleXZJitter = 0.18f; l.scaleYJitter = 0.28f;
        cfg.layers.push_back(l);
    }

    // Mushrooms — split by biome so each dark biome feels different:
    //   Spiders: a few mushrooms among the dead trees
    //   CentipedesWorms: mushrooms dominate (underground world)
    //   Bugs: ALL four mushroom types at high density (toxic wasteland)
    {
        auto l = layer("mushroom_spider", "assets/Mushroom_1.glb",
            {BugClass::Spiders},
            0.12f, 6.0f, 16.8f, 0.06f, 0.0f, false);
        l.maxSlope = 0.35f;
        l.scaleXZJitter = 0.25f; l.scaleYJitter = 0.35f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("mushroom_cent_a", "assets/Mushroom_3.glb",
            {BugClass::CentipedesWorms},
            0.22f, 14.0f, 44.0f, 0.08f, 0.0f, false);
        l.maxSlope = 0.3f;
        l.scaleXZJitter = 0.20f; l.scaleYJitter = 0.40f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("mushroom_cent_b", "assets/Mushroom_4.glb",
            {BugClass::CentipedesWorms},
            0.18f, 10.0f, 36.0f, 0.07f, 0.0f, false);
        l.maxSlope = 0.35f;
        l.scaleXZJitter = 0.22f; l.scaleYJitter = 0.38f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("mushroom_bug_a", "assets/Mushroom_1.glb",
            {BugClass::Bugs},
            0.25f, 8.0f, 28.0f, 0.06f, 0.0f, false);
        l.maxSlope = 0.35f;
        l.scaleXZJitter = 0.20f; l.scaleYJitter = 0.40f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("mushroom_bug_b", "assets/Mushroom_2.glb",
            {BugClass::Bugs},
            0.22f, 10.0f, 34.0f, 0.07f, 0.0f, false);
        l.maxSlope = 0.35f;
        l.scaleXZJitter = 0.20f; l.scaleYJitter = 0.40f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("mushroom_bug_c", "assets/Mushroom_3.glb",
            {BugClass::Bugs},
            0.20f, 12.0f, 40.0f, 0.08f, 0.0f, false);
        l.maxSlope = 0.3f;
        l.scaleXZJitter = 0.18f; l.scaleYJitter = 0.42f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("mushroom_bug_d", "assets/Mushroom_4.glb",
            {BugClass::Bugs},
            0.18f, 10.0f, 36.0f, 0.07f, 0.0f, false);
        l.maxSlope = 0.35f;
        l.scaleXZJitter = 0.20f; l.scaleYJitter = 0.40f;
        cfg.layers.push_back(l);
    }

    // -----------------------------------------------------------------------
    // Desert biomes:
    //   Scorpions: large varied cacti (Cactus_1+3+4) — imposing cactus field
    //   Ants:      scrubland — smaller cacti + sparse dead trees + rocks
    // -----------------------------------------------------------------------
    {
        auto l = layer("cactus_1", "assets/Cactus_1.glb",
            {BugClass::Scorpions},
            0.45f, 4.0f, 9.0f, 0.06f, 0.0f, false);
        l.maxSlope = 0.4f;
        l.scaleXZJitter = 0.12f; l.scaleYJitter = 0.22f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("cactus_3", "assets/Cactus_3.glb",
            {BugClass::Scorpions},
            0.30f, 3.5f, 8.0f, 0.06f, 0.0f, false);
        l.maxSlope = 0.4f;
        l.scaleXZJitter = 0.12f; l.scaleYJitter = 0.22f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("cactus_4", "assets/Cactus_4.glb",
            {BugClass::Scorpions},
            0.22f, 3.0f, 7.0f, 0.07f, 0.0f, false);
        l.maxSlope = 0.4f;
        l.scaleXZJitter = 0.12f; l.scaleYJitter = 0.22f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("cactus_small", "assets/Cactus_2.glb",
            {BugClass::Scorpions, BugClass::Ants},
            0.35f, 2.0f, 5.0f, 0.07f, 0.0f, false);
        l.maxSlope = 0.5f;
        l.scaleXZJitter = 0.15f; l.scaleYJitter = 0.20f;
        cfg.layers.push_back(l);
    }
    {   // Ants get the no-bloom variant for a scrubland / dead-bush feel
        auto l = layer("cactus_scrub", "assets/Cactus_4_no_bloom.glb",
            {BugClass::Ants},
            0.25f, 1.5f, 4.0f, 0.07f, 0.0f, false);
        l.maxSlope = 0.5f;
        l.scaleXZJitter = 0.15f; l.scaleYJitter = 0.20f;
        cfg.layers.push_back(l);
    }
    {   // Bleached dead trees in the scorpion biome
        auto l = layer("desert_dead", "assets/BirchTree_Dead_4.glb",
            {BugClass::Scorpions},
            0.40f, 1.8f, 3.6f, 0.08f, 0.0f, false);
        l.maxSlope = 0.3f; l.cliffBuffer = 2;
        l.scaleXZJitter = 0.18f; l.scaleYJitter = 0.28f;
        cfg.layers.push_back(l);
    }
    {   // Rocky desert ground in the ant colony
        auto l = layer("ants_rock", "assets/Rock_Moss_3.glb",
            {BugClass::Ants},
            0.28f, 2.4f, 6.0f, 0.05f, 0.0f, true);
        l.maxSlope = 0.8f; l.yOffset = -0.05f;
        l.scaleXZJitter = 0.25f; l.scaleYJitter = 0.30f;
        cfg.layers.push_back(l);
    }

    // -----------------------------------------------------------------------
    // Plants — small shrubs/flowering plants in green and meadow biomes.
    // Listed before grass so they get priority at each candidate over grass.
    // -----------------------------------------------------------------------
    const std::vector<BugClass> kPlantBiomes = {
        BugClass::Mantis, BugClass::Snails, BugClass::BeesWasps,
        BugClass::ButterfliesMoths, BugClass::Dragonflies, BugClass::Woodlice
    };
    {
        auto l = layer("plant_1", "assets/Plant_1.glb", kPlantBiomes,
            0.30f, 0.4f, 1.4f, 0.06f, 0.0f, false);
        l.maxSlope = 0.4f;
        l.scaleXZJitter = 0.20f; l.scaleYJitter = 0.30f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("plant_2", "assets/Plant_2.glb", kPlantBiomes,
            0.28f, 0.3f, 1.2f, 0.06f, 0.0f, false);
        l.maxSlope = 0.4f;
        l.scaleXZJitter = 0.20f; l.scaleYJitter = 0.30f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("plant_3", "assets/Plant_3.glb", kPlantBiomes,
            0.28f, 0.4f, 1.5f, 0.06f, 0.0f, false);
        l.maxSlope = 0.4f;
        l.scaleXZJitter = 0.20f; l.scaleYJitter = 0.30f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("plant_4", "assets/Plant_4.glb", kPlantBiomes,
            0.25f, 0.3f, 1.3f, 0.06f, 0.0f, false);
        l.maxSlope = 0.4f;
        l.scaleXZJitter = 0.22f; l.scaleYJitter = 0.28f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("plant_5", "assets/Plant_5.glb", kPlantBiomes,
            0.25f, 0.5f, 1.6f, 0.06f, 0.0f, false);
        l.maxSlope = 0.4f;
        l.scaleXZJitter = 0.20f; l.scaleYJitter = 0.30f;
        cfg.layers.push_back(l);
    }

    // -----------------------------------------------------------------------
    // Grass — four variants in all green/wet/forest biomes.
    // Desert and all dark biomes excluded (sand/dark ground, no grass).
    // Listed last so trees take priority at each candidate position.
    // -----------------------------------------------------------------------
    const std::vector<BugClass> kGrassBiomes = {
        BugClass::Mantis, BugClass::Snails,
        BugClass::BeesWasps, BugClass::ButterfliesMoths,
        BugClass::MosquitosTicks, BugClass::Dragonflies, BugClass::Fireflies,
        BugClass::Woodlice, BugClass::Termites, BugClass::Bugs
    };
    {
        auto l = layer("grass_1", "assets/grass_1.glb", kGrassBiomes,
            0.75f, 0.008f, 0.030f, 0.0f, 0.0f, false);
        l.maxSlope = 0.4f;
        l.scaleXZJitter = 0.20f; l.scaleYJitter = 0.30f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("grass_2", "assets/grass_2.glb", kGrassBiomes,
            0.80f, 0.006f, 0.025f, 0.0f, 0.0f, false);
        l.maxSlope = 0.4f;
        l.scaleXZJitter = 0.20f; l.scaleYJitter = 0.30f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("grass_3", "assets/grass_3.glb", kGrassBiomes,
            0.75f, 0.010f, 0.032f, 0.0f, 0.0f, false);
        l.maxSlope = 0.4f;
        l.scaleXZJitter = 0.20f; l.scaleYJitter = 0.30f;
        cfg.layers.push_back(l);
    }
    {
        auto l = layer("grass_4", "assets/grass_4.glb", kGrassBiomes,
            0.80f, 0.007f, 0.028f, 0.0f, 0.0f, false);
        l.maxSlope = 0.4f;
        l.scaleXZJitter = 0.20f; l.scaleYJitter = 0.30f;
        cfg.layers.push_back(l);
    }

    // -----------------------------------------------------------------------
    // Crops — growth stages _1 (sprout) → _4 (full), plus _Crop where it
    // exists.  Density decreases as the plant matures so most instances are
    // young; fully-grown ones read as landmarks.
    // -----------------------------------------------------------------------

    // Bamboo — Dragonflies + MosquitosTicks (riverside, swamp)
    // Thin tall columns; scaleBias narrows XZ so they read as canes.
    {
        const std::vector<BugClass> kBamboo = {BugClass::Dragonflies, BugClass::MosquitosTicks};
        const struct { const char* n; const char* p; float dens; float mn; float mx; float bx; float by; } rows[] = {
            { "bamboo_1", "assets/Bamboo_1.glb", 0.35f, 0.3f, 0.7f, 0.6f, 1.0f },
            { "bamboo_2", "assets/Bamboo_2.glb", 0.25f, 0.5f, 1.1f, 0.5f, 1.1f },
            { "bamboo_3", "assets/Bamboo_3.glb", 0.18f, 0.9f, 2.0f, 0.45f, 1.2f },
            { "bamboo_4", "assets/Bamboo_4.glb", 0.12f, 2.0f, 5.0f, 0.35f, 1.3f },
        };
        for (auto& r : rows) {
            auto l = layer(r.n, r.p, kBamboo, r.dens, r.mn, r.mx, 0.04f, 0.0f, false);
            l.scaleBias = {r.bx, r.by, r.bx};
            l.scaleXZJitter = 0.15f; l.scaleYJitter = 0.20f;
            l.cliffBuffer = 1;
            cfg.layers.push_back(l);
        }
        {   // Bamboo_Crop: fallen/harvested cane on ground
            auto l = layer("bamboo_crop", "assets/Bamboo_Crop.glb", kBamboo,
                0.06f, 0.6f, 1.4f, 0.04f, 0.0f, true);
            l.scaleXZJitter = 0.20f; l.scaleYJitter = 0.15f;
            cfg.layers.push_back(l);
        }
    }

    // Rice — MosquitosTicks + Dragonflies (wet ground)
    {
        const std::vector<BugClass> kRice = {BugClass::MosquitosTicks, BugClass::Dragonflies};
        const struct { const char* n; const char* p; float dens; float mn; float mx; } rows[] = {
            { "rice_1", "assets/Rice_1.glb", 0.40f, 0.3f, 0.6f },
            { "rice_2", "assets/Rice_2.glb", 0.28f, 0.5f, 0.9f },
            { "rice_3", "assets/Rice_3.glb", 0.20f, 0.7f, 1.2f },
            { "rice_4", "assets/Rice_4.glb", 0.13f, 0.9f, 1.5f },
        };
        for (auto& r : rows) {
            auto l = layer(r.n, r.p, kRice, r.dens, r.mn, r.mx, 0.04f, 0.0f, false);
            l.scaleXZJitter = 0.18f; l.scaleYJitter = 0.22f;
            cfg.layers.push_back(l);
        }
    }

    // Wheat — Termites + BeesWasps (open grass/meadow).
    // Single stalks (not a cluster), placed densely so they read as a field.
    {
        const std::vector<BugClass> kWheat = {BugClass::Termites, BugClass::BeesWasps};
        const struct { const char* n; const char* p; float dens; float mn; float mx; } rows[] = {
            { "wheat_1", "assets/Wheat_1.glb", 0.45f, 0.3f, 0.6f },
            { "wheat_2", "assets/Wheat_2.glb", 0.32f, 0.5f, 0.8f },
            { "wheat_3", "assets/Wheat_3.glb", 0.22f, 0.6f, 1.0f },
            { "wheat_4", "assets/Wheat_4.glb", 0.14f, 0.8f, 1.2f },
        };
        for (auto& r : rows) {
            auto l = layer(r.n, r.p, kWheat, r.dens, r.mn, r.mx, 0.04f, 0.0f, false);
            l.scaleXZJitter = 0.15f; l.scaleYJitter = 0.20f;
            cfg.layers.push_back(l);
        }
        {   // Wheat_Crop: small bundles lying on the ground
            auto l = layer("wheat_crop", "assets/Wheat_Crop.glb", kWheat,
                0.08f, 0.5f, 1.0f, 0.04f, 0.0f, true);
            l.scaleXZJitter = 0.20f; l.scaleYJitter = 0.15f;
            cfg.layers.push_back(l);
        }
    }

    // Lettuce — Snails (classic fit)
    {
        const std::vector<BugClass> kLettuce = {BugClass::Snails};
        const struct { const char* n; const char* p; float dens; float mn; float mx; } rows[] = {
            { "lettuce_1", "assets/Lettuce_1.glb", 0.30f, 0.3f, 0.5f },
            { "lettuce_2", "assets/Lettuce_2.glb", 0.22f, 0.4f, 0.7f },
            { "lettuce_3", "assets/Lettuce_3.glb", 0.16f, 0.5f, 0.9f },
            { "lettuce_4", "assets/Lettuce_4.glb", 0.10f, 0.7f, 1.2f },
        };
        for (auto& r : rows) {
            auto l = layer(r.n, r.p, kLettuce, r.dens, r.mn, r.mx, 0.05f, 0.10f, false);
            l.scaleXZJitter = 0.18f; l.scaleYJitter = 0.12f;
            cfg.layers.push_back(l);
        }
    }

    // Pumpkin — Woodlice + Mantis (damp forest floor)
    {
        const std::vector<BugClass> kPumpkin = {BugClass::Woodlice, BugClass::Mantis};
        const struct { const char* n; const char* p; float dens; float mn; float mx; } rows[] = {
            { "pumpkin_1", "assets/Pumpkin_1.glb", 0.22f, 0.3f, 0.5f },
            { "pumpkin_2", "assets/Pumpkin_2.glb", 0.16f, 0.5f, 0.8f },
            { "pumpkin_3", "assets/Pumpkin_3.glb", 0.11f, 0.7f, 1.1f },
            { "pumpkin_4", "assets/Pumpkin_4.glb", 0.07f, 1.0f, 1.7f },
        };
        for (auto& r : rows) {
            auto l = layer(r.n, r.p, kPumpkin, r.dens, r.mn, r.mx, 0.05f, 0.0f, false);
            l.maxSlope = 0.4f;
            l.scaleXZJitter = 0.22f; l.scaleYJitter = 0.18f;
            cfg.layers.push_back(l);
        }
        {   // Pumpkin_Crop: full pumpkin lying on the ground
            auto l = layer("pumpkin_crop", "assets/Pumpkin_Crop.glb", kPumpkin,
                0.07f, 0.9f, 1.6f, 0.05f, 0.0f, false);
            l.maxSlope = 0.3f;
            l.scaleXZJitter = 0.25f; l.scaleYJitter = 0.20f;
            cfg.layers.push_back(l);
        }
    }

    // Watermelon — Scorpions + Ants (desert heat)
    {
        const std::vector<BugClass> kMelon = {BugClass::Scorpions, BugClass::Ants};
        const struct { const char* n; const char* p; float dens; float mn; float mx; } rows[] = {
            { "watermelon_1", "assets/Watermelon_1.glb", 0.22f, 0.2f, 0.4f },
            { "watermelon_2", "assets/Watermelon_2.glb", 0.16f, 0.4f, 0.7f },
            { "watermelon_3", "assets/Watermelon_3.glb", 0.11f, 0.6f, 1.0f },
            { "watermelon_4", "assets/Watermelon_4.glb", 0.07f, 0.9f, 1.5f },
        };
        for (auto& r : rows) {
            auto l = layer(r.n, r.p, kMelon, r.dens, r.mn, r.mx, 0.05f, 0.0f, false);
            l.maxSlope = 0.4f;
            l.scaleXZJitter = 0.25f; l.scaleYJitter = 0.18f;
            cfg.layers.push_back(l);
        }
        {   // Watermelon_Crop: big ripe melon on the ground
            auto l = layer("watermelon_crop", "assets/Watermelon_Crop.glb", kMelon,
                0.08f, 0.9f, 1.8f, 0.05f, 0.0f, false);
            l.maxSlope = 0.3f;
            l.scaleXZJitter = 0.25f; l.scaleYJitter = 0.20f;
            cfg.layers.push_back(l);
        }
    }

    return cfg;
}

// ---------------------------------------------------------------------------
// Sampling helpers
// ---------------------------------------------------------------------------

namespace {

/// BugClass of the territory owning a tile, or BugClass::None if out of range.
BugClass BiomeAt(const std::vector<TerrainData>& terrains, const TerrainTile& tile) {
    if (tile.territoryId >= terrains.size()) return BugClass::None;
    return terrains[tile.territoryId].bugClass;
}

} // namespace

// ---------------------------------------------------------------------------
// Populate
// ---------------------------------------------------------------------------

uint32_t ScatterSystem::Populate(entt::registry& registry, const WorldManager& world, const ScatterConfig& cfg) {
    const auto& wcfg = world.GetConfig();
    const auto& terrains = world.GetTerrains();

    if (world.GetGridSize() <= 0 || cfg.layers.empty()) {
        spdlog::warn("ScatterSystem: nothing to scatter (empty tile grid or no layers)");
        return 0;
    }

    const glm::vec2 worldSize = cfg.worldMax - cfg.worldMin;
    if (worldSize.x <= 0.f || worldSize.y <= 0.f || cfg.spacing <= 0.f) {
        spdlog::error("ScatterSystem: invalid world extents or spacing");
        return 0;
    }

    // Shared Perlin for clump masks (kept independent from the per-cell RNG so
    // clumping is a smooth field rather than white noise).
    siv::PerlinNoise perlin(static_cast<siv::PerlinNoise::seed_type>(cfg.seed));

    const int numTiers = std::max(1, wcfg.numTiers);

    const int cols = (int)std::ceil(worldSize.x / cfg.spacing);
    const int rows = (int)std::ceil(worldSize.y / cfg.spacing);

    uint32_t created = 0;

    for (int gz = 0; gz < rows && created < cfg.maxProps; ++gz) {
        for (int gx = 0; gx < cols && created < cfg.maxProps; ++gx) {
            // Deterministic per-cell RNG: depends only on seed + cell coords.
            std::mt19937 rng(cfg.seed ^ (uint32_t)(gx * 73856093) ^ (uint32_t)(gz * 19349663));
            std::uniform_real_distribution<float> u01(0.f, 1.f);

            // Jittered candidate position in world space.
            float jx = (u01(rng) - 0.5f) * cfg.jitter * cfg.spacing;
            float jz = (u01(rng) - 0.5f) * cfg.jitter * cfg.spacing;
            float wx = cfg.worldMin.x + (gx + 0.5f) * cfg.spacing + jx;
            float wz = cfg.worldMin.y + (gz + 0.5f) * cfg.spacing + jz;
            if (wx > cfg.worldMax.x || wz > cfg.worldMax.y) continue;

            // Tile lookup: skip cliffs and water entirely (no scatter there).
            int tx, tz;
            world.WorldToTile(wx, wz, tx, tz);
            const TerrainTile& tile = world.GetTile(tx, tz);
            if (tile.surface == TileSurface::Cliff || tile.surface == TileSurface::Water) continue;

            // Height band is normalized [0..1] across the tier range; slope is
            // binary - 0 on flat Plateau tiles, 1 on Ramp tiles (see
            // docs/WORLDGEN_PLAN.md §6).
            float hC = (numTiers > 1) ? (float)tile.tier / (float)(numTiers - 1) : 0.f;
            float slope = (tile.surface == TileSurface::Ramp) ? 1.0f : 0.0f;

            BugClass biome = BiomeAt(terrains, tile);

            // Evaluate layers in order; first match wins this candidate.
            for (size_t li = 0; li < cfg.layers.size(); ++li) {
                const ScatterLayer& L = cfg.layers[li];

                if (!L.biomes.empty() &&
                    std::find(L.biomes.begin(), L.biomes.end(), biome) == L.biomes.end())
                    continue;
                if (hC < L.minHeight || hC > L.maxHeight) continue;
                if (slope > L.maxSlope) continue;

                if (L.cliffBuffer > 0) {
                    bool tooClose = false;
                    for (int dz = -L.cliffBuffer; dz <= L.cliffBuffer && !tooClose; ++dz)
                        for (int dx = -L.cliffBuffer; dx <= L.cliffBuffer && !tooClose; ++dx) {
                            if (dx == 0 && dz == 0) continue;
                            const TerrainTile& nb = world.GetTile(tx + dx, tz + dz);
                            if (nb.surface == TileSurface::Cliff || nb.surface == TileSurface::Water)
                                tooClose = true;
                        }
                    if (tooClose) continue;
                }

                if (L.clumpThreshold > 0.f) {
                    float c = (float)perlin.octave2D_01(wx * L.clumpNoiseScale,
                                                        wz * L.clumpNoiseScale, 3);
                    if (c < L.clumpThreshold) continue;
                }

                if (u01(rng) > L.density) continue;

                // --- Place the prop ---------------------------------------
                float worldY = world.TierToWorldHeight(tile.tier) * cfg.heightWorldScale + L.yOffset;
                glm::vec3 pos(wx, worldY, wz);

                glm::vec3 rot(0.f);
                if (L.randomYaw) rot.y = u01(rng) * 360.f;

                float s = glm::mix(L.minScale, L.maxScale, u01(rng));
                glm::vec3 jitter(
                    1.f + (u01(rng) * 2.f - 1.f) * L.scaleXZJitter,
                    1.f + (u01(rng) * 2.f - 1.f) * L.scaleYJitter,
                    1.f + (u01(rng) * 2.f - 1.f) * L.scaleXZJitter
                );

                auto e = registry.create();
                auto& tf = registry.emplace<TransformComponent>(e, pos);
                tf.rotation = rot;
                tf.scale = L.scaleBias * s * jitter;
                registry.emplace<ModelComponent>(e, L.modelPath);
                registry.emplace<ScatterPropComponent>(e, (uint16_t)li);
                ++created;
                break; // candidate consumed
            }
        }
    }

    spdlog::info("ScatterSystem: placed {} props across {} layers ({}x{} candidate grid)",
                 created, cfg.layers.size(), cols, rows);
    return created;
}

// ---------------------------------------------------------------------------
// Clear
// ---------------------------------------------------------------------------

void ScatterSystem::Clear(entt::registry& registry) {
    auto view = registry.view<ScatterPropComponent>();
    std::vector<entt::entity> doomed(view.begin(), view.end());
    registry.destroy(doomed.begin(), doomed.end());
    spdlog::info("ScatterSystem: cleared {} scatter props", doomed.size());
}
