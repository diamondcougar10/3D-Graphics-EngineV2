#pragma once

#include "MathEq.h"


void (*VertexShader)(vertex&) = nullptr;
void (*PixelShader)(Pixel&) = nullptr;

matrix4x4 SV_WorldMatrix;
matrix4x4 SV_ViewMatrix;
matrix4x4 SV_ProjectionMatrix;

// Sun light direction (normalized) - like sun from upper-right
vec3 SV_LightDirection = { 0.6f, 0.8f, -0.3f };
float SV_AmbientLight = 0.15f;  // Space is dark, low ambient
vec3 SV_SunColor = { 1.0f, 0.95f, 0.8f }; // Warm sunlight tint

// Normalize a vec3
vec3 vec3Normalize(vec3 v) {
    float len = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len > 0.0001f) {
        v.x /= len;
        v.y /= len;
        v.z /= len;
    }
    return v;
}

// Cross product of two vec3
vec3 vec3Cross(vec3 a, vec3 b) {
    vec3 result;
    result.x = a.y * b.z - a.z * b.y;
    result.y = a.z * b.x - a.x * b.z;
    result.z = a.x * b.y - a.y * b.x;
    return result;
}

// Dot product of two vec3
float vec3Dot(vec3 a, vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

// Subtract two vec3
vec3 vec3Sub(vec3 a, vec3 b) {
    vec3 result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    result.z = a.z - b.z;
    return result;
}

// Calculate face normal from three vertices (in world space)
vec3 calculateFaceNormal(vec4 v0, vec4 v1, vec4 v2) {
    vec3 edge1 = { v1.x - v0.x, v1.y - v0.y, v1.z - v0.z };
    vec3 edge2 = { v2.x - v0.x, v2.y - v0.y, v2.z - v0.z };
    vec3 normal = vec3Cross(edge1, edge2);
    return vec3Normalize(normal);
}

// Calculate lighting factor for a face
float calculateLighting(vec3 faceNormal) {
    vec3 lightDir = vec3Normalize(SV_LightDirection);
    float diffuse = vec3Dot(faceNormal, lightDir);
    // Clamp to positive values and add ambient
    diffuse = (diffuse < 0.0f) ? 0.0f : diffuse;
    float lighting = SV_AmbientLight + (1.0f - SV_AmbientLight) * diffuse;
    return (lighting > 1.0f) ? 1.0f : lighting;
}

void PS_WVP(vertex& v) {
    v.pos = matrixMultiplicationVec(SV_WorldMatrix, v.pos);
    v.pos = matrixMultiplicationVec(SV_ViewMatrix, v.pos);
    v.pos = matrixMultiplicationVec(SV_ProjectionMatrix, v.pos);
    if (v.pos.w != 0) {
        v.pos.x /= v.pos.w;
        v.pos.y /= v.pos.w;
        v.pos.z /= v.pos.w;
    }
}

void PS_White(Pixel& pixel) {
    pixel.color = 0xFFFFFFFF;
}

void PS_Green(Pixel& pixel) {
    pixel.color = 0x0000FF00;
}