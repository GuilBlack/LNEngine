#pragma once

// Standard Library
#include <iostream>
#include <memory>
#include <utility>
#include <algorithm>
#include <functional>

// Data Structures
#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <unordered_set>

// Platform
#ifdef LNE_PLATFORM_WINDOWS
#include <Windows.h>
#endif // LNE_PLATFORM_WINDOWS

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
