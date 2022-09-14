
#include "vk_engine.h"
#include "vk_pipeline.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_initializers.h>
#include <vk_types.h>

// Bootstrap library
#include "VkBootstrap.h"

#include <fstream>
#include <iostream>

// A macro function that will immediately abort when an error occurs
#define VK_CHECK(x)														\
	do																	\
	{																	\
		VkResult err = x;												\
		if (err)														\
		{																\
			std::cout <<"Detected Vulkan error: " << err << std::endl;	\
			abort();													\
		}																\
	} while (0)

void VulkanEngine::init()
{
	// We initialize SDL and create a window with it. 
	SDL_Init(SDL_INIT_VIDEO);

	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);
	
	_window = SDL_CreateWindow(
		"Vulkan Engine",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		_windowExtent.width,
		_windowExtent.height,
		window_flags
	);

	// Load the core Vulkan structures
	init_vulkan();

	// Create the swapchain
	init_swapchain();

	// Create the renderpass
	init_default_renderpass();

	// Create an array of framebuffers
	init_framebuffers();

	// Create the commands to be sent to the GPU
	init_commands();

	// Initialize the CPU and GPU sync structures
	init_sync_structures();
	
	// Initialize the object rendering pipelines
	init_pipelines();

	//everything went fine
	_isInitialized = true;
}

void VulkanEngine::cleanup()
{	
	if (_isInitialized)
	{
		// Make sure the gpu has stopped doing its things
		vkWaitForFences(_device, 1, &_renderFence, true, 1000000000);
		
		_mainDeletionQueue.flush();

		vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);

		vkDestroySurfaceKHR(_instance, _surface, nullptr);

		vkDestroyDevice(_device, nullptr);
		vkDestroyInstance(_instance, nullptr);

		SDL_DestroyWindow(_window);
	}
}

void VulkanEngine::draw()
{
	// Wait until the GPU has finished rendering the last frame. Timeout of 1 second
	// Fences must be reset in between each use
	VK_CHECK(vkWaitForFences(_device, 1, &_renderFence, true, 1000000000));
	VK_CHECK(vkResetFences(_device, 1, &_renderFence));
	
	// Reset the command buffer to empty it and queue new commands
	VK_CHECK(vkResetCommandBuffer(_mainCommandBuffer, 0));

	// Requet an image from the swapchain. Timeout of 1 second
	uint32_t swapchainImageIndex;
	VK_CHECK(vkAcquireNextImageKHR(_device, _swapchain, 1000000000, _presentSemaphore, nullptr, &swapchainImageIndex));

	// Shortening the name for convenience
	VkCommandBuffer cmd = _mainCommandBuffer;

	// Begin the command buffer recording and let Vulkan know we will use the command buffer only once
	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	// Make a clear color frame number. This will flash with a 120*pi frame period
	VkClearValue clearValue;
	float flash = abs(sin(_frameNumber / 120.0f));
	clearValue.color = { {0.0f, 0.0f, flash, 1.0f} };

	// Start the main renderpass
	// Use the clear value and the framebuffer of the index the swapchain returned
	VkRenderPassBeginInfo rpInfo = vkinit::renderpass_begin_info(_renderPass, _windowExtent, _framebuffers[swapchainImageIndex]);

	// Connect the clear values
	rpInfo.clearValueCount = 1;
	rpInfo.pClearValues = &clearValue;
	
	////////////// Begin the renderpass //////////////
	vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

	// Once rendering commands are added, they will go here

	if (_selectedShader == 0)
	{
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _trianglePipeline);
	}
	else
	{
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _redTrianglePipeline);
	}

	vkCmdDraw(cmd, 3, 1, 0, 0);

	// Finalize the render pass
	vkCmdEndRenderPass(cmd);

	// Finalize the command buffer (it can still be executed but no commands can be added)
	VK_CHECK(vkEndCommandBuffer(cmd));

	// Prepare the submition to the queue
	VkSubmitInfo submit = vkinit::submit_info(&cmd);

	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	submit.pWaitDstStageMask = &waitStage;

	// Wait on the _presentSemaphore, as it signals when the swapchain is ready
	submit.waitSemaphoreCount = 1;
	submit.pWaitSemaphores = &_presentSemaphore;

	// Signal the _renderSemaphore to signal that rendering is completed
	submit.signalSemaphoreCount = 1;
	submit.pSignalSemaphores = &_renderSemaphore;

	// Submit the command buffer to the queue and execute it
	// _renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit(_graphicsQueue, 1, &submit, _renderFence));

	// Put the rendered image into the visible window
	VkPresentInfoKHR presentInfo = vkinit::present_info();

	presentInfo.pSwapchains = &_swapchain;
	presentInfo.swapchainCount = 1;

	// Wait on the _renderSemaphore because it is necessary that drawing commands
	// have finished before the image is displayed to the user
	presentInfo.pWaitSemaphores = &_renderSemaphore;
	presentInfo.waitSemaphoreCount = 1;

	presentInfo.pImageIndices = &swapchainImageIndex;

	VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));

	// Increase the number of frames drawm
	_frameNumber++;


}

