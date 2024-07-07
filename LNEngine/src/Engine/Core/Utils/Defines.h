#pragma once

template<typename T>
using OwnedPtr = std::unique_ptr<T>;

template<typename T>
using SharedPtr = std::shared_ptr<T>;

using byte = uint8_t;
