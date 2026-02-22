#pragma once

// Assimp includes
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "MaterialMesh.h"
#include "stb_image.h"
#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <filesystem>
#include <algorithm>

namespace game {

// Simple texture holder
struct ModelTexture {
    std::vector<unsigned int> pixels;
    int width = 0;
    int height = 0;
    std::string path;
};

// Model class - loads and manages 3D models via Assimp
class Model : public Object {
private:
    std::vector<std::unique_ptr<MaterialMesh>> meshes;
    std::vector<ModelTexture> loadedTextures;
    std::string directory;
    std::string name;
    std::string fileExtension;
    bool useTextures = true;  // Can disable for performance
    int texturesLoaded = 0;   // Count of successfully loaded textures
    int texturesFailed = 0;   // Count of failed texture loads
    
    void processNode(aiNode* node, const aiScene* scene);
    MaterialMesh* processMesh(aiMesh* mesh, const aiScene* scene);
    ModelTexture* loadTexture(const std::string& path);
    
public:
    Model() : Object() {}
    Model(const std::string& path) : Object() { loadModel(path); }
    virtual ~Model() = default;
    
    // Load model from file
    bool loadModel(const std::string& path);
    
    // Get model name (filename without extension)
    const std::string& getName() const { return name; }
    void setName(const std::string& n) { name = n; }
    
    // Get file extension (for auto-orientation)
    const std::string& getFileExtension() const { return fileExtension; }
    
    // Check if model needs orientation fix (Z-up to Y-up conversion)
    bool needsOrientationFix() const {
        // FBX, Blender exports, and some other formats use Z-up
        return fileExtension == ".fbx" || fileExtension == ".blend" || 
               fileExtension == ".dae" || fileExtension == ".3ds";
    }
    
    // Get texture loading status
    int getTexturesLoaded() const { return texturesLoaded; }
    int getTexturesFailed() const { return texturesFailed; }
    
    // Get mesh count
    size_t getMeshCount() const { return meshes.size(); }
    
    // Get total triangle count
    size_t getTotalTriangles() const {
        size_t total = 0;
        for (const auto& mesh : meshes) {
            total += mesh->getTriangleCount();
        }
        return total;
    }
    
    // Texture toggle (disable for better performance)
    void setUseTextures(bool use) { 
        useTextures = use;
        for (auto& mesh : meshes) {
            mesh->setUseTexture(use);
        }
    }
    bool getUseTextures() const { return useTextures; }
    
    // Override Object methods
    void render() override;
    void update(float dt) override;
    
    // Render with triangle budget - returns number of triangles rendered
    // Pass remaining budget, returns actual triangles drawn
    int renderWithBudget(int remainingBudget);
};

// Implementation

inline bool Model::loadModel(const std::string& path) {
    Assimp::Importer importer;
    
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate |
        aiProcess_FlipUVs |
        aiProcess_GenNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_JoinIdenticalVertices);
    
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "Assimp Error: " << importer.GetErrorString() << std::endl;
        return false;
    }
    
    // Extract directory, name, and extension
    std::filesystem::path fsPath(path);
    directory = fsPath.parent_path().string();
    name = fsPath.stem().string();
    fileExtension = fsPath.extension().string();
    std::transform(fileExtension.begin(), fileExtension.end(), fileExtension.begin(), ::tolower);
    
    // Clear existing data
    meshes.clear();
    loadedTextures.clear();
    texturesLoaded = 0;
    texturesFailed = 0;
    
    // Process the scene graph
    processNode(scene->mRootNode, scene);
    
    std::cout << "Model loaded: " << name << " (" << meshes.size() << " meshes, " 
              << getTotalTriangles() << " triangles)" << std::endl;
    
    // Note: Model is loaded at origin - user can rotate via UI if needed
    // FBX files often need X-90 rotation to convert from Z-up to Y-up
    
    return true;
}

inline void Model::processNode(aiNode* node, const aiScene* scene) {
    // Process all meshes in this node
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        MaterialMesh* processed = processMesh(mesh, scene);
        if (processed) {
            meshes.emplace_back(processed);
        }
    }
    
    // Recursively process children
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        processNode(node->mChildren[i], scene);
    }
}

inline MaterialMesh* Model::processMesh(aiMesh* mesh, const aiScene* scene) {
    std::vector<vertex> vertices;
    std::vector<unsigned int> indices;
    
    vertices.reserve(mesh->mNumVertices);
    
    // Process vertices
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        vertex v;
        
        // Position
        v.pos.x = mesh->mVertices[i].x;
        v.pos.y = mesh->mVertices[i].y;
        v.pos.z = mesh->mVertices[i].z;
        v.pos.w = 1.0f;
        
        // Default color (white)
        v.color = 0xFFFFFFFF;
        
        // Texture coordinates (first channel if available)
        if (mesh->mTextureCoords[0]) {
            v.u = mesh->mTextureCoords[0][i].x;
            v.v = mesh->mTextureCoords[0][i].y;
        } else {
            v.u = 0.0f;
            v.v = 0.0f;
        }
        
        vertices.push_back(v);
    }
    
    // Process faces (indices)
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace& face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            indices.push_back(face.mIndices[j]);
        }
    }
    
    // Create the material mesh
    MaterialMesh* matMesh = new MaterialMesh(vertices, indices);
    
    // Process material / textures
    if (mesh->mMaterialIndex >= 0) {
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        
        // Try to load diffuse texture
        if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
            aiString texPath;
            material->GetTexture(aiTextureType_DIFFUSE, 0, &texPath);
            
            std::string fullPath = directory + "/" + texPath.C_Str();
            ModelTexture* tex = loadTexture(fullPath);
            
            if (tex && !tex->pixels.empty()) {
                matMesh->setTexture(tex->pixels.data(), tex->width, tex->height);
                matMesh->setUseTexture(true);
                texturesLoaded++;
            } else {
                texturesFailed++;
            }
        }
        
        // Get diffuse color if no texture
        aiColor3D diffuseColor(1.0f, 1.0f, 1.0f);
        material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor);
        
        matMesh->setColor(diffuseColor.r, diffuseColor.g, diffuseColor.b);
    }
    
    return matMesh;
}

