/*
 * SPDX-FileCopyrightText: Copyright 2026 Valentin Liu (LIU, YUANCHEN) <valentinliu@icloud.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "GrallocGenericMapperV5"
#include <private/log.h>

#include <aidl/android/hardware/graphics/allocator/BufferDescriptorInfo.h>
#include <aidl/android/hardware/graphics/common/BufferUsage.h>
#include <aidl/android/hardware/graphics/common/PixelFormat.h>
#include <aidl/android/hardware/graphics/common/StandardMetadataType.h>
#include <android-base/unique_fd.h>
#include <android/gralloc_handle.h>
#include <android/hardware/graphics/mapper/IMapper.h>
#include <android/hardware/graphics/mapper/utils/IMapperMetadataTypes.h>
#include <android/hardware/graphics/mapper/utils/IMapperProvider.h>
#include <cutils/native_handle.h>

#include "backend/MapperDmaBuf.hpp"
#include "backend/MapperGrallocGbm.hpp"
#include "MapperBackendImpl.hpp"

#define CHECK_BACKEND()	\
	do {		\
		if (mBackend == nullptr) {	\
			LOG_E("Invalid backend pointer");			\
			return AIMAPPER_ERROR_NO_RESOURCES;		\
		}	\
		if (!mBackend->isReady()) {	\
			if (mBackend->init()) {	\
				LOG_E("Backend initialization failed");	\
				return AIMAPPER_ERROR_NO_RESOURCES;	\
			}	\
		}	\
	} while (0)

using namespace ::android::hardware::graphics::mapper;
using ::aidl::android::hardware::graphics::allocator::BufferDescriptorInfo;
using ::android::base::unique_fd;
using ::gralloc_generic::MapperBackendImpl;

constexpr const char* STANDARD_METADATA_NAME =
        "android.hardware.graphics.common.StandardMetadataType";

inline bool is_standard_metadata(const AIMapper_MetadataType type)
{
	return (strcmp(STANDARD_METADATA_NAME, type.name) == 0);
}

inline bool is_native_handle_valid(const native_handle_t *handle)
{
	if (!handle || handle->numFds == 0) {
		return false;
	}
	return true;
}

class GrallocGenericMapperV5 final : public vendor::mapper::IMapperV5Impl {
public:
	explicit GrallocGenericMapperV5() = default;
	~GrallocGenericMapperV5() = default;

	AIMapper_Error importBuffer(const native_handle_t* _Nonnull handle,
				buffer_handle_t _Nullable* _Nonnull outBufferHandle) override;

	AIMapper_Error freeBuffer(buffer_handle_t _Nonnull buffer) override;

	AIMapper_Error getTransportSize(buffer_handle_t _Nonnull buffer, uint32_t* _Nonnull outNumFds,
					uint32_t* _Nonnull outNumInts) override;

	AIMapper_Error lock(buffer_handle_t _Nonnull buffer, uint64_t cpuUsage, ARect accessRegion,
			int acquireFence, void* _Nullable* _Nonnull outData) override;

	AIMapper_Error unlock(buffer_handle_t _Nonnull buffer, int* _Nonnull releaseFence) override;

	AIMapper_Error flushLockedBuffer(buffer_handle_t _Nonnull buffer) override;

	AIMapper_Error rereadLockedBuffer(buffer_handle_t _Nonnull buffer) override;

	int32_t getMetadata(buffer_handle_t _Nonnull buffer, AIMapper_MetadataType metadataType,
			void* _Nonnull outData, size_t outDataSize) override;

	int32_t getStandardMetadata(buffer_handle_t _Nonnull buffer, int64_t standardMetadataType,
				void* _Nonnull outData, size_t outDataSize) override;

	AIMapper_Error setMetadata(buffer_handle_t _Nonnull buffer, AIMapper_MetadataType metadataType,
				   const void* _Nonnull metadata, size_t metadataSize) override;

	AIMapper_Error setStandardMetadata(buffer_handle_t _Nonnull buffer,
					   int64_t standardMetadataType, const void* _Nonnull metadata,
					   size_t metadataSize) override;

	AIMapper_Error listSupportedMetadataTypes(
		const AIMapper_MetadataTypeDescription* _Nullable* _Nonnull outDescriptionList,
		size_t* _Nonnull outNumberOfDescriptions) override;

	AIMapper_Error dumpBuffer(buffer_handle_t _Nonnull bufferHandle,
				  AIMapper_DumpBufferCallback _Nonnull dumpBufferCallback,
				  void* _Null_unspecified context) override;

	AIMapper_Error dumpAllBuffers(AIMapper_BeginDumpBufferCallback _Nonnull beginDumpBufferCallback,
				  AIMapper_DumpBufferCallback _Nonnull dumpBufferCallback,
				  void* _Null_unspecified context) override;

	AIMapper_Error getReservedRegion(buffer_handle_t _Nonnull buffer,
					 void* _Nullable* _Nonnull outReservedRegion,
					 uint64_t* _Nonnull outReservedSize) override;
private:
	/// returns a shared-singleton Gralloc GBM backend
	std::shared_ptr<MapperBackendImpl> fetchBackendGrallocGbm();
	std::shared_ptr<MapperBackendImpl> fetchBackendDmaBuf();

	std::shared_ptr<MapperBackendImpl> mBackend;
	std::shared_ptr<MapperBackendImpl> selectBackendByType(const MapperBackendType type);
	int selectBackend(buffer_handle_t _Nonnull buffer);
};

std::shared_ptr<MapperBackendImpl> GrallocGenericMapperV5::fetchBackendDmaBuf()
{
	static std::mutex mutex;
	static std::weak_ptr<MapperBackendImpl> dmaBufBackend;
	std::lock_guard<std::mutex> lock(mutex);
	std::shared_ptr<MapperBackendImpl> backend = dmaBufBackend.lock();
	if (backend == nullptr) {
		backend = std::make_shared<MapperBackendDmaBuf>();
		dmaBufBackend = backend;
	}
	return backend;
}

std::shared_ptr<MapperBackendImpl> GrallocGenericMapperV5::fetchBackendGrallocGbm()
{
	static std::mutex mutex;
	static std::weak_ptr<MapperBackendImpl> gbmBackend;
	std::lock_guard<std::mutex> lock(mutex);
	std::shared_ptr<MapperBackendImpl> backend = gbmBackend.lock();
	if (backend == nullptr) {
		backend = std::make_shared<MapperBackendGrallocGbm>();
		gbmBackend = backend;
	}
	return backend;
}

std::shared_ptr<MapperBackendImpl> GrallocGenericMapperV5::selectBackendByType(const MapperBackendType type)
{
	std::shared_ptr<MapperBackendImpl> backend;
	switch (type) {
	case MapperBackendType::MAPPER_GRALLOC_DMABUF:
		backend = fetchBackendDmaBuf();
		break;
	case MapperBackendType::MAPPER_GRALLOC_GBM:
	default:
		backend = fetchBackendGrallocGbm();
	}
	assert((backend != nullptr));
	return backend;
}

inline bool shouldUseDrmBackend(buffer_handle_t _Nonnull buffer)
{
	gralloc_handle_t *handle = gralloc_handle(buffer);
	assert(handle);

	/*
	 * Linear buffers (1D) should use DMA-BUF.
	 */
	if (handle->format == HAL_PIXEL_FORMAT_BLOB && handle->height == 1)
		return true;

	/*
	 * We currently allocate DMA-BUF buffers for encoders.
	 */
	if (handle->usage & (GRALLOC_USAGE_HW_VIDEO_ENCODER | GRALLOC_USAGE_HW_IMAGE_ENCODER))
		return true;

	return false;
}

