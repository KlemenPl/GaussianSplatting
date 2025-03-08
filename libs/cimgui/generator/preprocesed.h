
struct ImGui_ImplWGPU_InitInfo
{
    WGPUDevice Device;
    int NumFramesInFlight = 3;
    WGPUTextureFormat RenderTargetFormat = WGPUTextureFormat_Undefined;
    WGPUTextureFormat DepthStencilFormat = WGPUTextureFormat_Undefined;
    WGPUMultisampleState PipelineMultisampleState = {};
    ImGui_ImplWGPU_InitInfo()
    {
        PipelineMultisampleState.count = 1;
        PipelineMultisampleState.mask =                                        (4294967295U)                                                 ;
        PipelineMultisampleState.alphaToCoverageEnabled = false;
    }
};
 bool ImGui_ImplWGPU_Init(ImGui_ImplWGPU_InitInfo* init_info);
 void ImGui_ImplWGPU_Shutdown();
 void ImGui_ImplWGPU_NewFrame();
 void ImGui_ImplWGPU_RenderDrawData(ImDrawData* draw_data, WGPURenderPassEncoder pass_encoder);
 bool ImGui_ImplWGPU_CreateDeviceObjects();
 void ImGui_ImplWGPU_InvalidateDeviceObjects();
struct ImGui_ImplWGPU_RenderState
{
    WGPUDevice Device;
    WGPURenderPassEncoder RenderPassEncoder;
};