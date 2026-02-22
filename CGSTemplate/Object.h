#pragma once
#include "MathEq.h"
#include <vector>

namespace game {

// Base class for all renderable objects in the scene
class Object {
protected:
    vec3 position = { 0.0f, 0.0f, 0.0f };
    vec3 rotation = { 0.0f, 0.0f, 0.0f };  // Euler angles in degrees
    vec3 scale = { 1.0f, 1.0f, 1.0f };
    vec3 color = { 1.0f, 1.0f, 1.0f };
    bool visible = true;
    
    // Cached world matrix
    matrix4x4 worldMatrix;
    bool matrixDirty = true;

public:
    Object() { worldMatrix = MatrixIdentity(); }
    virtual ~Object() = default;
    
    // Pure virtual methods - must be implemented by derived classes
    virtual void render() = 0;
    virtual void update(float dt) = 0;
    
    // Transform setters
    void setPosition(const vec3& pos) {
        position = pos;
        matrixDirty = true;
    }
    
    void setPosition(float x, float y, float z) {
        position = { x, y, z };
        matrixDirty = true;
    }
    
    void setRotation(const vec3& rot) {
        rotation = rot;
        matrixDirty = true;
    }
    
    void setRotation(float x, float y, float z) {
        rotation = { x, y, z };
        matrixDirty = true;
    }
    
    void setScale(const vec3& s) {
        scale = s;
        matrixDirty = true;
    }
    
    void setScale(float x, float y, float z) {
        scale = { x, y, z };
        matrixDirty = true;
    }
    
    void setScale(float uniform) {
        scale = { uniform, uniform, uniform };
        matrixDirty = true;
    }
    
    void setColor(const vec3& c) {
        color = c;
    }
    
    void setColor(float r, float g, float b) {
        color = { r, g, b };
    }
    
    void setVisible(bool v) { visible = v; }
    
    // Transform getters
    const vec3& getPosition() const { return position; }
    const vec3& getRotation() const { return rotation; }
    const vec3& getScale() const { return scale; }
    const vec3& getColor() const { return color; }
    bool isVisible() const { return visible; }
    
    // Get world matrix (recalculates if dirty)
    const matrix4x4& getWorldMatrix() {
        if (matrixDirty) {
            updateWorldMatrix();
        }
        return worldMatrix;
    }
    
    // Rotate by delta amounts (degrees)
    void rotate(float dx, float dy, float dz) {
        rotation.x += dx;
        rotation.y += dy;
        rotation.z += dz;
        matrixDirty = true;
    }
    
    // Translate by delta amounts
    void translate(float dx, float dy, float dz) {
        position.x += dx;
        position.y += dy;
        position.z += dz;
        matrixDirty = true;
    }

protected:
    void updateWorldMatrix() {
        // Build world matrix: Scale * RotationZ * RotationY * RotationX * Translation
        // Start with identity
        worldMatrix = MatrixIdentity();
        
        // Apply scale
        worldMatrix.axisX.x = scale.x;
        worldMatrix.axisY.y = scale.y;
        worldMatrix.axisZ.z = scale.z;
        
        // Apply rotation X (pitch)
        matrix4x4 rotX = matrixRotationX(rotation.x);
        worldMatrix = matrixMultiplicationMatrix(worldMatrix, rotX);
        
        // Apply rotation Y (yaw)
        worldMatrix = matrixRotationY(worldMatrix, rotation.y);
        
        // Apply rotation Z (roll)
        matrix4x4 rotZ = matrixRotationZ(rotation.z);
        worldMatrix = matrixMultiplicationMatrix(worldMatrix, rotZ);
        
        // Apply translation
        vec4 transVec = { position.x, position.y, position.z, 1.0f };
        matrix4x4 transMat = matrixTranslation(transVec);
        worldMatrix = matrixMultiplicationMatrix(worldMatrix, transMat);
        
        matrixDirty = false;
    }
};

} // namespace game
