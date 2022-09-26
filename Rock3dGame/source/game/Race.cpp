#include "stdafx.h"
#include "game/World.h"

#include "game/Race.h"
#include "lslSerialFileXml.h"

#ifdef _DEBUG
//#define DEBUG_WEAPON 1
#endif

namespace r3d
{
	namespace game
	{
		const float Race::cSellDiscount = 0.9f;
		const std::string Planet::cWorldTypeStr[cWorldTypeEnd] = {
			"wtWorld1", "wtWorld2", "wtWorld3", "wtWorld4", "wtWorld5", "wtWorld6"
		};


		Workshop::Workshop(Race* race): _race(race), _lib(nullptr)
		{
			_rootNode = new RootNode("workshopRoot", race);

			LoadLib();
		}

		Workshop::~Workshop()
		{
			ClearItems();
			DeleteSlots();

			delete _lib;
			delete _rootNode;
		}

		void Workshop::SaveSlot(Slot* slot, const std::string& name)
		{
			LSL_ASSERT(_lib->FindRecord(name) == 0);

			_lib->GetOrCreateRecord(name)->Save(slot);
			delete slot;
		}

		Slot* Workshop::AddSlot(Record* record)
		{
			auto slot = new Slot(nullptr);
			slot->AddRef();
			slot->SetRecord(record);
			_slots.push_back(slot);

			return slot;
		}

		void Workshop::DelSlot(Slots::const_iterator iter)
		{
			Slot* slot = *iter;
			_slots.erase(iter);
			slot->Release();
			delete slot;
		}

		void Workshop::DelSlot(Slot* slot)
		{
			DelSlot(_slots.Find(slot));
		}

		void Workshop::DeleteSlots()
		{
			while (!_slots.empty())
				DelSlot(_slots.begin());
		}

		void Workshop::LoadLib()
		{
			SafeDelete(_lib);

			try
			{
				SerialFileXML xml;
				xml.LoadNodeFromFile(*_rootNode, "workshop.xml");

				_lib = new RecordLib("workshop", _rootNode);
				_lib->SetOwner(_race);
				_lib->SrcSync();
			}
			catch (const EUnableToOpen&)
			{
				_lib = new RecordLib("workshop", _rootNode);
				_lib->SetOwner(_race);

				SerialFileXML xml;
				xml.SaveNodeToFile(*_rootNode, "workshop.xml");
			}

			DeleteSlots();
			for (auto iter = _lib->GetRecordList().begin(); iter != _lib->GetRecordList().end(); ++iter)
				AddSlot(*iter);
		}

		const RecordLib& Workshop::GetLib()
		{
			return *_lib;
		}

		Record& Workshop::GetRecord(const std::string& name)
		{
			Record* record = _lib->FindRecord(name);
			if (record == nullptr)
				throw Error("Slot record " + name + " does not exist");

			return *record;
		}

		Slot* Workshop::FindSlot(Record* record)
		{
			for (auto iter = _slots.begin(); iter != _slots.end(); ++iter)
				if ((*iter)->GetRecord() == record)
					return *iter;

			return nullptr;
		}

		void Workshop::InsertItem(Slot* slot)
		{
			_items.push_back(slot);
			slot->AddRef();
		}

		void Workshop::RemoveItem(Items::iterator iter)
		{
			Slot* slot = *iter;
			_items.erase(iter);
			slot->Release();
		}

		bool Workshop::RemoveItem(Slot* slot)
		{
			Items::iterator iter = _items.Find(slot);
			if (iter == _items.end())
				return false;

			RemoveItem(iter);
			return true;
		}

		void Workshop::ClearItems()
		{
			for (auto iter = _items.begin(); iter != _items.end(); ++iter)
				(*iter)->Release();

			_items.clear();
		}

		bool Workshop::BuyItem(Player* player, Slot* slot)
		{
			if (!_race->HasMoney(player, slot->GetItem().GetCost()))
				return false;
			//if (!RemoveItem(slot))
			//	return false;

			_race->BuyItem(player, slot->GetItem().GetCost());
			return true;
		}

		Slot* Workshop::BuyItem(Player* player, Record* slot)
		{
			Slot* item = FindSlot(slot);
			if (item && BuyItem(player, item))
				return item;
			return nullptr;
		}

		void Workshop::SellItem(Player* player, Slot* slot, bool sellDiscount, int chargeCount)
		{
			int cost = GetCostItem(slot, false, chargeCount);
			_race->SellItem(player, cost, sellDiscount);

			//InsertItem(slot);
		}

		void Workshop::SellItem(Player* player, Record* slot, bool sellDiscount, int chargeCount)
		{
			Slot* item = FindSlot(slot);
			if (item)
				SellItem(player, item, sellDiscount, chargeCount);
		}

		int Workshop::GetCostItem(Slot* slot, bool sellDiscount, int chargeCount)
		{
			WeaponItem* wpn = slot->GetItem().IsWeaponItem();
			if (chargeCount == -1 && wpn)
				chargeCount = wpn->GetCntCharge();

			return _race->GetSellCost(
				slot->GetItem().GetCost() + (wpn && chargeCount > 1 ? (chargeCount - 1) * wpn->GetChargeCost() : 0),
				sellDiscount);
		}

		bool Workshop::BuyChargeFor(Player* player, WeaponItem* slot)
		{
			unsigned chargeStep = std::min(slot->GetChargeStep(), slot->GetMaxCharge() - slot->GetCntCharge());

			if (chargeStep > 0)
			{
				if (!_race->BuyItem(player, slot->GetChargeCost() * chargeStep))
					return false;

				slot->SetCntCharge(slot->GetCntCharge() + chargeStep);
				return true;
			}

			return false;
		}

		Slot* Workshop::BuyUpgrade(Player* player, Record* slot)
		{
			Slot* item = FindSlot(slot);

			if (item && _race->BuyItem(player, item->GetItem().GetCost()))
				return item;

			return nullptr;
		}

		int Workshop::GetCost(Slot* slot) const
		{
			return _race->GetCost(slot->GetItem().GetCost());
		}

		int Workshop::GetSellCost(Slot* slot) const
		{
			return _race->GetSellCost(slot->GetItem().GetCost());
		}

		int Workshop::GetChargeCost(WeaponItem* slot) const
		{
			return _race->GetCost(slot->GetChargeCost());
		}

		int Workshop::GetChargeSellCost(WeaponItem* slot) const
		{
			return _race->GetSellCost(slot->GetChargeCost());
		}

		void Workshop::Reset()
		{
			ClearItems();
		}

		const Workshop::Items& Workshop::GetItems()
		{
			return _items;
		}

		const Workshop::Slots& Workshop::GetSlots()
		{
			return _slots;
		}

		Slot& Workshop::GetSlot(const std::string& name)
		{
			return *FindSlot(&GetRecord(name));
		}

		Race* Workshop::GetRace()
		{
			return _race;
		}


		Garage::Garage(Race* race, const std::string& name): _race(race), _upgradeMaxLevel(0), _selectedLevel(0)
		{
			SetName(name);
			SetOwner(race);

			LoadLib();
		}

		Garage::~Garage()
		{
			ClearItems();
			DeleteCars();
		}

		const Garage::PlaceItem* Garage::PlaceSlot::FindItem(Record* slot) const
		{
			for (auto iter = items.begin(); iter != items.end(); ++iter)
				if (iter->slot->GetRecord() == slot)
					return &(*iter);

			return nullptr;
		}

		const Garage::PlaceItem* Garage::PlaceSlot::FindItem(Slot* slot) const
		{
			for (auto iter = items.begin(); iter != items.end(); ++iter)
				if (iter->slot == slot)
					return &(*iter);

			return nullptr;
		}

		Garage::Car::Car(): _record(nullptr), _cost(0), _name(scNull), _desc(scNull), _initialUpgradeSet(0),
		                    _wheel(nullptr)
		{
		}

		Garage::Car::~Car()
		{
			ClearSlots();
			ClearBodies();

			SetWheel(nullptr);
			SetRecord(nullptr);
		}

		void Garage::Car::Assign(Car* ref)
		{
			ClearSlots();
			ClearBodies();

			SetWheel(ref->_wheel);
			SetRecord(ref->_record);

			SetCost(ref->_cost);
			SetName(ref->_name);
			SetDesc(ref->_desc);
			SetWheels(ref->_wheels);

			for (BodyMeshes::const_iterator iter = ref->_bodyMeshes.begin(); iter != ref->_bodyMeshes.end(); ++iter)
				AddBody(*iter);
			for (int i = 0; i < Player::cSlotTypeEnd; ++i)
				SetSlot(static_cast<Player::SlotType>(i), ref->_slot[i]);
		}

		void Garage::Car::ClearSlots()
		{
			for (int i = 0; i < Player::cSlotTypeEnd; ++i)
			{
				SetSlot(static_cast<Player::SlotType>(i), PlaceSlot());
			}
		}

		void Garage::Car::SaveTo(SWriter* writer, Garage* owner)
		{
			MapObjLib::SaveRecordRef(writer, "record", _record);

			SWriter* bodyMeshes = writer->NewDummyNode("bodyMeshes");
			int i = 0;
			for (auto iter = _bodyMeshes.begin(); iter != _bodyMeshes.end(); ++iter, ++i)
			{
				std::stringstream sstream;
				sstream << "bodyMesh" << i;
				SWriter* child = bodyMeshes->NewDummyNode(sstream.str().c_str());

				child->WriteValue("meshId", iter->meshId);
				child->WriteValue("decal", iter->decal);

				child->WriteRef("mesh", iter->mesh);
				child->WriteRef("texture", iter->texture);
			}

			writer->WriteValue("cost", _cost);

			SWriteValue(writer, "name", _name);
			SWriteValue(writer, "desc", _desc);

			writer->WriteValue("initialUpgradeSet", _initialUpgradeSet);

			writer->WriteRef("wheel", _wheel);
			writer->WriteValue("wheels", _wheels);

			for (int i = 0; i < Player::cSlotTypeEnd; ++i)
			{
				const PlaceSlot& slot = _slot[i];
				SWriter* child = writer->NewDummyNode(Player::cSlotTypeStr[i].c_str());

				child->WriteValue("active", slot.active);
				child->WriteValue("show", slot.show);
				child->WriteValue("lock", slot.lock);
				SWriteValue(child, "pos", slot.pos);
				RecordLib::SaveRecordRef(child, "defItem", slot.defItem ? slot.defItem->GetRecord() : nullptr);

				SWriter* items = child->NewDummyNode("items");
				int j = 0;
				for (auto iter = slot.items.begin(); iter != slot.items.end(); ++iter, ++j)
				{
					const PlaceItem& item = *iter;

					std::stringstream sstream;
					sstream << "item" << j;
					SWriter* child = items->NewDummyNode(sstream.str().c_str());

					RecordLib::SaveRecordRef(child, "record", item.slot ? item.slot->GetRecord() : nullptr);
					SWriteValue(child, "rot", item.rot);
					SWriteValue(child, "offset", item.offset);
				}
			}

			SWriter* nightLights = writer->NewDummyNode("nightLights");
			for (unsigned i = 0; i < _nightLights.size(); ++i)
			{
				std::stringstream sstream;
				sstream << "item" << i;
				SWriter* child = nightLights->NewDummyNode(sstream.str().c_str());

				child->WriteValue("head", _nightLights[i].head);
				SWriteValue(child, "pos", _nightLights[i].pos);
				SWriteValue(child, "size", _nightLights[i].size);
			}
		}

		void Garage::Car::LoadFrom(SReader* reader, Garage* owner)
		{
			ClearBodies();

			SetRecord(MapObjLib::LoadRecordRef(reader, "record"));

			SReader* bodyMeshes = reader->ReadValue("bodyMeshes");
			if (bodyMeshes)
			{
				SReader* child = bodyMeshes->FirstChildValue();
				while (child)
				{
					BodyMesh bodyMesh;

					child->ReadValue("meshId", bodyMesh.meshId);
					child->ReadValue("decal", bodyMesh.decal);

					FixUpName fixUpName;
					if (child->ReadRef("mesh", true, nullptr, &fixUpName))
						bodyMesh.mesh = fixUpName.GetCollItem<graph::IndexedVBMesh*>();

					if (child->ReadRef("texture", true, nullptr, &fixUpName))
						bodyMesh.texture = fixUpName.GetCollItem<graph::Tex2DResource*>();

					AddBody(bodyMesh);

					child = child->NextValue();
				}
			}

			reader->ReadValue("cost", _cost);

			SReadValue(reader, "name", _name);
			SReadValue(reader, "desc", _desc);

			reader->ReadValue("initialUpgradeSet", _initialUpgradeSet);

			FixUpName fixUpName;
			if (reader->ReadRef("wheel", true, nullptr, &fixUpName))
				SetWheel(fixUpName.GetCollItem<graph::IndexedVBMesh*>());

			reader->ReadValue("wheels", _wheels);

			for (int i = 0; i < Player::cSlotTypeEnd; ++i)
			{
				PlaceSlot slot;

				SReader* child = reader->ReadValue(Player::cSlotTypeStr[i].c_str());

				child->ReadValue("active", slot.active);
				child->ReadValue("show", slot.show);
				child->ReadValue("lock", slot.lock);
				SReadValue(child, "pos", slot.pos);

				Record* record = RecordLib::LoadRecordRef(child, "defItem");
				slot.defItem = record ? owner->_race->GetWorkshop().FindSlot(record) : nullptr;

				SReader* items = child->ReadValue("items");
				if (items)
				{
					SReader* child = items->FirstChildValue();
					while (child)
					{
						PlaceItem item;

						record = RecordLib::LoadRecordRef(child, "record");
						item.slot = record ? owner->_race->GetWorkshop().FindSlot(record) : nullptr;
						SReadValue(child, "rot", item.rot);
						SReadValue(child, "offset", item.offset);
						slot.items.push_back(item);

						child = child->NextValue();
					}
				}

				SetSlot(static_cast<Player::SlotType>(i), slot);
			}

			_nightLights.clear();
			SReader* nightLights = reader->ReadValue("nightLights");
			if (nightLights)
			{
				SReader* child = nightLights->FirstChildValue();
				while (child)
				{
					NightLight nightLight;

					child->ReadValue("head", nightLight.head);
					SReadValue(child, "pos", nightLight.pos);
					SReadValue(child, "size", nightLight.size);

					_nightLights.push_back(nightLight);

					child = child->NextValue();
				}
			}
		}

		MapObjRec* Garage::Car::GetRecord()
		{
			return _record;
		}

		void Garage::Car::SetRecord(MapObjRec* value)
		{
			if (ReplaceRef(_record, value))
				_record = value;
		}

		void Garage::Car::AddBody(const BodyMesh& body)
		{
			if (body.mesh)
				body.mesh->AddRef();
			if (body.texture)
				body.texture->AddRef();
			_bodyMeshes.push_back(body);
		}

		void Garage::Car::AddBody(graph::IndexedVBMesh* mesh, graph::Tex2DResource* texture, bool decal, int meshId)
		{
			BodyMesh body;
			body.mesh = mesh;
			body.texture = texture;
			body.decal = decal;
			body.meshId = meshId;
			AddBody(body);
		}

		void Garage::Car::ClearBodies()
		{
			for (BodyMeshes::const_iterator iter = _bodyMeshes.begin(); iter != _bodyMeshes.end(); ++iter)
			{
				if (iter->mesh)
					iter->mesh->Release();
				if (iter->texture)
					iter->texture->Release();
			}
			_bodyMeshes.clear();
		}

		const Garage::BodyMeshes& Garage::Car::GetBodies() const
		{
			return _bodyMeshes;
		}

		int Garage::Car::GetCost() const
		{
			return _cost;
		}

		void Garage::Car::SetCost(int value)
		{
			_cost = value;
		}

		const std::string& Garage::Car::GetName() const
		{
			return _name;
		}

		void Garage::Car::SetName(const std::string& value)
		{
			_name = value;
		}

		const std::string& Garage::Car::GetDesc() const
		{
			return _desc;
		}

		void Garage::Car::SetDesc(const std::string& value)
		{
			_desc = value;
		}

		int Garage::Car::GetInitialUpgradeSet() const
		{
			return _initialUpgradeSet;
		}

		void Garage::Car::SetInitialUpgradeSet(int value)
		{
			_initialUpgradeSet = value;
		}

		const Garage::Car::NightLights& Garage::Car::GetNightLights() const
		{
			return _nightLights;
		}

		void Garage::Car::SetNightLights(const NightLights& value)
		{
			_nightLights = value;
		}

		graph::IndexedVBMesh* Garage::Car::GetWheel()
		{
			return _wheel;
		}

		void Garage::Car::SetWheel(graph::IndexedVBMesh* value)
		{
			if (ReplaceRef(_wheel, value))
				_wheel = value;
		}

		const std::string& Garage::Car::GetWheels() const
		{
			return _wheels;
		}

		void Garage::Car::SetWheels(const std::string& value)
		{
			_wheels = value;
		}

		const Garage::PlaceSlot& Garage::Car::GetSlot(Player::SlotType type)
		{
			return _slot[type];
		}

		void Garage::Car::SetSlot(Player::SlotType type, const PlaceSlot& value)
		{
			SafeRelease(_slot[type].defItem);
			for (auto iter = _slot[type].items.begin(); iter != _slot[type].items.end(); ++iter)
				iter->slot->Release();

			_slot[type] = value;

			for (auto iter = _slot[type].items.begin(); iter != _slot[type].items.end(); ++iter)
			{
				LSL_ASSERT(iter->slot);

				iter->slot->AddRef();
			}
			if (_slot[type].defItem)
				_slot[type].defItem->AddRef();
		}

		Garage::Car* Garage::AddCar()
		{
			auto car = new Car();
			_cars.push_back(car);

			return car;
		}

		void Garage::DeleteCar(Cars::iterator iter)
		{
			Car* car = *iter;
			_cars.erase(iter);

			delete car;
		}

		void Garage::DeleteCar(Car* car)
		{
			DeleteCar(_cars.Find(car));
		}

		void Garage::DeleteCars()
		{
			for (auto iter = _cars.begin(); iter != _cars.end(); ++iter)
				delete *iter;

			_cars.clear();
		}

		void Garage::LoadLib()
		{
			DeleteCars();

			RootNode rootNode("garageRoot", _race);

			try
			{
				SerialFileXML xml;
				xml.LoadNodeFromFile(rootNode, "garage.xml");

				rootNode.Load(this);
			}
			catch (const EUnableToOpen&)
			{
				rootNode.Save(this);

				SerialFileXML xml;
				xml.SaveNodeToFile(rootNode, "garage.xml");
			}
		}

		Workshop& Garage::GetShop()
		{
			return _race->GetWorkshop();
		}

		void Garage::Save(SWriter* writer)
		{
			Logic* logic = _race->GetGame()->GetWorld()->GetLogic();

			SWriter* cars = writer->NewDummyNode("cars");
			int i = 0;
			for (auto iter = _cars.begin(); iter != _cars.end(); ++iter, ++i)
			{
				std::stringstream sstream;
				sstream << "car" << i;
				SWriter* child = cars->NewDummyNode(sstream.str().c_str());

				(*iter)->SaveTo(child, this);
			}

			SWriteValue(writer, "touchBorderDamage", logic->GetTouchBorderDamage());
			SWriteValue(writer, "touchBorderDamageForce", logic->GetTouchBorderDamageForce());
			SWriteValue(writer, "touchCarDamage", logic->GetTouchCarDamage());
			SWriteValue(writer, "touchCarDamageForce", logic->GetTouchCarDamageForce());
		}

