# Engine Architecture

This document provides a high-level overview of the Bugmin Engine architecture for developers.

## Core Stack
- **Platform/Windowing:** SDL3
- **Rendering:** SDL3 GPU API (Abstracts Vulkan, Metal, DX12)
- **Physics:** Jolt Physics
- **ECS:** EnTT
- **Audio:** OpenAL Soft
- **Networking:** ENet

## Rendering Architecture (SDL3 GPU)

The engine recently migrated from OpenGL to the modern **SDL3 GPU API**. This provides a universal abstraction over Vulkan (Android/Linux) and DX12 (Windows).

### Key Concepts
1. **Renderer:** Manages the `SDL_GPUDevice` and the frame lifecycle (Acquiring command buffers, swapchain textures, and submitting passes).
2. **GPUBuffer:** A modern C++ wrapper around `SDL_GPUBuffer`. Handles staging and uploading data to the GPU automatically.
3. **Pipeline State Objects (PSO):** (In Progress) All state (shaders, blending, depth) is baked into a pipeline object rather than set globally.

## Networking Architecture (NetLib)

The engine uses **ENet** for low-latency UDP-based networking.

### Key Concepts
1. **Server/Client:** Dedicated classes for hosting or joining sessions. Each runs its own network thread to avoid blocking the main game loop.
2. **GameSession:** A high-level template that abstracts the difference between a host and a client. It sorts incoming packets into typed queues.
3. **Type-Based Routing:** Developers can retrieve packets based on their unique ID (header), allowing different game systems to listen only for relevant data.

## Documentation Standards
Please use **Javadoc-style** comments for all public headers:

```cpp
/**
 * @brief Brief description of the class.
 * 
 * Detailed explanation of usage.
 */
class MyClass {
public:
    /**
     * @brief Performs a specific action.
     * @param value Description of the parameter.
     * @return Description of the return value.
     */
    bool DoAction(int value);
};
```
