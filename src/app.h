//
// Created by Klemen Plestenjak on 2/19/25.
//

#ifndef APP_H
#define APP_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <signal.h>

#include <webgpu/webgpu.h>
#include <webgpu/wgpu.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "input.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <webgpu/webgpu.h>
#include <webgpu/webgpu.h>

#include "imgui.h"

#include "webgpu-utils.h"

typedef struct AppState {
    GLFWwindow *window;
    WGPUInstance instance;
    WGPUSurface surface;
    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUTextureFormat format;
    WGPUSurfaceConfiguration config;
    WGPUTextureView view;
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

#ifndef APP_NO_MAIN

extern AppConfig appMain();

static void _glfwErrorCallback(int error, const char *description) {
    fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

static void _wgpuOnDeviceError(WGPUErrorType type, const char *message, void *userdata) {
    const char *errorType = "Unknown";
    switch (type) {
        case WGPUErrorType_NoError: errorType = "No error"; break;
        case WGPUErrorType_Validation: errorType = "Validation"; break;
        case WGPUErrorType_OutOfMemory: errorType = "OutOfMemory"; break;
        case WGPUErrorType_Internal: errorType = "Internal"; break;
        case WGPUErrorType_Unknown: errorType = "Unknown"; break;
        case WGPUErrorType_Force32: errorType = "Force32"; break;
        default: break;
    }

    fprintf(stderr, "WGPU [%s]: %s\n", errorType, message);
}


static bool _initWebGPU(AppState *state) {
    state->instance = wgpuCreateInstance(NULL);
    if (state->instance == NULL) {
        fprintf(stderr, "Failed to create WebGPU instance\n");
        return false;
    }

    state->surface = glfwGetWGPUSurface(state->instance, state->window);
    if (state->surface == NULL) {
        fprintf(stderr, "Failed to create GLFW window surface\n");
        return false;
    }

    state->adapter = requestAdapterSync(state->instance, &(WGPURequestAdapterOptions){
        .backendType = WGPUBackendType_Undefined,
        .powerPreference = WGPUPowerPreference_HighPerformance,
        .compatibleSurface = state->surface,
    });
    if (state->adapter == NULL) {
        fprintf(stderr, "Failed to create WebGPU adapter\n");
        return false;
    }

    state->device = requestDeviceSync(state->adapter, &(WGPUDeviceDescriptor){
        .requiredFeatureCount = 1,
        .requiredFeatures = (WGPUFeatureName[]){
            WGPUNativeFeature_VertexWritableStorage
        }
    });
    if (state->device == NULL) {
        fprintf(stderr, "Failed to create WebGPU device\n");
        return false;
    }

    return true;
}

int main(int argc, const char **argv) {
    AppConfig config = appMain();

    glfwSetErrorCallback(_glfwErrorCallback);
    if (!glfwInit())
        return 1;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow *window = glfwCreateWindow(config.width, config.height, config.title, NULL, NULL);

    AppState state = {
        .window = window,
    };

    if (!_initWebGPU(&state)) {
        glfwTerminate();
        return 1;
    }
    inspectAdapter(state.adapter);
    inspectDevice(state.device);
    wgpuDeviceSetUncapturedErrorCallback(state.device, _wgpuOnDeviceError, NULL);

    WGPUSurfaceCapabilities surfaceCapabilities = {0};
    wgpuSurfaceGetCapabilities(state.surface, state.adapter, &surfaceCapabilities);

    state.format = surfaceCapabilities.formats[0];
    state.config = (WGPUSurfaceConfiguration) {
        .device = state.device,
        .usage = WGPUTextureUsage_RenderAttachment,
        .format = surfaceCapabilities.formats[0],
        .presentMode = WGPUPresentMode_Immediate,
        .alphaMode = WGPUCompositeAlphaMode_Opaque,
        .width = config.width,
        .height = config.height,
    };
    wgpuSurfaceConfigure(state.surface, &state.config);

    glfwShowWindow(window);

    inputInit(window);


    igCreateContext(NULL);
    ImGui_ImplGlfw_InitForOther(window, true);
    igStyleColorsDark(NULL);

    ImGui_ImplWGPU_InitInfo initInfo = {
        .Device = state.device,
        .NumFramesInFlight = 3,
        .RenderTargetFormat = surfaceCapabilities.formats[0],
        .DepthStencilFormat = WGPUTextureFormat_Undefined,
        .PipelineMultisampleState = (WGPUMultisampleState) {
            .count = 1,
            .mask = UINT32_MAX,
        }
    };
    ImGui_ImplWGPU_Init(&initInfo);


    int status = 0;
    if (config.init) status = config.init(&state, argc, argv);
    if (status != 0) {
        fprintf(stderr, "Failed to initialize application!\n");
    }

    char titleBuf[256];
    float prevFrame = 0.0f;
    float currFrame = 0.0f;
    while (!glfwWindowShouldClose(window)) {
        // TODO: Accum for fixed update
        float deltaTime = currFrame - prevFrame;
        snprintf(titleBuf, sizeof(titleBuf), "%s [%.2f FPS | %.2f ms]", config.title, 1 / deltaTime, deltaTime * 1000.0f);
        glfwSetWindowTitle(window, titleBuf);
        inputUpdate();
        glfwPollEvents();

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        if (width != config.width || height != config.height) {
            config.width = width;
            config.height = height;
            ImGui_ImplWGPU_InvalidateDeviceObjects();
            wgpuSurfaceUnconfigure(state.surface);
            wgpuSurfaceConfigure(state.surface, &state.config);
            ImGui_ImplWGPU_CreateDeviceObjects();
        }

        if (config.update) config.update(&state, deltaTime);

        WGPUSurfaceTexture surfaceTexture;
        wgpuSurfaceGetCurrentTexture(state.surface, &surfaceTexture);
        switch (surfaceTexture.status) {
            case WGPUSurfaceGetCurrentTextureStatus_Success:
              // All good, could handle suboptimal here
              break;
            case WGPUSurfaceGetCurrentTextureStatus_Timeout:
            case WGPUSurfaceGetCurrentTextureStatus_Outdated:
            case WGPUSurfaceGetCurrentTextureStatus_Lost: {
                // Skip this frame, and re-configure surface.
                if (surfaceTexture.texture != NULL) {
                    wgpuTextureRelease(surfaceTexture.texture);
                }
                int width, height;
                glfwGetWindowSize(window, &width, &height);
                if (width != 0 && height != 0) {
                    state.config.width = width;
                    state.config.height = height;
                    wgpuSurfaceConfigure(state.surface, &state.config);
                }
                continue;
            }

        }
        WGPUTextureView view = wgpuTextureCreateView(surfaceTexture.texture, NULL);
        state.view = view;

        ImGui_ImplWGPU_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        igNewFrame();

        if (config.render) config.render(&state, deltaTime);


        wgpuSurfacePresent(state.surface);
        wgpuTextureViewRelease(view);
        wgpuTextureRelease(surfaceTexture.texture);

#ifndef __EMSCRIPTEN__
        wgpuDevicePoll(state.device, false, NULL);
#endif

        prevFrame = currFrame;
        currFrame = glfwGetTime();
    }

    if (config.deinit)
        config.deinit(&state);

    wgpuInstanceRelease(state.instance);
    wgpuSurfaceUnconfigure(state.surface);
    wgpuSurfaceRelease(state.surface);
    wgpuAdapterRelease(state.adapter);
    wgpuDeviceRelease(state.device);
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

#endif

#endif //APP_H