		void Garage::Load(SReader* reader)
		{
			Logic* logic = _race->GetGame()->GetWorld()->GetLogic();

			DeleteCars();

			SReader* cars = reader->ReadValue("cars");
			if (cars)
			{
				SReader* child = cars->FirstChildValue();
				while (child)
				{
					Car* car = AddCar();
					car->LoadFrom(child, this);

					child = child->NextValue();
				}
			}

			D3DXVECTOR2 touchBorderDamage = NullVec2;
			D3DXVECTOR2 touchBorderDamageForce = NullVec2;
			D3DXVECTOR2 touchCarDamage = NullVec2;
			D3DXVECTOR2 touchCarDamageForce = NullVec2;

			SReadValue(reader, "touchBorderDamage", touchBorderDamage);
			SReadValue(reader, "touchBorderDamageForce", touchBorderDamageForce);
			SReadValue(reader, "touchCarDamage", touchCarDamage);
			SReadValue(reader, "touchCarDamageForce", touchCarDamageForce);

			logic->SetTouchBorderDamage(touchBorderDamage);
			logic->SetTouchBorderDamageForce(touchBorderDamageForce);
			logic->SetTouchCarDamage(touchCarDamage);
			logic->SetTouchCarDamageForce(touchCarDamageForce);
		}

		Garage::Cars::iterator Garage::FindCar(Car* car)
		{
			for (auto iter = _cars.begin(); iter != _cars.end(); ++iter)
				if (*iter == car)
					return iter;

			return _cars.end();
		}

		Garage::Car* Garage::FindCar(MapObjRec* record)
		{
			for (auto iter = _cars.begin(); iter != _cars.end(); ++iter)
				if ((*iter)->GetRecord() == record)
					return *iter;

			return nullptr;
		}

		Garage::Car* Garage::FindCar(const std::string& name)
		{
			return name != "" ? FindCar(_race->GetDB()->GetRecord(MapObjLib::ctCar, name)) : nullptr;
		}

		void Garage::InsertItem(Car* item)
		{
			_items.push_back(item);
			item->AddRef();
		}

		void Garage::RemoveItem(Items::const_iterator iter)
		{
			Car* item = *iter;
			_items.erase(iter);
			item->Release();
		}

		void Garage::RemoveItem(Car* item)
		{
			RemoveItem(_items.Find(item));
		}

		void Garage::ClearItems()
		{
			for (auto iter = _items.begin(); iter != _items.end(); ++iter)
				(*iter)->Release();

			_items.clear();
		}

		Garage::Items::iterator Garage::FindItem(Car* item)
		{
			for (auto iter = _items.begin(); iter != _items.end(); ++iter)
				if (*iter == item)
					return iter;

			return _items.end();
		}

		Slot* Garage::InstalSlot(Player* player, Player::SlotType type, Car* car, Slot* slot, int chargeCount)
		{
			LSL_ASSERT(car->GetRecord() == player->GetCar().record);

			D3DXVECTOR3 pos;
			D3DXQUATERNION rot = NullQuaternion;
			if (slot)
			{
				const PlaceSlot& place = car->GetSlot(type);
				const PlaceItem* placeItem = place.FindItem(slot);

				LSL_ASSERT(placeItem);

				pos = place.pos + placeItem->offset;
				rot = placeItem->rot;
			}

			player->SetSlot(type, slot ? slot->GetRecord() : nullptr, pos, rot);

			Slot* slotInst = player->GetSlotInst(type);

			if (slotInst)
			{
				WeaponItem* weapon = slotInst->GetItem().IsWeaponItem();
				if (weapon)
				{
					if (_race->IsSkirmish())
						weapon->SetCntCharge(weapon->GetMaxCharge());
					else
					{
						weapon->SetCntCharge(chargeCount != -1
							                     ? chargeCount
							                     : slot->GetItem<WeaponItem>().GetCntCharge());
					}
				}
			}

			return slotInst;
		}

		bool Garage::IsSlotSupported(Car* car, Player::SlotType type, Slot* slot)
		{
			const PlaceSlot& place = car->GetSlot(type);

			return place.FindItem(slot) != nullptr;
		}

		bool Garage::TestCompSlot(Car* car, Car* newCar, Player::SlotType type, Slot* slot)
		{
			const PlaceSlot& newPlace = newCar->GetSlot(type);
			if (!newPlace.active || newPlace.lock)
				return false;

			if (car)
			{
				const PlaceSlot& place = car->GetSlot(type);
				if (place.lock)
					return false;
			}

			bool isSupp = IsSlotSupported(newCar, type, slot);
			if (isSupp && newPlace.defItem && newPlace.defItem != slot && slot->GetItem().GetCost() < newPlace.defItem->
				GetItem().GetCost())
				return false;

			return isSupp;
		}

		void Garage::GetSupportedSlots(Player* player, Car* car, std::pair<Slot*, int> (&slots)[Player::cSlotTypeEnd],
		                               bool includeDef)
		{
			ZeroMemory(slots, sizeof(slots));

			Car* myCar = player->GetCar().record ? FindCar(player->GetCar().record) : nullptr;

			for (int i = 0; i < Player::cSlotTypeEnd; ++i)
			{
				if (slots[i].first != nullptr)
					continue;
				slots[i].second = -1;

				auto type = static_cast<Player::SlotType>(i);

				Record* slotRec = player->GetSlot(type);
				Slot* slot = slotRec ? _race->GetWorkshop().FindSlot(slotRec) : nullptr;
				if (slot == nullptr)
					continue;

				WeaponItem* weapon = player->GetSlotInst(type)->GetItem().IsWeaponItem();

				if (TestCompSlot(myCar, car, type, slot))
				{
					slots[i].first = slot;
					slots[i].second = weapon ? weapon->GetCntCharge() : -1;
				}
				else
				{
					for (int j = 0; j < Player::cSlotTypeEnd; ++j)
					{
						if (j == i)
							continue;

						type = static_cast<Player::SlotType>(j);
						if (TestCompSlot(myCar, car, type, slot))
						{
							slots[j].first = slot;
							slots[j].second = weapon ? weapon->GetCntCharge() : -1;
							break;
						}
					}
				}
			}

			if (includeDef)
				for (int i = 0; i < Player::cSlotTypeEnd; ++i)
				{
					auto type = static_cast<Player::SlotType>(i);
					const PlaceSlot& place = car->GetSlot(type);

					if (slots[i].first == nullptr && place.active)
						slots[i].first = place.defItem;
				}
		}

		Record* Garage::GetUpgradeCar(Car* car, Player::SlotType type, int level)
		{
			Record* slots[Player::cSlotTypeEnd][cUpgCntLevel];
			ZeroMemory(slots, sizeof(slots));

			Record* gusWheel1 = _race->GetSlot("gusWheel1");
			if (car->GetSlot(Player::stWheel).FindItem(gusWheel1))
			{
				slots[Player::stWheel][0] = gusWheel1;
				slots[Player::stWheel][1] = _race->GetSlot("gusWheel2");
				slots[Player::stWheel][2] = _race->GetSlot("gusWheel3");
				slots[Player::stWheel][3] = _race->GetSlot("gusWheel4");
			}
			else
			{
				slots[Player::stWheel][0] = _race->GetSlot("wheel1");
				slots[Player::stWheel][1] = _race->GetSlot("wheel2");
				slots[Player::stWheel][2] = _race->GetSlot("wheel3");
				slots[Player::stWheel][3] = _race->GetSlot("wheel4");
			}

			slots[Player::stTruba][0] = _race->GetSlot("truba1");
			slots[Player::stTruba][1] = _race->GetSlot("truba2");
			slots[Player::stTruba][2] = _race->GetSlot("truba3");
			slots[Player::stTruba][3] = _race->GetSlot("truba4");

			slots[Player::stTrans][0] = _race->GetSlot("trans1");
			slots[Player::stTrans][1] = _race->GetSlot("trans2");
			slots[Player::stTrans][2] = _race->GetSlot("trans3");
			slots[Player::stTrans][3] = _race->GetSlot("trans4");

			slots[Player::stArmor][0] = _race->GetSlot("armor1");
			slots[Player::stArmor][1] = _race->GetSlot("armor2");
			slots[Player::stArmor][2] = _race->GetSlot("armor3");
			slots[Player::stArmor][3] = _race->GetSlot("armor4");


			slots[Player::stMotor][0] = _race->GetSlot("engine1");
			slots[Player::stMotor][1] = _race->GetSlot("engine2");
			slots[Player::stMotor][2] = _race->GetSlot("engine3");
			slots[Player::stMotor][3] = _race->GetSlot("engine4");

			if (slots[type] && level >= 0 && level < cUpgCntLevel)
				return slots[type][level];
			return nullptr;
		}

		Slot* Garage::GetUpgradeCar(Car* car, Player* player, Player::SlotType type, int level)
		{
			if (car == nullptr)
				return nullptr;

			if (player && player->GetCar().record)
			{
				return _race->GetWorkshop().FindSlot(player->GetSlot(type));
			}
			if (level == -1)
			{
				//return _race->IsCampaign() ? car->GetSlot(type).defItem : _race->GetWorkshop().FindSlot(GetUpgradeCar(car, type, _upgradeMaxLevel));
				return car->GetSlot(type).defItem;
			}
			Record* record = GetUpgradeCar(car, type, level);

			return record ? _race->GetWorkshop().FindSlot(record) : nullptr;
		}

		int Garage::GetUpgradeCarLevel(Car* car, Player::SlotType type, Record* record)
		{
			for (int i = 0; i < cUpgCntLevel; ++i)
				if (GetUpgradeCar(car, type, i) == record)
					return i;
			return -1;
		}

		void Garage::UpgradeCar(Player* player, int level, bool instalMaxCharge)
		{
			Car* car = FindCar(player->GetCar().record);

			LSL_ASSERT(car);

			for (int i = 0; i < Player::cSlotTypeEnd; ++i)
			{
				auto type = static_cast<Player::SlotType>(i);
				Record* slotRec = GetUpgradeCar(car, type, level);

				if (slotRec)
				{
					Slot* slot = _race->GetWorkshop().FindSlot(slotRec);

					const PlaceSlot& place = car->GetSlot(type);
					const PlaceItem* placeItem = place.FindItem(slot);
					if (placeItem != nullptr)
						InstalSlot(player, type, car, slot);
				}

				if (instalMaxCharge)
				{
					Slot* slot = player->GetSlotInst(type);
					WeaponItem* weapon = slot ? slot->GetItem().IsWeaponItem() : nullptr;
					if (weapon)
						weapon->SetCntCharge(weapon->GetMaxCharge());
				}
			}
		}

		void Garage::MaxUpgradeCar(Player* player)
		{
			UpgradeCar(player, _upgradeMaxLevel, true);
		}

		float Garage::GetMobilitySkill(const MobilityItem::CarFunc& func)
		{
			return func.maxTorque + std::max(func.longTire.extremumValue - 5.0f, 0.0f) * 300.0f + std::max(
				func.latTire.extremumValue - 1.5f, 0.0f) * 2000.0f;
		}

		float Garage::GetMobilitySkill(Car* car, Player* player, Player::SlotType type, int level)
		{
			Slot* slot = GetUpgradeCar(car, player, type, level);
			if (slot)
			{
				MobilityItem* item = &slot->GetItem<MobilityItem>();

				MotorItem::CarFuncMap::const_iterator iter = item->carFuncMap.find(car->GetRecord());
				if (iter != item->carFuncMap.end())
					return GetMobilitySkill(iter->second);
			}

			return 0.0f;
		}

		float Garage::GetArmorSkill(Car* car, Player* player, float& armorVal, float& maxArmorVal)
		{
			armorVal = maxArmorVal = 0;

			if (car == nullptr)
				return 0.0f;

			float maxArmor = 0.0f;

			Slot* slot = GetUpgradeCar(car, nullptr, Player::stArmor, cUpgMaxLevel);
			LSL_ASSERT(slot);

			ArmorItem* armor = &slot->GetItem<ArmorItem>();

			for (ArmorItem::CarFuncMap::const_iterator iter = armor->carFuncMap.begin(); iter != armor->carFuncMap.end()
			     ; ++iter)
			{
				float life = armor->CalcLife(iter->second);
				if (maxArmor < life)
					maxArmor = life;
			}

			slot = GetUpgradeCar(car, player, Player::stArmor, -1);
			LSL_ASSERT(slot);

			armor = &slot->GetItem<ArmorItem>();

			ArmorItem::CarFuncMap::const_iterator iter = armor->carFuncMap.find(car->GetRecord());
			LSL_ASSERT(iter != armor->carFuncMap.end());

			armorVal = armor->CalcLife(iter->second);
			maxArmorVal = maxArmor;

			return maxArmorVal != 0 ? armorVal / maxArmor : 1.0f;
		}

		float Garage::GetDamageSkill(Car* car, Player* player, float& damageVal, float& maxDamageVal)
		{
			maxDamageVal = damageVal = 0;

			if (car == nullptr)
				return 0.0f;

			float maxDamage = 0.0f;

			for (Cars::const_iterator iter = _cars.begin(); iter != _cars.end(); ++iter)
			{
				Car* car = *iter;
				float damage = 0.0f;

				for (int i = Player::stWeapon1; i <= Player::stWeapon4; ++i)
				{
					const PlaceSlot& place = car->GetSlot(static_cast<Player::SlotType>(i));
					if (!place.active)
						continue;

					if (place.lock && place.defItem)
					{
						float dmg = place.defItem->GetItem<WeaponItem>().GetDamage(true);
						damage += dmg;
					}
					else if (!place.lock)
					{
						float maxWpnDamage = 0.0f;
						for (auto iterItem = place.items.begin(); iterItem != place.items.end(); ++iterItem)
						{
							float dmg = iterItem->slot->GetItem<WeaponItem>().GetDamage(true);
							if (maxWpnDamage < dmg)
								maxWpnDamage = dmg;
						}
						damage += maxWpnDamage;
					}
				}

				if (maxDamage < damage)
					maxDamage = damage;
			}

			float carDamage = 0.0f;

			for (int i = Player::stWeapon1; i <= Player::stWeapon4; ++i)
			{
				auto type = static_cast<Player::SlotType>(i);
				Slot* slot = nullptr;

				if (player)
					slot = player->GetSlotInst(type);
				else
					slot = car->GetSlot(static_cast<Player::SlotType>(i)).defItem;

				if (slot)
				{
					carDamage += slot->GetItem<WeaponItem>().GetDamage(true);
				}
			}

			damageVal = carDamage;
			maxDamageVal = maxDamage;

			return maxDamageVal != 0 ? damageVal / maxDamageVal : 1.0f;
		}

		float Garage::GetSpeedSkill(Car* car, Player* player)
		{
			if (car == nullptr)
				return 0.0f;

			float maxSpeed = 0.0f;
			for (Cars::const_iterator iter = _cars.begin(); iter != _cars.end(); ++iter)
			{
				float speed = 0.0f;

				speed += GetMobilitySkill(*iter, nullptr, Player::stMotor, cUpgMaxLevel);
				speed += GetMobilitySkill(*iter, nullptr, Player::stTruba, cUpgMaxLevel);
				speed += GetMobilitySkill(*iter, nullptr, Player::stWheel, cUpgMaxLevel);

				if (maxSpeed < speed)
					maxSpeed = speed;
			}

			float speed = 0.0f;
			speed += GetMobilitySkill(car, player, Player::stMotor, -1);
			speed += GetMobilitySkill(car, player, Player::stTruba, -1);
			speed += GetMobilitySkill(car, player, Player::stWheel, -1);

			return maxSpeed != 0 ? speed / maxSpeed : 1.0f;
		}

		int Garage::GetCarCost(Car* car)
		{
			return _race->GetCost(car->GetCost());
		}

		int Garage::GetCarSellCost(Player* player)
		{
			Car* car = FindCar(player->GetCar().record);

			return car ? _race->GetSellCost(car->GetCost()) : 0;
		}

		bool Garage::BuyCar(Player* player, Car* car)
		{
			LSL_ASSERT(player && car);

			Car* curCar = player->GetCar().record ? FindCar(player->GetCar().record) : nullptr;

			if (curCar == car)
				return true;

			if (!_race->BuyItem(player, car->GetCost()))
				return false;

			if (player->IsHuman() && _race->IsCampaign() && curCar != nullptr)
			{
				_race->SetCarChanged(true);
			}

			std::pair<Slot*, int> slots[Player::cSlotTypeEnd];
			GetSupportedSlots(player, car, slots, false);

			player->SetCar(car->GetRecord());

			for (int i = 0; i < Player::cSlotTypeEnd; ++i)
			{
				auto type = static_cast<Player::SlotType>(i);
				const PlaceSlot& place = car->GetSlot(type);
				Record* curSlot = player->GetSlot(type);

				if (slots[i].first)
				{
					InstalSlot(player, type, car, slots[i].first);

					if (slots[i].second >= 0)
						player->GetSlotInst(type)->GetItem().IsWeaponItem()->SetCntCharge(slots[i].second);
				}
				else if (place.active && place.defItem)
				{
					//if (!place.lock)
					//	_race->GetWorkshop().RemoveItem(place.defItem);
					InstalSlot(player, type, car, place.defItem);
				}
				else
				{
					player->SetSlot(type, nullptr);
				}

				Record* slot = player->GetSlot(type);

				if (curCar)
				{
					const PlaceSlot& curPlace = curCar->GetSlot(type);

					if (!curPlace.lock && curSlot && curSlot != slot)
					{
						bool isFind = false;
						for (int j = 0; j < Player::cSlotTypeEnd; ++j)
							if (slots[j].first && slots[j].first->GetRecord() == curSlot)
							{
								isFind = true;
								break;
							}
						if (!isFind)
							_race->GetWorkshop().SellItem(player, curSlot);
					}
				}
			}

			if (_race->IsSkirmish())
				MaxUpgradeCar(player);
			else if (car->GetInitialUpgradeSet() > 0)
				UpgradeCar(player, car->GetInitialUpgradeSet() - 1, false);

			return true;
		}

		void Garage::Reset()
		{
			ClearItems();
		}

		const Garage::Cars& Garage::GetCars() const
		{
			return _cars;
		}

		const Garage::Items& Garage::GetItems() const
		{
			return _items;
		}

		int Garage::GetUpgradeMaxLevel() const
		{
			return _upgradeMaxLevel;
		}

		void Garage::SetUpgradeMaxLevel(int value)
		{
			if (_upgradeMaxLevel != value)
			{
				_upgradeMaxLevel = value;

				if (_race->IsSkirmish())
				{
					for (auto iter = _race->GetPlayerList().begin(); iter != _race->GetPlayerList().end(); ++iter)
					{
						if ((*iter)->GetCar().record)
							_race->GetGarage().MaxUpgradeCar(*iter);
					}

					_race->SendEvent(cUpgradeMaxLevelChanged, nullptr);
				}
			}
		}

