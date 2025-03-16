//
// Created by klemen on 3/16/25.
//

#ifndef WEBGPU_H
#define WEBGPU_H

// Note: For some reason enum mapping differs between native and emscripten...
#ifdef __EMSCRIPTEN__
#include "webgpu_emscripten.h"
#else
#include "webgpu_desktop.h"
#endif

#endif //WEBGPU_H
