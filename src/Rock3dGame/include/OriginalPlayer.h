#pragma once

#include "OriginalGameObject.h"
#include "OriginalRockCar.h"
#include "OriginalSlot.h"
#include "OriginalTrace.h"
#include "OriginalWeapon.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace r3d::game::originalrace
{
struct Vehicle;
struct WeaponDefinition;
}

namespace r3d::game::originalrace::source
{

class Map;
class MapObj;

// Slot order deliberately follows Player::SlotType from the Windows source:
// stHyper, stMine, stWeapon1..stWeapon4.  Player::TakeBonus depends on this
// order before applying its rounded random selection.
enum class PlayerBonusSlot : std::uint8_t
{
    None,
    Primary,
    Hyper,
    Mine,
};

enum class PlayerBonusType : std::uint8_t
{
    Money,
    Charge,
    Medpack,
    Immortal,
};

struct PlayerBonusResult
{
    PlayerBonusSlot slot = PlayerBonusSlot::None;
    std::size_t weapon = std::numeric_limits<std::size_t>::max();
    std::uint32_t amount = 0U;
};

enum class PlayerRestoreStep : std::uint8_t
{
    None,
    QueueRespawn,
};

enum class PlayerBlockMove : std::uint8_t
{
    Unblocked,
    Coast,
    Brake,
};

// Public events emitted by the source car GameObject and its Player listener.
// The race adapter consumes this queue without recreating their ordering.
enum class PlayerGameEventKind : std::uint8_t
{
    Damage,
    Kill,
    Overboard,
    DeathMine,
    Death,
};

struct PlayerGameEvent
{
    PlayerGameEventKind kind = PlayerGameEventKind::Damage;
    std::size_t otherPlayerId = GameObject::undefinedPlayerId;
    float value = 0.0F;
    DamageType damageType = DamageType::Simple;
};

// Backend result of the downward PhysX query made by Player::ResetCar.
// The source accepts the track plane and the player's own car, rejects every
// other shape, and treats a missing hit like the global death plane on the
// first sample.
enum class ResetCarRayKind : std::uint8_t
{
    None,
    TrackPlane,
    DeathPlane,
    OwnCar,
    Blocked,
};

struct ResetCarPose
{
    bool valid = false;
    Trace::NodeRef node;
    TraceVec3 position;
    TraceVec3 direction{1.0F, 0.0F, 0.0F};
};

using ResetCarRayCast =
    std::function<ResetCarRayKind(const TraceVec3&)>;

// Windows Player is a GameObjListener, while its RockCar owns physical life,
// damage and behaviors. Renderer/physics remain backend boundaries; race
// state, inventory, finish blocking and restore lifecycle stay here.
class Player : public GameObjectListener, public RockCarEventSink
{
public:
    // Race::Player identifiers from the original Windows runtime.  These
    // are bit fields, not indices into the portable racer vector.
    static constexpr int undefinedId = -1;
    static constexpr int humanId = 0;
    static constexpr int computerMask = 0x000000FF;
    static constexpr int opponentBit = 8;
    static constexpr int opponentMask = 0x0000FF00;
    static constexpr unsigned defaultNetSlot = 0U;
    static constexpr std::size_t undefinedPlayerId =
        GameObject::undefinedPlayerId;

    static constexpr std::size_t invalidWeapon =
        std::numeric_limits<std::size_t>::max();
    static constexpr std::size_t weaponSlotCount = 4U;
    static constexpr float restoreCarSeconds = 2.0F;
    static constexpr float finishBlockSeconds = 0.3F;

    static const std::array<float, 3> humanEasingMinimumDistance;
    static const std::array<float, 3> humanEasingMaximumDistance;
    static const std::array<float, 3> humanEasingMinimumSpeed;
    static const std::array<float, 3> humanEasingMaximumSpeed;

    Player();
    ~Player() override;

    struct BehaviorProgressResult
    {
        GameObject::ProgressResult gameObject;
        bool lowLifeActivated = false;
        bool lowLifeReleased = false;
        std::optional<EventEffect::SpawnResult> lowLifeSpawn;
        bool slowSpeedLimited = false;
        bool slowReleased = false;
    };

