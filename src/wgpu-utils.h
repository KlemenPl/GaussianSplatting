//
// Created by Klemen Plestenjak on 2/19/25.
//

#ifndef WEBGPU_UTILS_H
#define WEBGPU_UTILS_H

#include <webgpu/webgpu.h>

WGPUAdapter requestAdapter(WGPUInstance instance, const WGPURequestAdapterOptions *opts);

WGPUDevice requestDevice(WGPUAdapter adapter, const WGPUDeviceDescriptor *desc);

#endif //WEBGPU_UTILS_H
