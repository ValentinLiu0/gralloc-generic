/*
 * SPDX-FileCopyrightText: Copyright 2026 Valentin Liu (LIU, YUANCHEN) <valentinliu@icloud.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <aidl/android/hardware/graphics/common/StandardMetadataType.h>
#include <drm/android/gralloc_handle.h>
#include <errno.h>
#include <gralloc_generic.h>
#include <hardware/gralloc.h>
#include <string.h>
#include <sys/mman.h>
#include <BufferAllocator/BufferAllocator.h>

#include <gralloc_generic_format.h>

#define LOG_TAG "MapperDmaBuf"
#include <private/log.h>

#include "MapperDmaBuf.hpp"

using namespace ::android::hardware::graphics::mapper;

typedef struct dma_buffer_info {
	int prime_fd;
	void *buffer_id;
	int32_t width;
	int32_t height;
	int32_t format;
	int32_t android_format;
	int64_t usage;
	uint32_t stride;
	uint32_t android_stride;
	size_t size;
	void *mapped;

	bool is_protected;
} dma_buffer_info_t, buffer_info_t;

static class BufferAllocator *allocator = nullptr;
std::unordered_map<int, buffer_info_t *> importedBufferTable;

bool MapperBackendDmaBuf::isReady() const
{
	LOG_TRACE();

	if (allocator == nullptr) {
		LOG_D("No available DMA-BUF allocator, is it initialized?");
		return false;
	}

	if (allocator->GetDmabufHeapList().empty()) {
		LOG_D("No available DMA-BUF heaps.");
		return false;
	}

	return true;
}

int MapperBackendDmaBuf::init()
{
	LOG_TRACE();
	allocator = new BufferAllocator();
	LOG_D("DMA-BUF allocator initialized (%zu heap(s) available).",
	      allocator->GetDmabufHeapList().size());

	return !!(allocator == nullptr);
}

inline buffer_info_t *buffer_info_create(const gralloc_handle_t *handle)
{
	if (!handle) {
		LOG_E("invalid handle.");
		return nullptr;
	}

	buffer_info_t *bi = (buffer_info_t *) calloc(1, sizeof(buffer_info_t));
	if (!bi) {
		LOG_E("no memory.");
		return nullptr;
	}

	bi->prime_fd = handle->prime_fd;
	bi->buffer_id = (void *) handle;
	bi->width = handle->width;
	bi->height = handle->height;
	bi->format = color_fmt_a2g(handle->format);
	bi->android_format = handle->format;
	bi->usage = handle->usage;
	bi->stride = handle->stride;
	bi->android_stride = handle->stride / color_bytes_per_pixel(bi->format);
	bi->size = (bi->width * bi->height);
	
	if (handle->usage & GRALLOC_USAGE_PROTECTED)
		bi->is_protected = true;

	LOG_V("handle=%p, fd=%d, %dx%d, hal fmt=%d, gbm fmt=0x%x, usage=0x%llx, stride=%u.",
	      handle, bi->prime_fd, bi->width, bi->height, bi->android_format,
	      (uint32_t) bi->format, (unsigned long long) bi->usage, bi->stride);

	return bi;
}

int MapperBackendDmaBuf::importBuffer(const native_handle_t* _Nonnull handle)
{
	LOG_TRACE();

	gralloc_handle_t *ghandle = gralloc_handle(handle);
	if (!ghandle) {
		LOG_E("Failed to get gralloc handle!");
		return -EINVAL;
	}
	buffer_info_t *bi = buffer_info_create(ghandle);
	if (!bi) {
		LOG_E("Failed to create buffer info!");
		return -EINVAL;
	}

	importedBufferTable.insert(std::pair<int, buffer_info_t *>(bi->prime_fd, bi));

	LOG_D("imported buffer (handle=%p, fd=%d): %ux%u, hal fmt=%u, usage=0x%x, stride=%u",
	      handle, ghandle->prime_fd, ghandle->width, ghandle->height,
	      ghandle->format, ghandle->usage, ghandle->stride);
	return 0;
}

int MapperBackendDmaBuf::freeBuffer(buffer_handle_t _Nonnull buffer)
{
	LOG_TRACE();

	gralloc_handle_t *ghandle = gralloc_handle(buffer);
	if (!ghandle) {
		LOG_E("Failed to get gralloc handle!");
		return -EINVAL;
	}

	const int fd = ghandle->prime_fd;
	buffer_info_t *bi = nullptr;
	auto it = importedBufferTable.find(fd);
	if (it != importedBufferTable.end()) {
		bi = it->second;
	}
	if (!bi) {
		LOG_E("no managed buffer for fd=%d, handle=%p.", fd, buffer);
		return -EINVAL;
	}
	free(bi);

	importedBufferTable.erase(fd);

	LOG_D("freed buffer (handle=%p, fd=%d)", buffer, fd);
	return 0;
}

int MapperBackendDmaBuf::lock(buffer_handle_t _Nonnull buffer, uint64_t cpuUsage, ARect accessRegion,
				  int acquireFence, void* _Nullable* _Nonnull outData)
{
	LOG_TRACE();

	gralloc_handle_t *ghandle = gralloc_handle(buffer);
	if (!ghandle) {
		LOG_E("Failed to get gralloc handle!");
		return -EINVAL;
	}
	LOG_V("buffer=%p, fd=%d, cpuUsage=0x%llx, region=[%d,%d,%d,%d], acquireFence=%d.",
	      buffer, ghandle->prime_fd, (unsigned long long) cpuUsage,
	      accessRegion.left, accessRegion.top, accessRegion.right, accessRegion.bottom,
	      acquireFence);

	buffer_info_t *bi = nullptr;
	auto it = importedBufferTable.find(ghandle->prime_fd);
	if (it != importedBufferTable.end()) {
		bi = it->second;
	}
	if (!bi) {
		LOG_E("no managed buffer for fd=%d, handle=%p.", ghandle->prime_fd, buffer);
		return -EINVAL;
	}

	if (!bi->mapped) {
		void *p = mmap(nullptr, bi->size, PROT_READ | PROT_WRITE, MAP_SHARED, bi->prime_fd, 0);
		if (p == MAP_FAILED) {
			LOG_E("mmap failed for fd=%d (size=%zu): %s", bi->prime_fd, bi->size,
			      strerror(errno));
			return -errno;
		}
		bi->mapped = p;
		LOG_V("mapped DMA-BUF fd=%d at %p (size=%zu).", bi->prime_fd, p, bi->size);
	}

	/* Framebuffer must NOT be locked */
	if (cpuUsage & GRALLOC_USAGE_HW_FB) {
		LOG_W("The framebuffer shouldn't be locked!");
		return -EINVAL;
	}

	bool cpu_read = false;
	bool cpu_write = false;
	if (cpuUsage & GRALLOC_USAGE_SW_READ_OFTEN || cpuUsage & GRALLOC_USAGE_SW_READ_RARELY)
		cpu_read = true;
	if (cpuUsage & GRALLOC_USAGE_SW_WRITE_OFTEN || cpuUsage & GRALLOC_USAGE_SW_WRITE_RARELY)
		cpu_write = true;
	
	SyncType syncType = SyncType::kSyncReadWrite;
	if (cpu_read && cpu_write)
		syncType = SyncType::kSyncReadWrite;
	else if (cpu_read)
		syncType = SyncType::kSyncRead;
	else if (cpu_write)
		syncType = SyncType::kSyncWrite;
	else
		LOG_W("Unknown CPU usage for locking buffer, use RW by default.");

	LOG_V("cpu sync start (fd=%d, syncType=%d), outData=%p.",
	      ghandle->prime_fd, static_cast<int>(syncType), bi->mapped);
	allocator->CpuSyncStart(ghandle->prime_fd, syncType);
	*outData = bi->mapped;

	return 0;
}

