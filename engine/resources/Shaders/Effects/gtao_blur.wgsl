// Denoises gtao.wgsl: averages the 4x4 block around each pixel (exactly one tile of its noise
// pattern), skipping neighbours that lie on another surface (different depth).
// Inputs: depth (SceneDepth), ao (the raw GTAO result).

// Neighbours further apart in depth than this fraction of the centre's depth don't count.
const DEPTH_TOLERANCE: f32 = 0.08;

// View depth from the depth buffer (perspectiveLH_ZO: ndc z = proj[2][2] + proj[3][2] / z).
fn viewDepth(z: f32) -> f32 {
    return camera.projection[3][2] / (z - camera.projection[2][2]);
}

@fragment
fn fragmentMain(input: FullscreenInput) -> @location(0) vec4f {
    let pixel = vec2i(input.position.xy);
    let centerDepth = textureLoad(depth, pixel, 0);
    if (centerDepth >= 1.0) {
        return vec4f(1.0);
    }
    let center = viewDepth(centerDepth);
    let maxPixel = vec2i(passInfo.outputSize.xy) - 1;

    var sum = 0.0;
    var weight = 0.0;
    for (var y = -2; y < 2; y++) {
        for (var x = -2; x < 2; x++) {
            let p = clamp(pixel + vec2i(x, y), vec2i(0), maxPixel);
            let d = textureLoad(depth, p, 0);
            let w = select(saturate(1.0 - abs(viewDepth(d) - center) / (center * DEPTH_TOLERANCE)), 0.0, d >= 1.0);
            sum += textureLoad(ao, p, 0).r * w;
            weight += w;
        }
    }
    return vec4f(sum / max(weight, 1e-4), 0.0, 0.0, 1.0);
}
