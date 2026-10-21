/*
 * SPDX-FileCopyrightText: Copyright 2026 Valentin Liu (LIU, YUANCHEN) <valentinliu@icloud.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef GRALLOC_GBM_H
#define GRALLOC_GBM_H

#include <gbm.h>
#include <gralloc_generic.h>
#include <gralloc_gbm_bo_ext.h>
#include <hardware/gralloc.h>
#include <uthash/uthash.h>

#define GRALLOC_GBM_WIDTH_MAX 16384
#define GRALLOC_GBM_HEIGHT_MAX 16384
/**
 * Supported macros:
 *
 * GRALLOC_GBM_USE_SW_RENDER
 * GRALLOC_GBM_NO_CURSOR_RENDERING - 
 *     Remove GBM_BO_USE_CURSOR flag if there are both GBM_BO_USE_CURSOR and GBM_BO_USE_RENDERING in flags
 * GRALLOC_GBM_NO_CURSOR_ALIGNING -
 *     Disable aligning width and height with 4 for cursor.
 */

#ifndef __nullable
#define __nullable
#endif

#ifndef DIV_ROUND_UP
#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d)) 
#endif

#ifdef __cplusplus
extern "C" {
#endif


typedef struct gralloc_gbm_android_buffer {
	int fd; /* key */
	void *bo; /* value */
	UT_hash_handle hh;
} gralloc_gbm_android_buffer_t, android_buffer_t;

/* Gralloc GBM android_buffer_t functions */

android_buffer_t *find_buffer_k(const int fd);
android_buffer_t *find_buffer_v(const void *v);
int add_buffer(android_buffer_t *buffer);
int delete_buffer(android_buffer_t *buffer);
int delete_buffer_fd(const int fd);

typedef struct gralloc_gbm_driver {
	/* Is driver ready to work */
	bool ready;
	int dri_fd;
	struct gbm_device *gbm_dev;
	int gbm_dev_fd;
	const char *gbm_backend_name;
} gralloc_gbm_driver_t, driver_t;

/* Gralloc GBM driver functions */

bool is_gralloc_gbm_ready(void);
int gralloc_gbm_init(void);
int gralloc_gbm_deinit(void);
/**
 * Return the initialized driver instance or nullptr if the driver is uninitialized. 
 */
const gralloc_gbm_driver_t *gralloc_gbm_get(void);
void gralloc_gbm_bo_data_destroy(struct gbm_bo *bo, void *data);

// GBM compatible metadata
typedef struct gralloc_gbm_android_buffer_info {
	int fd;
	void *buffer_id;
	const char *name;
	int32_t width;
	int32_t height;
	int32_t layer_count;
	int32_t plane_count;
	uint32_t android_format;
	uint32_t gbm_format;
	uint64_t modifier;
	int32_t usage;
	uint64_t size;
	int32_t stride;
	bool is_protected;

	buffer_metadata_t metadata;
} gralloc_gbm_android_buffer_info_t, android_buffer_info_t;

/* Gralloc GBM Android platform functions */
int32_t gralloc_gbm_calculate_android_pixel_stride(const int32_t android_format, const uint32_t gbm_stride /*pitch*/);
uint_t gralloc_gbm_calculate_gbm_flags(const int usage, const int gbm_format);
bool gralloc_gbm_is_allocator_desc_supported(const allocator_desc_t *desc);
int gralloc_gbm_android_buffer_new(allocator_desc_t *desc, __nullable uint32_t *out_stride, native_handle_t **out_buffer_handle);
int gralloc_gbm_android_buffer_import(const buffer_handle_t handle);
int gralloc_gbm_android_buffer_free(const buffer_handle_t handle);
int gralloc_gbm_android_buffer_lock(const buffer_handle_t handle, int usage,
				    const int top, const int bottom, const int left, const int right,
				    void **out);
int gralloc_gbm_android_buffer_unlock(const buffer_handle_t handle);
int gralloc_gbm_android_buffer_query(const buffer_handle_t handle, android_buffer_info_t *out);
gbm_bo_data_t *gralloc_gbm_android_buffer_get_extdata(const buffer_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* GRALLOC_GBM_H */