struct VertexInput {
    @location(0) position: vec3f,
    @location(1) normal: vec3f,
};

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) normal: vec3f,
};

struct GB {
    projectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
};

struct TransformData {
    modelMatrix: mat4x4f,
    normalMatrix: mat4x4f,
};

@group(0) @binding(0) var<uniform> UBO: GB;
@group(1) @binding(0) var<uniform> transform: TransformData;

fn extract_mat3x3(m: mat4x4<f32>) -> mat3x3<f32> {
    return mat3x3<f32>(
        m[0].xyz, // First row
        m[1].xyz, // Second row
        m[2].xyz  // Third row
    );
}

@vertex fn vertexMain(input: VertexInput) ->
  VertexOutput  {
    var output: VertexOutput;
    output.position = UBO.projectionMatrix * UBO.viewMatrix * transform.modelMatrix * vec4(input.position, 1.0);
    output.normal = normalize(extract_mat3x3(transform.normalMatrix) * input.normal);
    return output;
}

@fragment fn fragment_main(input: VertexOutput) -> @location(0) vec4f {
    let lightDir = normalize(vec3f(1,1,1));
    let diffuseIntensity = max(dot(normalize(input.normal), lightDir), 0.0) + 0.2;
    return vec4f(1, 0, 0, 1) * diffuseIntensity;
}