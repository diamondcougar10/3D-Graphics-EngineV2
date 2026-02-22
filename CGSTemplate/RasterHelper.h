#pragma once
#include "Defines.h"
#include "Shaders.h"
#include "celestial.h"

// Function declarations
int coordinateTranslation2D(int x, int y, int Width);
float lineEquation(vertex start, vertex end, float x, float y);
float baryInterpolation(float a, float b, float y, Triangle tri);
Triangle baryRatio(vertex v0, vertex v1, vertex v2, float currX, float currY);
void pixelDrawer(int x, int y, float z, unsigned int color);
void clearColorBuffer(unsigned int color);
void LineDrawer(vertex start, vertex end, unsigned int color);
void fillTriangle(vertex v0, vertex v1, vertex v2, const unsigned* texture, int texWidth, int texHeight);
vertex toScreen(vertex inp);
void drawLine(const vertex& Start, const vertex& End, unsigned color);
void DrawTriangle(vertex& v0, vertex& v1, vertex& v2, const unsigned* texture, int texWidth, int texHeight);
unsigned sampleTexture(const unsigned* texture, int texWidth, int texHeight, float u, float v);
void Blit(const unsigned int* source, int srcWidth, int srcHeight, unsigned int* dest, int destWidth, int destHeight, int cubeFace, float scale);

// Function implementations
int coordinateTranslation2D(int x, int y, int Width)
{
	return y * Width + x;
}

float lineEquation(vertex start, vertex end, float x, float y)
{
	float ax = (start.pos.y - end.pos.y) * x;
	float by = (end.pos.x - start.pos.x) * y;
	float c = (start.pos.x * end.pos.y) - (start.pos.y * end.pos.x);
	return ax + by + c;
}

float baryInterpolation(float a, float b, float y, Triangle tri)
{
	float interpolation = (a * tri.a) + (b * tri.b) + (y * tri.y);
	return interpolation;
}

Triangle baryRatio(vertex v0, vertex v1, vertex v2, float currX, float currY)
{
	Triangle tri;
	float mB = lineEquation(v0, v2, v1.pos.x, v1.pos.y);
	float mG = lineEquation(v1, v0, v2.pos.x, v2.pos.y);
	float mA = lineEquation(v2, v1, v0.pos.x, v0.pos.y);
	float pB = lineEquation(v0, v2, currX, currY);
	float pG = lineEquation(v1, v0, currX, currY);
	float pA = lineEquation(v2, v1, currX, currY);
	tri.a = (pA / mA);
	tri.b = (pB / mB);
	tri.y = (pG / mG);
	return tri;
}

void pixelDrawer(int x, int y, float z, unsigned int color)
{
	if (x < 0 || x >= (int)RASTER_WIDTH || y < 0 || y >= (int)RASTER_HEIGHT)
		return;

	int index = y * RASTER_WIDTH + x;

	if (z < DEPTH_ARRAY[index])
	{
		DEPTH_ARRAY[index] = z;
		SCREEN_ARRAY[index] = color;
	}
}

// Star field data (generated once, dynamically allocated)
inline unsigned int* STAR_BUFFER = nullptr;
bool starsGenerated = false;

void generateStarField() {
	if (starsGenerated) return;
	
	// Allocate star buffer if needed
	if (!STAR_BUFFER) {
		STAR_BUFFER = new unsigned int[NUM_PIXELS];
	}
	
	// Fill with black space
	for (int i = 0; i < NUM_PIXELS; i++) {
		STAR_BUFFER[i] = 0xFF000008; // Very dark blue-black
	}
	
	// Add random stars
	srand(42); // Fixed seed for consistent stars
	for (int s = 0; s < 300; s++) {
		int x = rand() % RASTER_WIDTH;
		int y = rand() % RASTER_HEIGHT;
		int brightness = 100 + (rand() % 155); // 100-255
		int index = y * RASTER_WIDTH + x;
		
		// Slight color variation (white, blue-white, yellow-white)
		int colorType = rand() % 3;
		unsigned int starColor;
		if (colorType == 0) {
			starColor = 0xFF000000 | (brightness << 16) | (brightness << 8) | brightness; // White
		} else if (colorType == 1) {
			starColor = 0xFF000000 | ((brightness * 3/4) << 16) | ((brightness * 3/4) << 8) | brightness; // Blue-ish
		} else {
			starColor = 0xFF000000 | (brightness << 16) | ((brightness * 9/10) << 8) | (brightness * 3/4); // Yellow-ish
		}
		STAR_BUFFER[index] = starColor;
	}
	starsGenerated = true;
}

