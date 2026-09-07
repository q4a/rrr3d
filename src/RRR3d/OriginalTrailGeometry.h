#pragma once

#include "physics/OriginalVehiclePhysics.h"
#include "renderer/Renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

namespace rrr3d::race
{

struct TrailSample
{
    r3d::physics::Vec3 position;
    float frame = 0.0F;
    bool startsStrip = false;
};

// eff9338 FxTrailManager::RenderEmitter/BuildVertexLine/DrawPath. Each
// source particle group owns the colour of its outgoing segment. Duplicate
// the shared edge so interpolation cannot blend the colours of two groups.
// Separate MapObj generations have no indices connecting their strips.
template <typename ColorAtFrame>
void buildTrailGeometry(
    std::span<const TrailSample> samples, float width,
    r3d::physics::Vec3 up, bool fixedUp,
    r3d::physics::Vec3 camera, ColorAtFrame colorAtFrame,
    std::vector<r3d::renderer::StaticMeshVertex>& vertices,
    std::vector<std::uint32_t>& indices)
{
    using r3d::physics::Vec3;
    using r3d::renderer::StaticMeshVertex;
    const auto subtract = [](Vec3 a, Vec3 b) {
        return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
    };
    const auto normalized = [](Vec3 value, Vec3 fallback) {
        const float length = std::sqrt(
            value.x * value.x + value.y * value.y + value.z * value.z);
        return length > 0.00001F && std::isfinite(length)
            ? Vec3{value.x / length, value.y / length, value.z / length}
            : fallback;
    };
    const auto cross = [](Vec3 a, Vec3 b) {
        return Vec3{a.y * b.z - a.z * b.y,
                    a.z * b.x - a.x * b.z,
                    a.x * b.y - a.y * b.x};
    };
    const auto packColor = [](const std::array<float, 4U>& color) {
        std::uint32_t packed = 0U;
        for (std::uint32_t channel = 0U; channel < 4U; ++channel)
            packed |= static_cast<std::uint32_t>(std::lround(
                std::clamp(color[channel], 0.0F, 1.0F) * 255.0F))
                << (8U * channel);
        return packed;
    };
    vertices.clear();
    indices.clear();
    vertices.reserve(samples.size() * 4U);
    indices.reserve(samples.size() * 6U);
    up = normalized(up, {0.0F, 0.0F, 1.0F});
    std::array<StaticMeshVertex, 2U> previousEdge{};
    Vec3 direction{1.0F, 0.0F, 0.0F};
    Vec3 lastPosition{};
    std::size_t stripPoint = 0U;
    for (std::size_t point = 0U; point < samples.size(); ++point)
    {
        const auto& sample = samples[point];
        const bool first = point == 0U || sample.startsStrip;
        const bool terminal = point + 1U == samples.size() ||
                              samples[point + 1U].startsStrip;
        if (first)
        {
            stripPoint = 0U;
            direction = terminal ? Vec3{1.0F, 0.0F, 0.0F}
                : normalized(subtract(samples[point + 1U].position,
                                      sample.position),
                             {1.0F, 0.0F, 0.0F});
            lastPosition = subtract(sample.position, direction);
        }
        // The final edge is the current system position, handled after
        // the particle loop in Windows, with its own newest direction.
        if (terminal && !first)
            direction = normalized(subtract(sample.position, lastPosition),
                                   direction);
        const Vec3 side = normalized(
            fixedUp ? cross(up, direction)
                    : cross(direction, normalized(
                          subtract(sample.position, camera), up)),
            {0.0F, 1.0F, 0.0F});
        const float u = static_cast<float>(stripPoint % 2U);
        std::array<StaticMeshVertex, 2U> edge{};
        for (std::size_t corner = 0U; corner < edge.size(); ++corner)
        {
            const float sign = corner == 0U ? 1.0F : -1.0F;
            edge[corner] = {
                sample.position.x + sign * side.x * width,
                sample.position.y + sign * side.y * width,
                sample.position.z + sign * side.z * width,
                up.x, up.y, up.z, u, static_cast<float>(corner),
                direction.x, direction.y, direction.z,
                side.x, side.y, side.z};
        }
        if (!first)
        {
            const auto color = packColor(colorAtFrame(samples[point - 1U].frame));
            const auto base = static_cast<std::uint32_t>(vertices.size());
            for (auto vertex : {previousEdge[0U], previousEdge[1U],
                                edge[0U], edge[1U]})
            {
                vertex.color = color;
                vertices.push_back(vertex);
            }
            for (const auto index : {0U, 1U, 2U, 1U, 3U, 2U})
                indices.push_back(base + index);
        }
        previousEdge = edge;
        direction = normalized(subtract(sample.position, lastPosition),
                               direction);
        lastPosition = sample.position;
        ++stripPoint;
    }
}

} // namespace rrr3d::race