inline ModelTexture* Model::loadTexture(const std::string& path) {
    // Check if already loaded
    for (auto& tex : loadedTextures) {
        if (tex.path == path) {
            return &tex;
        }
    }
    
    // Load new texture - try multiple path variations
    ModelTexture newTex;
    newTex.path = path;
    
    int channels;
    unsigned char* data = nullptr;
    
    // List of paths to try - comprehensive search
    std::vector<std::string> pathsToTry;
    pathsToTry.push_back(path);  // Original path
    
    // Extract just the filename
    std::filesystem::path fsPath(path);
    std::string filename = fsPath.filename().string();
    std::string filenameNoExt = fsPath.stem().string();
    
    // Common texture extensions to try
    std::vector<std::string> extensions = {"", ".png", ".jpg", ".jpeg", ".tga", ".bmp"};
    
    // Try various directory structures
    std::filesystem::path modelDir(directory);
    
    for (const auto& ext : extensions) {
        std::string tryName = (ext.empty()) ? filename : (filenameNoExt + ext);
        
        // Same folder as model
        pathsToTry.push_back((modelDir / tryName).string());
        
        // textures subfolder
        pathsToTry.push_back((modelDir / "textures" / tryName).string());
        pathsToTry.push_back((modelDir / "Textures" / tryName).string());
        pathsToTry.push_back((modelDir / "texture" / tryName).string());
        
        // Parent's textures folder (model in "models" folder, textures in "textures" folder)
        pathsToTry.push_back((modelDir.parent_path() / "textures" / tryName).string());
        pathsToTry.push_back((modelDir.parent_path() / "Textures" / tryName).string());
        
        // materials folder
        pathsToTry.push_back((modelDir / "materials" / tryName).string());
        pathsToTry.push_back((modelDir.parent_path() / "materials" / tryName).string());
        
        // Current working directory
        pathsToTry.push_back(tryName);
        pathsToTry.push_back("textures/" + tryName);
        pathsToTry.push_back("assets/" + tryName);
    }
    
    for (const auto& tryPath : pathsToTry) {
        data = stbi_load(tryPath.c_str(), &newTex.width, &newTex.height, &channels, 4);
        if (data) {
            std::cout << "Found texture at: " << tryPath << std::endl;
            break;
        }
    }
    
    if (data) {
        // Convert RGBA to BGRA
        newTex.pixels.resize(newTex.width * newTex.height);
        for (int i = 0; i < newTex.width * newTex.height; i++) {
            unsigned char r = data[i * 4 + 0];
            unsigned char g = data[i * 4 + 1];
            unsigned char b = data[i * 4 + 2];
            unsigned char a = data[i * 4 + 3];
            newTex.pixels[i] = (a << 24) | (r << 16) | (g << 8) | b;  // ARGB/BGRA
        }
        stbi_image_free(data);
        
        std::cout << "Loaded texture: " << path << " (" << newTex.width << "x" << newTex.height << ")" << std::endl;
    } else {
        std::cerr << "Failed to load texture: " << path << std::endl;
        return nullptr;
    }
    
    loadedTextures.push_back(std::move(newTex));
    return &loadedTextures.back();
}

inline void Model::render() {
    if (!visible) return;
    
    // Apply model transform to all child meshes
    matrix4x4 modelMatrix = getWorldMatrix();
    
    for (auto& mesh : meshes) {
        // Combine this model's transform with mesh transform
        matrix4x4 originalWorld = SV_WorldMatrix;
        matrix4x4 meshWorld = mesh->getWorldMatrix();
        SV_WorldMatrix = matrixMultiplicationMatrix(modelMatrix, meshWorld);
        
        mesh->render();
        
        SV_WorldMatrix = originalWorld;
    }
}

inline int Model::renderWithBudget(int remainingBudget) {
    if (!visible) return 0;
    
    int trianglesRendered = 0;
    matrix4x4 modelMatrix = getWorldMatrix();
    
    for (auto& mesh : meshes) {
        int meshTriangles = (int)mesh->getTriangleCount();
        
        // Skip this mesh if it would exceed budget
        if (trianglesRendered + meshTriangles > remainingBudget) {
            continue;  // Skip this mesh, try next (smaller meshes might fit)
        }
        
        // Combine this model's transform with mesh transform
        matrix4x4 originalWorld = SV_WorldMatrix;
        matrix4x4 meshWorld = mesh->getWorldMatrix();
        SV_WorldMatrix = matrixMultiplicationMatrix(modelMatrix, meshWorld);
        
        mesh->render();
        trianglesRendered += meshTriangles;
        
        SV_WorldMatrix = originalWorld;
    }
    
    return trianglesRendered;
}

inline void Model::update(float dt) {
    for (auto& mesh : meshes) {
        mesh->update(dt);
    }
}

} // namespace game
