#pragma once
#include "../vendor/GLM/glm/glm.hpp"
#include "../vendor/GLM/glm/gtc/constants.hpp"
#include "../vendor/GLM/glm/gtc/matrix_transform.hpp"

#include "../vendor/VKBOOTSTRAP/vkbootstrap/src/VkBootstrap.h"
#include "Engine/Core/Utils/Defines.h"
#include "Engine/Core/Utils/Log.h"
#include "Engine/Core/SafePtr.h"
#include "Engine/Core/ApplicationBase.h"
#include "Engine/Core/Layer.h"
#include "Engine/Core/Events/ApplicationEvents.h"
#include "Engine/Core/Events/KeyboardEvents.h"
#include "Engine/Core/Events/MouseEvents.h"
#include "Engine/Core/Events/WindowEvents.h"
#include "Engine/Core/Inputs/InputCodes.h"
#include "Engine/Graphics/GfxContext.h"
#include "Engine/Graphics/Renderer.h"
#include "Engine/Graphics/Framebuffer.h"
#include "Engine/Graphics/Pipeline.h"
#include "Engine/Graphics/Texture.h"
#include "Engine/Graphics/DynamicDescriptorAllocator.h"
#include "Engine/Graphics/Mesh.h"
#include "Engine/Graphics/ImGui/ImGuiService.h"
#include "Engine/Graphics/Material.h"
#include "Engine/Scene/Components.h"

#include <vulkan/vulkan.hpp>

// Entry point
#include "Engine/Main.h"
