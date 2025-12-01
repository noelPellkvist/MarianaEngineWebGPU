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
    @location(0) normal: vec3f,
    @location(1) uv: vec2f,
};

struct UBO {
  projection: mat4x4<f32>,
  view: mat4x4<f32>,
  model: mat4x4<f32>,
};

@group(0) @binding(0) var<uniform> UniformBufferObject: UBO;

@group(1) @binding(0) var albedoTexture: texture_2d<f32>;
@group(1) @binding(1) var textureSampler: sampler;

@vertex
fn vertexMain(input: VertexInput) -> VertexOutput {
    var output: VertexOutput;
    let mvp = UniformBufferObject.projection * UniformBufferObject.view * UniformBufferObject.model;
    output.position = mvp * vec4f(input.position, 1.0);
    output.normal = input.normal;
    output.uv = input.texcoord0;
    return output;
}

@fragment
fn fragmentMain(input: VertexOutput) -> @location(0) vec4f {
    // Visualize normals as color
    //return vec4f(normalize(input.normal) * 0.5 + vec3f(0.5), 1.0);
    //return vec4f(1,1,1,1);
    let color = textureSample(albedoTexture, textureSampler, input.uv).rgb;
    let corrected_color = pow(color, vec3f(2.2));
    return vec4f(corrected_color, 1.0);
}
