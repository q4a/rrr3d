#include "OriginalMapObj.h"

#include "OriginalGameObject.h"
#include "OriginalGameCar.h"
#include "OriginalRockCar.h"
#include "OriginalWeapon.h"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>
#include <vector>

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

MapObjRecordNode::MapObjRecordNode(
    std::string name, MapObjRecordLibrary* library,
    MapObjRecordNode* parent)
    : name_(std::move(name)), library_(library), parent_(parent)
{
}

const std::string& MapObjRecordNode::GetName() const noexcept
{
    return name_;
}
const MapObjRecordNode* MapObjRecordNode::GetParent() const noexcept
{
    return parent_;
}
const MapObjRecordLibrary* MapObjRecordNode::GetLibrary() const noexcept
{
    return library_;
}
const MapObjRecordNode* MapObjRecordNode::FindNode(
    std::string_view name) const noexcept
{
    const auto found = nodes_.find(std::string(name));
    return found == nodes_.end() ? nullptr : found->second.get();
}
const MapObjRecord* MapObjRecordNode::FindRecord(
    std::string_view name) const noexcept
{
    const auto found = std::find_if(
        records_.begin(), records_.end(),
        [&](const auto* record) {
            return record != nullptr && record->GetName() == name;
        });
    return found == records_.end() ? nullptr : *found;
}
std::size_t MapObjRecordNode::GetNodeCount() const noexcept
{
    return nodes_.size();
}
std::size_t MapObjRecordNode::GetRecordCount() const noexcept
{
    return records_.size();
}

MapObjRecord::MapObjRecord(
    std::string path, std::string parent,
    MapObjRecordLibrary* library, MapObjRecordNode* parentNode,
    GameObjType type)
    : path_(std::move(path)),
      name_(makeRecordName(path_)),
      parent_(parent.empty() && parentNode != nullptr
                  ? parentNode->GetName()
                  : std::move(parent)),
      library_(library), parentNode_(parentNode), type_(type)
{
}

const std::string& MapObjRecord::GetPath() const noexcept { return path_; }
const std::string& MapObjRecord::GetName() const noexcept { return name_; }
const std::string& MapObjRecord::GetParent() const noexcept
{
    return parent_;
}
const MapObjRecordNode* MapObjRecord::GetParentNode() const noexcept
{
    return parentNode_;
}
const MapObjRecordLibrary* MapObjRecord::GetLibrary() const noexcept
{
    return library_;
}
MapObjCategory MapObjRecord::GetCategory() const noexcept
{
    return library_ != nullptr
        ? library_->GetCategory()
        : MapObjCategory::Effects;
}
GameObjType MapObjRecord::GetType() const noexcept { return type_; }

MapObjRecordLibrary::MapObjRecordLibrary()
    : root_(new MapObjRecordNode(
          MapObjCategoryName(category_), this, nullptr))
{
}

MapObjRecordLibrary::MapObjRecordLibrary(
    MapObjCategory category)
    : category_(category),
      root_(new MapObjRecordNode(
          MapObjCategoryName(category_), this, nullptr))
{
}

void MapObjRecordLibrary::SetCategory(MapObjCategory value)
{
    if (records_.empty())
    {
        category_ = value;
        root_.reset(new MapObjRecordNode(
            MapObjCategoryName(category_), this, nullptr));
    }
}

MapObjCategory MapObjRecordLibrary::GetCategory() const noexcept
{
    return category_;
}

std::vector<std::string> MapObjRecordLibrary::RecordPathParts(
    std::string_view path) const
{
    std::vector<std::string> parts;
    std::size_t first = 0U;
    while (first < path.size())
    {
        const auto slash = path.find_first_of("/\\", first);
        const auto end = slash == std::string_view::npos
            ? path.size()
            : slash;
        if (end > first)
            parts.emplace_back(path.substr(first, end - first));
        if (slash == std::string_view::npos)
            break;
        first = slash + 1U;
    }

    const std::string_view categoryName = MapObjCategoryName(category_);
    const auto category = std::find(parts.begin(), parts.end(), categoryName);
    if (category != parts.end())
        parts.erase(parts.begin(), category + 1);
    if (parts.empty())
        parts.emplace_back("obj");
    return parts;
}

