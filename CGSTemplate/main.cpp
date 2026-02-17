#include <iostream>
#include "RasterHelper.h"
#include "RasterSurface.h"
#include "XTime.h"
#include "celestial.h"
#include "GLCompute.h"
#include "MaterialMesh.h"
#include "Skybox.h"
#include "Texture.h"
#include "Model.h"
#include <cmath>
#include <commdlg.h>  // For file open dialog

// ImGui includes
#include "imgui/imgui.h"
#include "imgui/imgui_impl_sw.h"

// Loaded models container
std::vector<std::unique_ptr<game::Model>> g_LoadedModels;

// Helper function to open file dialog for model loading
std::string OpenModelFileDialog(HWND hwnd) {
    char filename[MAX_PATH] = "";
    
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = "3D Models\0*.obj;*.fbx;*.gltf;*.glb;*.dae;*.3ds;*.blend;*.stl;*.ply\0"
                      "OBJ Files (*.obj)\0*.obj\0"
                      "FBX Files (*.fbx)\0*.fbx\0"
                      "GLTF Files (*.gltf;*.glb)\0*.gltf;*.glb\0"
                      "All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Load 3D Model";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    
    if (GetOpenFileNameA(&ofn)) {
        return std::string(filename);
    }
    return "";
}

// GPU rendering mode
bool useGPU = true;  // Enable GPU rendering with texture support

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
    
    // Preserve original vertex attributes
    transformed.color = v.color;
    transformed.u = v.u;
    transformed.v = v.v;
    
    return transformed;
}

// Forward declarations for clipping functions (defined below)
static vertex LerpVertex(const vertex& a, const vertex& b, float t);
static int ClipTriangleToNear(const vertex& v0, const vertex& v1, const vertex& v2, 
                               float nearZ, vertex outVerts[6]);
static vertex TransformToView(const vertex& v);
static vertex ProjectToScreen(const vertex& v);
static constexpr float kNear = 0.1f;

// GPU version of DrawTriangle with lighting and near-plane clipping
void GPU_DrawTriangle(vertex v0, vertex v1, vertex v2, unsigned int baseColor) {
    // Ensure GPU doesn't use texture for solid-color triangles
    g_GLCompute.setUseTexture(false);
    
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
    
    // Transform to view space for clipping
    vertex view0 = TransformToView(v0);
    vertex view1 = TransformToView(v1);
    vertex view2 = TransformToView(v2);
    view0.color = view1.color = view2.color = litColor;
    
    // Clip triangle against near plane
    vertex clippedVerts[6];
    int numTris = ClipTriangleToNear(view0, view1, view2, kNear, clippedVerts);
    
    // Project and submit each clipped triangle
    for (int i = 0; i < numTris; i++) {
        vertex s0 = ProjectToScreen(clippedVerts[i * 3 + 0]);
        vertex s1 = ProjectToScreen(clippedVerts[i * 3 + 1]);
        vertex s2 = ProjectToScreen(clippedVerts[i * 3 + 2]);
        GPU_AddTriangle(s0, s1, s2);
    }
}

// GPU version for textured triangles with near-plane clipping
void GPU_DrawTexturedTriangle(vertex v0, vertex v1, vertex v2) {
    // Enable texture sampling for this triangle
    g_GLCompute.setUseTexture(true);
    
    // Transform to world space first for normal calculation
    vertex w0 = matrixMultiplicationVert(SV_WorldMatrix, v0);
    vertex w1 = matrixMultiplicationVert(SV_WorldMatrix, v1);
    vertex w2 = matrixMultiplicationVert(SV_WorldMatrix, v2);
    
    // Calculate face normal
    vec3 faceNormal = calculateFaceNormal(w0.pos, w1.pos, w2.pos);
    
    // Calculate diffuse lighting
    vec3 lightDir = vec3Normalize(SV_LightDirection);
    float NdotL = vec3Dot(faceNormal, lightDir);
    if (NdotL < 0.0f) NdotL = -NdotL;
    
    // Calculate lighting factor
    float ambient = 0.15f;
    float diffuse = NdotL * 0.85f;
    float total = ambient + diffuse;
    if (total > 1.0f) total = 1.0f;
    
    // Store lighting as grayscale in color
    unsigned int lightVal = (unsigned int)(total * 255.0f);
    unsigned int lightColor = 0xFF000000 | (lightVal << 16) | (lightVal << 8) | lightVal;
    
    // Transform to view space for clipping (preserve UVs)
    vertex view0 = TransformToView(v0);
    vertex view1 = TransformToView(v1);
    vertex view2 = TransformToView(v2);
    view0.color = view1.color = view2.color = lightColor;
    view0.u = v0.u; view0.v = v0.v;
    view1.u = v1.u; view1.v = v1.v;
    view2.u = v2.u; view2.v = v2.v;
    
    // Clip triangle against near plane
    vertex clippedVerts[6];
    int numTris = ClipTriangleToNear(view0, view1, view2, kNear, clippedVerts);
    
    // Project and submit each clipped triangle
    for (int i = 0; i < numTris; i++) {
        vertex s0 = ProjectToScreen(clippedVerts[i * 3 + 0]);
        vertex s1 = ProjectToScreen(clippedVerts[i * 3 + 1]);
        vertex s2 = ProjectToScreen(clippedVerts[i * 3 + 2]);
        GPU_AddTexturedTriangle(s0, s1, s2);
    }
}

