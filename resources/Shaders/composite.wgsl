// Final post-processing pass, rendered to the swapchain: mixes the bloom into the linear HDR
// scene, applies exposure and ACES tonemapping, and encodes to sRGB (the swapchain is UNORM).

// Per-pass constants (one dynamic-offset slot per pass).
struct PostParams {
    texel: vec4f,    // xy: 1 / bloom texture size, z: filter radius in bloom texels
    settings: vec4f, // x: exposure, y: bloom intensity, z: 1 / bloom level count
};

@group(0) @binding(0) var<uniform> Params: PostParams;
@group(0) @binding(1) var linearSampler: sampler;

@group(1) @binding(0) var sceneColor: texture_2d<f32>;
@group(1) @binding(1) var bloomColor: texture_2d<f32>;

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

fn tonemapACES(x: vec3f) -> vec3f {
    let a = 2.51;
    let b = 0.03;
    let c = 2.43;
    let d = 0.59;
    let e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), vec3f(0.0), vec3f(1.0));
}

fn linearToSrgb(c: vec3f) -> vec3f {
    let low = c * 12.92;
    let high = 1.055 * pow(c, vec3f(1.0 / 2.4)) - 0.055;
    return select(high, low, c <= vec3f(0.0031308));
}

fn bloomTap(uv: vec2f) -> vec3f {
    return textureSampleLevel(bloomColor, linearSampler, uv, 0.0).rgb;
}

// The half-resolution bloom, upsampled with the same 3x3 tent as the bloom chain.
fn bloom(uv: vec2f) -> vec3f {
    let d = Params.texel.xy * Params.texel.z;
    var c = bloomTap(uv) * 4.0;
    c += (bloomTap(uv + vec2f(-d.x, 0.0)) + bloomTap(uv + vec2f(d.x, 0.0)) + bloomTap(uv + vec2f(0.0, -d.y)) + bloomTap(uv + vec2f(0.0, d.y))) * 2.0;
    c += bloomTap(uv + vec2f(-d.x, -d.y)) + bloomTap(uv + vec2f(d.x, -d.y)) + bloomTap(uv + vec2f(-d.x, d.y)) + bloomTap(uv + vec2f(d.x, d.y));
    // The upsample chain sums every level; averaging keeps the strength independent of the level count.
    return c / 16.0 * Params.settings.z;
}

@fragment
fn fragmentMain(input: VertexOutput) -> @location(0) vec4f {
    let hdr = textureSampleLevel(sceneColor, linearSampler, input.uv, 0.0).rgb;

    // The bloom only holds light above the threshold, so it is added on top.
    var color = (hdr + bloom(input.uv) * Params.settings.y) * Params.settings.x;

    color = tonemapACES(color);
    return vec4f(linearToSrgb(color), 1.0);
}
