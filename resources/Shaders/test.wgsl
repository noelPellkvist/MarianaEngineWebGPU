// ===================== structs unchanged =====================

struct VertexInput {
    @location(0) position: vec3f,
    @location(1) normal: vec3f,
    @location(2) tangent: vec4f,
    @location(3) texcoord0: vec2f,
    @location(4) texcoord1: vec2f,
    @location(5) color: vec4f,
    @location(6) boneIndices: vec4u,
    @location(7) boneWeights: vec4f,
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
    normalMatrix: mat3x3<f32>,
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

struct BoneData {
    boneMatrices: array<mat4x4<f32>, 128>,
};

// ===================== bindings unchanged =====================

@group(0) @binding(0) var<uniform> UniformBufferObject: UBO;
@group(0) @binding(1) var<uniform> ModelDataObject: ModelData;
@group(0) @binding(2) var<uniform> camInfo: CameraInfoData;
@group(0) @binding(3) var<uniform> Material: MaterialProperties;
@group(0) @binding(4) var shadowMap: texture_depth_2d;
@group(0) @binding(5) var shadowSampler: sampler_comparison;
@group(0) @binding(6) var<storage, read_write> Bones: BoneData;

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
fn saturate3(v: vec3f) -> vec3f { return clamp(v, vec3f(0.0), vec3f(1.0)); }

fn D_GGX(N: vec3f, H: vec3f, a: f32) -> f32 {
    let a2 = a * a;
    let NoH = saturate(dot(N, H));
    let d = (NoH * NoH) * (a2 - 1.0) + 1.0;
    return a2 / (3.14159265 * d * d + 1e-7);
}

fn G_Smith_correlated(N: vec3f, V: vec3f, L: vec3f, a: f32) -> f32 {
    let a2 = a * a;
    let NoV = saturate(dot(N, V));
    let NoL = saturate(dot(N, L));
    let gv = NoV + sqrt(a2 + (1.0 - a2) * NoV * NoV);
    let gl = NoL + sqrt(a2 + (1.0 - a2) * NoL * NoL);
    return 1.0 / (gv * gl + 1e-7);
}

fn F_Schlick(F0: vec3f, VoH: f32) -> vec3f {
    let f = pow(1.0 - saturate(VoH), 5.0);
    return F0 + (1.0 - F0) * f;
}

fn tonemapACES(x: vec3f) -> vec3f {
    let a = 2.51;
    let b = 0.03;
    let c = 2.43;
    let d = 0.59;
    let e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), vec3f(0.0), vec3f(1.0));
}

fn safeNormalize(v: vec3f) -> vec3f {
    let len2 = dot(v, v);
    if (len2 > 1e-10) {
        return v * inverseSqrt(len2);
    }
    return vec3f(0.0, 0.0, 1.0);
}

// ===================== vertex =====================

@vertex
fn vertexMain(input: VertexInput) -> VertexOutput {
    var output: VertexOutput;

    let world_pos4 = ModelDataObject.modelMatrix * vec4f(input.position, 1.0);
    output.world_pos = world_pos4.xyz;

    let mvp = camInfo.viewProj * ModelDataObject.modelMatrix;
    output.position = mvp * vec4f(input.position, 1.0);

    let M3 = mat3x3<f32>(
        ModelDataObject.modelMatrix[0].xyz,
        ModelDataObject.modelMatrix[1].xyz,
        ModelDataObject.modelMatrix[2].xyz
    );

    let Nw_raw = ModelDataObject.normalMatrix * input.normal;
    let Tw_raw = M3 * input.tangent.xyz; 

    let Nw = normalize(Nw_raw);
    let Tn = normalize(Tw_raw - Nw * dot(Tw_raw, Nw));
    let Bw = normalize(cross(Nw, Tn)) * input.tangent.w;

    output.world_normal = Nw;
    output.world_tangent = Tn;
    output.world_bitangent = Bw;
    output.uv = input.texcoord0;
    return output;
}

// ===================== fragment (PBR + shadows) =====================

