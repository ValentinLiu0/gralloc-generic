/*
 * SPDX-FileCopyrightText: Copyright 2026 Valentin Liu (LIU, YUANCHEN) <valentinliu@icloud.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <drm/android/gralloc_handle.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <gralloc_generic_format.h>
#include <gralloc_gbm.h>

#define LOG_TAG "gralloc_gbm"
#include <log.h>

/**
 * Hash table to store managed (allocated/imported) android buffer.
 */
static android_buffer_t *android_buffer_table = NULL;

/* Gralloc GBM android_buffer_t functions */

android_buffer_t *find_buffer_k(const int fd)
{
	LOG_TRACE();

	android_buffer_t *entry;
	HASH_FIND_INT(android_buffer_table, &fd, entry);
	LOG_V("fd=%d -> %p", fd, entry);

	return entry;
}

android_buffer_t *find_buffer_v(const void *v)
{
	LOG_TRACE();
	if (!v) {
		LOG_E("Invalid v: %p, return nullptr", v);
		return nullptr;
	}

	android_buffer_t *entry, *tmp;
	LOG_V("buffer table count: %d", HASH_COUNT(android_buffer_table));
	HASH_ITER(hh, android_buffer_table, entry, tmp) {
		if (entry->bo == v) {
			LOG_V("bo=%p -> {fd=%d, buffer=%p}", v, entry->fd, entry);
			return entry;
		}
	}

	LOG_V("bo=%p -> no match", v);
	return nullptr;
}

int add_buffer(android_buffer_t *buffer)
{
	LOG_TRACE();
	if (!buffer) {
		LOG_E("invalid buffer.");
		return -EINVAL;
	}

	if (find_buffer_k(buffer->fd))
		LOG_W("fd=%d already exists in the buffer table, adding a duplicate entry.",
		      buffer->fd);

	if (!buffer->bo)
		LOG_W("buffer with fd=%d has an invalid BO.", buffer->fd);

	HASH_ADD_INT(android_buffer_table, fd, buffer);

	android_buffer_t *tmp;
	HASH_FIND_INT(android_buffer_table, &buffer->fd, tmp);
	if (!tmp) {
		LOG_E("fd=%d was not found after insertion.", buffer->fd);
		return -EINVAL;
	}
	assert(buffer->fd == tmp->fd);
	LOG_V("fd=%d, bo=%p, buffer=%p", buffer->fd, buffer->bo, buffer);

	return 0;
}

int delete_buffer(android_buffer_t *buffer)
{
	LOG_TRACE();
	if (!buffer) {
		LOG_E("invalid buffer.");
		return -EINVAL;
	}

	LOG_V("fd=%d, bo=%p, buffer=%p", buffer->fd, buffer->bo, buffer);
	HASH_DEL(android_buffer_table, buffer);
	free(buffer);

	return 0;
}

int delete_buffer_fd(const int fd)
{
	LOG_TRACE();
	if (fd < 0) {
		LOG_E("invalid fd=%d.", fd);
		return -EINVAL;
	}

	android_buffer_t *tmp = NULL;
	HASH_FIND_INT(android_buffer_table, &fd, tmp);
	if (tmp) {
		LOG_V("deleting fd=%d, buffer=%p", fd, tmp);
		HASH_DEL(android_buffer_table, tmp);
		free(tmp);
	} else {
		LOG_D("no fd=%d buffer found, skip deleting.", fd);
	}

	return 0;
}

static gralloc_gbm_driver_t *g_driver;

/* Gralloc GBM driver functions */

bool is_gralloc_gbm_ready(void)
{
	if (g_driver && g_driver->ready)
		return true;

	return false;
}

int gralloc_gbm_init(void)
{
	LOG_TRACE();
	if (g_driver && g_driver->ready) {
		LOG_V("Gralloc GBM driver is already initialized.");
		return 0;
	}

	g_driver = calloc(1, sizeof(*g_driver));
	if (!g_driver) {
		LOG_E("no memory for the driver.");
		return -ENOMEM;
	}

	g_driver->dri_fd = open("/dev/dri/renderD128", O_RDWR | O_CLOEXEC);
	if (g_driver->dri_fd < 0) {
		LOG_E("cannot open /dev/dri/renderD128: %s", strerror(errno));
		free(g_driver);
		g_driver = NULL;
		return -ENODEV;
	}

	g_driver->gbm_dev = gbm_create_device(g_driver->dri_fd);
	if (!g_driver->gbm_dev) {
		LOG_E("gbm_create_device() failed for fd=%d.", g_driver->dri_fd);
		close(g_driver->dri_fd);
		free(g_driver);
		g_driver = NULL;
		return -EINVAL;
	}

	g_driver->gbm_dev_fd = gbm_device_get_fd(g_driver->gbm_dev);
	g_driver->gbm_backend_name = gbm_device_get_backend_name(g_driver->gbm_dev);
	g_driver->ready = true;

	LOG_I("Initialized Gralloc GBM driver (backend: %s, dri_fd=%d, gbm_dev_fd=%d).",
	      g_driver->gbm_backend_name, g_driver->dri_fd, g_driver->gbm_dev_fd);

	return 0;
}

