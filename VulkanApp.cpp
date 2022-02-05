//==============================================================================================================//
//	Project:		Vulkan Renderer
//	Description:	Vulkan App framework, created as part of learning Vulkan API.
//	Author:			Dom McCollum
//==============================================================================================================//

#include "../Sandbox/TestBench/DMC.h"
#include "VulkanApp.h"
#include "VulkanUtils.h"
#include "Constants.h"

#include <iostream>		// Capture error reporting
#include <cassert>		//
#include <map>
#include <set>
#include <algorithm>	// Necessary for std::clamp
#include <optional>

// Error reporting
#define ASSERT(condition, message)	DMC::UTILS::ReportError(condition, message)

// Constants defined as macros purely to aid readability.
#define WINDOW_W					Render_constants::g_windowWidth
#define WINDOW_H					Render_constants::g_windowHeight
#define MAX_FRAMES_IN_FLIGHT		Render_constants::g_maxFramesInFlight
#define V_LAYERS					Validation_constants::g_vLayers
#define V_EXTENS					Extension_constants::g_vDeviceExtensions

void VulkanApp::Run()
{
	InitWindow();
	InitVulkan();
	MainLoop();
	CleanUp();
}

// Core app functions.

void VulkanApp::InitWindow()
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Tells GLFW not to use OpenGL context.
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	// Monitor can be selected using 4th parameter, 5th parameter is OpenGL exclusive.
	m_window = glfwCreateWindow(WINDOW_W, WINDOW_H, "Look at this funky triangle!", nullptr, nullptr);

	// Assign a function for resize callback.
	glfwSetWindowUserPointer(m_window, this);
	glfwSetFramebufferSizeCallback(m_window, FrameBufferResizeCallBack);
}

void VulkanApp::InitVulkan()
{
	CreateInstance();
	SetupDebugMessenger();
	CreateSurface();
	PickPhysicalDevice();
	CreateLogicalDevice();
	CreateSwapChain();
	CreateImageViews();
	CreateRenderPass();
	CreateGraphicsPipeline(); // Possible to avoid when using dynamic state for viewports and scissor rects.
	CreateFramebuffers();
	CreateCommandPool();
	CreateCommandBuffers();
	CreateSyncObjects();
}

void VulkanApp::MainLoop()
{
	while (!glfwWindowShouldClose(m_window))
	{
		glfwPollEvents();
		DrawFrame();
	}

	vkDeviceWaitIdle(m_device);
}

void VulkanApp::CleanUp()
{
	CleanupSwapChain();

	// Destroy sync objects.
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vkDestroySemaphore(m_device, m_vImageAvailableSemaphores[i], nullptr);
		vkDestroySemaphore(m_device, m_vRenderFinishedSemaphores[i], nullptr);
		vkDestroyFence(m_device, m_vFences[i], nullptr);
	}

	// Destroy command pool.
	vkDestroyCommandPool(m_device, m_commandPool, nullptr);

	// Destroy virtual device
	vkDestroyDevice(m_device, nullptr);

	// Does nothing in release mode.
	if (g_enableValidationLayers)
	{
		DestroyDebugUtilsMessengerExt(m_vulkanInstance, m_debugMessenger, nullptr);
	}

	vkDestroySurfaceKHR(m_vulkanInstance, m_surface, nullptr);
	vkDestroyInstance(m_vulkanInstance, nullptr);
	glfwDestroyWindow(m_window);
	glfwTerminate();
}

// =================================================================================================================================================================
// Initialisation

void VulkanApp::CreateInstance()
{
	uint32_t retcode = VK_SUCCESS;

	if (g_enableValidationLayers && !CheckValidationLayerSupport())
	{
		ASSERT(false, "Validation layers requested, but are not available!");
	}

	VkApplicationInfo appInfo{}; // Technically optional, but can be used by driver to perform optimisations.
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Hello Triangle";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo{}; // Select global extensions and validation layers.
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	// Retrieve the extensions required to interface with GLFW window.
	std::vector<const char*> extensions = GetRequiredExtensions();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	if (g_enableValidationLayers)
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(V_LAYERS.size());
		createInfo.ppEnabledLayerNames = V_LAYERS.data();

		PopulateDebugInfo(debugCreateInfo);
		createInfo.pNext = static_cast<VkDebugUtilsMessengerCreateInfoEXT*>(&debugCreateInfo);
	}
	else
	{
		createInfo.enabledLayerCount = 0;
		createInfo.pNext = nullptr;
	}

	// 2nd parameter of vkCreateInstance is used with custom allocator callbacks.
	retcode = vkCreateInstance(&createInfo, nullptr, &m_vulkanInstance);
	ASSERT(retcode == VK_SUCCESS, "Failed to create instance!");
}

