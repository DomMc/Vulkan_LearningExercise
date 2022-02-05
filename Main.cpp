//==============================================================================================================//
//	Project:		Vulkan Renderer
//	Description:	A basic Vulkan renderer, created while learning the Vulkan API.
//	Author:			Dom McCollum
//==============================================================================================================//

#include <iostream>		// Capture error reporting
#include <cstdlib>		// Exit Macros

#include "VulkanApp.h"

int main()
{
	VulkanApp app;

	try
	{
		app.Run();
	}
	catch(const std::exception& E)
	{
		std::cerr << E.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

