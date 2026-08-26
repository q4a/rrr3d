#include "OriginalMapObj.h"

#include "OriginalGameObject.h"
#include "OriginalWeapon.h"

#include <algorithm>
#include <array>
#include <stdexcept>
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

std::string makeRecordParent(std::string_view record)
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

std::string makeRecordName(std::string_view record)
{
    while (!record.empty() &&
           (record.back() == '/' || record.back() == '\\'))
        record.remove_suffix(1U);
    const auto slash = record.find_last_of("/\\");
    return std::string(
        slash == std::string_view::npos
            ? record
            : record.substr(slash + 1U));
}

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

MapObjRecord::MapObjRecord(
    std::string path, std::string parent,
    MapObjCategory category, GameObjType type)
    : path_(std::move(path)),
      name_(makeRecordName(path_)),
      parent_(parent.empty() ? makeRecordParent(path_) : std::move(parent)),
      category_(category), type_(type)
{
}

const std::string& MapObjRecord::GetPath() const noexcept { return path_; }
const std::string& MapObjRecord::GetName() const noexcept { return name_; }
const std::string& MapObjRecord::GetParent() const noexcept
{
    return parent_;
}
MapObjCategory MapObjRecord::GetCategory() const noexcept
{
    return category_;
}
GameObjType MapObjRecord::GetType() const noexcept { return type_; }

MapObjRecordLibrary::MapObjRecordLibrary(
    MapObjCategory category) noexcept
    : category_(category)
{
}

void MapObjRecordLibrary::SetCategory(MapObjCategory value) noexcept
{
    if (records_.empty())
        category_ = value;
}

MapObjCategory MapObjRecordLibrary::GetCategory() const noexcept
{
    return category_;
}

MapObjRecord& MapObjRecordLibrary::GetOrCreateRecord(
    std::string path, GameObjType type, std::string parent)
{
    const auto found = records_.find(path);
    if (found != records_.end())
    {
        if (found->second->GetType() != type)
        {
            throw std::invalid_argument(
                "MapObj record '" + path + "' changes type from " +
                GameObjTypeName(found->second->GetType()) + " to " +
                GameObjTypeName(type));
        }
        return *found->second;
    }
    const std::string key = path;
    auto record = std::unique_ptr<MapObjRecord>(new MapObjRecord(
        std::move(path), std::move(parent), category_, type));
    auto& result = *record;
    records_.emplace(key, std::move(record));
    return result;
}

MapObjRecord* MapObjRecordLibrary::FindRecord(
    std::string_view path)
{
    return const_cast<MapObjRecord*>(
        static_cast<const MapObjRecordLibrary*>(this)->FindRecord(path));
}

const MapObjRecord* MapObjRecordLibrary::FindRecord(
    std::string_view path) const
{
    const auto found = records_.find(std::string(path));
    return found == records_.end() ? nullptr : found->second.get();
}

std::size_t MapObjRecordLibrary::GetRecordCount() const noexcept
{
    return records_.size();
}

void MapObjRecordLibrary::AddProxyTo(
    MapObj& object, const MapObjRecord& record) const
{
    if (record.GetCategory() != category_)
        throw std::invalid_argument("MapObj record/library category mismatch");
    object.SetRecordProxy(&record);
}

MapObj::MapObj(MapObjects* owner) : owner_(owner)
{
    CreateGameObj();
}

