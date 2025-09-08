struct VertexInput {
        @location(0) position: vec3f,
        @location(1) normal: vec3f,
    };

    struct VertexOutput {
      @builtin(position) position: vec4f,
      @location(0) normal: vec3f
    };

    @vertex fn vertexMain(input: VertexInput) ->
      VertexOutput {
        var output: VertexOutput;
        output.position = vec4f(input.position.x, input.position.y, 0, 1);
        output.normal = input.normal;
        return output;
    }
    @fragment fn fragmentMain(input: VertexOutput) -> @location(0) vec4f {
        return vec4f(input.normal, 1);
    }