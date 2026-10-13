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
    @location(5) @interpolate(flat) drawIndex: u32,
    @location(6) uv1: vec2f,
    @location(7) color: vec4f,
    @location(8) @interpolate(flat) mirror: f32, // -1 when the transform mirrors (negative determinant)
};

struct UBO {
    lightDir: vec3f,
    cascadeVP: array<mat4x4<f32>, 4>,
    cascadeSplits: vec4f, // view depth where each shadow cascade ends
    cascadeTexel: vec4f,  // world size of one shadow texel in each cascade
    sunColor: vec4f,      // rgb = color * intensity
    skySH: array<vec4f, 9>, // diffuse sky light (SkyLight.hpp)
    skyParams: vec4f,     // x = sky diffuse intensity, y = sky reflection intensity
};

struct ModelData {
    modelMatrix: mat4x4<f32>,
    normalMatrix: mat3x3<f32>,
    entityID: vec2u,
};

struct MaterialProperties {
    baseColor: vec4<f32>,
    metallicFactor: f32,
    roughnessFactor: f32,
    normalMapStrength: f32,
    occlusionStrength: f32,
    emissiveFactor: vec3<f32>,
    alphaCutoff: f32,
    uvSets: u32, // bit i: texture i (albedo, normal, metallic-roughness, occlusion, emissive) reads uv1
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
    model: mat4x4<f32>,
};

struct DrawData
{
    transformIndex: u32,
    materialIndex: u32,
    boneOffset: u32, // first joint matrix in Bones, or NO_SKIN
}

// ===================== bindings unchanged =====================

@group(0) @binding(0) var<uniform> UniformBufferObject: UBO;
@group(0) @binding(1) var<uniform> camInfo: CameraInfoData;
@group(0) @binding(2) var<storage, read> DrawInfo: array<DrawData>;
@group(0) @binding(3) var<storage, read> ModelDataObjectBuffer: array<ModelData>;
@group(0) @binding(4) var<storage, read> MaterialBuffer: array<MaterialProperties>;
@group(0) @binding(5) var<storage, read> Bones: array<BoneData>;
@group(0) @binding(6) var shadowMap0: texture_depth_2d;
@group(0) @binding(7) var shadowMap1: texture_depth_2d;
@group(0) @binding(8) var shadowMap2: texture_depth_2d;
@group(0) @binding(9) var shadowMap3: texture_depth_2d;
@group(0) @binding(10) var shadowSampler: sampler_comparison;
@group(0) @binding(11) var skyCube: texture_cube<f32>;
@group(0) @binding(12) var skySampler: sampler;


@group(1) @binding(0) var albedo: texture_2d<f32>;
@group(1) @binding(1) var normalMap: texture_2d<f32>;
@group(1) @binding(2) var metallicRoughness: texture_2d<f32>;
@group(1) @binding(3) var ambientO: texture_2d<f32>;
@group(1) @binding(4) var emissiveTex: texture_2d<f32>;
// One sampler per texture: each keeps its glTF wrap and filter settings.
@group(1) @binding(5) var albedoSampler: sampler;
@group(1) @binding(6) var normalSampler: sampler;
@group(1) @binding(7) var metallicRoughnessSampler: sampler;
@group(1) @binding(8) var occlusionSampler: sampler;
@group(1) @binding(9) var emissiveSampler: sampler;

struct FragOut {
    @location(0) color: vec4f,
    @location(1) pick_pic: vec4u
};

// ===================== helpers =====================

fn packEntityId(id: vec2u) -> vec4u {
    return vec4u(id.x & 0xFFFFu, (id.x >> 16u) & 0xFFFFu, id.y & 0xFFFFu, (id.y >> 16u) & 0xFFFFu);
}

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


fn safeNormalize(v: vec3f) -> vec3f {
    let len2 = dot(v, v);
    if (len2 > 1e-10) {
        return v * inverseSqrt(len2);
    }
    return vec3f(0.0, 0.0, 1.0);
}

// The UV set the material picked for one of its textures (0 albedo, 1 normal, 2 metallic-roughness,
// 3 occlusion, 4 emissive).
fn textureUV(uv0: vec2f, uv1: vec2f, uvSets: u32, texture: u32) -> vec2f {
    return select(uv0, uv1, ((uvSets >> texture) & 1u) != 0u);
}

