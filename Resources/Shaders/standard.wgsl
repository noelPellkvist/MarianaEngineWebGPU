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
        modelMatrix: mat4x4f,
        color: vec4f,
        time: f32,
    };

    @group(0) @binding(0) var<uniform> UBO: GB;


    @vertex fn vertexMain(in: VertexInput) -> VertexOutput {
        var out: VertexOutput;

        out.position = UBO.projectionMatrix * UBO.viewMatrix * UBO.modelMatrix * vec4f(in.position, 1.0);
        out.normal = (UBO.modelMatrix * vec4f(in.normal, 0.0)).xyz;
        return out;
    }

    @fragment fn fragmentMain(in: VertexOutput) -> @location(0) vec4f {
        let color = in.normal * UBO.color.rgb;
        let corrected_color = pow(color, vec3f(2.2));
        return vec4f(corrected_color, UBO.color.a);
}