		int Garage::GetSelectedLevel() const
		{
			return _selectedLevel;
		}

		void Garage::SetSelectedLevel(int value)
		{
			_selectedLevel = value;
		}


		Planet::Planet(Race* race, int index): _race(race), _index(index), _name(scNull), _info(scNull),
		                                       _state(psUnavailable), _worldType(wtWorld1), _pass(-1), _mesh(nullptr),
		                                       _texture(nullptr)
		{
		}

		Planet::~Planet()
		{
			ClearCars();
			ClearSlots();

			SetTexture(nullptr);
			SetMesh(nullptr);
			ClearPlayers();

			ClearTracks();
		}

		int Planet::Track::GetIndex() const
		{
			return _index;
		}

		Planet* Planet::Track::GetPlanet()
		{
			return _planet;
		}

		unsigned Planet::Track::GetLapsCount()
		{
			return _planet->_race->IsCampaign() ? numLaps : _planet->_race->GetTournament().GetLapsCount();
		}

		float Planet::Track::GetMMAngle()
		{
			return mmAngle;
		}

		float Planet::Track::GetGravityK()
		{
			return gravityK;
		}

		bool Planet::Track::Minimap()
		{
			return minimap;
		}

		bool Planet::Track::CamLock()
		{
			return camlock;
		}

		unsigned Planet::Track::GetPrefCam()
		{
			return prefCam;
		}

		bool Planet::Track::AiJumpEnabled()
		{
			return aiJump;
		}

		void Planet::StartPass(int pass, Player* player)
		{
			//обрабатываем только события старта этапов на планете, потому что далее все старое состояние игроков сбрасывается
			if (pass <= 0)
				return;

			Players::const_iterator iter = _players.end();

			if (player->IsComputer())
			{
				for (Players::const_iterator plrIter = _players.begin(); plrIter != _players.end(); ++plrIter)
				{
					int id = player->GetId();

					if (id > Race::cComputer5)
					{
						if (_race->IsCampaign())
							id = (player->GetId() - Race::cComputer1) % (Race::cComputerCount - 1) + Race::cComputer2;
						else
							id = (player->GetId() - Race::cComputer1) % Race::cComputerCount + Race::cComputer1;
					}

					if (plrIter->id == id)
					{
						iter = plrIter;
						break;
					}
				}
			}

			if (iter != _players.end())
			{
				//if (pass > iter->maxPass || _race->IsSkirmish())
				//	pass = iter->maxPass;
				if (pass > iter->maxPass)
					pass = iter->maxPass;

				//cleanup last state
				player->SetCar(nullptr);
				for (int i = 0; i < Player::cSlotTypeEnd; ++i)
					player->SetSlot(static_cast<Player::SlotType>(i), nullptr);

				for (auto iterCar = iter->cars.begin(); iterCar != iter->cars.end(); ++iterCar)
					if (iterCar->pass == pass)
					{
						player->SetCar(iterCar->record);
						break;
					}

				Garage::Car* car = player->GetCar().record
					                   ? _race->GetGarage().FindCar(player->GetCar().record)
					                   : nullptr;

				for (auto iterSlot = iter->slots.begin(); iterSlot != iter->slots.end(); ++iterSlot)
				{
					Slot* slot = _race->GetWorkshop().FindSlot(iterSlot->record);
					if (iterSlot->pass == pass && car && slot)
					{
						_race->GetGarage().InstalSlot(player, iterSlot->type, car, slot);
						if (player->GetSlotInst(iterSlot->type) && player->GetSlotInst(iterSlot->type)->GetItem().
						                                                   IsWeaponItem())
							player->GetSlotInst(iterSlot->type)->GetItem().IsWeaponItem()->SetCntCharge(
								iterSlot->charge);
					}
				}

				if (_race->IsSkirmish() && player->GetCar().record)
				{
					_race->GetGarage().MaxUpgradeCar(player);

					for (int i = Garage::cWeaponCurLevel; i < Garage::cWeaponMaxLevel; ++i)
						player->SetSlot(static_cast<Player::SlotType>(i + Player::stWeapon1), nullptr);
				}
			}
		}

		void Planet::StartPass(int pass)
		{
			for (auto iter = _race->GetPlayerList().begin(); iter != _race->GetPlayerList().end(); ++iter)
				StartPass(pass, *iter);
		}

		void Planet::CompletePass(int pass)
		{
			for (Slots::const_iterator iter = _slots.begin(); iter != _slots.end(); ++iter)
				if (iter->pass == pass)
					_race->GetWorkshop().InsertItem(_race->GetWorkshop().FindSlot(iter->record));

			for (Cars::const_iterator iter = _cars.begin(); iter != _cars.end(); ++iter)
				if (iter->pass == pass)
					_race->GetGarage().InsertItem(_race->GetGarage().FindCar(iter->record));
		}

		void Planet::SaveSlots(SWriter* writer, const std::string& name, Slots& mSlots, Tournament* owner)
		{
			SWriter* slots = writer->NewDummyNode(name.c_str());
			int i = 0;
			for (auto iter = mSlots.begin(); iter != mSlots.end(); ++iter, ++i)
			{
				std::stringstream sstream;
				sstream << "slots" << i;
				SWriter* child = slots->NewDummyNode(sstream.str().c_str());

				RecordLib::SaveRecordRef(child, "record", iter->record);
				child->WriteValue("charge", iter->charge);
				if (iter->type != Player::cSlotTypeEnd)
					SWriteEnum(child, "type", iter->type, Player::cSlotTypeStr, Player::cSlotTypeEnd);
				child->WriteValue("pass", iter->pass);
			}
		}

		void Planet::LoadSlots(SReader* reader, const std::string& name, Slots& mSlots, Tournament* owner)
		{
			SReader* slots = reader->ReadValue(name.c_str());
			if (slots)
			{
				SReader* child = slots->FirstChildValue();
				while (child)
				{
					SlotData slot;

					slot.record = RecordLib::LoadRecordRef(child, "record");
					child->ReadValue("charge", slot.charge);
					SReadEnum(child, "type", slot.type, Player::cSlotTypeStr, Player::cSlotTypeEnd);
					child->ReadValue("pass", slot.pass);

					mSlots.push_back(slot);

					child = child->NextValue();
				}
			}
		}

		void Planet::SaveCars(SWriter* writer, const std::string& name, Cars& mCars, Tournament* owner)
		{
			SWriter* cars = writer->NewDummyNode(name.c_str());
			int i = 0;
			for (auto iter = mCars.begin(); iter != mCars.end(); ++iter, ++i)
			{
				std::stringstream sstream;
				sstream << "car" << i;
				SWriter* child = cars->NewDummyNode(sstream.str().c_str());

				MapObjLib::SaveRecordRef(child, "record", iter->record);
				child->WriteValue("pass", iter->pass);
			}
		}

		void Planet::LoadCars(SReader* reader, const std::string& name, Cars& mCars, Tournament* owner)
		{
			SReader* cars = reader->ReadValue(name.c_str());
			if (cars)
			{
				SReader* child = cars->FirstChildValue();
				while (child)
				{
					CarData car;

					car.record = MapObjLib::LoadRecordRef(child, "record");
					child->ReadValue("pass", car.pass);

					mCars.push_back(car);

					child = child->NextValue();
				}
			}
		}

		Planet::Track* Planet::AddTrack(int pass)
		{
			auto iter = _trackMap.find(pass);
			if (iter == _trackMap.end())
				iter = _trackMap.insert(iter, TrackMap::value_type(pass, Tracks()));

			auto track = new Track(this);
			iter->second.push_back(track);
			track->_index = iter->second.size() - 1;

			_trackList.push_back(track);

			return track;
		}

		void Planet::ClearTracks()
		{
			for (TrackMap::const_iterator iterMap = _trackMap.begin(); iterMap != _trackMap.end(); ++iterMap)
				for (auto iter = iterMap->second.begin(); iter != iterMap->second.end(); ++iter)
					delete *iter;

			_trackMap.clear();
			_trackList.clear();
		}

		Planet::Track* Planet::NextTrack(Track* track)
		{
			const Tracks& tracks = GetTracks();
			Tracks::const_iterator iter2 = tracks.Find(track);

			return iter2 != --(tracks.end()) ? *(++iter2) : nullptr;
		}

		Planet::TrackMap::const_iterator Planet::GetTracks(int pass) const
		{
			int maxPass = 0;

			for (auto iter = _trackMap.begin(); iter != _trackMap.end(); ++iter)
				if (maxPass <= iter->first)
					maxPass = iter->first;

			pass = abs(pass - 1) % maxPass + 1;

			auto iter = _trackMap.find(pass);
			if (iter != _trackMap.end())
				return iter;

			return _trackMap.begin();
		}

		const Planet::Tracks& Planet::GetTracks() const
		{
			LSL_ASSERT(_trackMap.size() > 0);

			if (_race->IsCampaign())
				return GetTracks(_pass)->second;
			return _trackList;
		}

		const Planet::TrackMap& Planet::GetTrackMap() const
		{
			return _trackMap;
		}

		const Planet::Tracks& Planet::GetTrackList() const
		{
			return _trackList;
		}

		void Planet::Unlock()
		{
			if (_state == psUnavailable || _state == psCompleted)
				SetState(psClosed);
		}

		bool Planet::Open()
		{
			if (_state == psUnavailable)
				return false;
			if (_state == psOpen || _state == psCompleted)
				return true;

			SetState(psOpen);
			return true;
		}

		bool Planet::Complete()
		{
			if (_state == psCompleted)
				return true;

			if (_state == psOpen)
			{
				SetState(psCompleted);
				return true;
			}

			return false;
		}

		void Planet::StartPass(Player* player)
		{
			StartPass(_pass, player);
		}

		void Planet::StartPass()
		{
			StartPass(_pass);
		}

		void Planet::NextPass()
		{
			SetPass(_pass + 1);

			if (_pass >= 3)
				Complete();
		}

		int Planet::GetPass() const
		{
			return _pass;
		}

		void Planet::SetPass(int value)
		{
			if (_pass != value)
			{
				if (_pass >= 0)
					CompletePass(_pass);

				_pass = value;

				if (_pass >= 0)
					StartPass(_pass);
			}
		}

		void Planet::Reset()
		{
			_state = psUnavailable;
			_pass = 0;
		}

		void Planet::SaveTo(SWriter* writer, Tournament* owner)
		{
			SWriteValue(writer, "name", _name);
			SWriteValue(writer, "info", _info);
			SWriteEnum(writer, "worldType", _worldType, cWorldTypeStr, cWorldTypeEnd);
			writer->WriteRef("mesh", _mesh);
			writer->WriteRef("texture", _texture);

			int i = 0;
			int j = 0;
			SWriter* trackMap = writer->NewDummyNode("trackMap");
			for (TrackMap::const_iterator iterMap = _trackMap.begin(); iterMap != _trackMap.end(); ++iterMap, ++i)
			{
				const Tracks& trackList = iterMap->second;

				SWriter* tracks = trackMap->NewDummyNode(StrFmt("tracks%d", i).c_str());
				tracks->WriteAttr("pass", iterMap->first);

				for (auto iter = trackList.begin(); iter != trackList.end(); ++iter, ++j)
				{
					Track* track = *iter;

					SWriter* child = tracks->NewDummyNode(StrFmt("track%d", j).c_str());

					child->WriteValue("level", track->level);
					child->WriteValue("numLaps", track->numLaps);
					child->WriteValue("mmAngle", track->mmAngle);
					child->WriteValue("gravityK", track->gravityK);
					child->WriteValue("minimap", track->minimap);
					child->WriteValue("camlock", track->camlock);
					child->WriteValue("prefCam", track->prefCam);
					child->WriteValue("aiJump", track->aiJump);
				}
			}

			SWriter* points = writer->NewDummyNode("points");
			i = 0;
			for (auto iter = _requestPoints.begin(); iter != _requestPoints.end(); ++iter, ++i)
			{
				std::stringstream sstream;
				sstream << "point" << i;
				SWriter* child = points->NewDummyNode(sstream.str().c_str());

				child->WriteValue("place", iter->first);
				child->WriteValue("value", iter->second);
			}

			SWriter* wheaters = writer->NewDummyNode("wheaters");
			i = 0;
			for (auto iter = _wheaters.begin(); iter != _wheaters.end(); ++iter, ++i)
			{
				std::stringstream sstream;
				sstream << "wheater" << i;
				SWriter* child = wheaters->NewDummyNode(sstream.str().c_str());

				SWriteEnum(child, "type", iter->type, Environment::cWheaterStr, Environment::cWheaterEnd);
				child->WriteValue("chance", iter->chance);
			}

			SWriter* prices = writer->NewDummyNode("prices");
			i = 0;
			for (auto iter = _prices.begin(); iter != _prices.end(); ++iter, ++i)
			{
				std::stringstream sstream;
				sstream << "price" << i;
				SWriter* child = prices->NewDummyNode(sstream.str().c_str());

				child->WriteValue("money", iter->money);
				child->WriteValue("points", iter->points);
			}

			SaveSlots(writer, "slots", _slots, owner);
			SaveCars(writer, "cars", _cars, owner);

			SWriter* players = writer->NewDummyNode("players");
			i = 0;
			for (auto iter = _players.begin(); iter != _players.end(); ++iter, ++i)
			{
				std::stringstream sstream;
				sstream << "player" << i;
				SWriter* child = players->NewDummyNode(sstream.str().c_str());

				child->WriteValue("id", iter->id);
				SWriteValue(child, "name", iter->name);
				SWriteValue(child, "bonus", iter->bonus);
				child->WriteRef("photo", iter->photo);
				child->WriteValue("maxPass", iter->maxPass);

				SaveCars(child, "cars", iter->cars, owner);
				SaveSlots(child, "slots", iter->slots, owner);
			}
		}

		void Planet::LoadFrom(SReader* reader, Tournament* owner)
		{
			_requestPoints.clear();
			_wheaters.clear();
			_prices.clear();
			ClearCars();
			ClearSlots();
			ClearPlayers();
			ClearTracks();

			SReadValue(reader, "name", _name);
			SReadValue(reader, "info", _info);
			SReadEnum(reader, "worldType", _worldType, cWorldTypeStr, cWorldTypeEnd);

			Serializable::FixUpName fixUpName;
			if (reader->ReadRef("mesh", true, nullptr, &fixUpName))
				SetMesh(fixUpName.GetCollItem<graph::IndexedVBMesh*>());

			if (reader->ReadRef("texture", true, nullptr, &fixUpName))
				SetTexture(fixUpName.GetCollItem<graph::Tex2DResource*>());

			SReader* trackMap = reader->ReadValue("trackMap");
			if (trackMap)
			{
				SReader* childMap = trackMap->FirstChildValue();
				while (childMap)
				{
					int pass = 1;
					const SerialNode::ValueDesc* val = childMap->ReadAttr("pass");
					if (val)
						val->CastTo<int>(&pass);

					//#ifdef _DEBUG
					//			{
					//				Track* track = AddTrack(pass);
					//				track->level = "Data\\Map\\debugTrack.r3dMap";
					//				track->numLaps = 99;
					//			}
					//#endif

					SReader* child = childMap->FirstChildValue();
					while (child)
					{
						Track* track = AddTrack(pass);

						child->ReadValue("level", track->level);
						child->ReadValue("numLaps", track->numLaps);
						child->ReadValue("mmAngle", track->mmAngle);
						child->ReadValue("gravityK", track->gravityK);
						child->ReadValue("minimap", track->minimap);
						child->ReadValue("camlock", track->camlock);
						child->ReadValue("prefCam", track->prefCam);
						child->ReadValue("aiJump", track->aiJump);

						child = child->NextValue();
					}

					childMap = childMap->NextValue();
				}
			}

			SReader* points = reader->ReadValue("points");
			if (points)
			{
				SReader* child = points->FirstChildValue();
				while (child)
				{
					int place;
					int value;

					child->ReadValue("place", place);
					child->ReadValue("value", value);

					_requestPoints.insert(RequestPoints::value_type(place, value));

					child = child->NextValue();
				}
			}

			SReader* wheaters = reader->ReadValue("wheaters");
			if (wheaters)
			{
				SReader* child = wheaters->FirstChildValue();
				while (child)
				{
					Wheater wheater;

					SReadEnum(child, "type", wheater.type, Environment::cWheaterStr, Environment::cWheaterEnd);
					child->ReadValue("chance", wheater.chance);

					_wheaters.push_back(wheater);

					child = child->NextValue();
				}
			}

			SReader* prices = reader->ReadValue("prices");
			if (prices)
			{
				SReader* child = prices->FirstChildValue();
				while (child)
				{
					Price price;

					child->ReadValue("money", price.money);
					child->ReadValue("points", price.points);

					_prices.push_back(price);

					child = child->NextValue();
				}
			}

			Slots slots;
			LoadSlots(reader, "slots", slots, owner);
			SetSlots(slots);

			Cars cars;
			LoadCars(reader, "cars", cars, owner);
			SetCars(cars);

			SReader* players = reader->ReadValue("players");
			if (players)
			{
				SReader* child = players->FirstChildValue();
				while (child)
				{
					PlayerData player;

					child->ReadValue("id", player.id);
					child->ReadValue("bonusSpeed", player.bonusSpeed);
					child->ReadValue("bonusRPM", player.bonusRPM);
					child->ReadValue("bonusTorque", player.bonusTorque);
					child->ReadValue("SEM", player.SEM);
					child->ReadValue("idlingRPM", player.idlingRPM);
					child->ReadValue("brakeTorque", player.brakeTorque);
					child->ReadValue("restTorque", player.restTorque);
					child->ReadValue("gearDiff", player.gearDiff);
					child->ReadValue("armorBonus", player.armorBonus);
					child->ReadValue("weaponBonus", player.weaponBonus);
					child->ReadValue("mineBonus", player.mineBonus);
					child->ReadValue("springBonus", player.springBonus);
					child->ReadValue("nitroBonus", player.nitroBonus);
					child->ReadValue("doubleJump", player.doubleJump);
					child->ReadValue("jumpPower", player.jumpPower);
					child->ReadValue("cooldown", player.cooldown);
					child->ReadValue("fixRecovery", player.fixRecovery);
					child->ReadValue("driftStrength", player.driftStrength);
					child->ReadValue("blowDamage", player.blowDamage);
					child->ReadValue("hardBorders", player.hardBorders);
					child->ReadValue("borderCluth", player.borderCluth);
					child->ReadValue("masloDrop", player.masloDrop);
					child->ReadValue("stabilityMine", player.stabilityMine);
					child->ReadValue("stabilityShot", player.stabilityShot);

					SReadValue(child, "name", player.name);
					SReadValue(child, "bonus", player.bonus);
					child->ReadValue("maxPass", player.maxPass);

					if (child->ReadRef("photo", true, nullptr, &fixUpName))
						player.photo = fixUpName.GetCollItem<graph::Tex2DResource*>();

					LoadCars(child, "cars", player.cars, owner);
					LoadSlots(child, "slots", player.slots, owner);

					InsertPlayer(player);

					child = child->NextValue();
				}
			}

			//deprecated block
			{
				SReader* tracks = reader->ReadValue("tracks");
				if (tracks)
				{
					SReader* child = tracks->FirstChildValue();
					while (child)
					{
						Track* track = AddTrack(1);

						child->ReadValue("level", track->level);
						child->ReadValue("numLaps", track->numLaps);
						child->ReadValue("mmAngle", track->mmAngle);
						child->ReadValue("gravityK", track->gravityK);
						child->ReadValue("minimap", track->minimap);
						child->ReadValue("camlock", track->camlock);
						child->ReadValue("prefCam", track->prefCam);
						child->ReadValue("aiJump", track->aiJump);

						child = child->NextValue();
					}
				}
			}
		}

