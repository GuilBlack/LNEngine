#pragma once

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

#include <vulkan/vulkan.hpp>

// Entry point
#include "Engine/Main.h"