void VulkanApp::SetupDebugMessenger()
{
	uint32_t retcode = VK_SUCCESS;

	// Do nothing in release mode.
	if constexpr (!g_enableValidationLayers)
	{
		return;
	}

	VkDebugUtilsMessengerCreateInfoEXT createInfo{};
	PopulateDebugInfo(createInfo);

	// Debug messenger is specific to the instance, so it must be specified in the first parameter.
	retcode = CreateDebugUtilsMessengerExt(m_vulkanInstance, &createInfo, nullptr, &m_debugMessenger);
	ASSERT(retcode == VK_SUCCESS, "Failed to set up debug messenger!");
}

void VulkanApp::CreateSurface()
{
	uint32_t retcode = VK_SUCCESS;

	retcode = glfwCreateWindowSurface(m_vulkanInstance, m_window, nullptr, &m_surface);
	ASSERT(retcode == VK_SUCCESS, "Failed to create window surface!");
}

void VulkanApp::PickPhysicalDevice()
{
	// Use to check return codes.
	uint32_t retCode = VK_SUCCESS;

	// How many devices?
	uint32_t deviceCount = 0;
	retCode = vkEnumeratePhysicalDevices(m_vulkanInstance, &deviceCount, nullptr);
	assert(retCode == VK_SUCCESS);

	ASSERT(deviceCount != 0, "Failed to find GPU with Vulkan support");

	// Get device data.
	std::vector<VkPhysicalDevice> devices(deviceCount);
	retCode = vkEnumeratePhysicalDevices(m_vulkanInstance, &deviceCount, devices.data());
	ASSERT(retCode == VK_SUCCESS, "No physical devices available!");

	std::multimap<uint32_t, VkPhysicalDevice> candidates;
	for (const auto& device : devices)
	{
		uint32_t score = RateDevice(device);
		candidates.insert(std::make_pair(score, device));
	}

	// It's possible for the highest scoring GPU to score 0 because it's not suitable.
	ASSERT(candidates.rbegin()->first > 0, "Failed to find suitable GPU!");

	m_physicalDevice = candidates.rbegin()->second;
}

void VulkanApp::CreateLogicalDevice()
{
	uint32_t retcode = VK_SUCCESS;

	QueueFamilyIndices indices = FindQueueFamilies(m_physicalDevice);

	// Must create a queue from all unique families.
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

	constexpr float QUEUE_PRIORITY = 1.f;
	for (uint32_t queueFamily : uniqueQueueFamilies)
	{
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		// Required even if there's only one queue.
		queueCreateInfo.pQueuePriorities = &QUEUE_PRIORITY;

		queueCreateInfos.push_back(queueCreateInfo);
	}

	// Defined for future use in more complex apps.
	VkPhysicalDeviceFeatures deviceFeatures{};

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.pEnabledFeatures = &deviceFeatures;
	createInfo.enabledExtensionCount = static_cast<uint32_t>(V_EXTENS.size());
	createInfo.ppEnabledExtensionNames = V_EXTENS.data();

	// Set validation layers to support older Vulkan implementations.
	// Modern implementations ignore these values as no distinction is made between instance and device validation layers.
	if (g_enableValidationLayers)
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(V_LAYERS.size());
		createInfo.ppEnabledLayerNames = V_LAYERS.data();
	}
	else
	{
		createInfo.enabledLayerCount = 0;
	}

	retcode = vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device);
	ASSERT(retcode == VK_SUCCESS, "Failed to create logical device!");

	vkGetDeviceQueue(m_device, indices.graphicsFamily.value(), 0, &m_graphicsQueue);
	vkGetDeviceQueue(m_device, indices.presentFamily.value(), 0, &m_presentQueue);
}

