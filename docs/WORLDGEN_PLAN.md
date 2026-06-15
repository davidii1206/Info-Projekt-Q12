# World Generation Plan — Thronefall-style Terraced Map

**Status: PLANNING + REMOVAL ONLY.** This document specifies the target
system and the teardown of the existing organic generator. The new
generator is **not** implemented here — this is the spec for the TODOs:

> **TODO: world generation (vertex displacement, LODs) + structure placement,
> as close to a Thronefall copy as possible.**

---

## 0. Corrected framing (read first)

The 16 `BugClass` types are **playable armies / factions** — each a roster of
Tier 1–5 *units*, not a terrain biome. The old `WorldManager` wrongly treated
each Voronoi cell as a permanently faction-typed "biome" with its own organic
height rules. That coupling is removed (see §7).

**Terrain is neutral and terraced.** Factions are *who you play*, not *what
the ground is made of*. However, the roster dictates which **movement modes**
the terrain must support — this is the real design driver:

| Movement mode | Example units | Terrain requirement |
|---|---|---|
| Air | Bees, Wasps, Dragonflies, Moths | Vertical clearance; cliffs don't block them |
| Ground (walk) | Ants, Beetles, most T1 | Flat tiers + ramps to change tier |
| Water / amphibious | Dragonfly larva, Mosquito larva, Ruderwanze, Toe-Biter | **Flat water tiles** (ponds at a low tier) |
| Wall-climb | Scorpions (Rindenskorpion), Spiders | Can traverse **cliff faces** between tiers |
| Tunnel / underground | Worms, Termite borer, Trapdoor spider | Pass *under* tiers; needs a tunnel layer concept |
| Slow/blocker | Snails, Woodlice | Chokepoints (ramps, tunnel mouths) matter |

**Design consequence:** the map is a small set of **flat terraces** connected
by **ramps** (ground chokepoints) and broken up by **cliffs** (climbers/air
bypass) and **water basins** (amphibious-only). This is exactly the Thronefall
silhouette and it gives every movement mode a reason to exist.

---

## 1. Thronefall fidelity targets

What "lowkey Thronefall copy" concretely means for the renderer/gen:

1. **Discrete height tiers only.** No continuous slopes anywhere. A tile is
   either a flat plateau at integer tier `T`, a vertical cliff edge, or a
   ramp. (Pokémon-overworld terraces, not rolling hills.)
2. **Low-poly, flat-shaded look.** Each terrace top is flat; cliffs are
   vertical quads. Lighting reads as faceted, not smooth. Hard tier color
   banding is part of the aesthetic, not a bug.
3. **Stylized water** at a fixed low tier (flat plane + shader, no waves
   mesh).
4. **Readable, board-game-like layout** — a handful of large terraces, not
   noise speckle. Tier changes happen at meaningful boundaries.
5. **Sparse, deliberate props/structures**, not dense naturalistic clutter.

---

## 2. Data model (target)

Replace the per-pixel `std::vector<float> m_Heightmap` with a coarse
**tile grid** sized in gameplay units (≈1–2 world units per tile), aligned to
the existing `TerritorySystem` (±50 world units) and `FogOfWar` (2.0 cell)
extents.

```cpp
enum class TileSurface : uint8_t { Plateau, Cliff, Ramp, Water };

struct TerrainTile {
    uint8_t  tier;          // integer terrace level 0..numTiers-1
    uint8_t  surface;       // TileSurface
    uint16_t territoryId;   // index into m_Terrains (Voronoi cell)
    uint8_t  rampDir;       // 0..3 descent direction (Ramp only)
    bool     buildable;     // flat, dry, non-edge → valid for structures
};
```

World height for a plateau tile = `tier * tierHeight` (constant). Ramp tiles
linearly interpolate between adjacent tiers across the tile — **the only
non-flat geometry on the map.** Cliffs are vertical faces between tiers.

---

