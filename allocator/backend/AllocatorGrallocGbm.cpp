/*
 * SPDX-FileCopyrightText: Copyright 2026 Valentin Liu (LIU, YUANCHEN) <valentinliu@icloud.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gralloc_gbm.h>

#define LOG_TAG "AllocatorGrallocGbm"
#include <private/log.h>

#include "AllocatorGrallocGbm.hpp"

bool AllocatorBackendGrallocGbm::isReady() const
{
	bool ready = is_gralloc_gbm_ready();
	LOG_V("Backend ready: %d.", ready);
	return ready;
}

int AllocatorBackendGrallocGbm::init()
{
	int ret = gralloc_gbm_init();
	LOG_D("gralloc_gbm_init returned %d.", ret);
	return ret;
}

ndk::ScopedAStatus AllocatorBackendGrallocGbm::grallocGbmAllocate(allocator_desc_t& desc, int32_t count, 
								  AllocationResult* outResult)
{
	LOG_TRACE();
	LOG_V("count=%d, desc: %s.", count, allocator_desc_to_string(&desc));

	if (!gralloc_gbm_is_allocator_desc_supported(&desc)) {
		LOG_E("Unsupported allocator desc: %s", allocator_desc_to_string(&desc));
		return ToBinderStatus(AllocationError::UNSUPPORTED);
	}

	std::vector<native_handle_t *> handles;
	handles.resize(count, nullptr);
    
	int32_t pixel_stride = 0;
	for (int32_t i = 0; i < count; i++) {
		native_handle_t *handle;
		uint32_t gbm_stride = 0;
		int ret = gralloc_gbm_android_buffer_new(&desc, &gbm_stride, &handle);
		if (ret || !handle) {
			LOG_E("gralloc_gbm_android_buffer_new() returned %d, handle=%p.",
			      ret, handle);
			for (int32_t j = 0; j < i; j++) {
				// Release all buffer and handle
				if (!handles[j])
					continue;
				gralloc_gbm_android_buffer_free(handles[j]);
				native_handle_close(handles[j]);
				native_handle_delete(handles[j]);
			}
			return ToBinderStatus(AllocationError::UNSUPPORTED);
		}
		handles[i] = handle;
		pixel_stride = gralloc_gbm_calculate_android_pixel_stride(desc.format, gbm_stride);
		LOG_V("allocated handle #%d/%d at %p (gbm_stride=%u).",
		      i, count, handle, gbm_stride);
	}

	LOG_D("Allocated %d buffer(s), pixel stride=%d.", count, pixel_stride);

	outResult->buffers.resize(count);
	for (int32_t i = 0; i < count; i++) {
		if (!handles[i])
			continue;
		auto handle = handles[i];
		outResult->buffers[i] = ::android::dupToAidl(handle);
		LOG_D("Copy handle from %p to %p (index: %d).",
		      handle, (void *) &outResult->buffers[i], i);
		// Release buffer and handle
		gralloc_gbm_android_buffer_free(handle);
		native_handle_close(handle);
		native_handle_delete(handle);
	}
	outResult->stride = pixel_stride;

	return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus AllocatorBackendGrallocGbm::allocate2(const BufferDescriptorInfo& descriptor, int32_t count,
							 AllocationResult* outResult)
{
	LOG_V("count=%d, desc: %s.", count, descriptor.toString().c_str());

	allocator_desc_t grallocGbmDesc = {};
	int status = generateGrallocGenericDesc(descriptor, &grallocGbmDesc);
	if (status != 0) {
		LOG_E("Failed to convert the request buffer desc to Gralloc GBM desc.");
		return ToBinderStatus(AllocationError::UNSUPPORTED);
	}

	return grallocGbmAllocate(grallocGbmDesc, count, outResult);
}

ndk::ScopedAStatus AllocatorBackendGrallocGbm::isSupported(const BufferDescriptorInfo& descriptor, bool* outResult)
{
	LOG_V("Descriptor: %s.", descriptor.toString().c_str());

	if (descriptor.format == PixelFormat::BLOB) {
		LOG_V("BLOB format is not supported by the Gralloc GBM backend.");
		*outResult = false;
		return ndk::ScopedAStatus::ok();
	}

	// Do NOT reject all software RW buffers here.
	// A dumb will be created for some supported formats.

	allocator_desc_t grallocGbmDesc = {};
	int status = generateGrallocGenericDesc(descriptor, &grallocGbmDesc);
	if (status != 0) {
		LOG_E("Failed to convert the request buffer desc to Gralloc GBM desc.");
		return ToBinderStatus(AllocationError::UNSUPPORTED);
	}

	*outResult = gralloc_gbm_is_allocator_desc_supported(&grallocGbmDesc);
	LOG_V("result=%d.", *outResult);

	return ndk::ScopedAStatus::ok();
}