MapObj::~MapObj()
{
    SetPlayer(nullptr);
    if (gameObj_ != nullptr)
        gameObj_->SetMapObj(nullptr);
}

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
    if (type_ == GameObjType::Proj)
        replacement = std::make_unique<AutoProj>();
    else if (type_ == GameObjType::DestrObj)
        replacement = std::make_unique<DestrObj>();
    else
        replacement = std::make_unique<GameObject>();

    // Source CreateGameObj calls its narrow Assign, deletes the old object,
    // then explicitly assigns empty name/parent/component-owner locals.
    if (gameObj_ != nullptr)
    {
        replacement->AssignSource(*gameObj_);
        gameObj_->SetParent(nullptr);
        gameObj_->SetLogic(nullptr);
    }
    replacement->SetMapObj(this);
    replacement->SetName({});
    replacement->SetParent(nullptr);
    gameObj_ = std::move(replacement);

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
AutoProj* MapObj::GetAutoProj() noexcept
{
    return dynamic_cast<AutoProj*>(gameObj_.get());
}
const AutoProj* MapObj::GetAutoProj() const noexcept
{
    return dynamic_cast<const AutoProj*>(gameObj_.get());
}

const std::string& MapObj::GetName() const noexcept
{
    return gameObj_->GetName();
}
void MapObj::SetName(std::string value)
{
    gameObj_->SetName(std::move(value));
}
GameObject* MapObj::GetParent() const noexcept
{
    return gameObj_ != nullptr ? gameObj_->GetParent() : nullptr;
}
void MapObj::SetParent(GameObject* value)
{
    if (gameObj_ != nullptr)
        gameObj_->SetParent(value);
}
const std::string& MapObj::GetRecord() const noexcept { return record_; }
const MapObjRecord* MapObj::GetRecordProxy() const noexcept
{
    return recordProxy_;
}
const std::string& MapObj::GetRecordParent() const noexcept
{
    return recordParent_;
}
MapObjCategory MapObj::GetCategory() const noexcept { return category_; }

void MapObj::SetRecord(std::string value, MapObjCategory category,
                       std::string parent)
{
    recordProxy_ = nullptr;
    record_ = std::move(value);
    category_ = category;
    recordParent_ = parent.empty()
        ? MapObjects::RecordParent(record_)
        : std::move(parent);
}

void MapObj::SetRecordProxy(const MapObjRecord* value)
{
    recordProxy_ = nullptr;
    if (value == nullptr)
        return;
    SetType(value->GetType());
    SetRecord(
        value->GetPath(), value->GetCategory(), value->GetParent());
    recordProxy_ = value;
}

Player* MapObj::GetPlayer() noexcept { return player_; }
const Player* MapObj::GetPlayer() const noexcept { return player_; }
void MapObj::SetPlayer(Player* value) noexcept { player_ = value; }
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
    const std::string baseName = makeRecordName(record);
    auto& object = Add(type, baseName);
    object.SetRecord(
        std::move(record), category, std::move(recordParent));
    object.SetId(id);
    return object;
}

MapObj& MapObjects::Add(
    const MapObjRecord& record, std::uint32_t id)
{
    auto object = std::make_unique<MapObj>(this);
    object->SetRecordProxy(&record);
    object->SetName(MakeUniqueName(record.GetName()));
    object->SetId(id);
    object->SetParent(owner_);
    auto& result = *object;
    objects_.push_back(std::move(object));
    return result;
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
    return makeRecordParent(record);
}

std::string MapObjects::MakeUniqueName(std::string baseName) const
{
    if (baseName.empty())
        baseName = "obj";
    const auto exists = [&](std::string_view candidate) {
        if (observer_ != nullptr)
            return observer_->IsMapObjNameUsed(candidate);
        if (owner_ != nullptr)
        {
            return std::any_of(
                owner_->GetChildren().begin(), owner_->GetChildren().end(),
                [&](const auto* object) {
                    return object != nullptr &&
                           object->GetName() == candidate;
                });
        }
        return std::any_of(
            objects_.begin(), objects_.end(),
            [&](const auto& object) {
                return object != nullptr &&
                       object->GetName() == candidate;
            });
    };
    // LexStd Component::MakeUniqueName always appends a decimal suffix,
    // starting with zero; the unsuffixed base is never returned.
    for (std::size_t suffix = 0U;; ++suffix)
    {
        auto candidate = baseName + std::to_string(suffix);
        if (!exists(candidate))
            return candidate;
    }
}

} // namespace r3d::game::originalrace::source
