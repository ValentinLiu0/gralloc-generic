/*
 * SPDX-FileCopyrightText: Copyright 2026 Valentin Liu (LIU, YUANCHEN) <valentinliu@icloud.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MAPPER_GRALLOC_GBM_HPP
#define MAPPER_GRALLOC_GBM_HPP

#include <gralloc_gbm.h>
#include <gralloc_generic_format.h>

#include <android/hardware/graphics/mapper/IMapper.h>
#include <android/hardware/graphics/mapper/utils/IMapperMetadataTypes.h>
#include <android/hardware/graphics/mapper/utils/IMapperProvider.h>

#include "../MapperBackendImpl.hpp"

using gralloc_generic::MapperBackendImpl;

class MapperBackendGrallocGbm final : public MapperBackendImpl {
public:
	explicit MapperBackendGrallocGbm() = default;
	virtual ~MapperBackendGrallocGbm() = default;

	bool isReady() const override {
		return is_gralloc_gbm_ready();
	};
	int init() override {
		return gralloc_gbm_init();
	};

	int importBuffer(const native_handle_t* _Nonnull handle) override;
	int freeBuffer(buffer_handle_t _Nonnull buffer) override;

	int lock(buffer_handle_t _Nonnull buffer, uint64_t cpuUsage, ARect accessRegion,
		 int acquireFence, void* _Nullable* _Nonnull outData) override;
	int unlock(buffer_handle_t _Nonnull buffer, int* _Nonnull releaseFence) override;

	int flushLockedBuffer(buffer_handle_t _Nonnull buffer) override;
	int rereadLockedBuffer(buffer_handle_t _Nonnull buffer) override;

	int getStandardMetadata(buffer_handle_t _Nonnull buffer,
				int64_t standardMetadataType,
				void* _Nullable destBuffer, size_t destBufferSize) override;
	int setStandardMetadata(buffer_handle_t _Nonnull buffer,
				int64_t standardMetadataType,
				buffer_metadata_t* _Nonnull metadata) override;
};

#endif /* MAPPER_GRALLOC_GBM_HPP */