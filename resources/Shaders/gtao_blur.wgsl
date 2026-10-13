// Denoises gtao.wgsl: averages the 4x4 block around each pixel (exactly one tile of its noise
// pattern), skipping neighbours that lie on another surface (different depth).

struct CameraInfoData {
    projection: mat4x4<f32>,
    view: mat4x4<f32>,
    viewProj: mat4x4<f32>,
    invView: mat4x4<f32>,
    invProj: mat4x4<f32>,
    invViewProj: mat4x4<f32>,
    position: vec3<f32>,
    exposure: f32,
};

struct AOParams {
    settings: vec4f, // x: radius in world units, y: power
    screen: vec4f,   // xy: size in pixels, zw: 1 / size
};

@group(0) @binding(0) var<uniform> camInfo: CameraInfoData;
@group(0) @binding(1) var<uniform> params: AOParams;

@group(1) @binding(0) var depthTex: texture_depth_2d;
@group(1) @binding(1) var aoTex: texture_2d<f32>;

// Neighbours further apart in depth than this fraction of the centre's depth don't count.
const DEPTH_TOLERANCE: f32 = 0.08;

struct VertexOutput {
    @builtin(position) position: vec4f,
};

// One triangle that covers the whole target.
@vertex
fn vertexMain(@builtin(vertex_index) index: u32) -> VertexOutput {
    let p = vec2f(f32((index << 1u) & 2u), f32(index & 2u));
    var out: VertexOutput;
    out.position = vec4f(p * 2.0 - 1.0, 0.0, 1.0);
    return out;
}

// View depth from the depth buffer (perspectiveLH_ZO: ndc z = proj[2][2] + proj[3][2] / z).
fn viewDepth(depth: f32) -> f32 {
    return camInfo.projection[3][2] / (depth - camInfo.projection[2][2]);
}

@fragment
fn fragmentMain(input: VertexOutput) -> @location(0) vec4f {
    let pixel = vec2i(input.position.xy);
    let depth = textureLoad(depthTex, pixel, 0);
    if (depth >= 1.0) {
        return vec4f(1.0);
    }
    let center = viewDepth(depth);
    let maxPixel = vec2i(params.screen.xy) - 1;

    var sum = 0.0;
    var weight = 0.0;
    for (var y = -2; y < 2; y++) {
        for (var x = -2; x < 2; x++) {
            let p = clamp(pixel + vec2i(x, y), vec2i(0), maxPixel);
            let d = textureLoad(depthTex, p, 0);
            let w = select(saturate(1.0 - abs(viewDepth(d) - center) / (center * DEPTH_TOLERANCE)), 0.0, d >= 1.0);
            sum += textureLoad(aoTex, p, 0).r * w;
            weight += w;
        }
    }
    return vec4f(sum / max(weight, 1e-4), 0.0, 0.0, 1.0);
}
