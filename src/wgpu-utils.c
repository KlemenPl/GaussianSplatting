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

void requestAdapterCB(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void *userdata1, void *userdata2) {
    RequestAdapterData *data = userdata1;
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
    wgpuInstanceRequestAdapter(instance, opts, (WGPURequestAdapterCallbackInfo){
        .callback = requestAdapterCB,
        .userdata1 = &data
    });

#ifdef __EMSCRIPTEN__
    while (!data.requestEnded) {
        emscripten_sleep(100);
    }
#endif

    assert(data.requestEnded);
    return data.adapter;
}

void requestDeviceCB(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void *userdata1, void *userdata2) {
    RequestDeviceData *data = userdata1;
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
    wgpuAdapterRequestDevice(adapter, desc, (WGPURequestDeviceCallbackInfo){
        .callback = requestDeviceCB,
        .userdata1 = &data
    });

#ifdef __EMSCRIPTEN__
    while (!data.requestEnded) {
        emscripten_sleep(100);
    }
#endif

    assert(data.requestEnded);
    return data.device;
}
