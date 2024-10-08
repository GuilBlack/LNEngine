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
#include <cstddef>
#include <atomic>

// Data Structures
#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <stack>
#include <queue>
#include <array>
#include <tuple>

// Platform
#ifdef LNE_PLATFORM_WINDOWS
#include <Windows.h>
#endif // LNE_PLATFORM_WINDOWS

// Third Party
// Math
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Logging
#include "spdlog/spdlog.h"

// Vulkan
#include <vulkan/vulkan.hpp>
#if defined(LNE_PLATFORM_WINDOWS)
#include <vulkan/vulkan_win32.h>
#endif

// Window
#include <GLFW/glfw3.h>

#if defined(LNE_DEBUG) && defined(LNE_PLATFORM_WINDOWS)
#include "crtdbg.h"
#endif

// ImGui
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