		int Planet::GetIndex() const
		{
			return _index;
		}

		int Planet::GetId() const
		{
			return GetBoss().id;
		}

		const std::string& Planet::GetName() const
		{
			return _name;
		}

		void Planet::SetName(const std::string& value)
		{
			_name = value;
		}

		const std::string& Planet::GetInfo() const
		{
			return _info;
		}

		void Planet::SetInfo(const std::string& value)
		{
			_info = value;
		}

		const Planet::RequestPoints& Planet::GetRequestPoints() const
		{
			return _requestPoints;
		}

		void Planet::SetRequestPoints(const RequestPoints& value)
		{
			_requestPoints = value;
		}

		int Planet::GetRequestPoints(int pass) const
		{
			int points = 0;

			auto iter = _requestPoints.find(pass);
			if (iter != _requestPoints.end())
				points = iter->second;
			else if (_requestPoints.size() > 0)
				points = _requestPoints.rbegin()->second;
			else
				return -1;

			return points;
		}

		bool Planet::HasRequestPoints(int pass, int points) const
		{
			int req = GetRequestPoints(pass);
			if (req == -1)
				return false;

			return points >= req;
		}

		Planet::State Planet::GetState() const
		{
			return _state;
		}

		void Planet::SetState(State value)
		{
			if (_state == value)
				return;

			switch (value)
			{
			case psClosed:
				SetPass(0);
				break;

			case psOpen:
				SetPass(1);
			}

			_state = value;
		}

		Planet::WorldType Planet::GetWorldType() const
		{
			return _worldType;
		}

		void Planet::SetWorldType(WorldType value)
		{
			_worldType = value;
		}

		Planet::Wheater Planet::GenerateWheater(bool allowNight, bool mostProbable) const
		{
			auto maxWheater = Wheater(Environment::ewFair, 0);
			float maxChance = 0.0f;
			float summChance = 0.0f;

			for (auto iter = _wheaters.begin(); iter != _wheaters.end(); ++iter)
			{
				if (iter->type == Environment::ewNight && !allowNight)
					continue;

				if (maxChance < iter->chance)
				{
					maxChance = iter->chance;
					maxWheater = *iter;
				}

				summChance += iter->chance;
			}

			if (mostProbable)
				return maxWheater;

			float chance = summChance * Random();
			summChance = 0;

			for (auto iter = _wheaters.begin(); iter != _wheaters.end(); ++iter)
			{
				if (iter->type == Environment::ewNight && !allowNight)
					continue;

				if (chance >= summChance && chance <= summChance + iter->chance)
					return *iter;

				summChance += iter->chance;
			}

			return !_wheaters.empty() ? _wheaters.front() : maxWheater;
		}

		void Planet::SetWheaters(const Wheaters& wheaters)
		{
			_wheaters = wheaters;
		}

		Planet::Price Planet::GetPrice(int place) const
		{
			if (place >= 1 && static_cast<unsigned>(place) <= _prices.size())
				return _prices[place - 1];

			return Price(0, 0);
		}

		void Planet::SetPrices(const Prices& prices)
		{
			_prices = prices;
		}

		graph::IndexedVBMesh* Planet::GetMesh()
		{
			return _mesh;
		}

		void Planet::SetMesh(graph::IndexedVBMesh* value)
		{
			if (ReplaceRef(_mesh, value))
				_mesh = value;
		}

		graph::Tex2DResource* Planet::GetTexture()
		{
			return _texture;
		}

		void Planet::SetTexture(graph::Tex2DResource* value)
		{
			if (ReplaceRef(_texture, value))
				_texture = value;
		}

		void Planet::InsertSlot(const SlotData& slot)
		{
			if (slot.record)
				slot.record->AddRef();
			_slots.push_back(slot);
		}

		void Planet::InsertSlot(Record* slot, int pass)
		{
			SlotData data;
			data.record = slot;
			data.pass = pass;

			InsertSlot(data);
		}

		void Planet::InsertSlot(const string& name, int pass)
		{
			InsertSlot(&_race->GetWorkshop().GetRecord(name), pass);
		}

		void Planet::ClearSlots()
		{
			for (Slots::const_iterator iter = _slots.begin(); iter != _slots.end(); ++iter)
				iter->record->Release();
			_slots.clear();
		}

		void Planet::SetSlots(const Slots& value)
		{
			ClearSlots();

			for (auto iter = value.begin(); iter != value.end(); ++iter)
				InsertSlot(*iter);
		}

		void Planet::InsertCar(const CarData& car)
		{
			if (car.record)
				car.record->AddRef();
			_cars.push_back(car);
		}

		void Planet::InsertCar(MapObjRec* car, int pass)
		{
			CarData data;
			data.record = car;
			data.pass = pass;

			InsertCar(data);
		}

		void Planet::InsertCar(const string& name, int pass)
		{
			InsertCar(_race->GetGarage().FindCar(name)->GetRecord(), pass);
		}

		void Planet::SetCars(const Cars& value)
		{
			ClearCars();

			for (auto iter = value.begin(); iter != value.end(); ++iter)
				InsertCar(*iter);
		}

		void Planet::ClearCars()
		{
			for (Cars::const_iterator iter = _cars.begin(); iter != _cars.end(); ++iter)
				iter->record->Release();
			_cars.clear();
		}

		Planet::Cars::const_iterator Planet::FindCar(Garage::Car* car)
		{
			for (Cars::const_iterator iter = _cars.begin(); iter != _cars.end(); ++iter)
			{
				if (iter->record == car->GetRecord())
					return iter;
			}

			return _cars.end();
		}

		const Planet::Cars& Planet::GetCars() const
		{
			return _cars;
		}

		void Planet::InsertPlayer(const PlayerData& data)
		{
			if (data.photo)
				data.photo->AddRef();
			for (auto iter = data.cars.begin(); iter != data.cars.end(); ++iter)
				iter->record->AddRef();
			for (auto iter = data.slots.begin(); iter != data.slots.end(); ++iter)
				iter->record->AddRef();

			_players.push_back(data);
		}

		void Planet::ClearPlayers()
		{
			for (auto iter = _players.begin(); iter != _players.end(); ++iter)
			{
				if (iter->photo)
					iter->photo->Release();

				for (Cars::const_iterator iterCar = iter->cars.begin(); iterCar != iter->cars.end(); ++iterCar)
					iterCar->record->Release();
				iter->cars.clear();

				for (Slots::const_iterator iterSlot = iter->slots.begin(); iterSlot != iter->slots.end(); ++iterSlot)
					iterSlot->record->Release();
				iter->slots.clear();
			}
			_players.clear();
		}

		const Planet::PlayerData* Planet::GetPlayer(int id) const
		{
			for (auto iter = _players.begin(); iter != _players.end(); ++iter)
				if (iter->id == id)
					return &(*iter);

			return nullptr;
		}

		const Planet::PlayerData* Planet::GetPlayer(const std::string& name) const
		{
			for (auto iter = _players.begin(); iter != _players.end(); ++iter)
				if (iter->name == name)
					return &(*iter);

			return nullptr;
		}

		Planet::PlayerData Planet::GetBoss() const
		{
			return _players.size() > 0 ? _players.front() : PlayerData();
		}


		Tournament::Tournament(Race* race, const std::string& name): _race(race), _curPlanet(nullptr),
		                                                             _curTrack(nullptr), _wheater(Environment::ewFair),
		                                                             _wheaterNightPass(false), _lapsCount(4)
		{
			SetName(name);
			SetOwner(race);

			LoadLib();
		}

		Tournament::~Tournament()
		{
			SetCurTrack(nullptr);
			SetCurPlanet(nullptr);

			ClearGamers();
			ClearPlanets();
		}

		void Tournament::LoadPlanets()
		{
		}

		void Tournament::LoadGamers()
		{
		}

		void Tournament::LoadLib()
		{
			ClearGamers();
			ClearPlanets();

			RootNode rootNode("tournametRoot", _race);

			try
			{
				//для каждой сложности - свой турнамент.
				//для каждой сложности - своя длительность турнира (количество планет).
				SerialFileXML xml;
				if (GAME_DIFF == 0)
				{
					xml.LoadNodeFromFile(rootNode, "tournament1.xml");
				}
				else if (GAME_DIFF == 1)
				{
					xml.LoadNodeFromFile(rootNode, "tournament2.xml");
				}
				else if (GAME_DIFF == 2)
				{
					xml.LoadNodeFromFile(rootNode, "tournament3.xml");
				}
				else if (GAME_DIFF == 3)
				{
					xml.LoadNodeFromFile(rootNode, "tournament4.xml");
				}

				rootNode.Load(this);
			}
			catch (const EUnableToOpen&)
			{
				LoadGamers();
				LoadPlanets();
				rootNode.Save(this);

				SerialFileXML xml;
				if (GAME_DIFF == 0)
					xml.SaveNodeToFile(rootNode, "tournament1.xml");
				else if (GAME_DIFF == 1)
					xml.SaveNodeToFile(rootNode, "tournament2.xml");
				else if (GAME_DIFF == 2)
					xml.SaveNodeToFile(rootNode, "tournament3.xml");
				else if (GAME_DIFF == 3)
					xml.SaveNodeToFile(rootNode, "tournament4.xml");
			}
		}

		void Tournament::Save(SWriter* writer)
		{
			SWriter* planets = writer->NewDummyNode("planets");
			int i = 0;
			for (auto iter = _planets.begin(); iter != _planets.end(); ++iter, ++i)
			{
				std::stringstream sstream;
				sstream << "planet" << i;
				SWriter* child = planets->NewDummyNode(sstream.str().c_str());

				(*iter)->SaveTo(child, this);
			}

			SWriter* gamers = writer->NewDummyNode("gamers");
			i = 0;
			for (auto iter = _gamers.begin(); iter != _gamers.end(); ++iter, ++i)
			{
				std::stringstream sstream;
				sstream << "gamer" << i;
				SWriter* child = gamers->NewDummyNode(sstream.str().c_str());

				(*iter)->SaveTo(child, this);
			}
		}

		void Tournament::Load(SReader* reader)
		{
			ClearGamers();
			ClearPlanets();

			SReader* planets = reader->ReadValue("planets");
			if (planets)
			{
				SReader* child = planets->FirstChildValue();
				while (child)
				{
					Planet* planet = AddPlanet();
					planet->LoadFrom(child, this);

					child = child->NextValue();
				}
			}

			SReader* gamers = reader->ReadValue("gamers");
			if (gamers)
			{
				SReader* child = gamers->FirstChildValue();
				while (child)
				{
					Planet* gamer = AddGamer();
					gamer->LoadFrom(child, this);

					child = child->NextValue();
				}
			}
		}

		Planet* Tournament::AddPlanet()
		{
			auto planet = new Planet(_race, _planets.size());
			planet->AddRef();
			_planets.push_back(planet);

			return planet;
		}

		void Tournament::ClearPlanets()
		{
			for (Planets::const_iterator iter = _planets.begin(); iter != _planets.end(); ++iter)
			{
				(*iter)->Release();
				delete (*iter);
			}
			_planets.clear();
		}

		Tournament::Planets::iterator Tournament::FindPlanet(Planet* planet)
		{
			for (auto iter = _planets.begin(); iter != _planets.end(); ++iter)
				if (*iter == planet)
					return iter;

			return _planets.end();
		}

		Planet* Tournament::FindPlanet(const std::string& name)
		{
			for (auto iter = _planets.begin(); iter != _planets.end(); ++iter)
				if ((*iter)->GetName() == name)
					return *iter;

			return nullptr;
		}

		Planet* Tournament::NextPlanet(Planet* planet)
		{
			auto iter = FindPlanet(planet);

			if (iter != _planets.end() && iter != --_planets.end())
				return *(++iter);

			return nullptr;
		}

		Planet* Tournament::PrevPlanet(Planet* planet)
		{
			auto iter = FindPlanet(planet);

			if (iter != _planets.begin())
				return *(--iter);

			return nullptr;
		}

		Planet* Tournament::GetPlanet(int index)
		{
			if (index >= 0 && static_cast<unsigned>(index) < _planets.size())
				return _planets[index];
			return nullptr;
		}

		Planet* Tournament::AddGamer()
		{
			auto gamer = new Planet(_race, _gamers.size());
			gamer->AddRef();
			_gamers.push_back(gamer);

			return gamer;
		}

		void Tournament::DelGamer(Planets::const_iterator iter)
		{
			Planet* gamer = *iter;
			_gamers.erase(iter);
			gamer->Release();
			delete gamer;
		}

		void Tournament::ClearGamers()
		{
			while (!_gamers.empty())
				DelGamer(_gamers.begin());
		}

		Tournament::Planets::iterator Tournament::FindGamer(Planet* gamer)
		{
			for (auto iter = _gamers.begin(); iter != _gamers.end(); ++iter)
				if (*iter == gamer)
					return iter;

			return _gamers.end();
		}

		Planet* Tournament::GetGamer(int gamerId)
		{
			for (auto iter = _gamers.begin(); iter != _gamers.end(); ++iter)
				if ((*iter)->GetId() == gamerId)
					return *iter;

			return nullptr;
		}

		void Tournament::CompleteTrack(int points, bool& passComplete, bool& passChampion, bool& planetChampion)
		{
			LSL_ASSERT(_curPlanet);

			passComplete = false;
			passChampion = false;
			planetChampion = false;

			Planet::Track* nextTrack = NextTrack(_curTrack);

			if (nextTrack == nullptr)
			{
				passComplete = true;

				if (_curPlanet->HasRequestPoints(_curPlanet->GetPass(), points))
				{
					_curPlanet->NextPass();

					passChampion = true;
					planetChampion = _curPlanet->GetState() == Planet::psCompleted;
				}

				nextTrack = NextTrack(nullptr);
				LSL_ASSERT(nextTrack != NULL);
			}
			SetCurTrack(nextTrack);
		}

		bool Tournament::ChangePlanet(Planet* planet)
		{
			if (planet == _curPlanet)
				return true;

			//try open
			if (!planet->Open())
				planet->SetPass(1);
			//set anyway
			SetCurPlanet(planet);

			return true;
		}

		Planet& Tournament::GetCurPlanet()
		{
			LSL_ASSERT(_curPlanet);

			return *_curPlanet;
		}

		int Tournament::GetCurPlanetIndex()
		{
			LSL_ASSERT(_curPlanet);

			return _planets.Find(_curPlanet) - _planets.begin();
		}

		void Tournament::SetCurPlanet(Planet* value)
		{
			if (ReplaceRef(_curPlanet, value))
			{
				_trackList.clear();
				_curPlanet = value;

				if (_curPlanet)
				{
					SetCurTrack(NextTrack(nullptr));
					_curPlanet->StartPass();
				}
				else
					SetCurTrack(nullptr);
			}
		}

		Planet* Tournament::GetNextPlanet()
		{
			return NextPlanet(_curPlanet);
		}

		Planet::Track& Tournament::GetCurTrack()
		{
			LSL_ASSERT(_curTrack);

			return *_curTrack;
		}

		int Tournament::GetCurTrackIndex()
		{
			LSL_ASSERT(_curTrack);

			return _curTrack->GetPlanet()->GetTracks().Find(_curTrack) - _curTrack->GetPlanet()->GetTracks().begin();
		}

		void Tournament::SetCurTrack(Planet::Track* value)
		{
			if (ReplaceRef(_curTrack, value))
			{
				_curTrack = value;
				if (_curPlanet)
				{
					_wheater = _curPlanet->GenerateWheater(
						_race->GetWorld()->GetEnv()->GetLightQuality() >= Environment::eqMiddle && !_wheaterNightPass,
						!_race->IsTutorialCompleted()).type;
					_wheaterNightPass |= _wheater == Environment::ewNight;
				}
			}
		}

		const Planet::Tracks& Tournament::GetTrackList() const
		{
			return _trackList;
		}

		Planet::Track* Tournament::NextTrack(Planet::Track* track)
		{
			LSL_ASSERT(_curPlanet);

			Planet::Track* nextTrack = nullptr;

			if (_race->IsCampaign())
			{
				if (track != nullptr)
				{
					nextTrack = _curPlanet->NextTrack(track);
				}
				else
				{
					const Planet::Tracks& tracks = _curPlanet->GetTracks();
					LSL_ASSERT(tracks.size() > 0);

					nextTrack = tracks.front();
					_wheaterNightPass = false;
				}
			}
			else
			{
				if (_trackList.size() == 0)
				{
					for (auto iter = _curPlanet->GetTrackMap().begin(); iter != _curPlanet->GetTrackMap().end(); ++iter)
					{
						if (iter->second.size() > 0)
						{
							_trackList.insert(_trackList.end(), iter->second.begin(), iter->second.end());
							std::random_shuffle(_trackList.end() - iter->second.size(), _trackList.end());
						}
					}

					LSL_ASSERT(_trackList.size() > 0);

					_wheaterNightPass = false;
				}
				//0 - это случайная трасса
				if (_race->GetGame()->SelectedLevel() == 0)
					nextTrack = _trackList.front();
				else
					nextTrack = _race->GetTournament().GetCurPlanet().GetTracks()[_race->GetGame()->SelectedLevel() -
						1];
				_trackList.erase(_trackList.begin());
			}

			return nextTrack;
		}

		Environment::Wheater Tournament::GetWheater()
		{
			return _wheater;
		}

		void Tournament::SetWheater(Environment::Wheater value)
		{
			_wheater = value;
		}

		unsigned Tournament::GetLapsCount() const
		{
			return _lapsCount;
		}

		void Tournament::SetLapsCount(unsigned value)
		{
			_lapsCount = value;
		}

		const Planet::PlayerData* Tournament::GetPlayerData(int id) const
		{
			for (auto iter = _gamers.begin(); iter != _gamers.end(); ++iter)
			{
				const Planet::PlayerData* plrData = (*iter)->GetPlayer(id);
				if (plrData)
					return plrData;
			}

			return _curPlanet ? _curPlanet->GetPlayer(id) : nullptr;
		}

		const Planet::PlayerData* Tournament::GetPlayerData(const std::string& name) const
		{
			for (auto iter = _gamers.begin(); iter != _gamers.end(); ++iter)
			{
				const Planet::PlayerData* plrData = (*iter)->GetPlayer(name);
				if (plrData)
					return plrData;
			}

			return _curPlanet ? _curPlanet->GetPlayer(name) : nullptr;
		}

		Player* Tournament::GetBossPlayer()
		{
			return _race->GetPlayerById(Race::cComputer1);
		}