    static constexpr std::uint32_t cheatDisabled = 0U;
    static constexpr std::uint32_t cheatEnableSlower = 1U << 0U;
    static constexpr std::uint32_t cheatEnableFaster = 1U << 1U;

    enum class HeadLightMode : std::uint8_t
    {
        None,
        One,
        Two,
    };

    struct PresentationVector
    {
        float x = 0.0F;
        float y = 0.0F;
        float z = 0.0F;
    };

    struct PresentationQuaternion
    {
        float x = 0.0F;
        float y = 0.0F;
        float z = 0.0F;
        float w = 1.0F;
    };

    struct HeadLightState
    {
        bool created = false;
        bool enabled = false;
        // Player::InitLight requests a shadow only for hlFirst; the
        // renderer combines this with Environment::eqHigh.
        bool highQualityShadow = false;
        PresentationVector position{};
        PresentationQuaternion rotation{};
        float nearDistance = 1.0F;
        float farDistance = 50.0F;
        float phi = 3.14159265358979323846F / 3.0F;
        float theta = 3.14159265358979323846F / 6.0F;
        std::array<float, 4U> ambient{0.0F, 0.0F, 0.0F, 1.0F};
        std::array<float, 4U> diffuse{1.0F, 1.0F, 1.0F, 1.0F};
    };

    struct NightLightState
    {
        bool head = true;
        PresentationVector position{};
        std::array<float, 2U> size{1.0F, 1.0F};
    };

    // Backend-neutral graph state owned by the original Player methods.
    // bgfx/Metal consumes these records but must not reconstruct their
    // creation, attachment, material or reflection rules.
    struct PresentationState
    {
        std::array<HeadLightState, 2U> headLights{};
        bool nightFlareCreated = false;
        bool nightFlareAttached = false;
        std::vector<NightLightState> nightLights;
        bool reflectionScene = true;
        bool colorMaterialCreated = false;
        bool colorMaterialAttached = false;
        std::array<float, 4U> color{1.0F, 1.0F, 1.0F, 1.0F};
    };

    struct CheatPlayerView
    {
        std::size_t playerId = 0U;
        bool humanOrOpponent = false;
        bool active = true;
        float lap = 0.0F;
    };

    struct CheatResult
    {
        bool slower = false;
        bool faster = false;
        float speedLimit = 0.0F;
        float torqueScale = 1.0F;
        float steeringScale = 1.0F;
    };

    struct ProgressResult
    {
        CheatResult cheat;
        PlayerRestoreStep restore = PlayerRestoreStep::None;
        PlayerBlockMove blockMove = PlayerBlockMove::Unblocked;
    };
    static const std::array<float, 3> computerCheatMinimumTorque;
    static const std::array<float, 3> computerCheatMaximumTorque;
    static const std::array<float, 3> humanArmorScale;

    class CarState
    {
    public:
        struct UpdateResult
        {
            Trace::NodeRef previousLast;
            Trace::NodeRef currentTile;
            Trace::NodeRef lastNode;
            bool lastNodeChanged = false;
            bool lapPassed = false;
            bool moveInverseStarted = false;
            bool lostControl = false;
        };

        void Reset(Trace* trace = nullptr) noexcept;
        void OnCreateCar(bool newRace) noexcept;
        void OnFreeCar(bool freeState) noexcept;
        UpdateResult Update(Trace& trace,
                            const TraceVec3& position,
                            const TraceVec3& direction,
                            float vehicleSpeed,
                            float deltaTime);

