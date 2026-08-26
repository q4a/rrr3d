#include "OriginalMap.h"

#include "OriginalGameObject.h"
#include "OriginalLogic.h"

#include <algorithm>
#include <stdexcept>
#include <string_view>

namespace r3d::game::originalrace::source
{
namespace
{

std::string_view recordName(std::string_view value) noexcept
{
    const auto slash = value.find_last_of("/\\");
    return slash == std::string_view::npos
        ? value
        : value.substr(slash + 1U);
}

} // namespace

Map::Map(Logic* logic) : logic_(logic)
{
    if (logic_ != nullptr)
        logic_->SetMap(this);
    for (std::size_t index = 0U; index < categories_.size(); ++index)
    {
        categories_[index].SetObserver(this);
        recordLibraries_[index].SetCategory(
            static_cast<MapObjCategory>(index));
    }
    groundTouchDeath_ = &ground_.GetGameObj().GetBehaviors()
        .Add<TouchDeath>(BehaviorType::TouchDeath);
}

Map::~Map()
{
    Clear();
    if (logic_ != nullptr && logic_->GetMap() == this)
        logic_->SetMap(nullptr);
}

std::size_t Map::CategoryIndex(MapObjCategory value) noexcept
{
    const auto index = static_cast<std::size_t>(value);
    return index < 7U ? index : 0U;
}

MapObj& Map::AddMapObj(
    MapObjCategory category, GameObjType type, std::string record,
    std::size_t sourceIndex)
{
    while (objects_.contains(++lastId_))
    {
    }
    return AddMapObj(
        category, type, std::move(record), lastId_, sourceIndex);
}

MapObj& Map::AddMapObj(
    MapObjCategory category, GameObjType type, std::string record,
    std::uint32_t sourceId, std::size_t sourceIndex)
{
    if (sourceId == defaultMapObjId || objects_.contains(sourceId))
    {
        const auto found = objects_.find(sourceId);
        const std::string existing =
            found != objects_.end() && found->second != nullptr
                ? found->second->GetRecord()
                : std::string("<null>");
        throw std::invalid_argument(
            "MapObj ID " + std::to_string(sourceId) +
            " is not unique/nonzero for '" + record +
            "' (existing '" + existing + "')");
    }
    const auto categoryIndex = CategoryIndex(category);
    auto& recordProxy = recordLibraries_[categoryIndex].GetOrCreateRecord(
        std::move(record), type);
    auto& result = categories_[categoryIndex].Add(recordProxy, sourceId);
    result.SetSourceIndex(sourceIndex);
    result.GetGameObj().SetLogic(logic_);
    Register(result, sourceId);
    return result;
}

MapObj& Map::AddMapObj(const MapObj& value)
{
    const auto* record = value.GetRecordProxy();
    if (record == nullptr)
        throw std::invalid_argument(
            "Map::AddMapObj clone requires a source record proxy");
    auto& result = AddMapObj(
        record->GetCategory(), value.GetType(), record->GetPath(),
        invalidSourceIndex);
    result.CopySerializedStateFrom(value);
    return result;
}

MapObj& Map::InsertMapObj(std::unique_ptr<MapObj> value)
{
    if (value == nullptr)
        throw std::invalid_argument("Map::InsertMapObj received null");
    const auto category = value->GetRecordProxy() != nullptr
        ? value->GetRecordProxy()->GetCategory()
        : MapObjCategory::Decoration;
    if (value->GetRecordProxy() == nullptr)
        value->SetRecord({}, category);

    while (objects_.contains(++lastId_))
    {
    }
    auto& result = categories_[CategoryIndex(category)].Insert(
        std::move(value));
    result.GetGameObj().SetLogic(logic_);
    Register(result, lastId_);
    return result;
}

void Map::Register(MapObj& value, std::uint32_t id)
{
    value.SetId(id);
    objects_.emplace(id, &value);
    lastId_ = std::max(lastId_, id);
}

bool Map::DelMapObj(MapObj* value) noexcept
{
    if (value == nullptr)
        return false;
    for (auto& category : categories_)
    {
        for (std::size_t slot = 0U;
             slot < category.GetSlotCount(); ++slot)
        {
            if (category.Get(slot) == value)
                return category.Remove(slot);
        }
    }
    return false;
}

void Map::ReserveIdsThrough(std::uint32_t value) noexcept
{
    lastId_ = std::max(lastId_, value);
}

void Map::Clear() noexcept
{
    for (auto& category : categories_)
        category.Clear();
    objects_.clear();
    lastId_ = defaultMapObjId;
}

std::size_t Map::GetMapObjCount(
    std::string_view record, MapObjCategory category) const noexcept
{
    const auto& objects = categories_[CategoryIndex(category)];
    std::size_t count = 0U;
    for (std::size_t slot = 0U; slot < objects.GetSlotCount(); ++slot)
    {
        const auto* object = objects.Get(slot);
        count += object != nullptr && object->GetRecord() == record
            ? 1U
            : 0U;
    }
    return count;
}

const Map::Objects& Map::GetObjects() const noexcept { return objects_; }

MapObjects& Map::GetMapObjList(MapObjCategory category) noexcept
{
    return categories_[CategoryIndex(category)];
}
const MapObjects& Map::GetMapObjList(
    MapObjCategory category) const noexcept
{
    return categories_[CategoryIndex(category)];
}

MapObjRecordLibrary& Map::GetRecordLib(
    MapObjCategory category) noexcept
{
    return recordLibraries_[CategoryIndex(category)];
}
const MapObjRecordLibrary& Map::GetRecordLib(
    MapObjCategory category) const noexcept
{
    return recordLibraries_[CategoryIndex(category)];
}

MapObj* Map::GetMapObj(std::uint32_t id, bool includeDead) noexcept
{
    return const_cast<MapObj*>(
        static_cast<const Map*>(this)->GetMapObj(id, includeDead));
}
const MapObj* Map::GetMapObj(
    std::uint32_t id, bool includeDead) const noexcept
{
    if (id == defaultMapObjId)
        return nullptr;
    const auto found = objects_.find(id);
    if (found == objects_.end() || found->second == nullptr)
        return nullptr;
    const auto* object = found->second;
    return includeDead || object->GetGameObj().GetLiveState() !=
                              GameObject::LiveState::Death
        ? object
        : nullptr;
}

MapObj* Map::GetSemaphore() noexcept
{
    auto& decorations =
        GetMapObjList(MapObjCategory::Decoration);
    for (std::size_t slot = 0U;
         slot < decorations.GetSlotCount(); ++slot)
    {
        auto* object = decorations.Get(slot);
        if (object != nullptr &&
            recordName(object->GetRecord()) == "semaphore")
            return object;
    }
    return nullptr;
}

Logic* Map::GetLogic() noexcept { return logic_; }
const Logic* Map::GetLogic() const noexcept { return logic_; }
MapObj& Map::GetGround() noexcept { return ground_; }
const MapObj& Map::GetGround() const noexcept { return ground_; }
TouchDeath& Map::GetGroundTouchDeath() noexcept
{
    return *groundTouchDeath_;
}
const TouchDeath& Map::GetGroundTouchDeath() const noexcept
{
    return *groundTouchDeath_;
}
Trace& Map::GetTrace() noexcept { return trace_; }
const Trace& Map::GetTrace() const noexcept { return trace_; }

std::uint32_t Map::GetLastId() const noexcept { return lastId_; }

void Map::OnMapObjRemoving(MapObj& value) noexcept
{
    const auto found = objects_.find(value.GetId());
    if (found != objects_.end() && found->second == &value)
        objects_.erase(found);
}

void Map::OnDestrObjSeparating(DestrObj& value)
{
    value.ReleaseDestruction(*this);
}

bool Map::IsMapObjNameUsed(std::string_view value) const noexcept
{
    return std::any_of(
        objects_.begin(), objects_.end(),
        [&](const auto& item) {
            return item.second != nullptr &&
                   item.second->GetName() == value;
        });
}

} // namespace r3d::game::originalrace::source
