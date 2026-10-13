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

struct ShadowOut {
    @builtin(position) position : vec4f,
    @location(0) uv: vec2f,
    @location(1) @interpolate(flat) drawIndex: u32,
    @location(2) uv1: vec2f,
    @location(3) alpha: f32, // vertex color alpha
};

// The light view-projection of the cascade being rendered (dynamic offset per cascade).
struct ShadowCascade {
  viewProj: mat4x4<f32>,
};

struct ModelData {
  modelMatrix: mat4x4<f32>,
  normalMatrix: mat3x3<f32>,
  entityID: vec2u,
};

@group(0) @binding(0) var<uniform> Cascade: ShadowCascade;

struct DrawData {
  transformIndex: u32,
  materialIndex: u32,
  boneOffset: u32,
};

struct BoneData {
  model: mat4x4<f32>,
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

@group(0) @binding(1) var<storage, read> ModelDataObjectBuffer: array<ModelData>;
@group(0) @binding(2) var<storage, read> DrawInfo: array<DrawData>;
@group(0) @binding(3) var<storage, read> Bones: array<BoneData>;

// Only bound for alpha-tested casters (ShadowMapMaskedShader). Group 1 is the PBR material group.
@group(0) @binding(4) var<storage, read> MaterialBuffer: array<MaterialProperties>;
@group(1) @binding(0) var albedo: texture_2d<f32>;
@group(1) @binding(5) var albedoSampler: sampler;

const NO_SKIN: u32 = 0xFFFFFFFFu;

// Same as test.wgsl: weighted blend of up to 4 joint matrices (identity without a skin).
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
fn vertexMain(input: VertexInput, @builtin(instance_index) drawIndex: u32) -> ShadowOut {
  var out : ShadowOut;
  // Casters sharing a mesh are drawn instanced; each instance looks up its own transform.
  let drawData = DrawInfo[drawIndex];
  var ModelDataObject = ModelDataObjectBuffer[drawData.transformIndex];
  let worldPos = ModelDataObject.modelMatrix * (skinMatrix(input, drawData.boneOffset) * vec4f(input.position, 1.0));
  out.position = Cascade.viewProj * worldPos;
  out.uv = input.texcoord0;
  out.uv1 = input.texcoord1;
  out.alpha = input.color.a;
  out.drawIndex = drawIndex;
  return out;
}


// Alpha-tested casters only (opaque ones have no fragment stage): cuts the same holes as the main
// pass, so leaves and fences cast their actual shape.
@fragment
fn fragmentMain(input: ShadowOut) {
  let material = MaterialBuffer[DrawInfo[input.drawIndex].materialIndex];
  let uv = select(input.uv, input.uv1, (material.uvSets & 1u) != 0u);
  let alpha = textureSample(albedo, albedoSampler, uv).a * material.baseColor.a * input.alpha;
  if (alpha < material.alphaCutoff) {
    discard;
  }
}