int gralloc_gbm_deinit(void)
{
	LOG_TRACE();
	if (!g_driver) {
		LOG_W("Gralloc GBM driver is not initialized, nothing to do.");
		return 0;
	}
	if (!g_driver->ready)
		LOG_W("Gralloc GBM driver is not ready, destroying it anyway.");

	LOG_I("Deinitializing Gralloc GBM driver (backend: %s).", g_driver->gbm_backend_name);
	gbm_device_destroy(g_driver->gbm_dev);

	free(g_driver);
	g_driver = NULL;

	return 0;
}

// Callback for gbm_bo_set_user_data()
void gralloc_gbm_bo_data_destroy(struct gbm_bo *bo, void *data) {
	LOG_TRACE();
	gbm_bo_data_t *bo_data = (gbm_bo_data_t *) data;
	LOG_V("Destroying BO data %p for BO %p.", bo_data, bo);
	free(bo_data->metadata.smpte2086);
	free(bo_data->metadata.cta861_3);
	free(bo_data);
}

const gralloc_gbm_driver_t *gralloc_gbm_get(void)
{
	LOG_TRACE();
	if (!is_gralloc_gbm_ready())
		return nullptr;

	return g_driver;
}

gbm_bo_data_t *gralloc_gbm_bo_user_data_init(struct gbm_bo *bo)
{
	LOG_TRACE();
	if (!bo) {
		LOG_E("invalid BO.");
		return nullptr;
	}

	gbm_bo_data_t *bo_data = (gbm_bo_data_t *) gbm_bo_get_user_data(bo);
	if (!bo_data) {
		LOG_V("no user data found in BO (%p), set a new one.", bo);
		bo_data = calloc(1, sizeof(gbm_bo_data_t));
		if (!bo_data) {
			LOG_E("no memory for BO data.");
			return nullptr;
		}
		bo_data->metadata.smpte2086 = calloc(1, sizeof(smpte2086_t));
		bo_data->metadata.cta861_3 = calloc(1, sizeof(cta861_3_t));
		bo_data->metadata.smpte2094_50 = calloc(GRALLOC_GENERIC_SMPTE2094_50_MAX_SIZE, sizeof(uint8_t));
		if (!bo_data->metadata.smpte2086 || !bo_data->metadata.cta861_3 ||
		    !bo_data->metadata.smpte2094_50) {
			LOG_E("no memory for metadata (BO=%p).", bo);
			free(bo_data->metadata.smpte2086);
			free(bo_data->metadata.cta861_3);
			free(bo_data->metadata.smpte2094_50);
			free(bo_data);
			return nullptr;
		}
		bo_data->metadata.smpte2094_50_size = 0;
		gbm_bo_set_user_data(bo, bo_data, gralloc_gbm_bo_data_destroy);
		LOG_V("Created BO data %p for BO %p.", bo_data, bo);
	}

	return bo_data;
}

/* Gralloc GBM Buffer Object functions */

static int gralloc_gbm_bo_map(struct gbm_bo *bo, int enable_write, void **addr)
{
	LOG_TRACE();
	if (!bo) {
		LOG_E("invalid BO.");
		return -EINVAL;
	}

	int flags = GBM_BO_TRANSFER_READ;
	gbm_bo_data_t *bo_data = (gbm_bo_data_t *) gbm_bo_get_user_data(bo);
	uint32_t stride;

	if (!bo_data) {
		LOG_E("no user data for BO %p.", bo);
		return -EINVAL;
	}

	if (bo_data->map_data) {
		LOG_E("BO %p is already mapped.", bo);
		return -EINVAL;
	}

	if (enable_write)
		flags |= GBM_BO_TRANSFER_WRITE;

	LOG_V("Mapping BO %p (%ux%u, write=%d).", bo,
	      gbm_bo_get_width(bo), gbm_bo_get_height(bo), enable_write);
	*addr = gbm_bo_map(bo, 0, 0, gbm_bo_get_width(bo), gbm_bo_get_height(bo),
                       flags, &stride, &bo_data->map_data);
	if (*addr == NULL) {
		LOG_E("gbm_bo_map() returned NULL for BO %p.", bo);
		return -ENOMEM;
	}

	assert(stride == gbm_bo_get_stride(bo));

	LOG_V("mapped BO (%p) at %p", bo, *addr);
	return 0;
}

