//
// Created by Klemen Plestenjak on 2/19/25.
//

#ifndef APP_H
#define APP_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <signal.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgpu.h>
#endif

#include "input.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <webgpu/webgpu.h>

#include "wgpu-utils.h"


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

#include <webgpu/webgpu.h>
#include <webgpu/wgpu.h>

#include <GLFW/glfw3.h>

extern AppConfig appMain();

static void _glfwErrorCallback(int error, const char *description) {
    fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

static void _wgpuLogCallback(WGPULogLevel level, WGPUStringView message, void *userdata) {
    const char *logLevel = "Unknown";
    switch (level) {
        case WGPULogLevel_Error: logLevel = "ERROR"; break;
        case WGPULogLevel_Warn:  logLevel = "WARN"; break;
        case WGPULogLevel_Info:  logLevel = "INFO"; break;
        case WGPULogLevel_Debug: logLevel = "DEBUG"; break;
        case WGPULogLevel_Trace: logLevel = "TRACE"; break;
        default: break;
    }

    if (level < WGPULogLevel_Warn) {
        raise(SIGTRAP);
    }

    fprintf(stderr, "WGPU [%s]: %s\n", logLevel, message);
}


static bool _initWebGPU(AppState *state) {
#ifdef WEBGPU_BACKEND_EMSCRIPTEN
    state->instance = wgpuCreateInstance(NULL);
#else
    state->instance = wgpuCreateInstance(NULL);
#endif
    if (state->instance == NULL) {
        fprintf(stderr, "Failed to create WebGPU instance\n");
        return false;
    }


    #if defined(GLFW_EXPOSE_NATIVE_COCOA)
  {
    id metal_layer = NULL;
    NSWindow *ns_window = glfwGetCocoaWindow(window);
    [ns_window.contentView setWantsLayer:YES];
    metal_layer = [CAMetalLayer layer];
    [ns_window.contentView setLayer:metal_layer];
    demo.surface = wgpuInstanceCreateSurface(
        demo.instance,
        &(const WGPUSurfaceDescriptor){
            .nextInChain =
                (const WGPUChainedStruct *)&(
                    const WGPUSurfaceSourceMetalLayer){
                    .chain =
                        (const WGPUChainedStruct){
                            .sType = WGPUSType_SurfaceSourceMetalLayer,
                        },
                    .layer = metal_layer,
                },
        });
  }
#elif defined(GLFW_EXPOSE_NATIVE_WAYLAND) && defined(GLFW_EXPOSE_NATIVE_X11)
  if (glfwGetPlatform() == GLFW_PLATFORM_X11) {
    Display *x11_display = glfwGetX11Display();
    Window x11_window = glfwGetX11Window(state->window);
    state->surface = wgpuInstanceCreateSurface(
        state->instance,
        &(const WGPUSurfaceDescriptor){
            .nextInChain =
                (const WGPUChainedStruct *)&(
                    const WGPUSurfaceSourceXlibWindow){
                    .chain =
                        (const WGPUChainedStruct){
                            .sType = WGPUSType_SurfaceSourceXlibWindow,
                        },
                    .display = x11_display,
                    .window = x11_window,
                },
        });
  }
  if (false && glfwGetPlatform() == GLFW_PLATFORM_WAYLAND) {
    struct wl_display *wayland_display = glfwGetWaylandDisplay();
    struct wl_surface *wayland_surface = glfwGetWaylandWindow(state->window);
    state->surface = wgpuInstanceCreateSurface(
        state->instance,
        &(const WGPUSurfaceDescriptor){
            .nextInChain =
                (const WGPUChainedStruct *)&(
                    const WGPUSurfaceSourceWaylandSurface){
                    .chain =
                        (const WGPUChainedStruct){
                            .sType =
                                WGPUSType_SurfaceSourceWaylandSurface,
                        },
                    .display = wayland_display,
                    .surface = wayland_surface,
                },
        });
  }
#elif defined(GLFW_EXPOSE_NATIVE_WIN32)
  {
    HWND hwnd = glfwGetWin32Window(window);
    HINSTANCE hinstance = GetModuleHandle(NULL);
    demo.surface = wgpuInstanceCreateSurface(
        demo.instance,
        &(const WGPUSurfaceDescriptor){
            .nextInChain =
                (const WGPUChainedStruct *)&(
                    const WGPUSurfaceSourceWindowsHWND){
                    .chain =
                        (const WGPUChainedStruct){
                            .sType = WGPUSType_SurfaceSourceWindowsHWND,
                        },
                    .hinstance = hinstance,
                    .hwnd = hwnd,
                },
        });
  }
#else
#error "Unsupported GLFW native platform"
#endif
    if (state->surface == NULL) {
        fprintf(stderr, "Failed to create GLFW window surface\n");
        return false;
    }

    state->adapter = requestAdapter(state->instance, &(WGPURequestAdapterOptions){
        .backendType = WGPUBackendType_Vulkan,
        .powerPreference = WGPUPowerPreference_HighPerformance,
        .compatibleSurface = state->surface,
    });
    if (state->adapter == NULL) {
        fprintf(stderr, "Failed to create WebGPU adapter\n");
        return false;
    }

    state->device = requestDevice(state->adapter, &(WGPUDeviceDescriptor){
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

    wgpuSetLogLevel(WGPULogLevel_Info);
    wgpuSetLogCallback(_wgpuLogCallback, NULL);
    if (!_initWebGPU(&state)) {
        glfwTerminate();
        return 1;
    }

    WGPUSurfaceCapabilities surfaceCapabilities = {0};
    wgpuSurfaceGetCapabilities(state.surface, state.adapter, &surfaceCapabilities);

    state.format = surfaceCapabilities.formats[0];
    state.config = (WGPUSurfaceConfiguration) {
        .device = state.device,
        .usage = WGPUTextureUsage_RenderAttachment,
        .format = surfaceCapabilities.formats[0],
        .presentMode = WGPUPresentMode_Fifo,
        .alphaMode = surfaceCapabilities.alphaModes[0],
        .width = config.width,
        .height = config.height,
    };
    wgpuSurfaceConfigure(state.surface, &state.config);

    glfwShowWindow(window);

    inputInit(window);

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

        if (config.update) config.update(&state, deltaTime);

        WGPUSurfaceTexture surfaceTexture;
        wgpuSurfaceGetCurrentTexture(state.surface, &surfaceTexture);
        switch (surfaceTexture.status) {
            case WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal:
            case WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal:
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

        if (config.render) config.render(&state, deltaTime);


        wgpuSurfacePresent(state.surface);
        wgpuTextureViewRelease(view);
        wgpuTextureRelease(surfaceTexture.texture);

        wgpuDevicePoll(state.device, false, NULL);

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
