#include "OriginalSlot.h"

#include <cmath>
#include <iostream>
#include <vector>

namespace original = r3d::game::originalrace;
namespace source = r3d::game::originalrace::source;

namespace
{
bool near(float first, float second)
{
    return std::abs(first - second) < 0.0001F;
}
}

int main()
{
    original::OriginalWorkshopItem wheel;
    wheel.record = "world\\race\\workshopRoot\\workshop\\wheel3";
    wheel.type = 1U;
    wheel.name = "scWheel3";
    wheel.info = "scWheel3Info";
    wheel.cost = 3000U;
    wheel.meshPath = "Data/Upgrade/wheel3.r3d";
    original::OriginalWorkshopItem::CarFunction wheelFunction;
    wheelFunction.car = "world\\db\\root\\ctCar\\buggi";
    wheelFunction.longitudinalTire = {0.5F, 7.0F, 3.0F, 6.9F};
    wheelFunction.lateralTire = {0.3F, 2.5F, 2.0F, 2.0F};
    wheelFunction.maximumSpeed = 45.0F;
    wheelFunction.tireSpring = 5.0F;
    wheel.carFunctions.push_back(wheelFunction);

    original::OriginalWorkshopItem motor;
    motor.record = "world\\race\\workshopRoot\\workshop\\motor3";
    motor.type = 4U;
    original::OriginalWorkshopItem::CarFunction motorFunction;
    motorFunction.car = wheelFunction.car;
    motorFunction.maximumTorque = 5100.0F;
    motor.carFunctions.push_back(motorFunction);

    original::OriginalWorkshopItem armor;
    armor.record = "world\\race\\workshopRoot\\workshop\\armor3";
    armor.type = 3U;
    armor.name = "scArmor3";
    armor.info = "scArmor3Info";
    armor.meshPath = "Data/Upgrade/armor3.r3d";
    armor.texturePath = "Data/Upgrade/armor3.dds";
    original::OriginalWorkshopItem::CarFunction armorFunction;
    armorFunction.car = wheelFunction.car;
    armorFunction.life = 40.0F;
    armor.carFunctions.push_back(armorFunction);

    original::OriginalWorkshopItem droid;
    droid.record = "world\\race\\workshopRoot\\workshop\\droid";
    droid.type = 8U;
    original::OriginalWorkshopItem reflector;
    reflector.record =
        "world\\race\\workshopRoot\\workshop\\reflector";
    reflector.type = 9U;

    std::vector<original::OriginalWorkshopItem> workshop{
        wheel, motor, armor, droid, reflector};
    std::vector<original::RacerSlot> loadout{
        {wheel.record, "stWheel", 0U},
        {motor.record, "stMotor", 0U},
        {armor.record, "stArmor", 0U},
        {droid.record, "stWeapon1", 1U},
        {reflector.record, "stWeapon3", 1U}};

    source::PlayerSlotRack rack;
    rack.Bind(workshop, loadout);
    if (rack.GetSlot(source::PlayerSlotType::Wheel).GetType() !=
            source::SlotType::Wheel ||
        rack.GetSlot(source::PlayerSlotType::Wheel)
                .GetItem().IsMobilityItem() == nullptr ||
        rack.GetSlot(source::PlayerSlotType::Weapon1).GetType() !=
            source::SlotType::Droid ||
        rack.GetSlot(source::PlayerSlotType::Weapon2).GetType() !=
            source::SlotType::Base ||
        rack.GetSlot(source::PlayerSlotType::Weapon3).GetType() !=
            source::SlotType::Reflector ||
        rack.GetSlotInst(source::SlotType::Droid) !=
            &rack.GetSlot(source::PlayerSlotType::Weapon1) ||
        rack.GetSlotInst(source::SlotType::Reflector) !=
            &rack.GetSlot(source::PlayerSlotType::Weapon3))
        return 1;

    original::Vehicle car;
    car.record = wheelFunction.car;
    car.physics.maximumSpeed = 20.0F;
    car.physics.tireSpring = 2.0F;
    car.physics.wheels.resize(4U);
    rack.ApplyMobility(car, "gdHard", true, false);
    if (!near(car.physics.maximumTorque, 5100.0F) ||
        !near(car.physics.maximumSpeed, 65.0F) ||
        !near(car.physics.tireSpring, 7.0F) ||
        !near(car.maximumLife, 60.0F) ||
        !near(car.physics.wheels.front()
                  .longitudinalTire.extremumValue, 7.0F) ||
        !near(car.physics.wheels.back()
                  .lateralTire.asymptoteValue, 2.0F))
        return 2;

    // Armor4 contributes ten before the hard difficulty coefficient.
    car.physics.maximumSpeed = 20.0F;
    car.physics.tireSpring = 2.0F;
    rack.ApplyMobility(car, "gdHard", true, true);
    if (!near(car.maximumLife, 75.0F))
        return 3;
    const auto& armorItem = dynamic_cast<const source::ArmorItem&>(
        rack.GetSlot(source::PlayerSlotType::Armor).GetItem());
    if (!armorItem.CheckArmor4() ||
        armorItem.GetName() != "scArmor4" ||
        armorItem.GetMeshPath() != "Data/Upgrade/armor4.r3d")
        return 4;

    // Computer armor is neither difficulty-scaled nor implicitly upgraded.
    car.physics.maximumSpeed = 20.0F;
    car.physics.tireSpring = 2.0F;
    rack.ApplyMobility(car, "gdEasy", false, true);
    if (!near(car.maximumLife, 40.0F))
        return 5;

    std::cout << "original Slot/SlotItem/MobilityItem source rules passed\n";
    return 0;
}