static void gralloc_gbm_bo_unmap(struct gbm_bo *bo) {
	LOG_TRACE();
	if (!bo) {
		LOG_E("invalid BO.");
		return;
	}

	gbm_bo_data_t *bo_data = (gbm_bo_data_t *) gbm_bo_get_user_data(bo);
	if (!bo_data) {
		LOG_E("no user data for BO %p.", bo);
		return;
	}

	gbm_bo_unmap(bo, bo_data->map_data);
	bo_data->map_data = NULL;
	LOG_V("unmapped BO (%p).", bo);
}

/* Gralloc GBM Android platform functions */

/** 
 * Defined at AllocationResult.aidl
 * Return the stride in pixels
 */
int32_t gralloc_gbm_calculate_android_pixel_stride(const int32_t android_format, const uint32_t gbm_stride /*pitch*/)
{
	LOG_TRACE();
	uint32_t gbm_format = color_fmt_a2g(android_format);
	if (gbm_format <= 0) {
		LOG_E("unsupported Android format %d.",
		      android_format);
		return -EINVAL;
	}

	int32_t pixel_stride = gbm_stride / color_bytes_per_pixel(gbm_format);
	LOG_V("format=%d, gbm_stride=%u -> %d.",
	      android_format, gbm_stride, pixel_stride);
	return pixel_stride;
}

uint_t gralloc_gbm_calculate_gbm_flags(const int usage, const int gbm_format)
{
	LOG_TRACE();
	uint_t flags = GBM_BO_USE_NONE;

	if (usage & GRALLOC_USAGE_SW_READ_OFTEN)
		flags |= GBM_BO_USE_LINEAR;
	if (usage & GRALLOC_USAGE_SW_READ_RARELY)
		flags |= GBM_BO_USE_LINEAR;
	if (usage & GRALLOC_USAGE_SW_WRITE_OFTEN) {
		flags |= GBM_BO_USE_LINEAR;
#ifdef GRALLOC_GBM_USE_SW_RENDER
		flags |= GBM_BO_USE_WRITE;
#endif
	}
	if (usage & GRALLOC_USAGE_SW_WRITE_RARELY) {
		flags |= GBM_BO_USE_LINEAR;
#ifdef GRALLOC_GBM_USE_SW_RENDER
		flags |= GBM_BO_USE_WRITE;
#endif
	}
	if (usage & GRALLOC_USAGE_HW_TEXTURE)
		flags |= GBM_BO_USE_RENDERING; // TODO: Check this.
	if (usage & GRALLOC_USAGE_HW_RENDER)
		flags |= GBM_BO_USE_RENDERING;
	if (usage & GRALLOC_USAGE_HW_2D)
		flags |= GBM_BO_USE_RENDERING;
	if (usage & GRALLOC_USAGE_HW_COMPOSER)
		flags |= GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING;
	if (usage & GRALLOC_USAGE_HW_FB)
		flags |= GBM_BO_USE_SCANOUT;
	if (usage & GRALLOC_USAGE_EXTERNAL_DISP)
		flags |= GBM_BO_USE_NONE;
	if (usage & GRALLOC_USAGE_PROTECTED)
		flags |= GBM_BO_USE_PROTECTED;
	if (usage & GRALLOC_USAGE_CURSOR)
#if (ANDROID_API_LEVEL >= 35) /* Android 15 */
		flags |= GBM_BO_USE_CURSOR;
#else
		;
#endif
	//if (usage & GRALLOC_USAGE_RENDERSCRIPT)
	//	flags |= GBM_BO_USE_FRONT_RENDERING;

	/* Only warning here, check flags in function gralloc_gbm_is_allocator_desc_supported() */
	if (flags & GBM_BO_USE_WRITE) {
		if (!!(flags & GBM_BO_USE_CURSOR) && gbm_format != GBM_FORMAT_ARGB8888)
			return flags;
		
		if (!!(flags & GBM_BO_USE_SCANOUT) && (gbm_format != GBM_FORMAT_XRGB8888 && gbm_format != GBM_FORMAT_XBGR8888))
			return flags;
	}
	if ((flags & GBM_BO_USE_CURSOR) && (flags & GBM_BO_USE_RENDERING)) {
#ifdef GRALLOC_GBM_NO_CURSOR_RENDERING
		flags &= ~GBM_BO_USE_CURSOR;
#endif
	}

	if (flags == GBM_BO_USE_NONE)
		LOG_D("No GBM flag matched for usage=0x%x.", (uint32_t) usage);

	LOG_V("usage=0x%x, gbm_format=0x%x -> flags=0x%x.",
	      (uint32_t) usage, (uint32_t) gbm_format, flags);

	return flags;
}

