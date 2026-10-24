/*
 * SPDX-FileCopyrightText: Copyright 2026 Valentin Liu (LIU, YUANCHEN) <valentinliu@icloud.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef GRALLOC_GENERIC_H
#define GRALLOC_GENERIC_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define GRALLOC_GENERIC_SMPTE2094_50_MAX_SIZE (1024 * 10)

#ifdef __cplusplus
extern "C" {
#endif

// android::hardware::graphics::mapper::V4_0::IMapper::BufferDescriptorInfo
typedef struct gralloc_generic_allocator_buffer_descriptor {
	unsigned char *name;
	int32_t width;
	int32_t height;
	int32_t layer_count;
	/* enum android_pixel_format_t */
	int32_t format;
	int64_t usage;
	int64_t reserved_size;
	// std::vector<::aidl::android::hardware::graphics::common::ExtendableType> additionalOptions;
} gralloc_generic_allocator_buffer_descriptor_t;
/**
 * This structure is used to pass the informations required to create
 * a new buffer by a Allocator backend. This structure is shared between
 * Allocator backends and the backend libraries.
 */
typedef gralloc_generic_allocator_buffer_descriptor_t allocator_desc_t;

/**
 * Format an allocator descriptor into a human-readable string.
 *
 * The returned pointer refers to a thread-local buffer, so concurrent calls
 * from different binder threads cannot overwrite each other's result. The
 * string stays valid until the next call from the same thread.
 */
static inline const char *allocator_desc_to_string(const allocator_desc_t *desc)
{
#ifdef __cplusplus
	static thread_local char buf[256];
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
	static _Thread_local char buf[256];
#else
	static __thread char buf[256]; /* GNU extension for pre-C11 compilers */
#endif

	if (desc == NULL) {
		return "(null)";
	}

	snprintf(buf, sizeof(buf),
		 "name=%s, width=%d, height=%d, layer_count=%d, "
		 "format=%d, usage=0x%llx, reserved_size=%lld",
		 desc->name ? (const char *)desc->name : "(null)",
		 desc->width,
		 desc->height,
		 desc->layer_count,
		 desc->format,
		 (unsigned long long)desc->usage,
		 (long long)desc->reserved_size);

	return buf;
}

// Smpte2086.h
typedef struct gralloc_generic_smpte2086 {
	float primary_red_x;
	float primary_red_y;
	float primary_green_x;
	float primary_green_y;
	float primary_blue_x;
	float primary_blue_y;
	float white_point_x;
	float white_point_y;
	float max_luminance;
	float min_luminance;
} gralloc_generic_smpte2086_t, smpte2086_t;

// Cta861_3.h
typedef struct gralloc_generic_cta861_3 {
	float max_content_light_level;
	float max_frame_average_light_level;
} gralloc_generic_cta861_3_t, cta861_3_t;

typedef struct android_buffer_metadata {
	int32_t dataspace;
	int32_t blend_mode;
	smpte2086_t *smpte2086;
	cta861_3_t *cta861_3;
	int32_t smpte2094_50_size;
	uint8_t *smpte2094_50;
} buffer_metadata_t;

#ifdef __cplusplus
}
#endif

#endif /* GRALLOC_GENERIC_H */