void VulkanEngine::run()
{
	SDL_Event e;
	bool bQuit = false;

	//main loop
	while (!bQuit)
	{
		//Handle events on queue
		while (SDL_PollEvent(&e) != 0)
		{
			//close the window when user alt-f4s or clicks the X button			
			if (e.type == SDL_QUIT)
			{
				bQuit = true;
			}
			else if (e.type == SDL_KEYDOWN)
			{
				// Swap between shaders by hitting space
				if (e.key.keysym.sym == SDLK_SPACE)
				{
					_selectedShader += 1;
					
					// Since there are only 2 sets of shaders at the moment, limit the numbers to 1 and 0
					if(_selectedShader > 1)
					{
						_selectedShader = 0;
					}
				}
			}
		}

		draw();
	}
}


void VulkanEngine::init_vulkan()
{
	vkb::InstanceBuilder builder;

	// Make the Vulkan instance with basic debug features
	auto inst_ret = builder.set_app_name("Example Vulkan Application")
		.request_validation_layers(true)
		.require_api_version(1, 1, 0)
		.use_default_debug_messenger()
		.build();

	vkb::Instance vkb_inst = inst_ret.value();

	// Store the instance
	_instance = vkb_inst.instance;
	// Store the debug messenger
	_debug_messenger = vkb_inst.debug_messenger;

	// Get the surface of the window that was opened with SDL
	SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

	// Use VKBootstrap to select a GPU
	// We want a GPU that can write to the SDL surface and supports Vulkan 1.1
	vkb::PhysicalDeviceSelector selector{ vkb_inst };
	vkb::PhysicalDevice physicalDevice = selector.set_minimum_version(1, 1)
		.set_surface(_surface)
		.select()
		.value();

	// Create the final Vulkan device
	vkb::DeviceBuilder deviceBiulder{ physicalDevice };
	vkb::Device vkbDevice = deviceBiulder.build().value();

	// Get the VkDevice handle used in the rest of a Vulkan application
	_device = vkbDevice.device;
	_chosenGPU = physicalDevice.physical_device;

	// Use VkBootstrap to get a graphics queue
	_graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
	_graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
}

void VulkanEngine::init_swapchain()
{
	vkb::SwapchainBuilder swapchainBuilder{ _chosenGPU, _device, _surface };

	vkb::Swapchain vkbSwapchain = swapchainBuilder.use_default_format_selection()
		// Use Vsynq present mode
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_extent(_windowExtent.width, _windowExtent.height)
		.build()
		.value();

	// Store the swapchain and its related images
	_swapchain = vkbSwapchain.swapchain;
	_swapchainImages = vkbSwapchain.get_images().value();
	_swapchainImageViews = vkbSwapchain.get_image_views().value();

	_swapchainImageFormat = vkbSwapchain.image_format;

	_mainDeletionQueue.push_function([=]() {
		vkDestroySwapchainKHR(_device, _swapchain, nullptr);
	});
}