MapObjRecordNode* MapObjRecordLibrary::GetOrCreateParentNode(
    const std::vector<std::string>& parts)
{
    auto* node = root_.get();
    for (std::size_t index = 0U; index + 1U < parts.size(); ++index)
    {
        if (node->FindRecord(parts[index]) != nullptr)
            throw std::invalid_argument(
                "MapObj record node '" + parts[index] +
                "' collides with an existing record");
        const auto found = node->nodes_.find(parts[index]);
        if (found != node->nodes_.end())
        {
            node = found->second.get();
            continue;
        }
        auto child = std::unique_ptr<MapObjRecordNode>(
            new MapObjRecordNode(parts[index], this, node));
        node = child.get();
        node->parent_->nodes_.emplace(parts[index], std::move(child));
    }
    return node;
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
    const auto parts = RecordPathParts(path);
    auto* parentNode = GetOrCreateParentNode(parts);
    if (const auto* existing = parentNode->FindRecord(parts.back()))
    {
        if (existing->GetType() != type)
            throw std::invalid_argument(
                "MapObj record '" + path + "' changes type from " +
                GameObjTypeName(existing->GetType()) + " to " +
                GameObjTypeName(type));
        return *const_cast<MapObjRecord*>(existing);
    }
    if (parentNode->FindNode(parts.back()) != nullptr)
        throw std::invalid_argument(
            "MapObj record '" + path +
            "' collides with an existing record node");

    const std::string key = path;
    auto record = std::unique_ptr<MapObjRecord>(new MapObjRecord(
        std::move(path), std::move(parent), this, parentNode, type));
    auto& result = *record;
    parentNode->records_.push_back(&result);
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
    if (found != records_.end())
        return found->second.get();

    const auto parts = RecordPathParts(path);
    const auto* node = root_.get();
    for (std::size_t index = 0U; index + 1U < parts.size(); ++index)
    {
        node = node->FindNode(parts[index]);
        if (node == nullptr)
            return nullptr;
    }
    return node->FindRecord(parts.back());
}

std::size_t MapObjRecordLibrary::GetRecordCount() const noexcept
{
    return records_.size();
}

MapObjRecordNode& MapObjRecordLibrary::GetRootNode() noexcept
{
    return *root_;
}
const MapObjRecordNode& MapObjRecordLibrary::GetRootNode() const noexcept
{
    return *root_;
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
    {
        gameObj_->SetMapObj(nullptr);
        gameObj_->SetLogic(nullptr);
        gameObj_->SetParent(nullptr);
    }
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
    if (type_ == GameObjType::GameCar)
        replacement = std::make_unique<GameCar>();
    else if (type_ == GameObjType::RockCar)
        replacement = std::make_unique<RockCar>();
    else if (type_ == GameObjType::Proj)
        replacement = std::make_unique<AutoProj>();
    else if (type_ == GameObjType::Weapon)
        replacement = std::make_unique<Weapon>();
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
        gameObj_->SetMapObj(nullptr);
    }
    ownedGameObj_ = std::move(replacement);
    gameObj_ = ownedGameObj_.get();
    gameObj_->SetMapObj(this);
    gameObj_->SetName({});
    gameObj_->SetParent(nullptr);
}

