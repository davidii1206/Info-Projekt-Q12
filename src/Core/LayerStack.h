/**
 * @file LayerStack.h
 * @brief Ordered stack of layers with regular layers below and overlays on top.
 */

#pragma once
#include "Layer.h"
#include <vector>
#include <memory>

// Layers are updated front-to-back.
// Overlays sit on top: they are pushed after all regular layers
// and are always iterated last (so they receive events first).
//
//  [ Layer0, Layer1, ..., Overlay0, Overlay1 ]
//                    ^
//               m_LayerInsert (index separating layers from overlays)

/**
 * @class LayerStack
 * @brief Owns and orders Layer instances.
 *
 * Regular layers are kept before overlays. Update order is front→back;
 * event propagation order is back→front (overlays handle events first).
 */
class LayerStack {
public:
    LayerStack()  = default; ///< Default constructor.
    ~LayerStack();           ///< Destroys and frees all owned layers.

    /// @brief Inserts a regular layer (before the overlays).
    void PushLayer(Layer* layer);
    /// @brief Pushes an overlay (kept on top of regular layers).
    void PushOverlay(Layer* overlay);
    /// @brief Removes a previously pushed regular layer.
    void PopLayer(Layer* layer);
    /// @brief Removes a previously pushed overlay.
    void PopOverlay(Layer* overlay);
    /// @brief Removes and frees all layers.
    void Clear();

    /// @return Iterator to the first layer (front, update order).
    std::vector<Layer*>::iterator       begin()       { return m_Layers.begin(); }
    /// @return Iterator past the last layer.
    std::vector<Layer*>::iterator       end()         { return m_Layers.end();   }
    /// @return Reverse iterator (back→front, event-propagation order).
    std::vector<Layer*>::reverse_iterator rbegin()    { return m_Layers.rbegin(); }
    /// @return Reverse end iterator.
    std::vector<Layer*>::reverse_iterator rend()      { return m_Layers.rend();   }

private:
    std::vector<Layer*> m_Layers;            ///< Owned layers (regular then overlays).
    unsigned int        m_LayerInsertIndex = 0; ///< Boundary between layers and overlays.
};
