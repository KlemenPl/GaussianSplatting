//
// Created by Klemen Plestenjak on 2/19/25.
//

#ifndef APP_H
#define APP_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <signal.h>

#include "input.h"
#include "wgpu-utils.h"


typedef struct AppState {
    WGPUDevice device;
    WGPUTextureFormat surfaceFormat;
    WGPUTextureView surfaceView;
} AppState;

typedef int (*AppInitFn)(const AppState *app, int argc, char **argv);
typedef void (*AppUpdateFn)(const AppState *app, float dt);
typedef void (*AppRenderFn)(const AppState *app, float dt);
typedef void (*AppDeInitFn)(const AppState *app);

typedef struct AppConfig {
    int32_t     width;
    int32_t     height;
    const char *title;

    AppInitFn   init;
    AppUpdateFn update;
    AppRenderFn render;
    AppDeInitFn deinit;
} AppConfig;

static void deviceLostCB(WGPUDeviceLostReason reason, const char *message, void *) {
    fprintf(stderr , "Device lost: 0x%0x (%s)\n", reason, message ? message : "?");
}

static void deviceErrorCB(WGPUErrorType type, const char *message, void *) {
    fprintf(stderr, "Device error: 0x%0x (%s)\n", type, message ? message : "?");
#ifndef NDEBUG
    // Trigger debug breakpoint
    raise(SIGTRAP);
#endif
}


#ifndef APP_NO_MAIN

#include <webgpu/webgpu.h>
#include <webgpu/wgpu.h>

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>

extern AppConfig appMain();


WGPUTextureView getNextSurfaceTextureView(WGPUSurface surface) {
    // Get the surface texture
    WGPUSurfaceTexture surfaceTexture;
    wgpuSurfaceGetCurrentTexture(surface, &surfaceTexture);
    if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
        return NULL;
    }

    // Create a view for this surface texture
    WGPUTextureView targetView = wgpuTextureCreateView(surfaceTexture.texture, &(WGPUTextureViewDescriptor) {
        .nextInChain = NULL,
        .label = "Surface texture view",
        .format = wgpuTextureGetFormat(surfaceTexture.texture),
        .dimension = WGPUTextureViewDimension_2D,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .baseArrayLayer = 0,
        .arrayLayerCount = 1,
        .aspect = WGPUTextureAspect_All,
    });

#ifndef WEBGPU_BACKEND_WGPU
    // We no longer need the texture, only its view
    // (NB: with wgpu-native, surface textures must not be manually released)
    wgpuTextureRelease(surfaceTexture.texture);
#endif // WEBGPU_BACKEND_WGPU

    return targetView;
}

int main(int argc, const char **argv) {
    AppConfig config = appMain();

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow *window = glfwCreateWindow(config.width, config.height, config.title, NULL, NULL);

#ifdef WEBGPU_BACKEND_EMSCRIPTEN
    WGPUInstance instance = wgpuCreateInstance(NULL);
#else
    WGPUInstance instance = wgpuCreateInstance(&(WGPUInstanceDescriptor){
        .nextInChain = NULL,
    });
#endif
    if (!instance) {
        printf("Could not initialize WebGPU!\n");
        return 1;
    }

    WGPUSurface surface = glfwGetWGPUSurface(instance, window);
    printf("WGPU instance: %p\n", instance);

    WGPUAdapter adapter = requestAdapter(instance, &(WGPURequestAdapterOptions) {
        .nextInChain = NULL,
        .compatibleSurface = surface,
    });
    wgpuInstanceRelease(instance);
    printf("\nWGPU Adapter: %p\n", adapter);
    inspectAdapter(adapter);

    WGPUDevice device = requestDevice(adapter, &(WGPUDeviceDescriptor) {
        .nextInChain = NULL,
        .label = "My Device",
        .requiredFeatureCount = 1,
        .requiredFeatures = (WGPUFeatureName[]) {
            // TODO: Disable?
            WGPUNativeFeature_VertexWritableStorage,
        },
        .requiredLimits = NULL,
        .defaultQueue.nextInChain = NULL,
        .defaultQueue.label = "My Queue",
        .deviceLostCallback = deviceLostCB,
    });
    printf("\nWGPU Device: %p\n", device);
    inspectDevice(device);
    printf("\n");
    wgpuDeviceSetUncapturedErrorCallback(device, deviceErrorCB, NULL);

    WGPUTextureFormat surfaceFormat = wgpuSurfaceGetPreferredFormat(surface, adapter);
    wgpuSurfaceConfigure(surface, &(WGPUSurfaceConfiguration) {
        .format = surfaceFormat,
        .usage = WGPUTextureUsage_RenderAttachment,
        .device = device,
        .width = config.width,
        .height = config.height,
        .nextInChain = NULL,
        .presentMode = WGPUPresentMode_Fifo,
        .alphaMode = WGPUCompositeAlphaMode_Auto,
    });
    wgpuAdapterRelease(adapter);

    inputInit(window);

    AppState appState = {
        .device = device,
        .surfaceFormat = surfaceFormat,
    };

    int status = 0;
    if (config.init) status = config.init(&appState, argc, argv);
    if (status != 0) {
        fprintf(stderr, "Failed to initialize application!\n");
    }


    float prevFrame = 0.0f;
    float currFrame = 0.0f;
    while (!glfwWindowShouldClose(window)) {
        // TODO: Accum for fixed update
        float deltaTime = currFrame - prevFrame;
        inputUpdate();
        glfwPollEvents();

        if (config.update) config.update(&appState, deltaTime);

        WGPUTextureView targetView = getNextSurfaceTextureView(surface);

        appState.surfaceView = targetView;
        if (config.render) config.render(&appState, deltaTime);
        appState.surfaceView = NULL,

        wgpuTextureViewRelease(targetView);
#ifndef __EMSCRIPTEN__
        wgpuSurfacePresent(surface);
#endif

        wgpuDevicePoll(device, false, NULL);

        prevFrame = currFrame;
        currFrame = glfwGetTime();
    }

    if (config.deinit)
        config.deinit(&appState);

    wgpuSurfaceUnconfigure(surface);
    wgpuSurfaceRelease(surface);
    wgpuDeviceRelease(device);
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

#endif


#endif //APP_H