void clearColorBuffer(unsigned int color)
{
	generateStarField();
	for (unsigned int i = 0; i < NUM_PIXELS; i++)
	{
		SCREEN_ARRAY[i] = STAR_BUFFER[i]; // Copy star field
		DEPTH_ARRAY[i] = 1.0f;
	}
}

void LineDrawer(vertex start, vertex end, unsigned int color)
{
	float dx = end.pos.x - start.pos.x;
	float dy = end.pos.y - start.pos.y;
	float largest = max(abs(dx), abs(dy));
	if (largest < 1.0f) largest = 1.0f;
	for (int i = 0; i <= (int)largest; i++)
	{
		float ratio = (largest > 0) ? float(i) / largest : 0.0f;
		float currX = Lerp(start.pos.x, end.pos.x, ratio);
		float currY = Lerp(start.pos.y, end.pos.y, ratio);
		float currZ = Lerp(start.pos.z, end.pos.z, ratio);
		pixelDrawer((int)currX, (int)currY, currZ, color);
	}
}

unsigned sampleTexture(const unsigned* texture, int texWidth, int texHeight, float u, float v)
{
	// Clamp UV coordinates to stay within texture bounds
	u = (u < 0.0f) ? 0.0f : ((u > 1.0f) ? 1.0f : u);
	v = (v < 0.0f) ? 0.0f : ((v > 1.0f) ? 1.0f : v);

	// Calculate texture coordinates
	int x = static_cast<int>(u * (texWidth - 1));
	int y = static_cast<int>(v * (texHeight - 1));

	// Sample color from texture (ARGB format)
	return texture[y * texWidth + x];
}

// Global lighting factor for current triangle (set by DrawTriangle)
float g_currentLightingFactor = 1.0f;

// Apply sun lighting to a color with warm tint
unsigned int applyLighting(unsigned int color, float lighting) {
    unsigned int a = (color >> 24) & 0xFF;
    unsigned int r = (color >> 16) & 0xFF;
    unsigned int g = (color >> 8) & 0xFF;
    unsigned int b = color & 0xFF;
    
    // Apply lighting with sun color tint
    r = (unsigned int)(r * lighting * SV_SunColor.x);
    g = (unsigned int)(g * lighting * SV_SunColor.y);
    b = (unsigned int)(b * lighting * SV_SunColor.z);
    
    // Clamp values
    r = (r > 255) ? 255 : r;
    g = (g > 255) ? 255 : g;
    b = (b > 255) ? 255 : b;
    
    return (a << 24) | (r << 16) | (g << 8) | b;
}

