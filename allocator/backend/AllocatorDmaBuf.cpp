/*
 * SPDX-FileCopyrightText: Copyright 2026 Valentin Liu (LIU, YUANCHEN) <valentinliu@icloud.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <drm/android/gralloc_handle.h>
#include <hardware/gralloc.h>
#include <system/graphics.h>
#include <BufferAllocator/BufferAllocator.h>

#define LOG_TAG "AllocatorDmaBuf"
#include <private/log.h>

#include "AllocatorDmaBuf.hpp"

static class BufferAllocator *allocator = nullptr;

bool AllocatorBackendDmaBuf::isReady() const
{
	LOG_TRACE();

	if (allocator == nullptr) {
		LOG_D("No available DMA allocator, is it initialized?");
		return false;
	}

	if (allocator->GetDmabufHeapList().empty()) {
		LOG_D("No available DMA heap.");
		return false;
	}

	return true;
}

int AllocatorBackendDmaBuf::init()
{
	LOG_TRACE();
	allocator = new BufferAllocator();

	return !!(allocator == nullptr);
}

ndk::ScopedAStatus AllocatorBackendDmaBuf::allocate2(const BufferDescriptorInfo& descriptor, int32_t count,
							 AllocationResult* outResult)
{
	LOG_TRACE();

	LOG_D("new buffer allocating request: %s", descriptor.toString().c_str());
	size_t buffer_size = descriptor.width * descriptor.height;
	std::vector<native_handle_t *> handles;
	handles.resize(count, nullptr);
    
	for (int i = 0; i < count; i++) {
		native_handle_t *handle = gralloc_handle_create(descriptor.width, descriptor.height,
								static_cast<int32_t>(descriptor.format),
								static_cast<int32_t>(descriptor.usage));
		int fd = allocator->AllocSystem(true, buffer_size);
		if (fd <= 0) {
			LOG_E("allocate2 failed: allocate from system heap failed.");
			return ToBinderStatus(AllocationError::NO_RESOURCES);
		}
		gralloc_handle(handle)->prime_fd = fd;
		gralloc_handle(handle)->stride = descriptor.width / (1 * 8); // 1 Byte (8 bits) per pixel
		handles[i] = handle;
	}

	for (int i = 0; i < count; i++) {
		auto handle = handles[i];
		LOG_D("new buffer (fd=%u) with count %d in request %p", gralloc_handle(handle)->prime_fd, i, &descriptor);
	}

	outResult->buffers.resize(count);
	for (int32_t i = 0; i < count; i++) {
		if (!handles[i])
			continue;
		auto handle = handles[i];
		outResult->buffers[i] = ::android::dupToAidl(handle);
		// Release buffer and handle
		native_handle_close(handle);
		native_handle_delete(handle);
	}
	outResult->stride = descriptor.width / 8;

	return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus AllocatorBackendDmaBuf::isSupported(const BufferDescriptorInfo& descriptor, bool* outResult)
{
	LOG_TRACE();

	if (descriptor.format == PixelFormat::BLOB && descriptor.height == 1) {
		*outResult = true;
		return ndk::ScopedAStatus::ok();
	}

	uint32_t usage = static_cast<uint32_t>(descriptor.usage);
	if (usage & (GRALLOC_USAGE_HW_VIDEO_ENCODER | GRALLOC_USAGE_HW_IMAGE_ENCODER)) {
		*outResult = true;
		return ndk::ScopedAStatus::ok();
	}

	if (usage & (GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN)) {
		*outResult = true;
		return ndk::ScopedAStatus::ok();
	}

	*outResult = false;
	return ndk::ScopedAStatus::ok();
}