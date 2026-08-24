#include "OriginalTrace.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace r3d::game::originalrace::source
{
namespace
{

constexpr float epsilon = 0.0001F;

TraceVec2 add(TraceVec2 first, TraceVec2 second) noexcept
{
    return {first.x + second.x, first.y + second.y};
}

TraceVec2 subtract(TraceVec2 first, TraceVec2 second) noexcept
{
    return {first.x - second.x, first.y - second.y};
}

TraceVec2 multiply(TraceVec2 value, float scale) noexcept
{
    return {value.x * scale, value.y * scale};
}

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

TraceVec3 multiply(TraceVec3 value, float scale) noexcept
{
    return {value.x * scale, value.y * scale, value.z * scale};
}

float dot(TraceVec2 first, TraceVec2 second) noexcept
{
    return first.x * second.x + first.y * second.y;
}

float dot(TraceVec3 first, TraceVec3 second) noexcept
{
    return first.x * second.x + first.y * second.y + first.z * second.z;
}

float cross(TraceVec2 first, TraceVec2 second) noexcept
{
    return first.x * second.y - first.y * second.x;
}

TraceVec3 cross(TraceVec3 first, TraceVec3 second) noexcept
{
    return {first.y * second.z - first.z * second.y,
            first.z * second.x - first.x * second.z,
            first.x * second.y - first.y * second.x};
}

float length(TraceVec2 value) noexcept
{
    return std::sqrt(dot(value, value));
}

float length(TraceVec3 value) noexcept
{
    return std::sqrt(dot(value, value));
}

TraceVec2 normalize(TraceVec2 value) noexcept
{
    const float magnitude = length(value);
    return magnitude > epsilon ? multiply(value, 1.0F / magnitude)
                               : TraceVec2{};
}

TraceVec3 normalize(TraceVec3 value) noexcept
{
    const float magnitude = length(value);
    return magnitude > epsilon ? multiply(value, 1.0F / magnitude)
                               : TraceVec3{};
}

TraceVec2 clockwiseNormal(TraceVec2 value) noexcept
{
    return {value.y, -value.x};
}

TraceVec2 counterClockwiseNormal(TraceVec2 value) noexcept
{
    return {-value.y, value.x};
}

TraceVec2 xy(TraceVec3 value) noexcept
{
    return {value.x, value.y};
}

float lineDistance(TraceVec2 normal, TraceVec2 linePoint,
                   TraceVec2 point) noexcept
{
    return dot(normal, subtract(point, linePoint));
}

bool raySphere(const TraceVec3& rayPos, const TraceVec3& rayVec,
               const TraceVec3& center, float radius,
               float* distance) noexcept
{
    const TraceVec3 offset = subtract(rayPos, center);
    const float a = dot(rayVec, rayVec);
    if (a <= epsilon)
        return false;
    const float b = 2.0F * dot(offset, rayVec);
    const float c = dot(offset, offset) - radius * radius;
    const float discriminant = b * b - 4.0F * a * c;
    if (discriminant < 0.0F)
        return false;
    const float root = std::sqrt(discriminant);
    float hit = (-b - root) / (2.0F * a);
    if (hit < 0.0F)
        hit = (-b + root) / (2.0F * a);
    if (hit < 0.0F)
        return false;
    if (distance != nullptr)
        *distance = hit;
    return true;
}

} // namespace

WayPoint::WayPoint(Trace* trace, std::uint32_t id) noexcept
    : trace_(trace), id_(id)
{
}

void WayPoint::InsertNode(WayNode* node)
{
    nodes_.push_back(node);
}

void WayPoint::RemoveNode(WayNode* node) noexcept
{
    std::erase(nodes_, node);
}

void WayPoint::Changed() noexcept
{
    for (auto* node : nodes_)
        node->Changed();
}

bool WayPoint::RayCast(const TraceVec3& rayPos,
                       const TraceVec3& rayVec,
                       float* distance) const
{
    return raySphere(rayPos, rayVec, position_, size_ * 0.5F,
                     distance);
}

bool WayPoint::IsContains(const TraceVec3& point,
                          float* distance) const
{
    const float centerDistance = length(subtract(point, position_));
    if (distance != nullptr)
        *distance = centerDistance;
    return centerDistance < size_ * 0.5F;
}

std::uint32_t WayPoint::GetId() const noexcept { return id_; }
const TraceVec3& WayPoint::GetPos() const noexcept { return position_; }

void WayPoint::SetPos(const TraceVec3& value) noexcept
{
    position_ = value;
    Changed();
}

float WayPoint::GetSize() const noexcept { return size_; }

void WayPoint::SetSize(float value) noexcept
{
    size_ = value;
    Changed();
}

const TraceVec3& WayPoint::GetOff() const noexcept { return offset_; }

void WayPoint::SetOff(const TraceVec3& value) noexcept
{
    offset_ = value;
    Changed();
}

bool WayPoint::IsFind(const WayPath* path) const noexcept
{
    return std::any_of(nodes_.begin(), nodes_.end(),
                       [path](const WayNode* node) {
                           return node->GetPath() == path;
                       });
}

bool WayPoint::IsFind(const WayNode* node,
                      const WayNode* ignore) const noexcept
{
    return std::any_of(nodes_.begin(), nodes_.end(),
                       [node, ignore](const WayNode* candidate) {
                           return candidate != ignore && candidate == node;
                       });
}

WayNode* WayPoint::GetRandomNode(WayNode* ignore, bool hasNext,
                                 float randomUnit) const noexcept
{
    std::vector<WayNode*> candidates;
    for (auto* node : nodes_)
    {
        if (node == ignore || (hasNext && node->GetNext() == nullptr))
            continue;
        candidates.push_back(node);
    }
    if (candidates.empty())
        return nullptr;
    const float unit = std::clamp(randomUnit, 0.0F, 1.0F);
    const auto index = unit >= 1.0F
                           ? candidates.size() - 1U
                           : static_cast<std::size_t>(
                                 unit * candidates.size());
    return candidates[index];
}

const WayPoint::Nodes& WayPoint::GetNodes() const noexcept
{
    return nodes_;
}

WayNode::WayNode(WayPath* path, WayPoint* point)
    : path_(path), point_(point), tile_(this)
{
    if (path_ == nullptr || point_ == nullptr)
        throw std::invalid_argument("WayNode requires path and point");
    point_->InsertNode(this);
}

WayNode::~WayNode()
{
    point_->RemoveNode(this);
}

WayNode::Tile::Tile(WayNode* node) noexcept : node_(node) {}

void WayNode::Tile::ApplyChanges() const
{
    if (!changed_)
        return;
    // Match Trace.cpp: clear the dirty bit before asking the first node for
    // its own previous direction, which intentionally resolves to the
    // direction just computed below.
    changed_ = false;
    const TraceVec2 start = xy(node_->GetPos());
    if (node_->GetNext() != nullptr)
    {
        const TraceVec2 delta = subtract(
            xy(node_->GetNext()->GetPos()), start);
        directionLength_ = length(delta);
        direction_ = normalize(delta);
    }
    else
    {
        direction_ = GetPreviousDirection();
        directionLength_ = node_->GetRadius();
    }
    normal_ = clockwiseNormal(direction_);

    const TraceVec2 previousDirection = GetPreviousDirection();
    middleDirection_ = normalize(add(direction_, previousDirection));
    if (length(middleDirection_) <= epsilon)
        middleDirection_ = direction_;
    middleNormal_ = counterClockwiseNormal(middleDirection_);

    const float cosine = std::clamp(
        dot(direction_, previousDirection), -1.0F, 1.0F);
    const float halfCosine = std::max((1.0F + cosine) * 0.5F,
                                      epsilon);
    nodeRadius_ = node_->GetRadius() / std::sqrt(halfCosine);
    edgeNormal_ = cross(previousDirection, direction_) > 0.0F
                      ? counterClockwiseNormal(middleDirection_)
                      : clockwiseNormal(middleDirection_);
    turnAngle_ = node_->GetPrev() != nullptr
                     ? std::acos(cosine)
                     : 0.0F;
    finishDistance_ = directionLength_;
    if (node_->GetNext() != nullptr && node_->GetNext() != node_)
        finishDistance_ +=
            node_->GetNext()->GetTile().GetFinishDist();
}

TraceVec2 WayNode::Tile::GetPreviousDirection() const
{
    const WayNode* previous = node_->GetPrev();
    return previous != nullptr && previous != node_
               ? previous->GetTile().GetDir()
               : direction_;
}

TraceVec2 WayNode::Tile::GetNextMidNormal() const
{
    const WayNode* next = node_->GetNext();
    return next != nullptr && next != node_
               ? next->GetTile().GetMidNorm()
               : GetMidNorm();
}

float WayNode::Tile::GetNextNodeRadius() const
{
    const WayNode* next = node_->GetNext();
    return next != nullptr && next != node_
               ? next->GetTile().GetNodeRadius()
               : GetNodeRadius();
}

void WayNode::Tile::Changed() const noexcept { changed_ = true; }

std::uint32_t WayNode::Tile::ComputeTrackInd(
    const TraceVec2& point) const
{
    ApplyChanges();
    const std::uint32_t count = std::max(GetTrackCount(), 1U);
    const float tileWidth = GetWidth(point);
    const float trackWidth = tileWidth / static_cast<float>(count);
    if (trackWidth <= epsilon)
        return 0U;
    const float trackPosition = lineDistance(
        normal_, xy(node_->GetPos()), point);
    const auto raw = static_cast<int>(std::abs(std::floor(
        trackPosition / trackWidth +
        static_cast<float>(count) * 0.5F)));
    return static_cast<std::uint32_t>(
        std::clamp(raw, 0, static_cast<int>(count) - 1));
}

TraceVec2 WayNode::Tile::ComputeTrackNormOff(
    const TraceVec2& point, std::uint32_t track) const
{
    ApplyChanges();
    const std::uint32_t count = std::max(GetTrackCount(), 1U);
    track = std::min(track, count - 1U);
    const float tileWidth = GetWidth(point);
    const float trackWidth = tileWidth / static_cast<float>(count);
    float offset = lineDistance(normal_, xy(node_->GetPos()), point);
    offset = tileWidth * 0.5F + offset;
    const float trackOffset =
        trackWidth * (static_cast<float>(track) + 0.5F) - offset;
    return multiply(normal_, trackOffset);
}

bool WayNode::Tile::RayCast(const TraceVec3& rayPos,
                            const TraceVec3& rayVec,
                            float* distance) const
{
    const TraceVec3 start = node_->GetPos();
    const TraceVec3 finish = node_->GetNext() != nullptr
                                 ? node_->GetNext()->GetPos()
                                 : start;
    const TraceVec3 segmentDirection = normalize(subtract(finish, start));
    TraceVec3 planeNormal{};
    TraceVec3 planePoint{};
    if (dot(segmentDirection, rayVec) > 0.001F)
    {
        const TraceVec3 right = cross(segmentDirection, rayVec);
        planeNormal = normalize(cross(right, segmentDirection));
        planePoint = start;
    }
    else
    {
        planeNormal = segmentDirection;
        planePoint = multiply(add(start, finish), 0.5F);
    }
    const float denominator = dot(planeNormal, rayVec);
    if (std::abs(denominator) <= epsilon)
        return false;
    const float hit = dot(planeNormal, subtract(planePoint, rayPos)) /
                      denominator;
    if (hit < 0.0F ||
        !IsContains(add(rayPos, multiply(rayVec, hit))))
        return false;
    if (distance != nullptr)
        *distance = hit;
    return true;
}

bool WayNode::Tile::IsContains(const TraceVec3& point,
                               bool lengthClamp, float* distance,
                               float widthError) const
{
    ApplyChanges();
    const TraceVec2 point2 = xy(point);
    const TraceVec2 start = xy(node_->GetPos());
    const TraceVec3 nextPosition = node_->GetNext() != nullptr
                                       ? node_->GetNext()->GetPos()
                                       : node_->GetPos();
    const float firstDistance = lineDistance(
        middleDirection_, start, point2);
    const float secondDistance = lineDistance(
        node_->GetNext() != nullptr ?
            node_->GetNext()->GetTile().GetMidDir() : middleDirection_,
        xy(nextPosition), point2);
    const float directionDistance =
        lineDistance(normal_, start, point2);
    const float coordinate = ComputeCoordX(firstDistance);
    const float height = ComputeHeight(coordinate);
    const float halfWidth = ComputeWidth(coordinate) * 0.5F;
    if (distance != nullptr)
        *distance = height;
    return (!lengthClamp || firstDistance * secondDistance < 0.0F) &&
           std::abs(directionDistance) < halfWidth + widthError &&
           std::abs(ComputeZCoord(coordinate) - point.z) < height;
}

bool WayNode::Tile::IsZLevelContains(const TraceVec3& point,
                                     float* distance) const
{
    const float height = GetHeight(xy(point));
    const float z = GetZCoord(xy(point));
    if (distance != nullptr)
        *distance = height;
    return z - point.z < height;
}

TraceVec2 WayNode::Tile::GetDir() const
{
    ApplyChanges();
    return direction_;
}

float WayNode::Tile::GetDirLength() const
{
    ApplyChanges();
    return directionLength_;
}

TraceVec2 WayNode::Tile::GetNorm() const
{
    ApplyChanges();
    return normal_;
}

TraceVec2 WayNode::Tile::GetMidDir() const
{
    ApplyChanges();
    return middleDirection_;
}

TraceVec2 WayNode::Tile::GetMidNorm() const
{
    ApplyChanges();
    return middleNormal_;
}

TraceVec2 WayNode::Tile::GetEdgeNorm() const
{
    ApplyChanges();
    return edgeNormal_;
}

float WayNode::Tile::GetNodeRadius() const
{
    ApplyChanges();
    return nodeRadius_;
}

float WayNode::Tile::GetTurnAngle() const
{
    ApplyChanges();
    return turnAngle_;
}

float WayNode::Tile::GetFinishDist() const
{
    ApplyChanges();
    return finishDistance_;
}

float WayNode::Tile::GetStartDist() const
{
    const auto* path = node_->GetPath();
    return std::max(path->GetLength() - GetFinishDist(), 0.0F);
}

float WayNode::Tile::ComputeCoordX(float distance) const
{
    ApplyChanges();
    return directionLength_ > 0.0F
               ? std::clamp(distance / directionLength_, 0.0F, 1.0F)
               : 0.0F;
}

float WayNode::Tile::ComputeCoordX(const TraceVec2& point) const
{
    ApplyChanges();
    return ComputeCoordX(lineDistance(
        direction_, xy(node_->GetPos()), point));
}

float WayNode::Tile::ComputeLength(float coordinate) const
{
    ApplyChanges();
    return directionLength_ * std::clamp(coordinate, 0.0F, 1.0F);
}

float WayNode::Tile::ComputeWidth(float coordinate) const
{
    return ComputeHeight(coordinate) * 2.0F;
}

float WayNode::Tile::ComputeHeight(float coordinate) const
{
    const float start = node_->GetRadius();
    const float finish = node_->GetNext() != nullptr
                             ? node_->GetNext()->GetRadius()
                             : start;
    return start + (finish - start) * coordinate;
}

float WayNode::Tile::ComputeZCoord(float coordinate) const
{
    const float start = node_->GetPos().z;
    const float finish = node_->GetNext() != nullptr
                             ? node_->GetNext()->GetPos().z
                             : start;
    return start + (finish - start) * coordinate;
}

TraceVec3 WayNode::Tile::GetPoint(float coordinate) const
{
    const TraceVec3 start = node_->GetPos();
    const TraceVec3 finish = node_->GetNext() != nullptr
                                 ? node_->GetNext()->GetPos()
                                 : start;
    return add(start, multiply(subtract(finish, start), coordinate));
}

float WayNode::Tile::GetLength(const TraceVec2& point) const
{
    return ComputeLength(ComputeCoordX(point));
}

float WayNode::Tile::GetHeight(const TraceVec2& point) const
{
    return ComputeHeight(ComputeCoordX(point));
}

float WayNode::Tile::GetWidth(const TraceVec2& point) const
{
    return ComputeWidth(ComputeCoordX(point));
}

float WayNode::Tile::GetZCoord(const TraceVec2& point) const
{
    return ComputeZCoord(ComputeCoordX(point));
}

TraceVec3 WayNode::Tile::GetCenter3() const
{
    return GetPoint(0.5F);
}

std::uint32_t WayNode::Tile::GetTrackCount() const noexcept
{
    return node_->GetPath()->GetTrace()->GetTrackCount();
}

void WayNode::SetPrev(WayNode* node) noexcept
{
    previous_ = node;
    Changed();
}

void WayNode::SetNext(WayNode* node) noexcept
{
    next_ = node;
    Changed();
}

void WayNode::Changed() noexcept
{
    if (path_ == nullptr)
        return;
    // The source invalidates the entire linked path because finish distance
    // and both neighbouring miter planes can change.
    WayNode* current = path_->GetFirst();
    for (std::uint32_t visited = 0U;
         current != nullptr && visited < path_->GetCount(); ++visited)
    {
        current->tile_.Changed();
        current = current->next_;
    }
}

bool WayNode::RayCast(const TraceVec3& rayPos,
                      const TraceVec3& rayVec,
                      float* distance) const
{
    return raySphere(rayPos, rayVec, point_->GetPos(),
                     tile_.GetNodeRadius(), distance);
}

bool WayNode::IsContains2(const TraceVec2& point,
                          float* distance) const
{
    const float centerDistance = length(subtract(point, GetPos2()));
    if (distance != nullptr)
        *distance = centerDistance;
    return centerDistance < tile_.GetNodeRadius();
}

bool WayNode::IsContains(const TraceVec3& point,
                         float* distance) const
{
    const float centerDistance = length(subtract(point, point_->GetPos()));
    if (distance != nullptr)
        *distance = centerDistance;
    return centerDistance < tile_.GetNodeRadius();
}

WayPath* WayNode::GetPath() noexcept { return path_; }
const WayPath* WayNode::GetPath() const noexcept { return path_; }
WayPoint* WayNode::GetPoint() noexcept { return point_; }
const WayPoint* WayNode::GetPoint() const noexcept { return point_; }
const WayNode::Tile& WayNode::GetTile() const noexcept { return tile_; }
WayNode* WayNode::GetPrev() noexcept { return previous_; }
const WayNode* WayNode::GetPrev() const noexcept { return previous_; }
WayNode* WayNode::GetNext() noexcept { return next_; }
const WayNode* WayNode::GetNext() const noexcept { return next_; }
const TraceVec3& WayNode::GetPos() const noexcept { return point_->GetPos(); }
TraceVec2 WayNode::GetPos2() const noexcept { return xy(GetPos()); }
float WayNode::GetSize() const noexcept { return point_->GetSize(); }
float WayNode::GetRadius() const noexcept { return GetSize() * 0.5F; }

WayPath::WayPath(Trace* trace) noexcept : trace_(trace) {}

WayNode* WayPath::Add(WayPoint* point, WayNode* where)
{
    if (point == nullptr)
        return nullptr;
    auto value = std::unique_ptr<WayNode>(new WayNode(this, point));
    WayNode* node = value.get();
    nodes_.push_back(std::move(value));
    if (first_ == nullptr)
    {
        first_ = last_ = node;
        node->Changed();
        return node;
    }
    if (where == nullptr)
    {
        node->SetPrev(last_);
        last_->SetNext(node);
        last_ = node;
    }
    else
    {
        node->SetPrev(where->previous_);
        node->SetNext(where);
        if (where->previous_ != nullptr)
            where->previous_->SetNext(node);
        else
            first_ = node;
        where->SetPrev(node);
    }
    node->Changed();
    return node;
}

void WayPath::Delete(WayNode* value)
{
    if (value == nullptr || value->path_ != this)
        return;
    if (value->previous_ != nullptr && value->previous_ != value)
        value->previous_->SetNext(value->next_ == value ? nullptr
                                                        : value->next_);
    if (value->next_ != nullptr && value->next_ != value)
        value->next_->SetPrev(value->previous_ == value ? nullptr
                                                        : value->previous_);
    if (value == first_)
        first_ = value->next_ == value ? nullptr : value->next_;
    if (value == last_)
        last_ = value->previous_ == value ? nullptr : value->previous_;
    const auto found = std::find_if(
        nodes_.begin(), nodes_.end(),
        [value](const auto& candidate) { return candidate.get() == value; });
    if (found != nodes_.end())
        nodes_.erase(found);
    if (nodes_.empty())
        first_ = last_ = nullptr;
}

void WayPath::Clear() noexcept
{
    nodes_.clear();
    first_ = last_ = nullptr;
    enclosed_ = false;
}

bool WayPath::IsEnclosed() const noexcept { return enclosed_; }

void WayPath::Enclosed(bool value) noexcept
{
    if (enclosed_ == value)
        return;
    enclosed_ = value;
    if (first_ != nullptr)
    {
        first_->SetPrev(enclosed_ ? last_ : nullptr);
        last_->SetNext(enclosed_ ? first_ : nullptr);
    }
}

WayNode* WayPath::RayCast(const TraceVec3& rayPos,
                          const TraceVec3& rayVec, WayNode* where,
                          float* distance) const
{
    float minimum = 0.0F;
    WayNode* result = nullptr;
    WayNode* node = where != nullptr ? where : first_;
    for (std::size_t visited = 0U;
         node != nullptr && visited < nodes_.size(); ++visited)
    {
        float candidateDistance = 0.0F;
        if (node->RayCast(rayPos, rayVec, &candidateDistance) &&
            (result == nullptr || minimum > candidateDistance))
        {
            minimum = candidateDistance;
            result = node;
        }
        node = node->GetNext();
    }
    if (distance != nullptr)
        *distance = minimum;
    return result;
}

WayNode* WayPath::IsTileContains(const TraceVec3& point,
                                 WayNode* where,
                                 float widthError) const
{
    WayNode* node = where != nullptr
                        ? (where->GetPrev() != nullptr ? where->GetPrev()
                                                       : where)
                        : first_;
    for (std::size_t visited = 0U;
         node != nullptr && visited < nodes_.size(); ++visited)
    {
        if (node->GetTile().IsContains(
                point, true, nullptr, widthError))
            return node;
        node = node->GetNext();
    }
    if (first_ != nullptr && first_->IsContains(point))
        return first_;
    if (last_ != nullptr && last_->IsContains(point))
        return last_;
    return nullptr;
}

Trace* WayPath::GetTrace() noexcept { return trace_; }
const Trace* WayPath::GetTrace() const noexcept { return trace_; }
WayNode* WayPath::GetFirst() noexcept { return first_; }
const WayNode* WayPath::GetFirst() const noexcept { return first_; }
WayNode* WayPath::GetLast() noexcept { return last_; }
const WayNode* WayPath::GetLast() const noexcept { return last_; }
std::uint32_t WayPath::GetCount() const noexcept
{
    return static_cast<std::uint32_t>(nodes_.size());
}

float WayPath::GetLength() const
{
    return first_ != nullptr ? first_->GetTile().GetFinishDist() : 0.0F;
}

WayNode* WayPath::GetNode(std::size_t index) noexcept
{
    return const_cast<WayNode*>(
        static_cast<const WayPath*>(this)->GetNode(index));
}

const WayNode* WayPath::GetNode(std::size_t index) const noexcept
{
    const WayNode* node = first_;
    for (std::size_t current = 0U;
         node != nullptr && current < index; ++current)
        node = node->GetNext();
    return node;
}

std::size_t WayPath::GetNodeIndex(const WayNode* node) const noexcept
{
    const WayNode* current = first_;
    for (std::size_t index = 0U;
         current != nullptr && index < nodes_.size(); ++index)
    {
        if (current == node)
            return index;
        current = current->GetNext();
    }
    return static_cast<std::size_t>(-1);
}

bool Trace::NodeRef::valid() const noexcept
{
    return path != static_cast<std::size_t>(-1) &&
           node != static_cast<std::size_t>(-1);
}

Trace::Trace(std::uint32_t tracksCount) noexcept
    : tracksCount_(std::max(tracksCount, 1U))
{
}

Trace::~Trace() { Clear(); }

WayPoint* Trace::AddPoint()
{
    return AddPoint(pointId_++);
}

WayPoint* Trace::AddPoint(std::uint32_t id)
{
    auto point = std::unique_ptr<WayPoint>(new WayPoint(this, id));
    WayPoint* result = point.get();
    points_.push_back(std::move(point));
    pointId_ = std::max(pointId_, id + 1U);
    return result;
}

void Trace::DelPoint(WayPoint* value)
{
    if (value == nullptr || !value->GetNodes().empty())
        return;
    std::erase_if(points_, [value](const auto& point) {
        return point.get() == value;
    });
}

void Trace::ClearPoints() noexcept
{
    if (std::any_of(points_.begin(), points_.end(),
                    [](const auto& point) {
                        return !point->GetNodes().empty();
                    }))
        ClearPaths();
    points_.clear();
    pointId_ = 0U;
}

WayPath* Trace::AddPath()
{
    auto path = std::unique_ptr<WayPath>(new WayPath(this));
    WayPath* result = path.get();
    paths_.push_back(std::move(path));
    return result;
}

void Trace::DelPath(WayPath* value)
{
    std::erase_if(paths_, [value](const auto& path) {
        return path.get() == value;
    });
}

void Trace::ClearPaths() noexcept { paths_.clear(); }

void Trace::Clear() noexcept
{
    ClearPaths();
    ClearPoints();
}

WayNode* Trace::RayCast(const TraceVec3& rayPos,
                        const TraceVec3& rayVec,
                        float* distance) const
{
    float minimum = 0.0F;
    WayNode* result = nullptr;
    for (const auto& path : paths_)
    {
        float candidateDistance = 0.0F;
        WayNode* node = path->RayCast(
            rayPos, rayVec, nullptr, &candidateDistance);
        if (node != nullptr &&
            (result == nullptr || minimum > candidateDistance))
        {
            result = node;
            minimum = candidateDistance;
        }
    }
    if (distance != nullptr)
        *distance = minimum;
    return result;
}

WayNode* Trace::IsTileContains(const TraceVec3& point,
                               WayNode* where,
                               float widthError) const
{
    const WayPath* preferredPath = nullptr;
    if (where != nullptr)
    {
        preferredPath = where->GetPath();
        if (auto* found = preferredPath->IsTileContains(
                point, where, widthError))
            return found;
    }
    for (const auto& path : paths_)
    {
        if (path.get() == preferredPath)
            continue;
        if (auto* found = path->IsTileContains(
                point, nullptr, widthError))
            return found;
    }
    return nullptr;
}

WayPoint* Trace::FindPoint(std::uint32_t id) noexcept
{
    return const_cast<WayPoint*>(
        static_cast<const Trace*>(this)->FindPoint(id));
}

const WayPoint* Trace::FindPoint(std::uint32_t id) const noexcept
{
    const auto found = std::find_if(
        points_.begin(), points_.end(),
        [id](const auto& point) { return point->GetId() == id; });
    return found == points_.end() ? nullptr : found->get();
}

WayNode* Trace::FindClosestNode(const TraceVec3& point) const noexcept
{
    float minimum = 0.0F;
    WayNode* result = nullptr;
    for (const auto& candidate : points_)
    {
        if (candidate->GetNodes().empty())
            continue;
        const float candidateDistance =
            length(subtract(point, candidate->GetPos()));
        if (result == nullptr || candidateDistance < minimum)
        {
            result = candidate->GetNodes().front();
            minimum = candidateDistance;
        }
    }
    return result;
}

const Trace::Points& Trace::GetPoints() const noexcept { return points_; }
const Trace::Paths& Trace::GetPaths() const noexcept { return paths_; }

WayPath* Trace::GetPath(std::size_t index) noexcept
{
    return index < paths_.size() ? paths_[index].get() : nullptr;
}

const WayPath* Trace::GetPath(std::size_t index) const noexcept
{
    return index < paths_.size() ? paths_[index].get() : nullptr;
}

WayNode* Trace::GetNode(std::size_t path, std::size_t node) noexcept
{
    auto* value = GetPath(path);
    return value != nullptr ? value->GetNode(node) : nullptr;
}

const WayNode* Trace::GetNode(std::size_t path,
                              std::size_t node) const noexcept
{
    const auto* value = GetPath(path);
    return value != nullptr ? value->GetNode(node) : nullptr;
}

Trace::NodeRef Trace::GetNodeRef(const WayNode* node) const noexcept
{
    if (node == nullptr)
        return {};
    for (std::size_t path = 0U; path < paths_.size(); ++path)
    {
        const std::size_t index = paths_[path]->GetNodeIndex(node);
        if (index != static_cast<std::size_t>(-1))
            return {path, index};
    }
    return {};
}

std::size_t Trace::GetPathCount() const noexcept { return paths_.size(); }
std::uint32_t Trace::GetTrackCount() const noexcept
{
    return tracksCount_;
}

} // namespace r3d::game::originalrace::source