int GrallocGenericMapperV5::selectBackend(buffer_handle_t _Nonnull buffer)
{
	LOG_TRACE();

	std::shared_ptr<MapperBackendImpl> backend;

	if (shouldUseDrmBackend(buffer)) {
		LOG_D("Selected DMA-BUF backend.");
		backend = selectBackendByType(MapperBackendType::MAPPER_GRALLOC_DMABUF);
	} else {
		LOG_D("Selected Gralloc GBM backend.");
		backend = selectBackendByType(MapperBackendType::MAPPER_GRALLOC_GBM);
	}

	mBackend = backend;
	return 0;
}

AIMapper_Error GrallocGenericMapperV5::importBuffer(const native_handle_t* _Nonnull handle,
						buffer_handle_t _Nullable* _Nonnull outBufferHandle)
{
	LOG_TRACE();    
	if (!::is_native_handle_valid(handle)) {
		LOG_E("importBuffer failed: invalid handle (%p).", handle);
		return AIMAPPER_ERROR_BAD_BUFFER;
	}

	selectBackend(handle);
	CHECK_BACKEND();

	native_handle_t *importedBufferHandle = native_handle_clone(handle);
	if (!importedBufferHandle) {
		LOG_E("importBuffer failed: handle clone failed");
		return AIMAPPER_ERROR_NO_RESOURCES;
	}

	if (mBackend->importBuffer((buffer_handle_t) importedBufferHandle) != 0) {
		native_handle_close(importedBufferHandle);
		native_handle_delete(importedBufferHandle);
		LOG_E("importBuffer failed: Backend operation failed.");
		return AIMAPPER_ERROR_NO_RESOURCES;
	}

	*outBufferHandle = (buffer_handle_t) importedBufferHandle;
	return AIMAPPER_ERROR_NONE;
}

