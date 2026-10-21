/*
 * SPDX-FileCopyrightText: Copyright 2026 Valentin Liu (LIU, YUANCHEN) <valentinliu@icloud.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _PRIVATE_LOG_H
#define _PRIVATE_LOG_H

#define NO_TRACE 1

#if !defined(NO_DEBUG) && defined(ANDROID_DEBUGGABLE)
//#warning Enabled verbose output.
#define LOG_NDEBUG 0
#endif

#ifndef LOG_TAG
#define LOG_TAG "GrallocGbmLog"
#warning The LOG_TAG is not defined! Using default tag.
#endif

#include <android/log.h>

#ifndef NO_TRACE
#define LOG_TRACE() __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "%s:%d func %s()", __FILE_NAME__, __LINE__, __FUNCTION__)
#else
#define LOG_TRACE()
#endif /* NO_TRACE */

#define LOG_V(fmt, ...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "[V] %s@%s[:%d]: " fmt, __func__, __FILE_NAME__, __LINE__, ##__VA_ARGS__)
#define LOG_D(fmt, ...) __android_log_print(ANDROID_LOG_DEBUG,   LOG_TAG, "[D] %s@%s[:%d]: " fmt, __func__, __FILE_NAME__, __LINE__, ##__VA_ARGS__)
#define LOG_I(fmt, ...) __android_log_print(ANDROID_LOG_INFO,    LOG_TAG, "[I] %s@%s[:%d]: " fmt, __func__, __FILE_NAME__, __LINE__, ##__VA_ARGS__)
#define LOG_W(fmt, ...) __android_log_print(ANDROID_LOG_WARN,    LOG_TAG, "[W] %s@%s[:%d]: " fmt, __func__, __FILE_NAME__, __LINE__, ##__VA_ARGS__)
#define LOG_E(fmt, ...) __android_log_print(ANDROID_LOG_ERROR,   LOG_TAG, "[E] %s@%s[:%d]: " fmt, __func__, __FILE_NAME__, __LINE__, ##__VA_ARGS__)
#define LOG_F(fmt, ...) __android_log_print(ANDROID_LOG_FATAL,   LOG_TAG, "[F] %s@%s[:%d]: " fmt, __func__, __FILE_NAME__, __LINE__, ##__VA_ARGS__)

#define LOG_ASSERT(cond, ...) __android_log_assert(cond, LOG_TAG, __VA_ARGS__)

#endif /* _PRIVATE_LOG_H */