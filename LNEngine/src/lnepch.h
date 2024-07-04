#pragma once

// Standard Library
#include <iostream>
#include <fstream>
#include <memory>
#include <utility>
#include <algorithm>
#include <functional>
#include <chrono>
#include <thread>
#include <mutex>
#include <filesystem>

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

// Third Party
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Volk/volk.h"
#include <GLFW/glfw3.h>

#include "spdlog/spdlog.h"