// ===================== shadows =====================

fn sampleShadowMap(cascade: u32, uv: vec2f, depthRef: f32) -> f32 {
    switch cascade {
        case 0u: { return textureSampleCompareLevel(shadowMap0, shadowSampler, uv, depthRef); }
        case 1u: { return textureSampleCompareLevel(shadowMap1, shadowSampler, uv, depthRef); }
        case 2u: { return textureSampleCompareLevel(shadowMap2, shadowSampler, uv, depthRef); }
        default: { return textureSampleCompareLevel(shadowMap3, shadowSampler, uv, depthRef); }
    }
}

// Lit fraction (1 = fully lit) from one cascade. 16 hardware-filtered compare taps in a 4x4 grid
// give a smooth 5x5-texel penumbra with no noise.
fn cascadeShadow(cascade: u32, worldPos: vec3f) -> f32 {
    let clip = UniformBufferObject.cascadeVP[cascade] * vec4f(worldPos, 1.0);
    let ndc = clip.xyz / clip.w;
    let uv = vec2f(ndc.x, -ndc.y) * 0.5 + vec2f(0.5);
    if (any(uv < vec2f(0.0)) || any(uv > vec2f(1.0)) || ndc.z > 1.0) {
        return 1.0;
    }

    let texel = 1.0 / vec2f(textureDimensions(shadowMap0));
    let depthRef = ndc.z - 0.0001;
    var lit = 0.0;
    for (var y = -1.5; y <= 1.5; y += 1.0) {
        for (var x = -1.5; x <= 1.5; x += 1.0) {
            lit += sampleShadowMap(cascade, uv + vec2f(x, y) * texel, depthRef);
        }
    }
    return lit / 16.0;
}

// Lit fraction at a surface point: picks the cascade by view depth, offsets the lookup along the
// normal (by a texel or two, more at grazing angles) so the surface doesn't shadow itself, blends
// into the next cascade near a cascade's end, and fades out at the shadow distance.
fn shadowFactor(worldPos: vec3f, N: vec3f, NoL: f32, viewDepth: f32) -> f32 {
    let splits = UniformBufferObject.cascadeSplits;
    if (viewDepth >= splits.w) {
        return 1.0;
    }

    var cascade = 0u;
    while (cascade < 3u && viewDepth >= splits[cascade]) {
        cascade += 1u;
    }

    let offsetScale = 1.0 + 2.0 * (1.0 - NoL);
    var lit = cascadeShadow(cascade, worldPos + N * UniformBufferObject.cascadeTexel[cascade] * offsetScale);

    let cascadeStart = select(0.0, splits[max(cascade, 1u) - 1u], cascade > 0u);
    let blendStart = mix(cascadeStart, splits[cascade], 0.9);
    if (cascade < 3u && viewDepth > blendStart) {
        let next = cascadeShadow(cascade + 1u, worldPos + N * UniformBufferObject.cascadeTexel[cascade + 1u] * offsetScale);
        lit = mix(lit, next, (viewDepth - blendStart) / (splits[cascade] - blendStart));
    }

    let fadeStart = splits.w * 0.9;
    return mix(lit, 1.0, saturate((viewDepth - fadeStart) / (splits.w - fadeStart)));
}

// ===================== sky light =====================

// Irradiance from the sky around normal n, divided by pi (times albedo = diffuse color).
fn skyIrradiance(n: vec3f) -> vec3f {
    let sh = UniformBufferObject.skySH;
    let e = sh[0].rgb
          + sh[1].rgb * n.y + sh[2].rgb * n.z + sh[3].rgb * n.x
          + sh[4].rgb * (n.x * n.y) + sh[5].rgb * (n.y * n.z) + sh[6].rgb * (3.0 * n.z * n.z - 1.0)
          + sh[7].rgb * (n.x * n.z) + sh[8].rgb * (n.x * n.x - n.y * n.y);
    return max(e, vec3f(0.0));
}