void VulkanApp::CreateSwapChain()
{
	uint32_t retcode = VK_SUCCESS;

	const SwapChainSupportDetails SWAP_CHAIN_SUPPORT = QuerySwapChainSupport(m_physicalDevice);

	VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(SWAP_CHAIN_SUPPORT.formats);
	VkPresentModeKHR presentMode = ChooseSwapPresentMode(SWAP_CHAIN_SUPPORT.presentModes);
	VkExtent2D extent = ChooseSwapExtent(SWAP_CHAIN_SUPPORT.capabilities);

	// Requesting just the minimum could cause us to have to wait for the driver to complete internal operations.
	uint32_t imageCount = SWAP_CHAIN_SUPPORT.capabilities.minImageCount + 1;

	// Ensure we don't go over the max image count. A max of zero would indicate that there is no max.
	if (SWAP_CHAIN_SUPPORT.capabilities.maxImageCount > 0 && imageCount > SWAP_CHAIN_SUPPORT.capabilities.maxImageCount)
	{
		imageCount = SWAP_CHAIN_SUPPORT.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = m_surface; // Ties the swap chain to the surface.
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1; // Number of layers of which, each image consists. Always 1 unless developing stereoscopic 3D.
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	QueueFamilyIndices indices = FindQueueFamilies(m_physicalDevice);
	uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

	// It's likely these families are the same, but we should cover the possibility they're not.
	if (indices.graphicsFamily != indices.presentFamily)
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // Offers better performance.
		createInfo.queueFamilyIndexCount = 0; // Optional
		createInfo.pQueueFamilyIndices = nullptr; // Optional
	}

	createInfo.preTransform = SWAP_CHAIN_SUPPORT.capabilities.currentTransform; // Specify a transform applied to the image.
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // Ignore alpha channel.
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = m_oldSwapChain; // Swap chains may become invalid in certain circumstances. Reference the old swap chain here if that happens.

	retcode = vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_currentSwapChain);
	ASSERT(retcode == VK_SUCCESS, "Failed to create swap chain!");

	// Resize the images container to the actual number becuase only a minimum number was set earlier.
	vkGetSwapchainImagesKHR(m_device, m_currentSwapChain, &imageCount, nullptr);
	m_vSwapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(m_device, m_currentSwapChain, &imageCount, m_vSwapChainImages.data()); // Get the image handles.

	m_swapChainImageFormat = surfaceFormat.format;
	m_swapChainExtent = extent;
}

