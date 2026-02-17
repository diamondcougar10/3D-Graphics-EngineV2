// Custom ImGui Software Renderer Backend Implementation
// Renders ImGui draw data to a raw pixel buffer

#include "imgui_impl_sw.h"
#include "imgui.h"
#include <cmath>
#include <algorithm>

// Font texture data
static unsigned char* g_FontTexture = nullptr;
static int g_FontTextureWidth = 0;
static int g_FontTextureHeight = 0;

bool ImGui_ImplSW_Init()
{
    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = "imgui_impl_software";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    return true;
}

void ImGui_ImplSW_Shutdown()
{
    if (g_FontTexture) {
        delete[] g_FontTexture;
        g_FontTexture = nullptr;
    }
}

void ImGui_ImplSW_CreateFontsTexture()
{
    ImGuiIO& io = ImGui::GetIO();
    
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    
    // Copy the font texture
    g_FontTextureWidth = width;
    g_FontTextureHeight = height;
    g_FontTexture = new unsigned char[width * height * 4];
    memcpy(g_FontTexture, pixels, width * height * 4);
    
    // Set texture ID
    io.Fonts->SetTexID((ImTextureID)(intptr_t)g_FontTexture);
}

// Helper: clamp value to range
inline int clamp_int(int v, int mn, int mx) { return v < mn ? mn : (v > mx ? mx : v); }
inline float clamp_float(float v, float mn, float mx) { return v < mn ? mn : (v > mx ? mx : v); }

// Sample texture with bilinear filtering
static unsigned int sampleTexture(const unsigned char* tex, int texW, int texH, float u, float v)
{
    // Clamp UV coordinates
    u = clamp_float(u, 0.0f, 1.0f);
    v = clamp_float(v, 0.0f, 1.0f);
    
    int x = clamp_int((int)(u * (texW - 1)), 0, texW - 1);
    int y = clamp_int((int)(v * (texH - 1)), 0, texH - 1);
    
    int idx = (y * texW + x) * 4;
    unsigned int r = tex[idx + 0];
    unsigned int g = tex[idx + 1];
    unsigned int b = tex[idx + 2];
    unsigned int a = tex[idx + 3];
    
    return (a << 24) | (r << 16) | (g << 8) | b;
}

// Alpha blend two colors
static unsigned int alphaBlend(unsigned int dst, unsigned int src)
{
    unsigned int srcA = (src >> 24) & 0xFF;
    if (srcA == 0) return dst;
    if (srcA == 255) return src;
    
    unsigned int srcR = (src >> 16) & 0xFF;
    unsigned int srcG = (src >> 8) & 0xFF;
    unsigned int srcB = src & 0xFF;
    
    unsigned int dstA = (dst >> 24) & 0xFF;
    unsigned int dstR = (dst >> 16) & 0xFF;
    unsigned int dstG = (dst >> 8) & 0xFF;
    unsigned int dstB = dst & 0xFF;
    
    unsigned int invA = 255 - srcA;
    unsigned int outR = (srcR * srcA + dstR * invA) / 255;
    unsigned int outG = (srcG * srcA + dstG * invA) / 255;
    unsigned int outB = (srcB * srcA + dstB * invA) / 255;
    unsigned int outA = srcA + (dstA * invA) / 255;
    
    return (outA << 24) | (outR << 16) | (outG << 8) | outB;
}

// Multiply vertex color with texture color
static unsigned int multiplyColors(unsigned int texColor, unsigned int vtxColor)
{
    unsigned int texA = (texColor >> 24) & 0xFF;
    unsigned int texR = (texColor >> 16) & 0xFF;
    unsigned int texG = (texColor >> 8) & 0xFF;
    unsigned int texB = texColor & 0xFF;
    
    unsigned int vtxA = (vtxColor >> 24) & 0xFF;
    unsigned int vtxR = (vtxColor >> 16) & 0xFF;
    unsigned int vtxG = (vtxColor >> 8) & 0xFF;
    unsigned int vtxB = vtxColor & 0xFF;
    
    return ((texA * vtxA / 255) << 24) |
           ((texR * vtxR / 255) << 16) |
           ((texG * vtxG / 255) << 8) |
           (texB * vtxB / 255);
}

