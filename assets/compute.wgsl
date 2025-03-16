
struct Splat {
    @align(16) pos: vec3f,
    @align(16) scale: vec3f,
    @align(4) color: u32,
    @align(4) rotation: u32,
}

struct Uniforms {
    viewProj: mat4x4<f32>,
    scale: f32,
}

struct SortUniforms {
    @align(16) comparePattern: u32,
}

@group(0) @binding(0) var<uniform> cUniforms: Uniforms;
@group(0) @binding(1) var<uniform> cSortUniforms: SortUniforms;
@group(0) @binding(2) var<storage, read> cSplats: array<Splat>;
@group(0) @binding(3) var<storage, read_write> cTransformedPos: array<vec4f>;
@group(0) @binding(4) var<storage, read_write> cSorted: array<u32>;


@compute @workgroup_size(256)
fn transform_main(@builtin(global_invocation_id) id: vec3u) {
    if (id.x >= arrayLength(&cSplats)) {
        return;
    }
    var splat = cSplats[id.x];
    cTransformedPos[id.x] = cUniforms.viewProj * vec4f(splat.pos, 1.0);
    cSorted[id.x] = id.x;
}


fn sort_cmp_and_swap(i: u32, j: u32) {
    let n = arrayLength(&cSplats);
    if (j >= n) {
        return;
    }
    let iIdx = cSorted[i];
    let jIdx = cSorted[j];
    if (i < j && cTransformedPos[iIdx].z < cTransformedPos[jIdx].z) {
        let tmp = cSorted[i];
        cSorted[i] = cSorted[j];
        cSorted[j] = tmp;
    }
}
@compute @workgroup_size(256)
fn sort_main(@builtin(global_invocation_id) id: vec3u) {
    let i = id.x;
    if (i >= arrayLength(&cSplats)) {
        return;
    }
    let j = i ^ cSortUniforms.comparePattern;
    sort_cmp_and_swap(i, j);
}