int MapperBackendDmaBuf::unlock(buffer_handle_t _Nonnull buffer, int* _Nonnull releaseFence)
{
	LOG_TRACE();

	gralloc_handle_t *ghandle = gralloc_handle(buffer);
	if (!ghandle) {
		LOG_E("Failed to get gralloc handle!");
		return -EINVAL;
	}
	LOG_V("buffer=%p, fd=%d.", buffer, ghandle->prime_fd);

	/* Framebuffer must NOT be locked */
	if (ghandle->usage & GRALLOC_USAGE_HW_FB) {
		LOG_W("The framebuffer shouldn't be locked!");
		return -EINVAL;
	}

	bool cpu_read = false;
	bool cpu_write = false;
	if (ghandle->usage & GRALLOC_USAGE_SW_READ_OFTEN || ghandle->usage & GRALLOC_USAGE_SW_READ_RARELY)
		cpu_read = true;
	if (ghandle->usage & GRALLOC_USAGE_SW_WRITE_OFTEN || ghandle->usage & GRALLOC_USAGE_SW_WRITE_RARELY)
		cpu_write = true;
	
	SyncType syncType = SyncType::kSyncReadWrite;
	if (cpu_read && cpu_write)
		syncType = SyncType::kSyncReadWrite;
	else if (cpu_read)
		syncType = SyncType::kSyncRead;
	else if (cpu_write)
		syncType = SyncType::kSyncWrite;
	else
		LOG_W("Unknown CPU usage for locking buffer, use RW by default.");

	LOG_V("cpu sync end (fd=%d, syncType=%d).",
	      ghandle->prime_fd, static_cast<int>(syncType));
	allocator->CpuSyncEnd(ghandle->prime_fd, syncType);

	return 0;
}

int MapperBackendDmaBuf::flushLockedBuffer(buffer_handle_t _Nonnull buffer)
{
	LOG_TRACE();
	LOG_V("buffer=%p (no-op).", buffer);

	return 0;
}

