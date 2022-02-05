//==============================================================================================================//
//	Project:		Vulkan Renderer
//	Description:	Utility functions related to Vulkan set up. Vulkan structs also included here.
//	Author:			Dom McCollum
//==============================================================================================================//

#pragma once

#include <vector>
#include <optional>
#include <fstream>
#include <vulkan/vulkan_core.h>

inline std::vector<char> ReadFile(const std::string& filename)
{
	// Start reading from the back, treat as binary file.
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{
		throw std::runtime_error("Failed to open file!");
	}

	// Read position is at the back so it can give us file size.
	const size_t FILE_SIZE = file.tellg();
	std::vector<char> buffer(FILE_SIZE);

	// Move read position to the front, then read file into the buffer.
	file.seekg(0);
	file.read(buffer.data(), static_cast<uint32_t>(FILE_SIZE));

	file.close();

	return buffer;
}

// Look up the Debug Utils Messenger Extension because it isn't loaded automatically.
inline VkResult CreateDebugUtilsMessengerExt
(
	VkInstance instance,
	const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks* pAllocator,
	VkDebugUtilsMessengerEXT* pDebugMessenger
)
{
	const auto FUNC = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));

	if (FUNC != nullptr)
	{
		return FUNC(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else
	{
		return VK_ERROR_EXTENSION_NOT_PRESENT;

	}
}

// Clean up debug messenger.
inline void DestroyDebugUtilsMessengerExt
(
	VkInstance instance,
	VkDebugUtilsMessengerEXT debugMessenger,
	const VkAllocationCallbacks* pAllocator
)
{
	const auto FUNC = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));

	if (FUNC != nullptr)
	{
		FUNC(instance, debugMessenger, pAllocator);
	}
}

struct QueueFamilyIndices
{
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;

	_NODISCARD bool IsComplete() const
	{
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities{};
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

