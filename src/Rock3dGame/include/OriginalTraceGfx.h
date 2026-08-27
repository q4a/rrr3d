#pragma once

#include "OriginalTrace.h"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace r3d::game::originalrace::source
{

struct TraceGfxColor
{
    float red = 1.0F;
    float green = 1.0F;
    float blue = 1.0F;
    float alpha = 1.0F;
};

struct TraceGfxMaterialPolicy
{
    bool transparency = true;
    bool lighting = false;
    bool depthWrite = false;
    bool depthTest = false;
    bool ignoreFog = true;
    bool cull = false;
    float alpha = 0.5F;
};

enum class TraceGfxPrimitive
{
    Box,
    Path,
    SelectedTile,
    PointLink,
};

struct TraceGfxDrawRecord
{
    TraceGfxPrimitive primitive = TraceGfxPrimitive::Path;
    TraceGfxColor color;
    std::vector<TraceVec3> vertices;
    std::vector<std::uint32_t> indices;
    TraceVec3 linkStart;
    TraceVec3 linkEnd;
    float linkWidth = 0.0F;
};

// Backend-neutral transcription of TraceGfx. It owns selection/link state
// and emits source geometry/material records; bgfx only uploads transient
// triangles and applies the returned render policy.
class TraceGfx
{
public:
    class PointLink
    {
    public:
        explicit PointLink(const WayPoint* point = nullptr) noexcept;

        const WayPoint* GetPoint() const noexcept;
        const TraceVec3& GetPos() const noexcept;
        void SetPos(const TraceVec3& value) noexcept;

    private:
        const WayPoint* point_ = nullptr;
        TraceVec3 position_;
    };

    explicit TraceGfx(const Trace* trace) noexcept;

    const WayPoint* GetSelPoint() const noexcept;
    void SetSelPoint(const WayPoint* value) noexcept;
    const WayPath* GetSelPath() const noexcept;
    void SetSelPath(const WayPath* value) noexcept;
    const WayNode* GetSelNode() const noexcept;
    void SetSelNode(const WayNode* value) noexcept;
    const PointLink* GetPointLink() const noexcept;
    void SetPointLink(const PointLink* value) noexcept;

    const TraceGfxMaterialPolicy& GetMaterialPolicy() const noexcept;
    std::vector<TraceGfxDrawRecord> BuildDrawList() const;

private:
    bool Contains(const WayPoint* value) const noexcept;
    bool Contains(const WayPath* value) const noexcept;
    bool Contains(const WayNode* value) const noexcept;

    const Trace* trace_ = nullptr;
    const WayPoint* selectedPoint_ = nullptr;
    const WayPath* selectedPath_ = nullptr;
    const WayNode* selectedNode_ = nullptr;
    std::optional<PointLink> pointLink_;
    TraceGfxMaterialPolicy material_;
};

} // namespace r3d::game::originalrace::source
