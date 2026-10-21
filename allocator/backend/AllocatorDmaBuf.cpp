/*
 * SPDX-FileCopyrightText: Copyright 2026 Valentin Liu (LIU, YUANCHEN) <valentinliu@icloud.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <drm/android/gralloc_handle.h>
#include <errno.h>
#include <hardware/gralloc.h>
#include <string.h>
#include <system/graphics.h>
#include <BufferAllocator/BufferAllocator.h>

#include <gralloc_generic_format.h>

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
	LOG_D("DMA-BUF allocator initialized (%zu heap(s) available).",
	      allocator->GetDmabufHeapList().size());

	return !!(allocator == nullptr);
}

ndk::ScopedAStatus AllocatorBackendDmaBuf::allocate2(const BufferDescriptorInfo& descriptor, int32_t count,
							 AllocationResult* outResult)
{
	LOG_TRACE();
	LOG_V("count=%d, desc: %s.", count, descriptor.toString().c_str());

	LOG_D("new buffer allocating request: %s", descriptor.toString().c_str());
	size_t buffer_size = descriptor.width * descriptor.height;
	std::vector<native_handle_t *> handles;
	handles.resize(count, nullptr);
    
	uint32_t drm_format = color_fmt_a2g(static_cast<uint32_t>(descriptor.format));
	if (!drm_format)
		LOG_W("Unsupported or unknown Android HAL format %u!",
		      static_cast<uint32_t>(descriptor.format));

	uint32_t pitch_bytes = descriptor.width * color_bytes_per_pixel(drm_format);

	for (int i = 0; i < count; i++) {
		native_handle_t *handle = gralloc_handle_create(descriptor.width, descriptor.height,
								static_cast<int32_t>(descriptor.format),
								static_cast<int32_t>(descriptor.usage));
		if (!handle) {
			LOG_E("gralloc_handle_create() failed (index=%d).", i);
			return ToBinderStatus(AllocationError::NO_RESOURCES);
		}
		int fd = allocator->AllocSystem(true, buffer_size);
		if (fd <= 0) {
			LOG_E("Allocation from the system heap failed (ret=%d, size=%zu): %s",
			      fd, buffer_size, strerror(errno));
			native_handle_delete(handle);
			return ToBinderStatus(AllocationError::NO_RESOURCES);
		}
		gralloc_handle(handle)->prime_fd = fd;
		gralloc_handle(handle)->stride = pitch_bytes;
		handles[i] = handle;
		LOG_V("allocated handle #%d/%d at %p (fd=%d).", i, count, handle, fd);
	}

	LOG_D("allocated %d buffer(s), stride=%u bytes.", count, pitch_bytes);

	outResult->buffers.resize(count);
	for (int32_t i = 0; i < count; i++) {
		if (!handles[i])
			continue;
		auto handle = handles[i];
		outResult->buffers[i] = ::android::dupToAidl(handle);
		LOG_D("Copy handle from %p to %p (index: %d).",
		      handle, (void *) &outResult->buffers[i], i);
		// Release buffer and handle
		native_handle_close(handle);
		native_handle_delete(handle);
	}
	outResult->stride = pitch_bytes / color_bytes_per_pixel(drm_format);

	return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus AllocatorBackendDmaBuf::isSupported(const BufferDescriptorInfo& descriptor, bool* outResult)
{
	LOG_TRACE();
	LOG_V("Descriptor: %s.", descriptor.toString().c_str());

	if (descriptor.format == PixelFormat::BLOB && descriptor.height == 1) {
		*outResult = true;
		LOG_V("Supported (1D BLOB).");
		return ndk::ScopedAStatus::ok();
	}

	uint32_t usage = static_cast<uint32_t>(descriptor.usage);
	if (usage & (GRALLOC_USAGE_HW_VIDEO_ENCODER | GRALLOC_USAGE_HW_IMAGE_ENCODER)) {
		*outResult = true;
		LOG_V("Supported (hardware encoder usage).");
		return ndk::ScopedAStatus::ok();
	}

	if (usage & (GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN)) {
		*outResult = true;
		LOG_V("Supported (software access usage).");
		return ndk::ScopedAStatus::ok();
	}

	*outResult = false;
	LOG_V("Not supported.");
	return ndk::ScopedAStatus::ok();
}