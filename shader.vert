#version 460
#extension GL_EXT_shader_8bit_storage: require
#extension GL_EXT_shader_16bit_storage: require
#extension GL_EXT_shader_explicit_arithmetic_types: require

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

struct Vertex {
	float16_t vx, vy, vz, vw; // vw is only for alignment
	uint8_t nx, ny, nz, nw; // nw is only for alignment
	float16_t tu, tv;
};
layout(set = 1, binding = 0) readonly buffer Vertices {
    Vertex vertices[];
};

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragTexCoords;

void main() {
    Vertex v = vertices[gl_VertexIndex];
    vec3 inPosition = vec3(v.vx, v.vy, v.vz);
    vec3 inNormal = vec3(v.nx, v.ny, v.nz) / 255.0 * 2.0 - 1.0; // convert it from [0, 255] to [-1.0f, 1.0f]
    vec2 inTexCoords = vec2(v.tu, v.tv);

    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    fragNormal = inNormal;
    fragTexCoords = inTexCoords;
}