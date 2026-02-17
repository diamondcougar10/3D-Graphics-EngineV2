#include <iostream>
#include "RasterHelper.h"
#include "RasterSurface.h"
#include "XTime.h"
#include "celestial.h"
#include "GLCompute.h"
#include <cmath>

// GPU rendering mode
bool useGPU = true;

// Apply lighting to a color
unsigned int applyLightingToColor(unsigned int baseColor, float lightIntensity) {
    float ambient = 0.15f;
    float diffuse = lightIntensity * 0.85f;
    float total = ambient + diffuse;
    if (total > 1.0f) total = 1.0f;
    
    unsigned int a = (baseColor >> 24) & 0xFF;
    unsigned int r = (baseColor >> 16) & 0xFF;
    unsigned int g = (baseColor >> 8) & 0xFF;
    unsigned int b = baseColor & 0xFF;
    
    r = (unsigned int)(r * total);
    g = (unsigned int)(g * total);
    b = (unsigned int)(b * total);
    
    return (a << 24) | (r << 16) | (g << 8) | b;
}

// Transform vertex to screen space for GPU
vertex transformToScreen(vertex v) {
    // Apply world-view-projection
    vertex transformed = matrixMultiplicationVert(SV_WorldMatrix, v);
    transformed = matrixMultiplicationVert(SV_ViewMatrix, transformed);
    transformed = matrixMultiplicationVert(SV_ProjectionMatrix, transformed);
    
    // Perspective divide
    if (transformed.pos.w != 0) {
        transformed.pos.x /= transformed.pos.w;
        transformed.pos.y /= transformed.pos.w;
        transformed.pos.z /= transformed.pos.w;
    }
    
    // NDC to screen
    transformed.pos.x = (transformed.pos.x + 1.0f) * 0.5f * RASTER_WIDTH;
    transformed.pos.y = (1.0f - transformed.pos.y) * 0.5f * RASTER_HEIGHT;
    transformed.color = v.color;
    
    return transformed;
}

// GPU version of DrawTriangle with lighting
void GPU_DrawTriangle(vertex v0, vertex v1, vertex v2, unsigned int baseColor) {
    // Transform to world space first for normal calculation
    vertex w0 = matrixMultiplicationVert(SV_WorldMatrix, v0);
    vertex w1 = matrixMultiplicationVert(SV_WorldMatrix, v1);
    vertex w2 = matrixMultiplicationVert(SV_WorldMatrix, v2);
    
    // Calculate face normal using existing function from Shaders.h
    vec3 faceNormal = calculateFaceNormal(w0.pos, w1.pos, w2.pos);
    
    // Calculate diffuse lighting (N dot L) using existing globals
    vec3 lightDir = vec3Normalize(SV_LightDirection);
    float NdotL = vec3Dot(faceNormal, lightDir);
    if (NdotL < 0.0f) NdotL = -NdotL;  // Two-sided lighting
    
    // Apply lighting to color
    unsigned int litColor = applyLightingToColor(baseColor, NdotL);
    
    // Transform to screen space
    vertex s0 = transformToScreen(v0);
    vertex s1 = transformToScreen(v1);
    vertex s2 = transformToScreen(v2);
    s0.color = s1.color = s2.color = litColor;
    
    GPU_AddTriangle(s0, s1, s2);
}

// Near plane clipping constant (must match projection)
static constexpr float kNear = 0.1f;

// Clip a line segment against the near plane in view space
// Returns true if any part of the line is visible
static bool ClipLineToNear(vec4& a, vec4& b, float nearZ) {
    bool aIn = a.z >= nearZ;
    bool bIn = b.z >= nearZ;

    if (!aIn && !bIn) return false; // fully behind camera

    if (aIn && bIn) return true;    // fully in front

    // One in, one out: find intersection with z = nearZ
    float t = (nearZ - a.z) / (b.z - a.z);
    vec4 hit;
    hit.x = a.x + (b.x - a.x) * t;
    hit.y = a.y + (b.y - a.y) * t;
    hit.z = nearZ;
    hit.w = 1.0f;

    if (!aIn) a = hit;
    else      b = hit;

    return true;
}