int MapperBackendDmaBuf::rereadLockedBuffer(buffer_handle_t _Nonnull buffer)
{
	LOG_TRACE();
	LOG_V("buffer=%p (no-op).", buffer);

	return 0;
}

template <typename F, StandardMetadataType metadataType>
int32_t queryAndroidBufferMetadata(buffer_handle_t handle, F&& provide,
					     StandardMetadata<metadataType>)
{
	LOG_TRACE();

	gralloc_handle_t *ghandle = gralloc_handle(handle);
	if (!ghandle) {
		LOG_E("Failed to get gralloc handle!");
		return -EINVAL;
	}

	buffer_info_t *info = nullptr;
	auto it = importedBufferTable.find(ghandle->prime_fd);
	if (it != importedBufferTable.end()) {
		info = it->second;
	}
	if (!info) {
		LOG_E("Failed to get managed buffer.");
		return -EINVAL;
	}

	if constexpr (metadataType == StandardMetadataType::BUFFER_ID) {
		return provide(reinterpret_cast<uint64_t>(info->buffer_id));
	}
	if constexpr (metadataType == StandardMetadataType::WIDTH) {
		return provide(static_cast<int32_t>(info->width));
	}
	if constexpr (metadataType == StandardMetadataType::HEIGHT) {
		return provide(static_cast<int32_t>(info->height));
	}
	if constexpr (metadataType == StandardMetadataType::LAYER_COUNT) {
		return provide(static_cast<int32_t>(1));
	}
	if constexpr (metadataType == StandardMetadataType::PIXEL_FORMAT_REQUESTED) {
		return provide(static_cast<PixelFormat>(info->android_format));
	}
	if constexpr (metadataType == StandardMetadataType::PIXEL_FORMAT_FOURCC) {
		return provide(static_cast<uint32_t>(info->format));
	}
	if constexpr (metadataType == StandardMetadataType::USAGE) {
		return provide(static_cast<BufferUsage>(info->usage));
	}
	if constexpr (metadataType == StandardMetadataType::ALLOCATION_SIZE) {
		return provide(static_cast<uint64_t>(info->size));
	}
	if constexpr (metadataType == StandardMetadataType::PROTECTED_CONTENT) {
		return provide(static_cast<bool>(info->is_protected));
	}
	if constexpr (metadataType == StandardMetadataType::COMPRESSION) {
		return provide(android::gralloc4::Compression_None);
	}
	if constexpr (metadataType == StandardMetadataType::INTERLACED) {
		return provide(android::gralloc4::Interlaced_None);
	}
	if constexpr (metadataType == StandardMetadataType::CHROMA_SITING) {
		return provide(android::gralloc4::ChromaSiting_None);
	}
	if constexpr (metadataType == StandardMetadataType::CROP) {
		const uint32_t numPlanes = 1; // FIXME: We only support 1 currently
		std::vector<aidl::android::hardware::graphics::common::Rect> crops;
		for (uint32_t plane = 0; plane < numPlanes; plane++) {
			aidl::android::hardware::graphics::common::Rect crop;
			crop.left = 0;
			crop.top = 0;
			crop.right = info->width;
			crop.bottom = info->height;
			crops.push_back(crop);
		}
		return provide(crops);
	}
	if constexpr (metadataType == StandardMetadataType::STRIDE) {
		return provide(static_cast<int32_t>(info->android_stride));
	}

	LOG_W("Unknown or unsupported metadata type: %s", toString(metadataType).c_str());
	return AIMAPPER_ERROR_UNSUPPORTED;
}

int MapperBackendDmaBuf::getStandardMetadata(buffer_handle_t _Nonnull buffer,
						 int64_t standardMetadataType,
						 void* _Nullable destBuffer, size_t destBufferSize)
{
	LOG_TRACE();
	LOG_V("buffer=%p, type=%s, dest size=%zu.",
	      buffer, toString(static_cast<StandardMetadataType>(standardMetadataType)).c_str(),
	      destBufferSize);

	auto provider = [&]<StandardMetadataType T>(auto&& provide) -> int32_t {
		return queryAndroidBufferMetadata(buffer, provide, StandardMetadata<T>{});
	};

	int ret = provideStandardMetadata(static_cast<StandardMetadataType>(standardMetadataType),
					  destBuffer, destBufferSize, provider);
	if (ret < 0)
		LOG_E("type %s is not available for buffer %p.",
		      toString(static_cast<StandardMetadataType>(standardMetadataType)).c_str(), buffer);
	return ret;
}

int MapperBackendDmaBuf::setStandardMetadata(buffer_handle_t _Nonnull buffer,
						 int64_t standardMetadataType,
						 buffer_metadata_t* _Nonnull metadata)
{
	LOG_TRACE();
	LOG_V("buffer=%p, type=%s.",
	      buffer, toString(static_cast<StandardMetadataType>(standardMetadataType)).c_str());

	LOG_D("Setting metadata is not supported for buffers allocated with DMA-BUF, ignoring.");

	return 0;
}