        WayNode* GetCurTile(bool lastCorrect = false) noexcept;
        const WayNode* GetCurTile(bool lastCorrect = false) const noexcept;
        WayNode* GetLiveTile() noexcept;
        const WayNode* GetLiveTile() const noexcept;
        WayNode* GetCurNode() noexcept;
        const WayNode* GetCurNode() const noexcept;
        WayNode* GetLastNode() noexcept;
        const WayNode* GetLastNode() const noexcept;
        WayNode* EnsureLastNode() noexcept;
        Trace::NodeRef GetCurTileRef(bool lastCorrect = false) const noexcept;
        Trace::NodeRef GetLiveTileRef() const noexcept;
        Trace::NodeRef GetCurNodeRef() const noexcept;
        Trace::NodeRef GetLastNodeRef() const noexcept;
        std::int32_t GetPathIndex(bool lastCorrect = false) const noexcept;
        bool IsMainPath(bool lastCorrect = false) const noexcept;
        float GetPathLength(bool lastCorrect = false) const noexcept;
        float GetDist(bool lastCorrect = false) const noexcept;
        float GetLap(bool lastCorrect = false) const noexcept;
        float GetSpeed() const noexcept;
        void SetSize(float value) noexcept;
        float GetSize() const noexcept;
        float GetRadius() const noexcept;
        TraceVec3 GetPosition() const noexcept;
        TraceVec3 GetDirection3() const noexcept;
        TraceVec3 GetMapPos() const noexcept;
        float GetLastNodeCoordX() const noexcept;
        std::uint32_t GetTrack() const noexcept;

        std::uint32_t numLaps = 0U;
        bool moveInverse = false;
        bool cheatSlower = false;
        bool cheatFaster = false;

    private:
        void SetCurTile(WayNode* value) noexcept;
        void SetCurNode(WayNode* value) noexcept;
        void SetLastNode(WayNode* value) noexcept;

        Trace* trace_ = nullptr;
        TraceVec3 position_{};
        TraceVec3 direction_{1.0F, 0.0F, 0.0F};
        TraceVec3 direction3_{1.0F, 0.0F, 0.0F};
        float speed_ = 0.0F;
        float size_ = 0.0F;
        float radius_ = 0.0F;
        WayNode* curTile_ = nullptr;
        WayNode* curNode_ = nullptr;
        WayNode* lastNode_ = nullptr;
        float lastNodeCoordX_ = 0.5F;
        std::uint32_t track_ = 0U;
        float moveInverseStart_ = -1.0F;
        float maximumSpeed_ = 0.0F;
        float maximumSpeedTime_ = 0.0F;
    };

