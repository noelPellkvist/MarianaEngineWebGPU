struct VertexInput {
    @location(0) position: vec3f,
};

struct GB {
    projectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
};

@group(0) @binding(0) var<uniform> UBO: GB;

@vertex fn vertexMain(@builtin(vertex_index) i : u32) -> @builtin(position) vec4f {
    const pos = array(vec2f(0, 1), vec2f(-1, -1), vec2f(1, -1));
    return vec4f(pos[i], 0, 1);
}
@fragment fn fragmentMain() -> @location(0) vec4f {
    return vec4f(1, 0, 0, 1);
}