// Project a view-space position to screen space
static vertex ProjectViewToScreen(const vec4& viewPos, unsigned int color) {
    vertex v;
    v.pos = viewPos;
    v.color = color;

    // Apply projection
    vertex clipV = matrixMultiplicationVert(SV_ProjectionMatrix, v);

    // Perspective divide (w should be > 0 after clipping)
    float w = clipV.pos.w;
    if (w <= 0.00001f) w = 0.00001f;

    clipV.pos.x /= w;
    clipV.pos.y /= w;
    clipV.pos.z /= w;

    // NDC to screen
    clipV.pos.x = (clipV.pos.x + 1.0f) * 0.5f * RASTER_WIDTH;
    clipV.pos.y = (1.0f - clipV.pos.y) * 0.5f * RASTER_HEIGHT;
    clipV.color = color;

    return clipV;
}

// GPU version of drawLine with near-plane clipping
void GPU_drawLine(vertex start, vertex end, unsigned int color) {
    // Transform to view space first (world -> view)
    vec4 a = matrixMultiplicationVec(SV_ViewMatrix,
             matrixMultiplicationVec(SV_WorldMatrix, start.pos));
    vec4 b = matrixMultiplicationVec(SV_ViewMatrix,
             matrixMultiplicationVec(SV_WorldMatrix, end.pos));

    // Clip against near plane in view space
    if (!ClipLineToNear(a, b, kNear))
        return; // Entire line behind camera

    // Project clipped endpoints to screen space
    vertex s0 = ProjectViewToScreen(a, color);
    vertex s1 = ProjectViewToScreen(b, color);

    GPU_AddLine(s0, s1, color);
}

