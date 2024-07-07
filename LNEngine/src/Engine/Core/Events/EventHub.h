#pragma once
#include "Events.h"

namespace lne
{
template<typename T>
concept TEvent = std::derived_from<T, Event> && requires { T::GetStaticType(); };

template<TEvent EventType, typename ListenerType>
using CallbackSig = bool (ListenerType::*)(EventType&);

class EventHub
{
public:
    EventHub() = default;
    ~EventHub();

    template<TEvent EventType, typename ListenerType>
    bool RegisterListener(ListenerType* listener, CallbackSig<EventType, ListenerType> callback)
    {
        if (m_Callbacks.find(EventType::GetStaticType()) != m_Callbacks.end())
            if (m_Listeners[EventType::GetStaticType()].find(listener) != m_Listeners[EventType::GetStaticType()].end())
                return false;

        m_Callbacks[EventType::GetStaticType()].emplace_back(new Listener(listener, callback));
        m_Listeners[EventType::GetStaticType()].insert(listener);
        return true;
    }

    template<TEvent EventType>
    bool UnregisterListener(void* listenerObj)
    {
        EEventType eventType = EventType::GetStaticType();
        if (m_Callbacks.find(eventType) == m_Callbacks.end())
            return false;

        if (m_Listeners[eventType].find(listenerObj) == m_Listeners[eventType].end())
            return false;

        auto& listeners = m_Callbacks[eventType];
        for (auto it = listeners.begin(); it != listeners.end(); ++it)
        {
            if ((*it)->ListenerObj == listenerObj)
            {
                delete *it;
                listeners.erase(it);
                return true;
            }
        }

        return false;
    }

    template<TEvent EventType>
    void FireEvent(EventType& e)
    {
        for (auto& listener : m_Callbacks[EventType::GetStaticType()])
            if ((*listener)(e))
                break;
    }

private:
    struct ListenerBase
    {
        void* ListenerObj;

        virtual ~ListenerBase() = default;
        ListenerBase(void* listenerObj)
            : ListenerObj(listenerObj)
        {}
        virtual bool Fire(Event& event) = 0;

        bool operator()(Event& event)
        {
            return Fire(event);
        }
    };

    // Template listener class
    template<TEvent EventType, typename ListenerType>
    struct Listener : ListenerBase
    {
        CallbackSig<EventType, ListenerType> Callback;

        Listener(ListenerType* listenerObj, CallbackSig<EventType, ListenerType> callback)
            : ListenerBase(listenerObj), Callback(callback)
        {}

        bool Fire(Event& event) override
        {
            return (static_cast<ListenerType*>(ListenerObj)->*Callback)(static_cast<EventType&>(event));
        }
    };

    std::unordered_map<EEventType, std::unordered_set<void*>> m_Listeners;
    std::unordered_map<EEventType, std::vector<ListenerBase*>> m_Callbacks;
};
}