AIMapper_Error GrallocGenericMapperV5::freeBuffer(buffer_handle_t _Nonnull buffer)
{
	LOG_TRACE();
	if (!buffer) {
		LOG_E("freeBuffer failed: invalid buffer (%p).", buffer);
		return AIMAPPER_ERROR_BAD_BUFFER;
	}

	selectBackend(buffer);
	CHECK_BACKEND();

	if (mBackend->freeBuffer(buffer) != 0) {
		LOG_E("freeBuffer failed: Backend operation failed.");
		return AIMAPPER_ERROR_NO_RESOURCES;
	}

	return AIMAPPER_ERROR_NONE;
}

AIMapper_Error GrallocGenericMapperV5::getTransportSize(buffer_handle_t _Nonnull buffer, uint32_t* _Nonnull outNumFds,
						    uint32_t* _Nonnull outNumInts)
{
	LOG_TRACE();
	if (!buffer) {
		LOG_E("getTransportSize failed: invalid buffer (%p).", buffer);
		return AIMAPPER_ERROR_BAD_BUFFER;
	}

	*outNumFds = buffer->numFds;
	*outNumInts = buffer->numInts;
	return AIMAPPER_ERROR_NONE;
}

AIMapper_Error GrallocGenericMapperV5::lock(buffer_handle_t _Nonnull buffer, uint64_t cpuUsage, ARect accessRegion,
					int acquireFence, void* _Nullable* _Nonnull outData)
{
	LOG_TRACE();
	if (!buffer) {
		LOG_E("lock failed: invalid buffer (%p).", buffer);
		return AIMAPPER_ERROR_BAD_BUFFER;
	}

	if (cpuUsage == 0) {
		LOG_E("lock failed: invalid usage.");
		return AIMAPPER_ERROR_BAD_VALUE;
	}

	selectBackend(buffer);
	CHECK_BACKEND();

	if (mBackend->lock(buffer, cpuUsage, accessRegion, acquireFence, outData) != 0) {
		LOG_E("lock failed: Backend operation failed.");
		return AIMAPPER_ERROR_NO_RESOURCES;
	}

	return AIMAPPER_ERROR_NONE;
}

