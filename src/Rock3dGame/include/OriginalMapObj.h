#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace r3d::game::originalrace::source
{

class AutoProj;
class GameObject;
class DestrObj;
class MapObj;
class MapObjRecord;
class MapObjRecordLibrary;
class MapObjRecordNode;
class MapObjects;
class MapObjectsObserver;
class Player;

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

// Runtime counterpart of RecordNode. The source library owns a hierarchy of
// folder nodes shared by all records, so parent identity is stable and can be
// compared directly instead of being reconstructed from path strings.
class MapObjRecordNode
{
public:
    const std::string& GetName() const noexcept;
    const MapObjRecordNode* GetParent() const noexcept;
    const MapObjRecordLibrary* GetLibrary() const noexcept;
    const MapObjRecordNode* FindNode(std::string_view name) const noexcept;
    const MapObjRecord* FindRecord(std::string_view name) const noexcept;
    std::size_t GetNodeCount() const noexcept;
    std::size_t GetRecordCount() const noexcept;

private:
    friend class MapObjRecordLibrary;
    MapObjRecordNode(std::string name, MapObjRecordLibrary* library,
                     MapObjRecordNode* parent);

    std::string name_;
    MapObjRecordLibrary* library_ = nullptr;
    MapObjRecordNode* parent_ = nullptr;
    std::unordered_map<std::string,
                       std::unique_ptr<MapObjRecordNode>> nodes_;
    std::vector<const MapObjRecord*> records_;
};

// Stable source Record identity. MapObjRec obtains its category from the
// owning MapObjLib and stores the concrete GameObject type loaded from the
// serialized record. XML nodes remain parser-owned; runtime proxy identity
// and AddProxyTo behavior live here.
class MapObjRecord
{
public:
    const std::string& GetPath() const noexcept;
    const std::string& GetName() const noexcept;
    const std::string& GetParent() const noexcept;
    const MapObjRecordNode* GetParentNode() const noexcept;
    const MapObjRecordLibrary* GetLibrary() const noexcept;
    MapObjCategory GetCategory() const noexcept;
    GameObjType GetType() const noexcept;

private:
    friend class MapObjRecordLibrary;
    MapObjRecord(std::string path, std::string parent,
                 MapObjRecordLibrary* library,
                 MapObjRecordNode* parentNode, GameObjType type);

    std::string path_;
    std::string name_;
    std::string parent_;
    MapObjRecordLibrary* library_ = nullptr;
    MapObjRecordNode* parentNode_ = nullptr;
    GameObjType type_ = GameObjType::GameObj;
};

class MapObjRecordLibrary
{
public:
    MapObjRecordLibrary();
    explicit MapObjRecordLibrary(MapObjCategory category);

    void SetCategory(MapObjCategory value);
    MapObjCategory GetCategory() const noexcept;
    MapObjRecord& GetOrCreateRecord(
        std::string path, GameObjType type,
        std::string parent = {});
    MapObjRecord* FindRecord(std::string_view path);
    const MapObjRecord* FindRecord(std::string_view path) const;
    std::size_t GetRecordCount() const noexcept;
    MapObjRecordNode& GetRootNode() noexcept;
    const MapObjRecordNode& GetRootNode() const noexcept;
    void AddProxyTo(MapObj& object, const MapObjRecord& record) const;

private:
    std::vector<std::string> RecordPathParts(
        std::string_view path) const;
    MapObjRecordNode* GetOrCreateParentNode(
        const std::vector<std::string>& parts);
    MapObjCategory category_ = MapObjCategory::Effects;
    std::unordered_map<std::string, std::unique_ptr<MapObjRecord>> records_;
    std::unique_ptr<MapObjRecordNode> root_;
};

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
    void SetParent(GameObject* value);

    const std::string& GetRecord() const noexcept;
    const MapObjRecord* GetRecordProxy() const noexcept;
    const std::string& GetRecordParent() const noexcept;
    MapObjCategory GetCategory() const noexcept;
    void SetRecord(std::string value, MapObjCategory category,
                   std::string parent = {});
    void SetRecordProxy(const MapObjRecord* value);

    Player* GetPlayer() noexcept;
    const Player* GetPlayer() const noexcept;
    void SetPlayer(Player* value) noexcept;
    std::uint32_t GetId() const noexcept;
    void SetId(std::uint32_t value) noexcept;
    std::size_t GetSourceIndex() const noexcept;
    void SetSourceIndex(std::size_t value) noexcept;
    bool IsSpecial() const noexcept;

private:
    void CreateGameObj();

    MapObjects* owner_ = nullptr;
    // Legacy ReplaceRef lifetime is represented by the owning race Player
    // collection; MapObj retains the exact non-owning gameplay association.
    Player* player_ = nullptr;
    std::uint32_t id_ = 0U;
    std::size_t sourceIndex_ = static_cast<std::size_t>(-1);
    GameObjType type_ = GameObjType::GameObj;
    MapObjCategory category_ = MapObjCategory::Effects;
    const MapObjRecord* recordProxy_ = nullptr;
    std::unique_ptr<GameObject> gameObj_;
    std::string record_;
    std::string recordParent_;
};

class MapObjectsObserver
{
public:
    virtual ~MapObjectsObserver() = default;
    virtual void OnMapObjRemoving(MapObj& value) noexcept = 0;
    virtual bool IsMapObjNameUsed(
        std::string_view value) const noexcept = 0;
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
    void SetObserver(MapObjectsObserver* value) noexcept;

    MapObj& Add(GameObjType type, std::string baseName = "obj");
    MapObj& Add(GameObjType type, MapObjCategory category,
                std::string record, std::uint32_t id,
                std::string recordParent = {});
    MapObj& Add(const MapObjRecord& record, std::uint32_t id);
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
    MapObjectsObserver* observer_ = nullptr;
    std::vector<std::unique_ptr<MapObj>> objects_;
    bool locked_ = false;
};

} // namespace r3d::game::originalrace::source