// The sky reflected along R. The cubemap's mips are box filtered, so a rougher surface reads a
// level whose texels are about as wide as its reflection lobe; the roughest fade to the diffuse
// sky light, which is smoother than the smallest mips.
fn skyReflection(R: vec3f, perceptualRoughness: f32) -> vec3f {
    let a = perceptualRoughness * perceptualRoughness;
    let maxLod = f32(textureNumLevels(skyCube) - 1u);
    let lod = clamp(log2(a * f32(textureDimensions(skyCube).x)) + 0.35, 0.0, maxLod);
    let sharp = textureSampleLevel(skyCube, skySampler, R, lod).rgb;
    return mix(sharp, skyIrradiance(R), smoothstep(0.6, 1.0, perceptualRoughness));
}

// Split-sum environment BRDF, analytic fit (Karis, "Physically Based Shading on Mobile").
fn envBRDFApprox(F0: vec3f, perceptualRoughness: f32, NoV: f32) -> vec3f {
    let c0 = vec4f(-1.0, -0.0275, -0.572, 0.022);
    let c1 = vec4f(1.0, 0.0425, 1.04, -0.04);
    let r = perceptualRoughness * c0 + c1;
    let a004 = min(r.x * r.x, exp2(-9.28 * NoV)) * r.x + r.y;
    let AB = vec2f(-1.04, 1.04) * a004 + r.zw;
    return F0 * AB.x + AB.y;
}

// ===================== vertex =====================

const NO_SKIN: u32 = 0xFFFFFFFFu;

// Weighted blend of up to 4 joint matrices (identity for meshes without a skin).
fn skinMatrix(input: VertexInput, boneOffset: u32) -> mat4x4<f32> {
    if (boneOffset == NO_SKIN) {
        return mat4x4<f32>(vec4f(1.0, 0.0, 0.0, 0.0), vec4f(0.0, 1.0, 0.0, 0.0), vec4f(0.0, 0.0, 1.0, 0.0), vec4f(0.0, 0.0, 0.0, 1.0));
    }
    return Bones[boneOffset + input.boneIndices.x].model * input.boneWeights.x
         + Bones[boneOffset + input.boneIndices.y].model * input.boneWeights.y
         + Bones[boneOffset + input.boneIndices.z].model * input.boneWeights.z
         + Bones[boneOffset + input.boneIndices.w].model * input.boneWeights.w;
}

@vertex
fn vertexMain(input: VertexInput, @builtin(instance_index) drawCallIndex: u32) -> VertexOutput {
    var output: VertexOutput;
    output.drawIndex = drawCallIndex;
    var drawData = DrawInfo[drawCallIndex];
    var ModelDataObject = ModelDataObjectBuffer[drawData.transformIndex];

    let skin = skinMatrix(input, drawData.boneOffset);
    let skin3 = mat3x3<f32>(skin[0].xyz, skin[1].xyz, skin[2].xyz);
    let localPos = skin * vec4f(input.position, 1.0);

    let world_pos4 = ModelDataObject.modelMatrix * localPos;
    output.world_pos = world_pos4.xyz;
    output.position = camInfo.viewProj * world_pos4;

    let M3 = mat3x3<f32>(
        ModelDataObject.modelMatrix[0].xyz,
        ModelDataObject.modelMatrix[1].xyz,
        ModelDataObject.modelMatrix[2].xyz
    );

    let Nw_raw = ModelDataObject.normalMatrix * (skin3 * input.normal);
    let Tw_raw = M3 * (skin3 * input.tangent.xyz);
    // Mirroring flips the handedness of the tangent frame (and which side the faces show).
    let mirror = select(1.0, -1.0, determinant(M3 * skin3) < 0.0);

    // Points and lines may have no normal: it stays zero and the fragment draws them unlit.
    let Nw = select(vec3f(0.0), normalize(Nw_raw), dot(Nw_raw, Nw_raw) > 1e-12);
    let Tn = safeNormalize(Tw_raw - Nw * dot(Tw_raw, Nw));
    let Bw = safeNormalize(cross(Nw, Tn)) * input.tangent.w * mirror;

    output.world_normal = Nw;
    output.world_tangent = Tn;
    output.world_bitangent = Bw;
    output.uv = input.texcoord0;
    output.uv1 = input.texcoord1;
    output.color = input.color;
    output.mirror = mirror;
    return output;
}

// ===================== fragment (PBR + shadows) =====================