    void Reset(float newMaximumLife, std::uint32_t initialPlace,
               Trace* trace = nullptr) noexcept;
    GameObject::DamageResult Damage(
        std::size_t senderPlayerId, float value,
        DamageType damageType = DamageType::Simple) noexcept;
    GameObject::DamageResult Damage(
        std::size_t senderPlayerId, float value, float newLife,
        bool death, DamageType damageType) noexcept;
    bool Death(
        DamageType damageType = DamageType::Simple,
        GameObject* target = nullptr) noexcept;
    bool Resc() noexcept;
    void Healt(float value) noexcept;
    void Immortal(float time) noexcept;
    void SetImmortalFlag(bool value) noexcept;
    bool IsImmortal() const noexcept;
    float GetLife() const noexcept;
    void SetLife(float value) noexcept;
    float GetMaxLife() const noexcept;
    void SetMaxLife(float value) noexcept;
    float GetShieldSeconds() const noexcept;
    bool IsDestroyed() const noexcept;
    void ConfigureIdentity(
        int playerId, int sourceGamerId, unsigned sourceNetSlot,
        std::string sourceName, std::string sourceNetName,
        const std::array<float, 4>& sourceColor);
    int GetId() const noexcept;
    int GetGamerId() const noexcept;
    unsigned GetNetSlot() const noexcept;
    const std::string& GetNetName() const noexcept;
    const std::string& GetName() const noexcept;
    const std::array<float, 4>& GetColor() const noexcept;
    void SetId(int value) noexcept;
    void SetGamerId(int value) noexcept;
    void SetNetSlot(unsigned value) noexcept;
    void SetNetName(std::string value);
    void SetName(std::string value);
    void SetColor(const std::array<float, 4>& value) noexcept;
    bool IsHuman() const noexcept;
    bool IsComputer() const noexcept;
    bool IsOpponent() const noexcept;
    bool IsHumanOrOpponent() const noexcept;
    std::uint32_t GetCheat() const noexcept;
    void SetCheat(std::uint32_t value) noexcept;
    HeadLightMode GetHeadLight() const noexcept;
    void SetHeadlight(HeadLightMode value) noexcept;
    bool HasCar() const noexcept;
    bool HasAttachedLights() const noexcept;
    const PresentationState& GetPresentationState() const noexcept;
    bool GetReflScene() const noexcept;
    void SetReflScene(bool value) noexcept;
    const Vehicle* GetCarRecord() const noexcept;
    void BindMap(
        Map* value,
        std::size_t sourceIndex =
            std::numeric_limits<std::size_t>::max()) noexcept;
    MapObj* GetCarMapObj() noexcept;
    const MapObj* GetCarMapObj() const noexcept;
    void BindWheelSlipCatalog(
        const ObjectDefinition* trailEffect,
        const ObjectDefinition* smokeEffect) noexcept;
    void SetCar(const Vehicle* record) noexcept;
    void CreateCar(bool newRace);
    void FreeCar(bool freeState) noexcept;
    void OnLapPass(std::size_t weaponDefinitionCount) noexcept;
    void ReloadWeapons(std::size_t weaponDefinitionCount) noexcept;
    struct WeaponLoadout
    {
        std::array<std::size_t, weaponSlotCount> primary{
            invalidWeapon, invalidWeapon, invalidWeapon, invalidWeapon};
        std::array<std::uint32_t, weaponSlotCount> primaryCountCharge{};
        std::array<std::uint32_t, weaponSlotCount> primaryCurrentCharge{};
        std::size_t hyper = invalidWeapon;
        std::uint32_t hyperCountCharge = 0U;
        std::uint32_t hyperCurrentCharge = 0U;
        std::size_t mine = invalidWeapon;
        std::uint32_t mineCountCharge = 0U;
        std::uint32_t mineCurrentCharge = 0U;
    };
    void BindWeaponItems(
        std::span<const WeaponDefinition> definitions,
        const WeaponLoadout& loadout) noexcept;
    std::array<WeaponItem*, weaponSlotCount>
        GetPrimaryWeaponItems() noexcept;
    std::array<const WeaponItem*, weaponSlotCount>
        GetPrimaryWeaponItems() const noexcept;
    WeaponItem* GetHyperWeaponItem() noexcept;
    const WeaponItem* GetHyperWeaponItem() const noexcept;
    WeaponItem* GetMineWeaponItem() noexcept;
    const WeaponItem* GetMineWeaponItem() const noexcept;
    float ReflectDamage(float value) const noexcept;
    bool Shot(
        WeaponItem& item,
        std::span<const Weapon::ShotContext> contexts,
        bool mineSlot, std::uint32_t projectileId,
        int newCharge = -1,
        Weapon::ProjList* projectiles = nullptr);
    RockCar::Weapons& GetWeaponRack() noexcept;
    const RockCar::Weapons& GetWeaponRack() const noexcept;
    void BindSlots(
        const std::vector<OriginalWorkshopItem>& workshop,
        const std::vector<RacerSlot>& loadout);
    void SetSlot(
        PlayerSlotType type, const OriginalWorkshopItem* record,
        const std::array<float, 3>& position = {},
        const std::array<float, 4>& rotation =
            {0.0F, 0.0F, 0.0F, 1.0F}) noexcept;
    void ApplyMobility(Vehicle& vehicle, std::string_view difficulty,
                       bool humanOrOpponent,
                       bool armor4Opened = false) noexcept;
    const OriginalWorkshopItem* GetSlot(
        PlayerSlotType type) const noexcept;
    Slot* GetSlotInst(PlayerSlotType type) noexcept;
    const Slot* GetSlotInst(PlayerSlotType type) const noexcept;
    Slot* GetSlotInst(SlotType type) noexcept;
    const Slot* GetSlotInst(SlotType type) const noexcept;

    std::uint32_t GetMoney() const noexcept;
    void SetMoney(std::uint32_t value) noexcept;
    std::uint32_t GetPoints() const noexcept;
    void SetPoints(std::uint32_t value) noexcept;
    std::uint32_t GetPickMoney() const noexcept;
    void ResetPickMoney() noexcept;
    std::uint32_t GetPlace() const noexcept;
    void SetPlace(std::uint32_t value) noexcept;
    bool GetFinished() const noexcept;

