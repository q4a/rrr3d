#include "OriginalTraceGfx.h"

#include <algorithm>
#include <cmath>

namespace r3d::game::originalrace::source
{
namespace
{

constexpr TraceGfxColor red{1.0F, 0.0F, 0.0F, 0.5F};
constexpr TraceGfxColor green{0.0F, 1.0F, 0.0F, 0.5F};

TraceVec3 add(TraceVec3 first, TraceVec3 second) noexcept
{
    return {first.x + second.x, first.y + second.y,
            first.z + second.z};
}

TraceVec3 subtract(TraceVec3 first, TraceVec3 second) noexcept
{
    return {first.x - second.x, first.y - second.y,
            first.z - second.z};
}

std::array<TraceVec3, 4> tileVertices(const WayNode& node)
{
    const auto* next = node.GetNext();
    if (next == nullptr)
        return {};
    const auto middleNormal = node.GetTile().GetMidNorm();
    const auto nextMiddleNormal = next->GetTile().GetMidNorm();
    const float radius = node.GetTile().GetNodeRadius();
    const float nextRadius = next->GetTile().GetNodeRadius();
    const TraceVec3 firstOffset{
        middleNormal.x * radius, middleNormal.y * radius, 0.0F};
    const TraceVec3 secondOffset{
        nextMiddleNormal.x * nextRadius,
        nextMiddleNormal.y * nextRadius, 0.0F};
    return {
        add(node.GetPos(), firstOffset),
        subtract(node.GetPos(), firstOffset),
        add(next->GetPos(), secondOffset),
        subtract(next->GetPos(), secondOffset)};
}

void appendQuad(TraceGfxDrawRecord& record,
                const std::array<TraceVec3, 4>& vertices)
{
    const auto base = static_cast<std::uint32_t>(record.vertices.size());
    record.vertices.insert(
        record.vertices.end(), vertices.begin(), vertices.end());
    record.indices.insert(
        record.indices.end(),
        {base, base + 1U, base + 2U,
         base + 1U, base + 3U, base + 2U});
}

TraceGfxDrawRecord pointBox(const WayPoint& point, bool selected)
{
    TraceGfxDrawRecord result;
    result.primitive = TraceGfxPrimitive::Box;
    result.color = selected ? green : red;
    const float half = point.GetSize() * 0.5F;
    const auto& center = point.GetPos();
    result.vertices = {
        {center.x - half, center.y - half, center.z - half},
        {center.x + half, center.y - half, center.z - half},
        {center.x + half, center.y + half, center.z - half},
        {center.x - half, center.y + half, center.z - half},
        {center.x - half, center.y - half, center.z + half},
        {center.x + half, center.y - half, center.z + half},
        {center.x + half, center.y + half, center.z + half},
        {center.x - half, center.y + half, center.z + half}};
    result.indices = {
        0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7,
        0, 1, 5, 0, 5, 4, 1, 2, 6, 1, 6, 5,
        2, 3, 7, 2, 7, 6, 3, 0, 4, 3, 4, 7};
    return result;
}

TraceGfxColor pathColor(std::size_t index, std::size_t count,
                        bool selected) noexcept
{
    if (selected)
        return green;
    const float amount = count > 1U
                             ? static_cast<float>(index) /
                                   static_cast<float>(count - 1U)
                             : 0.0F;
    const float value = 0.5F + amount * 0.5F;
    return {value, value, value, 0.5F};
}

} // namespace

TraceGfx::PointLink::PointLink(const WayPoint* point) noexcept
    : point_(point)
{
}

const WayPoint* TraceGfx::PointLink::GetPoint() const noexcept
{
    return point_;
}

const TraceVec3& TraceGfx::PointLink::GetPos() const noexcept
{
    return position_;
}

void TraceGfx::PointLink::SetPos(const TraceVec3& value) noexcept
{
    position_ = value;
}

TraceGfx::TraceGfx(const Trace* trace) noexcept : trace_(trace) {}

bool TraceGfx::Contains(const WayPoint* value) const noexcept
{
    return trace_ != nullptr && value != nullptr &&
           std::any_of(
               trace_->GetPoints().begin(), trace_->GetPoints().end(),
               [value](const auto& point) { return point.get() == value; });
}

bool TraceGfx::Contains(const WayPath* value) const noexcept
{
    return trace_ != nullptr && value != nullptr &&
           std::any_of(
               trace_->GetPaths().begin(), trace_->GetPaths().end(),
               [value](const auto& path) { return path.get() == value; });
}

bool TraceGfx::Contains(const WayNode* value) const noexcept
{
    return trace_ != nullptr && value != nullptr &&
           trace_->GetNodeRef(value).valid();
}

const WayPoint* TraceGfx::GetSelPoint() const noexcept
{
    return Contains(selectedPoint_) ? selectedPoint_ : nullptr;
}

void TraceGfx::SetSelPoint(const WayPoint* value) noexcept
{
    selectedPoint_ = Contains(value) ? value : nullptr;
}

const WayPath* TraceGfx::GetSelPath() const noexcept
{
    return Contains(selectedPath_) ? selectedPath_ : nullptr;
}

void TraceGfx::SetSelPath(const WayPath* value) noexcept
{
    selectedPath_ = Contains(value) ? value : nullptr;
}

const WayNode* TraceGfx::GetSelNode() const noexcept
{
    return Contains(selectedNode_) ? selectedNode_ : nullptr;
}

void TraceGfx::SetSelNode(const WayNode* value) noexcept
{
    selectedNode_ = Contains(value) ? value : nullptr;
}

const TraceGfx::PointLink* TraceGfx::GetPointLink() const noexcept
{
    return pointLink_.has_value() &&
                   Contains(pointLink_->GetPoint())
               ? &*pointLink_
               : nullptr;
}

void TraceGfx::SetPointLink(const PointLink* value) noexcept
{
    if (value != nullptr && Contains(value->GetPoint()))
        pointLink_ = *value;
    else
        pointLink_.reset();
}

const TraceGfxMaterialPolicy& TraceGfx::GetMaterialPolicy() const noexcept
{
    return material_;
}

std::vector<TraceGfxDrawRecord> TraceGfx::BuildDrawList() const
{
    std::vector<TraceGfxDrawRecord> result;
    if (trace_ == nullptr)
        return result;
    result.reserve(
        trace_->GetPoints().size() + trace_->GetPaths().size() + 2U);

    const auto* selectedPoint = GetSelPoint();
    for (const auto& point : trace_->GetPoints())
        result.push_back(pointBox(*point, point.get() == selectedPoint));

    const auto* selectedPath = GetSelPath();
    for (std::size_t index = 0U;
         index < trace_->GetPaths().size(); ++index)
    {
        const auto& path = *trace_->GetPaths()[index];
        TraceGfxDrawRecord record;
        record.primitive = TraceGfxPrimitive::Path;
        record.color = pathColor(
            index, trace_->GetPaths().size(), &path == selectedPath);
        const std::size_t segmentCount =
            path.GetCount() > 1U
                ? path.GetCount() - 1U +
                      (path.IsEnclosed() ? 1U : 0U)
                : 0U;
        for (std::size_t segment = 0U;
             segment < segmentCount; ++segment)
        {
            const auto* node = path.GetNode(segment);
            if (node != nullptr && node->GetNext() != nullptr)
                appendQuad(record, tileVertices(*node));
        }
        if (!record.indices.empty())
            result.push_back(std::move(record));
    }

    if (const auto* selectedNode = GetSelNode();
        selectedNode != nullptr && selectedNode->GetNext() != nullptr)
    {
        TraceGfxDrawRecord record;
        record.primitive = TraceGfxPrimitive::SelectedTile;
        record.color = green;
        appendQuad(record, tileVertices(*selectedNode));
        result.push_back(std::move(record));
    }

    if (const auto* link = GetPointLink())
    {
        TraceGfxDrawRecord record;
        record.primitive = TraceGfxPrimitive::PointLink;
        record.color = green;
        record.linkStart = link->GetPoint()->GetPos();
        record.linkEnd = link->GetPos();
        record.linkWidth = link->GetPoint()->GetSize();
        result.push_back(std::move(record));
    }
    return result;
}

} // namespace r3d::game::originalrace::source
