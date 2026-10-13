// Ground Truth Ambient Occlusion (Jimenez et al. 2016, "Practical Real-Time Strategies for Accurate
// Indirect Occlusion"), structured after Intel's XeGTAO. For each pixel, a few screen-space slices
// through the view direction are searched for their highest horizon in the prepass depth, and the
// part of the hemisphere above both horizons is integrated against the normal (cosine weighted).
// Writes the visible fraction (1 = unoccluded). Noisy by design: gtao_blur.wgsl averages it.
// Inputs: depth (SceneDepth), normals (SceneNormals, view space * 0.5 + 0.5).

const PI: f32 = 3.14159265;
const HALF_PI: f32 = 1.57079633;
const SLICES: u32 = 3u;
const STEPS: u32 = 4u;              // per side of each slice
const MAX_SCREEN_RADIUS: f32 = 200.0; // pixels; keeps close-ups from sampling all over the screen

// View-space position at a pixel (LH, +z into the screen).
fn viewPosition(pixel: vec2i) -> vec3f {
    let p = clamp(pixel, vec2i(0), vec2i(passInfo.outputSize.xy) - 1);
    let z = textureLoad(depth, p, 0);
    let uv = (vec2f(p) + 0.5) * passInfo.outputSize.zw;
    let view = camera.invProj * vec4f(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, z, 1.0);
    return view.xyz / view.w;
}

// Two offsets in [0, 1) from a 4x4 ordered pattern: each pixel of a tile gets its own slice
// rotation and step offset, and the blur averages exactly one tile.
fn tileNoise(pixel: vec2i) -> vec2f {
    var bayer = array<u32, 16>(0u, 8u, 2u, 10u, 12u, 4u, 14u, 6u, 3u, 11u, 1u, 9u, 15u, 7u, 13u, 5u);
    let i = (u32(pixel.y) & 3u) * 4u + (u32(pixel.x) & 3u);
    return vec2f((f32(bayer[i]) + 0.5) / 16.0, (f32(bayer[(i + 5u) & 15u]) + 0.5) / 16.0);
}

@fragment
fn fragmentMain(input: FullscreenInput) -> @location(0) vec4f {
    let pixel = vec2i(input.position.xy);
    if (textureLoad(depth, pixel, 0) >= 1.0) {
        return vec4f(1.0); // sky
    }
    var N = textureLoad(normals, pixel, 0).xyz * 2.0 - 1.0;
    if (dot(N, N) < 0.25) {
        return vec4f(1.0); // points and lines without normals
    }
    N = normalize(N);

    let P = viewPosition(pixel);
    let V = normalize(-P);

    // The radius in pixels at this depth (projection[1][1] = 1 / tan(fov / 2)).
    let radius = params.radius;
    let screenRadius = min(radius * camera.projection[1][1] * 0.5 * passInfo.outputSize.y / P.z, MAX_SCREEN_RADIUS);
    if (screenRadius < 1.0) {
        return vec4f(1.0); // the whole radius fits in a pixel
    }

    // Occluders fade out over the outer part of the radius instead of popping at its edge.
    let falloffRange = 0.615 * radius;
    let falloffMul = -1.0 / falloffRange;
    let falloffAdd = (radius - falloffRange) / falloffRange + 1.0;
    // The first step starts just outside the centre pixel.
    let minStep = 1.3 / screenRadius;

    let noise = tileNoise(pixel);
    var visibility = 0.0;
    for (var slice = 0u; slice < SLICES; slice++) {
        let phi = (f32(slice) + noise.x) * PI / f32(SLICES);
        let screenDir = vec2f(cos(phi), -sin(phi)); // screen y points down, view y up
        let direction = vec3f(cos(phi), sin(phi), 0.0);

        // The slice plane holds V and the direction; n is the normal's angle from V within it.
        let orthoDirection = direction - dot(direction, V) * V;
        let axis = normalize(cross(orthoDirection, V));
        let projectedN = N - axis * dot(N, axis);
        let projectedLength = max(length(projectedN), 1e-4);
        let cosN = clamp(dot(projectedN, V) / projectedLength, -1.0, 1.0);
        let n = sign(dot(orthoDirection, projectedN)) * acos(cosN);

        // Start each horizon at the tangent plane and raise it with every occluder found.
        let lowHorizon0 = cos(n + HALF_PI);
        let lowHorizon1 = cos(n - HALF_PI);
        var horizon0 = lowHorizon0;
        var horizon1 = lowHorizon1;
        for (var stepIndex = 0u; stepIndex < STEPS; stepIndex++) {
            var s = (f32(stepIndex) + noise.y) / f32(STEPS);
            s = s * s + minStep;
            let offset = vec2i(round(s * screenRadius * screenDir));

            let delta0 = viewPosition(pixel + offset) - P;
            let delta1 = viewPosition(pixel - offset) - P;
            let distance0 = max(length(delta0), 1e-4);
            let distance1 = max(length(delta1), 1e-4);
            let weight0 = saturate(distance0 * falloffMul + falloffAdd);
            let weight1 = saturate(distance1 * falloffMul + falloffAdd);
            horizon0 = max(horizon0, mix(lowHorizon0, dot(delta0 / distance0, V), weight0));
            horizon1 = max(horizon1, mix(lowHorizon1, dot(delta1 / distance1, V), weight1));
        }

        // Horizon angles on both sides, kept within the normal's hemisphere.
        var h0 = -acos(clamp(horizon1, -1.0, 1.0));
        var h1 = acos(clamp(horizon0, -1.0, 1.0));
        h0 = n + clamp(h0 - n, -HALF_PI, HALF_PI);
        h1 = n + clamp(h1 - n, -HALF_PI, HALF_PI);

        // Cosine-weighted visible arc between the horizons.
        let arc0 = (cosN + 2.0 * h0 * sin(n) - cos(2.0 * h0 - n)) / 4.0;
        let arc1 = (cosN + 2.0 * h1 * sin(n) - cos(2.0 * h1 - n)) / 4.0;
        visibility += mix(projectedLength, 1.0, 0.05) * (arc0 + arc1);
    }

    visibility = pow(saturate(visibility / f32(SLICES)), params.power);
    return vec4f(max(visibility, 0.03), 0.0, 0.0, 1.0);
}
