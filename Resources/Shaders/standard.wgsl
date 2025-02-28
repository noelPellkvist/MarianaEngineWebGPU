struct VertexInput {
                @location(0) position: vec3f,
            };

            struct GB {
                projectionMatrix: mat4x4f,
                viewMatrix: mat4x4f,
            };

            @group(0) @binding(0) var<uniform> UBO: GB;

            @vertex fn vertexMain(input: VertexInput) ->
              @builtin(position) vec4f {
                return UBO.projectionMatrix * UBO.viewMatrix * vec4(input.position, 1.0);
                return vec4f(input.position.xyz, 1);
            }
            @fragment fn fragmentMain() -> @location(0) vec4f {
                return vec4f(1, 0, 0, 1);
            }