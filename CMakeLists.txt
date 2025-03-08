cmake_minimum_required(VERSION 3.30)
project(GaussianSplatting)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -pedantic)

add_subdirectory(libs/cglm)
add_subdirectory(libs/webgpu)
include(libs/cimgui.cmake)

if (WIN32)
    add_definitions(-DGLFW_EXPOSE_NATIVE_WIN32)
    set(OS_LIBRARIES d3dcompiler ws2_32 userenv bcrypt ntdll opengl32 Propsys RuntimeObject)
elseif(UNIX AND NOT APPLE)
    add_definitions(-DGLFW_EXPOSE_NATIVE_X11)
    add_definitions(-DGLFW_EXPOSE_NATIVE_WAYLAND)
    set(OS_LIBRARIES "-lm -ldl")
elseif(APPLE)
    add_definitions(-DGLFW_EXPOSE_NATIVE_COCOA)
    set(OS_LIBRARIES "-framework CoreFoundation -framework QuartzCore -framework Metal")
endif()

# GLFW library
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
if (EMSCRIPTEN)
    add_library(glfw INTERFACE)
    target_link_options(glfw INTERFACE -sUSE_GLFW=3)
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
        src/wgpu-utils.c
        src/wgpu-utils.h
)

target_link_libraries(GaussianSplatting PRIVATE webgpu cglm glfw ${OS_LIBRARIES})
target_link_libraries(GaussianSplatting PRIVATE ${CIMGUI_LIBRARY})
target_compile_definitions(GaussianSplatting PRIVATE CIMGUI_USE_GLFW, CIMGUI_USE_WGPU)
target_copy_webgpu_binaries(GaussianSplatting)

if (EMSCRIPTEN)
    # Generate a full web page rather than a simple WebAssembly module
    set_target_properties(GaussianSplatting PROPERTIES SUFFIX ".html")
endif()