# Bugmin – Code-Dokumentation

Pikmin-inspirierter LAN-Multiplayer-RTS. C++ · CMake · SDL3 · Jolt Physics · EnTT · OpenAL · ImGui.

Diese Dokumentation begleitet das Projektportfolio. Sie ist **nicht** als
vollständige API-Referenz gedacht (der gesamte Quellcode ist im Repository),
sondern als geführter Überblick: Was sind die wichtigsten Klassen, wo läuft
welche Logik, und wie hängt das alles zusammen.

> *Hinweis: Wir arbeiten weiter am Projekt. Einzelne Klassen und Felder können
> sich bis zur Präsentation noch ändern.*

---

## Architektur in einer Minute

Das Spiel hat drei grosse Subsysteme:

- **Engine / Anwendungsrahmen** (Window, Renderer, Layer-Stack, Event-System)
- **Spielwelt** (ECS-Registries, Szenen, Gameplay-Systeme, Physik)
- **Netzwerk** (Server-authoritative, 20 Hz Fixed Tick, enet/UDP)

Im laufenden Spiel gibt es **zwei parallele EnTT-Registries**:
- *Server-Registry* (`SceneContext::serverRegistry`) — die Wahrheit, nur auf dem Host.
- *Client-Registry* (`SceneContext::clientRegistry`) — wird gerendert; die Server-Wahrheit fliesst per Snapshots hierher.

Beide Welten enthalten Entities mit gleichen `netId`s; darüber sind sie verbunden.

---

## Was wo lebt (im Repo)

| Ordner | Inhalt | Wichtigste Klassen |
|---|---|---|
| `src/Core/` | Engine-Kern: Anwendung, Fenster, Layer-Stack, Event-System, Physik, Worldgen | `Application`, `LayerStack`, `Layer`, `PhysicsServer`, `WorldManager`, `AssetManager`, `Input` |
| `src/Core/Events/` | Event-Klassen (Mouse/Key/Window) und Dispatcher | `Event`, `EventDispatcher`, `KeyPressedEvent`, … |
| `src/Networking/` | High-Level-Netzmanager + ENet-Wrapper + Pakettypen | `NetworkManager`, `NetLib::GameSession`, `Packets.h` |
| `src/Gameplay/` | Szenen, ECS-Komponenten, Spielsysteme | `GameScene`, `MainMenuScene`, `BuildingSystem`, `UpgradeSystem`, `ResourceManager`, `FogOfWar`, `TerritorySystem` |
| `src/Graphics/` | Renderer, Kamera, Modell-Loader, Terrain-Mesh-Builder | `Renderer`, `Camera`, `Model`, `TerrainMeshBuilder`, `LightComponent` |
| `src/Audio/` | Sound-System (OpenAL) + Sound-Events | `SoundSystem`, `BiomePlaylist`, `EntityDamagedEvent` |
| `shaders/` | GLSL-Shader (kompiliert zu SPIR-V): Model-/Terrain-/Shadow-/Post-Pass | — |

---

## Empfohlene Lesereihenfolge

Wer den Code nicht komplett liest, hat in dieser Reihenfolge in ca. 15 Minuten
den Überblick:

1. **`Application` (`src/Core/Application.h`)** – Einstiegspunkt. Erstellt Window,
   Renderer, Physik und Network und schiebt einen `GameLayer` auf den
   `LayerStack`.
2. **`GameScene` (`src/Gameplay/GameScene.h`)** – die zentrale Szene. Hier
   passiert praktisch alles: Networking-Polling, Server-Tick, Rendering,
   Bauplatzierung, Commander-Modus.
3. **`Packets.h` (`src/Networking/Packets.h`)** – alle 19 Pakettypen mit ihren
   Datenfeldern. Ein guter Einstieg ins Netzwerkprotokoll.
4. **`WorldManager` (`src/Core/WorldManager.h`)** – Voronoi-Territorien + die
   terrassenförmige Höhenkarte (Tile-Grid mit Plateau/Cliff/Ramp/Water).
5. **`TerrainMeshBuilder` (`src/Graphics/TerrainMeshBuilder.h`)** – baut aus dem
   Tile-Grid das eigentliche 3D-Mesh.
6. **`BuildingSystem` (`src/Gameplay/BuildingSystem.h`)** und
   **`Building_classes.h`** – Gebäude, Tiers, stammesspezifische Boni,
   Upgrade-Kosten.
7. **`UpgradeSystem` (`src/Gameplay/UpgradeSystem.h`)** und
   **`UpgradeDefs.h`** – Team-Upgrades und passive Boni.
8. **`FogOfWar` (`src/Gameplay/FogOfWar.h`)** und
   **`TerritorySystem.h`** – beide werden auf dem Server gepflegt und
   regelmässig (2 Hz) an Clients gebroadcastet.

---

## Netzwerk in Stichworten

- 20 Hz Fixed-Tick im `GameScene::FixedUpdate`: poll connections → poll
  packets → run server systems → send local input → broadcast snapshots.
- 19 Pakettypen (alle in `Packets.h`). Wichtigste:
  - `PlayerInputPacket` – Spieler-Input (Client → Server, jeden Tick)
  - `EntitySnapshotPacket` – Transform + Velocity einer Entity (Server → alle, 20 Hz)
  - `TerritorySnapshotPacket`, `FogSnapshotPacket` – Spielzustand (Server → alle, 2 Hz)
  - `UnitSpawnedPacket`, `BuildingSpawnedPacket`, `UpgradeCompletedPacket`, … – Lifecycle
- Lokale Prediction: der eigene Spieler bewegt sich pro Frame in `LogicUpdate`;
  Snapshots überschreiben seine Rotation nicht (einfache Reconciliation).
- Transport: **enet** (UDP) — siehe `NetLib::GameSession` in `NetworkWrapper.h`.

Der vollständige Architekturplan inkl. Diagramm ist im Portfolio (Abschnitt
"2.7 Netzwerk-Architektur").

---

## Worldgen in Stichworten

- **Voronoi + Lloyd-Relaxation** verteilt N (= 25) Territorien gleichmässig.
- Jedes Territorium bekommt eine `BugClass` (Stamm) zugewiesen.
- Ein **terrassiertes Tile-Grid** wird darüber gelegt: jede Kachel hat
  einen Tier-Index 0..numTiers-1 und einen Surface-Typ
  (`Plateau` / `Cliff` / `Ramp` / `Water`).
- **Saddle-Points**: an der Grenze zwischen zwei Territorien wird der
  Ridge-Boost zurückgenommen → es entsteht automatisch ein Übergang.
- **Corridor-Ramps**: zwei nebeneinanderliegende Cliff-Kacheln werden zu
  Rampen umgewandelt, damit Einheiten zwischen Tiers wechseln können.
- Aus dem Tile-Grid baut `TerrainMeshBuilder::Build()` ein deterministisches
  Mesh (Plateaus, Klippenwände, Rampen-Keile).
- **Scatter** (`ScatterSystem`): clientseitig, deterministisch — platziert ~10 000
  Deko-Props (Bäume, Felsen, Pflanzen) regelbasiert pro Biom.

---

## Hinweis zur Vollständigkeit

Diese Übersicht zeigt **die öffentliche API der wichtigsten Klassen**. Private
Implementierungsdetails, Helfer-Strukturen und Datei-Listen sind aus dieser
Ansicht ausgeblendet, sind aber im Repository einsehbar. Wer den vollständigen
Quellcode lesen möchte, findet ihn unter
[github.com/davidii1206/Info-Projekt-Q12](https://github.com/davidii1206/Info-Projekt-Q12)
im Branch `dev`.
