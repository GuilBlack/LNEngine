#include "lnepch.h"
#include "LayerStack.h"
#include "Layer.h"

namespace lne
{
LayerStack::~LayerStack()
{
    Clear();
}

void LayerStack::PushLayer(Layer* layer)
{
    layer->OnAttach();
    m_Layers.emplace(m_Layers.begin() + m_LayerInsertIndex, layer);
    m_LayerInsertIndex++;
}

void LayerStack::PushOverlay(Layer* overlay)
{
    overlay->OnAttach();
    m_Layers.emplace_back(overlay);
}

void LayerStack::PopLayer(Layer* layer)
{
    auto it = std::find(m_Layers.begin(), m_Layers.begin() + m_LayerInsertIndex, layer);
    if (it != m_Layers.begin() + m_LayerInsertIndex)
    {
        layer->OnDetach();
        m_Layers.erase(it);
        m_LayerInsertIndex--;
    }
}

void LayerStack::PopOverlay(Layer* overlay)
{
    auto it = std::find(m_Layers.begin() + m_LayerInsertIndex, m_Layers.end(), overlay);
    if (it != m_Layers.end())
    {
        overlay->OnDetach();
        m_Layers.erase(it);
    }

}
void LayerStack::Clear()
{
    for (Layer* layer : m_Layers)
    {
        layer->OnDetach();
        delete layer;
    }
    m_Layers.clear();
}
}
