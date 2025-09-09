struct VertexInput {
    @location(0) position: vec3f,
    @location(1) normal: vec3f,
};

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) normal: vec3f,
};

// Hardcoded projection * view * model matrix
fn getProjViewModel() -> mat4x4<f32> {
    // Perspective matrix
    let fov: f32 = 60.0 * 3.14159265 / 180.0; // radians
    let aspect: f32 = 16.0 / 9.0;
    let znear: f32 = 0.1;
    let zfar: f32 = 100.0;

    let f: f32 = 1.0 / tan(fov * 0.5);

    // Column-major mat4x4
    return mat4x4<f32>(
        vec4<f32>(f / aspect, 0.0, 0.0, 0.0),
        vec4<f32>(0.0, f, 0.0, 0.0),
        vec4<f32>(0.0, 0.0, (zfar + znear) / (znear - zfar), -1.0),
        vec4<f32>(0.0, 0.0, (2.0 * zfar * znear) / (znear - zfar), 0.0)
    );
}

@vertex
fn vertexMain(input: VertexInput) -> VertexOutput {
    var output: VertexOutput;
    let mvp = getProjViewModel();
    output.position = mvp * vec4f(input.position, 1.0);
    output.normal = input.normal;
    return output;
}

@fragment
fn fragmentMain(input: VertexOutput) -> @location(0) vec4f {
    // Visualize normals as color
    return vec4f(normalize(input.normal) * 0.5 + vec3f(0.5), 1.0);
}
