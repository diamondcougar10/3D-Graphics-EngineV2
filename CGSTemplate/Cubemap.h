#pragma once
#include "Texture.h"
#include "Defines.h"
#include <string>
#include <array>
#include <cmath>
#include <algorithm>

namespace game {

// Cubemap face indices
enum class CubeFace {
    Right = 0,   // +X
    Left = 1,    // -X
    Top = 2,     // +Y
    Bottom = 3,  // -Y
    Front = 4,   // +Z
    Back = 5     // -Z
};

// Forward declarations
class Cubemap;
inline CubeFace getFaceFromDirection(float x, float y, float z);
inline void getUVFromDirection(float x, float y, float z, CubeFace face, float& u, float& v);

// Currently bound cubemap (global for software renderer)
inline const Cubemap* g_BoundCubemap = nullptr;

// Cubemap class - holds 6 face textures for skybox/reflections
class Cubemap {
private:
    std::array<Texture, 6> faces_;
    bool loaded_ = false;
    
public:
    // Default constructor
    inline Cubemap() : loaded_(false) {}
    
    // Load cubemap from 6 file paths
    inline Cubemap(const std::array<const char*, 6>& facePaths) {
        load(facePaths);
    }
    
    // Load cubemap from embedded data arrays
    inline Cubemap(const std::array<const unsigned int*, 6>& faceData,
                   const std::array<int, 6>& widths,
                   const std::array<int, 6>& heights) {
        for (int i = 0; i < 6; ++i) {
            faces_[i].loadFromData(faceData[i], widths[i], heights[i]);
        }
        loaded_ = true;
    }
    
    // Destructor
    inline ~Cubemap() {}
    
    // Load from file paths
    inline bool load(const std::array<const char*, 6>& facePaths) {
        bool success = true;
        for (int i = 0; i < 6; ++i) {
            if (!faces_[i].load(facePaths[i])) {
                success = false;
            }
        }
        loaded_ = success;
        return success;
    }
    
    // Load from embedded data (all same size)
    inline void loadFromData(const std::array<const unsigned int*, 6>& faceData, int width, int height) {
        for (int i = 0; i < 6; ++i) {
            faces_[i].loadFromData(faceData[i], width, height);
        }
        loaded_ = true;
    }
    
    // Load a single face from data
    inline void setFace(CubeFace face, const unsigned int* data, int width, int height) {
        faces_[static_cast<int>(face)].loadFromData(data, width, height);
        
        // Check if all faces are loaded
        loaded_ = true;
        for (int i = 0; i < 6; ++i) {
            if (!faces_[i].isLoaded()) {
                loaded_ = false;
                break;
            }
        }
    }
    
    // Get a specific face texture
    inline const Texture& getFace(CubeFace face) const {
        return faces_[static_cast<int>(face)];
    }
    
    inline Texture& getFace(CubeFace face) {
        return faces_[static_cast<int>(face)];
    }
    
    // Check if loaded successfully
    inline bool isLoaded() const { return loaded_; }
    
    // Sample cubemap using a 3D direction vector
    inline unsigned int sample(float x, float y, float z) const {
        if (!loaded_) return 0xFF000000;
        
        float len = std::sqrt(x*x + y*y + z*z);
        if (len < 0.0001f) return 0xFF000000;
        x /= len; y /= len; z /= len;
        
        CubeFace face = getFaceFromDirection(x, y, z);
        float u, v;
        getUVFromDirection(x, y, z, face, u, v);
        
        return faces_[static_cast<int>(face)].sample(u, v);
    }
    
    inline unsigned int sample(const vec3& direction) const {
        return sample(direction.x, direction.y, direction.z);
    }
    
    // Sample with BGRA to ARGB conversion
    inline unsigned int sampleBGRA(float x, float y, float z) const {
        if (!loaded_) return 0xFF000000;
        
        float len = std::sqrt(x*x + y*y + z*z);
        if (len < 0.0001f) return 0xFF000000;
        x /= len; y /= len; z /= len;
        
        CubeFace face = getFaceFromDirection(x, y, z);
        float u, v;
        getUVFromDirection(x, y, z, face, u, v);
        
        return faces_[static_cast<int>(face)].sampleBGRA(u, v);
    }
    
    inline unsigned int sampleBGRA(const vec3& direction) const {
        return sampleBGRA(direction.x, direction.y, direction.z);
    }
    
    // Calculate reflection direction: R = I - 2(N·I)N
    static inline vec3 reflect(const vec3& incident, const vec3& normal) {
        float dot = incident.x * normal.x + incident.y * normal.y + incident.z * normal.z;
        return vec3{
            incident.x - 2.0f * dot * normal.x,
            incident.y - 2.0f * dot * normal.y,
            incident.z - 2.0f * dot * normal.z
        };
    }
    
    // Calculate refraction direction using Snell's law
    static inline vec3 refract(const vec3& incident, const vec3& normal, float eta) {
        float dotNI = incident.x * normal.x + incident.y * normal.y + incident.z * normal.z;
        float k = 1.0f - eta * eta * (1.0f - dotNI * dotNI);
        
        if (k < 0.0f) return reflect(incident, normal);
        
        float sqrtK = std::sqrt(k);
        return vec3{
            eta * incident.x - (eta * dotNI + sqrtK) * normal.x,
            eta * incident.y - (eta * dotNI + sqrtK) * normal.y,
            eta * incident.z - (eta * dotNI + sqrtK) * normal.z
        };
    }
    
    // Bind this cubemap as the current cubemap
    inline void bind() const { g_BoundCubemap = this; }
};

// Determine which face a direction vector points to
inline CubeFace getFaceFromDirection(float x, float y, float z) {
    float absX = std::abs(x);
    float absY = std::abs(y);
    float absZ = std::abs(z);
    
    if (absX >= absY && absX >= absZ) {
        return x > 0 ? CubeFace::Right : CubeFace::Left;
    }
    else if (absY >= absX && absY >= absZ) {
        return y > 0 ? CubeFace::Top : CubeFace::Bottom;
    }
    else {
        return z > 0 ? CubeFace::Front : CubeFace::Back;
    }
}

// Convert 3D direction to 2D UV coordinates on the selected face
inline void getUVFromDirection(float x, float y, float z, CubeFace face, float& u, float& v) {
    float ma = 0.0f, sc = 0.0f, tc = 0.0f;
    
    switch (face) {
        case CubeFace::Right:  ma = x;  sc = -z; tc = -y; break;
        case CubeFace::Left:   ma = -x; sc = z;  tc = -y; break;
        case CubeFace::Top:    ma = y;  sc = x;  tc = z;  break;
        case CubeFace::Bottom: ma = -y; sc = x;  tc = -z; break;
        case CubeFace::Front:  ma = z;  sc = x;  tc = -y; break;
        case CubeFace::Back:   ma = -z; sc = -x; tc = -y; break;
    }
    
    u = (sc / ma + 1.0f) * 0.5f;
    v = (tc / ma + 1.0f) * 0.5f;
    u = (std::max)(0.0f, (std::min)(1.0f, u));
    v = (std::max)(0.0f, (std::min)(1.0f, v));
}

} // namespace game
