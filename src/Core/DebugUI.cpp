#include "DebugUI.h"

namespace DebugUI {

static bool s_Visible = false; ///< Hidden by default — looks like a game on launch.

bool IsVisible() { return s_Visible; }
void Toggle()    { s_Visible = !s_Visible; }

} // namespace DebugUI