    PlayerBonusResult TakeMoney(float value) noexcept;
    PlayerBonusResult TakeMedpack(float value) noexcept;
    PlayerBonusResult TakeImmortal(float value) noexcept;
    PlayerBonusResult TakeAmmunition(
        float value,
        const std::vector<std::uint32_t>& maximumCharges,
        float randomUnit) noexcept;
    PlayerBonusResult TakeBonus(
        PlayerBonusType type, float value,
        const std::vector<std::uint32_t>& maximumCharges,
        float randomUnit) noexcept;
    PlayerBonusResult TakeBonus(
        GameObject& bonus, PlayerBonusType type, float value,
        const std::vector<std::uint32_t>& maximumCharges,
        float randomUnit) noexcept;
    BehaviorProgressResult ProgressBehaviors(
        float deltaTime, float lowLifeLevel,
        float linearSpeed) noexcept;
    void PrepareBehaviors(
        float lowLifeLevel, float linearSpeed) noexcept;
    BehaviorProgressResult FinishBehaviorProgress(
        float deltaTime) noexcept;
    std::size_t GetVehicleDeathEffectBehaviorCount() const noexcept;
    std::vector<DeathEffect::SpawnResult>
        ConsumeVehicleDeathEffectSpawns() noexcept;
    bool AttachSlowEffect(
        const ObjectDefinition* effectDefinition,
        float maximumTimeLife, std::size_t weapon,
        std::size_t projectile) noexcept;
    std::optional<EventEffect::SpawnResult>
        ConsumeSlowEffectSpawn() noexcept;
    void NotifySlowEffectDestroyed() noexcept;
    CheatResult CheatUpdate(
        std::uint32_t cheatMask, std::size_t playerId,
        std::size_t difficulty,
        const std::vector<CheatPlayerView>& players) noexcept;
    ProgressResult OnProgress(
        float deltaTime, std::uint32_t cheatMask, std::size_t playerId,
        std::size_t difficulty,
        const std::vector<CheatPlayerView>& players);
    Player* FindClosestEnemy(
        float viewAngle, bool zTest,
        std::span<Player* const> players) noexcept;
    void InsertBonusProjectile(std::uint32_t projectileId);
    void InsertBonusProjectile(
        Proj* projectile, std::uint32_t projectileId);
    bool RemoveBonusProjectile(std::uint32_t projectileId) noexcept;
    void ClearBonusProjectiles() noexcept;
    bool HasBonusProjectile(std::uint32_t projectileId) const noexcept;
    Proj* GetBonusProjectile(std::uint32_t projectileId) noexcept;
    const Proj* GetBonusProjectile(
        std::uint32_t projectileId) const noexcept;
    std::uint32_t GetBonusProjectileId(
        const Proj* projectile) const noexcept;
    std::uint32_t GetNextBonusProjectileId() const noexcept;
    bool ConsumeEnergyDamageEffectCreated() noexcept;
    std::optional<EventEffect::SpawnResult>
        ConsumeEnergyDamageEffectSpawn() noexcept;
    std::vector<PlayerGameEvent> TakeGameEvents() noexcept;

    void SetFinished(bool value, float time = -1.0F) noexcept;
    void Complete(std::uint32_t resultPlace,
                  std::uint32_t resultMoney,
                  std::uint32_t resultPoints,
                  float time) noexcept;
    void AddMoney(std::int32_t value) noexcept;
    void AddPoints(std::int32_t value) noexcept;

    void Destroy() noexcept;
    PlayerRestoreStep ProgressRestore(float seconds);
    ResetCarPose ResetCar(const ResetCarRayCast& rayCast);
    void Disconnect() noexcept;
    void ResetBlock(bool block) noexcept;
    bool IsBlock() const noexcept;
    float GetBlockTime() const noexcept;
    void SetBlockTime(float value) noexcept;
    PlayerBlockMove ProgressBlock(float seconds) noexcept;
    float FinishBrake(float elapsedSeconds) const noexcept;

    static std::size_t RoundedRandomIndex(
        std::size_t count, float randomUnit) noexcept;
    static std::uint32_t BonusCharge(
        std::uint32_t maximumCharge, float value) noexcept;

