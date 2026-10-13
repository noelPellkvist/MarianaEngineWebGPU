// Bloom downsample: one level of the chain, half the size of its source.
// 13-tap filter from "Next Generation Post Processing in Call of Duty: Advanced Warfare" (Jimenez 2014).

// Per-pass constants (one dynamic-offset slot per pass).
struct PostParams {
    texel: vec4f,    // xy: 1 / source size, z: filter radius in source texels, w: 1 on the first downsample
    settings: vec4f, // first downsample: x threshold, y knee. Composite: x exposure, y bloom intensity, z 1 / level count
};

@group(0) @binding(0) var<uniform> Params: PostParams;
@group(0) @binding(1) var linearSampler: sampler;

@group(1) @binding(0) var source: texture_2d<f32>;

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

fn luminance(c: vec3f) -> f32 {
    return dot(c, vec3f(0.2126, 0.7152, 0.0722));
}

fn tap(uv: vec2f) -> vec3f {
    // Alpha masks what may bloom (the skybox writes 0; bloom levels write 1).
    let s = textureSampleLevel(source, linearSampler, uv, 0.0);
    let c = s.rgb * s.a;
    // A NaN or infinity from the scene would otherwise smear across the whole bloom.
    return select(vec3f(0.0), min(c, vec3f(65000.0)), c == c);
}

// Weight of a group in a Karis average: 1 / (1 + luma), so one very bright pixel can't dominate.
fn karis(c: vec3f) -> f32 {
    return 1.0 / (1.0 + luminance(c));
}

// Soft-knee threshold: keeps only light brighter than `threshold`, fading in over `knee` so there's no hard edge.
fn prefilter(c: vec3f) -> vec3f {
    let threshold = Params.settings.x;
    let knee = max(Params.settings.y, 1e-4);
    let brightness = max(c.r, max(c.g, c.b));
    var soft = clamp(brightness - threshold + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee);
    return c * (max(soft, brightness - threshold) / max(brightness, 1e-4));
}

@fragment
fn fragmentMain(input: VertexOutput) -> @location(0) vec4f {
    let t = Params.texel.xy;
    let uv = input.uv;

    let a = tap(uv + t * vec2f(-2.0,  2.0));
    let b = tap(uv + t * vec2f( 0.0,  2.0));
    let c = tap(uv + t * vec2f( 2.0,  2.0));
    let d = tap(uv + t * vec2f(-2.0,  0.0));
    let e = tap(uv);
    let f = tap(uv + t * vec2f( 2.0,  0.0));
    let g = tap(uv + t * vec2f(-2.0, -2.0));
    let h = tap(uv + t * vec2f( 0.0, -2.0));
    let i = tap(uv + t * vec2f( 2.0, -2.0));
    let j = tap(uv + t * vec2f(-1.0,  1.0));
    let k = tap(uv + t * vec2f( 1.0,  1.0));
    let l = tap(uv + t * vec2f(-1.0, -1.0));
    let m = tap(uv + t * vec2f( 1.0, -1.0));

    if (Params.texel.w > 0.5) {
        // First level: Karis-average the five overlapping 2x2 groups, which stops single bright
        // pixels (speculars) from flickering into large blobs, then keep only what's over the threshold.
        let g0 = (a + b + d + e) * 0.25;
        let g1 = (b + c + e + f) * 0.25;
        let g2 = (d + e + g + h) * 0.25;
        let g3 = (e + f + h + i) * 0.25;
        let g4 = (j + k + l + m) * 0.25;
        let w0 = karis(g0) * 0.125;
        let w1 = karis(g1) * 0.125;
        let w2 = karis(g2) * 0.125;
        let w3 = karis(g3) * 0.125;
        let w4 = karis(g4) * 0.5;
        let average = (g0 * w0 + g1 * w1 + g2 * w2 + g3 * w3 + g4 * w4) / (w0 + w1 + w2 + w3 + w4);
        return vec4f(prefilter(average), 1.0);
    }

    let color = e * 0.125
              + (a + c + g + i) * 0.03125
              + (b + d + f + h) * 0.0625
              + (j + k + l + m) * 0.125;
    return vec4f(color, 1.0);
}
