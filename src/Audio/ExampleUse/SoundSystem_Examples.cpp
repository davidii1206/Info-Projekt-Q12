#if 0
// ═══════════════════════════════════════════════════════════════════════════════
// SoundSystem – Verwendungsbeispiele für das 3D-Spiel
// Diese Datei zeigt typische Verwendung im Game-Code.
// ═══════════════════════════════════════════════════════════════════════════════


// ── 1. Init & Update (GameLayer) ──────────────────────────────────────────────

void GameLayer::OnAttach() {
    SoundSystem::Get().Init();

    // Biome-Sounds registrieren (Playlist mit Crossfade)
    BiomePlaylist forestPl;
    forestPl.tracks       = { "assets/audio/amb_forest1.wav", "assets/audio/amb_forest2.wav" };
    forestPl.shuffle      = true;
    forestPl.crossfadeTime = 3.0f;
    SoundSystem::Get().RegisterBiomePlaylist(BiomeType::Forest, forestPl);
    SoundSystem::Get().RegisterBiomeSound(BiomeType::Cave,   "assets/audio/amb_cave.wav");
    SoundSystem::Get().RegisterBiomeSound(BiomeType::Desert, "assets/audio/amb_desert.wav");

    // Sounds vorladen
    SoundSystem::Get().LoadSound("assets/audio/sfx_hit.wav");
    SoundSystem::Get().LoadSound("assets/audio/sfx_hit_crit.wav");
    SoundSystem::Get().LoadSound("assets/audio/sfx_footstep_grass.wav");
}

void GameLayer::OnUpdate(float dt) {
    auto& sound = SoundSystem::Get();

    // Listener = Kamera-Position (glm::vec3 → SoundVec3)
    glm::vec3 camPos = m_Camera.GetPosition();
    glm::vec3 camFwd = m_Camera.GetForward();
    glm::vec3 camUp  = m_Camera.GetUp();
    sound.SetListenerPosition({ camPos.x, camPos.y, camPos.z });
    sound.SetListenerOrientation({ camFwd.x, camFwd.y, camFwd.z },
                                 { camUp.x,  camUp.y,  camUp.z  });

    sound.Update(dt);
}


// ── 2. 3D Positional Audio ────────────────────────────────────────────────────

// Explosion an Weltposition
void OnExplosion(glm::vec3 worldPos) {
    SoundSystem::Get().Play3D(
        "assets/audio/sfx_explosion.wav",
        { worldPos.x, worldPos.y, worldPos.z },
        1.0f,    // volume
        80.f     // maxDistance in Welt-Einheiten
    );
}

// Entity-Schritt mit Oberfläche und 3D-Position
void OnFootstep(SurfaceType surface, glm::vec3 pos) {
    static const std::unordered_map<SurfaceType, std::vector<std::string>> steps = {
        { SurfaceType::Grass,  { "assets/audio/sfx_step_grass1.wav", "assets/audio/sfx_step_grass2.wav" }},
        { SurfaceType::Stone,  { "assets/audio/sfx_step_stone1.wav", "assets/audio/sfx_step_stone2.wav" }},
        { SurfaceType::Wood,   { "assets/audio/sfx_step_wood1.wav"  }},
    };

    PlaySoundParams p;
    p.is3D        = true;
    p.position    = { pos.x, pos.y, pos.z };
    p.maxDistance = 20.f;
    p.pitchMin    = 0.9f;
    p.pitchMax    = 1.1f;
    p.volume      = 0.7f;
    p.group       = "footsteps";
    p.maxInGroup  = 2;
    p.cooldownSeconds = 0.1f;

    auto it = steps.find(surface);
    if (it != steps.end())
        SoundSystem::Get().PlayRandom(it->second, p);
}


// ── 3. Kampf-Sounds mit Priorität ────────────────────────────────────────────

void OnEntityDamaged(float amount, bool isCritical, glm::vec3 pos) {
    PlaySoundParams p;
    p.is3D        = true;
    p.position    = { pos.x, pos.y, pos.z };
    p.maxDistance = 30.f;
    p.pitchMin    = isCritical ? 0.85f : 0.92f;
    p.pitchMax    = isCritical ? 1.05f : 1.12f;
    p.priority    = isCritical ? SoundPriority::High : SoundPriority::Normal;
    p.volume      = isCritical ? 1.0f : 0.75f;

    const char* file = isCritical ? "assets/audio/sfx_hit_crit.wav"
                                  : "assets/audio/sfx_hit.wav";
    SoundSystem::Get().Play(file, p);
}

void OnPlayerDeath() {
    // Musik ausblenden (Ducking)
    SoundSystem::Get().DuckCategory(SoundCategory::Music, 0.1f, 5.0f);

    // Todes-Sound: Critical-Priorität, wird nie verworfen
    PlaySoundParams p;
    p.priority   = SoundPriority::Critical;
    p.fadeInSeconds = 0.3f;
    SoundSystem::Get().Play("assets/audio/sfx_player_death.wav", p);
}


// ── 4. Musik mit Fade In/Out ──────────────────────────────────────────────────

// Beim Betreten einer Dungeon-Area
void OnEnterDungeon() {
    auto& sound = SoundSystem::Get();

    // Ambient komplett ausblenden
    sound.DuckCategory(SoundCategory::Ambient, 0.0f, 99999.f);

    PlaySoundParams p;
    p.loop           = true;
    p.category       = SoundCategory::Music;
    p.fadeInSeconds  = 2.5f;
    p.volume         = 0.85f;
    SoundSource music = sound.Play("assets/audio/music_dungeon.wav", p);

    // Merken für späteren FadeOut
    // m_CurrentMusic = music;
}

void OnExitDungeon(SoundSource music) {
    SoundSystem::Get().FadeOut(music, 2.0f);
    // Ambient wieder einblenden
    SoundSystem::Get().DuckCategory(SoundCategory::Ambient, 1.0f, 0.01f);
    SoundSystem::Get().SetActiveBiome(BiomeType::Forest); // Biome mit Crossfade
}


// ── 5. Biome-Wechsel ──────────────────────────────────────────────────────────

void OnPlayerBiomeChanged(BiomeType newBiome) {
    // Automatischer Crossfade (kCROSSFADE_TIME = 2s)
    SoundSystem::Get().SetActiveBiome(newBiome);
}


// ── 6. UI-Sounds ─────────────────────────────────────────────────────────────

void OnButtonClick() {
    PlaySoundParams p;
    p.category       = SoundCategory::UI;
    p.volume         = 0.5f;
    p.pitchMin       = 0.98f;
    p.pitchMax       = 1.02f;
    p.cooldownSeconds = 0.05f;  // Spam-Schutz
    SoundSystem::Get().Play("assets/audio/sfx_ui_click.wav", p);
}


// ── 7. Lautstärke-Einstellungen (Options-Menü) ────────────────────────────────

void ApplyAudioSettings(float master, float effects, float ambient, float music) {
    auto& sound = SoundSystem::Get();
    sound.SetMasterVolume(master);
    sound.SetCategoryVolume(SoundCategory::Effects, effects);
    sound.SetCategoryVolume(SoundCategory::Ambient, ambient);
    sound.SetCategoryVolume(SoundCategory::Music,   music);
}
#endif