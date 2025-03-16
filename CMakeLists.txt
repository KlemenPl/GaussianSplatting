cmake_minimum_required(VERSION 3.15)
project(GaussianSplatting)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -pedantic)

add_subdirectory(libs/cglm)
add_subdirectory(libs/webgpu)
add_subdirectory(libs/glfw3webgpu)
include(libs/cimgui.cmake)

# GLFW library
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
if (EMSCRIPTEN)
    add_library(glfw INTERFACE)
    target_link_options(glfw INTERFACE -sUSE_GLFW=3 -sUSE_WEBGPU)
else ()
    add_subdirectory(libs/glfw3)
endif ()

add_executable(GaussianSplatting
        src/app.h
        src/camera.h
        src/imgui.h
        src/input.c
        src/input.h
        src/main.c
        src/utils.c
        src/utils.h
        src/webgpu-utils.c
        src/webgpu-utils.h
)

target_compile_options(GaussianSplatting PRIVATE -Wall -Wextra -pedantic)

target_link_libraries(GaussianSplatting PRIVATE webgpu cglm glfw glfw3webgpu)
target_link_libraries(GaussianSplatting PRIVATE ${CIMGUI_LIBRARY})
target_compile_definitions(GaussianSplatting PRIVATE CIMGUI_USE_GLFW CIMGUI_USE_WGPU)
target_copy_webgpu_binaries(GaussianSplatting)

if (EMSCRIPTEN)
    # Generate a full web page rather than a simple WebAssembly module
    set_target_properties(GaussianSplatting PROPERTIES
            SUFFIX ".html"
            LINK_FLAGS "-lidbfs.js -g -gseparate-dwarf -gsource-map -sASSERTIONS=3 --shell-file ../index.html -sFORCE_FILESYSTEM=1 -sWASM=1 -sALLOW_MEMORY_GROWTH -sSTACK_SIZE=10MB -sUSE_GLFW=3 -sUSE_WEBGPU -sASYNCIFY --preload-file ../assets"
    )
endif()