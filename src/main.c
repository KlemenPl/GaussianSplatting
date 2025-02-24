#include <assert.h>
#include <stdlib.h>

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

uint32_t numSplats;

WGPUQueue queue;

WGPUBindGroupLayout bindGroupLayout;
WGPUBindGroup bindGroup;
WGPUPipelineLayout pipelineLayout;

WGPUComputePipeline transformPipeline;
WGPUComputePipeline sortPipeline;

WGPUBuffer uniformBuffer;
WGPUBuffer sortUniformBuffer;
WGPUBuffer splatsBuffer;
WGPUBuffer transformedPosBuffer;
WGPUBuffer sortedIndexBuffer;
WGPURenderPipeline renderPipeline;

int init(const AppState *app, int argc, const char **argv) {
    assert(argc == 2 && "Usage: ./program input_file");
    const char *input = argv[1];
    FILE *f = fopen(input, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open file %s\n", input);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    size_t fileSize = ftell(f);
    numSplats = fileSize / sizeof(SplatRaw);
    assert(fileSize % sizeof(SplatRaw) == 0 && "Invalid file length");
    Splat *points = malloc(numSplats * sizeof(*points));
    fseek(f, 0, SEEK_SET);
    vec3 center = {0, 0, 0};
    for (size_t i = 0, idx = 0; i < fileSize; i += sizeof(SplatRaw), idx++) {
        SplatRaw splatRaw;
        fread(&splatRaw, sizeof(splatRaw), 1, f);
        glm_vec3_add(center, splatRaw.pos, center);

        glm_vec3_copy(splatRaw.pos, points[idx].pos);
        glm_vec3_copy(splatRaw.scale, points[idx].scale);
        points[idx].color = splatRaw.color;
        points[idx].rotation = splatRaw.rotation;
    }
    glm_vec3_div(center, (vec3){numSplats, numSplats, numSplats}, camera.center);
    fclose(f);
    printf("Loaded %s (%ld points)\n", argv[1], numSplats);


    queue = wgpuDeviceGetQueue(app->device);

    splatsBuffer = wgpuDeviceCreateBuffer(app->device, &(WGPUBufferDescriptor) {
        .label = "Splats Buffer",
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex | WGPUBufferUsage_Storage,
        .size = numSplats * sizeof(Splat),
        .mappedAtCreation = false
    });
    wgpuQueueWriteBuffer(queue, splatsBuffer, 0, points, numSplats * sizeof(Splat));
    free(points);

    transformedPosBuffer = wgpuDeviceCreateBuffer(app->device, &(WGPUBufferDescriptor) {
        .label = "Transformed Positions",
        .usage = WGPUBufferUsage_Storage | WGPUBufferUsage_Vertex | WGPUBufferUsage_CopySrc,
        .size = numSplats * sizeof(vec4),
    });
    sortedIndexBuffer = wgpuDeviceCreateBuffer(app->device, &(WGPUBufferDescriptor) {
        .label = "Sorted Indices",
        .usage = WGPUBufferUsage_Storage | WGPUBufferUsage_Vertex | WGPUBufferUsage_CopySrc,
        .size = numSplats * sizeof(uint32_t),
    });

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

    bindGroupLayout = wgpuDeviceCreateBindGroupLayout(app->device, &(WGPUBindGroupLayoutDescriptor) {
        .entryCount = 5,
        .entries = (WGPUBindGroupLayoutEntry[]) {
            [0] = {
                .binding = 0,
                .visibility = WGPUShaderStage_Compute | WGPUShaderStage_Vertex,
                .buffer = {
                    .type = WGPUBufferBindingType_Uniform,
                    .minBindingSize = sizeof(Uniform),
                },
            },
            [1] = {
                .binding = 1,
                .visibility = WGPUShaderStage_Compute,
                .buffer = {
                    .type = WGPUBufferBindingType_Uniform,
                    .minBindingSize = sizeof(SortUniform),
                }
            },
            [2] = {
                .binding = 2,
                .visibility = WGPUShaderStage_Compute | WGPUShaderStage_Vertex,
                .buffer = {
                    .type = WGPUBufferBindingType_ReadOnlyStorage,
                    .minBindingSize = wgpuBufferGetSize(splatsBuffer),
                }
            },
            [3] = {
                .binding = 3,
                .visibility = WGPUShaderStage_Compute | WGPUShaderStage_Vertex,
                .buffer = {
                    .type = WGPUBufferBindingType_Storage,
                    .minBindingSize = wgpuBufferGetSize(transformedPosBuffer),
                }
            },
            [4] = {
                .binding = 4,
                .visibility = WGPUShaderStage_Compute | WGPUShaderStage_Vertex,
                .buffer = {
                    .type = WGPUBufferBindingType_Storage,
                    .minBindingSize = wgpuBufferGetSize(sortedIndexBuffer),
                }
            },
        },
        .label = "Bind Group Layout",
    });


    bindGroup = wgpuDeviceCreateBindGroup(app->device, &(WGPUBindGroupDescriptor) {
        .layout = bindGroupLayout,
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
        .label = "Bind Group",
    });

    pipelineLayout = wgpuDeviceCreatePipelineLayout(app->device, &(WGPUPipelineLayoutDescriptor) {
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = (WGPUBindGroupLayout[]) {
            bindGroupLayout,
        },
        .label = "Pipeline Layout",
    });
    const char *shaderSource = readFile("shader.wgsl");
    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(app->device, &(WGPUShaderModuleDescriptor) {
        .nextInChain = (WGPUChainedStruct*) &(WGPUShaderModuleWGSLDescriptor) {
            .chain.sType = WGPUSType_ShaderModuleWGSLDescriptor,
            .code = shaderSource,
        },
    });
    free(shaderSource);

    transformPipeline = wgpuDeviceCreateComputePipeline(app->device, &(WGPUComputePipelineDescriptor) {
        .layout = pipelineLayout,
        .compute = {
            .module = shaderModule,
            .entryPoint = "transform_main",
        }
    });
    sortPipeline = wgpuDeviceCreateComputePipeline(app->device, &(WGPUComputePipelineDescriptor) {
        .layout = pipelineLayout,
        .compute = {
            .module = shaderModule,
            .entryPoint = "sort_main",
        }
    });

    renderPipeline = wgpuDeviceCreateRenderPipeline(app->device, &(WGPURenderPipelineDescriptor) {
        .layout = pipelineLayout,
        .primitive.topology = WGPUPrimitiveTopology_TriangleStrip,
        .primitive.stripIndexFormat = WGPUIndexFormat_Undefined,
        .primitive.frontFace = WGPUFrontFace_CCW,
        .primitive.cullMode = WGPUCullMode_None,
        .vertex.module = shaderModule,
        .vertex.bufferCount = 0,
        .vertex.entryPoint = "vs_main",
        .fragment = &(WGPUFragmentState) {
            .module = shaderModule,
            .entryPoint = "fs_main",
            .targetCount = 1,
            .targets = (WGPUColorTargetState[]) {
                [0].format = app->surfaceFormat,
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
    wgpuShaderModuleRelease(shaderModule);
    return 0;
}
void deinit(const AppState *app) {
    wgpuBindGroupLayoutRelease(bindGroupLayout);
    wgpuBindGroupRelease(bindGroup);
    wgpuPipelineLayoutRelease(pipelineLayout);

    wgpuBufferRelease(sortUniformBuffer);
    wgpuBufferRelease(uniformBuffer);
    wgpuBufferRelease(sortedIndexBuffer);
    wgpuBufferRelease(transformedPosBuffer);
    wgpuBufferRelease(splatsBuffer);

    wgpuComputePipelineRelease(transformPipeline);
    wgpuComputePipelineRelease(sortPipeline);
    wgpuRenderPipelineRelease(renderPipeline);
    wgpuQueueRelease(queue);
}

void compare(vec4 *positions, uint32_t *indices, uint32_t n, uint32_t i, uint32_t j) {
    if (j >= n) return;
    uint32_t iIdx = indices[i];
    uint32_t jIdx = indices[j];
    if (i < j && positions[jIdx][2] < positions[iIdx][2]) {
        uint32_t tmp = indices[i];
        indices[i] = indices[j];
        indices[j] = tmp;
    }
}
void sortCPU(vec4 *positions, uint32_t *indices, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        indices[i] = i;
    }
    for (uint32_t k = 2; (k >> 1) < n; k <<= 1) {
        for (uint32_t i = 0; i < n; i++)
            compare(positions, indices, n, i, i ^ (k - 1));
        for (uint32_t j = k >> 1; 0 < j; j >>= 1) {
            for (uint32_t i = 0; i < n; i++)
                compare(positions, indices, n, i, i ^ j);
        }
    }

}

bool stagingMapped = false;
WGPUBuffer stagingBuffer = 0;
void mapBuffer(WGPUBufferMapAsyncStatus status, void *userData) {
    uint32_t posSize = wgpuBufferGetSize(transformedPosBuffer);
    uint32_t indicesSize = wgpuBufferGetSize(sortedIndexBuffer);
    const void *data = wgpuBufferGetMappedRange(stagingBuffer, 0, posSize + indicesSize);
    vec4 *positions = (vec4 *)data;
    uint32_t *indices = (char *)data + posSize;

    uint32_t numIndices = indicesSize / sizeof(uint32_t);
    //sortCPU(positions, indices, numPositions);
    for (uint32_t i = 1; i < numIndices; i++) {
        uint32_t j = indices[i - 1];
        uint32_t k = indices[i];
        //assert(positions[i - 1][2] <= positions[i][2]);
        assert(positions[j][2] <= positions[k][2]);
    }

    wgpuBufferUnmap(stagingBuffer);
    stagingMapped = false;
}

void dispatchComputeSortPass(WGPUDevice device, uint32_t pattern) {
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, NULL);
    static uint32_t x = 0;
    SortUniform sortUniform = {
        .comparePattern = pattern,
    };
    wgpuQueueWriteBuffer(queue, sortUniformBuffer, 0, &sortUniform, sizeof(sortUniform));
    WGPUComputePassEncoder computePass = wgpuCommandEncoderBeginComputePass(encoder, NULL);
    wgpuComputePassEncoderSetPipeline(computePass, sortPipeline);
    wgpuComputePassEncoderSetBindGroup(computePass, 0, bindGroup, 0, NULL);
    uint32_t workgroups = (numSplats + 255) / 256;
    wgpuComputePassEncoderDispatchWorkgroups(computePass, workgroups, 1, 1);
    wgpuComputePassEncoderEnd(computePass);
    wgpuComputePassEncoderRelease(computePass);


    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, NULL);
    wgpuQueueSubmit(queue, 1, &command);
    wgpuCommandEncoderRelease(encoder);
}

void render(const AppState *app, float dt) {
    if (inputIsKeyPressed(GLFW_KEY_ESCAPE)) {
        exit(0);
    }
    if (inputIsButtonDown(GLFW_MOUSE_BUTTON_LEFT)) {
        const float mouseSpeed = 10.0f;
        vec2 mouseDelta;
        inputGetMouseDelta(mouseDelta);
        glm_vec2_scale(mouseDelta, mouseSpeed * dt, mouseDelta);

        arcballCameraRotate(&camera, mouseDelta);
    }
    //orbitCameraMove(&camera, 100 * dt, 0);
    vec2 wheelDelta;
    inputGetMouseWheelDelta(wheelDelta);
    if (wheelDelta[1] != 0.0) {
        arcballCameraZoom(&camera, -wheelDelta[1] * 0.2f);
    }

    arcballCameraUpdate(&camera);

    //glm_mat4_identity(camera.proj);
    //glm_mat4_transpose(camera.viewProj);
    Uniform uniform = {
        .scale = 0.025f,
    };
    glm_mat4_copy(camera.viewProj, uniform.viewProj);

    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(app->device, &(WGPUCommandEncoderDescriptor) {
        .nextInChain = NULL,
        .label = "My Command Encoder",
    });

    wgpuQueueWriteBuffer(queue, uniformBuffer, 0, &uniform, sizeof(uniform));
    // Transform pass
    if (1) {
        WGPUComputePassEncoder transformPass = wgpuCommandEncoderBeginComputePass(encoder, NULL);
        wgpuComputePassEncoderSetPipeline(transformPass, transformPipeline);
        wgpuComputePassEncoderSetBindGroup(transformPass, 0, bindGroup, 0, NULL);
        wgpuComputePassEncoderDispatchWorkgroups(transformPass, (numSplats + 255) / 256, 1, 1);
        wgpuComputePassEncoderEnd(transformPass);
        wgpuComputePassEncoderRelease(transformPass);

        WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, NULL);
        wgpuQueueSubmit(queue, 1, &command);
        wgpuCommandEncoderRelease(encoder);
        encoder = wgpuDeviceCreateCommandEncoder(app->device, &(WGPUCommandEncoderDescriptor) {});
    }
    // Sort pass
    if (1) {

        uint32_t n = numSplats;
        for (uint32_t k = 2; (k >> 1) < n; k <<= 1) {
            dispatchComputeSortPass(app->device, (k - 1));
            for (uint32_t j = k >> 1; 0 < j; j >>= 1) {
                dispatchComputeSortPass(app->device, j);
            }
        }
        /*
        for (uint32_t k = 2; k <= n; k <<= 1) {
            for (uint32_t j = k >> 1; j > 0; j >>= 1) {
                printf("stage %u,%u\n", k, j);
            }
        }
        */

        /*
        uint32_t stages = 32 - __builtin_clz(n - 1); // ceil(log2(n))
        for (uint32_t stage = 0; stage < stages; stage++) {
            for (uint32_t pass = 0; pass <= stage; pass++) {
                const uint32_t seqLen = 1u << (1u + stage);
                const uint32_t cmpLen = seqLen >> (1u + pass);

                SortUniform sortUniform = {
                    .seqLen = seqLen,
                    .cpmLen = cmpLen,
                };
                wgpuQueueWriteBuffer(queue, sortUniformBuffer, 0, &sortUniform, sizeof(sortUniform));

                WGPUComputePassEncoder computePass = wgpuCommandEncoderBeginComputePass(encoder, NULL);
                wgpuComputePassEncoderSetPipeline(computePass, sortPipeline);
                wgpuComputePassEncoderSetBindGroup(computePass, 0, bindGroup, 0, NULL);
                uint32_t workgroups = (numSplats + 255) / 256;
                wgpuComputePassEncoderDispatchWorkgroups(computePass, workgroups, 1, 1);
                wgpuComputePassEncoderEnd(computePass);


#if 0
                printf("stage %u,%u:\n", stage, pass);
                printf("\tseqLen: %u\n", seqLen);
                printf("\tcmpLen: %u\n", cmpLen);
                for (uint32_t tId = 0; tId < n; tId++) {
                    uint32_t xored = tId ^ cmpLen;
                    if (xored > tId) {
                        printf("\tthread %u\n", tId);
                        printf("\t\ti: %u\n", tId);
                        printf("\t\tj: %u\n", xored);
                        printf("\t\tdir: %u\n", (tId & seqLen) == 0);
                    }
                }
#endif

            }
        }
        //exit(0);
        */

    }

    //printf("%ld %ld %ld\n", wgpuBufferGetSize(vertexBuffer), wgpuBufferGetSize(vertexInBuffer), 4 * numSplats * sizeof(SplatVertex));
    //wgpuCommandEncoderCopyBufferToBuffer(encoder, vertexBuffer, 0, vertexInBuffer, 0, wgpuBufferGetSize(vertexBuffer));

    // Render pass
    {
        WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &(WGPURenderPassDescriptor) {
            .nextInChain = NULL,
            .colorAttachmentCount = 1,
            .colorAttachments = &(WGPURenderPassColorAttachment) {
                .view = app->surfaceView,
                .resolveTarget = NULL,
                .loadOp = WGPULoadOp_Clear,
                .storeOp = WGPUStoreOp_Store,
                /*
                .clearValue.r = 0.7,
                .clearValue.g = 0.7,
                .clearValue.b = 0.8,
                .clearValue.a = 1.0,
                */
                .clearValue = {
                    .r = 1.0f,
                    .g = 1.0f,
                    .b = 1.0f,
                    .a = 1.0f
                },
#ifndef WEBGPU_BACKEND_WGPU
                .depthSlive = WGPU_DEPTH_SLICE_UNDEFINED,
#endif
            },
            .depthStencilAttachment = NULL,
            .timestampWrites = NULL,

        });

        wgpuRenderPassEncoderSetPipeline(renderPass, renderPipeline);
        wgpuRenderPassEncoderSetBindGroup(renderPass, 0, bindGroup, 0, NULL);
        //wgpuRenderPassEncoderSetVertexBuffer(renderPass, 0, vertexInBuffer, 0, wgpuBufferGetSize(vertexInBuffer));
        wgpuRenderPassEncoderDraw(renderPass, 4, numSplats, 0, 0);

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


#if 1

    size_t posSize = wgpuBufferGetSize(transformedPosBuffer);
    size_t indicesSize = wgpuBufferGetSize(sortedIndexBuffer);
    if (!stagingBuffer) {
        stagingBuffer = wgpuDeviceCreateBuffer(app->device, &(WGPUBufferDescriptor) {
            .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead,
            .size = posSize + indicesSize,
            .mappedAtCreation = false,
        });
    }

    if (!stagingMapped) {
        encoder = wgpuDeviceCreateCommandEncoder(app->device, NULL);
        wgpuCommandEncoderCopyBufferToBuffer(encoder, transformedPosBuffer, 0, stagingBuffer, 0, posSize);
        wgpuCommandEncoderCopyBufferToBuffer(encoder, sortedIndexBuffer, 0, stagingBuffer, posSize, indicesSize);
        WGPUCommandBuffer commands = wgpuCommandEncoderFinish(encoder, NULL);
        wgpuQueueSubmit(queue, 1, &commands);
        wgpuBufferMapAsync(stagingBuffer, WGPUMapMode_Read, 0, posSize + indicesSize, mapBuffer, NULL);
        stagingMapped = true;
    }
#endif

}

AppConfig appMain() {
     return (AppConfig) {
        .width = 640,
        .height = 480,
        .title = "GuassianSplatting",
        .init = init,
        .deinit = deinit,
        .render = render,
    };
}
