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
#include <emscripten/html5.h>
#endif

#include "input.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>

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
    raise(SIGINT);
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
        .requiredFeatureCount = 0,
    });
    if (state->device == NULL) {
        fprintf(stderr, "Failed to create WebGPU device\n");
        return false;
    }

    return true;
}


char titleBuf[256];
float prevFrame = 0.0f;
float currFrame = 0.0f;

AppState state;
AppConfig config;


static void mainLoop() {

    // TODO: Accum for fixed update
    float deltaTime = currFrame - prevFrame;
    snprintf(titleBuf, sizeof(titleBuf), "%s [%.2f FPS | %.2f ms]", config.title, 1 / deltaTime, deltaTime * 1000.0f);
    glfwSetWindowTitle(state.window, titleBuf);

    glfwPollEvents();

    int width, height;
    glfwGetFramebufferSize(state.window, &width, &height);
    if (width != state.config.width || height != state.config.height) {
        state.config.width = width;
        state.config.height = height;
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
        case WGPUSurfaceGetCurrentTextureStatus_Lost:
        default: {
            // Skip this frame, and re-configure surface.
            if (surfaceTexture.texture != NULL) {
                wgpuTextureRelease(surfaceTexture.texture);
            }
            int width, height;
            glfwGetWindowSize(state.window, &width, &height);
            if (width != 0 && height != 0) {
                state.config.width = width;
                state.config.height = height;
                wgpuSurfaceConfigure(state.surface, &state.config);
            }
            return;
        }

    }
    WGPUTextureView view = wgpuTextureCreateView(surfaceTexture.texture, &(WGPUTextureViewDescriptor){
        .label = "Surface Texture View",
        .format = wgpuTextureGetFormat(surfaceTexture.texture),
        .dimension = WGPUTextureViewDimension_2D,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .baseArrayLayer = 0,
        .arrayLayerCount = 1,
        .aspect = WGPUTextureAspect_All,
    });
    state.view = view;

    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    igNewFrame();

    if (config.render) config.render(&state, deltaTime);


#ifndef __EMSCRIPTEN__
    wgpuSurfacePresent(state.surface);
#endif
    wgpuTextureViewRelease(view);
    wgpuTextureRelease(surfaceTexture.texture);

#ifndef __EMSCRIPTEN__
    wgpuDevicePoll(state.device, false, NULL);
#endif

    prevFrame = currFrame;
    currFrame = glfwGetTime();

    // Note: You know that feeling, when you are searching stackoverflow for answers
    // and then you stumble upon your own question that exactly answers your question:
    //
    // Couldn't be me :/
    // https://stackoverflow.com/questions/74955822/keyjustdown-implementation-in-emscripten-using-glfw3/74959249#74959249
#ifdef __EMSCRIPTEN__
    glfwSwapBuffers(state.window);
#endif
    inputUpdate();
}

int main(int argc, const char **argv) {
    config = appMain();

    glfwSetErrorCallback(_glfwErrorCallback);
    if (!glfwInit())
        return 1;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    GLFWwindow *window = glfwCreateWindow(config.width, config.height, config.title, NULL, NULL);

    state.window = window;
    if (!_initWebGPU(&state)) {
        glfwTerminate();
        return 1;
    }
    inspectAdapter(state.adapter);
    inspectDevice(state.device);
    wgpuDeviceSetUncapturedErrorCallback(state.device, _wgpuOnDeviceError, NULL);

    WGPUSurfaceCapabilities surfaceCapabilities = {0};
    wgpuSurfaceGetCapabilities(state.surface, state.adapter, &surfaceCapabilities);

    //state.format = surfaceCapabilities.formats[0];
    WGPUTextureFormat format = wgpuSurfaceGetPreferredFormat(state.surface, state.adapter);
    state.format = format;
    state.config = (WGPUSurfaceConfiguration) {
        .device = state.device,
        .usage = WGPUTextureUsage_RenderAttachment,
        .format = state.format,
#ifdef __EMSCRIPTEN__
        .presentMode = WGPUPresentMode_Fifo,
#else
        .presentMode = WGPUPresentMode_Immediate,
#endif
        .alphaMode = WGPUCompositeAlphaMode_Auto,
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


#ifndef __EMSCRIPTEN__
    while (!glfwWindowShouldClose(window)) {
        // TODO: Accum for fixed update
        mainLoop();
    }
#else
    emscripten_set_main_loop(mainLoop, 0, true);
    // No clean up on HTML5
    return 0;
#endif

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
