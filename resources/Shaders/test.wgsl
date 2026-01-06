// ===================== structs unchanged =====================

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

struct UBO {
    lightDir: vec3f,
    lightVP: mat4x4<f32>,
};

struct ModelData {
    modelMatrix: mat4x4<f32>,
    normalMatrix: mat4x4<f32>,
    entityID: u32,
};

struct MaterialProperties {
    baseColor: vec4<f32>,
    metallicFactor: f32,
    roughnessFactor: f32,
    normalMapStrength: f32,
    occlusionStrength: f32,
    emissiveFactor: vec3<f32>,
    alphaCutoff: f32,
};

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

// ===================== bindings unchanged =====================

@group(0) @binding(0) var<uniform> UniformBufferObject: UBO;
@group(0) @binding(1) var<uniform> ModelDataObject: ModelData;
@group(0) @binding(2) var<uniform> camInfo: CameraInfoData;
@group(0) @binding(3) var<uniform> Material: MaterialProperties;
@group(0) @binding(4) var shadowMap: texture_depth_2d;
@group(0) @binding(5) var shadowSampler: sampler_comparison;

@group(1) @binding(0) var albedo: texture_2d<f32>;
@group(1) @binding(1) var normalMap: texture_2d<f32>;
@group(1) @binding(2) var ambientO: texture_2d<f32>;
@group(1) @binding(3) var metallicRoughness: texture_2d<f32>;
@group(1) @binding(4) var emissiveTex: texture_2d<f32>;
@group(1) @binding(5) var textureSampler: sampler;

struct FragOut {
    @location(0) color: vec4f,
    @location(1) pick_pic: u32
};

// ===================== helpers =====================

fn saturate(x: f32) -> f32 { return clamp(x, 0.0, 1.0); }

// ===================== vertex =====================

@vertex
fn vertexMain(input: VertexInput) -> VertexOutput {
    var out: VertexOutput;

    let worldPos4 = ModelDataObject.modelMatrix * vec4f(input.position, 1.0);
    out.world_pos = worldPos4.xyz;

    out.position = camInfo.viewProj * worldPos4;

    out.world_normal =
        normalize((ModelDataObject.normalMatrix * vec4f(input.normal, 0.0)).xyz);

    // Kept to preserve the exact VS output layout; unused in the demo fragment.
    out.world_tangent = vec3f(0.0);
    out.world_bitangent = vec3f(0.0);

    out.uv = input.texcoord0;
    return out;
}

// ===================== fragment (final correct) =====================

@fragment
fn fragmentMain(input: VertexOutput) -> FragOut {
    let N = normalize(input.world_normal);
    let L = normalize(-UniformBufferObject.lightDir);
    let NoL = saturate(dot(N, L));

    let lightClip = UniformBufferObject.lightVP * vec4f(input.world_pos, 1.0);
    let ndc = lightClip.xyz / max(lightClip.w, 1e-8);

    // ✅ NO Y FLIP
    let uv = vec2f(ndc.x, -ndc.y) * 0.5 + vec2f(0.5);

    let biasMin = 0.0005;
    let biasMax = 0.0040;
    let bias = mix(biasMax, biasMin, NoL);

    // Directional + ortho + LessEqual → +bias
    let normalBias = 0.002 * (1.0 - NoL);
    let depthRef = ndc.z - (bias + normalBias);

    let shadowRaw = textureSampleCompare(
        shadowMap,
        shadowSampler,
        clamp(uv, vec2f(0.0), vec2f(1.0)),
        clamp(depthRef, 0.0, 1.0)
    );

    let inXY =
        step(-1.0, ndc.x) * step(ndc.x, 1.0) *
        step(-1.0, ndc.y) * step(ndc.y, 1.0);
    let inZ = step(0.0, ndc.z) * step(ndc.z, 1.0);
    let inW = step(0.0, lightClip.w);
    let inFrustum = inXY * inZ * inW;

    let litMask = step(0.0, NoL);

    let shadow =
        (mix(1.0, shadowRaw, inFrustum) * litMask) +
        (1.0 - litMask);

    let baseColor = vec3f(1.0);
    let ambient = 0.10 * baseColor;
    let diffuse = baseColor * NoL * shadow;

    var out: FragOut;
    out.color = vec4f(ambient + diffuse, 1.0);
    out.pick_pic = ModelDataObject.entityID;
    return out;
}
