#include "Layer.h"

std::ostream& operator<<(std::ostream& os, const lne::Layer& layer)
{
    os << layer.GetName();
    return os;
}