void VulkanApp::CreateImageViews()
{
	m_vSwapChainImageViews.resize(m_vSwapChainImages.size());

	for (uint32_t i = 0; i < m_vSwapChainImageViews.size(); ++i)
	{
		VkImageViewCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = m_vSwapChainImages[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;	// How to interpret the image
		createInfo.format = m_swapChainImageFormat;	//
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;	// USed to map colour channels
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;	//
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;	//
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;	//
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;	// Image purpose and which part should be accessed.
		createInfo.subresourceRange.baseMipLevel = 0;							//
		createInfo.subresourceRange.levelCount = 1;							//
		createInfo.subresourceRange.baseArrayLayer = 0;							//
		createInfo.subresourceRange.layerCount = 1;							//

		if (vkCreateImageView(m_device, &createInfo, nullptr, &m_vSwapChainImageViews[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create image views!");
		}
	}
}

void VulkanApp::CreateRenderPass()
{
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = m_swapChainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;			// No multisampling so set to 1.
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;		// Clear frame buffer to black before drawing new frame.
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;		// We want to see the triangle so store the attachment data.
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;	// There is no stenciling, so what happens to stenciling data is irrelevant.
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;	//
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;		// Previous image layout is irrelevent. CAUTION: Contents of the image are not guaranteed to be preserved.
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;	// Image will be presented using the swap chain.

	// Using a single subpass.
	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;										// Only one reference so its index is 0.
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // Best performance for intended use as colour buffer.

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

	if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create render pass!");
	}

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;
}

void VulkanApp::CreateGraphicsPipeline()
{
	std::vector<char> vertShaderCode = ReadFile("shaders/vert.spv");
	std::vector<char> fragShaderCode = ReadFile("shaders/frag.spv");

	// Only required until GFX pipeline is set up so can be destroyed at the end, rather than become class members.
	VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);
	VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

	// Shaders must be assigned to a pipeline stage for use.
	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;	// Stage where the shader is used.
	vertShaderStageInfo.module = vertShaderModule;				// Module with the relevant code.
	vertShaderStageInfo.pName = "main";						// Define entry point.
	vertShaderStageInfo.pSpecializationInfo = nullptr;						// Used to define shader constants.

	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule;
	fragShaderStageInfo.pName = "main";
	fragShaderStageInfo.pSpecializationInfo = nullptr;

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	// Used for vertex buffer.
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE; // When true it's possible to break up the primitive types.

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(m_swapChainExtent.width);	// Use swap chain size as it may differ from window.
	viewport.height = static_cast<float>(m_swapChainExtent.height);	//
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	// Scissor rects can be used to define in which regions pizels are drawn, here we just draw the whole frame buffer.
	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = m_swapChainExtent;

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE; // Useful for shadow maps.

	/*
		VK_POLYGON_MODE_FILL: Fill the area of the polygon with fragments.
		VK_POLYGON_MODE_LINE: Polygon edges are drawn as lines.
		VK_POLYGON_MODE_POINT: Polygon vertices are drawn as points.
	 */
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE; // USed in shadow mapping.
	rasterizer.depthBiasConstantFactor = 0.0f;		//
	rasterizer.depthBiasClamp = 0.0f;		//
	rasterizer.depthBiasSlopeFactor = 0.0f;		//

	// Multisampling is much cheaper than downscaling from a higher resolution.
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f;
	multisampling.pSampleMask = nullptr;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable = VK_FALSE;

	// Colour blending
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	// Must provide a pipeline even though it's not used yet.
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 0;
	pipelineLayoutInfo.pSetLayouts = nullptr;
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create pipeline layout!");
	}

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;

	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = nullptr;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = nullptr;
	pipelineInfo.layout = m_pipelineLayout;
	pipelineInfo.renderPass = m_renderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;	// Used to create new pipeline by inheriting from an old pipeline for improved performance.
	pipelineInfo.basePipelineIndex = -1;				//

	// The second parameter refers to a pipeline cache that can be used to significantly speed up pipeline creation, even across apps when stored to a file.
	if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create graphics pipeline!");
	}

	vkDestroyShaderModule(m_device, fragShaderModule, nullptr);
	vkDestroyShaderModule(m_device, vertShaderModule, nullptr);
}

void VulkanApp::CreateFramebuffers()
{
	m_vSwapChainFramebuffers.resize(m_vSwapChainImageViews.size());

	// Iterate through the image views and create fram buffers from them.
	for (uint32_t i = 0; i < m_vSwapChainImageViews.size(); i++)
	{
		VkImageView attachments[] =
		{
			m_vSwapChainImageViews[i]
		};

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = m_renderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = m_swapChainExtent.width;
		framebufferInfo.height = m_swapChainExtent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_vSwapChainFramebuffers[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create framebuffer!");
		}
	}
}

void VulkanApp::CreateCommandPool()
{
	QueueFamilyIndices queueFamilyIndices = FindQueueFamilies(m_physicalDevice);

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value(); // Submitting commands for drawing requires the graphics family.
	poolInfo.flags = 0;

	if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create command pool!");
	}
}

