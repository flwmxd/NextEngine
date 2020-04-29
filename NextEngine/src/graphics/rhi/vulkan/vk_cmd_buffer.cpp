#include "stdafx.h"
#include <assert.h>
#include <stdio.h>
#include "core/container/array.h"
#include "graphics/rhi/vulkan/vulkan.h"
#include "graphics/rhi/vulkan/device.h"
#include "graphics/rhi/vulkan/command_buffer.h"

void make_vk_CommandPool(VkCommandPool& pool, Device& device, QueueType queue_type) {
	int32_t queue_family_indices = device.queue_families[queue_type];
	assert(queue_family_indices != -1);

	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = queue_family_indices;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &pool))
}

void alloc_CommandBuffers(VkDevice device, VkCommandPool pool, uint count, VkCommandBuffer* result) {
	VkCommandBufferAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.commandPool = pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = count;

	VK_CHECK(vkAllocateCommandBuffers(device, &alloc_info, result));
}

void make_CommandPool(CommandPool& pool, Device& device, QueueType queue_type, uint count) {
	make_vk_CommandPool(pool.pool, device, queue_type);

	pool.free.resize(count * MAX_FRAMES_IN_FLIGHT);
	alloc_CommandBuffers(device, pool.pool, pool.free.length, pool.free.data);
}

void destroy_CommandPool(CommandPool& pool) {
	vkDestroyCommandPool(pool.device, pool.pool, nullptr);
}

void completed_frame(CommandPool& pool, uint frame_index) {
	for (VkCommandBuffer cmd_buffer : pool.submited[frame_index]) {
		pool.free.append(cmd_buffer);
	}

	pool.submited[frame_index].clear();
}

void begin_frame(CommandPool& pool, uint frame_index) {
	completed_frame(pool, frame_index);
	pool.frame_index = frame_index;
}

VkCommandBuffer begin_recording(CommandPool& pool) {
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0; 

	VkCommandBuffer cmd_buffer = pool.free.pop();

	vkBeginCommandBuffer(cmd_buffer, &beginInfo);
	return cmd_buffer;
}

void end_recording(CommandPool& pool, VkCommandBuffer cmd_buffer) {
	VkDevice device = pool.device;
	vkEndCommandBuffer(cmd_buffer);

	pool.submited[pool.frame_index].append(cmd_buffer);
}

void submit_cmd_buffers(CommandPool& pool, QueueSubmitInfo& info) {
	for (VkCommandBuffer cmd_buffer : pool.submited[pool.frame_index]) {
		info.cmd_buffers.append(cmd_buffer);
	}
}

void end_frame(CommandPool& pool) {
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = pool.submited[pool.frame_index].length;
	submitInfo.pCommandBuffers = pool.submited[pool.frame_index].data;

	VK_CHECK(vkQueueSubmit(pool.queue, 1, &submitInfo, VK_NULL_HANDLE));
}