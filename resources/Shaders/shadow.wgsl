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
  normalMatrix: mat3x3<f32>,
  entityID: vec2u,
};

@group(0) @binding(0) var<uniform> UniformBufferObject: UBO;

@group(0) @binding(1) var<storage, read> ModelDataObjectBuffer: array<ModelData>;


@vertex
fn vertexMain(input: VertexInput) -> ShadowOut {
  var out : ShadowOut;
  var ModelDataObject = ModelDataObjectBuffer[0];
  let worldPos = ModelDataObject.modelMatrix * vec4f(input.position, 1.0);
  out.position = UniformBufferObject.lightVP * worldPos;
  return out;
}


@fragment
fn fragmentMain() {
    }