GameObject& MapObj::GetGameObj() noexcept { return *gameObj_; }
const GameObject& MapObj::GetGameObj() const noexcept { return *gameObj_; }
GameObject& MapObj::SetGameObj(GameObjType value)
{
    SetType(value);
    return *gameObj_;
}
void MapObj::BindGameObj(RockCar& value)
{
    if (type_ != GameObjType::RockCar)
        throw std::invalid_argument(
            "MapObj can bind RockCar only for gotRockCar");
    if (gameObj_ == &value)
        return;
    if (value.GetMapObj() != nullptr)
        throw std::invalid_argument(
            "RockCar is already bound to another MapObj");

    const std::string name = gameObj_ != nullptr
        ? gameObj_->GetName() : std::string{};
    GameObject* parent = gameObj_ != nullptr
        ? gameObj_->GetParent() : nullptr;
    if (gameObj_ != nullptr)
    {
        value.AssignSource(*gameObj_);
        value.SetMaxLife(gameObj_->GetMaxLife());
        value.CopyProxyStateFrom(*gameObj_);
        gameObj_->SetParent(nullptr);
        gameObj_->SetLogic(nullptr);
        gameObj_->SetMapObj(nullptr);
    }
    ownedGameObj_.reset();
    gameObj_ = &value;
    gameObj_->SetMapObj(this);
    gameObj_->SetName(name);
    gameObj_->SetParent(parent);
}
GameCar* MapObj::GetGameCar() noexcept
{
    return dynamic_cast<GameCar*>(gameObj_);
}
const GameCar* MapObj::GetGameCar() const noexcept
{
    return dynamic_cast<const GameCar*>(gameObj_);
}
RockCar* MapObj::GetRockCar() noexcept
{
    return dynamic_cast<RockCar*>(gameObj_);
}
const RockCar* MapObj::GetRockCar() const noexcept
{
    return dynamic_cast<const RockCar*>(gameObj_);
}
Weapon* MapObj::GetWeapon() noexcept
{
    return dynamic_cast<Weapon*>(gameObj_);
}
const Weapon* MapObj::GetWeapon() const noexcept
{
    return dynamic_cast<const Weapon*>(gameObj_);
}
DestrObj* MapObj::GetDestrObj() noexcept
{
    return dynamic_cast<DestrObj*>(gameObj_);
}
const DestrObj* MapObj::GetDestrObj() const noexcept
{
    return dynamic_cast<const DestrObj*>(gameObj_);
}
AutoProj* MapObj::GetAutoProj() noexcept
{
    return dynamic_cast<AutoProj*>(gameObj_);
}
const AutoProj* MapObj::GetAutoProj() const noexcept
{
    return dynamic_cast<const AutoProj*>(gameObj_);
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

void MapObj::CopySerializedStateFrom(const MapObj& value)
{
    gameObj_->SetMaxLife(value.GetGameObj().GetMaxLife());
    gameObj_->CopyProxyStateFrom(value.GetGameObj());

    auto& destinationIncludes = gameObj_->GetIncludeList();
    destinationIncludes.Clear();
    const auto& sourceIncludes = value.GetGameObj().GetIncludeList();
    destinationIncludes.Reserve(sourceIncludes.GetLiveCount());
    for (std::size_t slot = 0U;
         slot < sourceIncludes.GetSlotCount(); ++slot)
    {
        const auto* sourceChild = sourceIncludes.Get(slot);
        if (sourceChild == nullptr)
            continue;
        MapObj* destinationChild = nullptr;
        if (sourceChild->GetRecordProxy() != nullptr)
        {
            destinationChild = &destinationIncludes.Add(
                *sourceChild->GetRecordProxy(), sourceChild->GetId());
        }
        else
        {
            destinationChild = &destinationIncludes.Add(
                sourceChild->GetType(), sourceChild->GetCategory(),
                sourceChild->GetRecord(), sourceChild->GetId(),
                sourceChild->GetRecordParent());
        }
        destinationChild->SetName(sourceChild->GetName());
        destinationChild->SetSourceIndex(sourceChild->GetSourceIndex());
        destinationChild->CopySerializedStateFrom(*sourceChild);
    }
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
    auto& result = *object;
    objects_.push_back(std::move(object));
    InsertItem(result);
    return result;
}

MapObj& MapObjects::Add(GameObjType type, MapObjCategory category,
                        std::string record, std::uint32_t id,
                        std::string recordParent)
{
    const std::string baseName = makeRecordName(record);
    auto object = std::make_unique<MapObj>(this);
    object->SetType(type);
    object->SetRecord(
        std::move(record), category, std::move(recordParent));
    object->SetName(MakeUniqueName(baseName));
    object->SetId(id);
    auto& result = *object;
    objects_.push_back(std::move(object));
    InsertItem(result);
    return result;
}

MapObj& MapObjects::Add(
    const MapObjRecord& record, std::uint32_t id)
{
    auto object = std::make_unique<MapObj>(this);
    object->SetRecordProxy(&record);
    object->SetName(MakeUniqueName(record.GetName()));
    object->SetId(id);
    auto& result = *object;
    objects_.push_back(std::move(object));
    InsertItem(result);
    return result;
}

MapObj& MapObjects::Insert(std::unique_ptr<MapObj> value)
{
    if (value == nullptr)
        throw std::invalid_argument("cannot insert a null MapObj");
    if (value->owner_ != nullptr)
        throw std::invalid_argument(
            "MapObj must be extracted from its previous owner first");
    value->owner_ = this;
    value->SetName(MakeUniqueName("item"));
    auto& result = *value;
    objects_.push_back(std::move(value));
    InsertItem(result);
    return result;
}

std::unique_ptr<MapObj> MapObjects::Extract(MapObj* value) noexcept
{
    if (locked_ || value == nullptr)
        return {};
    const auto found = std::find_if(
        objects_.begin(), objects_.end(),
        [&](const auto& object) { return object.get() == value; });
    if (found == objects_.end())
        return {};
    RemoveItem(**found);
    (*found)->SetParent(nullptr);
    (*found)->owner_ = nullptr;
    auto result = std::move(*found);
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
            RemoveItem(*object);
            object->GetGameObj().DestroyObject();
        }
    }
    objects_.clear();
    specialObjects_.clear();
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
    RemoveItem(*objects_[slot]);
    objects_[slot]->GetGameObj().DestroyObject();
    objects_[slot].reset();
    return true;
}