@fragment
fn fragmentMain(input: VertexOutput, @builtin(front_facing) frontFacing: bool) -> FragOut {
    var Material = MaterialBuffer[DrawInfo[input.drawIndex].materialIndex];

    // ----- Normal: the interpolated tangent frame, bent by the normal map -----
    // Back faces only get here for double-sided materials: the whole frame flips so they are lit
    // from their own side (as in the glTF sample viewer).
    let side = select(-1.0, 1.0, frontFacing) * input.mirror;
    let Ng = side * safeNormalize(input.world_normal);
    let tangent = side * input.world_tangent;
    // Interpolation bends the frame between vertices, so it is made orthonormal again.
    let T = safeNormalize(tangent - Ng * dot(tangent, Ng));
    let B = cross(Ng, T) * select(-1.0, 1.0, dot(cross(Ng, T), side * input.world_bitangent) >= 0.0);
    let mapped = textureSample(normalMap, normalSampler, textureUV(input.uv, input.uv1, Material.uvSets, 1u)).xyz * 2.0 - 1.0;
    let N = safeNormalize(T * (mapped.x * Material.normalMapStrength) + B * (mapped.y * Material.normalMapStrength) + Ng * mapped.z);

    // ----- Base color + alpha -----
    // COLOR_0 multiplies the base color (white when a mesh has none).
    let albedoSample = textureSample(albedo, albedoSampler, textureUV(input.uv, input.uv1, Material.uvSets, 0u));
    let baseColor = albedoSample.rgb * Material.baseColor.rgb * input.color.rgb;
    let alpha = albedoSample.a * Material.baseColor.a * input.color.a;
    if (alpha < Material.alphaCutoff) {
        discard;
    }

    // ----- Metallic/Roughness -----
    let mrSample = textureSample(metallicRoughness, metallicRoughnessSampler, textureUV(input.uv, input.uv1, Material.uvSets, 2u));
    let perceptualRoughness = clamp(mrSample.g * Material.roughnessFactor, 0.04, 1.0);
    let metallic = clamp(mrSample.b * Material.metallicFactor, 0.0, 1.0);

    // ----- AO -----
    let ao = textureSample(ambientO, occlusionSampler, textureUV(input.uv, input.uv1, Material.uvSets, 3u)).r;
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

    // ----- Shadows (cascaded, filtered) -----
    let viewDepth = (camInfo.view * vec4f(input.world_pos, 1.0)).z;
    // The lookup offset follows the actual surface, not the normal map.
    let shadow = shadowFactor(input.world_pos, Ng, saturate(dot(Ng, L)), viewDepth);

    // ----- Sun -----
    let direct = (diffuse + spec) * NoL * shadow * UniformBufferObject.sunColor.rgb;

    // ----- Sky: diffuse from its spherical harmonics, reflections from the cubemap -----
    let envSpecular = envBRDFApprox(F0, perceptualRoughness, NoV);
    let skyDiffuse = skyIrradiance(N) * baseColor * (1.0 - metallic) * (1.0 - envSpecular) * UniformBufferObject.skyParams.x;
    let skySpecular = skyReflection(reflect(-V, N), perceptualRoughness) * envSpecular * UniformBufferObject.skyParams.y;
    let ambient = (skyDiffuse + skySpecular) * aoTerm;

    // ----- Emissive -----
    // The emissive texture is sampled through an sRGB view, so it is already linear here.
    let emissive = textureSample(emissiveTex, emissiveSampler, textureUV(input.uv, input.uv1, Material.uvSets, 4u)).rgb * Material.emissiveFactor;

    // ----- Output: linear HDR (exposure, bloom and tonemapping happen in composite.wgsl) -----
    var out: FragOut;
    // Points and lines without normals can't be lit: they show their base color.
    let hasNormal = dot(input.world_normal, input.world_normal) > 1e-8;
    out.color = vec4f(select(baseColor, direct + ambient, hasNormal) + emissive, 1.0);
    //out.color = vec4f(input.world_normal * 0.5 + 0.5, 1.0);
    out.pick_pic = packEntityId(ModelDataObjectBuffer[DrawInfo[input.drawIndex].transformIndex].entityID);
    return out;
}
