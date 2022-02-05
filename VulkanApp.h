//==============================================================================================================//
//	Project:		Vulkan Renderer
//	Description:	Vulkan App framework, created as part of learning Vulkan API.
//	Author:			Dom McCollum
//==============================================================================================================//

#pragma once
#include "VulkanUtils.h"

#define GLFW_INCLUDE_VULKAN	// Loads GLFW funtionality and vulkan.h
#include <GLFW/glfw3.h>		//

#include <vector>

#ifdef NDEBUG
constexpr bool g_enableValidationLayers = false;
#else
constexpr bool g_enableValidationLayers = true;
#endif

class VulkanApp
{
public:

	VulkanApp() :
		m_window(nullptr),
		m_vulkanInstance(nullptr),
		m_debugMessenger(nullptr),
		m_surface(nullptr),
		m_physicalDevice(VK_NULL_HANDLE), // Syntatic sugar for nullptr.
		m_device(nullptr),
		m_graphicsQueue(nullptr),
		m_presentQueue(nullptr),
		m_currentSwapChain(nullptr),
		m_oldSwapChain(nullptr),
		m_swapChainImageFormat(VK_FORMAT_UNDEFINED),
		m_swapChainExtent({ 0, 0 }),
		m_renderPass(nullptr),
		m_pipelineLayout(nullptr),
		m_graphicsPipeline(nullptr),
		m_commandPool(nullptr),
		m_currentFrame(0),
		m_framebufferResized(false)
	{}

	void Run();

private:

	//=======================================================================================================================
	//													Functions
	//=======================================================================================================================

	// Core app functions.

	void InitWindow();
	void InitVulkan();
	void MainLoop();
	void CleanUp();

	// Initialisation.

	void CreateInstance();
	void SetupDebugMessenger();
	void CreateSurface();
	void PickPhysicalDevice();
	void CreateLogicalDevice();
	void CreateSwapChain();
	void CreateImageViews();
	void CreateRenderPass();
	void CreateGraphicsPipeline();
	void CreateFramebuffers();
	void CreateCommandPool();
	void CreateCommandBuffers();
	void CreateSyncObjects();

	void DrawFrame();

	// Needed when swap chain becomes incompatible, during window resize for example.
	void RecreateSwapChain();
	void CleanupSwapChain();
	static void FrameBufferResizeCallBack(GLFWwindow* window, int width, int height);

	// Checks used during instance creation.
	bool CheckValidationLayerSupport();
	std::vector<const char*> GetRequiredExtensions();

	// Debug message handler
	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallBack
	(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallBackData,
		void* pUserData
	);
	void PopulateDebugInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

	// Device selection
	uint32_t RateDevice(VkPhysicalDevice device);
	bool DeviceSuitable(VkPhysicalDevice device);
	bool DeviceExtensionSupport(VkPhysicalDevice device);

	// Swap chain creation
	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR ChooseSwapPresentMode(const std::vector <VkPresentModeKHR>& availableModes);
	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
	VkShaderModule CreateShaderModule(const std::vector<char>& code);

	//=======================================================================================================================
	//													Variables
	//=======================================================================================================================

	GLFWwindow* m_window;
	VkInstance	m_vulkanInstance;				// This is actually a wrapped up pointer!
	VkDebugUtilsMessengerEXT m_debugMessenger;	// And this is too!
	VkSurfaceKHR m_surface;
	VkPhysicalDevice m_physicalDevice;			// Destroyed implicitly when Vulkan instance is destroyed.
	VkDevice m_device;							// Here be pointers.
	VkQueue m_graphicsQueue;					// Implictly destroyed when logical device is destroyed.
	VkQueue m_presentQueue;

	// Swap chain.
	VkSwapchainKHR m_currentSwapChain;
	VkSwapchainKHR m_oldSwapChain;
	std::vector<VkImage> m_vSwapChainImages;	// Implicitly destroyed when swap chain is destroyed.
	VkFormat m_swapChainImageFormat;
	VkExtent2D m_swapChainExtent;
	std::vector<VkImageView> m_vSwapChainImageViews;

	// Rendering and pipeline.
	VkRenderPass m_renderPass;
	VkPipelineLayout m_pipelineLayout;
	VkPipeline m_graphicsPipeline;

	// Frame buffers
	std::vector<VkFramebuffer> m_vSwapChainFramebuffers;

	// Command pool
	VkCommandPool m_commandPool;
	std::vector<VkCommandBuffer> m_vCommandBuffers;

	// Sync objects
	std::vector<VkSemaphore> m_vImageAvailableSemaphores;
	std::vector<VkSemaphore> m_vRenderFinishedSemaphores;
	std::vector<VkFence> m_vFences;
	std::vector<VkFence> m_vImagesInFlight;
	uint32_t m_currentFrame;

	// Explicit resize variable required because VK_ERROR_OUT_OF_DATE_KHR is not guaranteed to be triggered on all systems.
	bool m_framebufferResized;
};

