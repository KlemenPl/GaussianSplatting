/**
 * This file is part of the "Learn WebGPU for C++" book.
 *   https://github.com/eliemichel/LearnWebGPU
 *
 * MIT License
 * Copyright (c) 2022-2024 Elie Michel
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Modified 16.03.2025:
 * Port to C
 */

#include "webgpu-utils.h"

#include <assert.h>
#include <stdio.h>
#include <stdbool.h>

#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#endif // __EMSCRIPTEN__


typedef struct RequestAdapterUserData {
	WGPUAdapter adapter;
	bool requestEnded;
} RequestAdapterUserData;

void requestAdapterCB(WGPURequestAdapterStatus status, WGPUAdapter adapter, const char *message, void *pUserData) {
	RequestAdapterUserData *userData = pUserData;
	if (status == WGPURequestAdapterStatus_Success ) {
		userData->adapter = adapter;
	} else {
		fprintf(stderr, "Could not get WebGPU adapter: %s\n", message);
	}
	userData->requestEnded = true;
}

WGPUAdapter requestAdapterSync(WGPUInstance instance, WGPURequestAdapterOptions const * options) {
	// A simple structure holding the local information shared with the
	// onAdapterRequestEnded callback.
	RequestAdapterUserData userData = {
		.adapter = NULL,
		.requestEnded = false,
	};

	// Callback called by wgpuInstanceRequestAdapter when the request returns
	// This is a C++ lambda function, but could be any function defined in the
	// global scope. It must be non-capturing (the brackets [] are empty) so
	// that it behaves like a regular C function pointer, which is what
	// wgpuInstanceRequestAdapter expects (WebGPU being a C API). The workaround
	// is to convey what we want to capture through the pUserData pointer,
	// provided as the last argument of wgpuInstanceRequestAdapter and received
	// by the callback as its last argument.
	// Call to the WebGPU request adapter procedure
	wgpuInstanceRequestAdapter(
		instance /* equivalent of navigator.gpu */,
		options,
		requestAdapterCB,
		(void*)&userData
	);

	// We wait until userData.requestEnded gets true
#ifdef __EMSCRIPTEN__
	while (!userData.requestEnded) {
		emscripten_sleep(100);
	}
#endif // __EMSCRIPTEN__

	assert(userData.requestEnded);

	return userData.adapter;
}

void inspectAdapter(WGPUAdapter adapter) {
#ifndef __EMSCRIPTEN__
	WGPUSupportedLimits supportedLimits = {};
	supportedLimits.nextInChain = NULL;
	bool success = wgpuAdapterGetLimits(adapter, &supportedLimits);
	if (success) {
		printf("Adapter limits:\n");
		printf(" - maxTextureDimension1D: %d\n", supportedLimits.limits.maxTextureDimension1D);
		printf(" - maxTextureDimension2D: %d\n", supportedLimits.limits.maxTextureDimension2D);
		printf(" - maxTextureDimension3D: %d\n", supportedLimits.limits.maxTextureDimension3D);
		printf(" - maxTextureArrayLayers: %d\n", supportedLimits.limits.maxTextureArrayLayers);
	}
#endif // NOT __EMSCRIPTEN__

	// Call the function a first time with a null return address, just to get
	// the entry count.
	size_t featureCount = wgpuAdapterEnumerateFeatures(adapter, NULL);
	// Allocate memory (could be a new, or a malloc() if this were a C program)
	WGPUFeatureName features[featureCount];
	// Call the function a second time, with a non-null return address
	wgpuAdapterEnumerateFeatures(adapter, features);

	printf("Adapter features:\n");
	for (size_t i = 0; i < featureCount; i++) {
		printf(" - 0x%X\n", features[i]);
	}
	WGPUAdapterProperties properties = {};
	properties.nextInChain = NULL;
	wgpuAdapterGetProperties(adapter, &properties);
	printf("Adapter properties:\n");
	printf(" - vendorID: %d\n", properties.vendorID);
	if (properties.vendorName) {
		printf(" - vendorName: %s\n", properties.vendorName);
	}
	if (properties.architecture) {
		printf(" - architecture: %s\n", properties.architecture);
	}
	printf(" - deviceID: %d\n", properties.deviceID);
	if (properties.name) {
		printf(" - name: %s\n", properties.name);
	}
	if (properties.driverDescription) {
		printf(" - driverDescription: %s\n", properties.driverDescription);
	}
	printf(" - adapterType: 0x%X\n", properties.adapterType);
	printf(" - backendType: 0x%X\n", properties.backendType);
}

typedef struct RequestDeviceUserData {
	WGPUDevice device;
	bool requestEnded;

} RequestDeviceUserData;

