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
    
    void processNode(aiNode* node, const aiScene* scene);
    MaterialMesh* processMesh(aiMesh* mesh, const aiScene* scene);
    ModelTexture* loadTexture(const std::string& path, const aiScene* scene = nullptr);
    
public:
    Model() : Object() {}
    Model(const std::string& path) : Object() { loadModel(path); }
    virtual ~Model() = default;
    
    // Load model from file
    bool loadModel(const std::string& path);
    
    // Get model name (filename without extension)
    const std::string& getName() const { return name; }
    void setName(const std::string& n) { name = n; }
    
    // Get mesh count
    size_t getMeshCount() const { return meshes.size(); }
    const std::string getPrimaryTexturePath() const {
        return loadedTextures.empty() ? std::string("[none]") : loadedTextures.front().path;
    }
    
    // Get total triangle count
    size_t getTotalTriangles() const {
        size_t total = 0;
        for (const auto& mesh : meshes) {
            total += mesh->getTriangleCount();
        }
        return total;
    }
    
    // Override Object methods
    void render() override;
    void update(float dt) override;
};

// Implementation

inline bool Model::loadModel(const std::string& path) {
    Assimp::Importer importer;
    
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate |
        aiProcess_FlipUVs |
        aiProcess_ConvertToLeftHanded |
        aiProcess_PreTransformVertices |
        aiProcess_GenNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_JoinIdenticalVertices);
    
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "Assimp Error: " << importer.GetErrorString() << std::endl;
        return false;
    }
    
    // Extract directory and name
    std::filesystem::path fsPath(path);
    directory = fsPath.parent_path().string();
    name = fsPath.stem().string();
    
    // Clear existing data
    meshes.clear();
    loadedTextures.clear();
    
    // Process the scene graph
    processNode(scene->mRootNode, scene);
    
    std::cout << "Model loaded: " << name << " (" << meshes.size() << " meshes, " 
              << getTotalTriangles() << " triangles)" << std::endl;
    
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
        
        // Try BASE_COLOR first, then fallback to DIFFUSE
        aiTextureType texType = aiTextureType_BASE_COLOR;
        if (material->GetTextureCount(texType) == 0) {
            texType = aiTextureType_DIFFUSE;
        }

        if (material->GetTextureCount(texType) > 0) {
            aiString texPath;
            material->GetTexture(texType, 0, &texPath);
            
            std::string textureRef = texPath.C_Str();
            std::string fullPath = textureRef;
            if (!textureRef.empty() && textureRef[0] != '*') {
                fullPath = directory + "/" + textureRef;
            }

            ModelTexture* tex = loadTexture(fullPath, scene);
            
            if (tex && !tex->pixels.empty()) {
                matMesh->setTexture(tex->pixels.data(), tex->width, tex->height);
                matMesh->setUseTexture(true);
            }
        }
        
        // Get diffuse color if no texture
        aiColor3D diffuseColor(1.0f, 1.0f, 1.0f);
        material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor);
        
        matMesh->setColor(diffuseColor.r, diffuseColor.g, diffuseColor.b);
    }
    
    return matMesh;
}

inline ModelTexture* Model::loadTexture(const std::string& path, const aiScene* scene) {
    // Check if already loaded
    for (auto& tex : loadedTextures) {
        if (tex.path == path) {
            return &tex;
        }
    }
    
    // Load new texture
    ModelTexture newTex;
    newTex.path = path;
    
    int channels;
    unsigned char* data = nullptr;

    if (!path.empty() && path[0] == '*' && scene) {
        const aiTexture* embedded = scene->GetEmbeddedTexture(path.c_str());
        if (embedded) {
            if (embedded->mHeight == 0) {
                data = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(embedded->pcData),
                    static_cast<int>(embedded->mWidth), &newTex.width, &newTex.height, &channels, 4);
            } else {
                newTex.width = static_cast<int>(embedded->mWidth);
                newTex.height = static_cast<int>(embedded->mHeight);
                newTex.pixels.resize(newTex.width * newTex.height);
                for (int i = 0; i < newTex.width * newTex.height; ++i) {
                    const aiTexel& t = embedded->pcData[i];
                    newTex.pixels[i] = (static_cast<unsigned int>(t.a) << 24) |
                                       (static_cast<unsigned int>(t.r) << 16) |
                                       (static_cast<unsigned int>(t.g) << 8) |
                                       static_cast<unsigned int>(t.b);
                }
            }
        }
    } else {
        data = stbi_load(path.c_str(), &newTex.width, &newTex.height, &channels, 4);
    }
    
    if (data) {
        // Convert RGBA bytes to packed ARGB uint32 (0xAARRGGBB)
        newTex.pixels.resize(newTex.width * newTex.height);
        for (int i = 0; i < newTex.width * newTex.height; i++) {
            unsigned char r = data[i * 4 + 0];
            unsigned char g = data[i * 4 + 1];
            unsigned char b = data[i * 4 + 2];
            unsigned char a = data[i * 4 + 3];
            newTex.pixels[i] = (a << 24) | (r << 16) | (g << 8) | b;
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

inline void Model::update(float dt) {
    for (auto& mesh : meshes) {
        mesh->update(dt);
    }
}

} // namespace game
