#pragma once

#include "volk.h"
#include "core/container/array.h"

struct Window;

const int MAX_FRAMES_IN_FLIGHT = 2;
const int MAX_SWAP_CHAIN_IMAGES = MAX_FRAMES_IN_FLIGHT + 1;

struct Swapchain {
	VkSwapchainKHR swapchain;
	array<10, VkImage> images;
	VkFormat imageFormat;
	VkExtent2D extent;
	array<MAX_SWAP_CHAIN_IMAGES, VkImageView> image_views;
	array<MAX_SWAP_CHAIN_IMAGES, VkFramebuffer> framebuffers;

	//Synchronization
	size_t current_frame = 0;
	uint32_t image_index;
	VkSemaphore image_available_semaphore[MAX_FRAMES_IN_FLIGHT];
	VkSemaphore render_finished_semaphore[MAX_FRAMES_IN_FLIGHT];
	VkFence in_flight_fences[MAX_FRAMES_IN_FLIGHT];
	array<10, VkFence> images_in_flight;

	VkSurfaceKHR surface;

	operator VkSwapchainKHR() {
		return swapchain;
	}
};

Swapchain make_SwapChain(VkDevice device, VkPhysicalDevice physical_device, Window& window, VkSurfaceKHR surface);
VkSurfaceKHR make_Surface(VkInstance instance, Window& window);
void destroy_Surface(VkInstance instance, VkSurfaceKHR surface);
void destroy_sync_objects(VkDevice device, Swapchain& swapchain);