		void Tournament::Reset()
		{
			for (Planets::const_iterator iter = _planets.begin(); iter != _planets.end(); ++iter)
				(*iter)->Reset();

			SetCurPlanet(nullptr);
		}

		const Tournament::Planets& Tournament::GetPlanets() const
		{
			return _planets;
		}

		const Tournament::Planets& Tournament::GetGamers() const
		{
			return _gamers;
		}


		Race::Race(GameMode* game, const std::string& name): _game(game), _profile(nullptr), _lastProfile(nullptr),
		                                                     _lastNetProfile(nullptr), _startRace(false),
		                                                     _myCarIndex(1), _myWorldType(1), _weatherType(1),
		                                                     _aiColors(0), _humanColor(1), _maxAiCount(5),
		                                                     _nickName("NEO"), _maxQuality(false), _goRace(false),
		                                                     _planetChampion(false), _passChampion(false),
		                                                     _lastLeadPlace(0), _lastThirdPlace(0), _carChanged(false),
		                                                     _minDifficulty(cDifficultyEnd), _tutorialStage(0),
		                                                     _weaponUpgrades(false), _survivalMode(false),
		                                                     _autoCamera(false), _subjectView(0), _devMode(false),
		                                                     _camLock(false), _staticCam(false), _camFov(3),
		                                                     _camProection(1), _oilDestroyer(true),
		                                                     _enableMineBug(true), _human(nullptr), _aiSystem(nullptr)
		{
			SetName(name);
			SetOwner(game->GetWorld());

			LSL_LOG("race create workshop");

			_workshop = new Workshop(this);

			LSL_LOG("race create garage");

			_garage = new Garage(this, "garage");

			LSL_LOG("race create tournament");

			_tournament = new Tournament(this, "tournament");

			LSL_LOG("race create achievment");

			_achievment = new AchievmentModel(this, "achievment");

			LSL_LOG("race create aiSystem");

			_aiSystem = new AISystem(this);

			LSL_LOG("race create profiles");

			_skProfile = new SkProfile(this, "skirmish");
			_snClientProfile = new SnProfile(this, "championshipClient");

			LSL_LOG("race loadlib");

			LoadLib();

			LSL_LOG("race load New Config");

			LoadCfg();
		}

		Race::~Race()
		{
			ExitRace();
			ExitProfile();

			SetLastProfile(nullptr);
			SetLastNetProfile(nullptr);
			ClearProfiles();
			ClearAIPlayers();
			FreeHuman();
			ClearPlayerList();

			delete _snClientProfile;
			delete _skProfile;

			delete _aiSystem;

			delete _achievment;
			delete _tournament;
			delete _garage;
			delete _workshop;
		}

		Race::Profile::Profile(Race* race, const std::string& name): _race(race), _name(name), _netGame(false),
		                                                             _difficulty(gdNormal)
		{
		}

		void Race::Profile::Enter()
		{
			Reset();

			EnterGame();
		}

		void Race::Profile::Reset()
		{
			_race->_results.clear();
			_race->ResetChampion();

			_race->_workshop->Reset();
			_race->_garage->Reset();
			_race->_tournament->Reset();

			_race->FreeHuman();
			_race->ClearAIPlayers();
			_race->ClearPlayerList();
		}

		void Race::Profile::SaveGame(std::ostream& stream)
		{
			RootNode node("profile", _race);

			node.BeginSave();
			SWriter* writer = &node;

			SaveGame(writer);
			SWriteEnum(writer, "dfficulty", _difficulty, cDifficultyStr, cDifficultyEnd);

			node.EndSave();

			SerialFileXML file;
			file.SaveNode(node, stream);
		}

		void Race::Profile::LoadGame(std::istream& stream)
		{
			Reset();

			RootNode node("profile", _race);
			SerialFileXML file;
			file.LoadNode(node, stream);

			node.BeginLoad();
			SReader* reader = &node;

			LoadGame(&node);
			SReadEnum(reader, "dfficulty", _difficulty, cDifficultyStr, cDifficultyEnd);

			node.EndLoad();
		}

		void Race::Profile::SaveGameFile()
		{
			std::ostream* stream = FileSystem::GetInstance()->NewOutStream(
				"Profile\\" + _name + ".xml", FileSystem::omText, 0);
			SaveGame(*stream);
			FileSystem::GetInstance()->FreeStream(stream);
		}

		void Race::Profile::LoadGameFile()
		{
			std::istream* stream = nullptr;
			try
			{
				try
				{
					stream = FileSystem::GetInstance()->
						NewInStream("Profile\\" + _name + ".xml", FileSystem::omText, 0);
					LoadGame(*stream);
				}
				catch (const EUnableToOpen&)
				{
				}
			}
			LSL_FINALLY(FileSystem::GetInstance()->FreeStream(stream);)
		}

		const std::string& Race::Profile::GetName() const
		{
			return _name;
		}

		bool Race::Profile::netGame() const
		{
			return _netGame;
		}

		void Race::Profile::netGame(bool value)
		{
			_netGame = value;
		}

		Difficulty Race::Profile::difficulty() const
		{
			return _difficulty;
		}

		void Race::Profile::difficulty(Difficulty value)
		{
			_difficulty = value;
		}

		Race::SnProfile::SnProfile(Race* race, const std::string& name): _MyBase(race, name)
		{
		}

		void Race::SnProfile::SaveWorkshop(SWriter* writer)
		{
			Workshop& shop = _race->GetWorkshop();

			SWriter* slots = writer->NewDummyNode("slots");
			int i = 0;
			for (auto iter = shop.GetItems().begin(); iter != shop.GetItems().end(); ++iter, ++i)
			{
				std::stringstream stream;
				stream << "slot" << i;

				RecordLib::SaveRecordRef(slots, stream.str(), (*iter)->GetRecord());
			}
		}

		void Race::SnProfile::LoadWorkshop(SReader* reader)
		{
			Workshop& shop = _race->GetWorkshop();
			shop.ClearItems();

			SReader* slots = reader->ReadValue("slots");
			SReader* slot = slots ? slots->FirstChildValue() : nullptr;
			while (slot)
			{
				Record* record = RecordLib::LoadRecordRefFrom(slot);
				if (Slot* item = shop.FindSlot(record))
					shop.InsertItem(item);

				slot = slot->NextValue();
			}
		}

		void Race::SnProfile::SaveGarage(SWriter* writer)
		{
			Garage& shop = _race->GetGarage();

			SWriter* cars = writer->NewDummyNode("cars");
			int i = 0;
			for (auto iter = shop.GetItems().begin(); iter != shop.GetItems().end(); ++iter, ++i)
			{
				MapObjLib::SaveRecordRef(cars, StrFmt("car%d", i).c_str(), (*iter)->GetRecord());
			}
		}

		void Race::SnProfile::LoadGarage(SReader* reader)
		{
			Garage& shop = _race->GetGarage();
			shop.ClearItems();

			SReader* cars = reader->ReadValue("cars");
			SReader* car = cars ? cars->FirstChildValue() : nullptr;
			while (car)
			{
				MapObjRec* record = MapObjLib::LoadRecordRefFrom(car);
				if (Garage::Car* item = shop.FindCar(record))
					shop.InsertItem(item);

				car = car->NextValue();
			}
		}

		void Race::SnProfile::SaveTournament(SWriter* writer)
		{
			Tournament& tournament = _race->GetTournament();

			for (unsigned i = 0; i < tournament.GetPlanets().size(); ++i)
			{
				SWriter* planet = writer->NewDummyNode(StrFmt("planet%d", i).c_str());
				planet->WriteValue("state", tournament.GetPlanets()[i]->GetState());
				planet->WriteValue("pass", tournament.GetPlanets()[i]->GetPass());
			}

			writer->WriteValue("planet", tournament.GetCurPlanetIndex());
			writer->WriteValue("track", tournament.GetCurTrackIndex());
		}

		void Race::SnProfile::LoadTournament(SReader* reader)
		{
			Tournament& tournament = _race->GetTournament();
			int ind;

			for (unsigned i = 0; i < tournament.GetPlanets().size(); ++i)
			{
				int v;
				SReader* planet = reader->ReadValue(StrFmt("planet%d", i).c_str());
				if (planet)
				{
					if (planet->ReadValue("state", v))
						tournament.GetPlanets()[i]->SetState(static_cast<Planet::State>(v));
					if (planet->ReadValue("pass", v))
						tournament.GetPlanets()[i]->SetPass(v);
				}
			}

			if (reader->ReadValue("planet", ind))
			{
				Planet* planet = tournament.GetPlanets()[ind];
				tournament.SetCurPlanet(planet);

				if (planet && reader->ReadValue("track", ind))
					tournament.SetCurTrack(planet->GetTracks()[ind]);
			}
		}

		void Race::SnProfile::SavePlayer(Player* player, SWriter* writer)
		{
			MapObjLib::SaveRecordRef(writer, "car", player->GetCar().record);
			writer->WriteValue("plrId", player->GetId());
			writer->WriteValue("gamerId", player->GetGamerId());
			writer->WriteValue("netSlot", player->GetNetSlot());
			SWriteValue(writer, "color", player->GetColor());
			writer->WriteValue("money", player->GetMoney());
			writer->WriteValue("points", player->GetPoints());
			writer->WriteValue("killstotal", player->GetKillsTotal());
			writer->WriteValue("deadstotal", player->GetDeadsTotal());
			writer->WriteValue("trial", player->GetPassTrial());

			for (int i = 0; i < Player::cSlotTypeEnd; ++i)
			{
				Slot* slot = player->GetSlotInst(static_cast<Player::SlotType>(i));
				WeaponItem* wpn = slot ? slot->GetItem().IsWeaponItem() : nullptr;

				SWriter* node = RecordLib::SaveRecordRef(writer, StrFmt("slot%d", i).c_str(),
				                                         slot ? slot->GetRecord() : nullptr);
				if (wpn && node)
				{
					node->WriteAttr("charge", wpn->GetCntCharge());
				}
			}
		}

		void Race::SnProfile::LoadPlayer(Player* player, SReader* reader)
		{
			string str;
			FixUpName fixUp;
			D3DXCOLOR color;
			int numb;

			MapObjRec* carRef = MapObjLib::LoadRecordRef(reader, "car");
			Garage::Car* car = _race->GetGarage().FindCar(carRef);
			player->SetCar(carRef);

			if (reader->ReadValue("plrId", numb))
			{
				player->SetId(numb);

				if (player->IsHuman())
					_race->CreateHuman(player);
			}

			if (reader->ReadValue("gamerId", numb))
				player->SetGamerId(numb);

			if (reader->ReadValue("netSlot", numb))
				player->SetNetSlot(numb);

			if (SReadValue(reader, "color", color))
				player->SetColor(color);
			if (reader->ReadValue("money", numb))
				player->SetMoney(numb);
			if (reader->ReadValue("points", numb))
				player->SetPoints(numb);
			if (reader->ReadValue("killstotal", numb))
				player->SetKillsTotal(numb);
			if (reader->ReadValue("deadstotal", numb))
				player->SetDeadsTotal(numb);
			if (reader->ReadValue("trial", numb))
				player->SetPassTrial(numb);

			for (int i = 0; i < Player::cSlotTypeEnd; ++i)
			{
				SReader* node = reader->ReadValue(StrFmt("slot%d", i).c_str());
				if (node)
				{
					Record* slotRef = RecordLib::LoadRecordRefFrom(node);
					Slot* slot = _race->GetWorkshop().FindSlot(slotRef);
					const SReader::ValueDesc* desc = node ? node->ReadAttr("charge") : nullptr;

					if (car && slot)
					{
						Slot* inst = _race->GetGarage().InstalSlot(player, static_cast<Player::SlotType>(i), car, slot);

						WeaponItem* wpn = inst ? inst->GetItem().IsWeaponItem() : nullptr;
						if (wpn && desc)
						{
							int v;
							desc->CastTo<int>(&v);
							inst->GetItem().IsWeaponItem()->SetCntCharge(v);
						}
					}
				}
			}
		}

		void Race::SnProfile::SaveHumans(SWriter* writer)
		{
			SWriter* humans = writer->NewDummyNode("humans");

			int i = 0;
			for (auto iter = _race->GetPlayerList().begin(); iter != _race->GetPlayerList().end(); ++iter, ++i)
				if ((*iter)->IsHuman() || (*iter)->IsOpponent())
				{
					SWriter* child = humans->NewDummyNode(StrFmt("human%d", i).c_str());
					SavePlayer(*iter, child);
				}
		}

		void Race::SnProfile::LoadHumans(SReader* reader)
		{
			SReader* humans = reader->ReadValue("humans");

			SReader* player = humans ? humans->FirstChildValue() : nullptr;
			int i = 0;
			while (player)
			{
				Player* item = _race->AddPlayer((i + 1) << cOpponentBit);
				LoadPlayer(item, player);

				player = player->NextValue();
				++i;
			}
		}

		void Race::SnProfile::SaveAIPlayers(SWriter* writer)
		{
			SWriter* ai = writer->NewDummyNode("ai");

			int i = 0;
			for (auto iter = _race->GetAIPlayers().begin(); iter != _race->GetAIPlayers().end(); ++iter, ++i)
			{
				if (!(_race->GetHuman() && _race->GetHuman()->GetPlayer() == (*iter)->GetPlayer()))
				{
					SWriter* child = ai->NewDummyNode(StrFmt("player%d", i).c_str());
				}
			}
		}

		void Race::SnProfile::LoadAIPlayers(SReader* reader)
		{
			SReader* ai = reader->ReadValue("ai");

			SReader* player = ai ? ai->FirstChildValue() : nullptr;
			int i = 0;
			while (player)
			{
				Player* item = _race->AddPlayer(cComputer1 + i);
				player = player->NextValue();
				++i;
			}
		}

		void Race::SnProfile::EnterGame()
		{
			_race->GetTournament().GetPlanets()[0]->Unlock();
			_race->GetTournament().GetPlanets()[0]->Open();
			_race->GetTournament().SetCurPlanet(_race->GetTournament().GetPlanets()[0]);
		}

		void Race::SnProfile::SaveGame(SWriter* writer)
		{
			writer->WriteValue("carChanged", _race->_carChanged);
			writer->WriteValue("minDifficulty", _race->_minDifficulty);

			SaveTournament(writer);
			//SaveWorkshop(writer);
			//SaveGarage(writer);

			SaveHumans(writer);
			//SaveAIPlayers(writer);	
		}

		void Race::SnProfile::LoadGame(SReader* reader)
		{
			reader->ReadValue("carChanged", _race->_carChanged);
			reader->ReadValue("minDifficulty", _race->_minDifficulty);

			LoadTournament(reader);
			//LoadWorkshop(reader);
			//LoadGarage(reader);

			LoadHumans(reader);
			//LoadAIPlayers(reader);
		}

		Race::SkProfile::SkProfile(Race* race, const std::string& name): _MyBase(race, name)
		{
		}

		void Race::SkProfile::EnterGame()
		{
			for (auto iter = _race->GetPlanetsCompleted().begin(); iter != _race->GetPlanetsCompleted().end(); ++iter)
			{
				Planet* planet = _race->GetTournament().GetPlanet(*iter);
				if (planet)
				{
					planet->Unlock();
					planet->Open();
					//planet->SetPass(2);
					//planet->SetPass(3);
				}
			}

			_race->GetTournament().GetPlanets()[0]->Unlock();
			_race->GetTournament().GetPlanets()[0]->Open();
			_race->GetTournament().SetCurPlanet(_race->GetTournament().GetPlanets()[0]);
		}

		void Race::SkProfile::SaveGame(SWriter* writer)
		{
			//Tournament& tournament = _race->GetTournament();
			//
			//writer->WriteValue("planet", tournament.GetCurPlanetIndex());
			//writer->WriteValue("track", tournament.GetCurTrackIndex());
		}

		void Race::SkProfile::LoadGame(SReader* reader)
		{
			EnterGame();
		}

		void Race::DisposePlayer(Player* player)
		{
			if (GetWorld()->GetCamera()->GetPlayer() == player)
				GetWorld()->GetCamera()->SetPlayer(nullptr);

			SendEvent(cPlayerDispose, &EventData(player->GetId()));

			player->Release();
			delete player;
		}

		void Race::SaveCfg(SWriter* writer)
		{
			StringVec vec;
			string str;

			vec.clear();
			str.clear();

			writer->WriteValue("myCarIndex", _myCarIndex);
			writer->WriteValue("myWorldType", _myWorldType);
			writer->WriteValue("weatherType", _weatherType);
			writer->WriteValue("aiColors", _aiColors);
			writer->WriteValue("humanColor", _humanColor);
			writer->WriteValue("maxAiCount", _maxAiCount);
			writer->WriteValue("nickName", _nickName);
			writer->WriteValue("maxQuality", _maxQuality);
		}

		void Race::ReadCfg(SReader* reader)
		{
			StringVec vec;
			string str;

			vec.clear();
			str.clear();

			reader->ReadValue("myCarIndex", _myCarIndex);
			reader->ReadValue("myWorldType", _myWorldType);
			reader->ReadValue("weatherType", _weatherType);
			reader->ReadValue("aiColors", _aiColors);
			reader->ReadValue("humanColor", _humanColor);
			reader->ReadValue("maxAiCount", _maxAiCount);
			reader->ReadValue("nickName", _nickName);
			reader->ReadValue("maxQuality", _maxQuality);
		}

		void Race::LoadCfg()
		{
			try
			{
				RootNode rootNode("configRoot", _game->GetWorld());

				SerialFileXML xml;
				xml.LoadNodeFromFile(rootNode, "config.xml");

				rootNode.BeginLoad();
				ReadCfg(&rootNode);
				rootNode.EndLoad();
			}
			catch (const EUnableToOpen&)
			{
				CreateCfg();
			}
		}

		void Race::CompleteRace(Player* player)
		{
			player->InRace(false);
			//защита от двойного завершения трасы на всякий случай
			if (GetResult(player->GetId()) == nullptr)
			{
				Result result;
				result.playerId = player->GetId();
				result.place = !_results.empty() ? _results.back().place + 1 : 1;
				result.voiceNameDur = 1.5f;

				Planet::Price price = _tournament->GetCurPlanet().GetPrice(result.place);
				result.money = price.money;
				result.points = price.points;
				result.pickMoney = player->GetPickMoney();
				result.killstotal = player->GetKillsTotal();
				result.deadstotal = player->GetDeadsTotal();
				player->SetPlace(result.place);

				_results.push_back(result);

				player->SetPlace(result.place);
				if (player->IsBlock() == false)
					player->AddRacesTotal(1);
				player->SetFinished(true);
				player->SetCameraStatus(0);
				player->inRamp(false);
				if (player->GetPlace() == 1 && player->GetSuicide() == false)
					player->AddWinsTotal(1);
				player->SetSuicide(false);
				player->ResetPickMoney();

				AIPlayer* aiPlayer = FindAIPlayer(player);
				if (aiPlayer)
					aiPlayer->FreeCar();
				//Фикс вращения машины игрока после финиша:
				if (!player->IsComputer() && player->GetFinished() && player->GetCar().gameObj)
				{
					player->GetCar().gameObj->SetSteerWheelAngle(0.0f);
					player->GetCar().gameObj->SetSteerWheel(GameCar::smManual);
					player->GetCar().gameObj->SetSteerRot(0.0f);
					player->GetCar().gameObj->LockClutch(0.0f);
					player->GetCar().gameObj->SetSteerSpeed(0.0f);
					player->GetCar().gameObj->SetRespBlock(false);
					player->GetCar().gameObj->SetDamageStop(false);
					player->GetCar().gameObj->InFly(false);
					player->GetCar().gameObj->InRage(false);
					player->SetBlockTime(0.3f);
					UnlimitedTurn = false;
				}
				player->GetCar().gameObj->SetLastMoveState(1);
			}
		}

