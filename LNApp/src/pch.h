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

#ifdef LNE_DEBUG
#include "crtdbg.h"
#define new   new( _NORMAL_BLOCK, __FILE__, __LINE__)
#endif

#endif // LNE_PLATFORM_WINDOWS

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
