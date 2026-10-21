/*
 * SPDX-FileCopyrightText: Copyright 2026 Valentin Liu (LIU, YUANCHEN) <valentinliu@icloud.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <aidl/android/hardware/graphics/common/StandardMetadataType.h>

#define LOG_TAG "MapperGrallocGbm"
#include <private/log.h>

#include "MapperGrallocGbm.hpp"
#include "../MapperPlaneLayouts.hpp"

using namespace ::android::hardware::graphics::mapper;

bool MapperBackendGrallocGbm::isReady() const
{
	bool ready = is_gralloc_gbm_ready();
	LOG_V("Backend ready: %d.", ready);
	return ready;
}

int MapperBackendGrallocGbm::init()
{
	int ret = gralloc_gbm_init();
	LOG_D("gralloc_gbm_init returned %d.", ret);
	return ret;
}

int MapperBackendGrallocGbm::importBuffer(const native_handle_t* _Nonnull handle)
{
	LOG_TRACE();
	LOG_V("handle=%p.", handle);

	int ret = gralloc_gbm_android_buffer_import(handle);
	if (ret)
		LOG_E("gralloc_gbm_android_buffer_import() returned %d for handle %p.",
		      ret, handle);
	return ret;
}

int MapperBackendGrallocGbm::freeBuffer(buffer_handle_t _Nonnull buffer)
{
	LOG_TRACE();
	LOG_V("buffer=%p.", buffer);

	int ret = gralloc_gbm_android_buffer_free(buffer);
	if (ret)
		LOG_E("gralloc_gbm_android_buffer_free() returned %d for buffer %p.",
		      ret, buffer);
	return ret;
}

int MapperBackendGrallocGbm::lock(buffer_handle_t _Nonnull buffer, uint64_t cpuUsage, ARect accessRegion,
				  int acquireFence, void* _Nullable* _Nonnull outData)
{
	LOG_TRACE();
	LOG_V("buffer=%p, cpuUsage=0x%llx, region=[%d,%d,%d,%d], acquireFence=%d.",
	      buffer, (unsigned long long) cpuUsage,
	      accessRegion.left, accessRegion.top, accessRegion.right, accessRegion.bottom,
	      acquireFence);

	int ret = gralloc_gbm_android_buffer_lock(buffer, cpuUsage,
						  accessRegion.top, accessRegion.bottom,
						  accessRegion.left, accessRegion.right,
						  outData);
	if (ret)
		LOG_E("gralloc_gbm_android_buffer_lock() returned %d for buffer %p.",
		      ret, buffer);
	else
		LOG_V("buffer=%p mapped at %p.", buffer, *outData);
	return ret;
}

int MapperBackendGrallocGbm::unlock(buffer_handle_t _Nonnull buffer, int* _Nonnull releaseFence)
{
	LOG_TRACE();
	LOG_V("buffer=%p.", buffer);

	int ret = gralloc_gbm_android_buffer_unlock(buffer);
	if (ret)
		LOG_E("gralloc_gbm_android_buffer_unlock() returned %d for buffer %p.",
		      ret, buffer);
	return ret;
}

int MapperBackendGrallocGbm::flushLockedBuffer(buffer_handle_t _Nonnull buffer)
{
	LOG_TRACE();
	LOG_V("buffer=%p (no-op).", buffer);
	return 0;
}

int MapperBackendGrallocGbm::rereadLockedBuffer(buffer_handle_t _Nonnull buffer)
{
	LOG_TRACE();
	LOG_V("buffer=%p (no-op).", buffer);
	return 0;
}

template <typename F, StandardMetadataType metadataType>
int32_t grallocGbmQueryAndroidBufferMetadata(buffer_handle_t handle, F&& provide,
					     StandardMetadata<metadataType>)
{
	android_buffer_info_t info;
	if (gralloc_gbm_android_buffer_query(handle, &info)) {
		LOG_E("gralloc_gbm_android_buffer_query() failed for handle %p.",
		      handle);
		return AIMAPPER_ERROR_NO_RESOURCES;
	}

	if constexpr (metadataType == StandardMetadataType::BUFFER_ID) {
		return provide(reinterpret_cast<uint64_t>(info.buffer_id));
	}
	if constexpr (metadataType == StandardMetadataType::NAME) {
		if (info.name)
			return provide(reinterpret_cast<const char *>(info.name));

		return provide("<unknown name>");
	}
	if constexpr (metadataType == StandardMetadataType::WIDTH) {
		return provide(static_cast<int32_t>(info.width));
	}
	if constexpr (metadataType == StandardMetadataType::HEIGHT) {
		return provide(static_cast<int32_t>(info.height));
	}
	if constexpr (metadataType == StandardMetadataType::LAYER_COUNT) {
		return provide(static_cast<int32_t>(info.layer_count));
	}
	if constexpr (metadataType == StandardMetadataType::PIXEL_FORMAT_REQUESTED) {
		return provide(static_cast<PixelFormat>(info.android_format));
	}
	if constexpr (metadataType == StandardMetadataType::PIXEL_FORMAT_FOURCC) {
		return provide(static_cast<uint32_t>(info.gbm_format));
	}
	if constexpr (metadataType == StandardMetadataType::PIXEL_FORMAT_MODIFIER) {
		return provide(info.modifier);
	}
	if constexpr (metadataType == StandardMetadataType::USAGE) {
		return provide(static_cast<BufferUsage>(info.usage));
	}
	if constexpr (metadataType == StandardMetadataType::ALLOCATION_SIZE) {
		return provide(static_cast<uint64_t>(info.size));
	}
	if constexpr (metadataType == StandardMetadataType::PROTECTED_CONTENT) {
		return provide(static_cast<bool>(info.is_protected));
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
	if constexpr (metadataType == StandardMetadataType::PLANE_LAYOUTS) {
		int32_t num = info.plane_count;
		std::vector<PlaneLayout> planeLayouts;
		if (getPlaneLayouts(info.gbm_format, &planeLayouts)) {
			return AIMAPPER_ERROR_UNSUPPORTED;
		}

		for (size_t plane = 0; plane < planeLayouts.size(); plane++) {
			PlaneLayout& planeLayout = planeLayouts[plane];
			planeLayout.offsetInBytes = 0;
			planeLayout.strideInBytes = info.stride * color_bytes_per_pixel(info.gbm_format);
			// FIXME: vertical_subsampling=1 for now
			planeLayout.totalSizeInBytes = planeLayout.strideInBytes * DIV_ROUND_UP(info.height, 1);
			planeLayout.widthInSamples = info.width / planeLayout.horizontalSubsampling;
			planeLayout.heightInSamples = info.height / planeLayout.verticalSubsampling;
		}
        	return provide(planeLayouts);
	}
	if constexpr (metadataType == StandardMetadataType::CROP) {
		const uint32_t numPlanes = 1; // FIXME: We only support 1 currently
		std::vector<aidl::android::hardware::graphics::common::Rect> crops;
		for (uint32_t plane = 0; plane < numPlanes; plane++) {
			aidl::android::hardware::graphics::common::Rect crop;
			crop.left = 0;
			crop.top = 0;
			crop.right = info.width;
			crop.bottom = info.height;
			crops.push_back(crop);
		}
		return provide(crops);
	}
	if constexpr (metadataType == StandardMetadataType::DATASPACE) {
		return provide(static_cast<Dataspace>(info.metadata.dataspace));
	}
	if constexpr (metadataType == StandardMetadataType::BLEND_MODE) {
		return provide(static_cast<BlendMode>(info.metadata.dataspace));
	}
	if constexpr (metadataType == StandardMetadataType::SMPTE2086) {
		std::optional<Smpte2086> smpte2086;
		smpte2086->primaryRed = XyColor(info.metadata.smpte2086->primary_red_x, info.metadata.smpte2086->primary_red_y);
		smpte2086->primaryGreen = XyColor(info.metadata.smpte2086->primary_green_x, info.metadata.smpte2086->primary_green_y);
		smpte2086->primaryBlue = XyColor(info.metadata.smpte2086->primary_blue_x, info.metadata.smpte2086->primary_blue_y);
		smpte2086->whitePoint = XyColor(info.metadata.smpte2086->white_point_x, info.metadata.smpte2086->white_point_y);
		smpte2086->maxLuminance = info.metadata.smpte2086->max_luminance;
		smpte2086->minLuminance = info.metadata.smpte2086->min_luminance;
		return provide(smpte2086);
	}
	if constexpr (metadataType == StandardMetadataType::CTA861_3) {
		std::optional<Cta861_3> cta861_3;
		cta861_3->maxContentLightLevel = info.metadata.cta861_3->max_content_light_level;
		cta861_3->maxFrameAverageLightLevel = info.metadata.cta861_3->max_frame_average_light_level;
		return provide(cta861_3);
	}
	if constexpr (metadataType == StandardMetadataType::SMPTE2094_40) {
		return AIMAPPER_ERROR_UNSUPPORTED;
	}
	if constexpr (metadataType == StandardMetadataType::SMPTE2094_10) {
		return AIMAPPER_ERROR_UNSUPPORTED;
	}
	if constexpr (metadataType == StandardMetadataType::STRIDE) {
		return provide(static_cast<int32_t>(info.stride));
	}
	if constexpr (metadataType == StandardMetadataType::SMPTE2094_50) {
		if (info.metadata.smpte2094_50_size > 0) {
			std::vector<uint8_t> data(info.metadata.smpte2094_50, info.metadata.smpte2094_50 + info.metadata.smpte2094_50_size);
			std::optional<std::vector<uint8_t>> smpte2094_50(data);
			return provide(smpte2094_50);
		}
		std::optional<std::vector<uint8_t>> zero;
		return provide(zero);
	}

	LOG_W("Unknown metadata type: %s", toString(metadataType).c_str());
	return AIMAPPER_ERROR_UNSUPPORTED;
}

int MapperBackendGrallocGbm::getStandardMetadata(buffer_handle_t _Nonnull buffer,
						 int64_t standardMetadataType,
						 void* _Nullable destBuffer, size_t destBufferSize)
{
	LOG_TRACE();
	LOG_V("buffer=%p, type=%lld, dest size=%zu.",
	      buffer, (long long) standardMetadataType, destBufferSize);

	auto provider = [&]<StandardMetadataType T>(auto&& provide) -> int32_t {
		return grallocGbmQueryAndroidBufferMetadata(buffer, provide, StandardMetadata<T>{});
	};

	int ret = provideStandardMetadata(static_cast<StandardMetadataType>(standardMetadataType),
					  destBuffer, destBufferSize, provider);
	if (ret < 0)
		LOG_E("type %lld is not available for buffer %p.",
		      (long long) standardMetadataType, buffer);
	return ret;
}

int MapperBackendGrallocGbm::setStandardMetadata(buffer_handle_t _Nonnull buffer,
						 int64_t standardMetadataType,
						 buffer_metadata_t* _Nonnull metadata)
{
	LOG_TRACE();
	LOG_V("buffer=%p, type=%lld.", buffer, (long long) standardMetadataType);

	gbm_bo_data_t *bo_data = gralloc_gbm_android_buffer_get_extdata(buffer);
	if (!bo_data) {
		LOG_E("Unable to find the extend data of buffer!");
		return -EINVAL;
	}

	switch (static_cast<StandardMetadataType>(standardMetadataType)) {
	case StandardMetadataType::DATASPACE:
		bo_data->metadata.dataspace = metadata->dataspace;
		break;
	case StandardMetadataType::BLEND_MODE:
		bo_data->metadata.blend_mode = metadata->blend_mode;
		break;
	case StandardMetadataType::SMPTE2086:
		bo_data->metadata.smpte2086 = metadata->smpte2086;
		break;
	case StandardMetadataType::CTA861_3:
		bo_data->metadata.cta861_3 = metadata->cta861_3;
		break;
	case StandardMetadataType::SMPTE2094_50:
		bo_data->metadata.smpte2094_50 = metadata->smpte2094_50;
		bo_data->metadata.smpte2094_50_size = metadata->smpte2094_50_size;
		break;
	case StandardMetadataType::SMPTE2094_40:
	case StandardMetadataType::SMPTE2094_10:
	default:
		// nothing to do.
		(void)0;
	}
	return 0;
}
