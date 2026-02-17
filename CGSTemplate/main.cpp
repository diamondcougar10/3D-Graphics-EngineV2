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

// GPU version of drawLine
void GPU_drawLine(vertex start, vertex end, unsigned int color) {
    vertex s0 = transformToScreen(start);
    vertex s1 = transformToScreen(end);
    GPU_AddLine(s0, s1, color);
}

int main() {
    // Set up the timer
    XTime timer(10, 0.75);
    timer.Restart();

    // Initialize transformation matrices
    matrix4x4 cube = MatrixIdentity();
    matrix4x4 grid = MatrixIdentity();

    // Set the camera position along the negative Z-axis
    vec4 camPos = { 0, 0, -2, 1 };

    // Define vertices for the cube with UV coordinates
    vertex topLeftFrontVert({ -0.25, 0.25, -0.25, 1 }, 0xFFFF0000, 0.0f, 0.0f);
    vertex topRightFrontVert({ 0.25, 0.25, -0.25, 1 }, 0xFFFF0000, 1.0f, 0.0f);
    vertex topLeftBackVert({ -0.25, -0.25, -0.25, 1 }, 0xFFFF0000, 0.0f, 1.0f);
    vertex topRightBackVert({ 0.25, -0.25, -0.25, 1 }, 0xFFFF0000, 1.0f, 1.0f);
    vertex botLeftFrontVert({ -0.25, 0.25, 0.25, 1 }, 0xFFFF0000, 0.0f, 0.0f);
    vertex botRightFrontVert({ 0.25, 0.25, 0.25, 1 }, 0xFFFF0000, 1.0f, 0.0f);
    vertex botLeftBackVert({ -0.25, -0.25, 0.25, 1 }, 0xFFFF0000, 0.0f, 1.0f);
    vertex botRightBackVert({ 0.25, -0.25, 0.25, 1 }, 0xFFFF0000, 1.0f, 1.0f);

    // Set up the view and projection matrices
    matrix4x4 viewTrans = matrixTranslation(camPos);
    matrix4x4 rotY = matrixRotationX(-18);
    matrix4x4 camMatrix = matrixMultiplicationMatrix(viewTrans, rotY);
    camMatrix = matrix4Inverse(camMatrix);
    SV_ViewMatrix = camMatrix;
    SV_ProjectionMatrix = projectionMatrixMath(90.0f, (float)RASTER_HEIGHT / RASTER_WIDTH, 10, 0.1f);

    // Initialize the raster surface
    RS_Initialize("Ryan Curphey - GPU Compute", RASTER_WIDTH, RASTER_HEIGHT);
    
    // Try to initialize GPU compute
    if (useGPU) {
        if (!GPU_Init()) {
            std::cout << "GPU init failed, falling back to CPU\n";
            useGPU = false;
        } else {
            std::cout << "Using GPU compute shader for rasterization!\n";
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
        timeElapsed += timer.Delta();
        if (timeElapsed > 0.016f) {
            timeElapsed = 0;
            cube = matrixRotationY(cube, 1.5f);  // 1.5 degrees per frame (~60fps = 90 deg/sec)
        }

        if (useGPU) {
            // ===== GPU RENDERING PATH =====
            GPU_BeginFrame();
            
            // Draw grid lines
            SV_WorldMatrix = grid;
            for (int i = 0; i <= 20; i++) {
                float z = -1.0f + 0.1f * i;
                unsigned int lineColor = (i % 2 == 0) ? gridColor : gridColorDim;
                vertex lineStart({ -1.0f, 0.0f, z, 1.0f }, lineColor);
                vertex lineEnd({ 1.0f, 0.0f, z, 1.0f }, lineColor);
                GPU_drawLine(lineStart, lineEnd, lineColor);
            }
            for (int i = 0; i <= 20; i++) {
                float x = -1.0f + 0.1f * i;
                unsigned int lineColor = (i % 2 == 0) ? gridColor : gridColorDim;
                vertex lineStart({ x, 0.0f, -1.0f, 1.0f }, lineColor);
                vertex lineEnd({ x, 0.0f, 1.0f, 1.0f }, lineColor);
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