bool gralloc_gbm_is_allocator_desc_supported(const allocator_desc_t *desc)
{
	LOG_TRACE();
	if (!is_gralloc_gbm_ready()) {
		LOG_D("Gralloc GBM driver is not ready, descriptor is unsupported.");
		return false;
	}

	if (!desc) {
		LOG_E("invalid descriptor.");
		return false;
	}

	LOG_V("Descriptor: %s", allocator_desc_to_string(desc));

	uint32_t gbm_format = color_fmt_a2g(desc->format);
	if (gbm_format <= 0) {
		LOG_D("Unsupported Android color format: %d.", desc->format);
		return false;
	}

	if ((desc->width > GRALLOC_GBM_WIDTH_MAX) || (desc->height > GRALLOC_GBM_HEIGHT_MAX)) {
		LOG_D("Buffer size %dx%d exceeds the maximum supported size (%dx%d).",
		      desc->width, desc->height, GRALLOC_GBM_WIDTH_MAX, GRALLOC_GBM_HEIGHT_MAX);
		return false;
	}

	if (!desc->usage) {
		LOG_D("Descriptor has zero usage flags.");
		return false;
	}

	uint32_t flags = gralloc_gbm_calculate_gbm_flags(desc->usage, gbm_format);

	/**
	 * GBM DRI backend will call the function create_dumb() if GBM_BO_USE_WRITE is presented.
	 *
	 * A part of source code in 'gbm_dri.c' of function create_dumb():
	 * is_cursor = (usage & GBM_BO_USE_CURSOR) != 0 && format == GBM_FORMAT_ARGB8888;
	 * is_scanout = (usage & GBM_BO_USE_SCANOUT) != 0 && (format == GBM_FORMAT_XRGB8888 || format == GBM_FORMAT_XBGR8888);
	 * if (!is_cursor && !is_scanout) {
	 * 	errno = EINVAL;
	 * 	return NULL;
	 * }
	 */
	if (flags & GBM_BO_USE_WRITE) {
		LOG_D("The GBM_BO_USE_WRITE flag is set, the DRI backend will create a dumb buffer.");
		if (!!(flags & GBM_BO_USE_CURSOR) && gbm_format != GBM_FORMAT_ARGB8888) {
			LOG_E("Using unsupported color format (%s) with GBM_BO_USE_CURSOR flag.",
			      color_fmt_getname(gbm_format, true));
			return false;
		}
		
		if (!!(flags & GBM_BO_USE_SCANOUT) && (gbm_format != GBM_FORMAT_XRGB8888 && gbm_format != GBM_FORMAT_XBGR8888)) {
			LOG_E("Using unsupported color format (%s) with GBM_BO_USE_SCANOUT flag.",
			      color_fmt_getname(gbm_format, true));
			return false;
		}
	}
	/**
	 * GBM DRI backend didn't support the flags exist both GBM_BO_USE_CURSOR and GBM_BO_USE_RENDERING
	 *
	 * A part of source code in 'gbm_dri.c' of function gbm_dri_is_format_supported():
	 * if ((usage & GBM_BO_USE_CURSOR) && (usage & GBM_BO_USE_RENDERING))
	 *    return 0;
	 */
	if ((flags & GBM_BO_USE_CURSOR) && (flags & GBM_BO_USE_RENDERING))
#ifndef GRALLOC_GBM_NO_CURSOR_RENDERING
	{
		LOG_D("Both GBM_BO_USE_CURSOR and GBM_BO_USE_RENDERING are set, unsupported by the DRI backend.");
		return false;
	}
#else
		LOG_W("Both GBM_BO_USE_CURSOR and GBM_BO_USE_RENDERING are set, dropping GBM_BO_USE_CURSOR.");
#endif

	if (!gbm_device_is_format_supported(g_driver->gbm_dev, gbm_format, flags)) {
		LOG_D("gbm_device_is_format_supported() rejected fmt=%s, flags=0x%x.",
		      color_fmt_getname(gbm_format, true), flags);
		return false;
	}

	LOG_V("Supported: fmt=%s, flags=0x%x.",
	      color_fmt_getname(gbm_format, true), flags);
	return true;
}

