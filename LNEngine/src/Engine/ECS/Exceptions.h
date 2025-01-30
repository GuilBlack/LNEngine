#pragma once
// This piece of code is from my other GitHub project: https://github.com/GuilBlack/ECS

namespace lne
{
class NoComponentException : public std::exception
{
public:
    NoComponentException()
        : exception("This entity has no component with this type attached to it.")
    {}
};

class EntityIDOutOfRange : public std::exception
{
public:
    EntityIDOutOfRange()
        : exception("EntityID is too big.")
    {}
};

class MaxEntityCountReached : public std::exception
{
public:
    MaxEntityCountReached()
        : exception("Max entity count reached.")
    {}
};

class InvalidComponentTypeException : public std::exception
{
public:
    InvalidComponentTypeException()
        : exception("The given component type doesn't exist as a storage!")
    {}
};

class ComponentAlreadyExistsException : public std::exception
{
public:
    ComponentAlreadyExistsException()
        : exception("This entity already has a component of this type.")
    {}
};
}