// Convert ImGui color to ARGB
static unsigned int imguiColorToARGB(ImU32 col)
{
    // ImGui uses ABGR, we need ARGB
    unsigned int a = (col >> 24) & 0xFF;
    unsigned int b = (col >> 16) & 0xFF;
    unsigned int g = (col >> 8) & 0xFF;
    unsigned int r = col & 0xFF;
    return (a << 24) | (r << 16) | (g << 8) | b;
}

// Render a single triangle with texture
static void renderTriangle(
    unsigned int* pixels, int fbWidth, int fbHeight,
    const ImDrawVert& v0, const ImDrawVert& v1, const ImDrawVert& v2,
    const unsigned char* texture, int texWidth, int texHeight,
    int clipMinX, int clipMinY, int clipMaxX, int clipMaxY)
{
    // Get vertex positions
    float x0 = v0.pos.x, y0 = v0.pos.y;
    float x1 = v1.pos.x, y1 = v1.pos.y;
    float x2 = v2.pos.x, y2 = v2.pos.y;
    
    // Compute bounding box
    int minX = (int)std::floor(std::min({x0, x1, x2}));
    int maxX = (int)std::ceil(std::max({x0, x1, x2}));
    int minY = (int)std::floor(std::min({y0, y1, y2}));
    int maxY = (int)std::ceil(std::max({y0, y1, y2}));
    
    // Clip to scissor rect
    minX = std::max(minX, clipMinX);
    maxX = std::min(maxX, clipMaxX);
    minY = std::max(minY, clipMinY);
    maxY = std::min(maxY, clipMaxY);
    
    // Clip to framebuffer
    minX = std::max(minX, 0);
    maxX = std::min(maxX, fbWidth - 1);
    minY = std::max(minY, 0);
    maxY = std::min(maxY, fbHeight - 1);
    
    // Compute edge equation constants
    float dx01 = x0 - x1, dy01 = y0 - y1;
    float dx12 = x1 - x2, dy12 = y1 - y2;
    float dx20 = x2 - x0, dy20 = y2 - y0;
    
    // Triangle area (2x)
    float area = dx01 * (y2 - y0) - dy01 * (x2 - x0);
    if (std::abs(area) < 0.001f) return; // Degenerate triangle
    float invArea = 1.0f / area;
    
    // Get vertex colors
    unsigned int c0 = imguiColorToARGB(v0.col);
    unsigned int c1 = imguiColorToARGB(v1.col);
    unsigned int c2 = imguiColorToARGB(v2.col);
    
    // Rasterize
    for (int py = minY; py <= maxY; py++) {
        for (int px = minX; px <= maxX; px++) {
            float fx = px + 0.5f;
            float fy = py + 0.5f;
            
            // Barycentric coordinates
            float w0 = (x1 - x2) * (fy - y2) - (y1 - y2) * (fx - x2);
            float w1 = (x2 - x0) * (fy - y0) - (y2 - y0) * (fx - x0);
            float w2 = (x0 - x1) * (fy - y1) - (y0 - y1) * (fx - x1);
            
            // Check if inside triangle
            bool inside = (area > 0) ? (w0 >= 0 && w1 >= 0 && w2 >= 0) : (w0 <= 0 && w1 <= 0 && w2 <= 0);
            if (!inside) continue;
            
            // Normalize barycentric coords
            w0 *= invArea;
            w1 *= invArea;
            w2 *= invArea;
            
            // Interpolate UV
            float u = w0 * v0.uv.x + w1 * v1.uv.x + w2 * v2.uv.x;
            float v = w0 * v0.uv.y + w1 * v1.uv.y + w2 * v2.uv.y;
            
            // Interpolate color
            unsigned int a = (unsigned int)(w0 * ((c0 >> 24) & 0xFF) + w1 * ((c1 >> 24) & 0xFF) + w2 * ((c2 >> 24) & 0xFF));
            unsigned int r = (unsigned int)(w0 * ((c0 >> 16) & 0xFF) + w1 * ((c1 >> 16) & 0xFF) + w2 * ((c2 >> 16) & 0xFF));
            unsigned int g = (unsigned int)(w0 * ((c0 >> 8) & 0xFF) + w1 * ((c1 >> 8) & 0xFF) + w2 * ((c2 >> 8) & 0xFF));
            unsigned int b = (unsigned int)(w0 * (c0 & 0xFF) + w1 * (c1 & 0xFF) + w2 * (c2 & 0xFF));
            unsigned int vtxColor = (a << 24) | (r << 16) | (g << 8) | b;
            
            // Sample texture
            unsigned int texColor = 0xFFFFFFFF;
            if (texture) {
                texColor = sampleTexture(texture, texWidth, texHeight, u, v);
            }
            
            // Multiply texture and vertex color
            unsigned int srcColor = multiplyColors(texColor, vtxColor);
            
            // Alpha blend to framebuffer
            int idx = py * fbWidth + px;
            pixels[idx] = alphaBlend(pixels[idx], srcColor);
        }
    }
}

