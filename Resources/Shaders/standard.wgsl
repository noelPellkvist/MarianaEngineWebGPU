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

struct MaterialProperties {
    baseColorFactor: vec4f,  
    emissiveFactor: vec3f,
    alphaCutoff: f32,        
    metallicFactor: f32,     
    roughnessFactor: f32,    
    
};

struct ModelData {
    modelMatrix: mat4x4f,    // 64 bytes
    material: MaterialProperties, // 48 bytes
};

@group(0) @binding(0) var<uniform> UBO: GB;
@group(0) @binding(1) var textureSampler: sampler;

@group(1) @binding(0) var<uniform> Model: ModelData;

// Vertex Shader
@vertex
fn vertex_main(input: VertexInput) -> VertexOutput {
    var output: VertexOutput;

    // Transform the vertex position to clip space
    output.position = UBO.projectionMatrix * UBO.viewMatrix * Model.modelMatrix * vec4(input.position, 1.0);

    // Pass through the normal and other vertex data
    output.normal = normalize((Model.modelMatrix * vec4(input.normal, 0.0)).xyz);
    output.worldpos = (Model.modelMatrix * vec4(input.position, 1.0)).xyz;
    output.uv = input.uv;
    output.color = input.color;

    return output;
}

// Fragment Shader
@fragment
fn fragment_main(input: VertexOutput) -> @location(0) vec4f {
    // // Fetch textures
    let albedoColor = Model.material.baseColorFactor;
    // let metallicRoughness = textureSample(metallicRoughness, textureSampler, input.uv);
    // let ao = textureSample(aoTexture, textureSampler, input.uv);

    // // Extract metallic and roughness values from the metallic-roughness map
    let metallic = Model.material.metallicFactor;  // Assume metallic is stored in the Red channel
    let roughness = Model.material.roughnessFactor; // Roughness stored in the Green channel 

    // // Normalize normal (for more realistic lighting calculations)
    let normal = normalize(input.normal);

    // // Lighting calculations
    // // Light Direction (as a simple color from the UBO)
    let lightDir = normalize(UBO.color.xyz); // Assuming color is the light direction in the UBO
    let viewDir = normalize(UBO.viewMatrix[3].xyz - input.worldpos); // Camera to fragment direction

    // // Simple ambient light (constant)
    let ambient = 0.1; // Ambient intensity

    // // Lambertian diffuse lighting (cosine of the angle between light and normal)
    let diffuse = max(dot(normal, lightDir), 0.0);

    // // Simple specular reflection using the Phong model (No Fresnel)
    // // Blinn-Phong specular
    let halfVector = normalize(lightDir + viewDir);
    let specular = pow(max(dot(normal, halfVector), 0.0), (1.0 - roughness) * 256.0); // Roughness controls shininess

    // // Ambient Occlusion (AO) to darken crevices
    // let aoFactor = ao.r; // AO map in red channel

    // // Combine all components
    let diffuseColor = albedoColor.rgb * diffuse; // Diffuse color modulated by lighting
    let specularColor = vec3f(1.0) * specular * (1.0 - metallic); // Specular reflection modulated by metallic
    let ambientColor = albedoColor.rgb * ambient; // Ambient lighting, affected by AO

    // // Final color is a combination of all lighting contributions
    let finalColor = ambientColor + diffuseColor + specularColor;
    return Model.material.baseColorFactor;
    // // Apply the AO factor and combine with albedo color
    return vec4f(finalColor, albedoColor.a); // Use the alpha from albedo texture (if available)
    //return Model.material.baseColorFactor;
    //return vec4f(0.0965700075, 0.0965700075, 0.0965700075, 0.0965700075);
}