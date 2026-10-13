// Bloom upsample: blurs the next smaller level up with a 3x3 tent filter and adds this level's
// downsample result, so every level contributes to the final glow.
// Inputs: source (the next smaller level), current (this level's downsample).

fn tap(uv: vec2f) -> vec3f {
    return textureSampleLevel(source, linearClamp, uv, 0.0).rgb;
}

// 3x3 tent (weights 1 2 1 / 2 4 2 / 1 2 1) with taps `params.radius` source texels apart.
fn tent(uv: vec2f) -> vec3f {
    let d = params.radius / vec2f(textureDimensions(source));
    var c = tap(uv) * 4.0;
    c += (tap(uv + vec2f(-d.x, 0.0)) + tap(uv + vec2f(d.x, 0.0)) + tap(uv + vec2f(0.0, -d.y)) + tap(uv + vec2f(0.0, d.y))) * 2.0;
    c += tap(uv + vec2f(-d.x, -d.y)) + tap(uv + vec2f(d.x, -d.y)) + tap(uv + vec2f(-d.x, d.y)) + tap(uv + vec2f(d.x, d.y));
    return c / 16.0;
}

@fragment
fn fragmentMain(input: FullscreenInput) -> @location(0) vec4f {
    let down = textureSampleLevel(current, linearClamp, input.uv, 0.0).rgb;
    return vec4f(down + tent(input.uv), 1.0);
}