int gralloc_gbm_android_buffer_new(allocator_desc_t *desc, __nullable uint32_t *out_stride, native_handle_t **out_buffer_handle)
{
	LOG_TRACE();
	if (!is_gralloc_gbm_ready()) {
		LOG_E("Gralloc GBM driver is not ready.");
		return -ENODEV;
	}

	if (!desc) {
		LOG_E("invalid descriptor.");
		return -EINVAL;
	}

	LOG_D("new buffer object allocating request: %s", allocator_desc_to_string(desc));

	uint32_t gbm_format = color_fmt_a2g(desc->format);
	if (gbm_format <= 0) {
		LOG_E("Unsupported Android color format: %d", desc->format);
		return -EINVAL;
	}

	uint_t flags = gralloc_gbm_calculate_gbm_flags(desc->usage, gbm_format);

	int32_t width = desc->width;
	int32_t height = desc->height;
	if (flags & GBM_BO_USE_CURSOR) {
		width = MAX(desc->width, 64);
		height = MAX(desc->height, 64);
#ifndef GRALLOC_GBM_NO_CURSOR_ALIGNING
		width = ALIGN(width, 4);
		height = ALIGN(height, 4);
#endif
		LOG_V("Aligned size for cursor from %dx%d to %dx%d.", desc->width, desc->height, width, height);
	}
	/*
	 * For YV12, we request GR88, so halve the width since we're getting
	 * 16bpp. Then increase the height by 1.5 for the U and V planes.
	 */
	if (desc->format == HAL_PIXEL_FORMAT_YV12) {
		width = ALIGN(desc->width, 32) / 2;
		height += ALIGN(desc->height, 2) / 2;
		LOG_D("width and height changed to %dx%d for HAL_PIXEL_FORMAT_YV12.", width, height);
	}

	native_handle_t *handle = gralloc_handle_create(width, height, desc->format, desc->usage);
	if (!handle) {
		LOG_E("Failed to create gralloc handle (%dx%d), hal fmt=%s, usage=0x%llx.",
		      width, height, color_fmt_getname(desc->format, false),
		      (unsigned long long) desc->usage);
		return -EINVAL;
	}

	struct gbm_bo *bo = gbm_bo_create(g_driver->gbm_dev, width, height, gbm_format, flags);
	if (!bo) {
		LOG_E("Failed to create BO (%dx%d), fmt=%s, flags=0x%x, usage=0x%llx.",
		      width, height, color_fmt_getname(gbm_format, true),
		      flags, (unsigned long long) desc->usage);
		return -EINVAL;
	}

	if(!gralloc_gbm_bo_user_data_init(bo)) {
		LOG_E("Failed to initialize user data for BO %p (handle=%p).", bo, handle);
		return -EINVAL;
	}

	struct gralloc_handle_t *ghandle = gralloc_handle(handle);
	if (!ghandle) {
		LOG_E("Failed to get the gralloc handle from %p.", handle);
		return -EINVAL;
	}
	ghandle->stride = gbm_bo_get_stride(bo);
	ghandle->prime_fd = gbm_bo_get_fd(bo);
	ghandle->modifier = gbm_bo_get_modifier(bo);

	android_buffer_t *buffer = calloc(1, sizeof(android_buffer_t));
	if (!buffer) {
		LOG_E("Failed to allocate the buffer table entry for fd=%d.", ghandle->prime_fd);
		return -ENOMEM;
	}

	buffer->fd = ghandle->prime_fd;
	buffer->bo = bo;
	if (add_buffer(buffer)) {
		LOG_E("Failed to add buffer (handle=%p, fd=%d) to the table.", handle, buffer->fd);
		free(buffer);
		return -EINVAL;
	}

	*out_buffer_handle = handle;
	if (out_stride)
		*out_stride = ghandle->stride;

	LOG_D("new buffer object (bo=%p, handle=%p, fd=%d): %dx%d, fmt=%s, flags=0x%x, stride=%u, modifier=0x%llx",
	      bo, handle, ghandle->prime_fd, width, height, color_fmt_getname(gbm_format, true),
	      flags, ghandle->stride, (unsigned long long) ghandle->modifier);
	return 0;
}