bool MapObjects::Remove(MapObj* value) noexcept
{
    if (value == nullptr)
        return false;
    const auto found = std::find_if(
        objects_.begin(), objects_.end(),
        [value](const auto& object) { return object.get() == value; });
    if (found == objects_.end())
        return false;
    return Remove(static_cast<std::size_t>(
        std::distance(objects_.begin(), found)));
}

bool MapObjects::ProgressSlot(
    std::size_t slot, float deltaTime) noexcept
{
    auto* mapObject = Get(slot);
    if (mapObject == nullptr)
        return false;
    if (auto* destructible = mapObject->GetDestrObj())
    {
        if (destructible->OnProgress(deltaTime) && observer_ != nullptr)
            observer_->OnDestrObjSeparating(*destructible);
    }
    else if (auto* rockCar = mapObject->GetRockCar())
        rockCar->OnProgress(deltaTime);
    else if (auto* gameCar = mapObject->GetGameCar())
        gameCar->OnProgress(deltaTime);
    else if (auto* weapon = mapObject->GetWeapon())
        weapon->OnProgress(deltaTime);
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
    for (std::size_t special = 0U;
         special < specialObjects_.size();)
    {
        auto* object = specialObjects_[special];
        const auto found = std::find_if(
            objects_.begin(), objects_.end(),
            [&](const auto& candidate) {
                return candidate.get() == object;
            });
        if (found == objects_.end())
        {
            specialObjects_.erase(
                specialObjects_.begin() +
                static_cast<std::ptrdiff_t>(special));
            continue;
        }
        const auto slot = static_cast<std::size_t>(
            std::distance(objects_.begin(), found));
        ++result.progressed;
        if (ProgressOne(slot, deltaTime))
        {
            ++result.removed;
            // RemoveItem erased the same entry from specialObjects_.
            continue;
        }
        ++special;
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

void MapObjects::InsertItem(MapObj& value)
{
    value.SetParent(owner_);
    SpecialListChanged(value, false);
}

void MapObjects::RemoveItem(MapObj& value) noexcept
{
    // MapObj.cpp asserts this for every collection removal. Portable callers
    // reject the operation before reaching this point while a progress
    // callback is running.
    SpecialListChanged(value, true);
    if (observer_ != nullptr)
        observer_->OnMapObjRemoving(value);
}

void MapObjects::SpecialListChanged(
    MapObj& value, bool remove) noexcept
{
    if (!value.IsSpecial())
        return;
    const auto found = std::find(
        specialObjects_.begin(), specialObjects_.end(), &value);
    if (remove)
    {
        if (found != specialObjects_.end())
            specialObjects_.erase(found);
        return;
    }
    if (found == specialObjects_.end())
        specialObjects_.push_back(&value);
}

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
