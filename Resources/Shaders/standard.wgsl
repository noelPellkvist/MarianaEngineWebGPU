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
    material: MaterialProperties, // 48 bytes
};

@group(0) @binding(0) var<uniform> UBO: GB;
@group(0) @binding(1) var textureSampler: sampler;

@group(1) @binding(0) var<uniform> Model: ModelData;

@group(2) @binding(0) var<storage, read> bones: array<mat4x4f>;

@group(3) @binding(0) var albedoTexture: texture_2d<f32>;

// Vertex Shader
@vertex
fn vertex_main(input: VertexInput, skin: SkinnedVertex) -> VertexOutput {
    var output: VertexOutput;
    var modelMatrix : mat4x4f;
    if(skin.indices[0] != -1)
    {
        modelMatrix = skin.weights.x * bones[skin.indices.x] + 
                          skin.weights.y * bones[skin.indices.y] + 
                          skin.weights.z * bones[skin.indices.z] + 
                          skin.weights.w * bones[skin.indices.w];
    }
    else
    {
        modelMatrix = Model.modelMatrix;
    }

    output.position = UBO.projectionMatrix * UBO.viewMatrix * modelMatrix * vec4(input.position, 1.0);

    
    output.normal = normalize((Model.modelMatrix * vec4(input.normal, 0.0)).xyz);
    output.worldpos = (Model.modelMatrix * vec4(input.position, 1.0)).xyz;
    output.uv = input.uv;
    output.color = input.color;

    return output;
}

@fragment
fn fragment_main(input: VertexOutput) -> @location(0) vec4f {
    var baseColor = Model.material.baseColorFactor * vec4f(input.color, 1.0);
    if ((Model.material.textureFlags & (1 << 0)) != 0) {
        // Sample the texture only if the flag is set
        baseColor *= textureSample(albedoTexture, textureSampler, input.uv);
    }
    let normal = normalize(input.normal);

    let lightDir = normalize(UBO.color.xyz); 

    let ambientIntensity = 0.1;

    let diffuseIntensity = max(dot(normal, lightDir), 0.0);

    let color = baseColor.rgb * (ambientIntensity + diffuseIntensity);

    //return vec4f(color, baseColor.a);
    return baseColor;
}