void requestDeviceCB(WGPURequestDeviceStatus status, WGPUDevice device, const char *message, void *pUserData) {
	RequestDeviceUserData *userData = (RequestDeviceUserData *) pUserData;
	if (status == WGPURequestDeviceStatus_Success) {
		userData->device = device;
	} else {
		fprintf(stderr, "Could not get WebGPU device: %s\n", message);
	}
	userData->requestEnded = true;
}

WGPUDevice requestDeviceSync(WGPUAdapter adapter, WGPUDeviceDescriptor const * descriptor) {
	RequestDeviceUserData userData = {
		.device = NULL,
		.requestEnded = false
	};

	wgpuAdapterRequestDevice(
		adapter,
		descriptor,
		requestDeviceCB,
		(void*)&userData
	);

#ifdef __EMSCRIPTEN__
	while (!userData.requestEnded) {
		emscripten_sleep(100);
	}
#endif // __EMSCRIPTEN__

	assert(userData.requestEnded);

	return userData.device;
}

void inspectDevice(WGPUDevice device) {
	size_t featureCount = wgpuDeviceEnumerateFeatures(device, NULL);
	WGPUFeatureName features[featureCount];
	wgpuDeviceEnumerateFeatures(device, features);

	printf("Device features:\n");
	for (size_t i = 0; i < featureCount; i++) {
		printf(" - 0x%X\n", features[i]);
	}

	WGPUSupportedLimits limits = {};
	limits.nextInChain = NULL;
	bool success = wgpuDeviceGetLimits(device, &limits);
	if (success) {
		printf("Device limits:\n");
		printf(" - maxTextureDimension1D: %d\n", limits.limits.maxTextureDimension1D);
		printf(" - maxTextureDimension2D: %d\n", limits.limits.maxTextureDimension2D);
		printf(" - maxTextureDimension3D: %d\n", limits.limits.maxTextureDimension3D);
		printf(" - maxTextureArrayLayers: %d\n", limits.limits.maxTextureArrayLayers);
		printf(" - maxBindGroups: %d\n", limits.limits.maxBindGroups);
		printf(" - maxDynamicUniformBuffersPerPipelineLayout: %d\n", limits.limits.maxDynamicUniformBuffersPerPipelineLayout);
		printf(" - maxDynamicStorageBuffersPerPipelineLayout: %d\n", limits.limits.maxDynamicStorageBuffersPerPipelineLayout);
		printf(" - maxSampledTexturesPerShaderStage: %d\n", limits.limits.maxSampledTexturesPerShaderStage);
		printf(" - maxSamplersPerShaderStage: %d\n", limits.limits.maxSamplersPerShaderStage);
		printf(" - maxStorageBuffersPerShaderStage: %d\n", limits.limits.maxStorageBuffersPerShaderStage);
		printf(" - maxStorageTexturesPerShaderStage: %d\n", limits.limits.maxStorageTexturesPerShaderStage);
		printf(" - maxUniformBuffersPerShaderStage: %d\n", limits.limits.maxUniformBuffersPerShaderStage);
		printf(" - maxUniformBufferBindingSize: %ld\n", limits.limits.maxUniformBufferBindingSize);
		printf(" - maxStorageBufferBindingSize: %ld\n", limits.limits.maxStorageBufferBindingSize);
		printf(" - minUniformBufferOffsetAlignment: %d\n", limits.limits.minUniformBufferOffsetAlignment);
		printf(" - minStorageBufferOffsetAlignment: %d\n", limits.limits.minStorageBufferOffsetAlignment);
		printf(" - maxVertexBuffers: %d\n", limits.limits.maxVertexBuffers);
		printf(" - maxVertexAttributes: %d\n", limits.limits.maxVertexAttributes);
		printf(" - maxVertexBufferArrayStride: %d\n", limits.limits.maxVertexBufferArrayStride);
		printf(" - maxInterStageShaderComponents: %d\n", limits.limits.maxInterStageShaderComponents);
		printf(" - maxComputeWorkgroupStorageSize: %d\n", limits.limits.maxComputeWorkgroupStorageSize);
		printf(" - maxComputeInvocationsPerWorkgroup: %d\n", limits.limits.maxComputeInvocationsPerWorkgroup);
		printf(" - maxComputeWorkgroupSizeX: %d\n", limits.limits.maxComputeWorkgroupSizeX);
		printf(" - maxComputeWorkgroupSizeY: %d\n", limits.limits.maxComputeWorkgroupSizeY);
		printf(" - maxComputeWorkgroupSizeZ: %d\n", limits.limits.maxComputeWorkgroupSizeZ);
		printf(" - maxComputeWorkgroupsPerDimension: %d\n", limits.limits.maxComputeWorkgroupsPerDimension);
	}
}