void VulkanEngine::init_default_renderpass()
{
	/////////// The main attachment ///////////
	// The renderpass will use this color attachment (the description of the image to be rendered)
	VkAttachmentDescription color_attachment = {};
	// Set the format of the color attachment to that needed by the swapchain
	color_attachment.format = _swapchainImageFormat;
	// 1 sample (no multisampling)
	color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	// Clear when the attachment is loaded
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	// Keep the attachment stored when the renderpass ends
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	// Do not care about stencil
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	// The starting layout is unknown and something we dont care about
	color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	// After the renderpass ends, set the layout of the image to be ready for displaying with the swapchain
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	/////////// The subpass ///////////
	// Create a subpass that uses the attachment
	VkAttachmentReference color_attachment_ref = {};
	// The attachment number will be the index number withing the parent renderpass's pAttachments array
	color_attachment_ref.attachment = 0;
	color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// Create 1 subpass, the minimum you can create
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;

	/////////// The renderpass ///////////
	// Create a renderpass to be loaded with the attachment and subpass
	VkRenderPassCreateInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

	// Connect the color attachment to the info
	render_pass_info.attachmentCount = 1;
	render_pass_info.pAttachments = &color_attachment;
	// Connect the subpass to the info
	render_pass_info.subpassCount = 1;
	render_pass_info.pSubpasses = &subpass;

	VK_CHECK(vkCreateRenderPass(_device, &render_pass_info, nullptr, &_renderPass));

	_mainDeletionQueue.push_function([=]() {
		vkDestroyRenderPass(_device, _renderPass, nullptr);
	});
}

void VulkanEngine::init_framebuffers()
{
	// Create the framebuffers for the swapchain images. This will connect the penderpass to the images for rendering
	VkFramebufferCreateInfo fb_info = vkinit::framebuffer_create_info(_renderPass, _windowExtent);

	// Count how many images are stored in the swapchain
	const uint32_t swapchain_imagecount = _swapchainImages.size();
	_framebuffers = std::vector<VkFramebuffer>(swapchain_imagecount);

	// Create framebuffers for each of the swapchain image views
	for (int i = 0; i < swapchain_imagecount; i++)
	{
		fb_info.pAttachments = &_swapchainImageViews[i];
		VK_CHECK(vkCreateFramebuffer(_device, &fb_info, nullptr, &_framebuffers[i]));

		_mainDeletionQueue.push_function([=]() {
			vkDestroyFramebuffer(_device, _framebuffers[i], nullptr);
			vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
		});
	}
}


void VulkanEngine::init_commands()
{
	// Create a command pool for commands submitted to the graphics queue and allow the pool to reset individual command buffers
	VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_commandPool));

	// Allocate the default command buffer that will be used for rendering
	VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_commandPool, 1);

	VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_mainCommandBuffer));

	_mainDeletionQueue.push_function([=]() {
		vkDestroyCommandPool(_device, _commandPool, nullptr);
	});
}

void VulkanEngine::init_sync_structures()
{
	// Create the synchronization structures
	// One fence to control when the GPU has finished rendering the frame,
	// 2 semaphores to syncronize rendering with swapchain
	// Have the fence to start signalled so the user
	//  can wait on it on the first frame
	////////// Creating the fence //////////
	VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);

	VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_renderFence));

	// Enqueue the destruction of the fence
	_mainDeletionQueue.push_function([=]() {
		vkDestroyFence(_device, _renderFence, nullptr);
	});

	////////// Creating the semaphores //////////
	VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

	VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_presentSemaphore));
	VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_renderSemaphore));

	// Enqueue the destruction of semaphores
	_mainDeletionQueue.push_function([=]() {
		vkDestroySemaphore(_device, _presentSemaphore, nullptr);
		vkDestroySemaphore(_device, _renderSemaphore, nullptr);
	});
}