		void Race::CompleteRace(const Results* results)
		{
			if (!_startRace)
				return;

			struct MyPlayer
			{
				Player* inst;
				float placePos;

				bool operator<(const MyPlayer& ref) const
				{
					return placePos > ref.placePos;
				}
			};

			using MyPlayers = List<MyPlayer>;

			MyPlayers players;

			if (results)
			{
				_results = *results;

				struct MyResult
				{
					bool operator()(const Result& res1, const Result& res2) const
					{
						return res1.place < res2.place;
					}
				};

				std::sort(_results.begin(), _results.end(), MyResult());
			}

			for (auto iter = _playerList.begin(); iter != _playerList.end(); ++iter)
			{
				Player* player = *iter;
				const Result* result = GetResult((*iter)->GetId());

				if (result == nullptr)
				{
					MyPlayer plrRes;
					plrRes.inst = player;

					//человек который не завершил трасу всегда на последнем месте
					if (player->IsHuman() || player->IsOpponent())
						plrRes.placePos = -static_cast<int>(_tournament->GetCurTrack().GetLapsCount()) - 1.0f + player->
							GetCar().GetLap();
					else
						plrRes.placePos = player->GetCar().GetLap();

					players.push_back(plrRes);
				}
			}

			players.sort();

			//принудительно завершаем гонку для сотавшихся игроков
			for (auto iter = players.begin(); iter != players.end(); ++iter)
				CompleteRace(iter->inst);

			if (IsCampaign())
			{
				for (Results::const_iterator iter = _results.begin(); iter != _results.end(); ++iter)
				{
					Player* plr = GetPlayerById(iter->playerId);
					if (plr)
					{
						plr->AddMoney(iter->money + iter->pickMoney);
						plr->AddPoints(iter->points);
						//plr->SetKillsTotal(iter->killstotal);
						//plr->SetDeadsTotal(iter->deadstotal);
					}
				}
			}

			bool passComplete;
			_tournament->CompleteTrack(GetTotalPoints(), passComplete, _passChampion, _planetChampion);

			if (passComplete)
			{
				for (auto iter = _playerList.begin(); iter != _playerList.end(); ++iter)
				{
					Player* player = *iter;
					if (player->IsGamer())
					{
						const int pointz = player->GetPoints();
						player->SetPoints(0);

						//Если дивизион успешно пройден, даем игроку денежную награду.
						//PS: сейчас работает логика для кооператива и соло прохождения:
						if (_passChampion)
						{
							int curplanet = GetTournament().GetCurPlanet().GetIndex();
							int trial = player->GetPassTrial();

							//максимальная награда за дивизион зависит от планеты.
							int prize = 0;
							if (curplanet == 0)
								prize = 10000; //Интария
							else if (curplanet == 1)
								prize = 20000; //Патагонис
							else if (curplanet == 2)
								prize = 25000; //Хеми
							else if (curplanet == 3)
								prize = 30000; //НХО
							else
								prize = 35000; //Инферно и другие.

							//если дивизион пройден с первой попытки, то награда 100%;
							if (trial == 0)
							{
								player->AddMoney(prize);
								player->SetSponsorMoney(prize);
							}
							//иначе награда 10%
							else
							{
								player->AddMoney(prize / 10);
								player->SetSponsorMoney(prize / 10);
							}
							player->SetPassTrial(0);
						}
						else
						{
							player->AddPassTrial(1);
						}
						/*
						//для FFA награда рандомная, но её максимум зависит от планеты.
						//PS: эта логика наград ещё ни разу не тестировалась!
						if (_passChampion)
						{
							int curplanet = GetTournament().GetCurPlanet().GetIndex();
		
							//максимальная награда за дивизион зависит от планеты.
							int maxPrize = 0;		
							int minPrize = prize / 10;
							if (curplanet == 0)
								maxPrize = 10000; //Интария
							else if (curplanet == 1)
								maxPrize = 20000; //Патагонис
							else if (curplanet == 2)
								maxPrize = 25000; //Хеми
							else if (curplanet == 3)
								maxPrize = 30000; //НХО
							else
								maxPrize = 35000; //Инферно и другие.
		
							int random = RandomRange(minPrize, maxPrize);
							{
								player->AddMoney(random);
								player->SetSponsorMoney(random);
							}
						} */
						/*
						//для командных режимов награда зависит от планеты и количества набранных очков.
						//PS: эта логика наград ещё ни разу не тестировалась!
						if (_passChampion)
						{
							int curplanet = GetTournament().GetCurPlanet().GetIndex();
		
							//максимальная награда за дивизион зависит от планеты.
							int maxPrize = 0;	
							int subtract = 0;
		
							//максимальная награда после третьей планеты увеличена в 2 раза.
							if (curplanet < 3)
							{
								maxPrize = 15000; 
								subtract = pointz * 4;
							}
							else 
							{
								maxPrize = 30000;
								subtract = pointz * 8;					
							}
		
							player->AddMoney(maxPrize - subtract);
							player->SetSponsorMoney(maxPrize - subtract);				
						} */
					}
				}
			}

			if (_planetChampion)
			{
				Planet* planet = &_tournament->GetCurPlanet();

				CompletePlanet(planet->GetIndex());
			}
		}

		void Race::CompletePlanet(int index)
		{
			if (!_planetsCompleted.IsFind(index))
				_planetsCompleted.push_back(index);

			//количество планет в турнире:
			unsigned int plantetsC = 3;
			if (GAME_DIFF == 1)
				plantetsC = 4;
			if (GAME_DIFF == 2)
				plantetsC = 5;
			if (GAME_DIFF == 3)
				plantetsC = 5;

			if (index == plantetsC - 1)
			{
				for (unsigned i = plantetsC; i < _tournament->GetPlanets().size(); ++i)
				{
					if (!_planetsCompleted.IsFind(i))
						_planetsCompleted.push_back(i);
				}
			}
		}

		void Race::SaveGame(SWriter* writer)
		{
			StringVec vec;
			string str;

			vec.clear();
			str.clear();
			for (Profiles::const_iterator iter = _profiles.begin(); iter != _profiles.end(); ++iter)
				if (!(*iter)->netGame())
					vec.push_back((*iter)->GetName());
			StrLinkValues(vec, str);
			writer->WriteValue("profiles", str);

			vec.clear();
			str.clear();
			for (Profiles::const_iterator iter = _profiles.begin(); iter != _profiles.end(); ++iter)
				if ((*iter)->netGame())
					vec.push_back((*iter)->GetName());
			StrLinkValues(vec, str);
			writer->WriteValue("netProfiles", str);

			if (_lastProfile)
				writer->WriteValue("lastProfile", _lastProfile->GetName());
			if (_lastNetProfile)
				writer->WriteValue("lastNetProfile", _lastNetProfile->GetName());

			vec.clear();
			str.clear();
			for (Planets::const_iterator iter = _planetsCompleted.begin(); iter != _planetsCompleted.end(); ++iter)
				vec.push_back(StrFmt("%d", *iter));
			StrLinkValues(vec, str);
			writer->WriteValue("planetsCompleted", str);

			writer->WriteValue("tutorialStage", _tutorialStage);
		}

		void Race::LoadGame(SReader* reader)
		{
			StringVec vec;
			string str;

			ClearProfiles();
			_planetsCompleted.clear();

			vec.clear();
			str.clear();
			reader->ReadValue("profiles", str);
			StrExtractValues(str, vec);
			for (StringVec::const_iterator iter = vec.begin(); iter != vec.end(); ++iter)
				AddProfile(*iter)->netGame(false);

			vec.clear();
			str.clear();
			reader->ReadValue("netProfiles", str);
			StrExtractValues(str, vec);
			for (StringVec::const_iterator iter = vec.begin(); iter != vec.end(); ++iter)
				AddProfile(*iter)->netGame(true);

			if (reader->ReadValue("lastProfile", str))
			{
				auto iter = FindProfile(str);
				SetLastProfile(iter != _profiles.end() ? *iter : nullptr);
			}

			if (reader->ReadValue("lastNetProfile", str))
			{
				auto iter = FindProfile(str);
				SetLastNetProfile(iter != _profiles.end() ? *iter : nullptr);
			}

			vec.clear();
			str.clear();
			reader->ReadValue("planetsCompleted", str);
			StrExtractValues(str, vec);
			for (StringVec::const_iterator iter = vec.begin(); iter != vec.end(); ++iter)
			{
				std::stringstream sstream(*iter);
				int intVal;
				sstream >> intVal;

				CompletePlanet(intVal);
			}

			reader->ReadValue("tutorialStage", _tutorialStage);
		}

		void Race::LoadLib()
		{
			try
			{
				RootNode rootNode("raceRoot", _game->GetWorld());

				SerialFileXML xml;
				xml.LoadNodeFromFile(rootNode, "race.xml");

				rootNode.BeginLoad();
				LoadGame(&rootNode);
				rootNode.EndLoad();
			}
			catch (const EUnableToOpen&)
			{
				SaveLib();
			}
		}

		void Race::OnFixedStep(float deltaTime)
		{
			for (unsigned i = 0; i < _playerList.size(); ++i)
				_playerList[i]->OnProgress(deltaTime);

			if (_goRace)
			{
				if (_aiSystem)
					_aiSystem->OnProgress(deltaTime);
				AutoCameraProgress(deltaTime);
			}
		}

		void Race::OnLateProgress(float deltaTime, bool pxStep)
		{
			Player* lastLeader = !_playerPlaceList.empty() ? _playerPlaceList.front() : nullptr;
			Player* lastThird = _playerPlaceList.size() >= 3 ? _playerPlaceList[2] : nullptr;
			_playerPlaceList = _playerList;

			struct Pred
			{
				bool operator()(const Player* p1, const Player* p2) const
				{
					if (p1->GetFinished() && p2->GetFinished())
						return p1->GetPlace() < p2->GetPlace();
					if (p1->GetFinished() && !p2->GetFinished())
						return true;
					if (!p1->GetFinished() && p2->GetFinished())
						return false;
					return p1->GetCar().GetLap() > p2->GetCar().GetLap();
				}
			};

			std::sort(_playerPlaceList.begin(), _playerPlaceList.end(), Pred());

			for (unsigned i = 0; i < _playerPlaceList.size(); ++i)
				_playerPlaceList[i]->SetPlace(i + 1);

			Player* leaderPlayer = !_playerPlaceList.empty() ? _playerPlaceList.front() : nullptr;
			Player* secondPlayer = _playerPlaceList.size() >= 2 ? _playerPlaceList[1] : nullptr;
			Player* thirdPlayer = _playerPlaceList.size() >= 3 ? _playerPlaceList[2] : nullptr;
			Player* nextLastPlayer = _playerPlaceList.size() >= 2
				                         ? _playerPlaceList[_playerPlaceList.size() - 2]
				                         : nullptr;
			Player* lastPlayer = _playerPlaceList.size() >= 2 ? _playerPlaceList.back() : nullptr;

			if (leaderPlayer && lastLeader && leaderPlayer != lastLeader && leaderPlayer->GetCar().IsMainPath() &&
				lastLeader->GetCar().IsMainPath())
			{
				float newLeadPlace = leaderPlayer->GetCar().GetLap(true);
				//проверяем также на пустой результат так как речь идет о лидере гонки
				if (leaderPlayer->GetCar().GetPathLength(true) * (newLeadPlace - _lastLeadPlace) > 300.0f && _results.
					empty())
				{
					SendEvent(cPlayerLeadChanged, &EventData(leaderPlayer->GetId()));
				}
				_lastLeadPlace = newLeadPlace;
			}

			if (thirdPlayer && lastThird && thirdPlayer != lastThird && thirdPlayer->GetCar().IsMainPath() && lastThird
				->GetCar().IsMainPath())
			{
				float newPlace = thirdPlayer->GetCar().GetLap(true);
				//проверяем также на пустой результат так как речь идет о лидере гонки
				if (thirdPlayer->GetCar().GetPathLength(true) * (newPlace - _lastThirdPlace) > 300.0f && _results.
					empty())
				{
					SendEvent(cPlayerThirdChanged, &EventData(thirdPlayer->GetId()));
				}
				_lastThirdPlace = newPlace;
			}

			if (lastPlayer && nextLastPlayer && lastPlayer->GetCar().IsMainPath() && nextLastPlayer->GetCar().
				IsMainPath() && lastPlayer->GetCar().GetPathLength(true) * (nextLastPlayer->GetCar().GetLap(true) -
					lastPlayer->GetCar().GetLap(true)) > 70.0f)
			{
				SendEvent(cPlayerLastFar, &EventData(lastPlayer->GetId()));
			}

			if (leaderPlayer && secondPlayer && leaderPlayer->GetCar().IsMainPath() && secondPlayer->GetCar().
				IsMainPath() && leaderPlayer->GetCar().GetPathLength(true) * (leaderPlayer->GetCar().GetLap(true) -
					secondPlayer->GetCar().GetLap(true)) > 70.0f)
			{
				if (_results.empty())
					SendEvent(cPlayerDomination, &EventData(leaderPlayer->GetId()));
			}

			if (secondPlayer && thirdPlayer && secondPlayer->GetCar().IsMainPath() && thirdPlayer->GetCar().IsMainPath()
				&& thirdPlayer->GetCar().GetPathLength(true) * (secondPlayer->GetCar().GetLap(true) - thirdPlayer->
					GetCar().GetLap(true)) > 70.0f)
			{
				if (_results.empty())
					SendEvent(cPlayerThirdFar, &EventData(thirdPlayer->GetId()));
			}
		}

		Player* Race::AddPlayer(int plrId)
		{
			const D3DXCOLOR color[cMaxPlayers] = {
				D3DXCOLOR(0xFF5B29A5), D3DXCOLOR(0xFF9E9E9E), D3DXCOLOR(0xFFFF80C0), D3DXCOLOR(0xFF83F7CC),
				D3DXCOLOR(0xFF83E500), D3DXCOLOR(0xFFD8E585), D3DXCOLOR(0xFF6100B9), D3DXCOLOR(0xFF006CA4)
			};

			auto player = new Player(this);
			player->AddRef();
			player->SetId(plrId);
			_playerList.push_back(player);

			_tournament->GetCurPlanet().StartPass(player);

			if (player->IsHuman() || player->IsOpponent())
				if (GetGame()->devMode() == true && TEST_BUILD == true)
					player->SetMoney(5000000);
				else
					player->SetMoney(40000);
			else if (player->IsComputer())
			{
				int index = plrId - cComputer1;
				player->SetGamerId(index + 1);
				player->SetColor(color[index % cMaxPlayers]);
			}

			return player;
		}

		Player* Race::AddPlayer(int plrId, int gamerId, int netSlot, const D3DXCOLOR& color)
		{
			Player* player = AddPlayer(plrId);
			player->SetGamerId(gamerId);
			player->SetNetSlot(netSlot);
			player->SetColor(color);

			return player;
		}

		void Race::DelPlayer(PlayerList::const_iterator iter)
		{
			DisposePlayer(*iter);
			_playerList.erase(iter);

			_playerPlaceList.clear();
		}

		void Race::DelPlayer(Player* plr)
		{
			DelPlayer(_playerList.Find(plr));
		}

		void Race::ClearPlayerList()
		{
			for (auto iter = _playerList.begin(); iter != _playerList.end(); ++iter)
				DisposePlayer(*iter);

			_playerList.clear();
			_playerPlaceList.clear();
		}

		AIPlayer* Race::AddAIPlayer(Player* player)
		{
			AIPlayer* aiPlayer = _aiSystem->AddPlayer(player);
			aiPlayer->AddRef();
			_aiPlayers.push_back(aiPlayer);

			return aiPlayer;
		}

		void Race::DelAIPlayer(AIPlayers::const_iterator iter)
		{
			AIPlayer* aiPlayer = *iter;
			_aiPlayers.Remove(*iter);
			aiPlayer->Release();
			_aiSystem->DelPlayer(aiPlayer);
		}

		void Race::DelAIPlayer(AIPlayer* plr)
		{
			DelAIPlayer(_aiPlayers.Find(plr));
		}

		void Race::ClearAIPlayers()
		{
			while (!_aiPlayers.empty())
				DelAIPlayer(_aiPlayers.begin());
		}

		AIPlayer* Race::FindAIPlayer(Player* player)
		{
			for (auto iter = _aiPlayers.begin(); iter != _aiPlayers.end(); ++iter)
				if ((*iter)->GetPlayer() == player)
					return *iter;

			return nullptr;
		}

		HumanPlayer* Race::CreateHuman(Player* player)
		{
			FreeHuman();

			_human = new HumanPlayer(player);
			_human->AddRef();
			_human->GetPlayer()->SetNetName(_human->GetRace()->MyNickName());

			return _human;
		}

		void Race::FreeHuman()
		{
			if (_human)
			{
				_human->Release();
				delete _human;
				_human = nullptr;
			}
		}

		void Race::CreateCfg()
		{
			RootNode rootNode("configRoot", _game->GetWorld());

			rootNode.BeginSave();
			SaveCfg(&rootNode);
			rootNode.EndSave();

			SerialFileXML xml;
			xml.SaveNodeToFile(rootNode, "config.xml");
		}

		void Race::CreatePlayers(unsigned numAI)
		{
#if DEBUG_WEAPON
	numAI = 3;
#elif _DEBUG | DEBUG_PX
			//numAI = 4;
#endif

			for (unsigned i = _playerList.size(); i < numAI + 1; ++i)
			{
				if (i == 0)
				{
					Player* player = AddPlayer(cHuman);
					CreateHuman(player);

#if _DEBUG | DEBUG_PX
			AIPlayer* aiPlayer = AddAIPlayer(player);
#endif
				}
				else
				{
					Player* player = AddPlayer(cComputer1 + i - 1);

#if DEBUG_WEAPON
			continue;
#endif

					AddAIPlayer(player);
				}
			}

			unsigned plrCount = _playerList.size();

			for (unsigned i = numAI + 1; i < plrCount; ++i)
			{
				Player* plr = _playerList[numAI + 1];

				AIPlayer* aiPlr = FindAIPlayer(plr);
				if (aiPlr)
					DelAIPlayer(aiPlr);

				DelPlayer(plr);
			}
		}

		unsigned Race::GetMyCarIndex() const
		{
			return _myCarIndex;
		}

		unsigned Race::GetMyWorldType() const
		{
			return _myWorldType;
		}

		unsigned Race::GetWeatherType() const
		{
			return _weatherType;
		}

		unsigned Race::GetAiColors() const
		{
			return _aiColors;
		}

		unsigned Race::GetHumanColor() const
		{
			return _humanColor;
		}

