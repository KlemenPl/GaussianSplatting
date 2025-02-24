//
// Created by Klemen Plestenjak on 2/19/25.
//

#include "wgpu-utils.h"

#include <stdbool.h>
#include <stdio.h>
#include <assert.h>

#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#endif // __EMSCRIPTEN__

typedef struct RequestAdapterData {
    WGPUAdapter adapter;
    bool requestEnded;
} RequestAdapterData;

typedef struct RequestDeviceData {
    WGPUDevice device;
    bool requestEnded;
} RequestDeviceData;

void requestAdapterCB(WGPURequestAdapterStatus status, WGPUAdapter adapter, char const *message, void *userdata) {
    RequestAdapterData *data = userdata;
    if (status == WGPURequestAdapterStatus_Success) {
        data->adapter = adapter;
    } else {
        data->adapter = NULL;
        printf("Could not get WebGPU adapter: %s\n", message);
    }
    data->requestEnded = true;
}
WGPUAdapter requestAdapter(WGPUInstance instance, const WGPURequestAdapterOptions *opts) {
    RequestAdapterData data;
    wgpuInstanceRequestAdapter(instance, opts, requestAdapterCB, &data);

#ifdef __EMSCRIPTEN__
    while (!data.requestEnded) {
        emscripten_sleep(100);
    }
#endif

    assert(data.requestEnded);
    return data.adapter;
}

void requestDeviceCB(WGPURequestDeviceStatus status, WGPUDevice device, char const *message, void *userdata) {
    RequestDeviceData *data = userdata;
    if (status == WGPURequestDeviceStatus_Success) {
        data->device = device;
    } else {
        data->device = NULL;
        printf("Could not get WebGPU device: %s\n", message);
    }
    data->requestEnded = true;
}

WGPUDevice requestDevice(WGPUAdapter adapter, const WGPUDeviceDescriptor *desc) {
    RequestDeviceData data;
    wgpuAdapterRequestDevice(adapter, desc, requestDeviceCB, &data);

#ifdef __EMSCRIPTEN__
    while (!data.requestEnded) {
        emscripten_sleep(100);
    }
#endif

    assert(data.requestEnded);
    return data.device;
}

void inspectAdapter(WGPUAdapter adapter) {
    WGPUSupportedLimits supportedLimits = {.nextInChain = NULL};

    WGPUBool success = wgpuAdapterGetLimits(adapter, &supportedLimits);
    if (success) {
        WGPULimits *const limits = &supportedLimits.limits;
        printf("Adapter limits:\n");
        printf("\tmaxTextureDimension1D: %d\n", limits->maxTextureDimension1D);
        printf("\tmaxTextureDimension2D: %d\n", limits->maxTextureDimension2D);
        printf("\tmaxTextureDimension3D: %d\n", limits->maxTextureDimension3D);
        printf("\tmaxTextureArrayLayers: %d\n", limits->maxTextureArrayLayers);
    }

    size_t featureCount = wgpuAdapterEnumerateFeatures(adapter, NULL);
    WGPUFeatureName features[featureCount];
    wgpuAdapterEnumerateFeatures(adapter, features);

    printf("Adapter features:\n");
    for (size_t i = 0; i < featureCount; i++) {
        printf("\t0x%0x\n", features[i]);
    }

    WGPUAdapterProperties properties = {.nextInChain = NULL};
    wgpuAdapterGetProperties(adapter, &properties);
    printf("Adapter properties:\n");
    printf("\tVendorID: %d\n", properties.vendorID);
    printf("\tDeviceID: %d\n", properties.deviceID);
    printf("\tArchitecture: %s\n", properties.architecture);
    printf("\tName: %s\n", properties.name ? properties.name : "");
    printf("\tDriverDesc: %s\n", properties.driverDescription ? properties.driverDescription : "");
    printf("\tAdapterType: 0x%0x\n", properties.adapterType);
    printf("\tBackendType: 0x%0x\n", properties.backendType);
}

void inspectDevice(WGPUDevice device) {

    size_t featureCount = wgpuDeviceEnumerateFeatures(device, NULL);
    WGPUFeatureName features[featureCount];
    wgpuDeviceEnumerateFeatures(device, features);

    printf("Device features:\n");
    for (size_t i = 0; i < featureCount; i++) {
        printf("\t0x%0x\n", features[i]);
    }

    WGPUSupportedLimits supportedLimits = {.nextInChain = NULL};
    bool success = wgpuDeviceGetLimits(device, &supportedLimits);
    if (success) {
        WGPULimits *const limits = &supportedLimits.limits;
        printf("Device limits:\n");
        printf("\tmaxTextureDimension1D: %d\n", limits->maxTextureDimension1D);
        printf("\tmaxTextureDimension2D: %d\n", limits->maxTextureDimension2D);
        printf("\tmaxTextureDimension3D: %d\n", limits->maxTextureDimension3D);
        printf("\tmaxTextureArrayLayers: %d\n", limits->maxTextureArrayLayers);
    }
}

