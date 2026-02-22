#pragma once
#define GLAD_IMPLEMENTATION
#include "glad.h"
#include "Defines.h"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iostream>
// GPU Vertex structure matching the compute shader
struct GPUVertex {
    float x, y, z, w;  // position
    unsigned int color;
    float u, v;
    float pad;
};

struct TextureBatch {
    const unsigned int* textureData = nullptr;
    int width = 0;
    int height = 0;
    std::vector<GPUVertex> triangles;
};

class GLCompute {
private:
    HWND hwnd = nullptr;
    HDC hdc = nullptr;
    HGLRC hglrc = nullptr;
    
    GLuint computeProgram = 0;
    GLuint pixelBuffer = 0;
    GLuint depthBuffer = 0;
    GLuint triangleBuffer = 0;
    GLuint lineBuffer = 0;
    GLuint textureBuffer = 0;
    
    int width, height;
    int texW = 0, texH = 0;
    bool initialized = false;
    bool useTexture = false;
    bool uvDebug = false;
    bool textureUploadedThisFrame = false;
    int texturedTrianglesThisFrame = 0;
    int solidTrianglesThisFrame = 0;

    std::vector<GPUVertex> solidTriangleData;
    std::vector<TextureBatch> texturedBatches;
    TextureBatch* activeTextureBatch = nullptr;
    std::vector<GPUVertex> lineData;

public:
    GLCompute() : width(RASTER_WIDTH), height(RASTER_HEIGHT) {}
    
    ~GLCompute() {
        shutdown();
    }
    
    bool init() {
        // Update dimensions to match current screen resolution
        width = RASTER_WIDTH;
        height = RASTER_HEIGHT;
        
        // Create a hidden window for OpenGL context
        WNDCLASSA wc = {};
        wc.lpfnWndProc = DefWindowProcA;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = "GLComputeClass";
        RegisterClassA(&wc);
        
        hwnd = CreateWindowA("GLComputeClass", "GLCompute", 0, 0, 0, 1, 1, nullptr, nullptr, wc.hInstance, nullptr);
        if (!hwnd) {
            std::cerr << "Failed to create window\n";
            return false;
        }
        
        hdc = GetDC(hwnd);
        
        // Set pixel format
        PIXELFORMATDESCRIPTOR pfd = {};
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        pfd.cDepthBits = 24;
        
        int format = ChoosePixelFormat(hdc, &pfd);
        SetPixelFormat(hdc, format, &pfd);
        
        // Create OpenGL context
        hglrc = wglCreateContext(hdc);
        if (!hglrc) {
            std::cerr << "Failed to create GL context\n";
            return false;
        }
        
        wglMakeCurrent(hdc, hglrc);
        
        // Load OpenGL functions
        if (!gladLoadGL()) {
            std::cerr << "Failed to load OpenGL functions\n";
            return false;
        }
        
        // Load and compile compute shader
        if (!loadComputeShader("CGSTemplate/rasterizer.comp")) {
            // Try alternate path
            if (!loadComputeShader("rasterizer.comp")) {
                std::cerr << "Failed to load compute shader\n";
                return false;
            }
        }
        
        // Create buffers
        glGenBuffers(1, &pixelBuffer);
        glGenBuffers(1, &depthBuffer);
        glGenBuffers(1, &triangleBuffer);
        glGenBuffers(1, &lineBuffer);
        
        // Initialize pixel buffer
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, pixelBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, NUM_PIXELS * sizeof(unsigned int), nullptr, GL_DYNAMIC_READ);
        