		unsigned Race::GetMaxAICount() const
		{
			return _maxAiCount;
		}

		const string& Race::MyNickName() const
		{
			return _nickName;
		}

		bool Race::IsMaxQuality() const
		{
			return _maxQuality;
		}

		Player* Race::GetPlayerByMapObj(MapObj* mapObj)
		{
			for (PlayerList::const_iterator iter = _playerList.begin(); iter != _playerList.end(); ++iter)
				if ((*iter)->GetCar().mapObj == mapObj)
					return *iter;
			return nullptr;
		}

		Player* Race::GetPlayerById(int id) const
		{
			if (id == cUndefPlayerId)
				return nullptr;

			for (auto iter = _playerList.begin(); iter != _playerList.end(); ++iter)
				if ((*iter)->GetId() == id)
					return *iter;

			return nullptr;
		}

		Player* Race::GetPlayerByNetSlot(unsigned netSlot) const
		{
			for (auto iter = _playerList.begin(); iter != _playerList.end(); ++iter)
				if ((*iter)->GetNetSlot() == netSlot)
					return *iter;

			return nullptr;
		}

		const Race::PlayerList& Race::GetPlayerList() const
		{
			return _playerList;
		}

		HumanPlayer* Race::GetHuman()
		{
			return _human;
		}

		const Race::AIPlayers& Race::GetAIPlayers() const
		{
			return _aiPlayers;
		}

		Race::Profile* Race::AddProfile(const std::string& name)
		{
			auto iter = FindProfile(name);
			LSL_ASSERT(iter == _profiles.end());

			Profile* profile = new SnProfile(this, name);
			profile->AddRef();
			_profiles.push_back(profile);

			return profile;
		}

		void Race::DelProfile(Profiles::const_iterator iter, bool saveLib)
		{
			Profile* profile = *iter;
			_profiles.erase(iter);

			profile->Release();
			if (_lastProfile == profile)
				SetLastProfile(nullptr);
			if (_lastNetProfile == profile)
				SetLastNetProfile(nullptr);

			delete profile;

			if (saveLib)
				SaveLib();
		}

		void Race::DelProfile(int index, bool saveLib)
		{
			DelProfile(_profiles.begin() + index, saveLib);
		}

		void Race::DelProfile(Profile* profile, bool saveLib)
		{
			DelProfile(_profiles.Find(profile), saveLib);
		}

		void Race::ClearProfiles(bool saveLib)
		{
			while (!_profiles.empty())
				DelProfile(_profiles.begin(), false);

			if (saveLib)
				SaveLib();
		}

		Race::Profiles::const_iterator Race::FindProfile(Profile* profile) const
		{
			for (auto iter = _profiles.begin(); iter != _profiles.end(); ++iter)
				if (*iter == profile)
					return iter;

			return _profiles.end();
		}

		Race::Profiles::const_iterator Race::FindProfile(const std::string& name) const
		{
			for (auto iter = _profiles.begin(); iter != _profiles.end(); ++iter)
				if ((*iter)->GetName() == name)
					return iter;

			return _profiles.end();
		}

		std::string Race::MakeProfileName(const std::string& base) const
		{
			int i = 1;

			std::string test = base;
			do
			{
				std::stringstream sstream;
				sstream << base << i;
				test = sstream.str();
				++i;
			}
			while (FindProfile(test) != _profiles.end());

			return test;
		}

		const Race::Profiles& Race::GetProfiles() const
		{
			return _profiles;
		}

		void Race::EnterProfile(Profile* profile, Mode mode)
		{
			LSL_ASSERT(!_startRace);

			ExitProfile();

			_profile = profile;
			_mode = mode;
			_carChanged = false;
			_minDifficulty = cDifficultyEnd;
			_profile->AddRef();

			if (mode != rmSkirmish && profile != _snClientProfile)
			{
				if (profile->netGame())
					SetLastNetProfile(profile);
				else
					SetLastProfile(profile);
			}

			profile->Enter();
		}

		void Race::ExitProfile()
		{
			if (_profile)
			{
				_profile->Release();
				_profile = nullptr;

				ClearAIPlayers();
				FreeHuman();
				ClearPlayerList();
			}
		}

		void Race::NewProfile(Mode mode, bool netGame, bool netClient)
		{
			switch (mode)
			{
			case rmChampionship:
				{
					if (netClient)
					{
						EnterProfile(_snClientProfile, rmChampionship);
						_snClientProfile->netGame(netGame);
					}
					else
					{
						std::string name = MakeProfileName();

						Profile* profile = AddProfile(name);
						profile->netGame(netGame);

						EnterProfile(profile, rmChampionship);
					}
					break;
				}

			case rmSkirmish:
				{
					EnterProfile(_skProfile, rmSkirmish);
					_skProfile->netGame(netGame);
					LoadProfile();
					break;
				}
			}
		}

		void Race::ReloadProfile(Mode mode, bool netGame, bool netClient)
		{
			switch (mode)
			{
			case rmChampionship:
				{
					if (netClient)
					{
						EnterProfile(_snClientProfile, rmChampionship);
						_snClientProfile->netGame(netGame);
					}
					else
					{
						Profile* profile = GetLastProfile();
						if (netGame == true)
							Profile* profile = GetLastNetProfile();

						profile->netGame(netGame);
						EnterProfile(profile, rmChampionship);
					}
					break;
				}

			case rmSkirmish:
				{
					EnterProfile(_skProfile, rmSkirmish);
					_skProfile->netGame(netGame);
					LoadProfile();
					break;
				}
			}
		}

		bool Race::IsMatchStarted() const
		{
			return _profile != nullptr;
		}

		void Race::SaveProfile()
		{
			if (_profile)
				_profile->SaveGameFile();
		}

		void Race::LoadProfile()
		{
			if (_profile)
				_profile->LoadGameFile();
		}

		void Race::SaveLib()
		{
			RootNode rootNode("raceRoot", _game->GetWorld());

			rootNode.BeginSave();
			SaveGame(&rootNode);
			rootNode.EndSave();

			SerialFileXML xml;
			xml.SaveNodeToFile(rootNode, "race.xml");
		}

		Race::Profile* Race::GetProfile()
		{
			return _profile;
		}

		Race::Profile* Race::GetLastProfile()
		{
			return _lastProfile;
		}

		void Race::SetLastProfile(Profile* value)
		{
			if (ReplaceRef(_lastProfile, value))
				_lastProfile = value;
		}

		Race::Profile* Race::GetLastNetProfile()
		{
			return _lastNetProfile;
		}

		void Race::SetLastNetProfile(Profile* value)
		{
			if (ReplaceRef(_lastNetProfile, value))
				_lastNetProfile = value;
		}

		Race::Mode Race::GetMode() const
		{
			return IsMatchStarted() ? _mode : cModeEnd;
		}

		bool Race::IsCampaign() const
		{
			return _mode == rmChampionship;
		}

		bool Race::IsSkirmish() const
		{
			return _mode == rmSkirmish;
		}

		bool Race::HasMoney(Player* player, int cost)
		{
			return IsCampaign() ? player->GetMoney() >= cost : true;
		}

		bool Race::BuyItem(Player* player, int cost)
		{
			if (IsCampaign())
			{
				if (!HasMoney(player, cost))
					return false;

				player->AddMoney(-cost);
				return true;
			}
			return true;
		}

		void Race::SellItem(Player* player, int cost, bool sellDiscount)
		{
			if (IsCampaign())
				player->AddMoney(static_cast<int>(cost * (sellDiscount ? cSellDiscount : 1.0f)));
		}

		int Race::GetCost(int realCost)
		{
			return realCost;
		}

		int Race::GetSellCost(int realCost, bool sellDiscount)
		{
			return static_cast<int>(realCost * (sellDiscount ? cSellDiscount : 1.0f));
		}

		void Race::HumanNextPlayer()
		{
			if (_human)
			{
				PlayerList::const_iterator iter = _playerList.Find(_human->GetPlayer());
				if (iter == _playerList.end() || (++iter) == _playerList.end())
					iter = _playerList.begin();
				Player* player = *iter;

				CreateHuman(player);

				AIPlayer* aiPlayer = _aiSystem->FindAIPlayer(player);
				if (aiPlayer)
					_aiSystem->CreateDebug(aiPlayer);
			}
		}

		void Race::AutoCameraProgress(float deltaTime)
		{
			Player* current = this->GetWorld()->GetCamera()->GetPlayer();
			//camera status: 
			//0 - авто/ручная (зависит от настроек) 
			//1 - сзади
			//2 - изометрия	
			//3 - ручной режим (принудительный выход из авто)
			//4 - автоматический режим (принудительный вход в авто)

			if (GetPlayerById(cHuman) != nullptr && current != nullptr && current->GetCar().gameObj != nullptr)
			{
				//принудительный вход в авто делается через триггер:
				if (current->GetCameraStatus() == 4)
				{
					if (!GetGame()->autoCamera())
						GetGame()->autoCamera(true);
				}

				//автоматическое переключение вида камеры.
				if (GetGame()->autoCamera())
				{
					if (current->GetCameraStatus() == 1)
					{
						if (GetWorld()->GetCamera()->GetStyle() != CameraManager::csThirdPerson)
							GetWorld()->GetCamera()->ChangeStyle(CameraManager::csThirdPerson);
					}
					else if (current->GetCameraStatus() == 2)
					{
						if (GetWorld()->GetCamera()->GetStyle() != CameraManager::csIsometric)
							GetWorld()->GetCamera()->ChangeStyle(CameraManager::csIsometric);
					}
					else if (current->GetCameraStatus() == 3)
					{
						//принудительный выход из авто через триггер.
						if (GetGame()->autoCamera())
							GetGame()->autoCamera(false);
					}
					else
					{
						//вне тригеров камера меняется в зависимости от обстоятельств:
						if (current->InRace() && current->GetCar().gameObj != nullptr)
						{
							if (current->GetCar().gameObj->GetSpeed() > 10)
							{
								//изометрия, если игрок первый.
								if (current->GetRace()->GetWorld()->GetCamera()->GetPlayer()->GetPlace() == 1)
									if (GetWorld()->GetCamera()->GetStyle() != CameraManager::csIsometric)
										GetWorld()->GetCamera()->ChangeStyle(CameraManager::csIsometric);

								//вид сзади, если игрок последний.
								if (current->GetRace()->GetWorld()->GetCamera()->GetPlayer()->GetPlace() ==
									TOTALPLAYERS_COUNT - SPECTATORS_COUNT)
									if (GetWorld()->GetCamera()->GetStyle() != CameraManager::csThirdPerson)
										GetWorld()->GetCamera()->ChangeStyle(CameraManager::csThirdPerson);
							}

							//если кончились патроны и игрок не последний
							if (current->IsEmptyWpn() && current->GetPlace() != TOTALPLAYERS_COUNT - SPECTATORS_COUNT)
								if (GetWorld()->GetCamera()->GetStyle() != CameraManager::csIsometric && current->
									GetRace()->GetWorld()->GetCamera()->GetStyle() != CameraManager::csInverseThird)
									GetWorld()->GetCamera()->ChangeStyle(CameraManager::csIsometric);

							if (current->GetCar().gameObj->GetMoveCar() == current->GetCar().gameObj->mcBack)
							{
								current->_backMoveTime += deltaTime;
								if (current->_backMoveTime > 1.0f)
								{
									if (current->GetRace()->GetWorld()->GetCamera()->GetPlayer()->GetPlace() ==
										TOTALPLAYERS_COUNT - SPECTATORS_COUNT)
									{
										if (GetWorld()->GetCamera()->GetStyle() != CameraManager::csInverseThird)
											GetWorld()->GetCamera()->ChangeStyle(CameraManager::csInverseThird);
									}
									else
									{
										if (GetWorld()->GetCamera()->GetStyle() != CameraManager::csIsometric)
											GetWorld()->GetCamera()->ChangeStyle(CameraManager::csIsometric);
									}
									current->_backMoveTime = 0;
								}
							}
						}
					}
				}
			}
		}

		void Race::ResetCarPos()
		{
			for (PlayerList::const_iterator iter = _playerList.begin(); iter != _playerList.end(); ++iter)
			{
				++TOTALPLAYERS_COUNT;
				if ((*iter)->GetGamerId() >= SPECTATOR_ID_BEGIN)
				{
					++SPECTATORS_COUNT;
				}
			}

			const unsigned cRowLength = 4;
			const float rowSpace = 7.0f;

			D3DXVECTOR3 stPos = ZVector * 7.0f;
			D3DXVECTOR3 dirVec = XVector;
			D3DXVECTOR3 lineVec = YVector;
			float nodeWidth = 10.0f;

			if (!GetMap()->GetTrace().GetPoints().empty())
			{
				WayPoint* point = GetMap()->GetTrace().GetPoints().front();
				WayNode* node = !point->GetNodes().empty() ? point->GetNodes().front() : nullptr;

				stPos = point->GetPos() + ZVector * 2.0f;
				nodeWidth = point->GetSize();

				if (node)
				{
					D3DXVECTOR2 dir2 = node->GetTile().GetDir();
					D3DXVECTOR2 norm2 = node->GetTile().GetNorm();

					dirVec = D3DXVECTOR3(dir2.x, dir2.y, 0);
					lineVec = D3DXVECTOR3(norm2.x, norm2.y, 0);
				}
			}

			float plSize = 0.0f;
			float spaceY = 0.0f;
			float stepY = 0.0f;

			//порядковый номер игрока (наблюдатели игнорируются)
			unsigned g = 0;
			unsigned s = 0;

			for (unsigned i = 0; i < _playerList.size(); ++i)
			{
				if ((g % cRowLength) == 0)
				{
					unsigned count = std::min((_playerList.size() - SPECTATORS_COUNT) - g, 4U);

					plSize = 0.0f;
					stepY = 0;

					for (unsigned j = g; j < g + count; ++j)
					{
						Player* player = _playerList[i];

						if (player->IsGamer())
						{
							MapObj* mapObj = player->GetCar().mapObj;
							GameObject* gameObj = player->GetCar().gameObj;
							if (mapObj)
								plSize += gameObj->GetGrActor().GetLocalAABB(true).GetSizes().y;
						}
					}

					spaceY = std::max((nodeWidth - plSize) / (count + 1.0f), 0.0f);
				}

				Player* player = _playerList[i];
				if (player->IsGamer())
				{
					MapObj* mapObj = player->GetCar().mapObj;
					GameObject* gameObj = player->GetCar().gameObj;

					if (mapObj)
					{
						AABB aabb = gameObj->GetGrActor().GetLocalAABB(true);
						D3DXVECTOR3 size = aabb.GetSizes();

						//dirVec начинается с небольшого порога чтобы машина не оказалась на последнем тайле
						gameObj->SetWorldPos(
							stPos + dirVec * (0.1f - g / 4 * rowSpace) + lineVec * (-nodeWidth / 2.0f + spaceY + size.y
								/ 2.0f + stepY));
						stepY += size.y + spaceY;

						gameObj->GetGrActor().SetDir(dirVec);
						gameObj->GetGrActor().SetUp(ZVector);
						gameObj->SetWorldRot(gameObj->GetGrActor().GetWorldRot());

						gameObj->GetPxActor().GetNxActor()->setLinearVelocity(NxVec3(NullVector));
						gameObj->GetPxActor().GetNxActor()->setLinearMomentum(NxVec3(NullVector));
						gameObj->GetPxActor().GetNxActor()->setAngularMomentum(NxVec3(NullVector));
						gameObj->GetPxActor().GetNxActor()->setAngularVelocity(NxVec3(NullVector));
					}
				}
				if (player->IsGamer())
					g += 1;
				else
					s += 1;
			}
		}