AIMapper_Error GrallocGenericMapperV5::unlock(buffer_handle_t _Nonnull buffer, int* _Nonnull releaseFence)
{
	LOG_TRACE();
	if (!buffer) {
		LOG_E("unlock failed: invalid buffer (%p).", buffer);
		return AIMAPPER_ERROR_BAD_BUFFER;
	}

	selectBackend(buffer);
	CHECK_BACKEND();

	if (mBackend->unlock(buffer, releaseFence) != 0) {
		LOG_E("unlock failed: Backend operation failed.");
		return AIMAPPER_ERROR_NO_RESOURCES;
	}

	return AIMAPPER_ERROR_NONE;
}

AIMapper_Error GrallocGenericMapperV5::flushLockedBuffer(buffer_handle_t _Nonnull buffer)
{
	LOG_TRACE();

	selectBackend(buffer);
	CHECK_BACKEND();

	if (mBackend->flushLockedBuffer(buffer) != 0) {
		LOG_E("flushLockedBuffer failed: Backend operation failed.");
		return AIMAPPER_ERROR_NO_RESOURCES;
	}
	return AIMAPPER_ERROR_NONE;
}

AIMapper_Error GrallocGenericMapperV5::rereadLockedBuffer(buffer_handle_t _Nonnull buffer)
{
	LOG_TRACE();

	selectBackend(buffer);
	CHECK_BACKEND();

	if (mBackend->rereadLockedBuffer(buffer) != 0) {
		LOG_E("rereadLockedBuffer failed: Backend operation failed.");
		return AIMAPPER_ERROR_NO_RESOURCES;
	}
	return AIMAPPER_ERROR_NONE;
}

constexpr AIMapper_MetadataTypeDescription newStandardMetadata(StandardMetadataType type,
							       bool isGettable, bool isSettable)
{
	return {
		{STANDARD_METADATA_NAME, static_cast<int64_t>(type)},
		nullptr,
		isGettable,
		isSettable,
		{0}
	};
}

int32_t GrallocGenericMapperV5::getMetadata(buffer_handle_t _Nonnull buffer, AIMapper_MetadataType metadataType,
					void* _Nonnull outData, size_t outDataSize)
{
	LOG_TRACE();
	if (!buffer) {
		LOG_E("getMetadata failed: invalid buffer (%p).", buffer);
		return AIMAPPER_ERROR_BAD_BUFFER;
	}

	if (is_standard_metadata(metadataType))
		return getStandardMetadata(buffer, metadataType.value, outData, outDataSize);

	LOG_E("getMetadata failed: Non-standard metadata (%s) is unsupported!", metadataType.name);
	return AIMAPPER_ERROR_UNSUPPORTED;
}

int32_t GrallocGenericMapperV5::getStandardMetadata(buffer_handle_t _Nonnull buffer, int64_t standardMetadataType,
						    void* _Nonnull outData, size_t outDataSize)
{
	LOG_TRACE();
	if (!buffer) {
		LOG_E("getStandardMetadata failed: invalid buffer (%p).", buffer);
		return AIMAPPER_ERROR_BAD_BUFFER;
	}
	// Convert the int64_t to StandardMetadataType enum and get readable name
	StandardMetadataType metadataTypeEnum = static_cast<StandardMetadataType>(standardMetadataType);
	std::string metadataTypeName = toString(metadataTypeEnum);

	LOG_V("get standard metadata %s", metadataTypeName.c_str());

	selectBackend(buffer);
	CHECK_BACKEND();

	int32_t result = mBackend->getStandardMetadata(buffer, standardMetadataType, outData, outDataSize);
	if (result < 0) {
		LOG_E("getStandardMetadata failed: Backend operation failed.");
		return AIMAPPER_ERROR_NO_RESOURCES;
	}

	return result;
}

