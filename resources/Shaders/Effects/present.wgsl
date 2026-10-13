// Copies the image to the screen with only exposure and sRGB encoding (no tonemapping). Used when
// no final effect is enabled. Input: scene.

fn linearToSrgb(c: vec3f) -> vec3f {
    let low = c * 12.92;
    let high = 1.055 * pow(c, vec3f(1.0 / 2.4)) - 0.055;
    return select(high, low, c <= vec3f(0.0031308));
}

@fragment
fn fragmentMain(input: FullscreenInput) -> @location(0) vec4f {
    let hdr = textureSampleLevel(scene, linearClamp, input.uv, 0.0).rgb;
    return vec4f(linearToSrgb(saturate(hdr * camera.exposure)), 1.0);
}
