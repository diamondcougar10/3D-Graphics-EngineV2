// Custom ImGui Software Renderer Backend
// Renders ImGui draw data to a raw pixel buffer
#pragma once

#include "imgui.h"

// Initialize the software renderer - call after ImGui::CreateContext()
bool ImGui_ImplSW_Init();

// Shutdown the software renderer
void ImGui_ImplSW_Shutdown();

// Create font texture atlas
void ImGui_ImplSW_CreateFontsTexture();

// Render ImGui draw data to pixel buffer
// pixels: ARGB format uint32_t array
// width, height: dimensions of the pixel buffer
void ImGui_ImplSW_RenderDrawData(ImDrawData* draw_data, unsigned int* pixels, int width, int height);
