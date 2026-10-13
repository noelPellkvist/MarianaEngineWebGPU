// ---------------------- Shared structures ----------------------

struct VertexInput {
    @location(0) position: vec3f,
    @location(1) normal: vec3f,
    @location(2) tangent: vec4f,
    @location(3) texcoord0: vec2f,
    @location(4) texcoord1: vec2f,
    @location(5) color: vec4f,
};

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) world_normal: vec3f,
    @location(1) world_tangent: vec3f,
    @location(2) world_bitangent: vec3f,
    @location(3) uv: vec2f,
    @location(4) world_pos: vec3f,
};

struct CameraInfoData {
  projection: mat4x4<f32>,
  view: mat4x4<f32>,
  viewProj: mat4x4<f32>,
  invView: mat4x4<f32>,
  invProj: mat4x4<f32>,
  invViewProj: mat4x4<f32>,
  position: vec3f,
  exposure: f32,
};
@group(0) @binding(0) var<uniform> camInfo: CameraInfoData;
@group(0) @binding(1) var albedo: texture_cube<f32>;
@group(0) @binding(2) var textureSampler: sampler;



// ---------------------- Skybox Vertex Shader ----------------------

@vertex
fn vertexMain(input: VertexInput) -> VertexOutput {
    var output: VertexOutput;

    // Use position as direction  
    let dir = input.position;

    // Remove translation from view matrix  
    let viewNoTranslation =
        mat4x4<f32>(
            vec4<f32>(camInfo.view[0].xyz, 0.0),
            vec4<f32>(camInfo.view[1].xyz, 0.0),
            vec4<f32>(camInfo.view[2].xyz, 0.0),
            vec4<f32>(0.0, 0.0, 0.0, 1.0)
        );

    let vp = camInfo.projection * viewNoTranslation;

    // Set depth = W so it renders at infinite depth  
    output.position = (vp * vec4f(dir, 1.0)).xyww;

    // Store direction for fragment shader  
    output.world_pos = dir;

    // Unused outputs but required by pipeline  
    output.world_normal = vec3f(0.0);
    output.world_tangent = vec3f(0.0);
    output.world_bitangent = vec3f(0.0);
    output.uv = vec2f(0.0);

    return output;
}

// ---------------------- Skybox Fragment Shader ----------------------

struct FragOut {
    @location(0) color: vec4f,
};

// Inverse of the ACES fit used in composite.wgsl, for y in [0, 1).
fn inverseTonemapACES(y: vec3f) -> vec3f {
    let a = 2.51;
    let b = 0.03;
    let c = 2.43;
    let d = 0.59;
    let e = 0.14;
    let v = clamp(y, vec3f(0.0), vec3f(0.999));
    let qa = a - c * v;
    let qb = b - d * v;
    return (-qb + sqrt(qb * qb + 4.0 * qa * e * v)) / (2.0 * qa);
}

@fragment
fn fragmentMain(input: VertexOutput) -> FragOut {
    let dir = normalize(input.world_pos);

    // The cubemap is sRGB, so sampling returns linear color. Level 0: the mips are only for blurry
    // reflections (test.wgsl).
    let sky = textureSampleLevel(albedo, textureSampler, dir, 0.0).rgb;

    // The cubemap is a display-ready photo, not HDR radiance: undo the composite's exposure and
    // tonemap in advance so it reaches the screen exactly as authored. Alpha 0 keeps it out of bloom.
    var out: FragOut;
    out.color = vec4f(inverseTonemapACES(sky) / max(camInfo.exposure, 1e-4), 0.0);
    return out;
}
