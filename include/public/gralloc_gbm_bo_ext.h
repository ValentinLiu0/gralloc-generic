/*
 * SPDX-FileCopyrightText: Copyright 2026 Valentin Liu (LIU, YUANCHEN) <valentinliu@icloud.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef GRALLOC_GBM_BO_EXT_H
#define GRALLOC_GBM_BO_EXT_H

#include <gralloc_generic.h>

#define GBM_BO_USE_NONE			0
#define GBM_BO_USE_EXT_RESERVED1	(1 << 11)
#define GBM_BO_USE_EXT_RESERVED2	(1 << 12)
#define GBM_BO_USE_EXT_RESERVED3	(1 << 13)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gralloc_gbm_bo_data {
	void *map_data;
	int lock_count;
	int locked_for;

	/* Android Graphics Buffer Metadata Support */
	buffer_metadata_t metadata;
} gralloc_gbm_bo_data_t, gbm_bo_data_t;


#ifdef __cplusplus
}
#endif

#endif /* GRALLOC_GBM_BO_EXT_H */