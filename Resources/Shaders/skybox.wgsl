// Vertex Input and Output
struct VertexInput {
    @location(0) position: vec3f,
    @location(1) normal: vec3f,
    @location(2) color: vec3f,  // Not used in the fragment shader as per your request
    @location(3) uv: vec2f,
};

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) normal: vec3f,
    @location(1) color: vec3f,  // Same as input color, not used in the fragment
    @location(2) worldpos: vec3f,
    @location(3) uv: vec2f,
};

// Uniform Buffer Object (UBO) to hold matrix and light information
struct GB {
    projectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
    modelMatrix: mat4x4f,
    color: vec4f,   // Light direction stored as a color (vec3f part)
    time: f32,
};

@group(0) @binding(0) var<uniform> UBO: GB;
@group(0) @binding(1) var textureSampler: sampler;
@group(0) @binding(2) var cubemap: texture_cube<f32>;

// Vertex Shader
@vertex
fn vertex_main(input: VertexInput) -> VertexOutput {
    var output: VertexOutput;

    output.normal = input.position;
    output.position = (UBO.projectionMatrix * mat4x4<f32>(
            vec4<f32>(UBO.viewMatrix[0].xyz, 0.0), // First row of view matrix (with padding)
            vec4<f32>(UBO.viewMatrix[1].xyz, 0.0), // Second row of view matrix (with padding)
            vec4<f32>(UBO.viewMatrix[2].xyz, 0.0), // Third row of view matrix (with padding)
            vec4<f32>(0.0, 0.0, 0.0, 1.0)         // Fourth row (homogeneous coordinate)
        ) * vec4<f32>(input.position, 1.0)).xyww;

    return output;
}

// Fragment Shader
@fragment
fn fragment_main(input: VertexOutput) -> @location(0) vec4f {
    let sampleDir = vec3f(-1.0, 0.0, 0.0);
    return textureSample(cubemap, textureSampler, sampleDir);
}