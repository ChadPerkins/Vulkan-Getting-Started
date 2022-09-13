// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <vector>

class VulkanEngine {
public:

	bool _isInitialized{ false };
	int _frameNumber {0};

	VkExtent2D _windowExtent{ 1700 , 900 };

	struct SDL_Window* _window{ nullptr };

	VkInstance _instance;								// Vulkan library handle
	VkDebugUtilsMessengerEXT _debug_messenger;			// Vulkan debug output handle
	VkPhysicalDevice _chosenGPU;						// GPU chosen as the default device
	VkDevice _device;									// Vulkan device for commands
	VkSurfaceKHR _surface;								// Vulkan window surface

	VkSemaphore _presentSemaphore, _renderSemaphore;	// Semaphore sync structures
	VkFence _renderFence;								// Fence sync structure

	VkSwapchainKHR _swapchain;							// Vulkan swapchain
	VkFormat _swapchainImageFormat;						// Image format expected by the windowing system
	std::vector<VkImage> _swapchainImages;				// Array of images from the swapchain
	std::vector<VkImageView> _swapchainImageViews;		// Array of image-views from the swapchain

	VkQueue _graphicsQueue;								// Queue that will be submitted to
	uint32_t _graphicsQueueFamily;						// The family of the graphics queue
	
	VkCommandPool _commandPool;							// The command pool for the cammands to be sent to the GPU
	VkCommandBuffer _mainCommandBuffer;					// The buffer that will be recorded into

	VkRenderPass _renderPass;							// Vulkan renderpass
	std::vector<VkFramebuffer> _framebuffers;			// Array of framebuffers

	VkPipeline _trianglePipeline;						// The actual graphics pipeline
	VkPipelineLayout _trianglePipelineLayout;			// The layout of graphics pipeline

	// Load a shader module from a spir-v file. Returns fasle if any errors occur
	bool load_shader_module(const char* filepath, VkShaderModule* outShaderModule);

	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();

	//run main loop
	void run();

private:
	void init_vulkan();
	void init_swapchain();
	void init_commands();
	void init_default_renderpass();
	void init_framebuffers();
	void init_sync_structures();
	void init_pipelines();
};


