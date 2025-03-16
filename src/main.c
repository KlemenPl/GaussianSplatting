#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "camera.h"
#include "utils.h"

typedef struct SplatRaw {
    float pos[3];
    float scale[3];
    uint32_t color;
    uint32_t rotation;
} SplatRaw;
_Static_assert(sizeof(SplatRaw) == 12 + 12 + 4 + 4, "");

typedef struct Splat {
    alignas(16) float pos[3];
    alignas(16) float scale[3];
    alignas(4) uint32_t color;
    alignas(4) uint32_t rotation;
} Splat;
_Static_assert(sizeof(Splat) == 48, "");


typedef struct Uniform {
    mat4 viewProj;
    float scale;
} Uniform;

typedef struct SortUniform {
    alignas(16) uint32_t comparePattern;
} SortUniform;
// NOTE: WGPU requires uniforms to be 16 bytes
_Static_assert(sizeof(SortUniform) == 16, "");
_Static_assert(offsetof(SortUniform, comparePattern) == 0, "");

ArcballCamera camera = CAMERA_ARCBALL_DEFAULT;

const char *splatFiles[] = {"nike.splat", "plush.splat", "train.splat", ""};

uint32_t numSplats;

WGPUQueue queue;

WGPUBindGroup computeBindGroup;
WGPUBindGroup pipelineBindGroup;
WGPUPipelineLayout computeLayout;
WGPUPipelineLayout pipelineLayout;

WGPUComputePipeline transformPipeline;
WGPUComputePipeline sortPipeline;

WGPUShaderModule computeShaderModule;
WGPUShaderModule renderShaderModule;
WGPUBuffer uniformBuffer;
WGPUBuffer sortUniformBuffer;
WGPUBuffer stagingSortUniformBuffer;
WGPUBuffer splatsBuffer;
WGPUBuffer transformedPosBuffer;
WGPUBuffer sortedIndexBuffer;
WGPURenderPipeline renderPipeline;

Splat *splats;
vec4 *transformedPos;
uint32_t *sortedIndex;

int cmpTransformedPosZ(const void *a, const void *b) {
    uint32_t idxA = *(const uint32_t *)a;
    uint32_t idxB = *(const uint32_t *)b;

    float zA = transformedPos[idxA][2];
    float zB = transformedPos[idxB][2];

    if (zA < zB) return 1;
    if (zA > zB) return -1;
    return 0;
}

int init(const AppState *app, int argc, const char **argv) {
    queue = wgpuDeviceGetQueue(app->device);

    const char *computeShader = readFile("assets/compute.wgsl");
    computeShaderModule = wgpuDeviceCreateShaderModule(app->device, &(WGPUShaderModuleDescriptor) {
        .nextInChain = (WGPUChainedStruct*) &(WGPUShaderModuleWGSLDescriptor) {
            .chain = (WGPUChainedStruct) {
                .next = NULL,
                .sType = WGPUSType_ShaderModuleWGSLDescriptor,
            },
            .code = computeShader,
        },
    });
    free(computeShader);
    const char *renderShader = readFile("assets/render.wgsl");
    renderShaderModule = wgpuDeviceCreateShaderModule(app->device, &(WGPUShaderModuleDescriptor) {
        .nextInChain = (WGPUChainedStruct*) &(WGPUShaderModuleWGSLDescriptor) {
            .chain.next = NULL,
            .chain.sType = WGPUSType_ShaderModuleWGSLDescriptor,
            .code = renderShader,
        },
    });
    free(renderShader);

    uniformBuffer = wgpuDeviceCreateBuffer(app->device, &(WGPUBufferDescriptor) {
        .label = "Uniform Buffer",
        .size = sizeof(Uniform),
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
    });
    sortUniformBuffer = wgpuDeviceCreateBuffer(app->device, &(WGPUBufferDescriptor) {
        .label = "Sort Uniform Buffer",
        .size = sizeof(SortUniform),
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
    });
    stagingSortUniformBuffer = wgpuDeviceCreateBuffer(app->device, &(WGPUBufferDescriptor) {
        .label = "Staging Uniform Buffer",
        .size = 10000 * sizeof(SortUniform),
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc,
    });

    return 0;
}
void deinit(const AppState *app) {
    wgpuBindGroupRelease(computeBindGroup);
    wgpuBindGroupRelease(pipelineBindGroup);
    wgpuPipelineLayoutRelease(pipelineLayout);
    wgpuPipelineLayoutRelease(computeLayout);

    free(splats);
    free(transformedPos);
    free(sortedIndex);

    wgpuBufferRelease(stagingSortUniformBuffer);
    wgpuBufferRelease(sortUniformBuffer);
    wgpuBufferRelease(uniformBuffer);
    wgpuBufferRelease(sortedIndexBuffer);
    wgpuBufferRelease(transformedPosBuffer);
    wgpuBufferRelease(splatsBuffer);

    wgpuShaderModuleRelease(computeShaderModule);
    wgpuShaderModuleRelease(renderShaderModule);

    wgpuComputePipelineRelease(transformPipeline);
    wgpuComputePipelineRelease(sortPipeline);
    wgpuRenderPipelineRelease(renderPipeline);
    wgpuQueueRelease(queue);
}