int gralloc_gbm_android_buffer_import(const buffer_handle_t handle)
{
	LOG_TRACE();
	if (!is_gralloc_gbm_ready()) {
		LOG_E("Gralloc GBM driver is not ready.");
		return -ENODEV;
	}

	if (!handle) {
		LOG_E("invalid handle.");
		return -EINVAL;
	}

	const struct gralloc_handle_t *ghandle = gralloc_handle(handle);
	LOG_D("new %ux%u buffer importing request: handle=%p, fd=%d",
	      ghandle->width, ghandle->height, handle, ghandle->prime_fd);

	struct gbm_bo *bo;
	struct gbm_import_fd_modifier_data *fd_data = calloc(1, sizeof(struct gbm_import_fd_modifier_data));
	if (!fd_data) {
		LOG_E("no memory for import data.");
		return -ENOMEM;
	}

	fd_data->format = color_fmt_a2g(ghandle->format);
	if (fd_data->format <= 0) {
		LOG_E("Trying to import buffer with unsupported Android color format (%d), abort.",
		      ghandle->format);
		free(fd_data);
		return -EINVAL;
	}

	fd_data->width = ghandle->width;
	fd_data->height = ghandle->height;
	/* Adjust the width and height for a GBM GR88 buffer */
	if (ghandle->format == HAL_PIXEL_FORMAT_YV12) {
		fd_data->width = ALIGN(ghandle->width, 32) / 2;
		fd_data->height = ghandle->height + ALIGN(ghandle->height, 2) / 2;
	}

	uint32_t flags = gralloc_gbm_calculate_gbm_flags(ghandle->usage, fd_data->format);

	/* Some GPUs require 64 pixels at least for cursor */
	if (flags & GBM_BO_USE_CURSOR) {
		fd_data->width = MAX(ghandle->width, 64);
		fd_data->height = MAX(ghandle->height, 64);
#ifndef GRALLOC_GBM_NO_CURSOR_ALIGNING
		fd_data->width = ALIGN(fd_data->width, 4);
		fd_data->height = ALIGN(fd_data->height, 4);
#endif
		LOG_V("Aligned size for cursor from %dx%d to %dx%d.", ghandle->width, ghandle->height, fd_data->width, fd_data->height);
	}

	fd_data->modifier = ghandle->modifier;
	fd_data->num_fds = 1;
	fd_data->fds[0] = ghandle->prime_fd;
	fd_data->strides[0] = ghandle->stride;

	LOG_D("call gbm_bo_import with params {w: %u, h: %u, fmt: 0x%x, s0: %u, f0: %d, modifier: 0x%llx, flags: 0x%x}",
	      fd_data->width, fd_data->height, fd_data->format, fd_data->strides[0],
	      fd_data->fds[0], (unsigned long long) fd_data->modifier, flags);
	bo = gbm_bo_import(g_driver->gbm_dev, GBM_BO_IMPORT_FD_MODIFIER, fd_data, flags);

	if (!bo) {
		LOG_E("Failed to import buffer object (handle=%p, %ux%u), fmt=%s, flags=0x%x.",
		      handle, fd_data->width, fd_data->height,
		      color_fmt_getname(fd_data->format, true), flags);
		free(fd_data);
		return -EINVAL;
	}

	if (!gralloc_gbm_bo_user_data_init(bo)) {
		LOG_E("Failed to initialize user data for imported BO %p (handle=%p).", bo, handle);
		gbm_bo_destroy(bo);
		free(fd_data);
		return -EINVAL;
	}

	android_buffer_t *buffer = (android_buffer_t *) malloc(sizeof(android_buffer_t));
	if (!buffer) {
		LOG_E("Failed to allocate the buffer table entry for imported handle %p.", handle);
		gbm_bo_destroy(bo);
		free(fd_data);
		return -ENOMEM;
	}
	buffer->fd = ghandle->prime_fd;
	buffer->bo = bo;
	if (add_buffer(buffer)) {
		LOG_E("Failed to add imported buffer (handle=%p, fd=%d) to the table.",
		      handle, buffer->fd);
		free(buffer);
		gbm_bo_destroy(bo);
		free(fd_data);
		return -EINVAL;
	}

	LOG_D("imported buffer object (bo=%p, handle=%p, fd=%d): %ux%u, fmt=%s, flags=0x%x, stride=%u",
	      bo, handle, fd_data->fds[0], fd_data->width, fd_data->height,
	      color_fmt_getname(fd_data->format, true), flags, fd_data->strides[0]);
	free(fd_data);
	return 0;
}

int gralloc_gbm_android_buffer_free(const buffer_handle_t handle)
{
	LOG_TRACE();
	if (!handle) {
		LOG_E("invalid handle.");
		return -EINVAL;
	}

	struct gralloc_handle_t *ghandle = gralloc_handle(handle);
	if (!ghandle) {
		LOG_E("cannot get the gralloc handle.");
		return -EINVAL;
	}

	if (!is_gralloc_gbm_ready()) {
		LOG_E("Gralloc GBM driver is not ready.");
		return -ENODEV;
	}

	LOG_V("handle=%p, fd=%d.", handle, ghandle->prime_fd);

	android_buffer_t *buffer = find_buffer_k(ghandle->prime_fd);
	if (buffer) {
		struct gbm_bo *bo = (struct gbm_bo *) buffer->bo;
		gbm_bo_destroy(bo);
		LOG_V("freeing buffer (handle=%p, buffer=%p, fd=%d)",
		      handle, buffer, ghandle->prime_fd);
		delete_buffer(buffer);
	} else {
		LOG_D("Not a managed buffer (handle=%p, fd=%d), nothing to free.",
		      handle, ghandle->prime_fd);
	}

	return 0;
}

