//==============================================================================================================//
//	Project:		Vulkan Renderer
//	Description:	Useful constants.
//	Author:			Dom McCollum
//==============================================================================================================//

#pragma once

namespace Render_constants
{
	// Window size
	constexpr inline uint32_t g_windowWidth = 800;
	constexpr inline uint32_t g_windowHeight = 600;

	constexpr uint32_t g_maxFramesInFlight = 2;
}

namespace Validation_constants
{
	const std::vector<const char*> g_vLayers = { "VK_LAYER_KHRONOS_validation" };
}

namespace Extension_constants
{
	const std::vector<const char*> g_vDeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
}

