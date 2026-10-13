// Bloom upsample: blurs the next smaller level up with a 3x3 tent filter and adds this level's
// downsample result, so every level contributes to the final glow.

// Per-pass constants (one dynamic-offset slot per pass).
struct PostParams {
    texel: vec4f,    // xy: 1 / source size, z: filter radius in source texels, w: 1 on the first downsample
    settings: vec4f, // first downsample: x threshold, y knee. Composite: x exposure, y bloom intensity, z 1 / level count
};

@group(0) @binding(0) var<uniform> Params: PostParams;
@group(0) @binding(1) var linearSampler: sampler;

@group(1) @binding(0) var source: texture_2d<f32>;  // the next smaller level
@group(1) @binding(1) var current: texture_2d<f32>; // this level's downsample result

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
};

// One triangle that covers the whole target.
@vertex
fn vertexMain(@builtin(vertex_index) index: u32) -> VertexOutput {
    let p = vec2f(f32((index << 1u) & 2u), f32(index & 2u));
    var out: VertexOutput;
    out.position = vec4f(p * 2.0 - 1.0, 0.0, 1.0);
    out.uv = vec2f(p.x, 1.0 - p.y);
    return out;
}

fn tap(uv: vec2f) -> vec3f {
    return textureSampleLevel(source, linearSampler, uv, 0.0).rgb;
}

// 3x3 tent (weights 1 2 1 / 2 4 2 / 1 2 1) with taps `Params.texel.z` source texels apart.
fn tent(uv: vec2f) -> vec3f {
    let d = Params.texel.xy * Params.texel.z;
    var c = tap(uv) * 4.0;
    c += (tap(uv + vec2f(-d.x, 0.0)) + tap(uv + vec2f(d.x, 0.0)) + tap(uv + vec2f(0.0, -d.y)) + tap(uv + vec2f(0.0, d.y))) * 2.0;
    c += tap(uv + vec2f(-d.x, -d.y)) + tap(uv + vec2f(d.x, -d.y)) + tap(uv + vec2f(-d.x, d.y)) + tap(uv + vec2f(d.x, d.y));
    return c / 16.0;
}

@fragment
fn fragmentMain(input: VertexOutput) -> @location(0) vec4f {
    let down = textureSampleLevel(current, linearSampler, input.uv, 0.0).rgb;
    return vec4f(down + tent(input.uv), 1.0);
}
