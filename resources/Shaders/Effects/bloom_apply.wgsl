// Adds the bloom on top of the HDR image (it only holds light above the threshold).
// Inputs: scene (the image coming in), bloom (the top of the upsample chain, half size).
// passInfo.constants.x = 1 / level count: the chain sums every level, so this keeps the strength
// independent of how many there are.

fn bloomTap(uv: vec2f) -> vec3f {
    return textureSampleLevel(bloom, linearClamp, uv, 0.0).rgb;
}

// The half-resolution bloom, upsampled with the same 3x3 tent as the chain.
fn upsampledBloom(uv: vec2f) -> vec3f {
    let d = params.radius / vec2f(textureDimensions(bloom));
    var c = bloomTap(uv) * 4.0;
    c += (bloomTap(uv + vec2f(-d.x, 0.0)) + bloomTap(uv + vec2f(d.x, 0.0)) + bloomTap(uv + vec2f(0.0, -d.y)) + bloomTap(uv + vec2f(0.0, d.y))) * 2.0;
    c += bloomTap(uv + vec2f(-d.x, -d.y)) + bloomTap(uv + vec2f(d.x, -d.y)) + bloomTap(uv + vec2f(-d.x, d.y)) + bloomTap(uv + vec2f(d.x, d.y));
    return c / 16.0 * passInfo.constants.x;
}

@fragment
fn fragmentMain(input: FullscreenInput) -> @location(0) vec4f {
    let hdr = textureSampleLevel(scene, linearClamp, input.uv, 0.0);
    return vec4f(hdr.rgb + upsampledBloom(input.uv) * params.intensity, hdr.a);
}
