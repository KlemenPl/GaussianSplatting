struct Splat {
    @align(16) pos: vec3f,
    @align(16) scale: vec3f,
    @align(4) color: u32,
    @align(4) rotation: u32,
}

struct VertexOutput {
    @builtin(position) pos: vec4f,
    @location(0) @interpolate(perspective) offset: vec2f,
    @location(1) @interpolate(flat) scale: vec3f,
    @location(2) @interpolate(flat) depth: f32,
    @location(3) @interpolate(flat) color: u32,
    @location(4) @interpolate(flat) rotation: u32,
}

struct Uniforms {
    viewProj: mat4x4<f32>,
    scale: f32,
}

@group(0) @binding(0) var<uniform> uniforms: Uniforms;
@group(0) @binding(1) var<storage, read> splats: array<Splat>;
@group(0) @binding(2) var<storage, read> transformedPos: array<vec4f>;
@group(0) @binding(3) var<storage, read> sorted: array<u32>;


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
    let z = max(pos.z, 1.0);
    //let z = pos.z;

    var out: VertexOutput;
    out.pos = pos + vec4f(quad[vIdx] * (s / z), 0.0, 0.0);
    out.offset = quad[vIdx];
    //out.offset = quad[vIdx] * (s / z);
    out.scale = splat.scale;
    out.depth = s / z;
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
    let sigma = 1 / in.depth; // (s / z) ^ (-1)
    let gaus = exp(-0.5 * offset * offset * sigma);
    let finalAlpha = color.a * gaus;


    return vec4f(color.rgb,  finalAlpha);
}
