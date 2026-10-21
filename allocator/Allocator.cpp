/*
 * SPDX-FileCopyrightText: Copyright 2026 Valentin Liu (LIU, YUANCHEN) <valentinliu@icloud.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <android-base/logging.h>
#include <android/binder_ibinder_platform.h>
#include <gralloctypes/Gralloc4.h>

#include "backend/AllocatorDmaBuf.hpp"
#include "backend/AllocatorGrallocGbm.hpp"
#include "Allocator.h"

#define LOG_TAG "GrallocGenericAllocatorV2"
#include <log.h>

#define CHECK_BACKEND()	\
	do {		\
		if (mBackend == nullptr) {	\
			LOG_E("Invalid backend pointer");			\
			return ToBinderStatus(AllocationError::NO_RESOURCES);		\
		}	\
		if (!mBackend->isReady()) {	\
			if (mBackend->init()) {	\
				LOG_E("Backend initialization failed");	\
				return ToBinderStatus(AllocationError::NO_RESOURCES);	\
			}	\
		}	\
	} while (0)

using aidl::android::hardware::common::NativeHandle;
using aidl::android::hardware::graphics::common::ExtendableType;
using BufferDescriptorInfoV4 = android::hardware::graphics::mapper::V4_0::IMapper::BufferDescriptorInfo;

static const std::string STANDARD_METADATA_DATASPACE = "android.hardware.graphics.common.Dataspace";

int AllocatorBackendImpl::generateGrallocGenericDesc(const BufferDescriptorInfo& info, allocator_desc_t* outResult)
{
	LOG_TRACE();
	LOG_V("Descriptor: %s", info.toString().c_str());

	if (!outResult) {
		LOG_E("Invalid out pointer.");
		return -EINVAL;
	}

	if (info.width == 0 || info.height == 0) {
		LOG_E("Invalid buffer descriptor: width or height is zero (width=%u, height=%u)",
		      static_cast<uint32_t>(info.width), static_cast<uint32_t>(info.height));
		return -EINVAL;
	}

	outResult->width = static_cast<uint32_t>(info.width);
	outResult->height = static_cast<uint32_t>(info.height);

	// TODO: Add multiple layer support.
	if (info.layerCount > 1) {
		LOG_E("Failed to convert descriptor. Unsupported layerCount: %d", info.layerCount);
		return -EINVAL;
	}

	outResult->format = static_cast<uint32_t>(info.format);
	outResult->usage = static_cast<uint32_t>(info.usage);
	outResult->reserved_size = static_cast<uint32_t>(info.reservedSize);
	outResult->layer_count = static_cast<uint32_t>(info.layerCount);

	outResult->name = (unsigned char *) calloc((info.name.size() + 1), sizeof(unsigned char));
	if (!outResult->name) {
		LOG_E("No memory for the buffer name.");
		return -ENOMEM;
	}
	memcpy(outResult->name, info.name.data(), info.name.size());

	LOG_V("Generated: %s", allocator_desc_to_string(outResult));

	return 0;
}

namespace aidl::android::hardware::graphics::allocator::impl {

std::shared_ptr<AllocatorBackendImpl> GrallocGenericAllocatorV2::fetchBackendDmaBuf()
{
	static std::mutex mutex;
	static std::weak_ptr<AllocatorBackendImpl> dmaBufBackend;
	std::lock_guard<std::mutex> lock(mutex);
	std::shared_ptr<AllocatorBackendImpl> backend = dmaBufBackend.lock();
	if (backend == nullptr) {
		backend = std::make_shared<AllocatorBackendDmaBuf>();
		dmaBufBackend = backend;
	}
	return backend;
}

std::shared_ptr<AllocatorBackendImpl> GrallocGenericAllocatorV2::fetchBackendGrallocGbm()
{
	static std::mutex mutex;
	static std::weak_ptr<AllocatorBackendImpl> gbmBackend;
	std::lock_guard<std::mutex> lock(mutex);
	std::shared_ptr<AllocatorBackendImpl> backend = gbmBackend.lock();
	if (backend == nullptr) {
		backend = std::make_shared<AllocatorBackendGrallocGbm>();
		gbmBackend = backend;
	}
	return backend;
}

std::shared_ptr<AllocatorBackendImpl> GrallocGenericAllocatorV2::selectBackendByType(const AllocatorBackendType type)
{
	std::shared_ptr<AllocatorBackendImpl> backend;
	switch (type) {
	case AllocatorBackendType::ALLOCATOR_GRALLOC_DMABUF:
		backend = fetchBackendDmaBuf();
		break;
	case AllocatorBackendType::ALLOCATOR_GRALLOC_GBM:
	default:
		backend = fetchBackendGrallocGbm();
	}
	assert((backend != nullptr));
	return backend;
}

int GrallocGenericAllocatorV2::selectBackend(const BufferDescriptorInfo& descriptor)
{
	LOG_TRACE();

	assert(descriptor);
	std::shared_ptr<AllocatorBackendImpl> backend;
	bool supported = false;

	backend = selectBackendByType(AllocatorBackendType::ALLOCATOR_GRALLOC_GBM);
	backend->isSupported(descriptor, &supported);
	LOG_V("Gralloc GBM backend supported=%d.", supported);
	if (supported) {
		LOG_D("Selected Gralloc GBM backend for desc: %s.", descriptor.toString().c_str());
		mBackend = backend;
		return 0;
	}

	backend = selectBackendByType(AllocatorBackendType::ALLOCATOR_GRALLOC_DMABUF);
	backend->isSupported(descriptor, &supported);
	LOG_V("DMA-BUF backend supported=%d.", supported);
	if (supported) {
		LOG_D("Selected DMA-BUF backend for desc: %s.", descriptor.toString().c_str());
		mBackend = backend;
		return 0;
	}

	LOG_E("No backend supports the descriptor: %s.", descriptor.toString().c_str());
	return -EINVAL;
}

int GrallocGenericAllocatorV2::init(void)
{
	LOG_TRACE();

	int ret = selectBackendByType(AllocatorBackendType::ALLOCATOR_GRALLOC_DMABUF)->init();
	LOG_D("DMA-BUF backend init returned %d.", ret);

	ret = selectBackendByType(AllocatorBackendType::ALLOCATOR_GRALLOC_GBM)->init();
	LOG_D("Gralloc GBM backend init returned %d.", ret);

	return ret;
}

ndk::ScopedAStatus GrallocGenericAllocatorV2::allocate(const std::vector<uint8_t>& encodedDescriptor, int32_t count,
						       allocator::AllocationResult* outResult)
{
	LOG_TRACE();
	LOG_V("count=%d, encoded descriptor size=%zu.", count, encodedDescriptor.size());

	BufferDescriptorInfoV4 mapperV4Descriptor;
	int ret = ::android::gralloc4::decodeBufferDescriptorInfo(encodedDescriptor, &mapperV4Descriptor);
	if (ret) {
		LOG_E("Cannot decode the buffer descriptor, ret=%d.", ret);
		return ToBinderStatus(AllocationError::BAD_DESCRIPTOR);
	}

	LOG_D("count=%d, desc {w=%u, h=%u, layers=%u, format=%d, usage=0x%llx}.",
	      count,
	      static_cast<uint32_t>(mapperV4Descriptor.width),
	      static_cast<uint32_t>(mapperV4Descriptor.height),
	      static_cast<uint32_t>(mapperV4Descriptor.layerCount),
	      static_cast<int32_t>(mapperV4Descriptor.format),
	      static_cast<unsigned long long>(mapperV4Descriptor.usage));

	const BufferDescriptorInfo info = {
		.name = {"auto_generated"},
		.width = static_cast<int32_t>(mapperV4Descriptor.width),
		.height = static_cast<int32_t>(mapperV4Descriptor.height),
		.layerCount = static_cast<int32_t>(mapperV4Descriptor.layerCount),
		.format = (PixelFormat) mapperV4Descriptor.format,
		.usage = (BufferUsage) mapperV4Descriptor.usage,
		.reservedSize = static_cast<int64_t>(mapperV4Descriptor.reservedSize),
	};

	return allocate2(info, count, outResult);
}

ndk::ScopedAStatus GrallocGenericAllocatorV2::allocate2(const BufferDescriptorInfo& descriptor, int32_t count,
							allocator::AllocationResult* outResult)
{
	LOG_TRACE();
	LOG_V("count=%d, desc: %s.", count, descriptor.toString().c_str());

	selectBackend(descriptor);
	CHECK_BACKEND();

	return mBackend->allocate2(descriptor, count, outResult);
}

ndk::ScopedAStatus GrallocGenericAllocatorV2::isSupported(const BufferDescriptorInfo& descriptor,
							  bool* outResult)
{
	LOG_TRACE();
	LOG_V("Descriptor: %s.", descriptor.toString().c_str());

	/* Deny all non-standard metadata options */
	for (const auto& option : descriptor.additionalOptions) {
		if (option.name != STANDARD_METADATA_DATASPACE) {
			LOG_D("Rejecting non-standard metadata option '%s'.", option.name.c_str());
			*outResult = false;
			return ndk::ScopedAStatus::ok();
		}
	}

	selectBackend(descriptor);
	CHECK_BACKEND();

	ndk::ScopedAStatus status = mBackend->isSupported(descriptor, outResult);
	LOG_V("result=%d.", *outResult);
	return status;
}

ndk::ScopedAStatus GrallocGenericAllocatorV2::getIMapperLibrarySuffix(std::string* outResult)
{
	LOG_TRACE();
	*outResult = "generic";
	LOG_V("Returning IMapper library suffix: '%s'.", outResult->c_str());
	return ndk::ScopedAStatus::ok();
}

::ndk::SpAIBinder GrallocGenericAllocatorV2::createBinder()
{
	auto binder = BnAllocator::createBinder();
	AIBinder_setInheritRt(binder.get(), true);
	return binder;
}

}  // namespace aidl::android::hardware::graphics::allocator::impl