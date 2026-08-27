#pragma once

#include "OriginalMapObj.h"

#include <array>
#include <cstddef>
#include <string_view>

namespace r3d::game::originalrace
{

struct Race;

namespace source
{

// Backend-neutral owner of DataBase.cpp's seven MapObjLib instances and
// source-record load transaction.  XML/R3D parsing supplies Race records;
// bgfx and Jolt remain consumers of the graph/physics descriptors loaded
// into those records.
class DataBase
{
public:
    DataBase();

    void Configure(const Race& race);
    void Clear() noexcept;

    MapObjRecordLibrary& GetMapObjLib(
        MapObjCategory category) noexcept;
    const MapObjRecordLibrary& GetMapObjLib(
        MapObjCategory category) const noexcept;
    MapObjRecord* GetRecord(
        MapObjCategory category, std::string_view name,
        bool assertFind = true);
    const MapObjRecord* GetRecord(
        MapObjCategory category, std::string_view name,
        bool assertFind = true) const;
    std::size_t GetRecordCount() const noexcept;

private:
    static std::size_t CategoryIndex(MapObjCategory value) noexcept;

    std::array<MapObjRecordLibrary, 7U> recordLibraries_;
};

} // namespace source
} // namespace r3d::game::originalrace
