/**
 * @file ScatterSystem.h
 * @brief Biome-aware decorative prop scattering (grass, rocks, twigs, …).
 *
 * The scatter system turns the procedural data already produced by
 * WorldManager (heightmap + Voronoi biomes) into a dense field of small
 * decorative meshes, so the terrain reads as a *place* instead of a bare
 * heightmap with a few objects dropped on it.
 *
 * Design
 * ------
 *  - **Deterministic & client-side.** Every prop is derived purely from the
 *    world seed + (x,z) position, so every peer generates an identical field
 *    locally. Nothing is networked — this is how we afford thousands of
 *    instances. Props are tagged with ScatterPropComponent and live in the
 *    *client* registry alongside other renderable entities.
 *  - **Rule driven.** Each ScatterLayer describes one kind of prop and the
 *    conditions under which it appears: which biomes, which altitude band,
 *    how steep the ground may be, how densely it clumps, and how it is scaled
 *    / rotated. Adding a new prop = adding a layer.
 *
 * Placeholders
 * ------------
 * Every layer ships pointing at a small generated low-poly primitive
 * (`assets/prim_*.glb` — sphere/cone/cylinder/slab, flat-colored via
 * `baseColorFactor`) so the field is visible and runnable *today* and reads
 * as grass/rocks/wood rather than a field of identical cubes. Replace
 * `ScatterLayer::modelPath` with the real art as it is produced — the asset
 * each layer wants is documented in `ScatterLayer::name`.
 *
 * Usage
 * -----
 * @code
 *   // OnEnter() — on every peer (props are local/deterministic, not networked)
 *   ScatterConfig cfg = ScatterConfig::Default(worldSeed);
 *   ScatterSystem::Populate(ctx.clientRegistry, worldManager, cfg);
 *
 *   // OnExit()
 *   ScatterSystem::Clear(ctx.clientRegistry);
 * @endcode
 */

#pragma once
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <cstdint>

#include "Bug_classes.h"

class WorldManager;

/**
 * @struct ScatterLayer
 * @brief One kind of scattered prop and the rules governing where it appears.
 */
struct ScatterLayer {
    /// Human-readable name of the asset an artist needs to provide.
    std::string name = "prop";

    /// Mesh used for this layer. Defaults to a placeholder primitive; swap for art.
    std::string modelPath = "assets/prim_sphere_green.glb";

    /// Biomes this prop may appear in. Empty = every biome.
    std::vector<BugClass> biomes;

    /// Allowed normalized-height band [0..1] sampled from the heightmap.
    float minHeight = 0.0f;
    float maxHeight = 1.0f;

    /// Maximum ground steepness [0..1] (0 = flat only, 1 = any slope).
    float maxSlope = 1.0f;

    /// Probability [0..1] this layer is placed when a candidate point matches.
    float density = 0.25f;

    /// Uniform scale range applied to each instance.
    float minScale = 0.8f;
    float maxScale = 1.2f;

    /// Vertical offset (world units) added after sampling the ground height,
    /// e.g. to sink a rock slightly into the ground or lift a hovering prop.
    float yOffset = 0.0f;

    /// Perlin frequency controlling how this layer clumps (lower = bigger patches).
    float clumpNoiseScale = 0.04f;

    /// Clump cutoff [0..1]; the prop only spawns where clump noise exceeds this.
    /// 0 disables clumping (uniform random distribution).
    float clumpThreshold = 0.0f;

    /// Randomize yaw around the up axis for natural variation.
    bool randomYaw = true;

    /// Tilt the prop to follow the terrain normal (good for rocks, bad for trees).
    bool alignToSlope = false;
};

/**
 * @struct ScatterConfig
 * @brief Global scatter parameters plus the list of prop layers.
 */
struct ScatterConfig {
    /// Seed — share with the world seed so the field matches the terrain.
    uint32_t seed = 12345;

    /// World-space XZ extents the scatter covers. Match your terrain footprint.
    /// Defaults align with the Fog/Territory extents used in GameScene (±150).
    glm::vec2 worldMin = {-150.f, -150.f};
    glm::vec2 worldMax = { 150.f,  150.f};

    /// Spacing (world units) between candidate points. Smaller = denser & slower.
    float spacing = 2.0f;

    /// How far (fraction of `spacing`) each candidate may jitter off the grid.
    float jitter = 0.9f;

    /// Multiplier mapping the normalized heightmap value [0..1] to world Y.
    /// Set to 0 to scatter onto a flat plane at y=0 (matches a flat test scene);
    /// set to your terrain's vertical scale to sit props on procedural hills.
    float heightWorldScale = 0.0f;

    /// Safety cap on the number of props generated.
    uint32_t maxProps = 60000;

    /// Prop layers, evaluated in order per candidate point.
    std::vector<ScatterLayer> layers;

    /**
     * @brief Build the default placeholder layer set.
     *
     * Returns a ready-to-use config covering the four currently active biomes
     * (Ants, Termites, Spiders, Woodlice) plus biome-agnostic ground litter.
     * Every layer points at the placeholder cube and names the real asset it
     * wants — see ScatterLayer::name.
     *
     * @param seed World seed (so the field lines up with the terrain).
     */
    static ScatterConfig Default(uint32_t seed);
};

/**
 * @namespace ScatterSystem
 * @brief Stateless helpers that populate / clear the scatter prop field.
 */
namespace ScatterSystem {

/**
 * @brief Generate the decorative prop field into a registry.
 *
 * Reads the heightmap and biome layout from @p world, then walks a jittered
 * grid across the configured world extents, placing props per layer rules.
 * Each prop entity gets a TransformComponent, a ModelComponent and a
 * ScatterPropComponent tag.
 *
 * @param registry Registry to populate (typically the client registry).
 * @param world    Generated WorldManager providing heightmap + biomes.
 * @param cfg      Scatter configuration / layer rules.
 * @return Number of prop entities created.
 */
uint32_t Populate(entt::registry& registry, const WorldManager& world, const ScatterConfig& cfg);

/**
 * @brief Destroy every entity carrying a ScatterPropComponent.
 * @param registry Registry to clear scatter props from.
 */
void Clear(entt::registry& registry);

} // namespace ScatterSystem
