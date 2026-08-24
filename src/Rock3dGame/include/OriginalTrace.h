#pragma once

#include "physics/OriginalVehiclePhysics.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace r3d::game::originalrace::source
{

using TraceVec3 = r3d::physics::Vec3;

struct TraceVec2
{
    float x = 0.0F;
    float y = 0.0F;
};

class Trace;
class WayPath;
class WayNode;

// Portable gameplay transcription of the original WayPoint. Object/Component
// reference counting and editor serialization stay at the resource boundary;
// the point identity, geometry and node links retain the Windows API.
class WayPoint
{
    friend class WayNode;
    friend class Trace;

public:
    using Nodes = std::vector<WayNode*>;

    bool RayCast(const TraceVec3& rayPos, const TraceVec3& rayVec,
                 float* distance = nullptr) const;
    bool IsContains(const TraceVec3& point,
                    float* distance = nullptr) const;

    std::uint32_t GetId() const noexcept;
    const TraceVec3& GetPos() const noexcept;
    void SetPos(const TraceVec3& value) noexcept;
    float GetSize() const noexcept;
    void SetSize(float value) noexcept;
    const TraceVec3& GetOff() const noexcept;
    void SetOff(const TraceVec3& value) noexcept;

    bool IsFind(const WayPath* path) const noexcept;
    bool IsFind(const WayNode* node,
                const WayNode* ignore = nullptr) const noexcept;
    WayNode* GetRandomNode(WayNode* ignore, bool hasNext,
                           float randomUnit = 0.0F) const noexcept;
    const Nodes& GetNodes() const noexcept;

private:
    WayPoint(Trace* trace, std::uint32_t id) noexcept;
    void InsertNode(WayNode* node);
    void RemoveNode(WayNode* node) noexcept;
    void Changed() noexcept;

    [[maybe_unused]] Trace* trace_ = nullptr;
    std::uint32_t id_ = 0U;
    TraceVec3 position_{};
    float size_ = 0.0F;
    TraceVec3 offset_{};
    Nodes nodes_;
};

class WayNode
{
    friend class WayPath;
    friend class Trace;

public:
    class Tile
    {
    public:
        void Changed() const noexcept;
        std::uint32_t ComputeTrackInd(const TraceVec2& point) const;
        TraceVec2 ComputeTrackNormOff(const TraceVec2& point,
                                      std::uint32_t track) const;

        bool RayCast(const TraceVec3& rayPos, const TraceVec3& rayVec,
                     float* distance = nullptr) const;
        bool IsContains(const TraceVec3& point, bool lengthClamp = true,
                        float* distance = nullptr,
                        float widthError = 0.0F) const;
        bool IsZLevelContains(const TraceVec3& point,
                              float* distance = nullptr) const;

        TraceVec2 GetDir() const;
        float GetDirLength() const;
        TraceVec2 GetNorm() const;
        TraceVec2 GetMidDir() const;
        TraceVec2 GetMidNorm() const;
        TraceVec2 GetEdgeNorm() const;
        float GetNodeRadius() const;
        float GetTurnAngle() const;
        float GetFinishDist() const;
        float GetStartDist() const;

        float ComputeCoordX(float distance) const;
        float ComputeCoordX(const TraceVec2& point) const;
        float ComputeLength(float coordinate) const;
        float ComputeWidth(float coordinate) const;
        float ComputeHeight(float coordinate) const;
        float ComputeZCoord(float coordinate) const;
        TraceVec3 GetPoint(float coordinate) const;
        float GetLength(const TraceVec2& point) const;
        float GetHeight(const TraceVec2& point) const;
        float GetWidth(const TraceVec2& point) const;
        float GetZCoord(const TraceVec2& point) const;
        TraceVec3 GetCenter3() const;

        std::uint32_t GetTrackCount() const noexcept;

    private:
        friend class WayNode;
        explicit Tile(WayNode* node) noexcept;
        void ApplyChanges() const;
        TraceVec2 GetPreviousDirection() const;
        TraceVec2 GetNextMidNormal() const;
        float GetNextNodeRadius() const;

        WayNode* node_ = nullptr;
        mutable TraceVec2 direction_{};
        mutable float directionLength_ = 0.0F;
        mutable TraceVec2 normal_{};
        mutable TraceVec2 middleDirection_{};
        mutable TraceVec2 middleNormal_{};
        mutable TraceVec2 edgeNormal_{};
        mutable float nodeRadius_ = 0.0F;
        mutable float turnAngle_ = 0.0F;
        mutable float finishDistance_ = 0.0F;
        mutable bool changed_ = true;
    };

    void Changed() noexcept;
    bool RayCast(const TraceVec3& rayPos, const TraceVec3& rayVec,
                 float* distance = nullptr) const;
    bool IsContains2(const TraceVec2& point,
                     float* distance = nullptr) const;
    bool IsContains(const TraceVec3& point,
                    float* distance = nullptr) const;

    WayPath* GetPath() noexcept;
    const WayPath* GetPath() const noexcept;
    WayPoint* GetPoint() noexcept;
    const WayPoint* GetPoint() const noexcept;
    const Tile& GetTile() const noexcept;
    WayNode* GetPrev() noexcept;
    const WayNode* GetPrev() const noexcept;
    WayNode* GetNext() noexcept;
    const WayNode* GetNext() const noexcept;
    const TraceVec3& GetPos() const noexcept;
    TraceVec2 GetPos2() const noexcept;
    float GetSize() const noexcept;
    float GetRadius() const noexcept;
    ~WayNode();

private:
    WayNode(WayPath* path, WayPoint* point);
    void SetPrev(WayNode* node) noexcept;
    void SetNext(WayNode* node) noexcept;

    WayPath* path_ = nullptr;
    WayPoint* point_ = nullptr;
    Tile tile_;
    WayNode* previous_ = nullptr;
    WayNode* next_ = nullptr;
};

class WayPath
{
    friend class Trace;

public:
    WayNode* Add(WayPoint* point, WayNode* where = nullptr);
    void Delete(WayNode* value);
    void Clear() noexcept;
    bool IsEnclosed() const noexcept;
    void Enclosed(bool value) noexcept;

    WayNode* RayCast(const TraceVec3& rayPos, const TraceVec3& rayVec,
                     WayNode* where = nullptr,
                     float* distance = nullptr) const;
    WayNode* IsTileContains(const TraceVec3& point,
                            WayNode* where = nullptr,
                            float widthError = 0.0F) const;

    Trace* GetTrace() noexcept;
    const Trace* GetTrace() const noexcept;
    WayNode* GetFirst() noexcept;
    const WayNode* GetFirst() const noexcept;
    WayNode* GetLast() noexcept;
    const WayNode* GetLast() const noexcept;
    std::uint32_t GetCount() const noexcept;
    float GetLength() const;
    WayNode* GetNode(std::size_t index) noexcept;
    const WayNode* GetNode(std::size_t index) const noexcept;
    std::size_t GetNodeIndex(const WayNode* node) const noexcept;

private:
    explicit WayPath(Trace* trace) noexcept;

    Trace* trace_ = nullptr;
    bool enclosed_ = false;
    WayNode* first_ = nullptr;
    WayNode* last_ = nullptr;
    std::vector<std::unique_ptr<WayNode>> nodes_;
};

class Trace
{
public:
    using Points = std::vector<std::unique_ptr<WayPoint>>;
    using Paths = std::vector<std::unique_ptr<WayPath>>;

    struct NodeRef
    {
        std::size_t path = static_cast<std::size_t>(-1);
        std::size_t node = static_cast<std::size_t>(-1);

        bool valid() const noexcept;
        bool operator==(const NodeRef& other) const noexcept
        {
            return path == other.path && node == other.node;
        }
        bool operator!=(const NodeRef& other) const noexcept
        {
            return !(*this == other);
        }
    };

    explicit Trace(std::uint32_t tracksCount = 4U) noexcept;
    ~Trace();
    Trace(const Trace&) = delete;
    Trace& operator=(const Trace&) = delete;

    WayPoint* AddPoint();
    WayPoint* AddPoint(std::uint32_t id);
    void DelPoint(WayPoint* value);
    void ClearPoints() noexcept;
    WayPath* AddPath();
    void DelPath(WayPath* value);
    void ClearPaths() noexcept;
    void Clear() noexcept;

    WayNode* RayCast(const TraceVec3& rayPos, const TraceVec3& rayVec,
                     float* distance = nullptr) const;
    WayNode* IsTileContains(const TraceVec3& point,
                            WayNode* where = nullptr,
                            float widthError = 0.0F) const;
    WayPoint* FindPoint(std::uint32_t id) noexcept;
    const WayPoint* FindPoint(std::uint32_t id) const noexcept;
    WayNode* FindClosestNode(const TraceVec3& point) const noexcept;

    const Points& GetPoints() const noexcept;
    const Paths& GetPaths() const noexcept;
    WayPath* GetPath(std::size_t index) noexcept;
    const WayPath* GetPath(std::size_t index) const noexcept;
    WayNode* GetNode(std::size_t path, std::size_t node) noexcept;
    const WayNode* GetNode(std::size_t path,
                           std::size_t node) const noexcept;
    NodeRef GetNodeRef(const WayNode* node) const noexcept;
    std::size_t GetPathCount() const noexcept;
    std::uint32_t GetTrackCount() const noexcept;

private:
    Points points_;
    Paths paths_;
    std::uint32_t pointId_ = 0U;
    std::uint32_t tracksCount_ = 4U;
};

} // namespace r3d::game::originalrace::source
