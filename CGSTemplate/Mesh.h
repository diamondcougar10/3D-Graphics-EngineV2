#pragma once
#include "Object.h"
#include "Defines.h"
#include <vector>

namespace game {

// Mesh class - holds geometry data (vertices and indices)
class Mesh : public Object {
protected:
    std::vector<vertex> vertices;
    std::vector<unsigned int> indices;  // Triangles as index triplets
    
public:
    Mesh() : Object() {}
    
    Mesh(const std::vector<vertex>& verts, const std::vector<unsigned int>& inds)
        : Object(), vertices(verts), indices(inds) {}
    
    virtual ~Mesh() = default;
    
    // Set geometry
    void setGeometry(const std::vector<vertex>& verts, const std::vector<unsigned int>& inds) {
        vertices = verts;
        indices = inds;
    }
    
    // Get geometry
    const std::vector<vertex>& getVertices() const { return vertices; }
    const std::vector<unsigned int>& getIndices() const { return indices; }
    size_t getTriangleCount() const { return indices.size() / 3; }
    
    // Get vertex by index
    const vertex& getVertex(size_t idx) const { return vertices[idx]; }
    
    // Override in derived classes
    void render() override {
        // Base mesh doesn't render - see MaterialMesh
    }
    
    void update(float dt) override {
        // Default: no-op, override for animated meshes
    }
    
    // Static factory methods for common primitives
    static std::vector<vertex> createCubeVertices();
    static std::vector<unsigned int> createCubeIndices();
};

// Cube vertex data factory (24 vertices for per-face UVs)
inline std::vector<vertex> Mesh::createCubeVertices() {
    std::vector<vertex> verts;
    verts.reserve(24);
    
    // Helper to create vertex with position and UV
    auto makeVert = [](float x, float y, float z, float u, float v) {
        return vertex(vec4{x, y, z, 1.0f}, 0xFFFFFFFF, u, v);
    };
    
    // Front face (Z = 0.5)
    verts.push_back(makeVert(-0.5f,  0.5f, 0.5f, 0.0f, 0.0f));  // TL
    verts.push_back(makeVert( 0.5f,  0.5f, 0.5f, 1.0f, 0.0f));  // TR
    verts.push_back(makeVert( 0.5f, -0.5f, 0.5f, 1.0f, 1.0f));  // BR
    verts.push_back(makeVert(-0.5f, -0.5f, 0.5f, 0.0f, 1.0f));  // BL
    
    // Back face (Z = -0.5)
    verts.push_back(makeVert( 0.5f,  0.5f, -0.5f, 0.0f, 0.0f));  // TL
    verts.push_back(makeVert(-0.5f,  0.5f, -0.5f, 1.0f, 0.0f));  // TR
    verts.push_back(makeVert(-0.5f, -0.5f, -0.5f, 1.0f, 1.0f));  // BR
    verts.push_back(makeVert( 0.5f, -0.5f, -0.5f, 0.0f, 1.0f));  // BL
    
    // Top face (Y = 0.5)
    verts.push_back(makeVert(-0.5f,  0.5f, -0.5f, 0.0f, 0.0f));  // TL
    verts.push_back(makeVert( 0.5f,  0.5f, -0.5f, 1.0f, 0.0f));  // TR
    verts.push_back(makeVert( 0.5f,  0.5f,  0.5f, 1.0f, 1.0f));  // BR
    verts.push_back(makeVert(-0.5f,  0.5f,  0.5f, 0.0f, 1.0f));  // BL
    
    // Bottom face (Y = -0.5)
    verts.push_back(makeVert(-0.5f, -0.5f,  0.5f, 0.0f, 0.0f));  // TL
    verts.push_back(makeVert( 0.5f, -0.5f,  0.5f, 1.0f, 0.0f));  // TR
    verts.push_back(makeVert( 0.5f, -0.5f, -0.5f, 1.0f, 1.0f));  // BR
    verts.push_back(makeVert(-0.5f, -0.5f, -0.5f, 0.0f, 1.0f));  // BL
    
    // Left face (X = -0.5)
    verts.push_back(makeVert(-0.5f,  0.5f, -0.5f, 0.0f, 0.0f));  // TL
    verts.push_back(makeVert(-0.5f,  0.5f,  0.5f, 1.0f, 0.0f));  // TR
    verts.push_back(makeVert(-0.5f, -0.5f,  0.5f, 1.0f, 1.0f));  // BR
    verts.push_back(makeVert(-0.5f, -0.5f, -0.5f, 0.0f, 1.0f));  // BL
    
    // Right face (X = 0.5)
    verts.push_back(makeVert( 0.5f,  0.5f,  0.5f, 0.0f, 0.0f));  // TL
    verts.push_back(makeVert( 0.5f,  0.5f, -0.5f, 1.0f, 0.0f));  // TR
    verts.push_back(makeVert( 0.5f, -0.5f, -0.5f, 1.0f, 1.0f));  // BR
    verts.push_back(makeVert( 0.5f, -0.5f,  0.5f, 0.0f, 1.0f));  // BL
    
    return verts;
}

inline std::vector<unsigned int> Mesh::createCubeIndices() {
    std::vector<unsigned int> inds;
    inds.reserve(36);
    
    // 6 faces, 2 triangles each, 3 indices per triangle
    for (unsigned int face = 0; face < 6; face++) {
        unsigned int base = face * 4;
        // Triangle 1: TL, TR, BR
        inds.push_back(base + 0);
        inds.push_back(base + 1);
        inds.push_back(base + 2);
        // Triangle 2: TL, BR, BL
        inds.push_back(base + 0);
        inds.push_back(base + 2);
        inds.push_back(base + 3);
    }
    
    return inds;
}

} // namespace game