AIMapper_Error GrallocGenericMapperV5::setMetadata(buffer_handle_t _Nonnull buffer, AIMapper_MetadataType metadataType,
					       const void* _Nonnull metadata, size_t metadataSize)
{
	LOG_TRACE();
	if (!buffer) {
		LOG_E("setMetadata failed: invalid buffer (%p).", buffer);
		return AIMAPPER_ERROR_BAD_BUFFER;
	}

	if (is_standard_metadata(metadataType))
		return setStandardMetadata(buffer, metadataType.value, metadata, metadataSize);

	LOG_E("setMetadata failed: Non-standard metadata (%s) is unsupported!", metadataType.name);
	return AIMAPPER_ERROR_UNSUPPORTED;
}

AIMapper_Error GrallocGenericMapperV5::setStandardMetadata(buffer_handle_t _Nonnull buffer,
						       int64_t standardMetadataType, const void* _Nonnull metadata,
						       size_t metadataSize)
{
	LOG_TRACE();
	if (!buffer) {
		LOG_E("setStandardMetadata failed: invalid buffer (%p).", buffer);
		return AIMAPPER_ERROR_BAD_BUFFER;
	}

	selectBackend(buffer);
	CHECK_BACKEND();

	// Convert the int64_t to StandardMetadataType enum and get readable name
	StandardMetadataType metadataTypeEnum = static_cast<StandardMetadataType>(standardMetadataType);
	std::string metadataTypeName = toString(metadataTypeEnum);

	LOG_V("set standard metadata %s (size: %zu)", metadataTypeName.c_str(), metadataSize);

	buffer_metadata_t *data = (buffer_metadata_t *) calloc(1, sizeof(buffer_metadata_t));
	if (!data) {
		LOG_E("setStandardMetadata failed: no memory");
		return AIMAPPER_ERROR_NO_RESOURCES;
	}

	switch (metadataTypeEnum) {
        case StandardMetadataType::BUFFER_ID:
        case StandardMetadataType::NAME:
        case StandardMetadataType::WIDTH:
        case StandardMetadataType::HEIGHT:
        case StandardMetadataType::LAYER_COUNT:
        case StandardMetadataType::PIXEL_FORMAT_REQUESTED:
        case StandardMetadataType::PIXEL_FORMAT_FOURCC:
        case StandardMetadataType::PIXEL_FORMAT_MODIFIER:
        case StandardMetadataType::USAGE:
        case StandardMetadataType::STRIDE:
		LOG_E("setStandardMetadata failed: Read-only metadata type (%s).", metadataTypeName.c_str());
		return AIMAPPER_ERROR_BAD_VALUE;
	case StandardMetadataType::DATASPACE:
		data->dataspace = static_cast<int32_t>(*(Dataspace *)metadata);
		LOG_V("set DATASPACE to %d, received %d (%s)", data->dataspace, *(Dataspace *)metadata, toString(*(Dataspace *)metadata).c_str());
		break;
	case StandardMetadataType::BLEND_MODE:
		data->blend_mode = static_cast<int32_t>(*(BlendMode *)metadata);
		LOG_V("set BLEND_MODE to %d, received %d (%s)", data->blend_mode, *(BlendMode *)metadata, toString(*(BlendMode *)metadata).c_str());
		break;
	case StandardMetadataType::SMPTE2086:
		{
			data->smpte2086 = (smpte2086_t *) calloc(1, sizeof(smpte2086_t));
			assert((data->smpte2086 != nullptr));
			const Smpte2086 *smpte2086 = static_cast<const Smpte2086*>(metadata);
			data->smpte2086->primary_red_x = smpte2086->primaryRed.x;
			data->smpte2086->primary_red_y = smpte2086->primaryRed.y;
			data->smpte2086->primary_green_x = smpte2086->primaryGreen.x;
			data->smpte2086->primary_green_y = smpte2086->primaryGreen.y;
			data->smpte2086->primary_blue_x = smpte2086->primaryBlue.x;
			data->smpte2086->primary_blue_y = smpte2086->primaryBlue.y;
			data->smpte2086->white_point_x = smpte2086->whitePoint.x;
			data->smpte2086->white_point_y = smpte2086->whitePoint.y;
			data->smpte2086->max_luminance = smpte2086->maxLuminance;
			data->smpte2086->min_luminance = smpte2086->minLuminance;
			LOG_V("set SMPTE2086 to address %p, received %s", data->smpte2086, smpte2086->toString().c_str());
		}
		break;
	case StandardMetadataType::CTA861_3:
		{
			data->cta861_3 = (cta861_3_t *) calloc(1, sizeof(cta861_3_t));
			assert((data->cta861_3 != nullptr));
			const Cta861_3 *cta861_3 = static_cast<const Cta861_3*>(metadata);
			data->cta861_3->
				max_content_light_level = cta861_3->maxContentLightLevel;
			data->cta861_3->
				max_frame_average_light_level = cta861_3->maxFrameAverageLightLevel;
			LOG_V("set CTA861_3 to address %p, received %s", data->cta861_3, cta861_3->toString().c_str());
		}
		break;
	case StandardMetadataType::SMPTE2094_50:
		if (metadataSize > GRALLOC_GENERIC_SMPTE2094_50_MAX_SIZE) {
			LOG_E("Received payload is out of size limitation");
			return AIMAPPER_ERROR_BAD_VALUE;
		}
		{
			data->smpte2094_50 = (uint8_t *) calloc(metadataSize, sizeof(uint8_t));
			assert((data->smpte2094_50 != nullptr));
			std::vector<uint8_t> rawdata((const uint8_t *)metadata, (const uint8_t *)metadata + metadataSize);
			std::optional<std::vector<uint8_t>> smpte2094_50(rawdata);
			std::copy(smpte2094_50->begin(), smpte2094_50->end(), data->smpte2094_50);
			data->smpte2094_50_size = smpte2094_50->size();
			LOG_V("set SMPTE2094_50 to address %p (size: %d)", data->smpte2094_50,
			      data->smpte2094_50_size);
		}
		break;
	case StandardMetadataType::SMPTE2094_40:
	case StandardMetadataType::SMPTE2094_10:
		LOG_W("known but not implemented metadata type(%s).", metadataTypeName.c_str());
		break;
	default:
		LOG_D("unsupported metadata type (%s).", metadataTypeName.c_str());
	}

	if (mBackend->setStandardMetadata(buffer, standardMetadataType, data) != 0) {
		LOG_E("setStandardMetadata failed: Backend operation failed.");
		return AIMAPPER_ERROR_NO_RESOURCES;
	}

	return AIMAPPER_ERROR_NONE;
}

