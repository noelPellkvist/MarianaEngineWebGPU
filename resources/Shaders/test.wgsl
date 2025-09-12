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
    @location(2) world_worldbittangent: vec3f,
    @location(3) uv: vec2f,
};

struct UBO {
  projection: mat4x4<f32>,
  view: mat4x4<f32>,
  model: mat4x4<f32>,
  normalMatrix: mat4x4<f32>,
  lightDir: vec3f,
};

@group(0) @binding(0) var<uniform> UniformBufferObject: UBO;

@group(1) @binding(0) var albedo: texture_2d<f32>;
@group(1) @binding(1) var normalMap: texture_2d<f32>;
@group(1) @binding(2) var ambientO: texture_2d<f32>;
@group(1) @binding(3) var textureSampler: sampler;

@vertex
fn vertexMain(input: VertexInput) -> VertexOutput {
    var output: VertexOutput;
    let mvp = UniformBufferObject.projection * UniformBufferObject.view * UniformBufferObject.model;
    output.position = mvp * vec4f(input.position, 1.0);

    let Nw_raw = (UniformBufferObject.normalMatrix * vec4f(input.normal, 0.0)).xyz;
    let Tw_raw = (UniformBufferObject.model        * vec4f(input.tangent.xyz, 0.0)).xyz;

    let Nw = normalize(Nw_raw);
    let Tn = normalize(Tw_raw - Nw * dot(Tw_raw, Nw));
    let Bw = normalize(cross(Nw, Tn)) * input.tangent.w;

    output.world_normal = Nw;
    output.world_tangent = Tn;
    output.world_worldbittangent = Bw;
    output.uv = input.texcoord0;
    return output;
}

@fragment
fn fragmentMain(input: VertexOutput) -> @location(0) vec4f {

    var n = textureSample(normalMap, textureSampler, input.uv).xyz;
    n = n * 2.0 - 1.0;
    let strength = 1.0;
    n = normalize(mix(vec3f(0.0, 0.0, 1.0), n, strength));

    let TBN = mat3x3<f32>(input.world_tangent, input.world_worldbittangent, input.world_normal);
    let N = normalize(TBN * n);

    var albedoLinear = textureSample(albedo, textureSampler, input.uv).rgb;
    albedoLinear = pow(albedoLinear, vec3f(2.2));

    let aoStrength = 1.0;
    let ao = textureSample(ambientO, textureSampler, input.uv).r;
    let aoTerm = mix(1.0, ao, aoStrength);

    let L = normalize(UniformBufferObject.lightDir);

    let lambert = max(dot(N, L), 0.0);
    let ambient = 0.04 * aoTerm;
    let colorLinear = albedoLinear * (lambert + ambient);

    return vec4f(pow(colorLinear, vec3f(1.0/2.2)), 1.0);
}