## 3. Generation pipeline (target)

1. **Territory layout** — Voronoi + Lloyd relaxation (KEEP existing code).
   Assign each cell a `territoryId`. Faction ownership is a *gameplay*
   property set later, not baked into terrain.
2. **Tier assignment** — low-frequency Perlin quantized to integer tiers so
   neighbouring territories often share a tier (avoids floating islands).
   Bounded so a single territory varies by at most ±1 tier internally.
3. **Water basins** — lowest tier(s) below a threshold flagged `Water`.
   Guarantee at least one pond for amphibious units.
4. **Cliff + ramp resolution** — any tile bordering a different tier is a
   `Cliff`. Place fixed-size straight `Ramp` runs at territory borders so
   ramps double as ground chokepoints (climbers/air bypass cliffs; ground
   must use ramps).
5. **Buildable-slot marking** — flat, dry, non-edge tiles flagged
   `buildable` → feeds structure placement (§6).
6. **Spawn points** — one flat buildable tile near each territory site
   reserved as a faction home plateau.

All steps must be **fully deterministic from `seed`** (host & clients
generate identical grids — same constraint the scatter system relies on).

---

## 4. TODO A — Vertex displacement (terrain mesh)

Turning the tile grid into the Thronefall mesh.

**Approach: CPU-generated chunked mesh (recommended over GPU displacement).**
Reasons: tiers are discrete + need hard normals (flat shading) and the mesh
must also serve as **physics collision** (Jolt `MeshShape`, via the existing
`MeshCollisionBuilder`). A displacement vertex shader can't feed Jolt and
fights flat shading.

Per chunk (e.g. 16×16 tiles):
- **Plateau top:** 2 triangles per tile at `y = tier * tierHeight`, normal up.
  Greedy-merge coplanar same-tier tiles into larger quads to cut vertex count.
- **Cliff walls:** for each tile/neighbour tier delta, emit vertical quads
  spanning the height difference, normal facing out. Hard edges (duplicate
  verts) for flat shading.
- **Ramp wedges:** sloped quad from `tier` down to `tier-1` over the ramp
  run, plus side fill triangles.
- **Water:** separate flat translucent plane at water tier (own pipeline).
- **Vertex colors / material id** per tier band for the flat-shaded palette
  (reuse the existing `ModelVertex.color` channel the renderer already reads).

Output plugs into the existing model render pass (`ModelVertex` layout,
`GameModelPipeline`) and `MeshCollisionBuilder::Build` for collision — both
already exist, so no new rendering backend is needed.

**"Vertex displacement"** in the TODO is satisfied by displacing the chunk
grid vertices to tier heights at build time (with an optional tiny per-vertex
noise jitter for a hand-made feel — bounded so it never breaks the flat read).

## 5. TODO B — LODs

The terrain is large + props/structures are numerous, so LOD is required.

1. **Terrain chunk LOD** — each chunk built at multiple decimation levels
   (full → merged-quads → single quad per tier region). Select by camera
   distance. Because tiers are flat, aggressive merging is nearly lossless.
2. **Prop/structure instancing + LOD** — props (scatter) and structures use
   instanced draw with distance-based mesh swaps; far chunks drop small props
   entirely (density falloff) and use billboard/impostor for medium range.
   ⚠️ The current render path draws **one call per entity** — true instancing
   is NOT wired yet. Instanced batching for `ScatterPropComponent` /
   structures is a prerequisite for LOD to pay off (flag as sub-task).
3. **Collision LOD = none** — physics always uses the full-resolution mesh;
   LOD is visual only.
4. **Chunk culling** — frustum-cull chunks before LOD selection.

## 6. TODO C — Structure placement

Deterministic placement of faction structures + world props on the tile grid.