void ImGui_ImplSW_RenderDrawData(ImDrawData* draw_data, unsigned int* pixels, int width, int height)
{
    if (!draw_data) return;
    
    // Get framebuffer scale
    float fbScaleX = draw_data->FramebufferScale.x;
    float fbScaleY = draw_data->FramebufferScale.y;
    
    // Render each command list
    for (int n = 0; n < draw_data->CmdListsCount; n++) {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        const ImDrawVert* vtx_buffer = cmd_list->VtxBuffer.Data;
        const ImDrawIdx* idx_buffer = cmd_list->IdxBuffer.Data;
        
        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            
            if (pcmd->UserCallback) {
                pcmd->UserCallback(cmd_list, pcmd);
                continue;
            }
            
            // Get scissor rect
            int clipMinX = (int)(pcmd->ClipRect.x * fbScaleX);
            int clipMinY = (int)(pcmd->ClipRect.y * fbScaleY);
            int clipMaxX = (int)(pcmd->ClipRect.z * fbScaleX);
            int clipMaxY = (int)(pcmd->ClipRect.w * fbScaleY);
            
            // Clamp to screen
            clipMinX = std::max(clipMinX, 0);
            clipMinY = std::max(clipMinY, 0);
            clipMaxX = std::min(clipMaxX, width);
            clipMaxY = std::min(clipMaxY, height);
            
            if (clipMinX >= clipMaxX || clipMinY >= clipMaxY)
                continue;
            
            // Get texture
            const unsigned char* texture = (const unsigned char*)pcmd->GetTexID();
            int texWidth = g_FontTextureWidth;
            int texHeight = g_FontTextureHeight;
            
            // Render triangles
            for (unsigned int i = 0; i < pcmd->ElemCount; i += 3) {
                unsigned int idx0 = idx_buffer[pcmd->IdxOffset + i + 0] + pcmd->VtxOffset;
                unsigned int idx1 = idx_buffer[pcmd->IdxOffset + i + 1] + pcmd->VtxOffset;
                unsigned int idx2 = idx_buffer[pcmd->IdxOffset + i + 2] + pcmd->VtxOffset;
                
                renderTriangle(pixels, width, height,
                    vtx_buffer[idx0], vtx_buffer[idx1], vtx_buffer[idx2],
                    texture, texWidth, texHeight,
                    clipMinX, clipMinY, clipMaxX, clipMaxY);
            }
        }
    }
}
