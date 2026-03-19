# Bugmin Graphics System Documentation

This document provides a centralized guide to the rendering architecture of the Bugmin engine. The system is built on top of the **SDL3 GPU API**, providing a modern, low-level abstraction.

---

## 1. Core Architecture: "Modern & Precise"
The graphics system follows a **Pass-Based Framegraph** model and utilizes **Reverse-Z Infinite Projection** for maximum depth precision.

### Key Performance Features:
- **Reverse-Z:** Eliminates Z-flickering and allows for infinite far planes. 
    - *Note:* Depth values range from 1.0 (Near) to 0.0 (Far). Use `GREATER` comparison.
- **Indirect Rendering:** Supports GPU-driven draw calls via `DrawIndirect` and `DrawIndexedIndirect`.
- **SSBO Materials:** Material data is stored in GPU Storage Buffers instead of being pushed individually.

---

## 2. Standardized Uniforms (UBOs)
To keep shaders clean, the engine provides a `GlobalUniforms` buffer bound to **Slot 0** by default.

### `GlobalUniforms` Structure:
```cpp
struct GlobalUniforms {
    mat4 view;         // View Matrix
    mat4 proj;         // Projection Matrix
    mat4 viewProj;     // Combined VP Matrix
    vec3 cameraPos;    // World-space camera position
    float time;        // Absolute engine time
    vec2 resolution;   // Render target dimensions
    float deltaTime;   // Frame time
    uint32_t frameCount;
};
```

---

## 3. Team-Facing API: `RenderContext`
The `RenderContext` is the primary interface for recording commands.

### Common Commands:
- `BindPipeline(ptr)`: Sets shaders and state.
- `BindVertexBuffer(buffer)`: Binds vertex data.
- `BindIndexBuffer(buffer)`: Binds indices.
- `BindGraphicsStorageBuffer(slot, buffer)`: Binds an SSBO (e.g., Materials) for reading in a shader.
- `PushVertexConstants(slot, data, size)`: Fast path for small, per-object data (e.g., Model Matrix).
- `DrawIndexed(count, instances, offset)`: Standard indexed draw.
- `DrawIndexedIndirect(buffer, offset)`: Draw using parameters stored in a GPU buffer.

---

## 4. Resource Management

### `GPUBuffer`
Wraps `SDL_GPUBuffer`. Handles staging and uploading automatically.
- **Vertex:** Geometry data.
- **Index:** Triangle indices.
- **Uniform:** Constant data (UBO).
- **Compute:** Storage data (SSBO).

### `Model` & `Material`
Models automatically manage their own Vertex, Index, and Material (SSBO) buffers.
- Use `model->GetMaterialBuffer()` to bind all materials at once for a pass.

---

## 5. Implementation Example (Reverse-Z)
When creating a pipeline, ensure the depth state is configured for Reverse-Z:
```cpp
PipelineConfig config;
config.enableDepthTest = true;
config.depthCompareOp = SDL_GPU_COMPAREOP_GREATER; // REQUIRED for Reverse-Z
```

Clear values are handled automatically by the `FrameGraph` (clears to 0.0).