        // Initialize depth buffer
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, depthBuffer);
        std::vector<float> depthInit(NUM_PIXELS, 1000000.0f);
        glBufferData(GL_SHADER_STORAGE_BUFFER, NUM_PIXELS * sizeof(float), depthInit.data(), GL_DYNAMIC_DRAW);
        
        // Initialize triangle buffer (will resize as needed)
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, triangleBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, 1024 * sizeof(GPUVertex), nullptr, GL_DYNAMIC_DRAW);
        
        // Initialize line buffer
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, lineBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, 1024 * sizeof(GPUVertex), nullptr, GL_DYNAMIC_DRAW);
        
        // Initialize texture buffer
        glGenBuffers(1, &textureBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, textureBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, 4, nullptr, GL_STATIC_DRAW); // Will resize when texture uploaded
        
        initialized = true;
        std::cout << "GPU Compute initialized successfully!\n";
        return true;
    }
    
    void shutdown() {
        if (computeProgram) glDeleteProgram(computeProgram);
        if (pixelBuffer) glDeleteBuffers(1, &pixelBuffer);
        if (depthBuffer) glDeleteBuffers(1, &depthBuffer);
        if (triangleBuffer) glDeleteBuffers(1, &triangleBuffer);
        if (lineBuffer) glDeleteBuffers(1, &lineBuffer);
        if (textureBuffer) glDeleteBuffers(1, &textureBuffer);
        
        if (hglrc) {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(hglrc);
        }
        if (hwnd && hdc) ReleaseDC(hwnd, hdc);
        if (hwnd) DestroyWindow(hwnd);
        
        initialized = false;
    }
    
    // Clear for new frame
    void beginFrame() {
        solidTriangleData.clear();
        texturedBatches.clear();
        activeTextureBatch = nullptr;
        lineData.clear();
        textureUploadedThisFrame = false;
        texturedTrianglesThisFrame = 0;
        solidTrianglesThisFrame = 0;
    }
    
    // Upload texture data (BGRA format)
    void uploadTexture(const unsigned int* textureData, int width, int height) {
        if (!initialized) return;
        if (!textureData || width <= 0 || height <= 0) {
            return;
        }
        texW = width;
        texH = height;
        textureUploadedThisFrame = true;
        useTexture = true;

        for (auto& batch : texturedBatches) {
            if (batch.textureData == textureData && batch.width == width && batch.height == height) {
                activeTextureBatch = &batch;
                return;
            }
        }

        texturedBatches.push_back(TextureBatch{ textureData, width, height, {} });
        activeTextureBatch = &texturedBatches.back();
    }
    
    // Enable/disable texture sampling (call before adding triangles)
    void setUseTexture(bool use) {
        useTexture = use;
    }
    
    // Add a triangle (screen-space vertices) - pad=0 means NOT textured
    void addTriangle(float x0, float y0, float z0, unsigned int c0,
                     float x1, float y1, float z1, unsigned int c1,
                     float x2, float y2, float z2, unsigned int c2) {
        solidTriangleData.push_back({x0, y0, z0, 1.0f, c0, 0, 0, 0.0f});  // pad=0 = solid color
        solidTriangleData.push_back({x1, y1, z1, 1.0f, c1, 0, 0, 0.0f});
        solidTriangleData.push_back({x2, y2, z2, 1.0f, c2, 0, 0, 0.0f});
        solidTrianglesThisFrame++;
    }
    
    // Add a textured triangle (screen-space vertices with UVs) - pad=1 means TEXTURED
    void addTexturedTriangle(float x0, float y0, float z0, unsigned int c0, float u0, float v0,
                              float x1, float y1, float z1, unsigned int c1, float u1, float v1,
                              float x2, float y2, float z2, unsigned int c2, float u2, float v2) {
        if (!activeTextureBatch) {
            addTriangle(x0, y0, z0, c0, x1, y1, z1, c1, x2, y2, z2, c2);
            return;
        }

        activeTextureBatch->triangles.push_back({x0, y0, z0, 1.0f, c0, u0, v0, 1.0f});  // pad=1 = textured
        activeTextureBatch->triangles.push_back({x1, y1, z1, 1.0f, c1, u1, v1, 1.0f});
        activeTextureBatch->triangles.push_back({x2, y2, z2, 1.0f, c2, u2, v2, 1.0f});
        texturedTrianglesThisFrame++;
    }
    
    // Add a line (screen-space vertices)
    void addLine(float x0, float y0, float z0, unsigned int c0,
                 float x1, float y1, float z1, unsigned int c1) {
        lineData.push_back({x0, y0, z0, 1.0f, c0, 0, 0, 0});
        lineData.push_back({x1, y1, z1, 1.0f, c1, 0, 0, 0});
    }
    
    // Execute compute shader and read back pixels
    void dispatch(unsigned int* outputPixels) {
        if (!initialized) return;
        
        // Reset depth + pixel buffers
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, depthBuffer);
        std::vector<float> depthReset(NUM_PIXELS, 1000000.0f);
        glBufferData(GL_SHADER_STORAGE_BUFFER, NUM_PIXELS * sizeof(float), depthReset.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, pixelBuffer);
        std::vector<unsigned int> pixelReset(NUM_PIXELS, 0);
        glBufferData(GL_SHADER_STORAGE_BUFFER, NUM_PIXELS * sizeof(unsigned int), pixelReset.data(), GL_DYNAMIC_DRAW);
        
        // Upload line data
        if (!lineData.empty()) {
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, lineBuffer);
            glBufferData(GL_SHADER_STORAGE_BUFFER, lineData.size() * sizeof(GPUVertex), lineData.data(), GL_DYNAMIC_DRAW);
        }
        
        // Bind buffers
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, pixelBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, depthBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, triangleBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, lineBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, textureBuffer);
        
        // Set uniforms
        glUseProgram(computeProgram);
        glUniform1i(glGetUniformLocation(computeProgram, "screenWidth"), width);
        glUniform1i(glGetUniformLocation(computeProgram, "screenHeight"), height);
        glUniform1i(glGetUniformLocation(computeProgram, "uvDebug"), uvDebug ? 1 : 0);

        auto dispatchTriangles = [&](const std::vector<GPUVertex>& triangles, int tw, int th, bool passUseTexture, bool includeLines) {
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, triangleBuffer);
            if (!triangles.empty()) {
                glBufferData(GL_SHADER_STORAGE_BUFFER, triangles.size() * sizeof(GPUVertex), triangles.data(), GL_DYNAMIC_DRAW);
            } else {
                glBufferData(GL_SHADER_STORAGE_BUFFER, 4, nullptr, GL_DYNAMIC_DRAW);
            }

            if (includeLines && !lineData.empty()) {
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, lineBuffer);
                glBufferData(GL_SHADER_STORAGE_BUFFER, lineData.size() * sizeof(GPUVertex), lineData.data(), GL_DYNAMIC_DRAW);
            } else {
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, lineBuffer);
                glBufferData(GL_SHADER_STORAGE_BUFFER, 4, nullptr, GL_DYNAMIC_DRAW);
            }

            glUniform1i(glGetUniformLocation(computeProgram, "numTriangles"), (int)(triangles.size() / 3));
            glUniform1i(glGetUniformLocation(computeProgram, "numLines"), includeLines ? (int)(lineData.size() / 2) : 0);
            glUniform1i(glGetUniformLocation(computeProgram, "texWidth"), tw);
            glUniform1i(glGetUniformLocation(computeProgram, "texHeight"), th);
            glUniform1i(glGetUniformLocation(computeProgram, "useTexture"), passUseTexture ? 1 : 0);

            GLuint groupsX = (width + 15) / 16;
            GLuint groupsY = (height + 15) / 16;
            glDispatchCompute(groupsX, groupsY, 1);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        };

        for (const auto& batch : texturedBatches) {
            if (batch.triangles.empty()) continue;
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, textureBuffer);
            glBufferData(GL_SHADER_STORAGE_BUFFER, batch.width * batch.height * sizeof(unsigned int), batch.textureData, GL_STATIC_DRAW);
            dispatchTriangles(batch.triangles, batch.width, batch.height, true, false);
        }

        dispatchTriangles(solidTriangleData, 0, 0, false, true);
        
        // Read back pixel data
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, pixelBuffer);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, NUM_PIXELS * sizeof(unsigned int), outputPixels);
    }
    
    bool isInitialized() const { return initialized; }
    void setUVTextureDebug(bool enabled) { uvDebug = enabled; }
    int getTextureWidth() const { return texW; }
    int getTextureHeight() const { return texH; }
    bool didUploadTextureThisFrame() const { return textureUploadedThisFrame; }
    int getTexturedTriangleCountThisFrame() const { return texturedTrianglesThisFrame; }
    int getSolidTriangleCountThisFrame() const { return solidTrianglesThisFrame; }

