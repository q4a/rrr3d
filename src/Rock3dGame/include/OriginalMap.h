#pragma once

#include "OriginalGameObject.h"
#include "OriginalMapObj.h"
#include "OriginalTrace.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>

namespace r3d::game::originalrace::source
{

class Logic;

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

    explicit Map(Logic* logic = nullptr);
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
    MapObj& AddMapObj(const MapObj& value);
    MapObj& InsertMapObj(std::unique_ptr<MapObj> value);
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
    MapObjRecordLibrary& GetRecordLib(MapObjCategory category) noexcept;
    const MapObjRecordLibrary& GetRecordLib(
        MapObjCategory category) const noexcept;
    MapObj* GetMapObj(
        std::uint32_t id, bool includeDead = false) noexcept;
    const MapObj* GetMapObj(
        std::uint32_t id, bool includeDead = false) const noexcept;
    MapObj* GetSemaphore() noexcept;
    Logic* GetLogic() noexcept;
    const Logic* GetLogic() const noexcept;
    MapObj& GetGround() noexcept;
    const MapObj& GetGround() const noexcept;
    TouchDeath& GetGroundTouchDeath() noexcept;
    const TouchDeath& GetGroundTouchDeath() const noexcept;
    Trace& GetTrace() noexcept;
    const Trace& GetTrace() const noexcept;
    std::uint32_t GetLastId() const noexcept;

private:
    static std::size_t CategoryIndex(MapObjCategory value) noexcept;
    void Register(MapObj& value, std::uint32_t id);
    void OnMapObjRemoving(MapObj& value) noexcept override;
    void OnDestrObjSeparating(DestrObj& value) override;
    bool IsMapObjNameUsed(
        std::string_view value) const noexcept override;

    // Record libraries outlive live category objects, matching DataBase.
    // Declaration order makes categories destruct before their proxies.
    std::array<MapObjRecordLibrary, 7U> recordLibraries_;
    std::array<MapObjects, 7U> categories_;
    Objects objects_;
    Logic* logic_ = nullptr;
    // Map.cpp owns these for the complete lifetime of the world.  The Jolt
    // adapter supplies the physical Z=0 plane; this object retains its
    // source gameplay identity and TouchDeath behavior.
    MapObj ground_;
    TouchDeath groundTouchDeath_;
    Trace trace_{4U};
    std::uint32_t lastId_ = defaultMapObjId;
};

} // namespace r3d::game::originalrace::source
