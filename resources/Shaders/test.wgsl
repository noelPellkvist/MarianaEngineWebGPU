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
};

struct ModelData {
  modelMatrix: mat4x4<f32>,
  normalMatrix: mat4x4<f32>,
};

struct MaterialProperties {
    baseColor: vec4f,
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

@group(0) @binding(0) var<uniform> UniformBufferObject: UBO;

@group(1) @binding(0) var<uniform> ModelDataObject: ModelData;

@group(2) @binding(0) var<uniform> Material : MaterialProperties;
@group(2) @binding(1) var albedo: texture_2d<f32>;
@group(2) @binding(2) var normalMap: texture_2d<f32>;
@group(2) @binding(3) var ambientO: texture_2d<f32>;
@group(2) @binding(4) var metallicRoughness: texture_2d<f32>;  
@group(2) @binding(5) var emissiveTex: texture_2d<f32>;  
@group(2) @binding(6) var textureSampler: sampler;

@group(3) @binding(0) var<uniform> camInfo: CameraInfoData;

@vertex
fn vertexMain(input: VertexInput) -> VertexOutput {
    var output: VertexOutput;

    let world_pos4 = ModelDataObject.modelMatrix * vec4f(input.position, 1.0);
    output.world_pos = world_pos4.xyz;

    let mvp = camInfo.viewProj * ModelDataObject.modelMatrix;
    output.position = mvp * vec4f(input.position, 1.0);

    // If your normalMatrix is inverse-transpose(model), you can safely use it for N & T rotation
    let Nw_raw = (ModelDataObject.normalMatrix * vec4f(input.normal, 0.0)).xyz;
    let Tw_raw = (ModelDataObject.normalMatrix * vec4f(input.tangent.xyz, 0.0)).xyz;

    let Nw = normalize(Nw_raw);
    let Tn = normalize(Tw_raw - Nw * dot(Tw_raw, Nw));
    let Bw = normalize(cross(Nw, Tn)) * input.tangent.w; // handedness in .w

    output.world_normal  = Nw;
    output.world_tangent = Tn;
    output.world_bitangent = Bw;
    output.uv = input.texcoord0;
    return output;
}

// ---- PBR helpers (GGX + Smith + Schlick) ----
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
    // ACES fitted curve (Krzysztof Narkowicz, 2015)
    let a = 2.51;
    let b = 0.03;
    let c = 2.43;
    let d = 0.59;
    let e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), vec3f(0.0), vec3f(1.0));
}

@fragment
fn fragmentMain(input: VertexOutput) -> @location(0) vec4f {
    var n = textureSample(normalMap, textureSampler, input.uv).xyz * 2.0 - 1.0;
    let TBN = mat3x3<f32>(input.world_tangent, input.world_bitangent, input.world_normal);
    let N = normalize(TBN * n);

    var baseColor = textureSample(albedo, textureSampler, input.uv).rgb * Material.baseColor.rgb;

    let mrSample = textureSample(metallicRoughness, textureSampler, input.uv);
    let perceptualRoughness = clamp(mrSample.g, 0.04, 1.0);
    let metallic = clamp(mrSample.b, 0.0, 1.0);

    let aoStrength = 1.0;
    let ao = textureSample(ambientO, textureSampler, input.uv).r;
    let aoTerm = mix(1.0, ao, aoStrength);

    let L = normalize(UniformBufferObject.lightDir);
    let V = normalize(camInfo.position - input.world_pos);
    let H = normalize(L + V);
    let NoL = saturate(dot(N, L));
    let NoV = saturate(dot(N, V));
    let VoH = saturate(dot(V, H));

    // Dielectric F0 ~ 0.04, metals use baseColor as F0
    let F0_dielectric = vec3f(0.04);
    let F0 = mix(F0_dielectric, baseColor, metallic);

    let a = max(1e-3, perceptualRoughness * perceptualRoughness);

    let  D = D_GGX(N, H, a);
    let  G = G_Smith_correlated(N, V, L, a);
    let  F = F_Schlick(F0, VoH);

    let  spec = (D * G) * F / max(4.0 * NoV * NoL + 1e-7, 1e-7);

    let kd = (1.0 - F) * (1.0 - metallic);
    let diffuse = kd * baseColor / 3.14159265;

    let direct = (diffuse + spec) * NoL * 1.0;

    let ambient = 0.03 * aoTerm * baseColor;

    let exposure = 2.0;

    var emmisiveTexture = textureSample(emissiveTex, textureSampler, input.uv).rgb;
    emmisiveTexture = pow(emmisiveTexture, vec3f(2.2));

    var colorLinear = (direct + ambient + emmisiveTexture) * exposure;
    
    colorLinear = tonemapACES(colorLinear);
    
    colorLinear = saturate3(colorLinear);
    return vec4f(colorLinear, 1.0);
}
