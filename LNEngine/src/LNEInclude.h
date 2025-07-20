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
#include <type_traits>
#include <limits>

// Data Structures
#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <stack>
#include <queue>
#include <array>
#include <span>
#include <tuple>
#include <bitset>
#include <variant>

#define GLM_ENABLE_EXPERIMENTAL
#include "../vendor/GLM/glm/glm.hpp"
#include "../vendor/GLM/glm/gtc/quaternion.hpp"
#include "../vendor/GLM/glm/gtc/constants.hpp"
#include "../vendor/GLM/glm/gtc/type_ptr.hpp"
#include "../vendor/GLM/glm/gtc/epsilon.hpp"
#include "../vendor/GLM/glm/gtc/matrix_transform.hpp"
#include "../vendor/GLM/glm/gtx/quaternion.hpp"
#include "../vendor/GLM/glm/gtx/transform.hpp"

#include "../vendor/VKBOOTSTRAP/vkbootstrap/src/VkBootstrap.h"
#include "../vendor/IMGUI/imgui/imgui.h"

#include "Engine/Core/Utils/Defines.h"
#include "Engine/Core/Utils/Log.h"
#include "Engine/Core/Utils/Profiling.h"
#include "Engine/Core/SafePtr.h"
#include "Engine/Core/ApplicationBase.h"
#include "Engine/Core/Layer.h"
#include "Engine/Core/Events/ApplicationEvents.h"
#include "Engine/Core/Events/KeyboardEvents.h"
#include "Engine/Core/Events/MouseEvents.h"
#include "Engine/Core/Events/WindowEvents.h"
#include "Engine/Core/Inputs/InputCodes.h"
#include "Engine/Graphics/GfxContext.h"
#include "Engine/Graphics/CommandPoolManager.h"
#include "Engine/Graphics/Renderer.h"
#include "Engine/Graphics/Framebuffer.h"
#include "Engine/Graphics/Pipeline.h"
#include "Engine/Graphics/Texture.h"
#include "Engine/Graphics/DynamicDescriptorAllocator.h"
#include "Engine/Graphics/Mesh.h"
#include "Engine/Graphics/ImGui/ImGuiService.h"
#include "Engine/Graphics/Material.h"
#include "Engine/Graphics/Mesh.h"
#include "Engine/Graphics/FrameGraph/FrameGraph.h"
#include "Engine/Graphics/FrameGraph/RenderPass/RenderPasses.h"
#include "Engine/Graphics/WorldRenderer.h"

#include "Engine/Scene/Components.h"
#include "Engine/Scene/HierarchicalScene.h"
#include "Engine/Scene/Entity.h"

#include <vulkan/vulkan.hpp>
