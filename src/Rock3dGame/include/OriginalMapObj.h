#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace r3d::game::originalrace::source
{

class AutoProj;
class GameObject;
class DestrObj;
class MapObjects;

// MapObj.h::GameObjType.  These values are serialized by name in the
// Windows data, so keep their source order even where a platform adapter
// still supplies the concrete car or renderer class.
enum class GameObjType : std::uint8_t
{
    GameObj = 0,
    GameCar,
    RockCar,
    Proj,
    Weapon,
    DestrObj,
};

// IMapObjLib::Category, in the exact source order used while assigning
// global map-object IDs.
enum class MapObjCategory : std::uint8_t
{
    Effects = 0,
    Decoration,
    Track,
    Weapon,
    Car,
    Waypoint,
    Bonus,
};

const char* GameObjTypeName(GameObjType value) noexcept;
const char* MapObjCategoryName(MapObjCategory value) noexcept;

// Backend-neutral owner corresponding to source MapObj.  The portable
// GameObject hierarchy is being introduced incrementally: every entry owns
// the common GameObject lifetime, while gotProj additionally owns AutoProj's
// projectile state.  gotDestrObj is a real DestrObj already.
class MapObj
{
public:
    explicit MapObj(MapObjects* owner = nullptr);
    ~MapObj();

    MapObj(const MapObj&) = delete;
    MapObj& operator=(const MapObj&) = delete;
    MapObj(MapObj&&) = delete;
    MapObj& operator=(MapObj&&) = delete;

    MapObjects* GetOwner() const noexcept;
    GameObjType GetType() const noexcept;
    void SetType(GameObjType value);

    GameObject& GetGameObj() noexcept;
    const GameObject& GetGameObj() const noexcept;
    GameObject& SetGameObj(GameObjType value);
    DestrObj* GetDestrObj() noexcept;
    const DestrObj* GetDestrObj() const noexcept;
    AutoProj* GetAutoProj() noexcept;
    const AutoProj* GetAutoProj() const noexcept;

    const std::string& GetName() const noexcept;
    void SetName(std::string value);
    GameObject* GetParent() const noexcept;
    void SetParent(GameObject* value) noexcept;

    const std::string& GetRecord() const noexcept;
    const std::string& GetRecordParent() const noexcept;
    MapObjCategory GetCategory() const noexcept;
    void SetRecord(std::string value, MapObjCategory category,
                   std::string parent = {});

    std::size_t GetPlayerId() const noexcept;
    void SetPlayerId(std::size_t value) noexcept;
    std::uint32_t GetId() const noexcept;
    void SetId(std::uint32_t value) noexcept;
    bool IsSpecial() const noexcept;

private:
    void CreateGameObj();

    MapObjects* owner_ = nullptr;
    std::size_t playerId_ = static_cast<std::size_t>(-1);
    std::uint32_t id_ = 0U;
    GameObjType type_ = GameObjType::GameObj;
    MapObjCategory category_ = MapObjCategory::Effects;
    std::unique_ptr<GameObject> gameObj_;
    std::unique_ptr<AutoProj> autoProj_;
    std::string name_;
    std::string record_;
    std::string recordParent_;
    GameObject* parent_ = nullptr;
};

// Source MapObjects deletes a dead object only after its OnProgress callback
// and has a second progress path for Decoration/Misc and Decoration/Crush.
// Slots retain map-file indices after deletion so renderer and network views
// can continue to address the original global map-object ID safely.
class MapObjects
{
public:
    struct ProgressResult
    {
        std::size_t progressed = 0U;
        std::size_t removed = 0U;
    };

    MapObjects() = default;
    explicit MapObjects(GameObject* owner) noexcept;

    MapObj& Add(GameObjType type, std::string baseName = "obj");
    MapObj& Add(GameObjType type, MapObjCategory category,
                std::string record, std::uint32_t id,
                std::string recordParent = {});
    void Reserve(std::size_t value);
    void Clear() noexcept;

    MapObj* Get(std::size_t slot) noexcept;
    const MapObj* Get(std::size_t slot) const noexcept;
    std::size_t GetSlotCount() const noexcept;
    std::size_t GetLiveCount() const noexcept;
    bool IsLocked() const noexcept;

    bool Remove(std::size_t slot) noexcept;
    bool ProgressOne(std::size_t slot, float deltaTime) noexcept;
    ProgressResult OnProgress(float deltaTime) noexcept;
    ProgressResult OnProgressSpecial(float deltaTime) noexcept;
    void Death(int damageType, GameObject* target = nullptr) noexcept;

    GameObject* GetOwner() const noexcept;

private:
    friend class MapObj;
    static std::string RecordParent(std::string_view record);
    std::string MakeUniqueName(std::string baseName) const;
    bool ProgressSlot(std::size_t slot, float deltaTime) noexcept;

    GameObject* owner_ = nullptr;
    std::vector<std::unique_ptr<MapObj>> objects_;
    bool locked_ = false;
};

} // namespace r3d::game::originalrace::source
