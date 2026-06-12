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

class LayerStack {
public:
    LayerStack()  = default;
    ~LayerStack();

    void PushLayer(Layer* layer);
    void PushOverlay(Layer* overlay);
    void PopLayer(Layer* layer);
    void PopOverlay(Layer* overlay);
    void Clear();

    // Iterate layers front → back (update order)
    std::vector<Layer*>::iterator       begin()       { return m_Layers.begin(); }
    std::vector<Layer*>::iterator       end()         { return m_Layers.end();   }
    // Iterate layers back → front (event propagation order: overlays first)
    std::vector<Layer*>::reverse_iterator rbegin()    { return m_Layers.rbegin(); }
    std::vector<Layer*>::reverse_iterator rend()      { return m_Layers.rend();   }

private:
    std::vector<Layer*> m_Layers;
    unsigned int        m_LayerInsertIndex = 0;
};