void VulkanEngine::init_pipelines()
{
	// Compile colored triangle modules
	VkShaderModule triangleFragShader;
	if (!load_shader_module("../../shaders/colored_triangle.frag.spv", &triangleFragShader))
	{
		std::cout << "Error when building the triangle fragment shader module" << std::endl;
	}
	else {
		std::cout << "Triangle fragment shader successfully loaded" << std::endl;
	}

	VkShaderModule triangleVertexShader;
	if (!load_shader_module("../../shaders/colored_triangle.vert.spv", &triangleVertexShader))
	{
		std::cout << "Error when building the triangle vertex shader module" << std::endl;
	}
	else {
		std::cout << "Triangle vertex shader successfully loaded" << std::endl;
	}

	// Compile red triangle modules
	VkShaderModule redTriangleFragShader;
	if (!load_shader_module("../../shaders/triangle.frag.spv", &redTriangleFragShader))
	{
		std::cout << "Error when building the triangle fragment shader module" << std::endl;
	}
	else {
		std::cout << "Red Triangle fragment shader successfully loaded" << std::endl;
	}

	VkShaderModule redTriangleVertShader;
	if (!load_shader_module("../../shaders/triangle.vert.spv", &redTriangleVertShader))
	{
		std::cout << "Error when building the triangle vertex shader module" << std::endl;
	}
	else {
		std::cout << "Red Triangle vertex shader successfully loaded" << std::endl;
	}

	// Build the pipeline layout that controls the inputs/outputs of the shader
	VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();

	VK_CHECK(vkCreatePipelineLayout(_device, &pipeline_layout_info, nullptr, &_trianglePipelineLayout));

	// Build the stage-create-info for both the vertex and fragment stages
	PipelineBuilder pipelineBuilder;

	pipelineBuilder._shaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, triangleVertexShader));

	pipelineBuilder._shaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, triangleFragShader));

	// Vertex input controls how to read vertices from vertex buffers.
	pipelineBuilder._vertexInputInfo = vkinit::vertex_input_state_create_info();

	// Input assembly is the configuration for drawing triangle lists, strips, or individual points.
	pipelineBuilder._inputAssembly = vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

	// Build the viewport and scissor from the swapchain extents
	pipelineBuilder._viewport.x = 0.0f;
	pipelineBuilder._viewport.y = 0.0f;
	pipelineBuilder._viewport.width = (float)_windowExtent.width;
	pipelineBuilder._viewport.height = (float)_windowExtent.height;
	pipelineBuilder._viewport.minDepth = 0.0f;
	pipelineBuilder._viewport.maxDepth = 1.0f;

	pipelineBuilder._scissor.offset = { 0, 0 };
	pipelineBuilder._scissor.extent = _windowExtent;

	// Configure the rasterizer to draw filled triangles
	pipelineBuilder._rasterizer = vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);

	// Multisampling isnt being used so just run the default one
	pipelineBuilder._multisampling = vkinit::multisampling_state_create_info();

	// A single blend attachment with no blending and writing to RGBA
	pipelineBuilder._colorBlendAttachment = vkinit::color_blend_attachment_state();

	// Use the triangle layout that was just created
	pipelineBuilder._pipelineLayout = _trianglePipelineLayout;

	// Finally build the pipeline
	_trianglePipeline = pipelineBuilder.build_pipeline(_device, _renderPass);

	// Clear the shader stages for the builder
	pipelineBuilder._shaderStages.clear();

	// Add the other shaders
	pipelineBuilder._shaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, redTriangleVertShader));

	pipelineBuilder._shaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, redTriangleFragShader));

	// Build the red triangle pipeline
	_redTrianglePipeline = pipelineBuilder.build_pipeline(_device, _renderPass);

	// Destroy all shader modules, outside of the queue
	vkDestroyShaderModule(_device, redTriangleVertShader, nullptr);
	vkDestroyShaderModule(_device, redTriangleFragShader, nullptr);
	vkDestroyShaderModule(_device, triangleFragShader, nullptr);
	vkDestroyShaderModule(_device, triangleVertexShader, nullptr);

	_mainDeletionQueue.push_function([=]() {
		// Destroy the 2 pipelines we have created
		vkDestroyPipeline(_device, _redTrianglePipeline, nullptr);
		vkDestroyPipeline(_device, _trianglePipeline, nullptr);

		// Destroy the pipeline layout that they use
		vkDestroyPipelineLayout(_device, _trianglePipelineLayout, nullptr);
	});
}

bool VulkanEngine::load_shader_module(const char* filepath, VkShaderModule* outShaderModule)
{
	// Open the file with the cursor at the end of the file
	std::ifstream file(filepath, std::ios::ate | std::ios::binary);

	if (!file.is_open())
{
		return false;
	}

	// Use the cursor to figure out the size of the file (since the cursor is at the end, it will give us the file size directly in bytes)
	size_t fileSize = (size_t)file.tellg();

	// Reserve an int vector big enough for the entire file since Spirv expects the buffer to be on uint32
	std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

	// Put the cursor at the begining of the file
	file.seekg(0);

	// Load the whole file into the buffer
	file.read((char*)buffer.data(), fileSize);

	// Close the file since it is no longer needed
	file.close();

	// Create a new shader module using the buffer

	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.pNext = nullptr;

	// codeSize has to be in bytes, so multiply the ints in the buffer by size of int to know the real size of the buffer
	createInfo.codeSize = buffer.size() * sizeof(uint32_t);
	createInfo.pCode = buffer.data();

	// Make sure the creation succeeded
	VkShaderModule shaderModule;
	if (vkCreateShaderModule(_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		return false;
	}
	*outShaderModule = shaderModule;
	return true;
}