void fillTriangle(vertex v0, vertex v1, vertex v2, const unsigned* texture, int texWidth, int texHeight)
{
	// Use standard (non-perspective-corrected) interpolation for now
	float startX = min(min(v0.pos.x, v1.pos.x), v2.pos.x);
	float startY = min(min(v0.pos.y, v1.pos.y), v2.pos.y);
	float endX = max(max(v0.pos.x, v1.pos.x), v2.pos.x);
	float endY = max(max(v0.pos.y, v1.pos.y), v2.pos.y);

	for (int y = static_cast<int>(startY); y <= static_cast<int>(endY); y++)
	{
		for (int x = static_cast<int>(startX); x <= static_cast<int>(endX); x++)
		{
			Triangle tri = baryRatio(v0, v1, v2, static_cast<float>(x), static_cast<float>(y));
			if (tri.b >= 0 && tri.b <= 1 && tri.y >= 0 && tri.y <= 1 && tri.a >= 0 && tri.a <= 1)
			{
				// Standard interpolation of UV coordinates
				float u = (v0.u * tri.a) + (v1.u * tri.b) + (v2.u * tri.y);
				float v = (v0.v * tri.a) + (v1.v * tri.b) + (v2.v * tri.y);
				float z = (v0.pos.z * tri.a) + (v1.pos.z * tri.b) + (v2.pos.z * tri.y);

				// Get pixel color - use texture if available, otherwise use vertex color
				unsigned int pixelColor;
				if (texture && texWidth > 0 && texHeight > 0) {
					pixelColor = sampleTexture(texture, texWidth, texHeight, u, v);
				} else {
					// Use interpolated vertex color
					pixelColor = v0.color;  // All vertices have same color for solid-colored meshes
				}

				// Apply lighting to the color
				pixelColor = applyLighting(pixelColor, g_currentLightingFactor);

				// Draw the pixel
				pixelDrawer(x, y, z, pixelColor);
			}
		}
	}
}

vertex toScreen(vertex inp)
{
	int x = static_cast<int>((inp.pos.x + 1) * (RASTER_WIDTH / 2));
	int y = static_cast<int>((1 - inp.pos.y) * (RASTER_HEIGHT / 2));
	vertex ans;
	ans.pos.x = static_cast<float>(x);
	ans.pos.y = static_cast<float>(y);
	ans.pos.z = inp.pos.z;
	ans.pos.w = inp.pos.w;
	ans.u = inp.u;     // Copy UV coordinates
	ans.v = inp.v;
	ans.color = inp.color;
	return ans;
}

void drawLine(const vertex& Start, const vertex& End, unsigned color) {
	vertex copyStart = Start;
	vertex copyEnd = End;
	if (VertexShader) {
		VertexShader(copyStart);
		VertexShader(copyEnd);
	}
	vertex screenStart = toScreen(copyStart);
	vertex screenEnd = toScreen(copyEnd);
	Pixel copyColor;
	copyColor.color = color;
	if (PixelShader) {
		PixelShader(copyColor);
	}
	LineDrawer(screenStart, screenEnd, copyColor.color);
}

void DrawTriangle(vertex& v0, vertex& v1, vertex& v2, const unsigned* texture, int texWidth, int texHeight)
{
	vertex copy_v0 = v0;
	vertex copy_v1 = v1;
	vertex copy_v2 = v2;

	// Transform vertices to world space for lighting calculation
	vertex world_v0 = v0;
	vertex world_v1 = v1;
	vertex world_v2 = v2;
	world_v0.pos = matrixMultiplicationVec(SV_WorldMatrix, v0.pos);
	world_v1.pos = matrixMultiplicationVec(SV_WorldMatrix, v1.pos);
	world_v2.pos = matrixMultiplicationVec(SV_WorldMatrix, v2.pos);

	// Calculate face normal and lighting
	vec3 faceNormal = calculateFaceNormal(world_v0.pos, world_v1.pos, world_v2.pos);
	g_currentLightingFactor = calculateLighting(faceNormal);

	if (VertexShader)
	{
		VertexShader(copy_v0);
		VertexShader(copy_v1);
		VertexShader(copy_v2);
	}
	vertex screen_v0 = toScreen(copy_v0);
	vertex screen_v1 = toScreen(copy_v1);
	vertex screen_v2 = toScreen(copy_v2);
	
	// Backface culling: calculate signed area in screen space
	float edge1x = screen_v1.pos.x - screen_v0.pos.x;
	float edge1y = screen_v1.pos.y - screen_v0.pos.y;
	float edge2x = screen_v2.pos.x - screen_v0.pos.x;
	float edge2y = screen_v2.pos.y - screen_v0.pos.y;
	float signedArea = edge1x * edge2y - edge1y * edge2x;
	
	// Cull back-facing triangles (positive signed area = back-facing with our winding)
	if (signedArea >= 0.0f) return;
	
	fillTriangle(screen_v0, screen_v1, screen_v2, texture, texWidth, texHeight);
}

