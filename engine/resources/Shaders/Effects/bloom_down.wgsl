// Bloom downsample: one level of the chain, half the size of its input. 13-tap filter from
// "Next Generation Post Processing in Call of Duty: Advanced Warfare" (Jimenez 2014).
// Input: source. passInfo.constants.x = 1 on the first level (Karis average + threshold).

fn luminance(c: vec3f) -> f32 {
    return dot(c, vec3f(0.2126, 0.7152, 0.0722));
}

fn tap(uv: vec2f) -> vec3f {
    // Alpha masks what may bloom (the skybox writes 0; bloom levels write 1).
    let s = textureSampleLevel(source, linearClamp, uv, 0.0);
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
    let threshold = params.threshold;
    let knee = max(params.knee, 1e-4);
    let brightness = max(c.r, max(c.g, c.b));
    var soft = clamp(brightness - threshold + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee);
    return c * (max(soft, brightness - threshold) / max(brightness, 1e-4));
}

@fragment
fn fragmentMain(input: FullscreenInput) -> @location(0) vec4f {
    let t = 1.0 / vec2f(textureDimensions(source));
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

    if (passInfo.constants.x > 0.5) {
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
