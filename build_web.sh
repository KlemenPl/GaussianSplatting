#!/bin/bash

emscripten() {
    docker run --rm -v $(pwd):/src -w /src -u $(id -u):$(id -g) emscripten/emsdk:latest "$@" ;
}

emcmake() {
    emscripten emcmake "$@"
}

emmake() {
    emscripten emmake "$@"
}

emcmake cmake -DPLATFORM=Web -DCMAKE_BUILD_TYPE=Release . -B build_web
emmake make -j $(nproc) -C build_web