- **Buildable slots:** placement only on `buildable` tiles (§3.5). Each
  faction home territory gets reserved slots around its spawn for: main
  hive/base, resource depot, production, defensive structures (matches the
  roster's builder/defender theme — Termites, Woodlice phalanx).
- **Neutral structures:** resource nodes, chokepoint markers, landmark props
  placed on buildable/edge tiles per deterministic rules.
- **Validation:** reject slots on ramps, cliffs, water, or occupied tiles;
  enforce min spacing; keep ramps/chokepoints clear.
- **Reuse `ScatterSystem`** (already in tree) for the *small decorative* layer
  — re-pointed at the tile grid (slope becomes binary: 0 on plateau, 1 on
  ramp, none on cliff/water). Structures are a separate, sparser, validated
  pass — not random scatter.
- **Determinism + (later) networking:** structures that are gameplay-relevant
  (bases, captured resource nodes) are authoritative/networked; purely
  decorative placement is local + seed-deterministic like scatter.

---

## 7. Removal pass — what is being deleted now

Performed as part of this change to give the new generator a clean slate.
All of this is the organic / continuous-height / faction-as-biome machinery
that directly contradicts the terraced model:

1. **Per-pixel heightmap generation loop** in `WorldManager::Generate`
   (the `finalH` double-loop using `smoothstep` / `octave2D_01`).
2. **Ant-biome "jagged plaza" sub-site system** — `subSites` generation and
   its `dists[3]` / `plazaFactor` / `crackThreshold` consumption (organic
   cracked-pavement edges).
3. **Continuous per-biome height nudges** (Termite/Spider/Woodlice `biomeH`
   `detailN` offsets).
4. **Slope/altitude config fields**: `noiseScale`, `noiseOctaves`,
   `slopeSharpness`, `slopeWidth`, `altitudeNoiseScale`, `altitudeMaxHeight`,
   `numAltitudeLevels` (superseded by `numTiers` / `tierHeight` later),
   `randomSpawnInTerrain`, `showHeightmap`.
5. **Unused `TerrainData` fields**: `subSites`, `vertices`, `altitude`.
6. **Declared-but-unimplemented height helpers**: `GenerateHeightmap`,
   `GetAntHeight`, `GetTermiteHeight`, `GetSpiderHeight`, `GetWoodliceHeight`.
7. **`UpdateDebugTexture` heightmap-grayscale branch** (the `showHeightmap`
   path).

**Kept** (still valid foundations): Voronoi point generation + Lloyd
relaxation, `BugClass` round-robin assignment (extended to all 16), and
`TerrainData::{id, bugClass, site, spawnPoint, color}`.

**Temporary compatibility note:** the float `m_Heightmap` / `GetHeightmap()`
is being kept this pass as a **flat (all-zero) placeholder** so `ScatterSystem`
and `GameScene` still compile/run. The float-heightmap → `TerrainTile` grid
swap (§2) happens in the implementation phase; the scatter slope source moves
to tiles then. This keeps the tree green during planning.

---

## 8. Integration points (for implementation phase)

- **Physics:** generated terrain mesh → `MeshCollisionBuilder` →
  `PhysicsServer::AddStaticMesh` (replaces `test_scene_pixelation.glb`).
- **TerritorySystem:** Voronoi cells should become the capture zones (unify
  visual territory with strategic zones); ramps = contested chokepoints.
- **FogOfWar:** grid already 2.0-unit cells; align tile grid to it.
- **ScatterSystem:** re-point slope sampling at the tile grid post-swap.
- **MainMenu/lobby:** expose `seed`, `numTiers`, `numTerrains`; reuse the
  debug texture as a map preview (reworked to flat tier color bands).

## 9. Open questions

- Tunnel/underground layer: real second mesh layer, or movement-rule fake?
- Ramp ownership vs `TerritorySystem` (neutral chokepoint vs one cell).
- Chunk size vs tile grid resolution vs ±50 world extents — pick concrete
  numbers before mesh work.
- Instanced rendering must land before prop/structure LOD is worthwhile.
