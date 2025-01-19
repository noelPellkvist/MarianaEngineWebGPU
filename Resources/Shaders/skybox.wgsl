// Vertex Input and Output
struct VertexInput {
    @location(0) position: vec3f,
    @location(1) normal: vec3f,
    @location(2) color: vec3f,  // Not used in the fragment shader as per your request
    @location(3) uv: vec2f,
};

struct SkinnedVertex {
    @location(4) indices: vec4<i32>,  // 4 signed 32-bit integers (bone indices)
    @location(5) weights: vec4f,  // 4 floating-point values (bone weights)
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

struct MaterialProperties {
    baseColorFactor: vec4f,  
    emissiveFactor: vec3f,
    alphaCutoff: f32,        
    metallicFactor: f32,     
    roughnessFactor: f32,    
    textureFlags: u32,
};

struct ModelData {
    modelMatrix: mat4x4f,    // 64 bytes
    normalMatrix: mat4x4f,
    material: MaterialProperties, // 48 bytes
};

struct BonesData {
    modelMatrix: mat4x4f,
    normalMatrix: mat4x4f,
};

@group(0) @binding(0) var<uniform> UBO: GB;
@group(0) @binding(1) var textureSampler: sampler;

@group(1) @binding(0) var<uniform> Model: ModelData;

@group(2) @binding(0) var<storage, read> bones: array<BonesData>;

@group(3) @binding(0) var albedoTexture: texture_2d<f32>;

fn extract_mat3x3(m: mat4x4<f32>) -> mat3x3<f32> {
    return mat3x3<f32>(
        m[0].xyz, // First row
        m[1].xyz, // Second row
        m[2].xyz  // Third row
    );
}

@vertex
fn vertex_main(input: VertexInput, skin: SkinnedVertex) -> VertexOutput {
    var output: VertexOutput;

    output.position = (UBO.projectionMatrix * mat4x4<f32>(
            vec4<f32>(UBO.viewMatrix[0].xyz, 0.0),
            vec4<f32>(UBO.viewMatrix[1].xyz, 0.0),
            vec4<f32>(UBO.viewMatrix[2].xyz, 0.0),
            vec4<f32>(0.0, 0.0, 0.0, 1.0)
        ) * vec4<f32>(input.position, 1.0)).xyww;

    return output;
}

// Fragment Shader
@fragment
fn fragment_main(input: VertexOutput) -> @location(0) vec4f {
    return vec4f(1, 0, 0, 1);
    // let sampleDir = vec3f(-1.0, 0.0, 0.0);
    // return textureSample(cubemap, textureSampler, sampleDir);
}