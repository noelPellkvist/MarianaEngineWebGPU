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
    @location(1) pick_pic: u32
};

@fragment
fn fragmentMain(input: VertexOutput) -> FragOut {
    let dir = normalize(input.world_pos);

    let sky = textureSample(albedo, textureSampler, dir).rgb;

    var out: FragOut;
    out.color = vec4f(sky, 1.0);
    out.pick_pic = 0xFFFFFFFFu;
    return out;
}
