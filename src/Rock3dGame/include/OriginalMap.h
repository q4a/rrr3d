#pragma once

#include "OriginalMapObj.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

namespace r3d::game::originalrace::source
{

// Backend-neutral Map.cpp runtime registry. XML/resource parsing remains in
// OriginalRace, while this class owns the seven live category collections
// and the global MapObj ID namespace used by gameplay and network RPCs.
class Map final : private MapObjectsObserver
{
public:
    using Objects = std::map<std::uint32_t, MapObj*>;
    static constexpr std::uint32_t defaultMapObjId = 0U;
    static constexpr std::size_t invalidSourceIndex =
        static_cast<std::size_t>(-1);

    Map();
    ~Map() override;

    Map(const Map&) = delete;
    Map& operator=(const Map&) = delete;

    MapObj& AddMapObj(
        MapObjCategory category, GameObjType type,
        std::string record = {},
        std::size_t sourceIndex = invalidSourceIndex);
    MapObj& AddMapObj(
        MapObjCategory category, GameObjType type,
        std::string record, std::uint32_t sourceId,
        std::size_t sourceIndex);
    bool DelMapObj(MapObj* value) noexcept;
    void ReserveIdsThrough(std::uint32_t value) noexcept;
    void Clear() noexcept;

    std::size_t GetMapObjCount(
        std::string_view record,
        MapObjCategory category) const noexcept;
    const Objects& GetObjects() const noexcept;
    MapObjects& GetMapObjList(MapObjCategory category) noexcept;
    const MapObjects& GetMapObjList(
        MapObjCategory category) const noexcept;
    MapObj* GetMapObj(
        std::uint32_t id, bool includeDead = false) noexcept;
    const MapObj* GetMapObj(
        std::uint32_t id, bool includeDead = false) const noexcept;
    MapObj* GetSemaphore() noexcept;
    std::uint32_t GetLastId() const noexcept;

private:
    static std::size_t CategoryIndex(MapObjCategory value) noexcept;
    void Register(MapObj& value, std::uint32_t id);
    void OnMapObjRemoving(MapObj& value) noexcept override;

    std::array<MapObjects, 7U> categories_;
    Objects objects_;
    std::uint32_t lastId_ = defaultMapObjId;
};

} // namespace r3d::game::originalrace::source