int gralloc_gbm_android_buffer_lock(const buffer_handle_t handle, int usage,
				    const int top, const int bottom, const int left, const int right,
				    void **out)
{
	LOG_TRACE();
	if (!is_gralloc_gbm_ready()) {
		LOG_E("Gralloc GBM driver is not ready.");
		return -ENODEV;
	}

	if (!handle) {
		LOG_E("invalid handle.");
		return -EINVAL;
	}

	/* Framebuffer must NOT be locked */
	if (usage & GRALLOC_USAGE_HW_FB) {
		LOG_W("The framebuffer shouldn't be locked!");
		return -EINVAL;
	}

	const int width = right - left;
	const int height = bottom - top;
	const struct gralloc_handle_t *ghandle = (const struct gralloc_handle_t *) (uintptr_t)handle;
	LOG_V("handle=%p, fd=%d, usage=0x%x, region=[%d,%d,%d,%d].",
	      handle, ghandle->prime_fd, (uint32_t) usage, left, top, right, bottom);

	/* Protect the GPU FB/TEXTURE memory from accessing by CPU side */
	if ((ghandle->usage & usage) != (uint32_t)usage) {
		/* make FB special for testing software renderer with */
		if (!(ghandle->usage & (GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN)) &&
		    !(ghandle->usage & GRALLOC_USAGE_HW_FB) &&
		    !(ghandle->usage & GRALLOC_USAGE_HW_TEXTURE)) {
			LOG_E("Unsupported buffer usage: buffer=0x%x, requested=0x%x!",
			      ghandle->usage, (uint32_t) usage);
			return -EINVAL;
		}
	}

	android_buffer_t *buffer = find_buffer_k(ghandle->prime_fd);
	if (!buffer) {
		LOG_D("Not a managed buffer (handle=%p, fd=%d).", handle, ghandle->prime_fd);
		return -EINVAL;
	}

	struct gbm_bo *bo = (struct gbm_bo *) buffer->bo;
	if (!bo) {
		LOG_E("buffer (handle=%p, fd=%d) has no BO.", handle, ghandle->prime_fd);
		return -EINVAL;
	}

	gbm_bo_data_t *bo_data = gralloc_gbm_bo_user_data_init(bo);
	if (!bo_data) {
		LOG_E("no user data for BO %p (handle=%p).", bo, handle);
		return -EINVAL;
	}

	/* allow multiple locks with compatible usages */
	if (bo_data->lock_count && (bo_data->locked_for & usage) != usage) {
		LOG_D("BO %p is already locked for usage=0x%x, requested=0x%x.",
		      bo, (uint32_t) bo_data->locked_for, (uint32_t) usage);
		return -EINVAL;
	}

	usage |= bo_data->locked_for;

	/* Map CPU accessible BO */
	if (usage & (GRALLOC_USAGE_SW_WRITE_MASK | GRALLOC_USAGE_SW_READ_MASK)) {
		/* the driver is supposed to wait for the bo */
		int write = !!(usage & GRALLOC_USAGE_SW_WRITE_MASK);
		int err = gralloc_gbm_bo_map(bo, write, out);
		if (err)
			return err;
	} else {
	    /* kernel handles the synchronization here */
	}

	bo_data->lock_count++;
	bo_data->locked_for |= usage;

	LOG_V("handle=%p, lock_count=%d, locked_for=0x%x.",
	      handle, bo_data->lock_count, (uint32_t) bo_data->locked_for);
	return 0;
}

int gralloc_gbm_android_buffer_unlock(const buffer_handle_t handle)
{
	LOG_TRACE();
	if (!handle) {
		LOG_E("invalid handle.");
		return -EINVAL;
	}

	struct gralloc_handle_t *ghandle = gralloc_handle(handle);
	if (!ghandle) {
		LOG_E("cannot get the gralloc handle.");
		return -EINVAL;
	}

	LOG_V("handle=%p, fd=%d.", handle, ghandle->prime_fd);

	android_buffer_t *buffer = find_buffer_k(ghandle->prime_fd);
	if (!buffer) {
		LOG_D("Not a managed buffer (handle=%p, fd=%d).", handle, ghandle->prime_fd);
		return -EINVAL;
	}

	struct gbm_bo *bo = (struct gbm_bo *) buffer->bo;
	if (!bo) {
		LOG_E("buffer (handle=%p, fd=%d) has no BO.",
		      handle, ghandle->prime_fd);
		return -EINVAL;
	}

	gbm_bo_data_t *bo_data;
	bo_data = (gbm_bo_data_t *)gbm_bo_get_user_data(bo);
	if (!bo_data) {
		LOG_E("no user data for BO %p (handle=%p).", bo, handle);
		return -EINVAL;
	}

	int mapped = bo_data->locked_for & (GRALLOC_USAGE_SW_WRITE_MASK | GRALLOC_USAGE_SW_READ_MASK);

	if (!bo_data->lock_count) {
		LOG_W("Trying to unlock an already unlocked BO, nothing to do.");
		return 0;
	}

	if (mapped)
		gralloc_gbm_bo_unmap(bo);

	bo_data->lock_count--;
	if (!bo_data->lock_count)
		bo_data->locked_for = 0;

	LOG_V("handle=%p, lock_count=%d, locked_for=0x%x.",
	      handle, bo_data->lock_count, (uint32_t) bo_data->locked_for);
	return 0;
}

