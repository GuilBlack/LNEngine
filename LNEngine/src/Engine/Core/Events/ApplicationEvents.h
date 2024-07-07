#pragma once
#include "Events.h"

namespace lne
{
class AppUpdatedEvent : public Event
{
public:
    AppUpdatedEvent() = default;

    EVENT_CLASS_METHODS(AppUpdated)
};
}