// Interpolate a vertex between two vertices at parameter t
static vertex LerpVertex(const vertex& a, const vertex& b, float t) {
    vertex result;
    result.pos.x = a.pos.x + (b.pos.x - a.pos.x) * t;
    result.pos.y = a.pos.y + (b.pos.y - a.pos.y) * t;
    result.pos.z = a.pos.z + (b.pos.z - a.pos.z) * t;
    result.pos.w = a.pos.w + (b.pos.w - a.pos.w) * t;
    result.u = a.u + (b.u - a.u) * t;
    result.v = a.v + (b.v - a.v) * t;
    result.color = a.color; // Use color from first vertex
    return result;
}

// Clip triangle against near plane in view space
// Returns number of output triangles (0, 1, or 2)
// Outputs up to 2 triangles in outVerts (6 vertices max)
static int ClipTriangleToNear(const vertex& v0, const vertex& v1, const vertex& v2, 
                               float nearZ, vertex outVerts[6]) {
    // Check which vertices are in front of near plane
    bool in0 = v0.pos.z >= nearZ;
    bool in1 = v1.pos.z >= nearZ;
    bool in2 = v2.pos.z >= nearZ;
    int numIn = (in0 ? 1 : 0) + (in1 ? 1 : 0) + (in2 ? 1 : 0);
    
    if (numIn == 0) {
        // All behind - cull entire triangle
        return 0;
    }
    
    if (numIn == 3) {
        // All in front - keep triangle as is
        outVerts[0] = v0;
        outVerts[1] = v1;
        outVerts[2] = v2;
        return 1;
    }
    
    if (numIn == 1) {
        // One vertex in front - clip to smaller triangle
        const vertex* vIn;
        const vertex* vOut1;
        const vertex* vOut2;
        
        if (in0) { vIn = &v0; vOut1 = &v1; vOut2 = &v2; }
        else if (in1) { vIn = &v1; vOut1 = &v2; vOut2 = &v0; }
        else { vIn = &v2; vOut1 = &v0; vOut2 = &v1; }
        
        // Find intersection points
        float t1 = (nearZ - vIn->pos.z) / (vOut1->pos.z - vIn->pos.z);
        float t2 = (nearZ - vIn->pos.z) / (vOut2->pos.z - vIn->pos.z);
        
        vertex clip1 = LerpVertex(*vIn, *vOut1, t1);
        vertex clip2 = LerpVertex(*vIn, *vOut2, t2);
        
        outVerts[0] = *vIn;
        outVerts[1] = clip1;
        outVerts[2] = clip2;
        return 1;
    }
    
    // numIn == 2: Two vertices in front - clip to quad (2 triangles)
    const vertex* vOut;
    const vertex* vIn1;
    const vertex* vIn2;
    
    if (!in0) { vOut = &v0; vIn1 = &v1; vIn2 = &v2; }
    else if (!in1) { vOut = &v1; vIn1 = &v2; vIn2 = &v0; }
    else { vOut = &v2; vIn1 = &v0; vIn2 = &v1; }
    
    // Find intersection points
    float t1 = (nearZ - vIn1->pos.z) / (vOut->pos.z - vIn1->pos.z);
    float t2 = (nearZ - vIn2->pos.z) / (vOut->pos.z - vIn2->pos.z);
    
    vertex clip1 = LerpVertex(*vIn1, *vOut, t1);
    vertex clip2 = LerpVertex(*vIn2, *vOut, t2);
    
    // First triangle: vIn1, clip1, vIn2
    outVerts[0] = *vIn1;
    outVerts[1] = clip1;
    outVerts[2] = *vIn2;
    
    // Second triangle: vIn2, clip1, clip2
    outVerts[3] = *vIn2;
    outVerts[4] = clip1;
    outVerts[5] = clip2;
    
    return 2;
}

