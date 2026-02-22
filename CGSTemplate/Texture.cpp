#include "Texture.h"
#include <iostream>

// Enable stb_image for file loading
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace game {

// Global bound texture
const Texture* g_BoundTexture = nullptr;

Texture::Texture() 
    : width_(0), height_(0), numberOfChannels_(0), loaded_(false) {
}

Texture::Texture(const char* path) 
    : width_(0), height_(0), numberOfChannels_(0), loaded_(false) {
    load(path);
}

Texture::Texture(const unsigned int* data, int width, int height)
    : width_(0), height_(0), numberOfChannels_(4), loaded_(false) {
    loadFromData(data, width, height);
}

Texture::~Texture() {
    // Vector cleans up automatically
    if (g_BoundTexture == this) {
        g_BoundTexture = nullptr;
    }
}

bool Texture::load(const char* path) {
    path_ = path;
    
    // Load image using stb_image
    int w, h, channels;
    unsigned char* data = stbi_load(path, &w, &h, &channels, 4);
    if (!data) {
        std::cerr << "Failed to load texture: " << path << std::endl;
        return false;
    }
    
    width_ = w;
    height_ = h;
    numberOfChannels_ = 4;
    
    // Convert to packed ARGB uint32 (0xAARRGGBB)
    pixels_.resize(w * h);
    for (int i = 0; i < w * h; i++) {
        unsigned char r = data[i * 4 + 0];
        unsigned char g = data[i * 4 + 1];
        unsigned char b = data[i * 4 + 2];
        unsigned char a = data[i * 4 + 3];
        if (a == 0) a = 255;  // Force opaque for formats without alpha (like JPG)
        pixels_[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }
    
    stbi_image_free(data);
    loaded_ = true;
    std::cout << "Loaded texture: " << path << " (" << w << "x" << h << ")\n";
    return true;
}

void Texture::loadFromData(const unsigned int* data, int width, int height) {
    if (!data || width <= 0 || height <= 0) {
        std::cerr << "Invalid texture data\n";
        return;
    }
    
    width_ = width;
    height_ = height;
    numberOfChannels_ = 4;
    
    // Copy pixel data
    pixels_.resize(width * height);
    for (int i = 0; i < width * height; i++) {
        pixels_[i] = data[i];
    }
    
    loaded_ = true;
    path_ = "[embedded]";
}

const unsigned int* Texture::getPixels() const {
    return loaded_ ? pixels_.data() : nullptr;
}

int Texture::getWidth() const {
    return width_;
}

int Texture::getHeight() const {
    return height_;
}

int Texture::getChannels() const {
    return numberOfChannels_;
}

bool Texture::isLoaded() const {
    return loaded_;
}

const std::string& Texture::getPath() const {
    return path_;
}

void Texture::bind() const {
    g_BoundTexture = this;
}

unsigned int Texture::sample(float u, float v) const {
    if (!loaded_) return 0xFF000000;  // Black if not loaded
    
    // Clamp UV coordinates
    u = (u < 0.0f) ? 0.0f : ((u > 1.0f) ? 1.0f : u);
    v = (v < 0.0f) ? 0.0f : ((v > 1.0f) ? 1.0f : v);
    
    int x = static_cast<int>(u * (width_ - 1));
    int y = static_cast<int>(v * (height_ - 1));
    
    return pixels_[y * width_ + x];
}

unsigned int Texture::sampleBGRA(float u, float v) const {
    if (!loaded_) return 0xFF000000;
    
    // Clamp UV coordinates
    u = (u < 0.0f) ? 0.0f : ((u > 1.0f) ? 1.0f : u);
    v = (v < 0.0f) ? 0.0f : ((v > 1.0f) ? 1.0f : v);
    
    int x = static_cast<int>(u * (width_ - 1));
    int y = static_cast<int>(v * (height_ - 1));
    
    return pixels_[y * width_ + x];
}

} // namespace game
