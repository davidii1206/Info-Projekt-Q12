#pragma once
#include <cstdint>

/**
 * @enum CameraMode
 * @brief Defines the available camera control modes.
 */
enum class CameraMode : uint8_t {
    Commander = 0, ///< Top-down RTS view for commanding units.
    Building = 1,  ///< Diagonal top-down orthographic view for management.
    FreeFly = 2    ///< Debug free-fly camera.
};
