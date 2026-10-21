/*
 * SPDX-FileCopyrightText: Copyright 2026 Valentin Liu (LIU, YUANCHEN) <valentinliu@icloud.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MAPPER_PLANE_LAYOUTS_HPP
#define MAPPER_PLANE_LAYOUTS_HPP

#include <stdlib.h>

#include <gralloc_generic_format.h>

#include <gralloctypes/Gralloc4.h>

using namespace ::aidl::android::hardware::graphics::common;

/**
 * Return the plane layout for target GBM format color.
 * All formats in color_format_map[] should be supported.
 * The additional formats in color_bpp_map[] also be allowed.
 */
inline const std::unordered_map<uint32_t, std::vector<PlaneLayout>>& GetPlaneLayoutsMap()
{
	static const auto* kPlaneLayoutsMap = new std::unordered_map<uint32_t, std::vector<PlaneLayout>>({
		{
			GBM_FORMAT_ABGR8888,
			{{
				.components = {
					{
						.type = android::gralloc4::PlaneLayoutComponentType_R,
						.offsetInBits = 0,
						.sizeInBits = 8
					},
					{
						.type = android::gralloc4::PlaneLayoutComponentType_G,
						.offsetInBits = 8,
						.sizeInBits = 8
					},
					{
						.type = android::gralloc4::PlaneLayoutComponentType_B,
						.offsetInBits = 16,
						.sizeInBits = 8
					},
					{
						.type = android::gralloc4::PlaneLayoutComponentType_A,
						.offsetInBits = 24,
						.sizeInBits = 8
					}
				},
				.sampleIncrementInBits = color_bpp(GBM_FORMAT_ABGR8888),
				.horizontalSubsampling = 1,
				.verticalSubsampling = 1,
			}},
		},
		{
			GBM_FORMAT_XBGR8888,
			{{
				.components = {
					{
						.type = android::gralloc4::PlaneLayoutComponentType_R,
						.offsetInBits = 0,
						.sizeInBits = 8
					},
					{
						.type = android::gralloc4::PlaneLayoutComponentType_G,
						.offsetInBits = 8,
						.sizeInBits = 8
					},
					{
						.type = android::gralloc4::PlaneLayoutComponentType_B,
						.offsetInBits = 16,
						.sizeInBits = 8
					}
				},
				.sampleIncrementInBits = color_bpp(GBM_FORMAT_XBGR8888),
				.horizontalSubsampling = 1,
				.verticalSubsampling = 1,
			}},
		},
		{
			GBM_FORMAT_ARGB8888,
			{{
				.components = {
					{
						.type = android::gralloc4::PlaneLayoutComponentType_B,
						.offsetInBits = 0,
						.sizeInBits = 8
					},
					{
						.type = android::gralloc4::PlaneLayoutComponentType_G,
						.offsetInBits = 8,
						.sizeInBits = 8
					},
					{
						.type = android::gralloc4::PlaneLayoutComponentType_R,
						.offsetInBits = 16,
						.sizeInBits = 8
					},
					{
						.type = android::gralloc4::PlaneLayoutComponentType_A,
						.offsetInBits = 24,
						.sizeInBits = 8
					}
				},
				.sampleIncrementInBits = color_bpp(GBM_FORMAT_ARGB8888),
				.horizontalSubsampling = 1,
				.verticalSubsampling = 1,
			}},
		},
		{
			GBM_FORMAT_BGR888,
			{{
				.components = {
					{
						.type = android::gralloc4::PlaneLayoutComponentType_R,
						.offsetInBits = 0,
						.sizeInBits = 8
					},
					{
						.type = android::gralloc4::PlaneLayoutComponentType_G,
						.offsetInBits = 8,
						.sizeInBits = 8
					},
					{
						.type = android::gralloc4::PlaneLayoutComponentType_B,
						.offsetInBits = 16,
						.sizeInBits = 8
					}
				},
				.sampleIncrementInBits = color_bpp(GBM_FORMAT_BGR888),
				.horizontalSubsampling = 1,
				.verticalSubsampling = 1,
			}},
		},
		{
			GBM_FORMAT_BGR565,
			{{
				.components = {
					{
						.type = android::gralloc4::PlaneLayoutComponentType_R,
						.offsetInBits = 0,
						.sizeInBits = 5
					},
					{
						.type = android::gralloc4::PlaneLayoutComponentType_G,
						.offsetInBits = 5,
						.sizeInBits = 6
					},
					{
						.type = android::gralloc4::PlaneLayoutComponentType_B,
						.offsetInBits = 11,
						.sizeInBits = 5
					}
				},
				.sampleIncrementInBits = color_bpp(GBM_FORMAT_BGR565),
				.horizontalSubsampling = 1,
				.verticalSubsampling = 1,
                     }},
		},
		{
			GBM_FORMAT_YUV422,
			{
				{
					.components = {
						{
							.type = android::gralloc4::PlaneLayoutComponentType_Y,
							.offsetInBits = 0,
							.sizeInBits = 8
						}
					},
					.sampleIncrementInBits = 8,
					.horizontalSubsampling = 1,
					.verticalSubsampling = 1,
				},
				{
					.components = {
						{
							.type = android::gralloc4::PlaneLayoutComponentType_CB,
							.offsetInBits = 0,
							.sizeInBits = 8
						}
					},
					.sampleIncrementInBits = 8,
					.horizontalSubsampling = 2,
					.verticalSubsampling = 1,
				},
				{
					.components = {
						{
							.type = android::gralloc4::PlaneLayoutComponentType_CR,
							.offsetInBits = 0,
							.sizeInBits = 8
						}
					},
					.sampleIncrementInBits = 8,
					.horizontalSubsampling = 2,
					.verticalSubsampling = 1,
				}
			},
		},
		{
			GBM_FORMAT_YVU420,
			{
				{
					.components = {
						{
							.type = android::gralloc4::PlaneLayoutComponentType_Y,
							.offsetInBits = 0,
							.sizeInBits = 8
						}
					},
					.sampleIncrementInBits = 8,
					.horizontalSubsampling = 1,
					.verticalSubsampling = 1,
				},
				{
					.components = {
						{
							.type = android::gralloc4::PlaneLayoutComponentType_CR,
							.offsetInBits = 0,
							.sizeInBits = 8
						}
					},
					.sampleIncrementInBits = 8,
					.horizontalSubsampling = 2,
					.verticalSubsampling = 2,
				},
				{
                                     .components = {{.type = android::gralloc4::
                                                             PlaneLayoutComponentType_CB,
                                                     .offsetInBits = 0,
                                                     .sizeInBits = 8}},
                                     .sampleIncrementInBits = 8,
                                     .horizontalSubsampling = 2,
                                     .verticalSubsampling = 2,
				}
			},
		},
		{
			GBM_FORMAT_VYUY,
			{{
				.components = {
					{
						.type = android::gralloc4::PlaneLayoutComponentType_CR,
						.offsetInBits = 0,
						.sizeInBits = 8,
					},
					{
						.type = android::gralloc4::PlaneLayoutComponentType_Y,
						.offsetInBits = 8,
						.sizeInBits = 8,
					},
					{
						.type = android::gralloc4::PlaneLayoutComponentType_CB,
						.offsetInBits = 16,
						.sizeInBits = 8,
					},
					{
						.type = android::gralloc4::PlaneLayoutComponentType_Y,
						.offsetInBits = 24,
						.sizeInBits = 8,
					},
				},
				.sampleIncrementInBits = color_bpp(GBM_FORMAT_VYUY),
				.horizontalSubsampling = 1,
				.verticalSubsampling = 1,
			}},
		},
		{
			GBM_FORMAT_YUV420,
			{
				{
					.components = {
						{
							.type = android::gralloc4::PlaneLayoutComponentType_Y,
							.offsetInBits = 0,
							.sizeInBits = 8
						}
					},
					.sampleIncrementInBits = 8,
					.horizontalSubsampling = 1,
					.verticalSubsampling = 1,
				},
				{
					.components = {
						{
							.type = android::gralloc4::PlaneLayoutComponentType_CB,
							.offsetInBits = 0,
							.sizeInBits = 8
						}
					},
					.sampleIncrementInBits = 8,
					.horizontalSubsampling = 2,
					.verticalSubsampling = 2,
				},
				{
					.components = {
						{
							.type = android::gralloc4::PlaneLayoutComponentType_CR,
							.offsetInBits = 0,
							.sizeInBits = 8
						}
					},
					.sampleIncrementInBits = 8,
					.horizontalSubsampling = 2,
					.verticalSubsampling = 2,
				}
			},
		},
		{
			GBM_FORMAT_ABGR2101010,
			{{
				.components = {
					{
						.type = android::gralloc4::PlaneLayoutComponentType_R,
						.offsetInBits = 0,
						.sizeInBits = 10
					},
					{
						.type = android::gralloc4::PlaneLayoutComponentType_G,
						.offsetInBits = 10,
						.sizeInBits = 10
					},
					{
						.type = android::gralloc4::PlaneLayoutComponentType_B,
						.offsetInBits = 20,
						.sizeInBits = 10
					},
					{
						.type = android::gralloc4::PlaneLayoutComponentType_A,
						.offsetInBits = 30,
						.sizeInBits = 2
					}
				},
				.sampleIncrementInBits = color_bpp(GBM_FORMAT_ABGR2101010),
				.horizontalSubsampling = 1,
				.verticalSubsampling = 1,
			}},
		},
		{
			GBM_FORMAT_R8,
			{{
				.components = {
					{
						.type = android::gralloc4::PlaneLayoutComponentType_R,
						.offsetInBits = 0,
						.sizeInBits = 8
					}
				},
				.sampleIncrementInBits = color_bpp(GBM_FORMAT_R8),
				.horizontalSubsampling = 1,
				.verticalSubsampling = 1,
			}},
		},
		{
			GBM_FORMAT_R16,
			{{
				.components = {
					{
						.type = android::gralloc4::PlaneLayoutComponentType_R,
						.offsetInBits = 0,
						.sizeInBits = 16
					}
				},
				.sampleIncrementInBits = color_bpp(GBM_FORMAT_R16),
				.horizontalSubsampling = 1,
				.verticalSubsampling = 1,
			}},
		},
		{
			GBM_FORMAT_GR88,
			{{
				.components = {
					{
						.type = android::gralloc4::PlaneLayoutComponentType_R,
						.offsetInBits = 0,
						.sizeInBits = 8
					},
					{
						.type = android::gralloc4::PlaneLayoutComponentType_G,
						.offsetInBits = 8,
						.sizeInBits = 8
					},
				},
				.sampleIncrementInBits = color_bpp(GBM_FORMAT_GR88),
				.horizontalSubsampling = 1,
				.verticalSubsampling = 1,
			}},
		},
		{
			GBM_FORMAT_ABGR16161616F,
			{{
				.components = {
					{
						.type = android::gralloc4::PlaneLayoutComponentType_R,
						.offsetInBits = 0,
						.sizeInBits = 16
					},
					{
						.type = android::gralloc4::PlaneLayoutComponentType_G,
						.offsetInBits = 16,
						.sizeInBits = 16
					},
					{
						.type = android::gralloc4::PlaneLayoutComponentType_B,
						.offsetInBits = 32,
						.sizeInBits = 16
					},
					{
						.type = android::gralloc4::PlaneLayoutComponentType_A,
						.offsetInBits = 48,
						.sizeInBits = 16
					}
				},
				.sampleIncrementInBits = color_bpp(GBM_FORMAT_ABGR16161616F),
				.horizontalSubsampling = 1,
				.verticalSubsampling = 1,
			}},
		},
		/* additional */
		{
			GBM_FORMAT_RGB565,
			{{
				.components = {
					{
						.type = android::gralloc4::PlaneLayoutComponentType_B,
						.offsetInBits = 0,
						.sizeInBits = 5},
					{
						.type = android::gralloc4::PlaneLayoutComponentType_G,
						.offsetInBits = 5,
						.sizeInBits = 6},
					{
						.type = android::gralloc4::PlaneLayoutComponentType_R,
						.offsetInBits = 11,
						.sizeInBits = 5
					}
				},
				.sampleIncrementInBits = color_bpp(GBM_FORMAT_RGB565),
				.horizontalSubsampling = 1,
				.verticalSubsampling = 1,
			}},
		},
		/* additional */
		{
			GBM_FORMAT_NV12,
			{{
				.components = {
					{
						.type = android::gralloc4::PlaneLayoutComponentType_Y,
						.offsetInBits = 0,
						.sizeInBits = 8
					}
				},
				.sampleIncrementInBits = 8,
				.horizontalSubsampling = 1,
				.verticalSubsampling = 1,
			},
			{
				.components = {
					{
						.type = android::gralloc4::PlaneLayoutComponentType_CB,
						.offsetInBits = 0,
						.sizeInBits = 8
					},
					{
						.type = android::gralloc4::PlaneLayoutComponentType_CR,
						.offsetInBits = 8,
						.sizeInBits = 8
					}
				},
				.sampleIncrementInBits = 16,
				.horizontalSubsampling = 2,
				.verticalSubsampling = 2,
			}},
		},
		/* additional */
		{
			GBM_FORMAT_NV21,
			{{
				.components = {
					{
						.type = android::gralloc4::PlaneLayoutComponentType_Y,
						.offsetInBits = 0,
						.sizeInBits = 8
					}
				},
				.sampleIncrementInBits = 8,
				.horizontalSubsampling = 1,
				.verticalSubsampling = 1,
			},
			{
				.components = {
					{
						.type = android::gralloc4::PlaneLayoutComponentType_CR,
						.offsetInBits = 0,
						.sizeInBits = 8
					},
					{
						.type = android::gralloc4::PlaneLayoutComponentType_CB,
						.offsetInBits = 8,
						.sizeInBits = 8
					}
				},
				.sampleIncrementInBits = 16,
				.horizontalSubsampling = 2,
				.verticalSubsampling = 2,
			}},
		},
		/* additional */
		{
			GBM_FORMAT_RGBX8888,
			{{
				.components = {
					{
						.type = android::gralloc4::PlaneLayoutComponentType_B,
						.offsetInBits = 16,
						.sizeInBits = 8
					},
					{
						.type = android::gralloc4::PlaneLayoutComponentType_G,
						.offsetInBits = 8,
						.sizeInBits = 8
					},
					{
						.type = android::gralloc4::PlaneLayoutComponentType_R,
						.offsetInBits = 0,
						.sizeInBits = 8
					}
				},
				.sampleIncrementInBits = color_bpp(GBM_FORMAT_RGBX8888),
				.horizontalSubsampling = 1,
				.verticalSubsampling = 1,
                     }},
		},
		/* additional */
		{
			GBM_FORMAT_XRGB8888,
			{{
				.components = {
					{
						.type = android::gralloc4::PlaneLayoutComponentType_B,
						.offsetInBits = 16,
						.sizeInBits = 8
					},
					{
						.type = android::gralloc4::PlaneLayoutComponentType_G,
						.offsetInBits = 8,
						.sizeInBits = 8
					},
					{
						.type = android::gralloc4::PlaneLayoutComponentType_R,
						.offsetInBits = 0,
						.sizeInBits = 8
					}
				},
				.sampleIncrementInBits = color_bpp(GBM_FORMAT_XRGB8888),
				.horizontalSubsampling = 1,
				.verticalSubsampling = 1,
			}},
		},
	});
	return *kPlaneLayoutsMap;
}

inline int getPlaneLayouts(uint32_t gbm_format, std::vector<PlaneLayout>* outPlaneLayouts)
{
	const auto& planeLayoutsMap = GetPlaneLayoutsMap();
	const auto it = planeLayoutsMap.find(gbm_format);
	if (it == planeLayoutsMap.end())
		return -EINVAL;

	*outPlaneLayouts = it->second;
	return 0;
}

#endif /* MAPPER_PLANE_LAYOUTS_HPP */