static constexpr std::array<AIMapper_MetadataTypeDescription, 22> sSupportedMetadataTypes {
	/* Read-only types */
	newStandardMetadata(StandardMetadataType::BUFFER_ID, true, false),
	newStandardMetadata(StandardMetadataType::NAME, false, false),
	newStandardMetadata(StandardMetadataType::WIDTH, true, false),
	newStandardMetadata(StandardMetadataType::HEIGHT, true, false),
	newStandardMetadata(StandardMetadataType::LAYER_COUNT, true, false),
	newStandardMetadata(StandardMetadataType::PIXEL_FORMAT_REQUESTED, true, false),
	newStandardMetadata(StandardMetadataType::PIXEL_FORMAT_FOURCC, true, false),
	newStandardMetadata(StandardMetadataType::PIXEL_FORMAT_MODIFIER, true, false),
	newStandardMetadata(StandardMetadataType::USAGE, true, false),
	newStandardMetadata(StandardMetadataType::ALLOCATION_SIZE, true, false),
	newStandardMetadata(StandardMetadataType::PROTECTED_CONTENT, false, false),
	newStandardMetadata(StandardMetadataType::STRIDE, true, false),
	newStandardMetadata(StandardMetadataType::COMPRESSION, true, false),
	newStandardMetadata(StandardMetadataType::INTERLACED, true, false),
	newStandardMetadata(StandardMetadataType::CHROMA_SITING, true, false),
	newStandardMetadata(StandardMetadataType::PLANE_LAYOUTS, true, false),
	newStandardMetadata(StandardMetadataType::CROP, true, false),
	/* Writable types */
	newStandardMetadata(StandardMetadataType::DATASPACE, true, true),
	newStandardMetadata(StandardMetadataType::BLEND_MODE, true, true),
	newStandardMetadata(StandardMetadataType::SMPTE2086, true, true),
	newStandardMetadata(StandardMetadataType::CTA861_3, true, true),
	newStandardMetadata(StandardMetadataType::SMPTE2094_50, true, true),
};

