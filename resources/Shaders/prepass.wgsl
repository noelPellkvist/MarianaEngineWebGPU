// Depth/normal prepass (single sample): the scene's depth, view-space normals for the ambient
// occlusion pass (gtao.wgsl), and the entity id under each pixel for picking. Transforms match test.wgsl.

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
    @location(1) uv: vec2f,
    @location(2) uv1: vec2f,
    @location(3) alpha: f32, // vertex color alpha
    @location(4) @interpolate(flat) drawIndex: u32,
    @location(5) @interpolate(flat) mirror: f32,
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

struct ModelData {
    modelMatrix: mat4x4<f32>,
    normalMatrix: mat3x3<f32>,
    entityID: vec2u,
};

// Same as test.wgsl.
struct MaterialProperties {
    baseColor: vec4<f32>,
    metallicFactor: f32,
    roughnessFactor: f32,
    normalMapStrength: f32,
    occlusionStrength: f32,
    emissiveFactor: vec3<f32>,
    alphaCutoff: f32,
    uvSets: u32,
};

struct BoneData {
    model: mat4x4<f32>,
};

struct DrawData {
    transformIndex: u32,
    materialIndex: u32,
    boneOffset: u32,
};

@group(0) @binding(0) var<uniform> camInfo: CameraInfoData;
@group(0) @binding(1) var<storage, read> DrawInfo: array<DrawData>;
@group(0) @binding(2) var<storage, read> ModelDataObjectBuffer: array<ModelData>;
@group(0) @binding(3) var<storage, read> MaterialBuffer: array<MaterialProperties>;
@group(0) @binding(4) var<storage, read> Bones: array<BoneData>;

// The PBR material group; only the base color is read, for alpha testing.
@group(1) @binding(0) var albedo: texture_2d<f32>;
@group(1) @binding(5) var albedoSampler: sampler;

struct PrepassOut {
    @location(0) normal: vec4f, // view-space normal * 0.5 + 0.5 (all 0.5 without a normal)
    @location(1) pick: vec4u,   // 64-bit entity id as 4 x u16
};

const NO_SKIN: u32 = 0xFFFFFFFFu;

fn skinMatrix(input: VertexInput, boneOffset: u32) -> mat4x4<f32> {
    if (boneOffset == NO_SKIN) {
        return mat4x4<f32>(vec4f(1.0, 0.0, 0.0, 0.0), vec4f(0.0, 1.0, 0.0, 0.0), vec4f(0.0, 0.0, 1.0, 0.0), vec4f(0.0, 0.0, 0.0, 1.0));
    }
    return Bones[boneOffset + input.boneIndices.x].model * input.boneWeights.x
         + Bones[boneOffset + input.boneIndices.y].model * input.boneWeights.y
         + Bones[boneOffset + input.boneIndices.z].model * input.boneWeights.z
         + Bones[boneOffset + input.boneIndices.w].model * input.boneWeights.w;
}

fn packEntityId(id: vec2u) -> vec4u {
    return vec4u(id.x & 0xFFFFu, (id.x >> 16u) & 0xFFFFu, id.y & 0xFFFFu, (id.y >> 16u) & 0xFFFFu);
}

@vertex
fn vertexMain(input: VertexInput, @builtin(instance_index) drawCallIndex: u32) -> VertexOutput {
    var output: VertexOutput;
    let drawData = DrawInfo[drawCallIndex];
    let model = ModelDataObjectBuffer[drawData.transformIndex];

    let skin = skinMatrix(input, drawData.boneOffset);
    let skin3 = mat3x3<f32>(skin[0].xyz, skin[1].xyz, skin[2].xyz);
    let M3 = mat3x3<f32>(model.modelMatrix[0].xyz, model.modelMatrix[1].xyz, model.modelMatrix[2].xyz);

    output.position = camInfo.viewProj * (model.modelMatrix * (skin * vec4f(input.position, 1.0)));
    output.world_normal = model.normalMatrix * (skin3 * input.normal);
    output.uv = input.texcoord0;
    output.uv1 = input.texcoord1;
    output.alpha = input.color.a;
    output.drawIndex = drawCallIndex;
    output.mirror = select(1.0, -1.0, determinant(M3 * skin3) < 0.0);
    return output;
}

@fragment
fn fragmentMain(input: VertexOutput, @builtin(front_facing) frontFacing: bool) -> PrepassOut {
    let material = MaterialBuffer[DrawInfo[input.drawIndex].materialIndex];
    let uv = select(input.uv, input.uv1, (material.uvSets & 1u) != 0u);
    let alpha = textureSample(albedo, albedoSampler, uv).a * material.baseColor.a * input.alpha;
    if (alpha < material.alphaCutoff) {
        discard;
    }

    var out: PrepassOut;
    out.normal = vec4f(0.5, 0.5, 0.5, 1.0);
    let lengthSq = dot(input.world_normal, input.world_normal);
    if (lengthSq > 1e-8) {
        // Back faces (double-sided materials) face the other way, as in test.wgsl.
        let side = select(-1.0, 1.0, frontFacing) * input.mirror;
        let N = side * input.world_normal * inverseSqrt(lengthSq);
        out.normal = vec4f(normalize((camInfo.view * vec4f(N, 0.0)).xyz) * 0.5 + 0.5, 1.0);
    }
    out.pick = packEntityId(ModelDataObjectBuffer[DrawInfo[input.drawIndex].transformIndex].entityID);
    return out;
}