private:
    bool loadComputeShader(const char* filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return false;
        }
        
        std::stringstream ss;
        ss << file.rdbuf();
        std::string source = ss.str();
        const char* srcPtr = source.c_str();
        
        // Create and compile shader
        GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
        glShaderSource(shader, 1, &srcPtr, nullptr);
        glCompileShader(shader);
        
        // Check compilation
        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            std::cerr << "Compute shader compilation failed:\n" << infoLog << std::endl;
            glDeleteShader(shader);
            return false;
        }
        
        // Create program
        computeProgram = glCreateProgram();
        glAttachShader(computeProgram, shader);
        glLinkProgram(computeProgram);
        
        // Check linking
        glGetProgramiv(computeProgram, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(computeProgram, 512, nullptr, infoLog);
            std::cerr << "Compute program linking failed:\n" << infoLog << std::endl;
            glDeleteShader(shader);
            return false;
        }
        
        glDeleteShader(shader);
        std::cout << "Loaded compute shader: " << filepath << std::endl;
        return true;
    }
};

// Global instance
inline GLCompute g_GLCompute;

// Helper functions for easy use from existing code
inline bool GPU_Init() {
    return g_GLCompute.init();
}

inline void GPU_BeginFrame() {
    g_GLCompute.beginFrame();
}

inline void GPU_AddTriangle(const vertex& v0, const vertex& v1, const vertex& v2) {
    g_GLCompute.addTriangle(
        v0.pos.x, v0.pos.y, v0.pos.z, v0.color,
        v1.pos.x, v1.pos.y, v1.pos.z, v1.color,
        v2.pos.x, v2.pos.y, v2.pos.z, v2.color
    );
}

inline void GPU_AddTexturedTriangle(const vertex& v0, const vertex& v1, const vertex& v2) {
    g_GLCompute.addTexturedTriangle(
        v0.pos.x, v0.pos.y, v0.pos.z, v0.color, v0.u, v0.v,
        v1.pos.x, v1.pos.y, v1.pos.z, v1.color, v1.u, v1.v,
        v2.pos.x, v2.pos.y, v2.pos.z, v2.color, v2.u, v2.v
    );
}

inline void GPU_UploadTexture(const unsigned int* textureData, int width, int height) {
    g_GLCompute.uploadTexture(textureData, width, height);
}

inline void GPU_AddLine(const vertex& v0, const vertex& v1, unsigned int color) {
    g_GLCompute.addLine(
        v0.pos.x, v0.pos.y, v0.pos.z, color,
        v1.pos.x, v1.pos.y, v1.pos.z, color
    );
}

inline void GPU_Dispatch(unsigned int* pixels) {
    g_GLCompute.dispatch(pixels);
}

inline void GPU_Shutdown() {
    g_GLCompute.shutdown();
}
