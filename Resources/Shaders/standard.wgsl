    struct VertexInput {
        @location(0) position: vec3f,
        @location(1) normal: vec3f,
        @location(2) color: vec3f,
        @location(3) uv: vec2f,
    };

    struct VertexOutput {
        @builtin(position) position: vec4f,
        @location(0) normal: vec3f,
        @location(1) color: vec3f,
        @location(2) uv: vec2f,
    };

    struct GB {
        projectionMatrix: mat4x4f,
        viewMatrix: mat4x4f,
        modelMatrix: mat4x4f,
        color: vec4f,
        time: f32,
    };

    @group(0) @binding(0) var<uniform> UBO: GB;
    @group(0) @binding(1) var gradientTexture: texture_2d<f32>;


    @vertex fn vertexMain(in: VertexInput) -> VertexOutput {
        var out: VertexOutput;

        out.position = UBO.projectionMatrix * UBO.viewMatrix * UBO.modelMatrix * vec4f(in.position, 1.0);
        out.normal = (UBO.modelMatrix * vec4f(in.normal, 0.0)).xyz;
        out.color = in.color;
        out.uv = in.uv;
        return out;
    }

    @fragment fn fragmentMain(in: VertexOutput) -> @location(0) vec4f {
        // let normal = normalize(in.normal);
        // let lightDirection1 = vec3f(0.5, -0.9, 0.1);
        // let shading = max(0.0, dot(lightDirection1, normal));
        // let color = in.color * shading;
        // let corrected_color = pow(color, vec3f(2.2));
        // return vec4f(corrected_color, UBO.color.a);

        let texelCoords = vec2i(in.uv * vec2f(textureDimensions(gradientTexture)));
        let color = textureLoad(gradientTexture, texelCoords, 0).rgb;
	    let corrected_color = pow(color, vec3f(2.2));
	    return vec4f(corrected_color, UBO.color.a);
}