void VulkanApp::CreateCommandBuffers()
{
	m_vCommandBuffers.resize(m_vSwapChainFramebuffers.size());

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = static_cast<uint32_t>(m_vCommandBuffers.size());

	if (vkAllocateCommandBuffers(m_device, &allocInfo, m_vCommandBuffers.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate command buffers!");
	}

	for (size_t i = 0; i < m_vCommandBuffers.size(); i++)
	{
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0;
		beginInfo.pInheritanceInfo = nullptr; // Only relevant for secondary command buffers.

		if (vkBeginCommandBuffer(m_vCommandBuffers[i], &beginInfo) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to begin recording command buffer!");
		}

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = m_renderPass;
		renderPassInfo.framebuffer = m_vSwapChainFramebuffers[i];
		renderPassInfo.renderArea.offset = { 0, 0 };	// Defines the render area, should match attachments for best performance.
		renderPassInfo.renderArea.extent = m_swapChainExtent;//

		VkClearValue clearColor = { {{0.52f, 0.63f, 0.95f, 1.0f}} }; // Clear to pastel blue.
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearColor;

		vkCmdBeginRenderPass(m_vCommandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(m_vCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
		vkCmdDraw(m_vCommandBuffers[i], 3, 1, 0, 0); // Draw the blooming triangle! (And it's about time too!)
		vkCmdEndRenderPass(m_vCommandBuffers[i]); // End the render pass.

		if (vkEndCommandBuffer(m_vCommandBuffers[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to record command buffer!");
		}
	}
}

void VulkanApp::CreateSyncObjects()
{
	m_vImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	m_vRenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	m_vFences.resize(MAX_FRAMES_IN_FLIGHT);
	m_vImagesInFlight.resize(m_vSwapChainImageViews.size(), VK_NULL_HANDLE);

	// Current API requires this struct, but it only defines what type of info struct it is.
	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	// Created in the signalled state, otherwise DrawFrame will wait forever.
	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_vImageAvailableSemaphores[i]) != VK_SUCCESS
			|| vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_vRenderFinishedSemaphores[i]) != VK_SUCCESS
			|| vkCreateFence(m_device, &fenceInfo, nullptr, &m_vFences[i]) != VK_SUCCESS)
		{

			throw std::runtime_error("Failed to create semaphores for a frame!");
		}
	}
}

void VulkanApp::DrawFrame()
{
	// Wait for frame
	vkWaitForFences(m_device, 1, &m_vFences[m_currentFrame], VK_TRUE, UINT64_MAX); // Will wait for all fences, with no timeout.

	// Get an image from the swap chain.
	uint32_t imageIndex;

	// Using UINT64_MAX disables the timeout.
	VkResult result = vkAcquireNextImageKHR(m_device, m_currentSwapChain, UINT64_MAX, m_vImageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);

	// Usually happens because the window was resized.
	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		RecreateSwapChain();
		return;
	}

	// Check for successful image acquasition. Suboptimal is considered a success return code.
	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		throw std::runtime_error("Failed to acquire swap chain image!");
	}

	// Check if a previous frame is using this image.
	if (m_vImagesInFlight[imageIndex] != VK_NULL_HANDLE)
	{
		vkWaitForFences(m_device, 1, &m_vImagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
	}

	// Mark the image as in use by this frame.
	m_vImagesInFlight[imageIndex] = m_vFences[m_currentFrame];

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = { m_vImageAvailableSemaphores[m_currentFrame] };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_vCommandBuffers[imageIndex];

	VkSemaphore signalSemaphores[] = { m_vRenderFinishedSemaphores[m_currentFrame] };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	// Reset the fence.
	vkResetFences(m_device, 1, &m_vFences[m_currentFrame]);
	if (vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_vFences[m_currentFrame]) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to submit draw command buffer!");
	}

	// Submit frame back to swap chain for presentation to screen.
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapChains[] = { m_currentSwapChain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr;

	// Check if a previous frame is using this image.
	if (m_vImagesInFlight[imageIndex] != VK_NULL_HANDLE)
	{
		vkWaitForFences(m_device, 1, &m_vImagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
	}

	// Check presentation return codes.
	result = vkQueuePresentKHR(m_presentQueue, &presentInfo);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_framebufferResized)
	{
		// Reset local resize flag.
		m_framebufferResized = false;
		RecreateSwapChain();
	}
	else if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to present swap chain image!");
	}

	vkQueueWaitIdle(m_presentQueue);

	m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

// =================================================================================================================================================================
// Needed when swap chain becomes incompatible, during window resize for example.
void VulkanApp::RecreateSwapChain()
{
	// Handle minimisation as a special case of resizing.
	int width = 0, height = 0;
	glfwGetFramebufferSize(m_window, &width, &height);
	while (width == 0 || height == 0)
	{
		glfwGetFramebufferSize(m_window, &width, &height);
		glfwWaitEvents();
	}

	/*	We need to clean up the old swap chain, but we don't want
		to destroy any resources that are still in use so we wait.
	*/
	vkDeviceWaitIdle(m_device);
	CleanupSwapChain();

	CreateSwapChain();
	CreateImageViews();
	CreateRenderPass();
	CreateGraphicsPipeline();
	CreateFramebuffers();
	CreateCommandBuffers();
}

