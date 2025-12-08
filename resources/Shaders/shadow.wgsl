struct VertexInput {
    @location(0) position: vec3f,
    @location(1) normal: vec3f,
    @location(2) tangent: vec4f,
    @location(3) texcoord0: vec2f,
    @location(4) texcoord1: vec2f,
    @location(5) color: vec4f,
};

struct ShadowOut {
    @builtin(position) position : vec4f
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
  position: vec3f,
  exposure: f32,
};

@group(0) @binding(0) var<uniform> UniformBufferObject: UBO;

@group(1) @binding(0) var<uniform> ModelDataObject: ModelData;

@group(2) @binding(0) var<uniform> camInfo: CameraInfoData;

@vertex
fn vertexMain(input: VertexInput) -> ShadowOut {
  var out : ShadowOut;
  let worldPos = ModelDataObject.modelMatrix * vec4f(input.position, 1.0);
  out.position = UniformBufferObject.lightVP * worldPos;
  return out;
}


@fragment
fn fragmentMain() {
    }