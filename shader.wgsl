struct Splat {
    @align(16) pos: vec3f,
    @align(16) scale: vec3f,
    @align(4) color: u32,
    @align(4) rotation: u32,
}

struct VertexOutput {
    @builtin(position) pos: vec4f,
    @location(0) offset: vec2f,
    @location(1) scale: vec3f,
    @location(2) depth: f32,
    @location(3) color: u32,
    @location(4) rotation: u32,
}

struct Uniforms {
    viewProj: mat4x4<f32>,
    scale: f32,
}
struct SortUniforms {
    @align(16) comparePattern: u32,
}

@group(0) @binding(0) var<uniform> uniforms: Uniforms;
@group(0) @binding(1) var<uniform> sortUniforms: SortUniforms;
@group(0) @binding(2) var<storage, read> splats: array<Splat>;
@group(0) @binding(3) var<storage, read_write> transformedPos: array<vec4f>;
@group(0) @binding(4) var<storage, read_write> sorted: array<u32>;


@compute @workgroup_size(256)
fn transform_main(@builtin(global_invocation_id) id: vec3u) {
    if (id.x >= arrayLength(&splats)) {
        return;
    }
    var splat = splats[id.x];
    transformedPos[id.x] = uniforms.viewProj * vec4f(splat.pos, 1.0);
    sorted[id.x] = id.x;
}


fn sort_cmp_and_swap(i: u32, j: u32) {
    let n = arrayLength(&splats);
    if (j >= n) {
        return;
    }
    let iIdx = sorted[i];
    let jIdx = sorted[j];
    if (i < j && transformedPos[iIdx].z < transformedPos[jIdx].z) {
        let tmp = sorted[i];
        sorted[i] = sorted[j];
        sorted[j] = tmp;
    }
}
@compute @workgroup_size(256)
fn sort_main(@builtin(global_invocation_id) id: vec3u) {
    let i = id.x;
    if (i >= arrayLength(&splats)) {
        return;
    }
    let j = i ^ sortUniforms.comparePattern;
    sort_cmp_and_swap(i, j);
}


@vertex
fn vs_main(
    @builtin(vertex_index) vIdx: u32,
    @builtin(instance_index) iIdx: u32,
) -> VertexOutput {
    var quad = array(
        vec2f(1, -1),
        vec2f(1, 1),
        vec2f(-1, -1),
        vec2f(-1, 1),
    );
    let sIdx = sorted[iIdx];
    let splat = splats[sIdx];
    let pos = transformedPos[sIdx];
    let s = uniforms.scale;
    let z = pos.z;

    var out: VertexOutput;
    out.pos = pos + vec4f(quad[vIdx] * (s / z), 0.0, 0.0);
    out.offset = pos.xy - out.pos.xy;
    out.scale = splat.scale;
    out.depth = z / s;
    out.color = splat.color;
    out.rotation = splat.rotation;
    //out.pos.w = 0.0;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let r = f32((in.color >> 0) & 0xff) / 255.0f;
    let g = f32((in.color >> 8) & 0xff) / 255.0f;
    let b = f32((in.color >> 16) & 0xff) / 255.0f;
    let a = f32((in.color >> 24) & 0xff) / 255.0f;
    let color = vec4f(r, g, b, a);

    let offset = sqrt(dot(in.offset, in.offset));
    let sigma = in.depth; // s / z
    let gaus = exp(-0.5 * offset * offset * sigma);
    let finalAlpha = gaus * a;


    return vec4f(color.rgb * finalAlpha, finalAlpha);
}
