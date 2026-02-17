#pragma once
#include "Cubemap.h"
#include "Defines.h"
#include "Shaders.h"
#include <vector>
#include <cmath>

namespace game {

// Global skybox instance
inline class Skybox* g_Skybox = nullptr;

// Skybox class - renders a cubemap as the environment background
class Skybox {
private:
    Cubemap cubemap_;
    bool enabled_ = true;
    
public:
    inline Skybox() : enabled_(true) {}
    inline ~Skybox() {}
    
    // Load cubemap from 6 file paths
    inline bool load(const std::array<const char*, 6>& facePaths) {
        return cubemap_.load(facePaths);
    }
    
    // Load from embedded data
    inline void loadFromData(const std::array<const unsigned int*, 6>& faceData, int width, int height) {
        cubemap_.loadFromData(faceData, width, height);
    }
    
    // Set a single face
    inline void setFace(CubeFace face, const unsigned int* data, int width, int height) {
        cubemap_.setFace(face, data, width, height);
    }
    
    // Get the cubemap for reflection sampling
    inline const Cubemap& getCubemap() const { return cubemap_; }
    inline Cubemap& getCubemap() { return cubemap_; }
    
    // Render the skybox
    // Uses the global SV_ViewMatrix and SV_ProjectionMatrix
    inline void render(unsigned int* screenBuffer, float* depthBuffer, int width, int height) {
        if (!enabled_ || !cubemap_.isLoaded()) return;
        
        // Extract camera axes from view matrix
        // The view matrix transforms world to camera, so the first 3 columns 
        // (transposed) give us camera right, up, forward in world space
        vec3 camRight = { SV_ViewMatrix.axisX.x, SV_ViewMatrix.axisY.x, SV_ViewMatrix.axisZ.x };
        vec3 camUp = { SV_ViewMatrix.axisX.y, SV_ViewMatrix.axisY.y, SV_ViewMatrix.axisZ.y };
        vec3 camForward = { SV_ViewMatrix.axisX.z, SV_ViewMatrix.axisY.z, SV_ViewMatrix.axisZ.z };
        
        // Compute FOV from projection matrix
        float tanHalfFovY = 1.0f / SV_ProjectionMatrix.axisY.y;
        float tanHalfFovX = 1.0f / SV_ProjectionMatrix.axisX.x;
        
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int idx = y * width + x;
                
                // Only draw skybox where nothing else has been drawn (depth at max)
                if (depthBuffer[idx] < 0.999f) continue;
                
                // Convert pixel to normalized device coordinates [-1, 1]
                float ndcX = (2.0f * x / width) - 1.0f;
                float ndcY = 1.0f - (2.0f * y / height);
                
                // Convert to view space direction
                float viewX = ndcX * tanHalfFovX;
                float viewY = ndcY * tanHalfFovY;
                float viewZ = 1.0f;
                
                // Convert to world space direction using camera axes
                vec3 worldDir = {
                    viewX * camRight.x + viewY * camUp.x + viewZ * camForward.x,
                    viewX * camRight.y + viewY * camUp.y + viewZ * camForward.y,
                    viewX * camRight.z + viewY * camUp.z + viewZ * camForward.z
                };
                
                // Sample cubemap and write to screen
                screenBuffer[idx] = cubemap_.sampleBGRA(worldDir);
            }
        }
    }
    
    // Enable/disable skybox
    inline void setEnabled(bool enabled) { enabled_ = enabled; }
    inline bool isEnabled() const { return enabled_; }
    
    // Check if loaded
    inline bool isLoaded() const { return cubemap_.isLoaded(); }
};

} // namespace game