void VulkanApp::CleanupSwapChain()
{
	// Destroy framebuffers.
	for (auto framebuffer : m_vSwapChainFramebuffers)
	{
		vkDestroyFramebuffer(m_device, framebuffer, nullptr);
	}

	// Clean up existing command buffers, keeping the pool intact for future use.
	vkFreeCommandBuffers(m_device, m_commandPool, static_cast<uint32_t>(m_vCommandBuffers.size()), m_vCommandBuffers.data());

	// Destroy Pipeline.
	vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);

	// Destroy Render Pass.
	vkDestroyRenderPass(m_device, m_renderPass, nullptr);

	// Destroy image views.
	for (auto view : m_vSwapChainImageViews)
	{
		vkDestroyImageView(m_device, view, nullptr);
	}

	vkDestroySwapchainKHR(m_device, m_oldSwapChain, nullptr);
	vkDestroySwapchainKHR(m_device, m_currentSwapChain, nullptr);
}

void VulkanApp::FrameBufferResizeCallBack(GLFWwindow* window, int width, int height)
{
	VulkanApp* app = reinterpret_cast<VulkanApp*>(glfwGetWindowUserPointer(window));
	app->m_framebufferResized = true;
}


// =================================================================================================================================================================
// Checks used during instance creation.

bool VulkanApp::CheckValidationLayerSupport()
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	assert(vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data()) == VK_SUCCESS);

	// If at any point a validation layer is missing, we return false from within this loop.
	for (const char* layerName : V_LAYERS)
	{
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers)
		{
			if (strcmp(layerName, layerProperties.layerName) == 0)
			{
				layerFound = true;
				break;
			}
		}

		if (!layerFound)
		{
			return false;
		}
	}

	return true;
}

std::vector<const char*> VulkanApp::GetRequiredExtensions()
{
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	if (g_enableValidationLayers)
	{
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	return extensions;
}

// =================================================================================================================================================================
// Debug message handler

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanApp::DebugCallBack
(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallBackData,
	void* pUserData
)
{
	std::cerr << "Validation Layer: " << pCallBackData->pMessage << std::endl;

	return VK_FALSE;
}

void VulkanApp::PopulateDebugInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
	createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |			// Select which sevarities the callback is called for.
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |				// Select the types of messages the callback notified about.
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

	createInfo.pfnUserCallback = DebugCallBack;
	createInfo.pUserData = nullptr;
}

// =================================================================================================================================================================
// Device selection

uint32_t VulkanApp::RateDevice(VkPhysicalDevice device)
{
	uint32_t score = 0;

	// Support for basic properties like name, type and Vulkan version.
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);

	// Support for optional features, such as, texture comnpression, 64-bit floats, and multi-viewport rendering.
	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	// No geometry shader would make the device totally unsuitable, as would lack of required queue families.
	if (!deviceFeatures.geometryShader || !DeviceSuitable(device))
	{
		return 0;
	}

	// Discrete GPU's are preferable.
	if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
	{
		score += 1000;
	}

	// As are GPU's that can output the highest gfx quality.
	score += deviceProperties.limits.maxImageDimension2D;

	return score;
}

bool VulkanApp::DeviceSuitable(VkPhysicalDevice device)
{
	const QueueFamilyIndices INDICES = FindQueueFamilies(device);
	const bool EXTENSIONS_SUPPORTED = DeviceExtensionSupport(device);
	bool swapChainSupportAdequate = false;

	if (EXTENSIONS_SUPPORTED)
	{
		SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
		swapChainSupportAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}

	// Going mad or is EXTENSIONS_SUPPORTED unecessary at this point?
	return INDICES.IsComplete() && EXTENSIONS_SUPPORTED && swapChainSupportAdequate;
}

