/*
 * SPDX-FileCopyrightText: Copyright 2026 Valentin Liu (LIU, YUANCHEN) <valentinliu@icloud.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

#define LOG_TAG "GrallocGenericAllocator"
#include <private/log.h>

#include "Allocator.h"

using aidl::android::hardware::graphics::allocator::impl::GrallocGenericAllocatorV2;

int main(int /*argc*/, char** /*argv*/) {
	LOG_I("Generic AIDL allocator starting up...");

	// same as SF main thread
	struct sched_param param = {0};
	param.sched_priority = 2;
	if (sched_setscheduler(0, SCHED_FIFO | SCHED_RESET_ON_FORK, &param) != 0) {
		LOG_W("Failed to set priority: %s", strerror(errno));
	}

	auto allocator = ndk::SharedRefBase::make<GrallocGenericAllocatorV2>();
	CHECK(allocator != nullptr);

	if (allocator->init()) {
		LOG_E("Failed to initialize the allocator backends.");
		return EXIT_FAILURE;
	}

	const std::string instance = std::string() + GrallocGenericAllocatorV2::descriptor + "/default";
	binder_status_t status =
		AServiceManager_addService(allocator->asBinder().get(), instance.c_str());
	if (status != STATUS_OK) {
		LOG_E("Failed to register allocator service %s, status=%d.", instance.c_str(), status);
	}
	CHECK_EQ(status, STATUS_OK);
	LOG_I("Allocator service %s registered, starting binder thread pool.", instance.c_str());

	ABinderProcess_setThreadPoolMaxThreadCount(4);
	ABinderProcess_startThreadPool();
	ABinderProcess_joinThreadPool();

	return EXIT_FAILURE;
}