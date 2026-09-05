#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace rast::graphics
{
struct Vertex
{
    float position[3] = {};
    float normal[3] = {};
    float textureCoordinates[2] = {};
};

struct Texture
{
    std::string path;
    std::string type;
};

class Mesh
{
public:
    Mesh() = default;
    Mesh(std::vector<Vertex> vertices,
         std::vector<std::uint32_t> indices,
         std::vector<Texture> textures);

    const std::vector<Vertex>& vertices() const noexcept;
    const std::vector<std::uint32_t>& indices() const noexcept;
    const std::vector<Texture>& textures() const noexcept;

private:
    std::vector<Vertex> vertices_;
    std::vector<std::uint32_t> indices_;
    std::vector<Texture> textures_;
};
}