int main() {
    // Set up the timer
    XTime timer(10, 0.75);
    timer.Restart();

    // Initialize transformation matrices
    matrix4x4 cube = MatrixIdentity();
    matrix4x4 grid = MatrixIdentity();

    // ===== CAMERA STATE =====
    float camX = 0.0f, camY = 0.5f, camZ = -2.0f;  // Camera position
    float camYaw = 0.0f;     // Rotation around Y axis (left/right)
    float camPitch = -15.0f; // Rotation around X axis (up/down)
    float moveSpeed = 2.0f;  // Units per second
    float turnSpeed = 60.0f; // Degrees per second

    // Define vertices for the cube with UV coordinates
    vertex topLeftFrontVert({ -0.25, 0.25, -0.25, 1 }, 0xFFFF0000, 0.0f, 0.0f);
    vertex topRightFrontVert({ 0.25, 0.25, -0.25, 1 }, 0xFFFF0000, 1.0f, 0.0f);
    vertex topLeftBackVert({ -0.25, -0.25, -0.25, 1 }, 0xFFFF0000, 0.0f, 1.0f);
    vertex topRightBackVert({ 0.25, -0.25, -0.25, 1 }, 0xFFFF0000, 1.0f, 1.0f);
    vertex botLeftFrontVert({ -0.25, 0.25, 0.25, 1 }, 0xFFFF0000, 0.0f, 0.0f);
    vertex botRightFrontVert({ 0.25, 0.25, 0.25, 1 }, 0xFFFF0000, 1.0f, 0.0f);
    vertex botLeftBackVert({ -0.25, -0.25, 0.25, 1 }, 0xFFFF0000, 0.0f, 1.0f);
    vertex botRightBackVert({ 0.25, -0.25, 0.25, 1 }, 0xFFFF0000, 1.0f, 1.0f);

    // Set up projection matrix (only needs to be set once)
    SV_ProjectionMatrix = projectionMatrixMath(90.0f, (float)RASTER_HEIGHT / RASTER_WIDTH, 100.0f, 0.1f);

    // Initialize the raster surface
    RS_Initialize("3D Engine - WASD to move, Arrows to look", RASTER_WIDTH, RASTER_HEIGHT);
    
    // Try to initialize GPU compute
    if (useGPU) {
        if (!GPU_Init()) {
            std::cout << "GPU init failed, falling back to CPU\n";
            useGPU = false;
        } else {
            std::cout << "Using GPU compute shader for rasterization!\n";
            std::cout << "Controls: WASD = Move, Arrow Keys = Look, Q/E = Up/Down\n";
        }
    }

    float timeElapsed = 0;
    cube.wy += 0.25;
    
    // Grid colors
    unsigned int gridColor = 0xFF00FFFF; // Cyan
    unsigned int gridColorDim = 0xFF00AAAA; // Dimmer cyan for alternating

    do {
        // Update the cube rotation based on the timer
        timer.Signal();
        float deltaTime = (float)timer.Delta();
        timeElapsed += deltaTime;
        if (timeElapsed > 0.016f) {
            timeElapsed = 0;
            cube = matrixRotationY(cube, 1.5f);  // 1.5 degrees per frame (~60fps = 90 deg/sec)
        }
        
        // ===== CAMERA INPUT =====
        // Look controls (arrow keys)
        if (GetAsyncKeyState(VK_LEFT) & 0x8000)  camYaw -= turnSpeed * deltaTime;
        if (GetAsyncKeyState(VK_RIGHT) & 0x8000) camYaw += turnSpeed * deltaTime;
        if (GetAsyncKeyState(VK_UP) & 0x8000)    camPitch -= turnSpeed * deltaTime;
        if (GetAsyncKeyState(VK_DOWN) & 0x8000)  camPitch += turnSpeed * deltaTime;
        
        // Clamp pitch to prevent flipping
        if (camPitch > 89.0f) camPitch = 89.0f;
        if (camPitch < -89.0f) camPitch = -89.0f;
        
        // Calculate forward and right vectors based on yaw
        float yawRad = camYaw * 3.14159f / 180.0f;
        float forwardX = sinf(yawRad);
        float forwardZ = cosf(yawRad);
        float rightX = cosf(yawRad);
        float rightZ = -sinf(yawRad);
        
        // Movement controls (WASD)
        if (GetAsyncKeyState('W') & 0x8000) {
            camX += forwardX * moveSpeed * deltaTime;
            camZ += forwardZ * moveSpeed * deltaTime;
        }
        if (GetAsyncKeyState('S') & 0x8000) {
            camX -= forwardX * moveSpeed * deltaTime;
            camZ -= forwardZ * moveSpeed * deltaTime;
        }
        if (GetAsyncKeyState('A') & 0x8000) {
            camX -= rightX * moveSpeed * deltaTime;
            camZ -= rightZ * moveSpeed * deltaTime;
        }
        if (GetAsyncKeyState('D') & 0x8000) {
            camX += rightX * moveSpeed * deltaTime;
            camZ += rightZ * moveSpeed * deltaTime;
        }
        // Vertical movement (Q/E)
        if (GetAsyncKeyState('Q') & 0x8000) camY -= moveSpeed * deltaTime;
        if (GetAsyncKeyState('E') & 0x8000) camY += moveSpeed * deltaTime;
        
        // ===== UPDATE VIEW MATRIX =====
        vec4 camPos = { camX, camY, camZ, 1.0f };
        matrix4x4 camTranslation = matrixTranslation(camPos);
        matrix4x4 camRotY = matrixRotationY(MatrixIdentity(), camYaw);
        matrix4x4 camRotX = matrixRotationX(camPitch);
        matrix4x4 camMatrix = matrixMultiplicationMatrix(camTranslation, camRotY);
        camMatrix = matrixMultiplicationMatrix(camMatrix, camRotX);
        SV_ViewMatrix = matrix4Inverse(camMatrix);

        if (useGPU) {
            // ===== GPU RENDERING PATH =====
            GPU_BeginFrame();
            
            // Draw grid lines (large grid for exploration)
            SV_WorldMatrix = grid;
            float gridExtent = 10.0f;  // Grid from -10 to +10
            int gridLines = 40;        // 40 lines each direction
            float gridSpacing = (gridExtent * 2.0f) / gridLines;
            
            for (int i = 0; i <= gridLines; i++) {
                float z = -gridExtent + gridSpacing * i;
                unsigned int lineColor = (i % 2 == 0) ? gridColor : gridColorDim;
                vertex lineStart({ -gridExtent, 0.0f, z, 1.0f }, lineColor);
                vertex lineEnd({ gridExtent, 0.0f, z, 1.0f }, lineColor);
                GPU_drawLine(lineStart, lineEnd, lineColor);
            }
            for (int i = 0; i <= gridLines; i++) {
                float x = -gridExtent + gridSpacing * i;
                unsigned int lineColor = (i % 2 == 0) ? gridColor : gridColorDim;
                vertex lineStart({ x, 0.0f, -gridExtent, 1.0f }, lineColor);
                vertex lineEnd({ x, 0.0f, gridExtent, 1.0f }, lineColor);
                GPU_drawLine(lineStart, lineEnd, lineColor);
            }
            
            // Draw cube triangles
            SV_WorldMatrix = cube;
            unsigned int cubeColor = 0xFF4444FF; // Blue cube
            GPU_DrawTriangle(topLeftFrontVert, topRightFrontVert, topRightBackVert, cubeColor);
            GPU_DrawTriangle(topLeftFrontVert, topRightBackVert, topLeftBackVert, cubeColor);
            GPU_DrawTriangle(botLeftFrontVert, botRightFrontVert, botRightBackVert, cubeColor);
            GPU_DrawTriangle(botLeftFrontVert, botRightBackVert, botLeftBackVert, cubeColor);
            GPU_DrawTriangle(topLeftFrontVert, botLeftFrontVert, botRightFrontVert, cubeColor);
            GPU_DrawTriangle(topLeftFrontVert, botRightFrontVert, topRightFrontVert, cubeColor);
            GPU_DrawTriangle(topLeftBackVert, botLeftBackVert, botRightBackVert, cubeColor);
            GPU_DrawTriangle(topLeftBackVert, botRightBackVert, topRightBackVert, cubeColor);
            GPU_DrawTriangle(topLeftFrontVert, botLeftFrontVert, botLeftBackVert, cubeColor);
            GPU_DrawTriangle(topLeftFrontVert, botLeftBackVert, topLeftBackVert, cubeColor);
            GPU_DrawTriangle(topRightFrontVert, botRightFrontVert, botRightBackVert, cubeColor);
            GPU_DrawTriangle(topRightFrontVert, botRightBackVert, topRightBackVert, cubeColor);
            
            // Dispatch GPU compute and get pixels
            GPU_Dispatch(SCREEN_ARRAY);
            
        } else {
            // ===== CPU RENDERING PATH (fallback) =====
            clearColorBuffer(0xFF000000);
            PixelShader = nullptr;
            VertexShader = PS_WVP;
            
            // Draw grid
            SV_WorldMatrix = grid;
            for (int i = 0; i <= 20; i++) {
                float z = -1.0f + 0.1f * i;
                unsigned int lineColor = (i % 2 == 0) ? gridColor : gridColorDim;
                vertex lineStart({ -1.0f, 0.0f, z, 1.0f }, lineColor);
                vertex lineEnd({ 1.0f, 0.0f, z, 1.0f }, lineColor);
                drawLine(lineStart, lineEnd, lineColor);
            }
            for (int i = 0; i <= 20; i++) {
                float x = -1.0f + 0.1f * i;
                unsigned int lineColor = (i % 2 == 0) ? gridColor : gridColorDim;
                vertex lineStart({ x, 0.0f, -1.0f, 1.0f }, lineColor);
                vertex lineEnd({ x, 0.0f, 1.0f, 1.0f }, lineColor);
                drawLine(lineStart, lineEnd, lineColor);
            }
            
            // Draw cube
            SV_WorldMatrix = cube;
            DrawTriangle(topLeftFrontVert, topRightFrontVert, topRightBackVert, celestial_pixels, celestial_width, celestial_height);
            DrawTriangle(topLeftFrontVert, topRightBackVert, topLeftBackVert, celestial_pixels, celestial_width, celestial_height);
            DrawTriangle(botLeftFrontVert, botRightFrontVert, botRightBackVert, celestial_pixels, celestial_width, celestial_height);
            DrawTriangle(botLeftFrontVert, botRightBackVert, botLeftBackVert, celestial_pixels, celestial_width, celestial_height);
            DrawTriangle(topLeftFrontVert, botLeftFrontVert, botRightFrontVert, celestial_pixels, celestial_width, celestial_height);
            DrawTriangle(topLeftFrontVert, botRightFrontVert, topRightFrontVert, celestial_pixels, celestial_width, celestial_height);
            DrawTriangle(topLeftBackVert, botLeftBackVert, botRightBackVert, celestial_pixels, celestial_width, celestial_height);
            DrawTriangle(topLeftBackVert, botRightBackVert, topRightBackVert, celestial_pixels, celestial_width, celestial_height);
            DrawTriangle(topLeftFrontVert, botLeftFrontVert, botLeftBackVert, celestial_pixels, celestial_width, celestial_height);
            DrawTriangle(topLeftFrontVert, botLeftBackVert, topLeftBackVert, celestial_pixels, celestial_width, celestial_height);
            DrawTriangle(topRightFrontVert, botRightFrontVert, botRightBackVert, celestial_pixels, celestial_width, celestial_height);
            DrawTriangle(topRightFrontVert, botRightBackVert, topRightBackVert, celestial_pixels, celestial_width, celestial_height);
        }

    } while (RS_Update(SCREEN_ARRAY, NUM_PIXELS));

    if (useGPU) GPU_Shutdown();
    RS_Shutdown();
    return 0;
}
