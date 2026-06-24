/**
 * @file DebugUI.h
 * @brief Global flag controlling the visibility of developer ImGui panels.
 *
 * Every debug-only ImGui window (Post-Processing, World Debug, Shadow, Network,
 * Bugmin Debugger, G-Buffer Visualisation, etc.) should early-out when
 * DebugUI::IsVisible() returns false. The flag is toggled with F12 from the
 * scene LogicUpdate code so the player can flip the dev UI on/off at runtime.
 *
 * Default: hidden — keeps the game looking clean for normal play.
 */
#pragma once

namespace DebugUI {

/// @return True if developer panels should be drawn this frame.
bool IsVisible();

/// @brief Flip the visibility state. Wire this to an F12 key tap.
void Toggle();

} // namespace DebugUI