bool VulkanApp::DeviceExtensionSupport(VkPhysicalDevice device)
{
	// Get extension count.
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	// Get extension names.
	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	// Tick off avilable extensions against those that are required.
	std::set<std::string> requiredExtensions(V_EXTENS.begin(), V_EXTENS.end());
	for (const auto& extension : availableExtensions)
	{
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}

// =================================================================================================================================================================
// Swap chain creation.

// Find the GFX queue family.
QueueFamilyIndices VulkanApp::FindQueueFamilies(VkPhysicalDevice device)
{
	QueueFamilyIndices indices{};

	// Types of operations supported and number of queues that can be created based on that queue family.
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	uint32_t i = 0;
	for (const auto& queueFamily : queueFamilies)
	{
		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);

		if (presentSupport)
		{
			indices.presentFamily = i;
		}

		// Require at least one queue family that supports GFX bit.
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			indices.graphicsFamily = i;
		}

		// Break if we have at least one of each required family.
		if (indices.IsComplete())
		{
			break;
		}

		i++;
	}

	return indices;
}

SwapChainSupportDetails VulkanApp::QuerySwapChainSupport(VkPhysicalDevice device)
{
	SwapChainSupportDetails details;

	// The physical device and the surface are included in all swap chain queries as they are core components of the swap chain.
	// Surface capabilities.
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);

	// Face formats.
	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);

	if (formatCount != 0)
	{
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, details.formats.data());
	}

	// Presentation modes.
	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);

	if (presentModeCount != 0)
	{
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, details.presentModes.data());
	}

	return details;
}

VkSurfaceFormatKHR VulkanApp::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
	// Set the prefered format here.
	for (const auto& AVAILABLE_FORMAT : availableFormats)
	{
		if (AVAILABLE_FORMAT.format == VK_FORMAT_B8G8R8A8_SRGB && AVAILABLE_FORMAT.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			// If the prefered format is available, return here.
			return AVAILABLE_FORMAT;
		}
	}

	// Else, return the first available format.
	// Alternatively we could start ranking available formats to get one that is "close enough".
	return availableFormats[0];
}

VkPresentModeKHR VulkanApp::ChooseSwapPresentMode(const std::vector <VkPresentModeKHR>& availableModes)
{
	/*

		VK_PRESENT_MODE_IMMEDIATE_KHR:		Images submitted by your application are transferred to the screen right away, which may result in tearing.

		VK_PRESENT_MODE_FIFO_KHR:			The swap chain is a queue where the display takes an image from the front of the queue when the display is refreshed
											and the program inserts rendered images at the back of the queue. If the queue is full then the program has to wait.
											This is most similar to vertical sync as found in modern games. The moment that the display is refreshed is known as "vertical blank".

		VK_PRESENT_MODE_FIFO_RELAXED_KHR:	This mode only differs from the previous one if the application is late and the queue was empty at the last vertical blank.
											Instead of waiting for the next vertical blank, the image is transferred right away when it finally arrives. This may result in visible tearing.

		VK_PRESENT_MODE_MAILBOX_KHR:		This is another variation of the second mode. Instead of blocking the application when the queue is full, the images that are already queued are
											simply replaced with the newer ones. This mode can be used to render frames as fast as possible while still avoiding tearing, resulting in fewer
											latency issues than standard vertical sync. This is commonly known as "triple buffering", although the existence of three buffers alone does not
											necessarily mean that the framerate is unlocked.

																==>	ONLY VK_PRESENT_MODE_FIFO_KHR is guranteed to be avilable.	<==
	 */

	 // Check for the prefered present mode.
	for (const auto& MODE : availableModes)
	{
		if (MODE == VK_PRESENT_MODE_MAILBOX_KHR) // VK_PRESENT_MODE_MAILBOX_KHR is quite power hungy, consier VK_PRESENT_MODE_FIFO_KHR for mobile devices.
		{
			return MODE;
		}
	}

	// Default mode.
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanApp::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
	// UINT32_MAX is used to indicate that we are not simply matching the window extents.
	if (capabilities.currentExtent.width != UINT32_MAX)
	{
		return capabilities.currentExtent;
	}
	else
	{
		int width, height;
		glfwGetFramebufferSize(m_window, &width, &height);

		VkExtent2D actualExtent =
		{
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};

		// Fix values between min and max supported extents.
		actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return actualExtent;
	}
}

VkShaderModule VulkanApp::CreateShaderModule(const std::vector<char>& code)
{
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data()); // Must satisfy Uint32_t alignment, which the default allocator of std::vector does.

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(m_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create shader module!");
	}

	return shaderModule;
}

