#include "OriginalMapObj.h"

#include "OriginalGameObject.h"
#include "OriginalWeapon.h"

#include <algorithm>
#include <array>
#include <utility>

namespace r3d::game::originalrace::source
{
namespace
{

constexpr std::array<const char*, 6U> gameObjectTypeNames{
    "gotGameObj", "gotGameCar", "gotRockCar", "gotProj",
    "gotWeapon", "gotDestrObj"};
constexpr std::array<const char*, 7U> mapObjectCategoryNames{
    "ctEffects", "ctDecoration", "ctTrack", "ctWeapon", "ctCar",
    "ctWaypoint", "ctBonus"};

} // namespace

const char* GameObjTypeName(GameObjType value) noexcept
{
    const auto index = static_cast<std::size_t>(value);
    return index < gameObjectTypeNames.size()
        ? gameObjectTypeNames[index]
        : "gotGameObj";
}

const char* MapObjCategoryName(MapObjCategory value) noexcept
{
    const auto index = static_cast<std::size_t>(value);
    return index < mapObjectCategoryNames.size()
        ? mapObjectCategoryNames[index]
        : "ctEffects";
}

MapObj::MapObj(MapObjects* owner) : owner_(owner)
{
    CreateGameObj();
}

MapObj::~MapObj() = default;

MapObjects* MapObj::GetOwner() const noexcept { return owner_; }
GameObjType MapObj::GetType() const noexcept { return type_; }

void MapObj::SetType(GameObjType value)
{
    if (type_ == value)
        return;
    type_ = value;
    CreateGameObj();
}

void MapObj::CreateGameObj()
{
    std::unique_ptr<GameObject> replacement;
    if (type_ == GameObjType::DestrObj)
        replacement = std::make_unique<DestrObj>();
    else
        replacement = std::make_unique<GameObject>();

    // MapObj::CreateGameObj calls Assign before deleting the old instance.
    // Portable assignment intentionally does not copy listener ownership.
    if (gameObj_ != nullptr)
        *replacement = *gameObj_;
    gameObj_ = std::move(replacement);

    if (type_ == GameObjType::Proj)
        autoProj_ = std::make_unique<AutoProj>();
    else
        autoProj_.reset();
}

GameObject& MapObj::GetGameObj() noexcept { return *gameObj_; }
const GameObject& MapObj::GetGameObj() const noexcept { return *gameObj_; }
GameObject& MapObj::SetGameObj(GameObjType value)
{
    SetType(value);
    return *gameObj_;
}
DestrObj* MapObj::GetDestrObj() noexcept
{
    return dynamic_cast<DestrObj*>(gameObj_.get());
}
const DestrObj* MapObj::GetDestrObj() const noexcept
{
    return dynamic_cast<const DestrObj*>(gameObj_.get());
}
AutoProj* MapObj::GetAutoProj() noexcept { return autoProj_.get(); }
const AutoProj* MapObj::GetAutoProj() const noexcept
{
    return autoProj_.get();
}

const std::string& MapObj::GetName() const noexcept { return name_; }
void MapObj::SetName(std::string value) { name_ = std::move(value); }
GameObject* MapObj::GetParent() const noexcept { return parent_; }
void MapObj::SetParent(GameObject* value) noexcept { parent_ = value; }
const std::string& MapObj::GetRecord() const noexcept { return record_; }
const std::string& MapObj::GetRecordParent() const noexcept
{
    return recordParent_;
}
MapObjCategory MapObj::GetCategory() const noexcept { return category_; }

void MapObj::SetRecord(std::string value, MapObjCategory category,
                       std::string parent)
{
    record_ = std::move(value);
    category_ = category;
    recordParent_ = parent.empty()
        ? MapObjects::RecordParent(record_)
        : std::move(parent);
}

std::size_t MapObj::GetPlayerId() const noexcept { return playerId_; }
void MapObj::SetPlayerId(std::size_t value) noexcept { playerId_ = value; }
std::uint32_t MapObj::GetId() const noexcept { return id_; }
void MapObj::SetId(std::uint32_t value) noexcept { id_ = value; }
std::size_t MapObj::GetSourceIndex() const noexcept
{
    return sourceIndex_;
}
void MapObj::SetSourceIndex(std::size_t value) noexcept
{
    sourceIndex_ = value;
}

bool MapObj::IsSpecial() const noexcept
{
    return category_ == MapObjCategory::Decoration &&
           (recordParent_ == "Misc" || recordParent_ == "Crush");
}

MapObjects::MapObjects(GameObject* owner) noexcept : owner_(owner) {}
void MapObjects::SetObserver(MapObjectsObserver* value) noexcept
{
    observer_ = value;
}

MapObj& MapObjects::Add(GameObjType type, std::string baseName)
{
    auto object = std::make_unique<MapObj>(this);
    object->SetType(type);
    object->SetName(MakeUniqueName(std::move(baseName)));
    object->SetParent(owner_);
    auto& result = *object;
    objects_.push_back(std::move(object));
    return result;
}

MapObj& MapObjects::Add(GameObjType type, MapObjCategory category,
                        std::string record, std::uint32_t id,
                        std::string recordParent)
{
    const std::string baseName = record.empty() ? "obj" : record;
    auto& object = Add(type, baseName);
    object.SetRecord(
        std::move(record), category, std::move(recordParent));
    object.SetId(id);
    return object;
}

void MapObjects::Reserve(std::size_t value) { objects_.reserve(value); }

void MapObjects::Clear() noexcept
{
    if (locked_)
        return;
    for (auto& object : objects_)
    {
        if (object != nullptr)
        {
            if (observer_ != nullptr)
                observer_->OnMapObjRemoving(*object);
            object->GetGameObj().DestroyObject();
        }
    }
    objects_.clear();
}

MapObj* MapObjects::Get(std::size_t slot) noexcept
{
    return slot < objects_.size() ? objects_[slot].get() : nullptr;
}
const MapObj* MapObjects::Get(std::size_t slot) const noexcept
{
    return slot < objects_.size() ? objects_[slot].get() : nullptr;
}
std::size_t MapObjects::GetSlotCount() const noexcept
{
    return objects_.size();
}
std::size_t MapObjects::GetLiveCount() const noexcept
{
    return static_cast<std::size_t>(std::count_if(
        objects_.begin(), objects_.end(),
        [](const auto& value) { return value != nullptr; }));
}
bool MapObjects::IsLocked() const noexcept { return locked_; }

bool MapObjects::Remove(std::size_t slot) noexcept
{
    if (locked_ || slot >= objects_.size() || objects_[slot] == nullptr)
        return false;
    if (observer_ != nullptr)
        observer_->OnMapObjRemoving(*objects_[slot]);
    objects_[slot]->GetGameObj().DestroyObject();
    objects_[slot].reset();
    return true;
}

bool MapObjects::ProgressSlot(
    std::size_t slot, float deltaTime) noexcept
{
    auto* mapObject = Get(slot);
    if (mapObject == nullptr)
        return false;
    if (auto* destructible = mapObject->GetDestrObj())
        destructible->OnProgress(deltaTime);
    else
        mapObject->GetGameObj().OnProgress(deltaTime);
    if (auto* projectile = mapObject->GetAutoProj())
        projectile->OnProgress(deltaTime);
    return mapObject->GetGameObj().GetLiveState() ==
           GameObject::LiveState::Death;
}

bool MapObjects::ProgressOne(
    std::size_t slot, float deltaTime) noexcept
{
    if (locked_ || Get(slot) == nullptr)
        return false;
    locked_ = true;
    const bool remove = ProgressSlot(slot, deltaTime);
    locked_ = false;
    return remove && Remove(slot);
}

MapObjects::ProgressResult MapObjects::OnProgress(
    float deltaTime) noexcept
{
    ProgressResult result;
    for (std::size_t slot = 0U; slot < objects_.size(); ++slot)
    {
        if (objects_[slot] == nullptr)
            continue;
        ++result.progressed;
        result.removed += ProgressOne(slot, deltaTime) ? 1U : 0U;
    }
    return result;
}

MapObjects::ProgressResult MapObjects::OnProgressSpecial(
    float deltaTime) noexcept
{
    ProgressResult result;
    for (std::size_t slot = 0U; slot < objects_.size(); ++slot)
    {
        if (objects_[slot] == nullptr || !objects_[slot]->IsSpecial())
            continue;
        ++result.progressed;
        result.removed += ProgressOne(slot, deltaTime) ? 1U : 0U;
    }
    return result;
}

void MapObjects::Death(int damageType, GameObject* target) noexcept
{
    const auto type = static_cast<DamageType>(damageType);
    for (auto& object : objects_)
    {
        if (object != nullptr)
            object->GetGameObj().Death(type, target);
    }
}

GameObject* MapObjects::GetOwner() const noexcept { return owner_; }

std::string MapObjects::RecordParent(std::string_view record)
{
    const auto slash = record.find_last_of("/\\");
    if (slash == std::string_view::npos)
        return {};
    const auto previous = slash == 0U
        ? std::string_view::npos
        : record.find_last_of("/\\", slash - 1U);
    const auto first = previous == std::string_view::npos
        ? 0U
        : previous + 1U;
    return std::string(record.substr(first, slash - first));
}

std::string MapObjects::MakeUniqueName(std::string baseName) const
{
    if (baseName.empty())
        baseName = "obj";
    const auto exists = [&](std::string_view candidate) {
        return std::any_of(
            objects_.begin(), objects_.end(),
            [&](const auto& object) {
                return object != nullptr &&
                       object->GetName() == candidate;
            });
    };
    if (!exists(baseName))
        return baseName;
    for (std::size_t suffix = 1U;; ++suffix)
    {
        auto candidate = baseName + std::to_string(suffix);
        if (!exists(candidate))
            return candidate;
    }
}

} // namespace r3d::game::originalrace::source
