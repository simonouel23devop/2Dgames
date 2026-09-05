#pragma once

#include "graphics/Mesh.h"

#include <filesystem>
#include <string>
#include <vector>

struct aiNode;
struct aiScene;
struct aiMesh;
struct aiMaterial;

namespace rast::graphics
{
class Model
{
public:
    explicit Model(const std::filesystem::path& file, bool flipTexturesVertically = false);

    const std::vector<Mesh>& meshes() const noexcept;
    const std::filesystem::path& sourcePath() const noexcept;

private:
    void load(const std::filesystem::path& file, bool flipTexturesVertically);
    void processNode(const aiNode* node, const aiScene* scene);
    Mesh processMesh(const aiMesh* mesh, const aiScene* scene);
    std::vector<Texture> loadMaterialTextures(const aiMaterial* material,
                                               int textureType,
                                               const std::string& semantic);

    std::vector<Mesh> meshes_;
    std::vector<Texture> loadedTextures_;
    std::filesystem::path sourcePath_;
    std::filesystem::path directory_;
};
}
