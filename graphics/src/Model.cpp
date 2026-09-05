#include "graphics/Model.h"

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <stdexcept>
#include <utility>

namespace rast::graphics
{
Mesh::Mesh(std::vector<Vertex> vertices,
           std::vector<std::uint32_t> indices,
           std::vector<Texture> textures)
    : vertices_(std::move(vertices)), indices_(std::move(indices)), textures_(std::move(textures))
{
}

const std::vector<Vertex>& Mesh::vertices() const noexcept
{
    return vertices_;
}

const std::vector<std::uint32_t>& Mesh::indices() const noexcept
{
    return indices_;
}

const std::vector<Texture>& Mesh::textures() const noexcept
{
    return textures_;
}

Model::Model(const std::filesystem::path& file, bool flipTexturesVertically)
{
    load(file, flipTexturesVertically);
}

const std::vector<Mesh>& Model::meshes() const noexcept
{
    return meshes_;
}

const std::filesystem::path& Model::sourcePath() const noexcept
{
    return sourcePath_;
}

void Model::load(const std::filesystem::path& file, bool flipTexturesVertically)
{
    Assimp::Importer importer;
    unsigned int flags = aiProcess_Triangulate | aiProcess_GenSmoothNormals |
                         aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality;
    if (flipTexturesVertically)
        flags |= aiProcess_FlipUVs;

    const aiScene* scene = importer.ReadFile(file.string(), flags);
    if (scene == nullptr || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0 || scene->mRootNode == nullptr)
        throw std::runtime_error("Assimp could not load model '" + file.string() + "': " + importer.GetErrorString());

    sourcePath_ = file;
    directory_ = file.parent_path();
    meshes_.clear();
    loadedTextures_.clear();
    processNode(scene->mRootNode, scene);
}

void Model::processNode(const aiNode* node, const aiScene* scene)
{
    for (unsigned int index = 0; index < node->mNumMeshes; ++index)
        meshes_.push_back(processMesh(scene->mMeshes[node->mMeshes[index]], scene));

    for (unsigned int index = 0; index < node->mNumChildren; ++index)
        processNode(node->mChildren[index], scene);
}

Mesh Model::processMesh(const aiMesh* mesh, const aiScene* scene)
{
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    vertices.reserve(mesh->mNumVertices);

    for (unsigned int index = 0; index < mesh->mNumVertices; ++index)
    {
        Vertex vertex;
        vertex.position[0] = mesh->mVertices[index].x;
        vertex.position[1] = mesh->mVertices[index].y;
        vertex.position[2] = mesh->mVertices[index].z;
        if (mesh->HasNormals())
        {
            vertex.normal[0] = mesh->mNormals[index].x;
            vertex.normal[1] = mesh->mNormals[index].y;
            vertex.normal[2] = mesh->mNormals[index].z;
        }
        if (mesh->mTextureCoords[0] != nullptr)
        {
            vertex.textureCoordinates[0] = mesh->mTextureCoords[0][index].x;
            vertex.textureCoordinates[1] = mesh->mTextureCoords[0][index].y;
        }
        vertices.push_back(vertex);
    }

    for (unsigned int faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex)
    {
        const aiFace& face = mesh->mFaces[faceIndex];
        for (unsigned int index : face.mIndices)
            indices.push_back(index);
    }

    std::vector<Texture> textures;
    if (mesh->mMaterialIndex < scene->mNumMaterials)
    {
        const aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        const auto diffuse = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse");
        const auto specular = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
        const auto normal = loadMaterialTextures(material, aiTextureType_NORMALS, "texture_normal");
        textures.insert(textures.end(), diffuse.begin(), diffuse.end());
        textures.insert(textures.end(), specular.begin(), specular.end());
        textures.insert(textures.end(), normal.begin(), normal.end());
    }
    return Mesh(std::move(vertices), std::move(indices), std::move(textures));
}

std::vector<Texture> Model::loadMaterialTextures(const aiMaterial* material,
                                                  int textureType,
                                                  const std::string& semantic)
{
    std::vector<Texture> textures;
    const auto type = static_cast<aiTextureType>(textureType);
    for (unsigned int index = 0; index < material->GetTextureCount(type); ++index)
    {
        aiString texturePath;
        if (material->GetTexture(type, index, &texturePath) != AI_SUCCESS)
            continue;

        const std::string relativePath = texturePath.C_Str();
        bool alreadyLoaded = false;
        for (const Texture& loaded : loadedTextures_)
        {
            if (loaded.path == relativePath && loaded.type == semantic)
            {
                textures.push_back(loaded);
                alreadyLoaded = true;
                break;
            }
        }
        if (alreadyLoaded)
            continue;

        Texture texture;
        texture.path = (directory_ / relativePath).lexically_normal().string();
        texture.type = semantic;
        textures.push_back(texture);
        loadedTextures_.push_back(texture);
    }
    return textures;
}
}