    std::array<std::size_t, weaponSlotCount> weaponSlots{
        invalidWeapon, invalidWeapon, invalidWeapon, invalidWeapon};
    std::size_t hyperWeapon = invalidWeapon;
    std::size_t mineWeapon = invalidWeapon;
    std::uint32_t rewardMoney = 0;
    std::uint32_t rewardPoints = 0;
    float restoreSeconds = 0.0F;
    float finishTime = -1.0F;
    float blockSeconds = -1.0F;
    bool disconnected = false;
    LowLifePoints lowLifePoints;
    DamageEffect energyDamageEffect{DamageType::Energy, 0.5F};
    ImmortalEffect immortalEffect;
    SlowEffect slowEffect;
    RockCar gameCar;
    CarState car;

protected:
    void OnDestroy(GameObject& sender) noexcept override;
    void OnLowLife(
        GameObject& sender, Behavior* behavior) noexcept override;
    void OnDeath(
        GameObject& sender, DamageType damageType,
        GameObject* target) noexcept override;
    void OnRockCarDamageDispatch(
        std::size_t senderPlayerId, float value,
        DamageType damageType) noexcept override;
    void OnRockCarKillDispatch(
        std::size_t senderPlayerId, float value,
        DamageType damageType) noexcept override;

private:
    class LowLifeBehavior;
    class EnergyDamageBehavior;
    class PlayerImmortalBehavior;
    class SlowBehavior;
    void BindSourceBehaviors();
    void PrepareVehicleDeathEffects() noexcept;
    void ClearSlowBehavior() noexcept;
    void AttachWeaponMapObjects() noexcept;
    void DetachWeaponMapObjects() noexcept;
    void InitLight(
        std::size_t light, PresentationVector position,
        PresentationQuaternion rotation) noexcept;
    void FreeLight(std::size_t light) noexcept;
    void CreateNightLights(bool attach) noexcept;
    void SetLightsParent(bool attach) noexcept;
    void ApplyReflScene() noexcept;
    void FreeColorMaterial() noexcept;
    void ApplyColorMaterial() noexcept;
    void ApplyColor() noexcept;

    int id_ = undefinedId;
    int gamerId_ = -1;
    unsigned netSlot_ = defaultNetSlot;
    std::string name_;
    std::string netName_;
    std::array<float, 4> color_{1.0F, 1.0F, 1.0F, 1.0F};
    std::uint32_t cheatEnable_ = cheatDisabled;
    std::uint32_t money_ = 0U;
    std::uint32_t points_ = 0U;
    std::uint32_t pickedMoney_ = 0U;
    std::uint32_t place_ = 1U;
    bool finished_ = false;
    std::vector<DeathEffectBehavior*> vehicleDeathEffects_;
    HeadLightMode headLight_ = HeadLightMode::None;
    // Exact CarState::mapObj ownership. Map owns the allocation; Player
    // creates/deletes it in CreateCar/FreeCar and retains the live identity.
    Map* mapOwner_ = nullptr;
    MapObj* carMapObj_ = nullptr;
    std::size_t mapSourceIndex_ =
        std::numeric_limits<std::size_t>::max();
    // Standalone source-unit tests may run without a Map owner. This flag
    // retains their backend-neutral car graph while the active game always
    // uses carMapObj_ as well.
    bool carPresent_ = false;
    const Vehicle* carRecord_ = nullptr;
    const ObjectDefinition* wheelTrailEffect_ = nullptr;
    const ObjectDefinition* wheelSmokeEffect_ = nullptr;
    bool reflScene_ = true;
    PresentationState presentation_;
    std::optional<EventEffect::SpawnResult> energyDamageEffectSpawn_;
    bool lowLifeActivated_ = false;
    bool lowLifeReleased_ = false;
    std::optional<EventEffect::SpawnResult> lowLifeEffectSpawn_;
    float behaviorLinearSpeed_ = 0.0F;
    bool slowSpeedLimited_ = false;
    bool slowReleased_ = false;
    std::optional<EventEffect::SpawnResult> slowEffectSpawn_;
    std::vector<PlayerGameEvent> gameEvents_;
    struct BonusProjectileRef
    {
        Proj* projectile = nullptr;
        std::uint32_t id = 0U;
    };
    std::vector<BonusProjectileRef> bonusProjectiles_;
    std::uint32_t nextBonusProjectileId_ = 1U;
    PlayerSlotRack slotRack_;
};

} // namespace r3d::game::originalrace::source
