#pragma once

#include "OriginalGarage.h"
#include "OriginalRace.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace r3d::game::originalrace::source
{

// Serialized Slot::Type values from Player.h.  These values are part of the
// workshop database format and therefore must not be reordered.
enum class SlotType : std::uint8_t
{
    Base = 0U,
    Wheel = 1U,
    Truba = 2U,
    Armor = 3U,
    Motor = 4U,
    Hyper = 5U,
    Mine = 6U,
    Weapon = 7U,
    Droid = 8U,
    Reflector = 9U,
    Count = 10U,
};

class MobilityItem;

// Backend-neutral part of the original SlotItem.  Mesh/texture fields keep
// source resource identity; creation of bgfx actors stays at the renderer
// boundary.
class SlotItem
{
public:
    explicit SlotItem(SlotType type = SlotType::Base) noexcept;
    virtual ~SlotItem() = default;

    virtual MobilityItem* IsMobilityItem() noexcept;
    virtual const MobilityItem* IsMobilityItem() const noexcept;

    SlotType GetType() const noexcept;
    const std::string& GetRecord() const noexcept;
    virtual const std::string& GetName() const noexcept;
    virtual const std::string& GetInfo() const noexcept;
    std::uint32_t GetCost() const noexcept;
    virtual const std::string& GetMeshPath() const noexcept;
    virtual const std::string& GetTexturePath() const noexcept;
    const std::array<float, 3>& GetPos() const noexcept;
    const std::array<float, 4>& GetRot() const noexcept;

    virtual void Load(const OriginalWorkshopItem& item);

protected:
    SlotType type_ = SlotType::Base;
    std::string record_;
    std::string name_;
    std::string info_;
    std::uint32_t cost_ = 0U;
    std::string meshPath_;
    std::string texturePath_;
    std::array<float, 3> position_{};
    std::array<float, 4> rotation_{0.0F, 0.0F, 0.0F, 1.0F};
};

class MobilityItem : public SlotItem
{
public:
    struct Tire
    {
        float extremumSlip = 0.0F;
        float extremumValue = 0.0F;
        float asymptoteSlip = 0.0F;
        float asymptoteValue = 0.0F;
    };

    struct CarFunc
    {
        std::string car;
        Tire longTire;
        Tire latTire;
        float maxTorque = 0.0F;
        float life = 0.0F;
        float maxSpeed = 0.0F;
        float tireSpring = 0.0F;
    };

    explicit MobilityItem(SlotType type) noexcept;
    MobilityItem* IsMobilityItem() noexcept override;
    const MobilityItem* IsMobilityItem() const noexcept override;
    void Load(const OriginalWorkshopItem& item) override;
    const CarFunc* FindCarFunc(std::string_view car) const noexcept;
    virtual float CalcLife(const CarFunc& function) const noexcept;
    const std::vector<CarFunc>& GetCarFuncMap() const noexcept;

private:
    std::vector<CarFunc> carFuncMap_;
};

class WheelItem final : public MobilityItem
{
public:
    WheelItem() noexcept;
};

class TrubaItem final : public MobilityItem
{
public:
    TrubaItem() noexcept;
};

class MotorItem final : public MobilityItem
{
public:
    MotorItem() noexcept;
};

class ArmorItem final : public MobilityItem
{
public:
    ArmorItem() noexcept;

    void SetAchievementOpened(bool value) noexcept;
    void SetHuman(bool value) noexcept;
    bool CheckArmor4(bool ignorePlayers = false) const noexcept;
    bool IsArmor4Installed() const noexcept;
    void InstalArmor4(bool install) noexcept;

    const std::string& GetName() const noexcept override;
    const std::string& GetInfo() const noexcept override;
    const std::string& GetMeshPath() const noexcept override;
    const std::string& GetTexturePath() const noexcept override;
    float CalcLife(const CarFunc& function) const noexcept override;

private:
    bool armor4Installed_ = false;
    bool achievementOpened_ = false;
    bool human_ = false;
};

class Slot
{
public:
    Slot();
    Slot(Slot&&) noexcept = default;
    Slot& operator=(Slot&&) noexcept = default;
    Slot(const Slot&) = delete;
    Slot& operator=(const Slot&) = delete;

    SlotItem& CreateItem(SlotType type);
    void SetRecord(const OriginalWorkshopItem* record);
    SlotItem& GetItem() noexcept;
    const SlotItem& GetItem() const noexcept;
    const OriginalWorkshopItem* GetRecord() const noexcept;
    SlotType GetType() const noexcept;

private:
    const OriginalWorkshopItem* record_ = nullptr;
    std::unique_ptr<SlotItem> item_;
};

// Original Player owns one Slot for every serialized type.  This rack now
// owns selection and Player::ApplyMobility accumulation instead of leaving
// those rules in the XML loader.
class PlayerSlotRack
{
public:
    static constexpr std::size_t slotCount =
        static_cast<std::size_t>(SlotType::Count);

    void Bind(const std::vector<OriginalWorkshopItem>& workshop,
              const std::vector<RacerSlot>& loadout);
    void ApplyMobility(Vehicle& vehicle, std::string_view difficulty,
                       bool humanOrOpponent,
                       bool armor4Opened = false) noexcept;

    Slot& GetSlot(SlotType type) noexcept;
    const Slot& GetSlot(SlotType type) const noexcept;

private:
    std::array<Slot, slotCount> slots_{};
};

} // namespace r3d::game::originalrace::source
