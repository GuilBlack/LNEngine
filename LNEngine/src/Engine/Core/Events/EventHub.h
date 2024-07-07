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

    /// <summary>
    /// Register a listener to listen to a specific event type. The listener's callback must be a member function of the listener class.
    /// Any listeners subscribed from the engine code will have a priority that is >= 0.
    /// there can only be ONE callback per listener per event type.
    /// </summary>
    /// <typeparam name="ListenerType">Type of the listener (can be any time)</typeparam>
    /// <typeparam name="EventType">Type of the event. It MUST be derived from the Event class</typeparam>
    /// <param name="listener">: a pointer to the listener's object</param>
    /// <param name="callback">: a member function of the listener's type</param>
    /// <param name="priority">: a number specifying the priority of the event. The lower the number, the earlier it will be called</param>
    /// <returns>If false, the listener already a callback bound the the specified event type.</returns>
    template<TEvent EventType, typename ListenerType>
    bool RegisterListener(ListenerType* listener, CallbackSig<EventType, ListenerType> callback, int priority = 0)
    {
        if (m_Callbacks.find(EventType::GetStaticType()) != m_Callbacks.end())
            if (m_Listeners[EventType::GetStaticType()].find(listener) != m_Listeners[EventType::GetStaticType()].end())
                return false;

        auto& listenerCallbacks = m_Callbacks[EventType::GetStaticType()];

        auto it = std::lower_bound(listenerCallbacks.begin(), listenerCallbacks.end(), priority,
            [](ListenerBase* listener, int priority) { return listener->Priority < priority; });

        listenerCallbacks.insert(it, new Listener<EventType, ListenerType>(listener, callback, priority));

        m_Listeners[EventType::GetStaticType()].insert(listener);

        return true;
    }

    /// <summary>
    /// Unregister a listener from listening to a specific event type.
    /// </summary>
    /// <typeparam name="EventType">Specific event type</typeparam>
    /// <param name="listenerObj">: a pointer to the listener's object</param>
    /// <returns>If false, the listener didn't have a callback associated to the specified event type</returns>
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

    /// <summary>
    /// handles the event by calling the appropriate callbacks
    /// </summary>
    /// <typeparam name="EventType">Type of the event that's going to be fired</typeparam>
    /// <param name="e">Event</param>
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
        int Priority;

        virtual ~ListenerBase() = default;
        ListenerBase(void* listenerObj, int priority)
            : ListenerObj(listenerObj), Priority(priority)
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

        Listener(ListenerType* listenerObj, CallbackSig<EventType, ListenerType> callback, int priority)
            : ListenerBase(listenerObj, priority), Callback(callback)
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
