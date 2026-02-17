#include <iostream>
#include "RasterHelper.h"
#include "RasterSurface.h"
#include "XTime.h"
#include "celestial.h"

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
    RS_Initialize("Ryan Curphey", RASTER_WIDTH, RASTER_HEIGHT);

    float timeElapsed = 0;
    cube.wy += 0.25;

    do {
        // Clear the color buffer to space (stars)
        clearColorBuffer(0xFF000000);

        // Set the PixelShader for cyan holographic grid
        PixelShader = nullptr; // Use the vertex color directly

        // Draw the grid lines with cyan sci-fi color
        SV_WorldMatrix = grid;
        VertexShader = PS_WVP;

        unsigned int gridColor = 0xFF00FFFF; // Cyan
        unsigned int gridColorDim = 0xFF00AAAA; // Dimmer cyan for alternating
        
        // Draw horizontal lines (along X axis)
        for (int i = 0; i <= 20; i++) {
            float z = -1.0f + 0.1f * i;
            unsigned int lineColor = (i % 2 == 0) ? gridColor : gridColorDim;
            vertex lineStart({ -1.0f, 0.0f, z, 1.0f }, lineColor);
            vertex lineEnd({ 1.0f, 0.0f, z, 1.0f }, lineColor);
            drawLine(lineStart, lineEnd, lineColor);
        }
        
        // Draw vertical lines (along Z axis)
        for (int i = 0; i <= 20; i++) {
            float x = -1.0f + 0.1f * i;
            unsigned int lineColor = (i % 2 == 0) ? gridColor : gridColorDim;
            vertex lineStart({ x, 0.0f, -1.0f, 1.0f }, lineColor);
            vertex lineEnd({ x, 0.0f, 1.0f, 1.0f }, lineColor);
            drawLine(lineStart, lineEnd, lineColor);
        }

        // Set the world matrix for the cube
        SV_WorldMatrix = cube;

        // Update the cube rotation based on the timer
        timer.Signal();
        timeElapsed += timer.Delta();
        if (timeElapsed > 0.016f) {
            timeElapsed = 0;
            cube = matrixRotationY(cube, 1.5f);  // 1.5 degrees per frame (~60fps = 90 deg/sec)
        }

        // Draw the cube triangles with texture
        DrawTriangle(topLeftFrontVert, topRightFrontVert, topRightBackVert, celestial_pixels, celestial_width, celestial_height); // Front
        DrawTriangle(topLeftFrontVert, topRightBackVert, topLeftBackVert, celestial_pixels, celestial_width, celestial_height); // Front
        DrawTriangle(botLeftFrontVert, botRightFrontVert, botRightBackVert, celestial_pixels, celestial_width, celestial_height); // Back
        DrawTriangle(botLeftFrontVert, botRightBackVert, botLeftBackVert, celestial_pixels, celestial_width, celestial_height); // Back
        DrawTriangle(topLeftFrontVert, botLeftFrontVert, botRightFrontVert, celestial_pixels, celestial_width, celestial_height); // Top
        DrawTriangle(topLeftFrontVert, botRightFrontVert, topRightFrontVert, celestial_pixels, celestial_width, celestial_height); // Top
        DrawTriangle(topLeftBackVert, botLeftBackVert, botRightBackVert, celestial_pixels, celestial_width, celestial_height); // Bottom
        DrawTriangle(topLeftBackVert, botRightBackVert, topRightBackVert, celestial_pixels, celestial_width, celestial_height); // Bottom
        DrawTriangle(topLeftFrontVert, botLeftFrontVert, botLeftBackVert, celestial_pixels, celestial_width, celestial_height); // Left
        DrawTriangle(topLeftFrontVert, botLeftBackVert, topLeftBackVert, celestial_pixels, celestial_width, celestial_height); // Left
        DrawTriangle(topRightFrontVert, botRightFrontVert, botRightBackVert, celestial_pixels, celestial_width, celestial_height); // Right
        DrawTriangle(topRightFrontVert, botRightBackVert, topRightBackVert, celestial_pixels, celestial_width, celestial_height); // Right

    } while (RS_Update(SCREEN_ARRAY, NUM_PIXELS));

    RS_Shutdown();
    return 0;
}
