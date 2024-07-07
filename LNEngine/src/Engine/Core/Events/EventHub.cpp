#include "EventHub.h"

namespace lne
{
EventHub::~EventHub()
{
    for (auto& [_, listeners] : m_Callbacks)
        for (auto& listener : listeners)
            delete listener;
}

} // namespace lne
