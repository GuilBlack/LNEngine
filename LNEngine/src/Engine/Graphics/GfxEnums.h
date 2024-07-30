#pragma once

namespace lne
{
enum class EQueueFamilyType : uint8_t
{
    Graphics,
    Compute,
    Transfer,
    Present
};
}
