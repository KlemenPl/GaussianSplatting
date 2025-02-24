cmake_minimum_required(VERSION 3.30)
project(GaussianSplatting)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -pedantic)

add_subdirectory(cglm)
add_subdirectory(webgpu)
add_subdirectory(glfw3webgpu)

# GLFW library
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
if (EMSCRIPTEN)
    add_library(glfw INTERFACE)
    target_link_options(glfw INTERFACE -sUSE_GLFW=3)
else ()
    add_subdirectory(glfw3)
endif ()

add_executable(GaussianSplatting
        src/app.h
        src/camera.h
        src/input.c
        src/input.h
        src/main.c
        src/utils.c
        src/utils.h
        src/wgpu-utils.c
        src/wgpu-utils.h
)

target_link_libraries(GaussianSplatting PRIVATE webgpu cglm glfw glfw3webgpu)
target_copy_webgpu_binaries(GaussianSplatting)

if (EMSCRIPTEN)
    # Generate a full web page rather than a simple WebAssembly module
    set_target_properties(GaussianSplatting PROPERTIES SUFFIX ".html")
endif()