void loadSplat(const AppState *app, const char *splatFile) {
    char buf[256];
    snprintf(buf, sizeof(buf), "assets/%s", splatFile);
    splatFile = buf;
    FILE *f = fopen(splatFile, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open file %s\n", splatFile);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    size_t fileSize = ftell(f);
    numSplats = fileSize / sizeof(SplatRaw);
    assert(fileSize % sizeof(SplatRaw) == 0 && "Invalid file length");
    if (splats)
        free(splats);
    splats = malloc(numSplats * sizeof(*splats));
    fseek(f, 0, SEEK_SET);

    vec3 center = {0, 0, 0};
    for (size_t i = 0, idx = 0; i < fileSize; i += sizeof(SplatRaw), idx++) {
        SplatRaw splatRaw;
        fread(&splatRaw, sizeof(splatRaw), 1, f);
        glm_vec3_add(center, splatRaw.pos, center);
        glm_vec3_copy(splatRaw.pos, splats[idx].pos);
        glm_vec3_copy(splatRaw.scale, splats[idx].scale);
        splats[idx].color = splatRaw.color;
        splats[idx].rotation = splatRaw.rotation;
    }
    glm_vec3_div(center, (vec3){numSplats, numSplats, numSplats}, camera.center);

    fclose(f);
    printf("Loaded %s (%ld points)\n", splatFile, numSplats);

    if (splatsBuffer) {
        wgpuBufferRelease(splatsBuffer);
    }
    if (transformedPosBuffer) {
        wgpuBufferRelease(transformedPosBuffer);
    }
    if (sortedIndexBuffer) {
        wgpuBufferRelease(sortedIndexBuffer);
    }

    if (transformedPos)
        free(transformedPos);
    if (sortedIndex)
        free(sortedIndex);


    splatsBuffer = wgpuDeviceCreateBuffer(app->device, &(WGPUBufferDescriptor) {
        .label = "Splats Buffer",
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex | WGPUBufferUsage_Storage,
        .size = numSplats * sizeof(Splat),
        .mappedAtCreation = false
    });
    wgpuQueueWriteBuffer(queue, splatsBuffer, 0, splats, numSplats * sizeof(Splat));

    transformedPosBuffer = wgpuDeviceCreateBuffer(app->device, &(WGPUBufferDescriptor) {
        .label = "Transformed Positions",
        .usage = WGPUBufferUsage_Storage | WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst,
        .size = numSplats * sizeof(vec4),
    });
    transformedPos = malloc(numSplats * sizeof(vec4));
    sortedIndexBuffer = wgpuDeviceCreateBuffer(app->device, &(WGPUBufferDescriptor) {
        .label = "Sorted Indices",
        .usage = WGPUBufferUsage_Storage | WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst,
        .size = numSplats * sizeof(uint32_t),
    });
    sortedIndex = malloc(numSplats * sizeof(uint32_t));

    WGPUBindGroupLayout computeBindLayout = wgpuDeviceCreateBindGroupLayout(app->device, &(WGPUBindGroupLayoutDescriptor) {
        .entryCount = 5,
        .entries = (WGPUBindGroupLayoutEntry[]) {
            [0] = {
                .binding = 0,
                .visibility = WGPUShaderStage_Compute,
                .buffer.type = WGPUBufferBindingType_Uniform,
            },
            [1] = {
                .binding = 1,
                .visibility = WGPUShaderStage_Compute,
                .buffer.type = WGPUBufferBindingType_Uniform,
            },
            [2] = {
                .binding = 2,
                .visibility = WGPUShaderStage_Compute,
                .buffer.type = WGPUBufferBindingType_ReadOnlyStorage,
            },
            [3] = {
                .binding = 3,
                .visibility = WGPUShaderStage_Compute,
                .buffer.type = WGPUBufferBindingType_Storage,
            },
            [4] = {
                .binding = 4,
                .visibility = WGPUShaderStage_Compute,
                .buffer.type = WGPUBufferBindingType_Storage,
            },
        }
    });
    WGPUBindGroupLayout pipelineBindLayout = wgpuDeviceCreateBindGroupLayout(app->device, &(WGPUBindGroupLayoutDescriptor) {
        .entryCount = 4,
        .entries = (WGPUBindGroupLayoutEntry[]) {
            [0] = {
                .binding = 0,
                .visibility = WGPUShaderStage_Vertex,
                .buffer.type = WGPUBufferBindingType_Uniform,
            },
            [1] = {
                .binding = 1,
                .visibility = WGPUShaderStage_Vertex,
                .buffer.type = WGPUBufferBindingType_ReadOnlyStorage,
            },
            [2] = {
                .binding = 2,
                .visibility = WGPUShaderStage_Vertex,
                .buffer.type = WGPUBufferBindingType_ReadOnlyStorage,
            },
            [3] = {
                .binding = 3,
                .visibility = WGPUShaderStage_Vertex,
                .buffer.type = WGPUBufferBindingType_ReadOnlyStorage,
            }
        }
    });
    if (computeBindGroup) {
        wgpuBindGroupRelease(computeBindGroup);
    }
    if (pipelineBindGroup) {
        wgpuBindGroupRelease(pipelineBindGroup);
    }
    computeBindGroup = wgpuDeviceCreateBindGroup(app->device, &(WGPUBindGroupDescriptor) {
        .layout = computeBindLayout,
        .entryCount = 5,
        .entries = (WGPUBindGroupEntry[]) {
            [0] = {
                .binding = 0,
                .buffer = uniformBuffer,
                .offset = 0,
                .size = sizeof(Uniform),
            },
            [1] = {
                .binding = 1,
                .buffer = sortUniformBuffer,
                .offset = 0,
                .size = sizeof(SortUniform),
            },
            [2] = {
                .binding = 2,
                .buffer = splatsBuffer,
                .offset = 0,
                .size = wgpuBufferGetSize(splatsBuffer),
            },
            [3] = {
                .binding = 3,
                .buffer = transformedPosBuffer,
                .offset = 0,
                .size = wgpuBufferGetSize(transformedPosBuffer),
            },
            [4] = {
                .binding = 4,
                .buffer = sortedIndexBuffer,
                .offset = 0,
                .size = wgpuBufferGetSize(sortedIndexBuffer),
            }

        },
        .label = "Bind Group 0",
    });
    pipelineBindGroup = wgpuDeviceCreateBindGroup(app->device, &(WGPUBindGroupDescriptor) {
        .layout = pipelineBindLayout,
        .entryCount = 4,
        .entries = (WGPUBindGroupEntry[]) {
            [0] = {
                .binding = 0,
                .buffer = uniformBuffer,
                .offset = 0,
                .size = sizeof(Uniform),
            },
            [1] = {
                .binding = 1,
                .buffer = splatsBuffer,
                .offset = 0,
                .size = wgpuBufferGetSize(splatsBuffer),
            },
            [2] = {
                .binding = 2,
                .buffer = transformedPosBuffer,
                .offset = 0,
                .size = wgpuBufferGetSize(transformedPosBuffer),
            },
            [3] = {
                .binding = 3,
                .buffer = sortedIndexBuffer,
                .offset = 0,
                .size = wgpuBufferGetSize(sortedIndexBuffer),
            }

        },
        .label = "Bind Group 1",
    });

    if (computeLayout) {
        wgpuPipelineLayoutRelease(computeLayout);
    }
    computeLayout = wgpuDeviceCreatePipelineLayout(app->device, &(WGPUPipelineLayoutDescriptor) {
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = (WGPUBindGroupLayout[]) {
            computeBindLayout,
        },
        .label = "Compute Pipeline",

    });

    if (pipelineLayout) {
        wgpuPipelineLayoutRelease(pipelineLayout);
    }
    pipelineLayout = wgpuDeviceCreatePipelineLayout(app->device, &(WGPUPipelineLayoutDescriptor) {
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = (WGPUBindGroupLayout[]) {
            pipelineBindLayout,
        },
        .label = "Pipeline Layout",
    });
    wgpuBindGroupLayoutRelease(computeBindLayout);
    wgpuBindGroupLayoutRelease(pipelineBindLayout);

    if (transformPipeline) {
        wgpuComputePipelineRelease(transformPipeline);
    }
    transformPipeline = wgpuDeviceCreateComputePipeline(app->device, &(WGPUComputePipelineDescriptor) {
        .layout = computeLayout,
        .compute = {
            .module = computeShaderModule,
            .entryPoint = "transform_main",
        }
    });

    if (sortPipeline) {
        wgpuComputePipelineRelease(sortPipeline);
    }
    sortPipeline = wgpuDeviceCreateComputePipeline(app->device, &(WGPUComputePipelineDescriptor) {
        .layout = computeLayout,
        .compute = {
            .module = computeShaderModule,
            .entryPoint = "sort_main",
        }
    });

    if (renderPipeline) {
        wgpuRenderPipelineRelease(renderPipeline);
    }
    renderPipeline = wgpuDeviceCreateRenderPipeline(app->device, &(WGPURenderPipelineDescriptor) {
        .layout = pipelineLayout,
        .primitive.topology = WGPUPrimitiveTopology_TriangleStrip,
        .primitive.stripIndexFormat = WGPUIndexFormat_Undefined,
        .primitive.frontFace = WGPUFrontFace_CCW,
        .primitive.cullMode = WGPUCullMode_None,
        .vertex.module = renderShaderModule,
        .vertex.bufferCount = 0,
        .vertex.entryPoint = "vs_main",
        .fragment = &(WGPUFragmentState) {
            .module = renderShaderModule,
            .entryPoint = "fs_main",
            .targetCount = 1,
            .targets = (WGPUColorTargetState[]) {
                [0].format = app->format,
                [0].writeMask = WGPUColorWriteMask_All,
                [0].blend = &(WGPUBlendState) {
                    .color = {
                        .operation = WGPUBlendOperation_Add,
                        .srcFactor = WGPUBlendFactor_SrcAlpha,
                        .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
                    },
                    .alpha = {
                        .operation = WGPUBlendOperation_Add,
                        .srcFactor = WGPUBlendFactor_One,
                        .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
                    },
                }
            }
        },
        .depthStencil = NULL,
        .multisample.count = 1,
        .multisample.mask = ~0u,
        .multisample.alphaToCoverageEnabled = false,
    });
}

void render(const AppState *app, float dt) {
    static bool cameraUpdated = true;
    if (inputIsKeyPressed(GLFW_KEY_ESCAPE)) {
        exit(0);
    }
    bool mouseCaptured = igGetIO()->WantCaptureMouse;
    bool keyboardCaptured = igGetIO()->WantCaptureKeyboard;
    if (!mouseCaptured && inputIsButtonDown(GLFW_MOUSE_BUTTON_LEFT)) {
        const float mouseSpeed = 10.0f;
        vec2 mouseDelta;
        inputGetMouseDelta(mouseDelta);
        glm_vec2_scale(mouseDelta, mouseSpeed * dt, mouseDelta);
        mouseDelta[1] = - mouseDelta[1];

        arcballCameraRotate(&camera, mouseDelta);
        cameraUpdated = true;
    }
    vec2 wheelDelta;
    inputGetMouseWheelDelta(wheelDelta);
    if (!mouseCaptured && wheelDelta[1] != 0.0) {
        arcballCameraZoom(&camera, -wheelDelta[1] * 0.2f);
        cameraUpdated = true;
    }

    static bool changeSplat = true;
    static int splatIdx = 0;

    if (changeSplat) {
        loadSplat(app, splatFiles[splatIdx]);
        changeSplat = false;
        cameraUpdated = true;
    }
    arcballCameraUpdate(&camera);

    static bool gpuSort = true;
    static bool alwaysSort = false;


    static Uniform uniform = {
        .scale = 0.125f,
    };
    glm_mat4_copy(camera.viewProj, uniform.viewProj);

    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(app->device, &(WGPUCommandEncoderDescriptor) {
        .nextInChain = NULL,
        .label = "My Command Encoder",
    });

    wgpuQueueWriteBuffer(queue, uniformBuffer, 0, &uniform, sizeof(uniform));

    if (gpuSort && (alwaysSort || cameraUpdated)) {
        // Transform pass
        WGPUComputePassEncoder transformPass = wgpuCommandEncoderBeginComputePass(encoder, NULL);
        wgpuComputePassEncoderSetPipeline(transformPass, transformPipeline);
        wgpuComputePassEncoderSetBindGroup(transformPass, 0, computeBindGroup, 0, NULL);
        wgpuComputePassEncoderDispatchWorkgroups(transformPass, (numSplats + 255) / 256, 1, 1);
        wgpuComputePassEncoderEnd(transformPass);
        wgpuComputePassEncoderRelease(transformPass);
        // Sort pass
        uint32_t uniformCount = 0;
        for (uint32_t k = 2; (k >> 1) < numSplats; k <<= 1) {
            uniformCount++;
            for (uint32_t j = k >> 1; 0 < j; j >>= 1) {
                uniformCount++;
            }
        }
        SortUniform sortUniforms[uniformCount];
        uint32_t uniformIndex = 0;
        for (uint32_t k = 2; (k >> 1) < numSplats; k <<= 1) {
            sortUniforms[uniformIndex++] = (SortUniform) {k - 1};
            for (uint32_t j = k >> 1; 0 < j; j >>= 1) {
                sortUniforms[uniformIndex++] = (SortUniform) {j};
            }
        }
        assert(uniformIndex == uniformCount);
        assert(uniformCount * sizeof(SortUniform) <= wgpuBufferGetSize(stagingSortUniformBuffer));
        wgpuQueueWriteBuffer(queue, stagingSortUniformBuffer, 0, sortUniforms, uniformCount * sizeof(SortUniform));

        for (uint32_t i = 0; i < uniformCount; i++) {
            uint32_t offset = i * sizeof(SortUniform);
            wgpuCommandEncoderCopyBufferToBuffer(encoder, stagingSortUniformBuffer, offset, sortUniformBuffer, 0, sizeof(SortUniform));
            WGPUComputePassEncoder computePass = wgpuCommandEncoderBeginComputePass(encoder, NULL);
            wgpuComputePassEncoderSetPipeline(computePass, sortPipeline);
            wgpuComputePassEncoderSetBindGroup(computePass, 0, computeBindGroup, 0, NULL);
            uint32_t workgroups = (numSplats + 255) / 256;
            wgpuComputePassEncoderDispatchWorkgroups(computePass, workgroups, 1, 1);
            wgpuComputePassEncoderEnd(computePass);
            wgpuComputePassEncoderRelease(computePass);
        }
    }
    if (!gpuSort && (alwaysSort || cameraUpdated)) {
        for (int32_t i = 0; i < numSplats; i++) {
            Splat *splat = splats + i;
            vec4 pos = {splat->pos[0], splat->pos[1], splat->pos[2], 1.0f};
            glm_mat4_mulv(camera.viewProj, pos, transformedPos[i]);
        }
        for (int i = 0; i < numSplats; i++) {
            sortedIndex[i] = i;
        }
        qsort(sortedIndex, numSplats, sizeof(*sortedIndex), cmpTransformedPosZ);

        wgpuQueueWriteBuffer(queue, transformedPosBuffer, 0, transformedPos, numSplats * sizeof(*transformedPos));
        wgpuQueueWriteBuffer(queue, sortedIndexBuffer, 0, sortedIndex, numSplats * sizeof(*sortedIndex));
    }
    cameraUpdated = false;

    // Render pass
    {
        WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &(WGPURenderPassDescriptor) {
            .nextInChain = NULL,
            .colorAttachmentCount = 1,
            .colorAttachments = &(WGPURenderPassColorAttachment) {
                .view = app->view,
                .loadOp = WGPULoadOp_Clear,
                .storeOp = WGPUStoreOp_Store,
                .clearValue = {
                    .r = 1.0f,
                    .g = 1.0f,
                    .b = 1.0f,
                    .a = 1.0f
                },
            },
            .depthStencilAttachment = NULL,
            .timestampWrites = NULL,

        });

        wgpuRenderPassEncoderSetPipeline(renderPass, renderPipeline);
        wgpuRenderPassEncoderSetBindGroup(renderPass, 0, pipelineBindGroup, 0, NULL);
        //wgpuRenderPassEncoderSetVertexBuffer(renderPass, 0, vertexInBuffer, 0, wgpuBufferGetSize(vertexInBuffer));
        wgpuRenderPassEncoderDraw(renderPass, 4, numSplats, 0, 0);



        igBegin("GaussianSplatting", NULL, 0);
        igSeparator();
        igText("==========Config==========");
        igSliderFloat("Splat size", &uniform.scale, 0.01f, 1.0f, "%.2f", 0);
        igSliderFloat3("Camera center", camera.center, -10.0f, 10.0f, "%.2f", 0);
        changeSplat = igCombo_Str("Splat file", &splatIdx, splatFiles[0], 0);
        igCheckbox("GPU Sort", &gpuSort);
        igCheckbox("Always Sort", &alwaysSort);
        igSeparator();
        igText("==========Performance==========");
        igEnd();

        igRender();
        ImGui_ImplWGPU_RenderDrawData(igGetDrawData(), renderPass);

        wgpuRenderPassEncoderEnd(renderPass);
        wgpuRenderPassEncoderRelease(renderPass);
    }

    // Encode and submit
    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &(WGPUCommandBufferDescriptor) {
        .nextInChain = NULL,
        .label = "My Command Buffer",
    });

    wgpuQueueSubmit(queue, 1, &command);
    wgpuCommandBufferRelease(command);

    wgpuCommandEncoderRelease(encoder);
}

AppConfig appMain() {
     return (AppConfig) {
        .width = 1280,
        .height = 720,
        .title = "GuassianSplatting",
        .init = (AppInitFn) init,
        .deinit = (AppDeInitFn) deinit,
        .render = (AppRenderFn) render,
    };
}
