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
	return is_gralloc_gbm_ready();
}

int AllocatorBackendGrallocGbm::init()
{
	return gralloc_gbm_init();
}

ndk::ScopedAStatus AllocatorBackendGrallocGbm::grallocGbmAllocate(allocator_desc_t& desc, int32_t count, 
								  AllocationResult* outResult)
{
	LOG_TRACE();

	if (!gralloc_gbm_is_allocator_desc_supported(&desc)) {
		LOG_E("grallocGbmAllocate failed: Unsupported allocator desc: %s", allocator_desc_to_string(&desc));
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
			LOG_E("grallocGbmAllocate failed: GBM operation failed, ret=%d", ret);
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
	}

	outResult->buffers.resize(count);
	for (int32_t i = 0; i < count; i++) {
		if (!handles[i])
			continue;
		auto handle = handles[i];
		outResult->buffers[i] = ::android::dupToAidl(handle);
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
	allocator_desc_t grallocGbmDesc = {};
	int status = generateGrallocGenericDesc(descriptor, &grallocGbmDesc);
	if (status != 0) {
		LOG_E("allocate2 failed: Failed to convert the request buffer desc to Gralloc GBM desc.\n");
		return ToBinderStatus(AllocationError::UNSUPPORTED);
	}

	return grallocGbmAllocate(grallocGbmDesc, count, outResult);
}

ndk::ScopedAStatus AllocatorBackendGrallocGbm::isSupported(const BufferDescriptorInfo& descriptor, bool* outResult)
{
	if (descriptor.format == PixelFormat::BLOB) {
		*outResult = false;
		return ndk::ScopedAStatus::ok();
	}

	/* Reject all software RW buffers */
	if (static_cast<uint32_t>(descriptor.usage) & (GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN)) {
		*outResult = false;
		return ndk::ScopedAStatus::ok();
	}	

	allocator_desc_t grallocGbmDesc = {};
	int status = generateGrallocGenericDesc(descriptor, &grallocGbmDesc);
	if (status != 0) {
		LOG_E("isSupported failed: Failed to convert the request buffer desc to Gralloc GBM desc.\n");
		return ToBinderStatus(AllocationError::UNSUPPORTED);
	}
	
	*outResult = gralloc_gbm_is_allocator_desc_supported(&grallocGbmDesc);

	return ndk::ScopedAStatus::ok();
}