void Blit(const unsigned int* source, int srcWidth, int srcHeight, unsigned int* dest, int destWidth, int destHeight, int cubeFace, float scale)
{
	// Calculate source rectangle based on cube face and texture size
	int srcX, srcY, srcW, srcH;

	switch (cubeFace)
	{
	case 0: // Front face of the cube
		srcX = 0;
		srcY = 0;
		srcW = celestial_width / 4; // Assuming each face takes 1/4th of texture width
		srcH = celestial_height / 3; // Assuming each face takes 1/3rd of texture height
		break;
	case 1: // Back face of the cube
		srcX = celestial_width / 4; // Move to the second quarter of the texture width
		srcY = 0;
		srcW = celestial_width / 4;
		srcH = celestial_height / 3;
		break;
	case 2: // Right face of the cube
		srcX = celestial_width / 2; // Move to the half of the texture width
		srcY = 0;
		srcW = celestial_width / 4;
		srcH = celestial_height / 3;
		break;
	case 3: // Left face of the cube
		srcX = 3 * celestial_width / 4; // Move to the three-quarters of the texture width
		srcY = 0;
		srcW = celestial_width / 4;
		srcH = celestial_height / 3;
		break;
		// Add cases for other cube faces as needed
	default:
		return; // Invalid cube face
	}

	int scaledW = static_cast<int>(srcW * scale);
	int scaledH = static_cast<int>(srcH * scale);

	for (int y = 0; y < scaledH; ++y)
	{
		if (y >= destHeight) break; // Prevent drawing outside the bottom edge
		for (int x = 0; x < scaledW; ++x)
		{
			if (x >= destWidth) break; // Prevent drawing outside the right edge

			// Calculate source index
			int srcIndexX = static_cast<int>(srcX + (x / scale));
			int srcIndexY = static_cast<int>(srcY + (y / scale));
			int srcIndex = srcIndexY * srcWidth + srcIndexX;

			// Calculate destination index (assuming cube face position in the destination buffer)
			int dstIndex = y * destWidth + x;

			// Perform blending calculation (similar to previous Blit function)
			unsigned srcColor = SWAP_BGRA_TO_ARGB(source[srcIndex]);
			unsigned dstColor = dest[dstIndex];

			unsigned srcAlpha = (srcColor & 0xFF000000) >> 24;
			unsigned srcRed = (srcColor & 0x00FF0000) >> 16;
			unsigned srcGreen = (srcColor & 0x0000FF00) >> 8;
			unsigned srcBlue = srcColor & 0x000000FF;

			unsigned dstAlpha = (dstColor & 0xFF000000) >> 24;
			unsigned dstRed = (dstColor & 0x00FF0000) >> 16;
			unsigned dstGreen = (dstColor & 0x0000FF00) >> 8;
			unsigned dstBlue = dstColor & 0x000000FF;

			unsigned blendedAlpha = 255 - srcAlpha;
			unsigned blendedRed = ((srcRed * srcAlpha) + (dstRed * blendedAlpha)) >> 8;
			unsigned blendedGreen = ((srcGreen * srcAlpha) + (dstGreen * blendedAlpha)) >> 8;
			unsigned blendedBlue = ((srcBlue * srcAlpha) + (dstBlue * blendedAlpha)) >> 8;

			dest[dstIndex] = (blendedAlpha << 24) | (blendedRed << 16) | (blendedGreen << 8) | blendedBlue;
		}
	}
}
