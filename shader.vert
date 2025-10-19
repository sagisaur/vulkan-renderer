#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;
struct Vertex {
	float vx, vy, vz;
	float nx, ny, nz;
	float tu, tv;
};
layout (set = 1, binding = 0) readonly buffer Vertices {
    Vertex vertices[];
};

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragTexCoords;

void main() {
    Vertex v = vertices[gl_VertexIndex];
    vec3 inPosition = vec3(v.vx, v.vy, v.vz);
    vec3 inNormal = vec3(v.nx, v.ny, v.nz);
    vec2 inTexCoords = vec2(v.tu, v.tv);

    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    fragNormal = inNormal;
    fragTexCoords = inTexCoords;
}