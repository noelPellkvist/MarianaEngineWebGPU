// tonemap.wgsl for thumbnails: keeps the scene's alpha (0 where nothing was drawn, as the sky is
// off), so the background is transparent. Input: scene.

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

@fragment
fn fragmentMain(input: FullscreenInput) -> @location(0) vec4f {
    let color = textureSampleLevel(scene, linearClamp, input.uv, 0.0);
    // Edges resolved against the cleared background are premultiplied by their coverage.
    let hdr = color.rgb / max(color.a, 1e-4);
    return vec4f(linearToSrgb(tonemapACES(hdr * camera.exposure)), color.a);
}
