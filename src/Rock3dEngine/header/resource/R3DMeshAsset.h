#pragma once

#include "resource/ResourceFileSystem.h"

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

namespace r3d::resource
{

struct R3DVertex
{
    std::array<float, 3> position{};
    std::array<float, 3> normal{};
    std::array<float, 2> texcoord{};
};

struct R3DMaterialGroup
{
    std::int32_t materialId = 0;
    std::uint32_t firstIndex = 0;
    std::uint32_t indexCount = 0;
};

struct R3DMeshAsset
{
    std::int32_t version = 0;
    bool leftCoordinateSystem = false;
    bool hasTexcoords = false;
    std::vector<R3DVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<R3DMaterialGroup> materialGroups;
    std::array<float, 3> minimum{};
    std::array<float, 3> maximum{};
};

// Reads the shipped Motor Rock mesh format without translating it through
// D3D9 types.  The same decoded data is consumed by the legacy D3D9 resource
// adapter and the bgfx backend.
R3DMeshAsset loadR3DMeshAsset(const ResourceFileSystem& resources,
                              std::string_view virtualPath);
R3DMeshAsset decodeR3DMeshAsset(const std::vector<std::uint8_t>& bytes,
                                std::string_view resourceName);

} // namespace r3d::resource