		void Race::StartRace()
		{
			const Environment::WorldType envWorldType[Planet::cWorldTypeEnd] = {
				Environment::wtWorld1, Environment::wtWorld2, Environment::wtWorld3, Environment::wtWorld4,
				Environment::wtWorld5, Environment::wtWorld6
			};
			const CameraManager::Style camStyle[GameMode::cPrefCameraEnd] = {
				CameraManager::csThirdPerson, CameraManager::csIsometric
			};

			if (!_startRace)
			{
				_startRace = true;

				_goRace = false;

				_game->RegFixedStepEvent(this);
				_game->RegLateProgressEvent(this);

				_results.clear();
				ResetChampion();
				_lastThirdPlace = _lastLeadPlace = 0;

				GetWorld()->LoadLevel(_tournament->GetCurTrack().level);

#ifdef _DEBUG
		Environment::Wheater wheater = Environment::ewClody;
#else
				Environment::Wheater wheater = _tournament->GetWheater();


#endif

				GetWorld()->GetEnv()->SetWorldType(envWorldType[_tournament->GetCurPlanet().GetWorldType()]);

				GetWorld()->GetEnv()->SetWheater(wheater);
				GetWorld()->GetEnv()->StartScene();

				GetWorld()->GetGraph()->BuildOctree();

				for (auto iter = _playerList.begin(); iter != _playerList.end(); ++iter)
				{
					if ((*iter)->GetGamerId() >= SPECTATOR_ID_BEGIN)
					{
						(*iter)->IsSpectator(true);
						(*iter)->IsGamer(false);
					}
					else
					{
						(*iter)->IsSpectator(false);
						(*iter)->IsGamer(true);
						(*iter)->CreateCar(true);
					}
					(*iter)->SetHeadlight(wheater == Environment::ewNight
						                      ? ((*iter)->IsHuman() ? Player::hlmTwo : Player::hlmOne)
						                      : Player::hlmNone);
					(*iter)->ReloadWeapons();
					(*iter)->SetFinished(false);
					(*iter)->ResetBlock(false);
					(*iter)->InRace(true);
					(*iter)->isSubject(false);
					(*iter)->SetAutoSkip(true);
					(*iter)->SetRaceTime(0);
					(*iter)->SetRaceSeconds(0);
					(*iter)->SetRaceMSeconds(0);
					(*iter)->SetRaceMinutes(0);
				}

				if (_human)
				{
					_human->GetPlayer()->ResetBlock(true);
					_human->SetCurWeapon(0);
				}

				for (unsigned i = 0; i < _aiPlayers.size(); ++i)
				{
					AIPlayer* plr = _aiPlayers[i];
					int gamerId = plr->GetPlayer()->GetGamerId();

					for (auto iter = _playerList.begin(); iter != _playerList.end(); ++iter)
					{
						if (plr->GetPlayer()->IsComputer() && *iter != plr->GetPlayer() && (*iter)->GetGamerId() ==
							gamerId)
						{
							for (auto iter2 = _tournament->GetGamers().begin(); iter2 != _tournament->GetGamers().end();
							     ++iter2)
							{
								bool isFind = false;

								for (auto iter3 = _playerList.begin(); iter3 != _playerList.end(); ++iter3)
									if ((*iter2)->GetBoss().id == (*iter3)->GetGamerId())
									{
										isFind = true;
										break;
									}

								if (!isFind)
								{
									gamerId = (*iter2)->GetBoss().id;
									break;
								}
							}
							break;
						}
					}
					plr->GetPlayer()->SetGamerId(gamerId);
					plr->CreateCar(); //clrBot5
					const D3DXCOLOR natives[cMaxPlayers] = {
						clrBlack, clrRip, clrShred, clrKristy, clrBot1, clrBlack, clrBot3, clrBot4
					};
					const D3DXCOLOR sepia[cMaxPlayers] = {
						D3DXCOLOR(0xFF5B29A5), clrWhite, clrBlack, clrGray35, clrGray50, clrBlack, clrGray80, clrGray15
					};
					const D3DXCOLOR alters[cMaxPlayers] = {
						clrBlack, clrBot6, D3DXCOLOR(0xDBAF00), clrBot8, clrBot9, clrBlack, clrBot11, clrBot12
					};
					const D3DXCOLOR red[cMaxPlayers] = {
						clrBlack, clrRed, D3DXCOLOR(0xFFC60000), D3DXCOLOR(0xFF720000), D3DXCOLOR(0xFFE23100), clrBlack,
						D3DXCOLOR(0xFFFF003B), D3DXCOLOR(0xFFB7002A)
					};
					const D3DXCOLOR green[cMaxPlayers] = {
						clrBlack, clrGreen, D3DXCOLOR(0xFF00C409), D3DXCOLOR(0xFF006303), D3DXCOLOR(0xFF88FF00),
						clrBlack, D3DXCOLOR(0xFF023D00), D3DXCOLOR(0xFF8EC100)
					};
					const D3DXCOLOR blue[cMaxPlayers] = {
						clrBlack, clrBlue, D3DXCOLOR(0xFF001591), D3DXCOLOR(0xFF004A7F), D3DXCOLOR(0xFF0094FF),
						clrBlack, D3DXCOLOR(0xFF23E9FF), D3DXCOLOR(0xFF99C8FF)
					};
					const D3DXCOLOR rainbow[cMaxPlayers] = {
						clrBlack, clrRed, clrBlue, clrYellow, clrGreen, clrBlack, clrOrange, clrViolet
					};
					int index = plr->GetPlayer()->GetId() - cComputer1;

					if (GetAiColors() == 0)
						plr->GetPlayer()->SetColor(natives[index % cMaxPlayers]);
					else if (GetAiColors() == 1)
						plr->GetPlayer()->SetColor(sepia[index % cMaxPlayers]);
					else if (GetAiColors() == 2)
						plr->GetPlayer()->SetColor(alters[index % cMaxPlayers]);
					else if (GetAiColors() == 3)
						plr->GetPlayer()->SetColor(red[index % cMaxPlayers]);
					else if (GetAiColors() == 4)
						plr->GetPlayer()->SetColor(green[index % cMaxPlayers]);
					else if (GetAiColors() == 5)
						plr->GetPlayer()->SetColor(blue[index % cMaxPlayers]);
					else if (GetAiColors() == 6)
						plr->GetPlayer()->SetColor(rainbow[index % cMaxPlayers]);
					else
						plr->GetPlayer()->SetColor(natives[index % cMaxPlayers]);
				}
				//количество всех игроков:
				TOTALPLAYERS_COUNT = 0;

				//количество наблюдателей:
				SPECTATORS_COUNT = 0;

				ResetCarPos();

				//стиль заблокированой камеры указываем в турнаменте:
				if (_tournament->GetCurTrack().CamLock())
					GetWorld()->GetCamera()->ChangeStyle(camStyle[_tournament->GetCurTrack().GetPrefCam()]);
				else
					GetWorld()->GetCamera()->ChangeStyle(camStyle[_game->GetPrefCamera()]);

				GetWorld()->GetCamera()->SetPlayer(_human ? _human->GetPlayer() : nullptr);

				//максимальное количество наблюдателей == 4. //максимальное количество игроков 6.
				//если это сетевая игра, наблюдение будет происходить за хостом.
				if (GetGame()->GetMenu()->IsNetGame() && GetPlayerById(cHuman)->IsSpectator())
				{
					//только если хост существует и не является наблюдателем.
					if (GetPlayerByNetSlot(1) != nullptr && GetPlayerByNetSlot(1)->IsGamer())
						GetWorld()->GetCamera()->SetPlayer(GetPlayerByNetSlot(1));
					else
					{
						if (GetPlayerByNetSlot(2) != nullptr && GetPlayerByNetSlot(2)->IsGamer())
							GetWorld()->GetCamera()->SetPlayer(GetPlayerByNetSlot(2));
						else
						{
							if (GetPlayerByNetSlot(3) != nullptr && GetPlayerByNetSlot(3)->IsGamer())
								GetWorld()->GetCamera()->SetPlayer(GetPlayerByNetSlot(3));
							else
							{
								if (GetPlayerByNetSlot(4) != nullptr && GetPlayerByNetSlot(4)->IsGamer())
									GetWorld()->GetCamera()->SetPlayer(GetPlayerByNetSlot(4));
								else
								{
									if (GetPlayerByNetSlot(5) != nullptr && GetPlayerByNetSlot(5)->IsGamer())
										GetWorld()->GetCamera()->SetPlayer(GetPlayerByNetSlot(5));
									else
									{
										if (GetPlayerByNetSlot(6) != nullptr && GetPlayerByNetSlot(6)->IsGamer())
											GetWorld()->GetCamera()->SetPlayer(GetPlayerByNetSlot(6));
										else
										{
											if (GetPlayerByNetSlot(7) != nullptr && GetPlayerByNetSlot(7)->IsGamer())
												GetWorld()->GetCamera()->SetPlayer(GetPlayerByNetSlot(7));
											else
											{
												if (GetPlayerByNetSlot(8) != nullptr && GetPlayerByNetSlot(8)->
													IsGamer())
													GetWorld()->GetCamera()->SetPlayer(GetPlayerByNetSlot(8));
												else
												{
													//если все игроки являются наблюдателями, то переключаемся на свободную камеру.
													GetWorld()->GetCamera()->SetPlayer(
														_human ? _human->GetPlayer() : nullptr);
													GetWorld()->GetCamera()->ChangeStyle(CameraManager::csFreeView);
												}
											}
										}
									}
								}
							}
						}
					}
					if (GetGame()->GetMenu()->subjectView() != 0)
					{
						if (GetGame()->GetMenu()->subjectView() == 1 && GetPlayerByNetSlot(1) != nullptr &&
							GetPlayerByNetSlot(1)->IsGamer())
							GetWorld()->GetCamera()->SetPlayer(GetPlayerByNetSlot(1));

						if (GetGame()->GetMenu()->subjectView() == 2 && GetPlayerByNetSlot(2) != nullptr &&
							GetPlayerByNetSlot(2)->IsGamer())
							GetWorld()->GetCamera()->SetPlayer(GetPlayerByNetSlot(2));

						if (GetGame()->GetMenu()->subjectView() == 3 && GetPlayerByNetSlot(3) != nullptr &&
							GetPlayerByNetSlot(3)->IsGamer())
							GetWorld()->GetCamera()->SetPlayer(GetPlayerByNetSlot(3));

						if (GetGame()->GetMenu()->subjectView() == 4 && GetPlayerByNetSlot(4) != nullptr &&
							GetPlayerByNetSlot(4)->IsGamer())
							GetWorld()->GetCamera()->SetPlayer(GetPlayerByNetSlot(4));

						if (GetGame()->GetMenu()->subjectView() == 5 && GetPlayerByNetSlot(5) != nullptr &&
							GetPlayerByNetSlot(5)->IsGamer())
							GetWorld()->GetCamera()->SetPlayer(GetPlayerByNetSlot(5));

						if (GetGame()->GetMenu()->subjectView() == 6 && GetPlayerByNetSlot(6) != nullptr &&
							GetPlayerByNetSlot(6)->IsGamer())
							GetWorld()->GetCamera()->SetPlayer(GetPlayerByNetSlot(6));

						if (GetGame()->GetMenu()->subjectView() == 7 && GetPlayerByNetSlot(7) != nullptr &&
							GetPlayerByNetSlot(7) != nullptr && GetPlayerByNetSlot(7)->IsGamer())
							GetWorld()->GetCamera()->SetPlayer(GetPlayerByNetSlot(7));

						if (GetGame()->GetMenu()->subjectView() == 8 && GetPlayerById(cComputer1) != nullptr)
							GetWorld()->GetCamera()->SetPlayer(GetPlayerById(cComputer1));
					}
				}

				GetWorld()->GetCamera()->GetPlayer()->isSubject(true);


#if _DEBUG | DEBUG_PX
		if (AIPlayer* aiPlayer = FindAIPlayer(_human->GetPlayer()))
			_aiSystem->CreateDebug(aiPlayer);
#endif
			}
		}

		void Race::ExitRace(const Results* results)
		{
			CompleteRace(results);
			_goRace = false;

			if (_startRace)
			{
				_startRace = false;
				CompleteTutorialStage();

#if _DEBUG | DEBUG_PX
		_aiSystem->FreeDebug();
#endif

				_game->UnregFixedStepEvent(this);
				_game->UnregLateProgressEvent(this);

				for (auto iter = _aiPlayers.begin(); iter != _aiPlayers.end(); ++iter)
				{
					(*iter)->FreeCar();
				}
				for (auto iter = _playerList.begin(); iter != _playerList.end(); ++iter)
				{
					(*iter)->SetHeadlight(Player::hlmNone);
					(*iter)->FreeCar(true);
				}

				_achievment->ResetRaceState();

				GetWorld()->GetCamera()->SetPlayer(nullptr);
				GetWorld()->GetEnv()->ReleaseScene();
				GetWorld()->GetLogic()->CleanGameObjs();
				GetMap()->Clear();

				if (_minDifficulty > _profile->difficulty())
					_minDifficulty = _profile->difficulty();
			}
		}

		bool Race::IsStartRace() const
		{
			return _startRace;
		}

		void Race::GoRace()
		{
			_goRace = true;
			if (_human)
			{
				_human->GetPlayer()->ResetBlock(false);
				//наблюдатель заканчивает гонку раньше всех
				if (_human->GetPlayer()->IsSpectator())
				{
					this->SendEvent(cRaceEscape, nullptr);
					_human->GetPlayer()->SetAutoSkip(false);
				}
			}
		}

		bool Race::IsRaceGo() const
		{
			return _goRace;
		}

		bool Race::GetCarChanged() const
		{
			return _carChanged;
		}

		void Race::SetCarChanged(bool value)
		{
			_carChanged = value;
		}

		Difficulty Race::GetMinDifficulty() const
		{
			return _minDifficulty;
		}

		void Race::SetMinDifficulty(Difficulty value)
		{
			_minDifficulty = value;
		}

		int Race::GetTutorialStage() const
		{
			return _tutorialStage;
		}

		void Race::CompleteTutorialStage()
		{
			_tutorialStage = std::min(_tutorialStage + 1, 3);
		}

		bool Race::IsTutorialCompletedFirstStage()
		{
			return _tutorialStage >= 1;
		}

		bool Race::IsTutorialCompleted()
		{
			return _tutorialStage >= 3;
		}


		bool Race::GetWeaponUpgrades() const
		{
			return _weaponUpgrades;
		}

		void Race::SetWeaponUpgrades(bool value)
		{
			_weaponUpgrades = value;
		}

		bool Race::GetSurvivalMode() const
		{
			return _survivalMode;
		}

		void Race::SetSurvivalMode(bool value)
		{
			_survivalMode = value;
		}

		bool Race::GetAutoCamera() const
		{
			return _autoCamera;
		}

		void Race::SetAutoCamera(bool value)
		{
			_autoCamera = value;
		}

		int Race::GetSubjectView() const
		{
			return _subjectView;
		}

		void Race::SetSubjectView(int value)
		{
			_subjectView = value;
		}

		bool Race::GetDevMode() const
		{
			return _devMode;
		}

		void Race::SetDevMode(bool value)
		{
			_devMode = value;
		}

		bool Race::GetCamLock() const
		{
			return _camLock;
		}

		void Race::SetCamLock(bool value)
		{
			_camLock = value;
		}

		bool Race::GetStaticCam() const
		{
			return _staticCam;
		}

		void Race::SetStaticCam(bool value)
		{
			_staticCam = value;
		}

		int Race::GetCamFov() const
		{
			return _camFov;
		}

		void Race::SetCamFov(int value)
		{
			_camFov = value;
		}

		int Race::GetCamProection() const
		{
			return _camProection;
		}

		void Race::SetCamProection(int value)
		{
			_camProection = value;
		}

		bool Race::GetOilDestroyer() const
		{
			return _oilDestroyer;
		}

		void Race::SetOilDestroyer(bool value)
		{
			_oilDestroyer = value;
		}

		bool Race::GetEnableMineBug() const
		{
			return _enableMineBug;
		}

		void Race::SetEnableMineBug(bool value)
		{
			_enableMineBug = value;
		}

		void Race::SendEvent(unsigned id, EventData* data)
		{
			_game->SendEvent(id, data);
		}

		void Race::OnLapPass(Player* player)
		{
			bool isHuman = player->IsHuman();

			unsigned int lapscount = _tournament->GetCurTrack().GetLapsCount();
			unsigned int curLap = player->GetCar().numLaps;
			unsigned int kickPlace = 0;

			//в режиме "выбывание" количество кругов всегда равно количеству игроков. 
			if (GetSurvivalMode())
			{
				if (GetGame()->GetMenu()->IsNetGame())
				{
					lapscount = (TOTALPLAYERS_COUNT - SPECTATORS_COUNT) - 1;
					//выгоняем игрока который последним проходит круг.
					kickPlace = 2 + lapscount - curLap;
					if (player->GetPlace() == kickPlace && kickPlace != 1)
					{
						D3DXQUATERNION targetRot = player->GetCar().gameObj->GetRot();
						D3DXVECTOR3 targetPos = player->GetCar().gameObj->GetWorldPos();

						player->GetCar().gameObj->Death();
						player->SetAutoSkip(false);
						player->IsGamer(false);
						player->IsSpectator(true);
						//player->GetCar().gameObj->SetMaxTimeLife(1.5f);
						//player->GetCar().gameObj->GetPxActor().GetNxActor()->setLinearVelocity(NxVec3(ZVector * 25));
						if (player->IsHuman())
						{
							GetWorld()->GetCamera()->ChangeStyle(CameraManager::csFreeView);
							GetWorld()->GetCamera()->FlyTo(targetPos + (ZVector * 20.0f), targetRot, 2.0f, false, true);
							SendEvent(cRaceEscape, nullptr);
						}
					}
				}
			}

			if (player->GetCar().numLaps >= lapscount)
			{
				player->SetAutoSkip(false);
				CompleteRace(player);

				if (_results.size() == 1)
					SendEvent(cPlayerLeadFinish, &EventData(player->GetId()));
				else if (_results.size() == 2)
					SendEvent(cPlayerSecondFinish, &EventData(player->GetId()));
				else if (_results.size() == 3)
					SendEvent(cPlayerThirdFinish, &EventData(player->GetId()));
				else if (_results.size() == _playerList.size())
					SendEvent(cPlayerLastFinish, &EventData(player->GetId()));

				//услвоия завершения гонки, double completion def
				bool isRaceComplete = isHuman || (_results.size() >= _playerList.size() && GetPlayerById(cHuman) ==
					nullptr);
				if (isRaceComplete)
				{
					SendEvent(cRaceFinish, nullptr);
				}
			}

			if (isHuman)
				SendEvent(cRacePassLap, &EventData(cHuman));

			if (isHuman && player->GetCar().numLaps == lapscount - 1)
			{
				Player* leader = player;

				for (PlayerList::const_iterator iter = _playerList.begin(); iter != _playerList.end(); ++iter)
					if ((*iter)->GetPlace() == 1)
					{
						leader = *iter;
						break;
					}

				SendEvent(cRaceLastLap, &EventData(leader->GetId()));

				if (GetPlayerById(cHuman)->GetCar().gameObj != nullptr && HUD_STYLE == 3)
				{
					GameObjListener* bt = nullptr;
					GetPlayerById(cHuman)->GetCar().gameObj->GetLogic()->TakeBonus(
						GetPlayerById(cHuman)->GetCar().gameObj, GetPlayerById(
							cHuman)->GetCar().gameObj, bt->btLastLap, 1.0f, false);
				}
			}
		}

		void Race::QuickFinish(Player* player)
		{
			CompleteRace(player);
		}

		GameMode* Race::GetGame()
		{
			return _game;
		}

		World* Race::GetWorld()
		{
			return _game->GetWorld();
		}

		DataBase* Race::GetDB()
		{
			return GetWorld()->GetDB();
		}

		Map* Race::GetMap()
		{
			return GetWorld()->GetMap();
		}

		Workshop& Race::GetWorkshop()
		{
			return *_workshop;
		}

		Garage& Race::GetGarage()
		{
			return *_garage;
		}

		Tournament& Race::GetTournament()
		{
			return *_tournament;
		}

		AchievmentModel& Race::GetAchievment()
		{
			return *_achievment;
		}

		const Race::Planets& Race::GetPlanetsCompleted() const
		{
			return _planetsCompleted;
		}

		const Race::Result* Race::GetResult(int playerId) const
		{
			for (auto iter = _results.begin(); iter != _results.end(); ++iter)
			{
				if (iter->playerId == playerId)
					return &(*iter);
			}

			return nullptr;
		}

		const Race::Results& Race::GetResults() const
		{
			return _results;
		}

		bool Race::GetPlanetChampion() const
		{
			return _planetChampion;
		}

		bool Race::GetPassChampion() const
		{
			return _passChampion;
		}

		void Race::ResetChampion()
		{
			_planetChampion = false;
			_passChampion = false;
		}

		int Race::GetTotalPoints() const
		{
			int totalPoints = 0;

			for (auto iter = _playerList.begin(); iter != _playerList.end(); ++iter)
			{
				Player* player = *iter;

				if (player->IsOpponent() || player->IsHuman())
				{
					totalPoints += player->GetPoints();
				}
			}

			return totalPoints;
		}

		MapObjRec* Race::GetMapObjRec(MapObjLib::Category category, const std::string& name)
		{
			return GetDB()->GetRecord(category, name);
		}

		MapObjRec* Race::GetCar(const std::string& name)
		{
			return GetDB()->GetRecord(MapObjLib::ctCar, name);
		}

		Record* Race::GetSlot(const std::string& name)
		{
			return &_workshop->GetRecord(name);
		}

		graph::Tex2DResource* Race::GetTexture(const std::string& name)
		{
			return GetWorld()->GetResManager()->GetImageLib().Get(name).GetOrCreateTex2d();
		}

		graph::IndexedVBMesh* Race::GetMesh(const std::string& name)
		{
			return GetWorld()->GetResManager()->GetMeshLib().Get(name).GetOrCreateIVBMesh();
		}

		graph::LibMaterial* Race::GetLibMat(const std::string& name)
		{
			return &GetWorld()->GetResManager()->GetMatLib().Get(name);
		}

		const std::string& Race::GetString(StringValue value)
		{
			return GetWorld()->GetResManager()->GetStringLib().Get(value);
		}

		bool Race::IsHumanId(int id)
		{
			return id == cHuman;
		}

		bool Race::IsComputerId(int id)
		{
			return (id & cComputerMask) != 0;
		}

		bool Race::IsOpponentId(int id)
		{
			return (id & cOpponentMask) != 0;
		}
	}
}
