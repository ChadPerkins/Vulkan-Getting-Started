// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include "vk_mesh.h"

#include <glm/glm.hpp>

#include <functional>
#include <queue>
#include <vector>

//note that we store the VkPipeline and layout by value, not pointer.
//They are 64 bit handles to internal driver structures anyway so storing pointers to them isn't very useful
struct Material {
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
};

struct RenderObject {
	Mesh* mesh;

	Material* material;

	glm::mat4 transformMatrix;
};

struct MeshPushConstants
{
	glm::vec4 data;
	glm::mat4 render_matrix;
};

struct DeletionQueue
{
	std::deque<std::function<void()>> deletors;

	void push_function(std::function<void()>&& function)
	{
		deletors.push_back(function);
	}

	void flush()
	{
		// Reverse iterate the deletion queue to execute all of the functions
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++)
		{
			// Call the function
			(*it)();
		}

		deletors.clear();
	}
};

class VulkanEngine {
public:

	int _frameNumber {0};
	bool _isInitialized{ false };

	int _selectedShader{ 0 };

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

	VkPipelineLayout _trianglePipelineLayout;			// The layout of graphics pipeline

	VkPipeline _trianglePipeline;						// The actual graphics pipeline
	VkPipeline _redTrianglePipeline;					// The graphics pipeline for a second shader

	DeletionQueue _mainDeletionQueue;					// A deletion queue to make sure object get deleted only when they are done beign used

	VmaAllocator _allocator;							// A memory allocator to allocate memory for buffers (index/vertex)

	VkPipeline _meshPipeline;							// A pipeline that doesnt hardcode a triangle
	VkPipelineLayout _meshPipelineLayout;				// The layout for the mesh pipeline

	VkImageView _depthImageView;
	AllocatedImage _depthImage;

	VkFormat _depthFormat;

	Mesh _triangleMesh;
	Mesh _monkeyMesh;

	// Default array of renderable objects
	std::vector<RenderObject> _renderables;

	std::unordered_map<std::string, Material> _materials;
	std::unordered_map<std::string, Mesh> _meshes;

	// Create material and add it to the map
	Material* create_material(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name);

	// Returns nullptr if it can't be found
	Material* get_material(const std::string& name);

	// Returns nullptr if it can't be found
	Mesh* get_mesh(const std::string& name);

	// Draw function
	void draw_objects(VkCommandBuffer cmd, RenderObject* first, int count);

	// Load a shader module from a spir-v file. Returns fasle if any errors occur
	bool load_shader_module(const char* filepath, VkShaderModule* outShaderModule);

	void load_meshes();

	void upload_meshes(Mesh& mesh);

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
	void init_scene();
};


