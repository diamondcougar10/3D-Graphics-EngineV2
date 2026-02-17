#pragma once
#include <string>
#include <vector>

namespace game {

// Texture class - loads and holds image data for software rendering
class Texture {
private:
    std::vector<unsigned int> pixels_;
    int width_ = 0;
    int height_ = 0;
    int numberOfChannels_ = 0;
    bool loaded_ = false;
    std::string path_;

public:
    // Default constructor
    Texture();
    
    // Load texture from file path
    Texture(const char* path);
    
    // Load texture from existing pixel data (for embedded textures like celestial.h)
    Texture(const unsigned int* data, int width, int height);
    
    // Destructor
    ~Texture();
    
    // Load from file
    bool load(const char* path);
    
    // Load from existing data
    void loadFromData(const unsigned int* data, int width, int height);
    
    // Get raw pixel data
    const unsigned int* getPixels() const;
    
    // Get width
    int getWidth() const;
    
    // Get height
    int getHeight() const;
    
    // Get number of channels
    int getChannels() const;
    
    // Check if loaded successfully
    bool isLoaded() const;
    
    // Get file path
    const std::string& getPath() const;
    
    // Bind this texture as the current texture (for software renderer)
    void bind() const;
    
    // Sample a pixel at UV coordinates (0-1 range)
    unsigned int sample(float u, float v) const;
    
    // Sample with BGRA to ARGB conversion
    unsigned int sampleBGRA(float u, float v) const;
};

// Currently bound texture (global for software renderer)
extern const Texture* g_BoundTexture;

} // namespace game