// Transform vertex to view space (world -> view)
static vertex TransformToView(const vertex& v) {
    vertex temp = v;  // Make non-const copy
    vertex result = matrixMultiplicationVert(SV_WorldMatrix, temp);
    result = matrixMultiplicationVert(SV_ViewMatrix, result);
    result.u = v.u;
    result.v = v.v;
    result.color = v.color;
    return result;
}

// Project view-space vertex to screen space
static vertex ProjectToScreen(const vertex& v) {
    vertex temp = v;  // Make non-const copy
    vertex clipV = matrixMultiplicationVert(SV_ProjectionMatrix, temp);
    
    // Perspective divide
    float w = clipV.pos.w;
    if (w <= 0.00001f) w = 0.00001f;
    
    clipV.pos.x /= w;
    clipV.pos.y /= w;
    clipV.pos.z /= w;
    
    // NDC to screen
    clipV.pos.x = (clipV.pos.x + 1.0f) * 0.5f * RASTER_WIDTH;
    clipV.pos.y = (1.0f - clipV.pos.y) * 0.5f * RASTER_HEIGHT;
    
    clipV.u = v.u;
    clipV.v = v.v;
    clipV.color = v.color;
    
    return clipV;
}

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
    // Initialize screen buffers to desktop resolution
    InitScreenBuffers();
    
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
    float lookSensitivity = 0.2f;  // Degrees per pixel of mouse movement
    float panSensitivity = 0.005f; // Units per pixel of mouse movement
    float scrollSpeed = 0.5f;      // Units per scroll notch
    
    // ===== MOUSE STATE =====
    POINT lastMousePos = { 0, 0 };
    GetCursorPos(&lastMousePos);
    bool firstFrame = true;

    // Cube half-size
    float hs = 0.25f;
    
    // ===== CUBE FACE VERTICES WITH PROPER UVs =====
    // Each face needs its own set of vertices with correct UV mapping
    // UV coordinates: (0,0)=top-left, (1,0)=top-right, (0,1)=bottom-left, (1,1)=bottom-right
    
    // Front face (Z = -hs) - facing camera initially
    vertex frontTL({ -hs,  hs, -hs, 1 }, 0xFFFFFFFF, 0.0f, 0.0f);
    vertex frontTR({  hs,  hs, -hs, 1 }, 0xFFFFFFFF, 1.0f, 0.0f);
    vertex frontBL({ -hs, -hs, -hs, 1 }, 0xFFFFFFFF, 0.0f, 1.0f);
    vertex frontBR({  hs, -hs, -hs, 1 }, 0xFFFFFFFF, 1.0f, 1.0f);
    
    // Back face (Z = +hs)
    vertex backTL({  hs,  hs,  hs, 1 }, 0xFFFFFFFF, 0.0f, 0.0f);
    vertex backTR({ -hs,  hs,  hs, 1 }, 0xFFFFFFFF, 1.0f, 0.0f);
    vertex backBL({  hs, -hs,  hs, 1 }, 0xFFFFFFFF, 0.0f, 1.0f);
    vertex backBR({ -hs, -hs,  hs, 1 }, 0xFFFFFFFF, 1.0f, 1.0f);
    
    // Top face (Y = +hs)
    vertex topTL({ -hs,  hs,  hs, 1 }, 0xFFFFFFFF, 0.0f, 0.0f);
    vertex topTR({  hs,  hs,  hs, 1 }, 0xFFFFFFFF, 1.0f, 0.0f);
    vertex topBL({ -hs,  hs, -hs, 1 }, 0xFFFFFFFF, 0.0f, 1.0f);
    vertex topBR({  hs,  hs, -hs, 1 }, 0xFFFFFFFF, 1.0f, 1.0f);
    
    // Bottom face (Y = -hs)
    vertex botTL({ -hs, -hs, -hs, 1 }, 0xFFFFFFFF, 0.0f, 0.0f);
    vertex botTR({  hs, -hs, -hs, 1 }, 0xFFFFFFFF, 1.0f, 0.0f);
    vertex botBL({ -hs, -hs,  hs, 1 }, 0xFFFFFFFF, 0.0f, 1.0f);
    vertex botBR({  hs, -hs,  hs, 1 }, 0xFFFFFFFF, 1.0f, 1.0f);
    
    // Left face (X = -hs)
    vertex leftTL({ -hs,  hs,  hs, 1 }, 0xFFFFFFFF, 0.0f, 0.0f);
    vertex leftTR({ -hs,  hs, -hs, 1 }, 0xFFFFFFFF, 1.0f, 0.0f);
    vertex leftBL({ -hs, -hs,  hs, 1 }, 0xFFFFFFFF, 0.0f, 1.0f);
    vertex leftBR({ -hs, -hs, -hs, 1 }, 0xFFFFFFFF, 1.0f, 1.0f);
    
    // Right face (X = +hs)
    vertex rightTL({  hs,  hs, -hs, 1 }, 0xFFFFFFFF, 0.0f, 0.0f);
    vertex rightTR({  hs,  hs,  hs, 1 }, 0xFFFFFFFF, 1.0f, 0.0f);
    vertex rightBL({  hs, -hs, -hs, 1 }, 0xFFFFFFFF, 0.0f, 1.0f);
    vertex rightBR({  hs, -hs,  hs, 1 }, 0xFFFFFFFF, 1.0f, 1.0f);

    // Set up projection matrix (only needs to be set once)
    // Using 60 degree FOV for less distortion, proper aspect ratio
    SV_ProjectionMatrix = projectionMatrixMath(60.0f, (float)RASTER_HEIGHT / RASTER_WIDTH, 100.0f, 0.1f);

    // Initialize the raster surface
    RS_Initialize("3D Engine - RMB=Look, MMB=Pan, Scroll=Zoom, WASD=Move", RASTER_WIDTH, RASTER_HEIGHT);
    
    // Try to initialize GPU compute
    if (useGPU) {
        if (!GPU_Init()) {
            std::cout << "GPU init failed, falling back to CPU\n";
            useGPU = false;
        } else {
            std::cout << "Using GPU compute shader for rasterization!\n";
            std::cout << "Controls: Right-click+drag = Look, Middle-click+drag = Pan\n";
            std::cout << "          Scroll wheel = Zoom, WASD = Fly, Q/E = Up/Down\n";
            
            // Upload texture to GPU
            GPU_UploadTexture(celestial_pixels, celestial_width, celestial_height);
        }
    }
    
    // ===== SET UP RENDER CALLBACKS =====
    game::g_RenderCallbacks.useGPU = useGPU;
    game::g_RenderCallbacks.drawTexturedTriangleGPU = GPU_DrawTexturedTriangle;
    game::g_RenderCallbacks.drawTriangleGPU = GPU_DrawTriangle;
    game::g_RenderCallbacks.drawTriangleCPU = DrawTriangle;
    game::g_RenderCallbacks.uploadTextureGPU = GPU_UploadTexture;
    game::g_RenderCallbacks.texture = celestial_pixels;
    game::g_RenderCallbacks.texWidth = celestial_width;
    game::g_RenderCallbacks.texHeight = celestial_height;
    
    // ===== LOAD TEXTURES =====
    // Make texture static so it persists for the entire program
    static game::Texture woodboxTexture;
    
    // Try various paths to find the texture
    const char* texturePaths[] = {
        "textures/woodbox.jpg",
        "textures\\woodbox.jpg",
        "../textures/woodbox.jpg",
        "..\\textures\\woodbox.jpg",
        "C:\\Users\\curph\\OneDrive\\Documents\\3d cube\\textures\\woodbox.jpg",
        "C:/Users/curph/OneDrive/Documents/3d cube/textures/woodbox.jpg"
    };
    
    for (const char* path : texturePaths) {
        if (woodboxTexture.load(path)) {
            std::cout << "SUCCESS: Loaded texture from: " << path << " (" << woodboxTexture.getWidth() << "x" << woodboxTexture.getHeight() << ")\n";
            break;
        }
    }
    if (!woodboxTexture.isLoaded()) {
        std::cerr << "ERROR: Could not load woodbox texture from any path!\n";
    }
    
    // ===== CREATE SCENE OBJECTS =====
    // Create the main cube using the object system
    auto* mainCube = new game::MaterialMesh(
        game::Mesh::createCubeVertices(),
        game::Mesh::createCubeIndices()
    );
    mainCube->setPosition(0.0f, 0.5f, 0.0f);
    mainCube->setRotationSpeed(45.0f);  // Auto-rotate (degrees per second)
    
    // Apply woodbox texture to main cube
    if (woodboxTexture.isLoaded()) {
        mainCube->setTexture(woodboxTexture.getPixels(), woodboxTexture.getWidth(), woodboxTexture.getHeight());
        mainCube->setUseTexture(true);
    } else {
        mainCube->setColor(0.2f, 0.4f, 0.8f);  // Fallback blue color
        mainCube->setUseTexture(false);
    }
    game::g_ObjectManager.addObject(mainCube);
    
    // Add some additional cubes to demonstrate the object system
    // Like the example: we can spawn multiple objects easily
    srand(42);  // Fixed seed for consistency
    for (int i = 0; i < 10; i++) {
        auto* cube = new game::MaterialMesh(
            game::Mesh::createCubeVertices(),
            game::Mesh::createCubeIndices()
        );
        
        // Random position in a ring around the center
        float angle = (float)i * (3.14159f * 2.0f / 10.0f);
        float radius = 3.0f + (rand() % 100) / 100.0f;
        cube->setPosition(cosf(angle) * radius, 0.3f + (rand() % 50) / 100.0f, sinf(angle) * radius);
        
        // Random scale
        float scale = 0.2f + (rand() % 30) / 100.0f;
        cube->setScale(scale);
        
        // Random rotation speed (30-80 degrees per second)
        cube->setRotationSpeed(30.0f + (rand() % 50));
        
        // Blue color with slight variation
        cube->setColor(0.2f + (rand() % 20) / 100.0f, 
                       0.4f + (rand() % 20) / 100.0f, 
                       0.7f + (rand() % 30) / 100.0f);
        cube->setUseTexture(false);
        
        game::g_ObjectManager.addObject(cube);
    }
    
    std::cout << "Created " << game::g_ObjectManager.getObjectCount() << " objects\n";

    // ===== SKYBOX INITIALIZATION =====
    game::Skybox skybox;
    game::g_Skybox = &skybox;
    
    // Create procedural skybox textures (simple gradient space theme)
    const int SKY_SIZE = 64;
    static unsigned int skyFaces[6][SKY_SIZE * SKY_SIZE];
    
    // Generate each face with different colors for space environment
    auto generateSkyFace = [](unsigned int* face, int size, unsigned int topColor, unsigned int bottomColor) {
        for (int y = 0; y < size; ++y) {
            float t = (float)y / (float)(size - 1);
            unsigned int tr = ((topColor >> 16) & 0xFF);
            unsigned int tg = ((topColor >> 8) & 0xFF);
            unsigned int tb = (topColor & 0xFF);
            unsigned int br = ((bottomColor >> 16) & 0xFF);
            unsigned int bg = ((bottomColor >> 8) & 0xFF);
            unsigned int bb = (bottomColor & 0xFF);
            unsigned int r = (unsigned int)(tr + t * (br - tr));
            unsigned int g = (unsigned int)(tg + t * (bg - tg));
            unsigned int b = (unsigned int)(tb + t * (bb - tb));
            unsigned int color = 0xFF000000 | (r << 16) | (g << 8) | b;
            
            for (int x = 0; x < size; ++x) {
                face[y * size + x] = color;
                
                // Add some stars
                if ((x * 31 + y * 17) % 97 == 0) {
                    int brightness = 150 + ((x * y) % 105);
                    face[y * size + x] = 0xFF000000 | (brightness << 16) | (brightness << 8) | brightness;
                }
            }
        }
    };
    
    // Deep space colors
    unsigned int spaceTop = 0xFF050510;     // Very dark blue
    unsigned int spaceBottom = 0xFF000008;  // Nearly black
    unsigned int horizonGlow = 0xFF101830;  // Subtle purple-blue horizon
    
    // Generate faces (right, left, top, bottom, front, back)
    generateSkyFace(skyFaces[0], SKY_SIZE, spaceTop, horizonGlow);     // Right
    generateSkyFace(skyFaces[1], SKY_SIZE, spaceTop, horizonGlow);     // Left
    generateSkyFace(skyFaces[2], SKY_SIZE, spaceTop, spaceTop);        // Top (all dark)
    generateSkyFace(skyFaces[3], SKY_SIZE, horizonGlow, spaceBottom);  // Bottom
    generateSkyFace(skyFaces[4], SKY_SIZE, spaceTop, horizonGlow);     // Front
    generateSkyFace(skyFaces[5], SKY_SIZE, spaceTop, horizonGlow);     // Back
    
    // Load into skybox
    skybox.setFace(game::CubeFace::Right, skyFaces[0], SKY_SIZE, SKY_SIZE);
    skybox.setFace(game::CubeFace::Left, skyFaces[1], SKY_SIZE, SKY_SIZE);
    skybox.setFace(game::CubeFace::Top, skyFaces[2], SKY_SIZE, SKY_SIZE);
    skybox.setFace(game::CubeFace::Bottom, skyFaces[3], SKY_SIZE, SKY_SIZE);
    skybox.setFace(game::CubeFace::Front, skyFaces[4], SKY_SIZE, SKY_SIZE);
    skybox.setFace(game::CubeFace::Back, skyFaces[5], SKY_SIZE, SKY_SIZE);
    
    std::cout << "Skybox initialized\n";

    // ===== IMGUI INITIALIZATION =====
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.DisplaySize = ImVec2((float)RASTER_WIDTH, (float)RASTER_HEIGHT);
    
    // Setup style
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.Alpha = 0.95f;
    
    // Initialize software renderer
    ImGui_ImplSW_Init();
    ImGui_ImplSW_CreateFontsTexture();
    
    // ===== LIGHTING SETTINGS (adjustable via UI) =====
    float lightDirX = SV_LightDirection.x;
    float lightDirY = SV_LightDirection.y;
    float lightDirZ = SV_LightDirection.z;
    float ambientIntensity = SV_AmbientLight;
    float sunColor[3] = { SV_SunColor.x, SV_SunColor.y, SV_SunColor.z };
    float cubeColorRGB[3] = { 0.27f, 0.27f, 1.0f }; // Cube color (blue)
    bool showSettingsPanel = true;
    bool autoRotateCube = true;

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
        
        // Update all scene objects (handles auto-rotation via object system)
        game::g_ObjectManager.updateAll(deltaTime);
        game::g_RenderCallbacks.useGPU = useGPU;  // Update in case toggled
        
        // Update loaded models
        for (auto& model : g_LoadedModels) {
            model->update(deltaTime);
        }
        
        // ===== MOUSE CAMERA INPUT (Unity-style) =====
        POINT currentMousePos;
        GetCursorPos(&currentMousePos);
        
        // Calculate mouse delta
        int mouseDeltaX = 0, mouseDeltaY = 0;
        if (!firstFrame) {
            mouseDeltaX = currentMousePos.x - lastMousePos.x;
            mouseDeltaY = currentMousePos.y - lastMousePos.y;
        }
        firstFrame = false;
        lastMousePos = currentMousePos;
        
        // Check if our window is focused
        HWND ourWindow = (HWND)RS_GetWindowHandle();
        bool windowFocused = (GetForegroundWindow() == ourWindow);
        
        if (windowFocused) {
            // Right-click + drag = Look around (rotate camera)
            if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) {
                camYaw += mouseDeltaX * lookSensitivity;
                camPitch += mouseDeltaY * lookSensitivity;
            }
            
            // Middle-click + drag = Pan (move camera sideways/up-down)
            if (GetAsyncKeyState(VK_MBUTTON) & 0x8000) {
                // Calculate right and up vectors based on current rotation
                float yawRad = camYaw * 3.14159f / 180.0f;
                float rightX = cosf(yawRad);
                float rightZ = -sinf(yawRad);
                
                // Pan horizontally (along right vector)
                camX -= rightX * mouseDeltaX * panSensitivity;
                camZ -= rightZ * mouseDeltaX * panSensitivity;
                
                // Pan vertically (along world up)
                camY += mouseDeltaY * panSensitivity;
            }
            
            // Scroll wheel = Zoom (dolly forward/backward)
            int scrollDelta = RS_GetScrollDelta();
            if (scrollDelta != 0) {
                float yawRad = camYaw * 3.14159f / 180.0f;
                float pitchRad = camPitch * 3.14159f / 180.0f;
                
                // Move along view direction
                float forwardX = sinf(yawRad) * cosf(pitchRad);
                float forwardY = -sinf(pitchRad);
                float forwardZ = cosf(yawRad) * cosf(pitchRad);
                
                camX += forwardX * scrollDelta * scrollSpeed;
                camY += forwardY * scrollDelta * scrollSpeed;
                camZ += forwardZ * scrollDelta * scrollSpeed;
            }
        }
        
        // Clamp pitch to prevent flipping
        if (camPitch > 89.0f) camPitch = 89.0f;
        if (camPitch < -89.0f) camPitch = -89.0f;
        
        // Calculate forward and right vectors based on yaw
        float yawRad = camYaw * 3.14159f / 180.0f;
        float forwardX = sinf(yawRad);
        float forwardZ = cosf(yawRad);
        float rightX = cosf(yawRad);
        float rightZ = -sinf(yawRad);
        
        // WASD movement (fly mode, works alongside mouse)
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
        
        // ESC to exit fullscreen app
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
            PostMessage((HWND)RS_GetWindowHandle(), WM_CLOSE, 0, 0);
        }
        
        // ===== UPDATE VIEW MATRIX =====
        vec4 camPos = { camX, camY, camZ, 1.0f };
        matrix4x4 camTranslation = matrixTranslation(camPos);
        matrix4x4 camRotY = matrixRotationY(MatrixIdentity(), camYaw);
        matrix4x4 camRotX = matrixRotationX(camPitch);
        matrix4x4 camMatrix = matrixMultiplicationMatrix(camTranslation, camRotY);
        camMatrix = matrixMultiplicationMatrix(camMatrix, camRotX);
        SV_ViewMatrix = matrix4Inverse(camMatrix);

        // ===== UPDATE LIGHTING FROM UI VALUES =====
        SV_LightDirection = { lightDirX, lightDirY, lightDirZ };
        SV_AmbientLight = ambientIntensity;
        SV_SunColor = { sunColor[0], sunColor[1], sunColor[2] };
        
        // Calculate cube color from RGB
        unsigned int cubeColor = 0xFF000000 |
            ((unsigned int)(cubeColorRGB[0] * 255) << 16) |
            ((unsigned int)(cubeColorRGB[1] * 255) << 8) |
            (unsigned int)(cubeColorRGB[2] * 255);

        // ===== IMGUI UI UPDATE =====
        // Feed inputs to ImGui
        io.DeltaTime = deltaTime > 0.0f ? deltaTime : 0.016f;
        
        // Get mouse position relative to window
        HWND hwnd = (HWND)RS_GetWindowHandle();
        if (hwnd) {
            POINT mousePos;
            GetCursorPos(&mousePos);
            ScreenToClient(hwnd, &mousePos);
            io.MousePos = ImVec2((float)mousePos.x, (float)mousePos.y);
        }
        io.MouseDown[0] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        io.MouseDown[1] = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
        io.MouseDown[2] = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
        
        // Start ImGui frame
        ImGui::NewFrame();
        
        // Toggle settings panel with Tab key
        static bool tabWasPressed = false;
        bool tabPressed = (GetAsyncKeyState(VK_TAB) & 0x8000) != 0;
        if (tabPressed && !tabWasPressed) {
            showSettingsPanel = !showSettingsPanel;
        }
        tabWasPressed = tabPressed;
        
        // ===== MODEL LOADING / SCENE MANAGER PANEL =====
        if (showSettingsPanel) {
            ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(320, 400), ImGuiCond_FirstUseEver);
            
            if (ImGui::Begin("Scene Manager", &showSettingsPanel)) {
                ImGui::Text("Press TAB to toggle this panel");
                ImGui::Separator();
                
                // Model Loading Section
                ImGui::Text("Model Loading");
                if (ImGui::Button("+ Load Model", ImVec2(150, 30))) {
                    std::string filePath = OpenModelFileDialog(hwnd);
                    if (!filePath.empty()) {
                        auto model = std::make_unique<game::Model>();
                        if (model->loadModel(filePath)) {
                            // Position the new model in front of camera
                            model->setPosition(camX, 0.0f, camZ + 2.0f);
                            g_LoadedModels.push_back(std::move(model));
                        }
                    }
                }
                
                ImGui::Separator();
                
                // List loaded models
                ImGui::Text("Loaded Models (%zu):", g_LoadedModels.size());
                
                static int selectedModel = -1;
                for (size_t i = 0; i < g_LoadedModels.size(); i++) {
                    auto& model = g_LoadedModels[i];
                    char label[256];
                    snprintf(label, sizeof(label), "%s (%zu triangles)##model%zu", 
                             model->getName().c_str(), model->getTotalTriangles(), i);
                    
                    if (ImGui::Selectable(label, selectedModel == (int)i)) {
                        selectedModel = (int)i;
                    }
                }
                
                // Selected model controls
                if (selectedModel >= 0 && selectedModel < (int)g_LoadedModels.size()) {
                    ImGui::Separator();
                    ImGui::Text("Selected: %s", g_LoadedModels[selectedModel]->getName().c_str());
                    
                    // Position controls
                    float pos[3] = { 
                        g_LoadedModels[selectedModel]->getPosition().x,
                        g_LoadedModels[selectedModel]->getPosition().y,
                        g_LoadedModels[selectedModel]->getPosition().z
                    };
                    if (ImGui::DragFloat3("Position", pos, 0.1f)) {
                        g_LoadedModels[selectedModel]->setPosition(pos[0], pos[1], pos[2]);
                    }
                    
                    // Scale control
                    static float modelScale = 1.0f;
                    if (ImGui::DragFloat("Scale", &modelScale, 0.01f, 0.01f, 10.0f)) {
                        g_LoadedModels[selectedModel]->setScale(modelScale);
                    }
                    
                    // Delete button
                    if (ImGui::Button("Delete Model")) {
                        g_LoadedModels.erase(g_LoadedModels.begin() + selectedModel);
                        selectedModel = -1;
                    }
                }
                
                ImGui::Separator();
                
                // Camera Info
                ImGui::Text("Camera Position: %.2f, %.2f, %.2f", camX, camY, camZ);
                ImGui::Text("Camera Rotation: Yaw=%.1f Pitch=%.1f", camYaw, camPitch);
                
                ImGui::Separator();
                ImGui::Text("FPS: %.1f", 1.0f / deltaTime);
            }
            ImGui::End();
        }
        
        // End ImGui frame (generates draw data)
        ImGui::Render();

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
            
            // Draw cube triangles (textured) - using object manager
            game::g_ObjectManager.renderAll();
            
            // Render loaded models
            for (auto& model : g_LoadedModels) {
                model->render();
            }
            
            // Dispatch GPU compute and get pixels
            GPU_Dispatch(SCREEN_ARRAY);
            
            // Render ImGui on top
            ImGui_ImplSW_RenderDrawData(ImGui::GetDrawData(), SCREEN_ARRAY, RASTER_WIDTH, RASTER_HEIGHT);
            
        } else {
            // ===== CPU RENDERING PATH (fallback) =====
            clearColorBuffer(0xFF000000);
            
            // Render skybox (will fill background pixels where depth is still 1.0)
            if (game::g_Skybox && game::g_Skybox->isLoaded()) {
                game::g_Skybox->render(SCREEN_ARRAY, DEPTH_ARRAY, RASTER_WIDTH, RASTER_HEIGHT);
            }
            
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
            
            // Draw cube - using object manager
            game::g_ObjectManager.renderAll();
            
            // Render loaded models
            for (auto& model : g_LoadedModels) {
                model->render();
            }
            
            // Render ImGui on top
            ImGui_ImplSW_RenderDrawData(ImGui::GetDrawData(), SCREEN_ARRAY, RASTER_WIDTH, RASTER_HEIGHT);
        }

    } while (RS_Update(SCREEN_ARRAY, NUM_PIXELS));

    // Cleanup
    game::g_ObjectManager.clear();
    ImGui_ImplSW_Shutdown();
    ImGui::DestroyContext();
    if (useGPU) GPU_Shutdown();
    RS_Shutdown();
    FreeScreenBuffers();
    return 0;
}