@fragment
fn fragmentMain(input: VertexOutput) -> FragOut {
    // ----- Normal (use geometric normal for this test; swap back later) -----
    let N = safeNormalize(input.world_normal);

    // ----- Base color + alpha -----
    let albedoSample = textureSample(albedo, textureSampler, input.uv);
    let baseColor = albedoSample.rgb * Material.baseColor.rgb;
    let alpha = albedoSample.a * Material.baseColor.a;
    if (alpha < Material.alphaCutoff) {
        discard;
    }

    // ----- Metallic/Roughness -----
    let mrSample = textureSample(metallicRoughness, textureSampler, input.uv);
    let perceptualRoughness = clamp(mrSample.g * Material.roughnessFactor, 0.04, 1.0);
    let metallic = clamp(mrSample.b * Material.metallicFactor, 0.0, 1.0);

    // ----- AO -----
    let ao = textureSample(ambientO, textureSampler, input.uv).r;
    let aoTerm = mix(1.0, ao, Material.occlusionStrength);

    // ----- Lighting vectors -----
    let L = safeNormalize(-UniformBufferObject.lightDir);
    let V = safeNormalize(camInfo.position - input.world_pos);

    let NoL = saturate(dot(N, L));
    let NoV = saturate(dot(N, V));

    // ----- Fresnel base reflectance -----
    let F0_dielectric = vec3f(0.04);
    let F0 = mix(F0_dielectric, baseColor, metallic);

    // ----- Roughness -> alpha -----
    let a = max(1e-3, perceptualRoughness * perceptualRoughness);

    // ----- Specular (smoothly fade out near H degeneracy) -----
    // L and V are unit, so |L+V| is in [0..2].
    let sumLV = L + V;
    let sumLen2 = dot(sumLV, sumLV);
    let sumLen  = sqrt(max(sumLen2, 0.0));

    // Fade spec to 0 as sumLen -> 0 to avoid a hard seam.
    // Tweak eps if needed (0.02–0.10 are reasonable).
    let eps = 0.05;
    let hWeight = smoothstep(0.0, eps, sumLen);

    let H = safeNormalize(sumLV);
    let VoH = saturate(dot(V, H));

    let F = F_Schlick(F0, VoH);

    var spec = vec3f(0.0);
    if (NoL > 0.0 && NoV > 0.0) {
        let D = D_GGX(N, H, a);
        let G = G_Smith_correlated(N, V, L, a);
        let denom = max(4.0 * NoV * NoL, 1e-4);
        spec = (D * G) * F / denom;
    }

    // Apply the fade (this is the key change)
    spec *= hWeight;

    // ----- Diffuse (energy conserving) -----
    let kd = (1.0 - F) * (1.0 - metallic);
    let diffuse = kd * baseColor / 3.14159265;

    // ----- Shadow map sampling -----
    let lightClip = UniformBufferObject.lightVP * vec4f(input.world_pos, 1.0);
    let ndc = lightClip.xyz / max(lightClip.w, 1e-8);
    let uv = vec2f(ndc.x, -ndc.y) * 0.5 + vec2f(0.5);

    let biasMin = 0.0003;
    let biasMax = 0.0020;
    let bias = mix(biasMax, biasMin, NoL);
    let normalBias = 0.001 * (1.0 - NoL);
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

    let litMask = step(1e-5, NoL);
    let shadowStrength = 0.8;
    let shadow =
        (mix(1.0, shadowRaw, shadowStrength * inFrustum) * litMask) +
        (1.0 - litMask);

    // ----- Lighting -----
    let direct = (diffuse + spec) * NoL * shadow;
    let ambient = 0.1 * aoTerm * baseColor;

    // ----- Emissive -----
    var emissiveTexture = textureSample(emissiveTex, textureSampler, input.uv).rgb;
    emissiveTexture = pow(emissiveTexture, vec3f(2.2)); // * Material.emissiveFactor;

    // ----- Exposure + tonemap -----
    let exposure = 1.5;
    var colorLinear = (direct + ambient + emissiveTexture) * exposure;

    colorLinear = tonemapACES(colorLinear);
    colorLinear = saturate3(colorLinear);

    var out: FragOut;
    out.color = vec4f(colorLinear, 1.0);
    //out.color = vec4f(input.world_normal * 0.5 + 0.5, 1.0);
    out.pick_pic = ModelDataObject.entityID;
    return out;
}