int gralloc_gbm_android_buffer_query(const buffer_handle_t handle, android_buffer_info_t *out)
{
	LOG_TRACE();
	if (!handle) {
		LOG_E("invalid handle.");
		return -EINVAL;
	}
	if (!out) {
		LOG_E("invalid output pointer.");
		return -EINVAL;
	}

	struct gralloc_handle_t *ghandle = gralloc_handle(handle);
	if (!ghandle) {
		LOG_E("cannot get the gralloc handle.");
		return -EINVAL;
	}

	LOG_V("handle=%p, fd=%d.", handle, ghandle->prime_fd);

	android_buffer_t *buffer = find_buffer_k(ghandle->prime_fd);
	if (!buffer) {
		LOG_D("Not a managed buffer (handle=%p, fd=%d).", handle, ghandle->prime_fd);
		return -EINVAL;
	}

	struct gbm_bo *bo = (struct gbm_bo *) buffer->bo;
	if (!bo) {
		LOG_E("buffer (handle=%p, fd=%d) has no BO.",
		      handle, ghandle->prime_fd);
		return -EINVAL;
	}

	android_buffer_info_t info = {0};
	info.fd = gbm_bo_get_fd(bo);
	info.buffer_id = buffer;
	info.width = gbm_bo_get_width(bo);
	info.height = gbm_bo_get_height(bo);
	info.layer_count = 1;
	info.plane_count = gbm_bo_get_plane_count(bo);
	info.gbm_format = gbm_bo_get_format(bo);
	info.modifier = gbm_bo_get_modifier(bo);

	gbm_bo_data_t *bo_data = gralloc_gbm_bo_user_data_init(bo);
	if (!bo_data) {
		LOG_E("no user data for BO %p (handle=%p).", bo, handle);
		return -EINVAL;
	}
	info.metadata.dataspace = bo_data->metadata.dataspace;
	info.metadata.blend_mode = bo_data->metadata.blend_mode;
	info.metadata.smpte2086 = bo_data->metadata.smpte2086;
	info.metadata.cta861_3 = bo_data->metadata.cta861_3;
	info.metadata.smpte2094_50 = bo_data->metadata.smpte2094_50;
	info.metadata.smpte2094_50_size = bo_data->metadata.smpte2094_50_size;

	info.android_format = ghandle->format;
	info.usage = ghandle->usage;
	if (info.usage & GRALLOC_USAGE_PROTECTED)
		info.is_protected = true;

	info.size = gbm_bo_get_stride(bo) * info.height;
	info.stride = gralloc_gbm_calculate_android_pixel_stride(ghandle->format, gbm_bo_get_stride(bo));

	*out = info;

	LOG_V("Result: %dx%d, gbm fmt=0x%x, hal fmt=%u, usage=0x%x, stride=%d, size=%llu.",
	      info.width, info.height, info.gbm_format, info.android_format,
	      (uint32_t) info.usage, info.stride, (unsigned long long) info.size);

	return 0;
}

gbm_bo_data_t *gralloc_gbm_android_buffer_get_extdata(const buffer_handle_t handle)
{
	LOG_TRACE();
	if (!handle) {
		LOG_E("invalid handle.");
		return nullptr;
	}

	struct gralloc_handle_t *ghandle = gralloc_handle(handle);
	if (!ghandle) {
		LOG_E("cannot get the gralloc handle.");
		return nullptr;
	}

	android_buffer_t *buffer = find_buffer_k(ghandle->prime_fd);
	if (!buffer) {
		LOG_D("Not a managed buffer (handle=%p, fd=%d).", handle, ghandle->prime_fd);
		return nullptr;
	}

	struct gbm_bo *bo = (struct gbm_bo *) buffer->bo;
	if (!bo) {
		LOG_E("buffer (handle=%p, fd=%d) has no BO.",
		      handle, ghandle->prime_fd);
		return nullptr;
	}

	gbm_bo_data_t *bo_data = gralloc_gbm_bo_user_data_init(bo);
	LOG_V("handle=%p, fd=%d -> bo_data=%p.",
	      handle, ghandle->prime_fd, bo_data);
	return bo_data;
}
