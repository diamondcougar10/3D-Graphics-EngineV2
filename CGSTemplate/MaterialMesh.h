#pragma once
#include "Mesh.h"
#include "Shaders.h"
#include "Cubemap.h"

namespace game {

// Forward declarations for rendering functions (defined in main.cpp or RasterHelper.h)
// These will be set by the engine at startup
struct RenderCallbacks {
    void (*drawTexturedTriangleGPU)(vertex, vertex, vertex) = nullptr;
    void (*drawTriangleGPU)(vertex, vertex, vertex, unsigned int) = nullptr;
    void (*drawTriangleCPU)(vertex&, vertex&, vertex&, const unsigned*, int, int) = nullptr;
    bool useGPU = false;
    const unsigned int* texture = nullptr;
    int texWidth = 0;
    int texHeight = 0;
};

// Global render callbacks (set by engine)
inline RenderCallbacks g_RenderCallbacks;

// MaterialMesh - renderable mesh with texture/material support
class MaterialMesh : public Mesh {
private:
    const unsigned int* texture = nullptr;
    int texWidth = 0;
    int texHeight = 0;
    bool useTexture = true;
    float rotationSpeed = 0.0f;  // For auto-rotation
    
    // Environment mapping properties
    const Cubemap* envMap = nullptr;     // Environment cubemap for reflections
    float reflectivity = 0.0f;           // 0 = no reflection, 1 = mirror
    float refractiveIndex = 1.0f;        // For refraction (1.0 = no refraction)

public:
    MaterialMesh() : Mesh() {}
    
    MaterialMesh(const std::vector<vertex>& verts, const std::vector<unsigned int>& inds)
        : Mesh(verts, inds) {}
    
    MaterialMesh(const std::vector<vertex>& verts, const std::vector<unsigned int>& inds,
                 const unsigned int* tex, int tw, int th)
        : Mesh(verts, inds), texture(tex), texWidth(tw), texHeight(th) {}
    
    virtual ~MaterialMesh() = default;
    
    // Texture settings
    void setTexture(const unsigned int* tex, int w, int h) {
        texture = tex;
        texWidth = w;
        texHeight = h;
    }
    
    void setUseTexture(bool use) { useTexture = use; }
    bool getUseTexture() const { return useTexture; }
    
    // Animation
    void setRotationSpeed(float speed) { rotationSpeed = speed; }
    float getRotationSpeed() const { return rotationSpeed; }
    
    // Environment mapping / Reflections
    void setEnvironmentMap(const Cubemap* cubemap) { envMap = cubemap; }
    const Cubemap* getEnvironmentMap() const { return envMap; }
    
    void setReflectivity(float r) { reflectivity = (r < 0) ? 0 : (r > 1 ? 1 : r); }
    float getReflectivity() const { return reflectivity; }
    
    void setRefractiveIndex(float ri) { refractiveIndex = ri; }
    float getRefractiveIndex() const { return refractiveIndex; }
    
    // Update - handle auto rotation
    void update(float dt) override {
        if (rotationSpeed != 0.0f) {
            rotate(0, rotationSpeed * dt, 0);
        }
    }
    
    // Render the mesh
    void render() override {
        if (!visible || indices.empty()) return;
        
        // Set world matrix for this object
        SV_WorldMatrix = getWorldMatrix();
        
        // Get color as unsigned int
        unsigned int meshColor = colorToUint(color);
        
        // Determine texture to use
        const unsigned int* tex = useTexture ? (texture ? texture : g_RenderCallbacks.texture) : nullptr;
        int tw = useTexture ? (texture ? texWidth : g_RenderCallbacks.texWidth) : 0;
        int th = useTexture ? (texture ? texHeight : g_RenderCallbacks.texHeight) : 0;
        
        // Render each triangle
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            vertex v0 = vertices[indices[i]];
            vertex v1 = vertices[indices[i + 1]];
            vertex v2 = vertices[indices[i + 2]];
            
            // Apply mesh color to vertices
            v0.color = v1.color = v2.color = meshColor;
            
            if (g_RenderCallbacks.useGPU) {
                // GPU rendering
                if (useTexture && tex && g_RenderCallbacks.drawTexturedTriangleGPU) {
                    g_RenderCallbacks.drawTexturedTriangleGPU(v0, v1, v2);
                } else if (g_RenderCallbacks.drawTriangleGPU) {
                    g_RenderCallbacks.drawTriangleGPU(v0, v1, v2, meshColor);
                }
            } else {
                // CPU rendering
                if (g_RenderCallbacks.drawTriangleCPU) {
                    g_RenderCallbacks.drawTriangleCPU(v0, v1, v2, tex, tw, th);
                }
            }
        }
    }
    
private:
    static unsigned int colorToUint(const vec3& c) {
        unsigned int r = static_cast<unsigned int>(c.x * 255.0f);
        unsigned int g = static_cast<unsigned int>(c.y * 255.0f);
        unsigned int b = static_cast<unsigned int>(c.z * 255.0f);
        if (r > 255) r = 255;
        if (g > 255) g = 255;
        if (b > 255) b = 255;
        return 0xFF000000 | (r << 16) | (g << 8) | b;
    }
};

// ========== OBJECT MANAGER ==========
// Manages all objects in the scene

class ObjectManager {
private:
    std::vector<Object*> objects;
    
public:
    ObjectManager() = default;
    
    ~ObjectManager() {
        clear();
    }
    
    // Add object (takes ownership)
    void addObject(Object* obj) {
        if (obj) {
            objects.push_back(obj);
        }
    }
    
    // Remove object
    void removeObject(Object* obj) {
        auto it = std::find(objects.begin(), objects.end(), obj);
        if (it != objects.end()) {
            delete *it;
            objects.erase(it);
        }
    }
    
    // Clear all objects
    void clear() {
        for (auto* obj : objects) {
            delete obj;
        }
        objects.clear();
    }
    
    // Update all objects
    void updateAll(float dt) {
        for (auto* obj : objects) {
            obj->update(dt);
        }
    }
    
    // Render all objects
    void renderAll() {
        for (auto* obj : objects) {
            if (obj->isVisible()) {
                obj->render();
            }
        }
    }
    
    // Get object count
    size_t getObjectCount() const { return objects.size(); }
    
    // Get object by index
    Object* getObject(size_t index) {
        return (index < objects.size()) ? objects[index] : nullptr;
    }
    
    // Get all objects (for iteration)
    const std::vector<Object*>& getObjects() const { return objects; }
};

// Global object manager
inline ObjectManager g_ObjectManager;

} // namespace game