AIMapper_Error GrallocGenericMapperV5::listSupportedMetadataTypes(const AIMapper_MetadataTypeDescription* _Nullable* _Nonnull outDescriptionList,
							      size_t* _Nonnull outNumberOfDescriptions)
{
	LOG_TRACE();
	*outDescriptionList = sSupportedMetadataTypes.data();
	*outNumberOfDescriptions = sSupportedMetadataTypes.size();

	return AIMAPPER_ERROR_NONE;
}

AIMapper_Error GrallocGenericMapperV5::dumpBuffer(buffer_handle_t _Nonnull bufferHandle,
					      AIMapper_DumpBufferCallback _Nonnull dumpBufferCallback,
					      void* _Null_unspecified context)
{
	LOG_TRACE();
	if (!bufferHandle) {
		LOG_E("dumpBuffer failed: invalid buffer (%p).", bufferHandle);
		return AIMAPPER_ERROR_BAD_BUFFER;
	}

	auto callback = [&](AIMapper_MetadataType type, const std::vector<uint8_t>& buffer) {
		dumpBufferCallback(context, type, buffer.data(), buffer.size());
	};

	return AIMAPPER_ERROR_NONE;
}

AIMapper_Error GrallocGenericMapperV5::dumpAllBuffers(AIMapper_BeginDumpBufferCallback _Nonnull beginDumpBufferCallback,
				  		  AIMapper_DumpBufferCallback _Nonnull dumpBufferCallback,
				  		  void* _Null_unspecified context)
{
	LOG_TRACE();

	auto callback = [&](AIMapper_MetadataType type, const std::vector<uint8_t>& buffer) {
		//beginDumpBufferCallback(context);
		dumpBufferCallback(context, type, buffer.data(), buffer.size());
	};

	return AIMAPPER_ERROR_NONE;
}

// TODO: Add reserved region support.
AIMapper_Error GrallocGenericMapperV5::getReservedRegion(buffer_handle_t _Nonnull buffer,
						     void* _Nullable* _Nonnull outReservedRegion,
						     uint64_t* _Nonnull outReservedSize)
{
	LOG_TRACE();
	if (!buffer) {
		LOG_E("getReservedRegion failed: invalid buffer (%p).", buffer);
		return AIMAPPER_ERROR_BAD_BUFFER;
	}

	*outReservedRegion = nullptr;
	*outReservedSize = 0;
	LOG_W("We currently not support reserved region.");
	return AIMAPPER_ERROR_NONE;
}

extern "C" uint32_t ANDROID_HAL_MAPPER_VERSION = AIMAPPER_VERSION_5;

extern "C" AIMapper_Error AIMapper_loadIMapper(AIMapper* _Nullable* _Nonnull outImplementation) {
	LOG_TRACE();
	assert(outImplementation);

	static vendor::mapper::IMapperProvider<GrallocGenericMapperV5> provider;
	return provider.load(outImplementation);
}
