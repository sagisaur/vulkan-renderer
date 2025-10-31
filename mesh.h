struct Vertex {
	float16_t vx, vy, vz, vw; // vw is only for alignment
	uint8_t nx, ny, nz, nw; // nw is only for alignment
	float16_t tu, tv;
};

struct Meshlet {
    // those point (index) to the actual global buffer, but they contain unique numbers
    uint vertices[64];
    // those point to the local vertex array above, so range is [0, 63] 
    uint8_t indices[126*3]; // up to 126 triangles
    uint8_t triangleCount; // max 126
    uint8_t vertexCount;  // max 64 unique vertices
};