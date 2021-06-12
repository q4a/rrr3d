#include "stdafx.h"
#include "game\World.h"

#include "game\Race.h"
#include "lslSerialFileXml.h"

#ifdef _DEBUG
	//#define DEBUG_WEAPON 1
#endif

namespace r3d
{

namespace game
{

const float Race::cSellDiscount = 0.5f;
const std::string Planet::cWorldTypeStr[cWorldTypeEnd] = {"wtWorld1", "wtWorld2", "wtWorld3", "wtWorld4", "wtWorld5", "wtWorld6"};




Workshop::Workshop(Race* race): _race(race), _lib(NULL)
{
	_rootNode = new lsl::RootNode("workshopRoot", race);

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
	Slot* slot = new Slot(0);
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

void Workshop::LoadWheels()
{
	{
		Slot* slot = new Slot(0);

		WheelItem& item = slot->CreateItem<WheelItem>();
		item.SetName(_SC(scWheel1));
		item.SetInfo(_SC(scWheel1Info));
		item.SetMesh(_race->GetMesh("Upgrade\\wheel1.r3d"));
		item.SetTexture(_race->GetTexture("Upgrade\\wheel1.jpg"));
		item.SetCost(0);

		//marauder
		{
			WheelItem::CarFunc& func = item.carFuncMap[_race->GetCar("marauder")];
			//long
			func.longTire.extremumSlip   = 0.5f;
			func.longTire.extremumValue  = 6.0f;
			func.longTire.asymptoteSlip  = 3.0f;
			func.longTire.asymptoteValue = 5.9f;
			//lateral
			func.latTire.extremumSlip    = 0.3f;
			func.latTire.extremumValue   = 2.25f;
			func.latTire.asymptoteSlip   = 2.0f;
			func.latTire.asymptoteValue  = 1.0f;
		}		
		//dirtdevil
		{
			WheelItem::CarFunc& func = item.carFuncMap[_race->GetCar("dirtdevil")];
			//long
			func.longTire.extremumSlip   = 0.5f;
			func.longTire.extremumValue  = 6.0f;
			func.longTire.asymptoteSlip  = 3.0f;
			func.longTire.asymptoteValue = 5.9f;
			//lateral
			func.latTire.extremumSlip    = 0.3f;
			func.latTire.extremumValue   = 2.25f;
			func.latTire.asymptoteSlip   = 2.0f;
			func.latTire.asymptoteValue  = 1.0f;
		}		
		//manticora
		{
			WheelItem::CarFunc& func = item.carFuncMap[_race->GetCar("manticora")];
			//long
			func.longTire.extremumSlip   = 0.5f;
			func.longTire.extremumValue  = 6.5f;
			func.longTire.asymptoteSlip  = 3.0f;
			func.longTire.asymptoteValue = 6.4f;
			//lateral
			func.latTire.extremumSlip    = 0.3f;
			func.latTire.extremumValue   = 2.25f;
			func.latTire.asymptoteSlip   = 2.0f;
			func.latTire.asymptoteValue  = 1.0f;
			//boss
			WheelItem::CarFunc& boss = item.carFuncMap[_race->GetCar("manticoraBoss")];
			boss = func;
		}
		//monstertruck
		{
			WheelItem::CarFunc& func = item.carFuncMap[_race->GetCar("monstertruck")];
			//long
			func.longTire.extremumSlip   = 0.8f;
			func.longTire.extremumValue  = 8.0f;
			func.longTire.asymptoteSlip  = 3.0f;
			func.longTire.asymptoteValue = 7.9f;
			//lateral
			func.latTire.extremumSlip    = 0.3f;
			func.latTire.extremumValue   = 1.75f;
			func.latTire.asymptoteSlip   = 2.0f;
			func.latTire.asymptoteValue  = 1.0f;
			//boss
			WheelItem::CarFunc& boss = item.carFuncMap[_race->GetCar("monstertruckBoss")];
			boss = func;
		}
		
		SaveSlot(slot, "wheel1");
	}
	{
		Slot* slot = new Slot(0);

		WheelItem& item = slot->CreateItem<WheelItem>();
		item.SetName(_SC(scWheel2));
		item.SetInfo(_SC(scWheel2Info));
		item.SetMesh(_race->GetMesh("Upgrade\\wheel2.r3d"));
		item.SetTexture(_race->GetTexture("Upgrade\\wheel2.jpg"));
		item.SetCost(16000);

		//marauder
		{
			WheelItem::CarFunc& func = item.carFuncMap[_race->GetCar("marauder")];
			//long
			func.longTire.extremumSlip   = 0.5f;
			func.longTire.extremumValue  = 6.0f;
			func.longTire.asymptoteSlip  = 3.0f;
			func.longTire.asymptoteValue = 5.9f;
			//lateral
			func.latTire.extremumSlip    = 0.3f;
			func.latTire.extremumValue   = 2.6f;
			func.latTire.asymptoteSlip   = 2.0f;
			func.latTire.asymptoteValue  = 1.25f;
		}		
		//dirtdevil
		{
			WheelItem::CarFunc& func = item.carFuncMap[_race->GetCar("dirtdevil")];			
			//long
			func.longTire.extremumSlip   = 0.5f;
			func.longTire.extremumValue  = 6.0f;
			func.longTire.asymptoteSlip  = 3.0f;
			func.longTire.asymptoteValue = 5.9f;
			//lateral
			func.latTire.extremumSlip    = 0.3f;
			func.latTire.extremumValue   = 2.6f;
			func.latTire.asymptoteSlip   = 2.0f;
			func.latTire.asymptoteValue  = 1.0f;
		}		
		//manticora
		{
			WheelItem::CarFunc& func = item.carFuncMap[_race->GetCar("manticora")];			
			//long
			func.longTire.extremumSlip   = 0.5f;
			func.longTire.extremumValue  = 6.5f;
			func.longTire.asymptoteSlip  = 3.0f;
			func.longTire.asymptoteValue = 6.4f;
			//lateral
			func.latTire.extremumSlip    = 0.3f;
			func.latTire.extremumValue   = 2.6f;
			func.latTire.asymptoteSlip   = 2.0f;
			func.latTire.asymptoteValue  = 1.25f;
			//boss
			WheelItem::CarFunc& boss = item.carFuncMap[_race->GetCar("manticoraBoss")];
			boss = func;
		}
		//monstertruck
		{
			WheelItem::CarFunc& func = item.carFuncMap[_race->GetCar("monstertruck")];			
			//long
			func.longTire.extremumSlip   = 0.5f;
			func.longTire.extremumValue  = 6.0f;
			func.longTire.asymptoteSlip  = 3.0f;
			func.longTire.asymptoteValue = 5.9f;
			//lateral
			func.latTire.extremumSlip    = 0.3f;
			func.latTire.extremumValue   = 2.1f;
			func.latTire.asymptoteSlip   = 2.0f;
			func.latTire.asymptoteValue  = 1.25f;
			//boss
			WheelItem::CarFunc& boss = item.carFuncMap[_race->GetCar("monstertruckBoss")];
			boss = func;
		}
		
		SaveSlot(slot, "wheel2");
	}
	{
		Slot* slot = new Slot(0);

		WheelItem& item = slot->CreateItem<WheelItem>();
		item.SetName(_SC(scWheel3));
		item.SetInfo(_SC(scWheel3Info));
		item.SetMesh(_race->GetMesh("Upgrade\\wheel3.r3d"));
		item.SetTexture(_race->GetTexture("Upgrade\\wheel3.jpg"));
		item.SetCost(65000);

		//marauder
		{
			WheelItem::CarFunc& func = item.carFuncMap[_race->GetCar("marauder")];
			//long
			func.longTire.extremumSlip   = 0.5f;
			func.longTire.extremumValue  = 6.0f;
			func.longTire.asymptoteSlip  = 3.0f;
			func.longTire.asymptoteValue = 5.9f;
			//lateral
			func.latTire.extremumSlip    = 0.3f;
			func.latTire.extremumValue   = 3.0f;
			func.latTire.asymptoteSlip   = 2.0f;
			func.latTire.asymptoteValue  = 1.5f;
		}		
		//buggi
		{
			WheelItem::CarFunc& func = item.carFuncMap[_race->GetCar("buggi")];			
			//long
			func.longTire.extremumSlip   = 0.4f;
			func.longTire.extremumValue  = 6.0f;
			func.longTire.asymptoteSlip  = 3.0f;
			func.longTire.asymptoteValue = 5.9f;
			//lateral
			func.latTire.extremumSlip    = 0.3f;
			func.latTire.extremumValue   = 2.5f;
			func.latTire.asymptoteSlip   = 2.0f;
			func.latTire.asymptoteValue  = 1.5f;
		}
		//dirtdevil
		{
			WheelItem::CarFunc& func = item.carFuncMap[_race->GetCar("dirtdevil")];
			//long
			func.longTire.extremumSlip   = 0.5f;
			func.longTire.extremumValue  = 6.0f;
			func.longTire.asymptoteSlip  = 3.0f;
			func.longTire.asymptoteValue = 5.9f;
			//lateral
			func.latTire.extremumSlip    = 0.3f;
			func.latTire.extremumValue   = 3.0f;
			func.latTire.asymptoteSlip   = 2.0f;
			func.latTire.asymptoteValue  = 1.0f;
		}
		//tankchetti
		{
			WheelItem::CarFunc& func = item.carFuncMap[_race->GetCar("tankchetti")];
			//long
			func.longTire.extremumSlip   = 0.35f;
			func.longTire.extremumValue  = 6.0f;
			func.longTire.asymptoteSlip  = 3.0f;
			func.longTire.asymptoteValue = 5.9f;
			//lateral
			func.latTire.extremumSlip    = 0.3f;
			func.latTire.extremumValue   = 2.5f;
			func.latTire.asymptoteSlip   = 2.0f;
			func.latTire.asymptoteValue  = 1.0f;
		}
		//manticora
		{
			WheelItem::CarFunc& func = item.carFuncMap[_race->GetCar("manticora")];
			//long
			func.longTire.extremumSlip   = 0.5f;
			func.longTire.extremumValue  = 6.5f;
			func.longTire.asymptoteSlip  = 3.0f;
			func.longTire.asymptoteValue = 6.4f;
			//lateral
			func.latTire.extremumSlip    = 0.3f;
			func.latTire.extremumValue   = 3.0f;
			func.latTire.asymptoteSlip   = 2.0f;
			func.latTire.asymptoteValue  = 1.5f;
			//boss
			WheelItem::CarFunc& boss = item.carFuncMap[_race->GetCar("manticoraBoss")];
			boss = func;
		}
		//monstertruck
		{
			WheelItem::CarFunc& func = item.carFuncMap[_race->GetCar("monstertruck")];			
			//long
			func.longTire.extremumSlip   = 0.5f;
			func.longTire.extremumValue  = 6.0f;
			func.longTire.asymptoteSlip  = 3.0f;
			func.longTire.asymptoteValue = 5.9f;
			//lateral
			func.latTire.extremumSlip    = 0.3f;
			func.latTire.extremumValue   = 2.5f;
			func.latTire.asymptoteSlip   = 2.0f;
			func.latTire.asymptoteValue  = 1.5f;
			//boss
			WheelItem::CarFunc& boss = item.carFuncMap[_race->GetCar("monstertruckBoss")];
			boss = func;
		}

		SaveSlot(slot, "wheel3");
	}

	{
		Slot* slot = new Slot(0);

		WheelItem& item = slot->CreateItem<WheelItem>();
		item.SetName(_SC(scGuseniza1));
		item.SetInfo(_SC(scGuseniza1Info));
		item.SetMesh(_race->GetMesh("Upgrade\\gusWheel1.r3d"));
		item.SetTexture(_race->GetTexture("Upgrade\\gusWheel1.jpg"));
		item.SetCost(0);

		//guseniza
		{
			WheelItem::CarFunc& func = item.carFuncMap[_race->GetCar("guseniza")];
			//long
			func.longTire.extremumSlip   = 0.4f;
			func.longTire.extremumValue  = 6.0f;
			func.longTire.asymptoteSlip  = 3.0f;
			func.longTire.asymptoteValue = 5.9f;
			//lateral
			func.latTire.extremumSlip    = 0.3f;
			func.latTire.extremumValue   = 2.0f;
			func.latTire.asymptoteSlip   = 2.0f;
			func.latTire.asymptoteValue  = 2.0f;
			//boss
			WheelItem::CarFunc& boss = item.carFuncMap[_race->GetCar("gusenizaBoss")];
			boss = func;
		}
		
		SaveSlot(slot, "gusWheel1");
	}
	{
		Slot* slot = new Slot(0);

		WheelItem& item = slot->CreateItem<WheelItem>();
		item.SetName(_SC(scGuseniza2));
		item.SetInfo(_SC(scGuseniza2Info));
		item.SetMesh(_race->GetMesh("Upgrade\\gusWheel2.r3d"));
		item.SetTexture(_race->GetTexture("Upgrade\\gusWheel1.jpg"));
		item.SetCost(31000);

		//guseniza
		{
			WheelItem::CarFunc& func = item.carFuncMap[_race->GetCar("guseniza")];
			//long
			func.longTire.extremumSlip   = 0.4f;
			func.longTire.extremumValue  = 6.0f;
			func.longTire.asymptoteSlip  = 3.0f;
			func.longTire.asymptoteValue = 5.9f;
			//lateral
			func.latTire.extremumSlip    = 0.3f;
			func.latTire.extremumValue   = 2.5f;
			func.latTire.asymptoteSlip   = 2.0f;
			func.latTire.asymptoteValue  = 2.5f;
			//boss
			WheelItem::CarFunc& boss = item.carFuncMap[_race->GetCar("gusenizaBoss")];
			boss = func;
		}
		
		SaveSlot(slot, "gusWheel2");
	}
	{
		Slot* slot = new Slot(0);

		WheelItem& item = slot->CreateItem<WheelItem>();
		item.SetName(_SC(scGuseniza3));
		item.SetInfo(_SC(scGuseniza3Info));
		item.SetMesh(_race->GetMesh("Upgrade\\gusWheel3.r3d"));
		item.SetTexture(_race->GetTexture("Upgrade\\gusWheel3.jpg"));
		item.SetCost(74000);

		//guseniza
		{
			WheelItem::CarFunc& func = item.carFuncMap[_race->GetCar("guseniza")];
			//long
			func.longTire.extremumSlip   = 0.4f;
			func.longTire.extremumValue  = 6.0f;
			func.longTire.asymptoteSlip  = 3.0f;
			func.longTire.asymptoteValue = 5.9f;
			//lateral
			func.latTire.extremumSlip    = 0.3f;
			func.latTire.extremumValue   = 3.0f;
			func.latTire.asymptoteSlip   = 2.0f;
			func.latTire.asymptoteValue  = 3.0f;
			//boss
			WheelItem::CarFunc& boss = item.carFuncMap[_race->GetCar("gusenizaBoss")];
			boss = func;
		}

		SaveSlot(slot, "gusWheel3");
	}
}

void Workshop::LoadTruba()
{
	{
		Slot* slot = new Slot(0);

		TrubaItem& item = slot->CreateItem<TrubaItem>();
		item.SetName(_SC(scTruba1));
		item.SetInfo(_SC(scTruba1Info));
		item.SetMesh(_race->GetMesh("Upgrade\\truba1.r3d"));
		item.SetTexture(_race->GetTexture("Upgrade\\truba1.jpg"));
		item.SetCost(0);

		//marauder
		{
			WheelItem::CarFunc& func = item.carFuncMap[_race->GetCar("marauder")];
			func.maxTorque = 100.0f;
		}		
		//dirtdevil
		{
			WheelItem::CarFunc& func = item.carFuncMap[_race->GetCar("dirtdevil")];
			func.maxTorque = 100.0f;
		}		
		//manticora
		{
			WheelItem::CarFunc& func = item.carFuncMap[_race->GetCar("manticora")];
			func.maxTorque = 100.0f;
			//boss
			WheelItem::CarFunc& boss = item.carFuncMap[_race->GetCar("manticoraBoss")];
			boss = func;
		}
		//guseniza
		{
			WheelItem::CarFunc& func = item.carFuncMap[_race->GetCar("guseniza")];
			func.maxTorque = 100.0f;
			//boss
			WheelItem::CarFunc& boss = item.carFuncMap[_race->GetCar("gusenizaBoss")];
			boss = func;
		}
		//monstertruck
		{
			WheelItem::CarFunc& func = item.carFuncMap[_race->GetCar("monstertruck")];			
			func.maxTorque = 50.0f;
			//boss
			WheelItem::CarFunc& boss = item.carFuncMap[_race->GetCar("monstertruckBoss")];
			boss = func;
		}

		SaveSlot(slot, "truba1");
	}
	{
		Slot* slot = new Slot(0);

		TrubaItem& item = slot->CreateItem<TrubaItem>();
		item.SetName(_SC(scTruba2));
		item.SetInfo(_SC(scTruba2Info));
		item.SetMesh(_race->GetMesh("Upgrade\\truba2.r3d"));
		item.SetTexture(_race->GetTexture("Upgrade\\truba2.jpg"));
		item.SetCost(16000);

		//marauder
		{
			WheelItem::CarFunc& func = item.carFuncMap[_race->GetCar("marauder")];
			func.maxTorque = 150.0f;
		}		
		//dirtdevil
		{
			WheelItem::CarFunc& func = item.carFuncMap[_race->GetCar("dirtdevil")];
			func.maxTorque = 150.0f;
		}		
		//manticora
		{
			WheelItem::CarFunc& func = item.carFuncMap[_race->GetCar("manticora")];			
			func.maxTorque = 200.0f;
			//boss
			WheelItem::CarFunc& boss = item.carFuncMap[_race->GetCar("manticoraBoss")];
			boss = func;
		}
		//guseniza
		{
			WheelItem::CarFunc& func = item.carFuncMap[_race->GetCar("guseniza")];
			func.maxTorque = 150.0f;
			//boss
			WheelItem::CarFunc& boss = item.carFuncMap[_race->GetCar("gusenizaBoss")];
			boss = func;
		}
		//monstertruck
		{
			WheelItem::CarFunc& func = item.carFuncMap[_race->GetCar("monstertruck")];			
			func.maxTorque = 65.0f;
			//boss
			WheelItem::CarFunc& boss = item.carFuncMap[_race->GetCar("monstertruckBoss")];
			boss = func;
		}
		
		SaveSlot(slot, "truba2");
	}
	{
		Slot* slot = new Slot(0);

		TrubaItem& item = slot->CreateItem<TrubaItem>();
		item.SetName(_SC(scTruba3));
		item.SetInfo(_SC(scTruba3Info));
		item.SetMesh(_race->GetMesh("Upgrade\\truba3.r3d"));
		item.SetTexture(_race->GetTexture("Upgrade\\truba3.jpg"));
		item.SetCost(36000);

		//marauder
		{
			WheelItem::CarFunc& func = item.carFuncMap[_race->GetCar("marauder")];
			func.maxTorque = 200.0f;
		}
		//buggi
		{
			WheelItem::CarFunc& func = item.carFuncMap[_race->GetCar("buggi")];			
			func.maxTorque = 200.0f;
		}
		//dirtdevil		
		{
			WheelItem::CarFunc& func = item.carFuncMap[_race->GetCar("dirtdevil")];
			func.maxTorque = 200.0f;
		}
		//tankchetti
		{
			WheelItem::CarFunc& func = item.carFuncMap[_race->GetCar("tankchetti")];			
			func.maxTorque = 200.0f;
		}
		//manticora
		{
			WheelItem::CarFunc& func = item.carFuncMap[_race->GetCar("manticora")];		
			func.maxTorque = 300.0f;
			//boss
			WheelItem::CarFunc& boss = item.carFuncMap[_race->GetCar("manticoraBoss")];
			boss = func;
		}
		//guseniza
		{
			WheelItem::CarFunc& func = item.carFuncMap[_race->GetCar("guseniza")];			
			func.maxTorque = 200.0f;
			//boss
			WheelItem::CarFunc& boss = item.carFuncMap[_race->GetCar("gusenizaBoss")];
			boss = func;
		}
		//monstertruck
		{
			WheelItem::CarFunc& func = item.carFuncMap[_race->GetCar("monstertruck")];			
			func.maxTorque = 80.0f;
			//boss
			WheelItem::CarFunc& boss = item.carFuncMap[_race->GetCar("monstertruckBoss")];
			boss = func;
		}
		
		SaveSlot(slot, "truba3");
	}
}

void Workshop::LoadEngines()
{
	{
		Slot* slot = new Slot(0);

		MotorItem& item = slot->CreateItem<MotorItem>();
		item.SetName(_SC(scEngine1));
		item.SetInfo(_SC(scEngine1Info));
		item.SetMesh(_race->GetMesh("Upgrade\\engine1.r3d"));
		item.SetTexture(_race->GetTexture("Upgrade\\engine1.jpg"));
		item.SetCost(0);

		//marauder
		{
			MotorItem::CarFunc& func = item.carFuncMap[_race->GetCar("marauder")];
			func.maxTorque = 800.0f;
		}		
		//dirtdevil
		{
			MotorItem::CarFunc& func = item.carFuncMap[_race->GetCar("dirtdevil")];
			func.maxTorque = 800.0f;			
		}		
		//manticora
		{
			MotorItem::CarFunc& func = item.carFuncMap[_race->GetCar("manticora")];
			func.maxTorque = 1100.0f;
			//boss
			MotorItem::CarFunc& boss = item.carFuncMap[_race->GetCar("manticoraBoss")];
			boss = func;
		}
		//guseniza
		{
			MotorItem::CarFunc& func = item.carFuncMap[_race->GetCar("guseniza")];
			func.maxTorque = 1100.0f;
			//boss
			MotorItem::CarFunc& boss = item.carFuncMap[_race->GetCar("gusenizaBoss")];
			boss = func;
		}
		//podushka
		{
			MotorItem::CarFunc& func = item.carFuncMap[_race->GetCar("podushka")];
			func.maxTorque = 750.0f;
			
			//long
			func.longTire.extremumSlip   = 0.3f;
			func.longTire.extremumValue  = 6.0f;
			func.longTire.asymptoteSlip  = 3.0f;
			func.longTire.asymptoteValue = 5.9f;
			//lateral
			func.latTire.extremumSlip    = 0.3f;
			func.latTire.extremumValue   = 2.5f;
			func.latTire.asymptoteSlip   = 2.0f;
			func.latTire.asymptoteValue  = 2.5f;
		}
		//monstertruck
		{
			MotorItem::CarFunc& func = item.carFuncMap[_race->GetCar("monstertruck")];
			func.maxTorque = 300.0f;
			//boss
			MotorItem::CarFunc& boss = item.carFuncMap[_race->GetCar("monstertruckBoss")];
			boss = func;
		}
		//devildriver
		{
			MotorItem::CarFunc& func = item.carFuncMap[_race->GetCar("devildriver")];
			func.maxTorque = 450.0f;

			//long
			func.longTire.extremumSlip   = 0.35f;
			func.longTire.extremumValue  = 6.0f;
			func.longTire.asymptoteSlip  = 3.0f;
			func.longTire.asymptoteValue = 5.9f;
			//lateral
			func.latTire.extremumSlip    = 0.3f;
			func.latTire.extremumValue   = 2.5f;
			func.latTire.asymptoteSlip   = 2.0f;
			func.latTire.asymptoteValue  = 1.5f;

			//boss
			MotorItem::CarFunc& boss = item.carFuncMap[_race->GetCar("devildriverBoss")];
			boss = func;
		}

		SaveSlot(slot, "engine1");
	}
	{
		Slot* slot = new Slot(0);

		MotorItem& item = slot->CreateItem<MotorItem>();
		item.SetName(_SC(scEngine2));
		item.SetInfo(_SC(scEngine2Info));
		item.SetMesh(_race->GetMesh("Upgrade\\engine2.r3d"));
		item.SetTexture(_race->GetTexture("Upgrade\\engine2.jpg"));
		item.SetCost(25000);

		//marauder
		{
			MotorItem::CarFunc& func = item.carFuncMap[_race->GetCar("marauder")];
			func.maxTorque = 1000.0f;
		}		
		//dirtdevil
		{
			MotorItem::CarFunc& func = item.carFuncMap[_race->GetCar("dirtdevil")];
			func.maxTorque = 1000.0f;			
		}		
		//manticora
		{
			MotorItem::CarFunc& func = item.carFuncMap[_race->GetCar("manticora")];
			func.maxTorque = 1500.0f;			
			//boss
			MotorItem::CarFunc& boss = item.carFuncMap[_race->GetCar("manticoraBoss")];
			boss = func;
		}
		//guseniza
		{
			MotorItem::CarFunc& func = item.carFuncMap[_race->GetCar("guseniza")];
			func.maxTorque = 1350.0f;
			//boss
			MotorItem::CarFunc& boss = item.carFuncMap[_race->GetCar("gusenizaBoss")];
			boss = func;
		}
		//podushka
		{
			MotorItem::CarFunc& func = item.carFuncMap[_race->GetCar("podushka")];
			func.maxTorque = 900.0f;

			//long
			func.longTire.extremumSlip   = 0.3f;
			func.longTire.extremumValue  = 6.0f;
			func.longTire.asymptoteSlip  = 3.0f;
			func.longTire.asymptoteValue = 5.9f;
			//lateral
			func.latTire.extremumSlip    = 0.3f;
			func.latTire.extremumValue   = 3.0f;
			func.latTire.asymptoteSlip   = 2.0f;
			func.latTire.asymptoteValue  = 3.0f;
		}
		//monstertruck
		{
			MotorItem::CarFunc& func = item.carFuncMap[_race->GetCar("monstertruck")];
			func.maxTorque = 360.0f;
			//boss
			MotorItem::CarFunc& boss = item.carFuncMap[_race->GetCar("monstertruckBoss")];
			boss = func;
		}
		//devildriver
		{
			MotorItem::CarFunc& func = item.carFuncMap[_race->GetCar("devildriver")];
			func.maxTorque = 600.0f;

			//long
			func.longTire.extremumSlip   = 0.4f;
			func.longTire.extremumValue  = 6.0f;
			func.longTire.asymptoteSlip  = 3.0f;
			func.longTire.asymptoteValue = 5.9f;
			//lateral
			func.latTire.extremumSlip    = 0.3f;
			func.latTire.extremumValue   = 2.75f;
			func.latTire.asymptoteSlip   = 2.0f;
			func.latTire.asymptoteValue  = 1.5f;

			//boss
			MotorItem::CarFunc& boss = item.carFuncMap[_race->GetCar("devildriverBoss")];
			boss = func;
		}
		
		SaveSlot(slot, "engine2");
	}
	{
		Slot* slot = new Slot(0);

		MotorItem& item = slot->CreateItem<MotorItem>();
		item.SetName(_SC(scEngine3));
		item.SetInfo(_SC(scEngine3Info));
		item.SetMesh(_race->GetMesh("Upgrade\\engine3.r3d"));
		item.SetTexture(_race->GetTexture("Upgrade\\engine3.jpg"));
		item.SetCost(90000);

		//marauder
		{
			MotorItem::CarFunc& func = item.carFuncMap[_race->GetCar("marauder")];
			func.maxTorque = 1200.0f;
		}
		//buggi
		{
			MotorItem::CarFunc& func = item.carFuncMap[_race->GetCar("buggi")];
			func.maxTorque = 1200.0f;
		}
		//dirtdevil
		{
			MotorItem::CarFunc& func = item.carFuncMap[_race->GetCar("dirtdevil")];
			func.maxTorque = 1200.0f;			
		}
		//tankchetti
		{
			MotorItem::CarFunc& func = item.carFuncMap[_race->GetCar("tankchetti")];
			func.maxTorque = 1300.0f;
		}
		//manticora
		{
			MotorItem::CarFunc& func = item.carFuncMap[_race->GetCar("manticora")];
			func.maxTorque = 1900.0f;			
			//boss
			MotorItem::CarFunc& boss = item.carFuncMap[_race->GetCar("manticoraBoss")];
			boss = func;
		}
		//guseniza
		{
			MotorItem::CarFunc& func = item.carFuncMap[_race->GetCar("guseniza")];
			func.maxTorque = 1600.0f;
			//boss
			MotorItem::CarFunc& boss = item.carFuncMap[_race->GetCar("gusenizaBoss")];
			boss = func;
		}
		//podushka
		{
			MotorItem::CarFunc& func = item.carFuncMap[_race->GetCar("podushka")];
			func.maxTorque = 1100.0f;

			//long
			func.longTire.extremumSlip   = 0.3f;
			func.longTire.extremumValue  = 6.0f;
			func.longTire.asymptoteSlip  = 3.0f;
			func.longTire.asymptoteValue = 5.9f;
			//lateral
			func.latTire.extremumSlip    = 0.3f;
			func.latTire.extremumValue   = 3.5f;
			func.latTire.asymptoteSlip   = 2.0f;
			func.latTire.asymptoteValue  = 3.5f;
		}
		//monstertruck
		{
			MotorItem::CarFunc& func = item.carFuncMap[_race->GetCar("monstertruck")];
			func.maxTorque = 420.0f;
			//boss
			MotorItem::CarFunc& boss = item.carFuncMap[_race->GetCar("monstertruckBoss")];
			boss = func;
		}
		//devildriver
		{
			MotorItem::CarFunc& func = item.carFuncMap[_race->GetCar("devildriver")];
			func.maxTorque = 700.0f;

			//long
			func.longTire.extremumSlip   = 0.4f;
			func.longTire.extremumValue  = 6.0f;
			func.longTire.asymptoteSlip  = 3.0f;
			func.longTire.asymptoteValue = 5.9f;
			//lateral
			func.latTire.extremumSlip    = 0.3f;
			func.latTire.extremumValue   = 3.0f;
			func.latTire.asymptoteSlip   = 2.0f;
			func.latTire.asymptoteValue  = 1.5f;

			//boss
			MotorItem::CarFunc& boss = item.carFuncMap[_race->GetCar("devildriverBoss")];
			boss = func;
		}

		SaveSlot(slot, "engine3");
	}	
}

void Workshop::LoadArmors()
{
	{
		Slot* slot = new Slot(0);

		ArmorItem& item = slot->CreateItem<ArmorItem>();
		item.SetName(_SC(scArmor1));
		item.SetInfo(_SC(scArmor1Info));
		item.SetMesh(_race->GetMesh("Upgrade\\armor1.r3d"));
		item.SetTexture(_race->GetTexture("Upgrade\\armor1.jpg"));
		item.SetCost(0);

		//marauder
		{
			ArmorItem::CarFunc& func = item.carFuncMap[_race->GetCar("marauder")];
			func.life = 50.0f;
		}		
		//dirtdevil
		{
			ArmorItem::CarFunc& func = item.carFuncMap[_race->GetCar("dirtdevil")];
			func.life = 35.0f;
		}		
		//manticora
		{
			ArmorItem::CarFunc& func = item.carFuncMap[_race->GetCar("manticora")];			
			func.life = 50.0f;
			//boss
			ArmorItem::CarFunc& boss = item.carFuncMap[_race->GetCar("manticoraBoss")];
			boss = func;
		}
		//guseniza
		{
			ArmorItem::CarFunc& func = item.carFuncMap[_race->GetCar("guseniza")];
			func.life = 75.0f;
			//boss
			ArmorItem::CarFunc& boss = item.carFuncMap[_race->GetCar("gusenizaBoss")];
			boss = func;
		}
		//podushka
		{
			ArmorItem::CarFunc& func = item.carFuncMap[_race->GetCar("podushka")];
			func.life = 65.0f;
		}
		//monstertruck
		{
			ArmorItem::CarFunc& func = item.carFuncMap[_race->GetCar("monstertruck")];
			func.life = 100.0f;
			//boss
			ArmorItem::CarFunc& boss = item.carFuncMap[_race->GetCar("monstertruckBoss")];
			boss = func;
		}
		//devildriver
		{
			ArmorItem::CarFunc& func = item.carFuncMap[_race->GetCar("devildriver")];
			func.life = 110.0f;

			//boss
			ArmorItem::CarFunc& boss = item.carFuncMap[_race->GetCar("devildriverBoss")];
			boss = func;
		}
		
		SaveSlot(slot, "armor1");
	}
	{
		Slot* slot = new Slot(0);

		ArmorItem& item = slot->CreateItem<ArmorItem>();
		item.SetName(_SC(scArmor2));
		item.SetInfo(_SC(scArmor2Info));
		item.SetMesh(_race->GetMesh("Upgrade\\armor2.r3d"));
		item.SetTexture(_race->GetTexture("Upgrade\\armor2.jpg"));
		item.SetCost(32000);

		//marauder
		{
			ArmorItem::CarFunc& func = item.carFuncMap[_race->GetCar("marauder")];
			func.life = 65.0f;
		}		
		//dirtdevil
		{
			ArmorItem::CarFunc& func = item.carFuncMap[_race->GetCar("dirtdevil")];
			func.life = 50.0f;
		}		
		//manticora
		{
			ArmorItem::CarFunc& func = item.carFuncMap[_race->GetCar("manticora")];
			func.life = 65.0f;
			//boss
			ArmorItem::CarFunc& boss = item.carFuncMap[_race->GetCar("manticoraBoss")];
			boss = func;
		}
		//guseniza
		{
			ArmorItem::CarFunc& func = item.carFuncMap[_race->GetCar("guseniza")];
			func.life = 95.0f;
			//boss
			ArmorItem::CarFunc& boss = item.carFuncMap[_race->GetCar("gusenizaBoss")];
			boss = func;
		}
		//podushka
		{
			ArmorItem::CarFunc& func = item.carFuncMap[_race->GetCar("podushka")];
			func.life = 85.0f;
		}
		//monstertruck
		{
			ArmorItem::CarFunc& func = item.carFuncMap[_race->GetCar("monstertruck")];
			func.life = 120.0f;
			//boss
			ArmorItem::CarFunc& boss = item.carFuncMap[_race->GetCar("monstertruckBoss")];
			boss = func;
		}
		//devildriver
		{
			ArmorItem::CarFunc& func = item.carFuncMap[_race->GetCar("devildriver")];
			func.life = 150.0f;

			//boss
			ArmorItem::CarFunc& boss = item.carFuncMap[_race->GetCar("devildriverBoss")];
			boss = func;
		}
		
		SaveSlot(slot, "armor2");
	}
	{
		Slot* slot = new Slot(0);

		ArmorItem& item = slot->CreateItem<ArmorItem>();
		item.SetName(_SC(scArmor3));
		item.SetInfo(_SC(scArmor3Info));
		item.SetMesh(_race->GetMesh("Upgrade\\armor3.r3d"));
		item.SetTexture(_race->GetTexture("Upgrade\\armor3.jpg"));
		item.SetCost(70000);

		//marauder
		{
			ArmorItem::CarFunc& func = item.carFuncMap[_race->GetCar("marauder")];
			func.life = 95.0f;
		}
		//buggi
		{
			ArmorItem::CarFunc& func = item.carFuncMap[_race->GetCar("buggi")];
			func.life = 55.0f;
		}
		//dirtdevil
		{
			ArmorItem::CarFunc& func = item.carFuncMap[_race->GetCar("dirtdevil")];
			func.life = 65.5f;
		}
		//tankchetti
		{
			ArmorItem::CarFunc& func = item.carFuncMap[_race->GetCar("tankchetti")];
			func.life = 65.5f;
		}
		//manticora
		{
			ArmorItem::CarFunc& func = item.carFuncMap[_race->GetCar("manticora")];
			func.life = 95.0f;
			//boss
			ArmorItem::CarFunc& boss = item.carFuncMap[_race->GetCar("manticoraBoss")];
			boss = func;
		}
		//guseniza
		{
			ArmorItem::CarFunc& func = item.carFuncMap[_race->GetCar("guseniza")];
			func.life = 120.0f;
			//boss
			ArmorItem::CarFunc& boss = item.carFuncMap[_race->GetCar("gusenizaBoss")];
			boss = func;
		}
		//podushka
		{
			ArmorItem::CarFunc& func = item.carFuncMap[_race->GetCar("podushka")];
			func.life = 100.0f;
		}
		//monstertruck
		{
			ArmorItem::CarFunc& func = item.carFuncMap[_race->GetCar("monstertruck")];
			func.life = 145.0f;
			//boss
			ArmorItem::CarFunc& boss = item.carFuncMap[_race->GetCar("monstertruckBoss")];
			boss = func;
		}
		//devildriver
		{
			ArmorItem::CarFunc& func = item.carFuncMap[_race->GetCar("devildriver")];
			func.life = 170.0f;

			//boss
			ArmorItem::CarFunc& boss = item.carFuncMap[_race->GetCar("devildriverBoss")];
			boss = func;
		}
		
		SaveSlot(slot, "armor3");
	}	
}

void Workshop::LoadWeapons()
{
	{
		Slot* slot = new Slot(0);

		WeaponItem& item = slot->CreateItem<WeaponItem>();
		item.SetName(_SC(scBulletGun));
		item.SetInfo(_SC(scBulletGunInfo));
		item.SetMesh(_race->GetMesh("Weapon\\bulletGun.r3d"));
		item.SetTexture(_race->GetTexture("Car\\buggi.tga"));
		item.SetMapObj(_race->GetDB()->GetRecord(MapObjLib::ctWeapon, "bulletGun"));
		item.SetMaxCharge(35);
		item.SetCntCharge(5);
		item.SetChargeStep(5);
		item.SetDamage(5.0f);
		item.SetCost(0);
		item.SetChargeCost(1000);

		{
			game::Weapon::Desc desc;
			Weapon::ProjDesc projDesc;

			projDesc.type = Proj::ptRocket;
			projDesc.SetModel(_race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\bulletProj"));
			projDesc.pos = D3DXVECTOR3(0.4f, 0.0f, 0.1f);
			projDesc.size = D3DXVECTOR3(2.0f, 0.25f, 1.3f);
			projDesc.modelSize = false;
			projDesc.speed = 50.0f;
			projDesc.speedRelative = true;
			projDesc.maxDist = 500.0f;
			projDesc.mass = 50.0f;
			projDesc.damage = 5.0f;

			desc.projList.push_back(projDesc);
			desc.shotDelay = 0.2f;

			item.SetWpnDesc(desc);
		}

		SaveSlot(slot, "bulletGun");
	}	
	{
		Slot* slot = new Slot(0);

		WeaponItem& item = slot->CreateItem<WeaponItem>();
		item.SetName(_SC(scRifle));
		item.SetInfo(_SC(scRifleInfo));
		item.SetMesh(_race->GetMesh("Weapon\\rifleWeapon.r3d"));
		item.SetTexture(_race->GetTexture("Car\\tankchetti.tga"));
		item.SetMapObj(_race->GetDB()->GetRecord(MapObjLib::ctWeapon, "rifleWeapon"));
		item.SetMaxCharge(7);
		item.SetCntCharge(1);
		item.SetDamage(10.0f);
		item.SetCost(0);
		item.SetChargeCost(8500);

		{
			game::Weapon::Desc desc;
			Weapon::ProjDesc projDesc;

			projDesc.type = Proj::ptRocket;
			projDesc.SetModel(_race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\rifleProj"));
			projDesc.size = IdentityVector * 0.5f;
			projDesc.modelSize = false;
			projDesc.speed = 40.0f;
			projDesc.speedRelative = true;
			projDesc.maxDist = 500.0f;
			projDesc.mass = 100.0f;
			projDesc.damage = 10.0f/2;

			projDesc.pos = D3DXVECTOR3(0.2f, -0.7f, 0.0f);
			desc.projList.push_back(projDesc);

			projDesc.pos = D3DXVECTOR3(0.2f, 0.7f, 0.0f);
			desc.projList.push_back(projDesc);

			desc.shotDelay = 0.2f;
			item.SetWpnDesc(desc);
		}

		SaveSlot(slot, "rifleWeapon");
	}
	{
		Slot* slot = new Slot(0);

		WeaponItem& item = slot->CreateItem<WeaponItem>();
		item.SetName(_SC(scBlaster));
		item.SetInfo(_SC(scBlasterInfo));
		item.SetMesh(_race->GetMesh("Weapon\\blasterGun.r3d"));
		item.SetTexture(_race->GetTexture("Car\\manticora.tga"));
		item.SetMapObj(_race->GetDB()->GetRecord(MapObjLib::ctWeapon, "blasterGun"));
		item.SetMaxCharge(7);
		item.SetCntCharge(1);
		item.SetDamage(12.0f);
		item.SetCost(0);
		item.SetChargeCost(10300);

		{
			game::Weapon::Desc desc;
			Weapon::ProjDesc projDesc;

			projDesc.type = Proj::ptRocket;
			projDesc.SetModel(_race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\blaster"));
			projDesc.pos = D3DXVECTOR3(0.3f, -0.6f, 0.0f);
			projDesc.size = D3DXVECTOR3(1.0f, 0.5f, 0.5f);
			projDesc.modelSize = false;
			projDesc.speed = 40.0f;
			projDesc.speedRelative = true;
			projDesc.maxDist = 500.0f;
			projDesc.mass = 100.0f;
			projDesc.damage = 12.0f/2;
			desc.projList.push_back(projDesc);

			projDesc.pos = D3DXVECTOR3(0.3f, 0.6f, 0.0f);
			desc.projList.push_back(projDesc);

			desc.shotDelay = 0.2f;
			item.SetWpnDesc(desc);
		}

		SaveSlot(slot, "blasterGun");
	}

	{
		Slot* slot = new Slot(0);

		WeaponItem& item = slot->CreateItem<WeaponItem>();
		item.SetName(_SC(scTankLaser));
		item.SetInfo(_SC(scTankLaserInfo));
		item.SetMesh(_race->GetMesh("Weapon\\tankLaser.r3d"));
		item.SetTexture(_race->GetTexture("Car\\guseniza.tga"));
		item.SetMapObj(_race->GetDB()->GetRecord(MapObjLib::ctWeapon, "tankLaser"));
		item.SetMaxCharge(7);
		item.SetCntCharge(1);
		item.SetDamage(19.0f);
		item.SetCost(0);
		item.SetChargeCost(11500);

		{
			game::Weapon::Desc desc;
			Weapon::ProjDesc projDesc;

			projDesc.type = Proj::ptLaser;
			projDesc.SetLibMat(_race->GetLibMat("Effect\\laser3-red2"));
			projDesc.pos = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
			projDesc.size = IdentityVector * 1.0f;
			projDesc.maxDist = 100.0f;
			projDesc.damage = 19.0f/0.5f;
			projDesc.minTimeLife = 0.5f;

			desc.projList.push_back(projDesc);
			desc.shotDelay = 0.5f;

			item.SetWpnDesc(desc);
		}

		SaveSlot(slot, "tankLaser");
	}	

	{
		Slot* slot = new Slot(0);

		WeaponItem& item = slot->CreateItem<WeaponItem>();
		item.SetName(_SC(scSonar));
		item.SetInfo(_SC(scSonarInfo));
		item.SetMesh(_race->GetMesh("Weapon\\sonar.r3d"));
		item.SetTexture(_race->GetTexture("Car\\devildriver.tga"));
		item.SetMapObj(_race->GetDB()->GetRecord(MapObjLib::ctWeapon, "sonar"));
		item.SetMaxCharge(7);
		item.SetCntCharge(1);
		item.SetDamage(27.0f);
		item.SetCost(0);
		item.SetChargeCost(14000);

		{
			game::Weapon::Desc desc;
			Weapon::ProjDesc projDesc;

			projDesc.type = Proj::ptSonar;
			projDesc.SetModel(_race->GetDB()->GetRecord(MapObjLib::ctEffects, "sonar"));
			projDesc.pos = D3DXVECTOR3(0.0f, 0.0f, 0.2f);
			projDesc.size = D3DXVECTOR3(3.0f, 2.0f, 2.0f);
			projDesc.modelSize = false;
			projDesc.maxDist = 15.0f;
			projDesc.mass = 75.0f;
			projDesc.damage = 27.0f/1;
			projDesc.minTimeLife = 1.0f;
			projDesc.speed = 15.0f;
			projDesc.speedRelative = true;

			desc.projList.push_back(projDesc);
			desc.shotDelay = 0.5f;

			item.SetWpnDesc(desc);
		}


		SaveSlot(slot, "sonar");
	}

	{
		Slot* slot = new Slot(0);

		WeaponItem& item = slot->CreateItem<WeaponItem>();
		item.SetName(_SC(scTurel));
		item.SetInfo(_SC(scTurelInfo));
		item.SetMesh(_race->GetMesh("Weapon\\turel.r3d"));
		item.SetTexture(_race->GetTexture("Weapon\\turel.jpg"));
		item.SetMapObj(_race->GetDB()->GetRecord(MapObjLib::ctWeapon, "turel"));
		item.SetMaxCharge(7);
		item.SetCntCharge(1);
		item.SetDamage(13.0f);
		item.SetCost(22000);
		item.SetChargeCost(11100);

		{
			game::Weapon::Desc desc;
			Weapon::ProjDesc projDesc;

			projDesc.type = Proj::ptRocket;
			projDesc.SetModel(_race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\shotBall"));
			projDesc.pos = D3DXVECTOR3(0, 0.0f, 0.1f);
			projDesc.size = D3DXVECTOR3(0.5f, 0.5f, 1.0f);
			projDesc.modelSize = true;
			projDesc.speed = 45.0f;
			projDesc.speedRelative = true;
			projDesc.maxDist = 500.0f;
			projDesc.mass = 100.0f;
			projDesc.damage = 13.0f;

			desc.projList.push_back(projDesc);
			desc.shotDelay = 0.2f;

			item.SetWpnDesc(desc);
		}

		SaveSlot(slot, "turel");
	}

	{
		Slot* slot = new Slot(0);

		WeaponItem& item = slot->CreateItem<WeaponItem>();
		item.SetName(_SC(scPhaseImpulse));
		item.SetInfo(_SC(scPhaseImpulseInfo));
		item.SetMesh(_race->GetMesh("Weapon\\phaseImpulse.r3d"));
		item.SetTexture(_race->GetTexture("Weapon\\phaseImpulse.tga"));
		item.SetMapObj(_race->GetDB()->GetRecord(MapObjLib::ctWeapon, "phaseImpulse"));
		item.SetMaxCharge(7);
		item.SetCntCharge(1);
		item.SetDamage(10.0f);
		item.SetCost(25000);
		item.SetChargeCost(10000);

		{
			game::Weapon::Desc desc;
			Weapon::ProjDesc projDesc;

			projDesc.type = Proj::ptImpulse;
			projDesc.SetModel(_race->GetDB()->GetRecord(MapObjLib::ctEffects, "phaseRing"));
			projDesc.pos = D3DXVECTOR3(0, 0.0f, 0.0f);
			projDesc.size = D3DXVECTOR3(0.5f, 0.5f, 1.0f);
			projDesc.modelSize = false;
			projDesc.speed = 20.0f;
			projDesc.speedRelative = true;
			projDesc.angleSpeed = 0;
			projDesc.maxDist = 100.0f;
			projDesc.mass = 200.0f;
			projDesc.damage = 10.0f;

			desc.projList.push_back(projDesc);
			desc.shotDelay = 0.2f;

			item.SetWpnDesc(desc);
		}

		SaveSlot(slot, "phaseImpulse");
	}

	{
		Slot* slot = new Slot(0);

		WeaponItem& item = slot->CreateItem<WeaponItem>();
		item.SetName(_SC(scRocket));
		item.SetInfo(_SC(scRocketInfo));
		item.SetMesh(_race->GetMesh("Weapon\\rocketLauncher.r3d"));
		item.SetTexture(_race->GetTexture("Weapon\\rocketLauncher.jpg"));
		item.SetMapObj(_race->GetDB()->GetRecord(MapObjLib::ctWeapon, "rocketLauncher"));
		item.SetMaxCharge(7);
		item.SetCntCharge(1);
		item.SetDamage(8.0f);
		item.SetCost(14000);
		item.SetChargeCost(6300);

		{
			game::Weapon::Desc desc;
			Weapon::ProjDesc projDesc;

			projDesc.type = Proj::ptRocket;
			projDesc.SetModel(_race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\rocket"));
			projDesc.pos = D3DXVECTOR3(0, 0.0f, 0.0f);
			projDesc.size = D3DXVECTOR3(0.0f, 0.0f, 1.3f);
			projDesc.modelSize = true;
			projDesc.speed = 40.0f;
			projDesc.speedRelative = true;
			projDesc.maxDist = 500.0f;
			projDesc.mass = 150.0f;
			projDesc.damage = 8.0f;

			desc.projList.push_back(projDesc);
			desc.shotDelay = 0.2f;

			item.SetWpnDesc(desc);
		}

		SaveSlot(slot, "rocketLauncher");
	}
	{
		Slot* slot = new Slot(0);

		WeaponItem& item = slot->CreateItem<WeaponItem>();
		item.SetName(_SC(scPulsator));
		item.SetInfo(_SC(scPulsatorInfo));
		item.SetMesh(_race->GetMesh("Weapon\\pulsator.r3d"));
		item.SetTexture(_race->GetTexture("Weapon\\pulsator.jpg"));
		item.SetMapObj(_race->GetDB()->GetRecord(MapObjLib::ctWeapon, "pulsator"));
		item.SetMaxCharge(7);
		item.SetCntCharge(1);
		item.SetDamage(9.0f);
		item.SetCost(11000);
		item.SetChargeCost(7900);

		{
			game::Weapon::Desc desc;
			Weapon::ProjDesc projDesc;

			projDesc.type = Proj::ptRocket;
			projDesc.SetModel(_race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\laserPulse"));
			projDesc.pos = D3DXVECTOR3(0.0f, 0.0f, 0.1f);
			projDesc.size = D3DXVECTOR3(3.5f, 0.6f, 1.8f);
			projDesc.modelSize = false;
			projDesc.speed = 40.0f;
			projDesc.speedRelative = true;
			projDesc.maxDist = 500.0f;
			projDesc.mass = 150.0f;
			projDesc.damage = 9.0f;

			desc.projList.push_back(projDesc);
			desc.shotDelay = 0.2f;

			item.SetWpnDesc(desc);
		}

		SaveSlot(slot, "pulsator");
	}

	{
		Slot* slot = new Slot(0);

		WeaponItem& item = slot->CreateItem<WeaponItem>();
		item.SetName(_SC(scFireGun));
		item.SetInfo(_SC(scFireGunInfo));
		item.SetMesh(_race->GetMesh("Weapon\\fireGun.r3d"));
		item.SetTexture(_race->GetTexture("Weapon\\fireGun.jpg"));
		item.SetMapObj(_race->GetDB()->GetRecord(MapObjLib::ctWeapon, "fireGun"));
		item.SetMaxCharge(4);
		item.SetCntCharge(1);
		item.SetDamage(16.0f);
		item.SetCost(20000);
		item.SetChargeCost(9800);

		{
			game::Weapon::Desc desc;
			Weapon::ProjDesc projDesc;

			projDesc.type = Proj::ptFire;
			projDesc.SetModel(_race->GetDB()->GetRecord(MapObjLib::ctEffects, "fire2"));
			projDesc.pos = D3DXVECTOR3(0.4f, 0.0f, 0.1f);
			projDesc.size = D3DXVECTOR3(12.0, 1.5f, 1.5f);
			projDesc.offset = D3DXVECTOR3(6.0, 0.0f, 0.0f);
			projDesc.modelSize = false;
			projDesc.speed = 0.0f;
			projDesc.maxDist = 12.0f;
			projDesc.damage = 16.0f/1.6f;
			projDesc.minTimeLife = 1.6f;

			desc.projList.push_back(projDesc);
			desc.shotDelay = 1.5f;

			item.SetWpnDesc(desc);
		}

		SaveSlot(slot, "fireGun");
	}

	{
		Slot* slot = new Slot(0);

		WeaponItem& item = slot->CreateItem<WeaponItem>();
		item.SetName(_SC(scDrobilka));
		item.SetInfo(_SC(scDrobilkaInfo));
		item.SetMesh(_race->GetMesh("Weapon\\drobilka.r3d"));
		item.SetTexture(_race->GetTexture("Weapon\\drobilka.jpg"));
		item.SetMapObj(_race->GetDB()->GetRecord(MapObjLib::ctWeapon, "drobilka"));
		item.SetMaxCharge(7);
		item.SetCntCharge(1);
		item.SetDamage(60.0f);
		item.SetCost(15500);
		item.SetChargeCost(7000);

		{
			game::Weapon::Desc desc;
			Weapon::ProjDesc projDesc;

			projDesc.type = Proj::ptDrobilka;
			projDesc.SetModel(_race->GetDB()->GetRecord(MapObjLib::ctEffects, "spark2"));
			projDesc.pos = D3DXVECTOR3(1.3f, 0.0f, 0.0f);
			projDesc.size = D3DXVECTOR3(2.1f, 0.5f, 0.5f);
			projDesc.modelSize = true;
			projDesc.maxDist = 3.0f;
			projDesc.mass = 100.0f;
			projDesc.damage = 60.0f/4;
			projDesc.minTimeLife = 4.0f;
			projDesc.angleSpeed = 4.0f * D3DX_PI;

			desc.projList.push_back(projDesc);
			desc.shotDelay = 4.0f;

			item.SetWpnDesc(desc);
		}

		SaveSlot(slot, "drobilka");
	}

	{
		Slot* slot = new Slot(0);

		WeaponItem& item = slot->CreateItem<WeaponItem>();
		item.SetName(_SC(scSphereGun));
		item.SetInfo(_SC(scSphereGunInfo));
		item.SetMesh(_race->GetMesh("Weapon\\sphereGun.r3d"));
		item.SetTexture(_race->GetTexture("Car\\podushka.tga"));
		item.SetMapObj(_race->GetDB()->GetRecord(MapObjLib::ctWeapon, "sphereGun"));
		item.SetMaxCharge(7);
		item.SetCntCharge(1);
		item.SetDamage(15.0f);
		item.SetCost(0);
		item.SetChargeCost(11700);

		{
			game::Weapon::Desc desc;
			Weapon::ProjDesc projDesc;

			projDesc.type = Proj::ptTorpeda;
			projDesc.SetModel(_race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\spherePulse"));
			projDesc.pos = D3DXVECTOR3(0.1f, 0.0f, 0.15f);
			projDesc.size = D3DXVECTOR3(0.5f, 0.5f, 0.5f);
			projDesc.modelSize = false;
			projDesc.speed = 30.0f;
			projDesc.speedRelativeMin = 20;
			projDesc.angleSpeed = D3DX_PI * 4.0f;
			projDesc.maxDist = 150.0f;
			projDesc.mass = 100.0f;
			projDesc.damage = 15.0f;

			desc.projList.push_back(projDesc);
			desc.shotDelay = 0.2f;

			item.SetWpnDesc(desc);
		}

		SaveSlot(slot, "sphereGun");
	}	
	{
		Slot* slot = new Slot(0);

		WeaponItem& item = slot->CreateItem<WeaponItem>();
		item.SetName(_SC(scTorpeda));
		item.SetInfo(_SC(scTorpedaInfo));
		item.SetMesh(_race->GetMesh("Weapon\\torpeda.r3d"));
		item.SetTexture(_race->GetTexture("Weapon\\torpeda.jpg"));
		item.SetMapObj(_race->GetDB()->GetRecord(MapObjLib::ctWeapon, "torpedaWeapon"));
		item.SetMaxCharge(2);
		item.SetCntCharge(1);
		item.SetDamage(40.0f);
		item.SetCost(45000);
		item.SetChargeCost(25000);

		{
			game::Weapon::Desc desc;
			Weapon::ProjDesc projDesc;

			projDesc.type = Proj::ptTorpeda;
			projDesc.SetModel(_race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\torpeda"));
			projDesc.pos = D3DXVECTOR3(0, 0.0f, 0.2f);
			projDesc.size = NullVector;
			projDesc.modelSize = true;
			projDesc.speed = 30.0f;
			projDesc.speedRelativeMin = 20;
			projDesc.angleSpeed = D3DX_PI;
			projDesc.maxDist = 300.0f;
			projDesc.mass = 300.0f;
			projDesc.damage = 40.0f;

			desc.projList.push_back(projDesc);
			desc.shotDelay = 0.2f;

			item.SetWpnDesc(desc);
		}

		SaveSlot(slot, "torpedaWeapon");
	}	

	{
		Slot* slot = new Slot(0);

		WeaponItem& item = slot->CreateItem<WeaponItem>();
		item.SetName(_SC(scMortira));
		item.SetInfo(_SC(scMortiraInfo));
		item.SetMesh(_race->GetMesh("Weapon\\mortira.r3d"));
		item.SetTexture(_race->GetTexture("Weapon\\mortira.tga"));
		item.SetMapObj(_race->GetDB()->GetRecord(MapObjLib::ctWeapon, "mortira"));
		item.SetMaxCharge(3);
		item.SetCntCharge(1);
		item.SetDamage(23.0f);
		item.SetCost(32000);
		item.SetChargeCost(11000);

		{
			game::Weapon::Desc desc;
			Weapon::ProjDesc projDesc;

			projDesc.type = Proj::ptMortira;
			projDesc.SetModel(_race->GetDB()->GetRecord(MapObjLib::ctEffects, "mortiraBall"));
			projDesc.pos = D3DXVECTOR3(0, 0.0f, 0.1f);
			projDesc.size = D3DXVECTOR3(0.5f, 0.5f, 1.0f);
			projDesc.modelSize = true;
			projDesc.speed = 40.0f;
			projDesc.speedRelative = true;
			projDesc.maxDist = 500.0f;
			projDesc.mass = 100.0f;
			projDesc.damage = 8.0f;

			desc.projList.push_back(projDesc);
			desc.shotDelay = 0.2f;

			item.SetWpnDesc(desc);
		}

		SaveSlot(slot, "mortira");
	}

	{
		Slot* slot = new Slot(0);

		WeaponItem& item = slot->CreateItem<WeaponItem>();
		item.SetName(_SC(scAsyncFrost));
		item.SetInfo(_SC(scAsyncFrostInfo));
		item.SetMesh(_race->GetMesh("Weapon\\asyncFrost.r3d"));
		item.SetTexture(_race->GetTexture("Weapon\\asyncFrost.jpg"));
		item.SetMapObj(_race->GetDB()->GetRecord(MapObjLib::ctWeapon, "asyncFrost"));
		item.SetMaxCharge(7);
		item.SetCntCharge(1);
		item.SetDamage(40.0f);
		item.SetCost(0);
		item.SetChargeCost(25000);

		{
			game::Weapon::Desc desc;
			Weapon::ProjDesc projDesc;

			projDesc.type = Proj::ptFrostRay;
			projDesc.SetLibMat(_race->GetLibMat("Effect\\frostRay"));
			projDesc.SetModel(_race->GetDB()->GetRecord(MapObjLib::ctEffects, "frostHit"));
			projDesc.SetModel2(_race->GetDB()->GetRecord(MapObjLib::ctEffects, "frost"));
			projDesc.pos = D3DXVECTOR3(0.7f, 0.0f, 0.3f);
			D3DXQuaternionRotationAxis(&projDesc.rot, &YVector, D3DX_PI/24.0f);
			projDesc.size = D3DXVECTOR3(0.5f, 0.5f, 0.0f);
			projDesc.maxDist = 100.0f;
			projDesc.damage = 19.0f/0.5f;
			projDesc.minTimeLife = 1.0f;

			desc.projList.push_back(projDesc);
			desc.shotDelay = 1.0f;
			item.SetWpnDesc(desc);
		}

		SaveSlot(slot, "asyncFrost");
	}

	{
		Slot* slot = new Slot(0);

		WeaponItem& item = slot->CreateItem<WeaponItem>();
		item.SetName(_SC(scHyperBlaster));
		item.SetInfo(_SC(scHyperBlasterInfo));
		item.SetMesh(_race->GetMesh("Weapon\\hyperBlaster.r3d"));
		item.SetTexture(_race->GetTexture("Weapon\\hyperBlaster.tga"));
		item.SetMapObj(_race->GetDB()->GetRecord(MapObjLib::ctWeapon, "hyperBlaster"));
		item.SetMaxCharge(7);
		item.SetCntCharge(1);
		item.SetDamage(20.0f);
		item.SetCost(28000);
		item.SetChargeCost(12000);

		{
			game::Weapon::Desc desc;
			Weapon::ProjDesc projDesc;

			projDesc.type = Proj::ptRocket;
			projDesc.SetModel(_race->GetDB()->GetRecord(MapObjLib::ctEffects, "hyperBlaster"));
			projDesc.pos = D3DXVECTOR3(1.0f, 0.0f, 0.1f);
			projDesc.size = D3DXVECTOR3(3.5f, 0.75f, 1.0f);
			projDesc.modelSize = false;
			projDesc.speed = 40.0f;
			projDesc.speedRelative = true;
			projDesc.maxDist = 500.0f;
			projDesc.mass = 100.0f;
			projDesc.damage = 20.0f;

			desc.projList.push_back(projDesc);
			desc.shotDelay = 0.2f;

			item.SetWpnDesc(desc);
		}

		SaveSlot(slot, "hyperBlaster");
	}

	{
		Slot* slot = new Slot(0);

		MineItem& item = slot->CreateItem<MineItem>();	
		item.SetName(_SC(scMaslo));
		item.SetInfo(_SC(scMasloInfo));
		item.SetMesh(_race->GetMesh("Weapon\\maslo.r3d"));
		item.SetTexture(_race->GetTexture("Weapon\\maslo.jpg"));
		item.SetMapObj(_race->GetDB()->GetRecord(MapObjLib::ctWeapon, "masloWeapon"));
		item.SetMaxCharge(7);
		item.SetCntCharge(1);
		item.SetDamage(0.0f);
		item.SetCost(8000);		
		item.SetChargeCost(4000);

		{
			game::Weapon::Desc desc;
			Weapon::ProjDesc projDesc;

			projDesc.type = Proj::ptMaslo;
			projDesc.SetModel(_race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\maslo"));
			projDesc.pos = D3DXVECTOR3(0.0f, 0.0f, 0.2f);
			projDesc.modelSize = false;
			projDesc.size = D3DXVECTOR3(1.5f, 1.5f, 1.7f);
			projDesc.damage = 1.5f;

			desc.projList.push_back(projDesc);
			desc.shotDelay = 0.2f;

			item.SetWpnDesc(desc);
		}

		SaveSlot(slot, "maslo");
	}

	{
		Slot* slot = new Slot(0);

		MineItem& item = slot->CreateItem<MineItem>();
		item.SetName(_SC(scMineSpike));
		item.SetInfo(_SC(scMineSpikeInfo));
		item.SetMesh(_race->GetMesh("Weapon\\mine1.r3d"));
		item.SetTexture(_race->GetTexture("Weapon\\mine1.jpg"));
		item.SetMapObj(_race->GetDB()->GetRecord(MapObjLib::ctWeapon, "mine1Weapon"));
		item.SetMaxCharge(7);
		item.SetCntCharge(1);
		item.SetDamage(11.0f);
		item.SetCost(10200);
		item.SetChargeCost(10000);

		{
			game::Weapon::Desc desc;
			Weapon::ProjDesc projDesc;

			projDesc.type = Proj::ptMine;
			projDesc.SetModel(_race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\mine1"));
			projDesc.pos = D3DXVECTOR3(0.0f, 0.0f, 0.2f);
			projDesc.size = D3DXVECTOR3(0.0f, 0.0f, 1.7f);
			projDesc.modelSize = true;
			projDesc.damage = 11.0f;
			projDesc.maxDist = 0.0f;
			projDesc.speed = 3000.0f;

			desc.projList.push_back(projDesc);
			desc.shotDelay = 0.2f;

			item.SetWpnDesc(desc);
		}

		SaveSlot(slot, "mine1");		
	}

	{
		Slot* slot = new Slot(0);

		MineItem& item = slot->CreateItem<MineItem>();
		item.SetName(_SC(scMineRip));
		item.SetInfo(_SC(scMineRipInfo));
		item.SetMesh(_race->GetMesh("Weapon\\mine2.r3d"));
		item.SetTexture(_race->GetTexture("Weapon\\mine2.jpg"));
		item.SetMapObj(_race->GetDB()->GetRecord(MapObjLib::ctWeapon, "mine2Weapon"));
		item.SetMaxCharge(7);
		item.SetCntCharge(1);
		item.SetDamage(20.0f);
		item.SetCost(15000);
		item.SetChargeCost(13000);

		{
			game::Weapon::Desc desc;
			Weapon::ProjDesc projDesc;

			projDesc.type = Proj::ptMineRip;
			projDesc.SetModel(_race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\mine2"));
			projDesc.SetModel2(_race->GetDB()->GetRecord(MapObjLib::ctBonus, "mineRipKern"));
			projDesc.SetModel3(_race->GetDB()->GetRecord(MapObjLib::ctBonus, "mineRipPiece"));
			projDesc.pos = D3DXVECTOR3(0.0f, 0.0f, 0.2f);
			projDesc.size = D3DXVECTOR3(0.0f, 0.0f, 1.7f);
			projDesc.modelSize = true;
			projDesc.damage = 20.0f;
			projDesc.maxDist = 0.0f;
			projDesc.speed = 3000.0f;
			projDesc.angleSpeed = 2.0f;

			desc.projList.push_back(projDesc);
			desc.shotDelay = 0.2f;

			item.SetWpnDesc(desc);
		}

		SaveSlot(slot, "mine2");
	}

	{
		Slot* slot = new Slot(0);

		MineItem& item = slot->CreateItem<MineItem>();
		item.SetName(_SC(scMinePropton));
		item.SetInfo(_SC(scMineProptonInfo));
		item.SetMesh(_race->GetMesh("Weapon\\mine3.r3d"));
		item.SetTexture(_race->GetTexture("Weapon\\mine3.jpg"));
		item.SetMapObj(_race->GetDB()->GetRecord(MapObjLib::ctWeapon, "mine3Weapon"));
		item.SetMaxCharge(7);
		item.SetCntCharge(1);
		item.SetDamage(25.0f);
		item.SetCost(16000);
		item.SetChargeCost(13500);

		{
			game::Weapon::Desc desc;
			Weapon::ProjDesc projDesc;

			projDesc.type = Proj::ptMine;
			projDesc.SetModel(_race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\mine3"));
			projDesc.pos = D3DXVECTOR3(0.0f, 0.0f, 0.2f);
			projDesc.size = D3DXVECTOR3(5.0f, 5.0f, 1.7f);
			projDesc.modelSize = false;
			projDesc.damage = 25.0f;
			projDesc.maxDist = 0.0f;
			projDesc.speed = 3000.0f;
			//projDesc.angleSpeed = 1.0f;

			desc.projList.push_back(projDesc);
			desc.shotDelay = 0.2f;

			item.SetWpnDesc(desc);
		}

		SaveSlot(slot, "mine3");
	}

	{
		Slot* slot = new Slot(0);

		HyperItem& item = slot->CreateItem<HyperItem>();	
		item.SetName(_SC(scSpring));
		item.SetInfo(_SC(scSpringInfo));
		item.SetMesh(_race->GetMesh("Weapon\\spring.r3d"));
		item.SetTexture(_race->GetTexture("Weapon\\spring.jpg"));
		item.SetMapObj(_race->GetDB()->GetRecord(MapObjLib::ctWeapon, "spring"));
		item.SetMaxCharge(7);
		item.SetCntCharge(1);
		item.SetDamage(0.0f);
		item.SetCost(6000);		
		item.SetChargeCost(2000);

		{
			game::Weapon::Desc desc;
			Weapon::ProjDesc projDesc;

			projDesc.type = Proj::ptSpring;
			projDesc.SetModel(0);
			projDesc.speed = 14.0f;
			projDesc.mass = 100.0f;
			projDesc.minTimeLife = 0.5f;

			desc.projList.push_back(projDesc);
			desc.shotDelay = 0.2f;

			item.SetWpnDesc(desc);
		}

		SaveSlot(slot, "spring");
	}

	{
		Slot* slot = new Slot(0);

		HyperItem& item = slot->CreateItem<HyperItem>();
		item.SetName(_SC(scHyper));
		item.SetInfo(_SC(scHyperInfo));
		item.SetMesh(_race->GetMesh("Weapon\\hyperdrive.r3d"));
		item.SetTexture(_race->GetTexture("Weapon\\hyperdrive.jpg"));
		item.SetMapObj(_race->GetDB()->GetRecord(MapObjLib::ctWeapon, "hyperdrive"));
		item.SetMaxCharge(7);
		item.SetCntCharge(1);
		item.SetCost(18000);
		item.SetChargeCost(11600);

		{
			game::Weapon::Desc desc;
			Weapon::ProjDesc projDesc;

			projDesc.type = Proj::ptHyper;
			projDesc.SetModel(_race->GetDB()->GetRecord(MapObjLib::ctEffects, "accelEff"));
			projDesc.pos = D3DXVECTOR3(0.4f, 0.0f, 0.0f);
			projDesc.size = NullVector;
			projDesc.modelSize = true;
			projDesc.speed = 10.0f;
			projDesc.minTimeLife = 2.0f;

			desc.projList.push_back(projDesc);
			desc.shotDelay = 1.5f;	

			item.SetWpnDesc(desc);
		}

		SaveSlot(slot, "hyperdrive");		
	}
}

void Workshop::LoadLib()
{
	lsl::SafeDelete(_lib);

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

		LoadWheels();
		LoadTruba();
		LoadEngines();
		LoadArmors();
		LoadWeapons();

		SerialFileXML xml;
		xml.SaveNodeToFile(*_rootNode, "workshop.xml");
	}

	DeleteSlots();
	for (RecordLib::RecordList::const_iterator iter = _lib->GetRecordList().begin(); iter != _lib->GetRecordList().end(); ++iter)
		AddSlot(*iter);	
}

const RecordLib& Workshop::GetLib()
{
	return *_lib;
}

Record& Workshop::GetRecord(const std::string& name)
{
	Record* record = _lib->FindRecord(name);
	if (record == 0)
		throw lsl::Error("Slot record " + name + " does not exist");

	return *record;
}

Slot* Workshop::FindSlot(Record* record)
{
	for (Slots::iterator iter = _slots.begin(); iter != _slots.end(); ++iter)
		if ((*iter)->GetRecord() == record)
			return *iter;

	return 0;
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
	for (Items::iterator iter = _items.begin(); iter != _items.end(); ++iter)
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
	return NULL;
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

	return _race->GetSellCost(slot->GetItem().GetCost() + (wpn && chargeCount > 1 ? (chargeCount - 1) * wpn->GetChargeCost() : 0), sellDiscount);
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

	return NULL;
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




Garage::Garage(Race* race, const std::string& name): _race(race), _upgradeMaxLevel(0), _weaponMaxLevel(0)
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
	for (PlaceItems::const_iterator iter = items.begin(); iter != items.end(); ++iter)
		if (iter->slot->GetRecord() == slot)
			return &(*iter);

	return 0;
}

const Garage::PlaceItem* Garage::PlaceSlot::FindItem(Slot* slot) const
{
	for (PlaceItems::const_iterator iter = items.begin(); iter != items.end(); ++iter)
		if (iter->slot == slot)
			return &(*iter);

	return 0;
}

Garage::Car::Car(): _record(0), _wheel(0), _cost(0), _name(scNull), _desc(scNull), _initialUpgradeSet(0)
{	
}

Garage::Car::~Car()
{
	ClearSlots();
	ClearBodies();	

	SetWheel(0);
	SetRecord(0);
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
		SetSlot(Player::SlotType(i), ref->_slot[i]);
}

void Garage::Car::ClearSlots()
{
	for (int i = 0; i < Player::cSlotTypeEnd; ++i)
	{
		SetSlot(Player::SlotType(i), PlaceSlot());		
	}
}

void Garage::Car::SaveTo(lsl::SWriter* writer, Garage* owner)
{
	MapObjLib::SaveRecordRef(writer, "record", _record);

	lsl::SWriter* bodyMeshes = writer->NewDummyNode("bodyMeshes");
	int i = 0;
	for (BodyMeshes::iterator iter = _bodyMeshes.begin(); iter != _bodyMeshes.end(); ++iter, ++i)
	{
		std::stringstream sstream;
		sstream << "bodyMesh" << i;
		lsl::SWriter* child = bodyMeshes->NewDummyNode(sstream.str().c_str());		

		child->WriteValue("meshId", iter->meshId);
		child->WriteValue("decal", iter->decal);

		child->WriteRef("mesh", iter->mesh);		
		child->WriteRef("texture", iter->texture);		
	}	

	writer->WriteValue("cost", _cost);

	lsl::SWriteValue(writer, "name", _name);
	lsl::SWriteValue(writer, "desc", _desc);

	writer->WriteValue("initialUpgradeSet", _initialUpgradeSet);

	writer->WriteRef("wheel", _wheel);
	writer->WriteValue("wheels", _wheels);

	for (int i = 0; i < Player::cSlotTypeEnd; ++i)
	{
		const PlaceSlot& slot = _slot[i];
		lsl::SWriter* child = writer->NewDummyNode(Player::cSlotTypeStr[i].c_str());

		child->WriteValue("active", slot.active);
		child->WriteValue("show", slot.show);
		child->WriteValue("lock", slot.lock);
		lsl::SWriteValue(child, "pos", slot.pos);
		RecordLib::SaveRecordRef(child, "defItem", slot.defItem ? slot.defItem->GetRecord() : NULL);		

		lsl::SWriter* items = child->NewDummyNode("items");
		int j = 0;
		for (PlaceItems::const_iterator iter = slot.items.begin(); iter != slot.items.end(); ++iter, ++j)
		{
			const PlaceItem& item = *iter;

			std::stringstream sstream;
			sstream << "item" << j;
			lsl::SWriter* child = items->NewDummyNode(sstream.str().c_str());

			RecordLib::SaveRecordRef(child, "record", item.slot ? item.slot->GetRecord() : NULL);
			lsl::SWriteValue(child, "rot", item.rot);
			lsl::SWriteValue(child, "offset", item.offset);
		}
	}

	lsl::SWriter* nightLights = writer->NewDummyNode("nightLights");
	for (unsigned i = 0; i < _nightLights.size(); ++i)
	{
		std::stringstream sstream;
		sstream << "item" << i;
		lsl::SWriter* child = nightLights->NewDummyNode(sstream.str().c_str());		

		child->WriteValue("head", _nightLights[i].head);
		lsl::SWriteValue(child, "pos", _nightLights[i].pos);
		lsl::SWriteValue(child, "size", _nightLights[i].size);
	}	
}

void Garage::Car::LoadFrom(lsl::SReader* reader, Garage* owner)
{
	ClearBodies();

	SetRecord(MapObjLib::LoadRecordRef(reader, "record"));

	lsl::SReader* bodyMeshes = reader->ReadValue("bodyMeshes");
	if (bodyMeshes)
	{
		lsl::SReader* child = bodyMeshes->FirstChildValue();
		while (child)
		{	
			BodyMesh bodyMesh;

			child->ReadValue("meshId", bodyMesh.meshId);
			child->ReadValue("decal", bodyMesh.decal);

			lsl::Serializable::FixUpName fixUpName;
			if (child->ReadRef("mesh", true, NULL, &fixUpName))
				bodyMesh.mesh = fixUpName.GetCollItem<graph::IndexedVBMesh*>();

			if (child->ReadRef("texture", true, NULL, &fixUpName))
				bodyMesh.texture = fixUpName.GetCollItem<graph::Tex2DResource*>();
			
			AddBody(bodyMesh);

			child = child->NextValue();
		}
	}

	reader->ReadValue("cost", _cost);

	lsl::SReadValue(reader, "name", _name);
	lsl::SReadValue(reader, "desc", _desc);

	reader->ReadValue("initialUpgradeSet", _initialUpgradeSet);

	lsl::Serializable::FixUpName fixUpName;
	if (reader->ReadRef("wheel", true, NULL, &fixUpName))
		SetWheel(fixUpName.GetCollItem<graph::IndexedVBMesh*>());

	reader->ReadValue("wheels", _wheels);

	for (int i = 0; i < Player::cSlotTypeEnd; ++i)
	{
		PlaceSlot slot;

		lsl::SReader* child = reader->ReadValue(Player::cSlotTypeStr[i].c_str());

		child->ReadValue("active", slot.active);
		child->ReadValue("show", slot.show);
		child->ReadValue("lock", slot.lock);
		lsl::SReadValue(child, "pos", slot.pos);

		Record* record = RecordLib::LoadRecordRef(child, "defItem");
		slot.defItem = record ? owner->_race->GetWorkshop().FindSlot(record) : NULL;

		lsl::SReader* items = child->ReadValue("items");
		if (items)
		{
			lsl::SReader* child = items->FirstChildValue();
			while (child)
			{	
				PlaceItem item;

				record = RecordLib::LoadRecordRef(child, "record");
				item.slot = record ? owner->_race->GetWorkshop().FindSlot(record) : NULL;
				lsl::SReadValue(child, "rot", item.rot);
				lsl::SReadValue(child, "offset", item.offset);
				slot.items.push_back(item);

				child = child->NextValue();
			}
		}

		SetSlot((Player::SlotType)i, slot);
	}

	_nightLights.clear();
	lsl::SReader* nightLights = reader->ReadValue("nightLights");
	if (nightLights)
	{
		lsl::SReader* child = nightLights->FirstChildValue();
		while (child)
		{	
			NightLight nightLight;

			child->ReadValue("head", nightLight.head);
			lsl::SReadValue(child, "pos", nightLight.pos);
			lsl::SReadValue(child, "size", nightLight.size);

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
	if (Object::ReplaceRef(_wheel, value))
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
	lsl::SafeRelease(_slot[type].defItem);
	for (PlaceItems::iterator iter = _slot[type].items.begin(); iter != _slot[type].items.end(); ++iter)
		iter->slot->Release();

	_slot[type] = value;

	for (PlaceItems::iterator iter = _slot[type].items.begin(); iter != _slot[type].items.end(); ++iter)
	{
		LSL_ASSERT(iter->slot);

		iter->slot->AddRef();
	}
	if (_slot[type].defItem)
		_slot[type].defItem->AddRef();
}

void Garage::FillStandartSlots(Car* car, const std::string& wheel, bool truba, const D3DXVECTOR3* hyperDrive, bool spring, bool maslo, bool mineSpike, bool mineRip, bool mineProton, bool lockedMax)
{
	for (int i = 0; i < Player::cSlotTypeEnd; ++i)
	{
		PlaceSlot place;
				
		switch (i)
		{
		case Player::stWheel:
			if (!wheel.empty())
			{
				place.active = true;
				place.show = false;

				if (!lockedMax)
				{
					place.items.push_back(&GetShop().GetSlot(wheel + "1"));
					place.items.push_back(&GetShop().GetSlot(wheel + "2"));
				}
				place.items.push_back(&GetShop().GetSlot(wheel + "3"));				
				place.defItem = place.items.front().slot;
				place.lock = true;
			}
			break;
			
		case Player::stTruba:
			if (truba)
			{
				place.active = true;
				place.show = false;

				if (!lockedMax)
				{
					place.items.push_back(&GetShop().GetSlot("truba1"));
					place.items.push_back(&GetShop().GetSlot("truba2"));
				}
				place.items.push_back(&GetShop().GetSlot("truba3"));
				place.defItem = place.items.front().slot;
				place.lock = true;
			}
			break;
			
		case Player::stArmor:
			place.active = true;
			place.show = false;

			if (!lockedMax)
			{			
				place.items.push_back(&GetShop().GetSlot("armor1"));
				place.items.push_back(&GetShop().GetSlot("armor2"));
			}
			place.items.push_back(&GetShop().GetSlot("armor3"));
			place.defItem = place.items.front().slot;
			place.lock = true;
			break;
			
		case Player::stMotor:
			place.active = true;
			place.show = false;

			if (!lockedMax)
			{
				place.items.push_back(&GetShop().GetSlot("engine1"));
				place.items.push_back(&GetShop().GetSlot("engine2"));
			}
			place.items.push_back(&GetShop().GetSlot("engine3"));
			place.defItem = place.items.front().slot;
			place.lock = true;
			break;
			
		case Player::stHyper: 
		{
			//коллизинируют по свойству place.show, в этом суча необходимо его сделать частью PlaceItem
			LSL_ASSERT(!spring || !hyperDrive);

			place.active = true;
			place.pos = NullVector;

			if (spring)
			{
				place.show = false;
				place.items.push_back(&GetShop().GetSlot("spring"));				
			}
			if (hyperDrive)
			{
				place.show = true;
				D3DXQUATERNION rot;
				D3DXQuaternionRotationAxis(&rot, &ZVector, D3DX_PI);
				place.items.push_back(PlaceItem(&GetShop().GetSlot("hyperdrive"), rot, *hyperDrive));
			}				
			if (!place.items.empty())
				place.defItem = place.items.back().slot;

			place.lock = true;
			break;
		}
			
		case Player::stMine:
			place.active = true;
			place.show = false;			

			if (maslo)
			{
				place.items.push_back(&GetShop().GetSlot("maslo"));
				place.pos = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
			}
			if (mineSpike)
			{
				place.items.push_back(&GetShop().GetSlot("mine1"));
				place.pos = D3DXVECTOR3(-1.0f, 0.0f, 0.0f);
			}
			if (mineRip)
			{
				place.items.push_back(&GetShop().GetSlot("mine2"));
				place.pos = D3DXVECTOR3(-1.0f, 0.0f, 0.0f);
			}
			if (mineProton)
			{
				place.items.push_back(&GetShop().GetSlot("mine3"));
				place.pos = D3DXVECTOR3(-1.0f, 0.0f, 0.0f);
			}
			if (!place.items.empty())
				place.defItem = place.items.back().slot;
			place.lock = true;
			break;		
		}

		car->SetSlot(Player::SlotType(i), place);
	}
}

Garage::Car* Garage::AddCar()
{
	Car* car = new Car();
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
	for (Cars::iterator iter = _cars.begin(); iter != _cars.end(); ++iter)
		delete *iter;

	_cars.clear();
}

void Garage::LoadCars()
{
	//marauder
	{
		Car* car = AddCar();
		car->SetName(_SC(scMarauder));
		car->SetDesc(_SC(scMarauderInfo));
		car->SetRecord(_race->GetDB()->GetRecord(MapObjLib::ctCar, "marauder"));
		car->AddBody(_race->GetMesh("Car\\marauder.r3d"), _race->GetTexture("Car\\marauder.tga"), true);
		car->SetWheel(_race->GetMesh("Car\\marauderWheel.r3d"));
		car->SetWheels("Data\\Car\\marauderWheel.txt");
		car->SetCost(18000);

		FillStandartSlots(car, "wheel", true, 0, true, true, false, false, false, false);

		{
			PlaceSlot place;
			place.active = true;
			place.show = true;
			place.pos = D3DXVECTOR3(-0.65f, 0.0f, 0.7f);

			place.items.push_back(&GetShop().GetSlot("bulletGun"));
			place.defItem = place.items.front().slot;
			place.lock = true;

			car->SetSlot(Player::stWeapon1, place);
		}		
	}

	//dirtdevil
	{
		Car* car = AddCar();
		car->SetName(_SC(scDirtdevil));
		car->SetDesc(_SC(scDirtdevilInfo));
		car->SetRecord(_race->GetDB()->GetRecord(MapObjLib::ctCar, "dirtdevil"));
		car->AddBody(_race->GetMesh("Car\\dirtdevil.r3d"), _race->GetTexture("Car\\dirtdevil.tga"), true);
		car->SetWheel(_race->GetMesh("Car\\dirtdevilWheel.r3d"));
		car->SetWheels("Data\\Car\\dirtdevilWheel.txt");
		car->SetCost(20000);

		FillStandartSlots(car, "wheel", true, 0, true, true, false, false, false, false);

		{
			PlaceSlot place;
			place.active = true;
			place.show = true;
			place.pos = D3DXVECTOR3(1.6f, 0.0f, 0.0f);

			place.items.push_back(&GetShop().GetSlot("rifleWeapon"));
			place.defItem = place.items.front().slot;
			place.lock = true;

			car->SetSlot(Player::stWeapon1, place);
		}
		{
			PlaceSlot place;
			place.active = true;
			place.show = true;
			place.pos = D3DXVECTOR3(-0.38f, 0.0f, 1.1f);

			place.items.push_back(&GetShop().GetSlot("rocketLauncher"));

			place.items.push_back(&GetShop().GetSlot("pulsator"));

			car->SetSlot(Player::stWeapon2, place);
		}
	}

	//manticora
	{
		Car* car = AddCar();
		car->SetName(_SC(scManticora));
		car->SetDesc(_SC(scManticoraInfo));
		car->SetRecord(_race->GetDB()->GetRecord(MapObjLib::ctCar, "manticora"));
		car->AddBody(_race->GetMesh("Car\\manticora.r3d"), _race->GetTexture("Car\\manticora.tga"), true);
		car->SetWheel(_race->GetMesh("Car\\manticoraWheel.r3d"));
		car->SetWheels("Data\\Car\\manticoraWheel.txt");
		car->SetCost(90000);

		FillStandartSlots(car, "wheel", true, &D3DXVECTOR3(-1.5f, 0.0f, 0.35f), false, false, true, false, false, false);

		{
			PlaceSlot place;
			place.active = true;
			place.show = true;
			place.pos = D3DXVECTOR3(1.7f, 0.0f, 0.08f);

			place.items.push_back(&GetShop().GetSlot("blasterGun"));	
			place.defItem = place.items.front().slot;
			place.lock = true;

			car->SetSlot(Player::stWeapon1, place);
		}
		{
			PlaceSlot place;
			place.active = true;
			place.show = true;
			place.pos = D3DXVECTOR3(0.0f, 1.0f, 0.3f);
			D3DXQUATERNION rot;
			D3DXVECTOR3 offset;
			
			D3DXQuaternionRotationAxis(&rot, &XVector, -D3DX_PI/3.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("rocketLauncher"), rot));

			D3DXQuaternionRotationAxis(&rot, &XVector, -D3DX_PI/3.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("pulsator"), rot));

			D3DXQuaternionRotationAxis(&rot, &XVector, -D3DX_PI);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("fireGun"), rot));

			offset = D3DXVECTOR3(1.25f, -0.3f, 0.05f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("drobilka"), NullQuaternion, offset));

			car->SetSlot(Player::stWeapon2, place);
		}
		{
			PlaceSlot place;
			place.active = true;
			place.show = true;
			place.pos = D3DXVECTOR3(0.0f, -1.0f, 0.3f);
			D3DXQUATERNION rot;
			D3DXVECTOR3 offset;
			
			D3DXQuaternionRotationAxis(&rot, &XVector, D3DX_PI/3.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("rocketLauncher"), rot));
			
			D3DXQuaternionRotationAxis(&rot, &XVector, D3DX_PI/3.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("pulsator"), rot));

			place.items.push_back(&GetShop().GetSlot("fireGun"));

			offset = D3DXVECTOR3(1.25f, 0.3f, 0.05f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("drobilka"), NullQuaternion, offset));

			car->SetSlot(Player::stWeapon3, place);
		}

		//boss
		Car* boss = AddCar();
		boss->Assign(car);
		boss->SetRecord(_race->GetDB()->GetRecord(MapObjLib::ctCar, "manticoraBoss"));
		boss->ClearBodies();
		boss->AddBody(_race->GetMesh("Car\\manticoraBoss.r3d"), _race->GetTexture("Car\\manticoraBoss.tga"), true);
	}

	//guseniza
	{
		Car* car = AddCar();
		car->SetName(_SC(scGuseniza));
		car->SetDesc(_SC(scGusenizaInfo));
		car->SetRecord(_race->GetDB()->GetRecord(MapObjLib::ctCar, "guseniza"));
		car->AddBody(_race->GetMesh("Car\\guseniza.r3d"), _race->GetTexture("Car\\guseniza.tga"), true, 0);
		car->AddBody(_race->GetMesh("Car\\guseniza.r3d"), _race->GetTexture("Car\\gusenizaChain.tga"), false, 1);
		car->SetWheels("Data\\Car\\gusenizaWheel.txt");
		car->SetCost(140000);

		FillStandartSlots(car, "gusWheel", true, &D3DXVECTOR3(-1.5f, 0.0f, 0.25f), false, false, false, true, false, false);

		{
			PlaceSlot place;
			place.active = true;
			place.show = true;
			place.pos = D3DXVECTOR3(-0.27f, 0.0f, 0.36f);

			place.items.push_back(&GetShop().GetSlot("tankLaser"));
			
			place.defItem = place.items.front().slot;
			place.lock = true;

			car->SetSlot(Player::stWeapon1, place);
		}

		{
			PlaceSlot place;
			place.active = true;
			place.show = true;
			place.pos = D3DXVECTOR3(0.0f, 1.4f, 0.0f);
			D3DXQUATERNION rot;
			D3DXVECTOR3 offset;

			D3DXQuaternionRotationAxis(&rot, &XVector, -D3DX_PI/2.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("rocketLauncher"), rot));

			D3DXQuaternionRotationAxis(&rot, &XVector, -D3DX_PI/2.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("pulsator"), rot));

			D3DXQuaternionRotationAxis(&rot, &XVector, -D3DX_PI);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("fireGun"), rot));

			offset = D3DXVECTOR3(0.85f, -0.8f, 0.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("drobilka"), NullQuaternion, offset));

			place.items.push_back(&GetShop().GetSlot("torpedaWeapon"));

			D3DXQuaternionRotationAxis(&rot, &XVector, -D3DX_PI/2.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("turel"), rot));

			D3DXQuaternionRotationAxis(&rot, &XVector, -D3DX_PI/2.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("phaseImpulse"), rot));

			D3DXQuaternionRotationAxis(&rot, &YVector, -D3DX_PI/24.0f);
			offset = D3DXVECTOR3(0.85f, -0.6f, 0.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("mortira"), rot, offset));

			D3DXQuaternionRotationAxis(&rot, &XVector, -D3DX_PI/2.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("hyperBlaster"), rot));

			car->SetSlot(Player::stWeapon2, place);
		}
		{
			PlaceSlot place;
			place.active = true;
			place.show = true;
			place.pos = D3DXVECTOR3(0.0f, -1.4f, 0.0f);
			D3DXQUATERNION rot;
			D3DXVECTOR3 offset;

			D3DXQuaternionRotationAxis(&rot, &XVector, D3DX_PI/2.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("rocketLauncher"), rot));

			D3DXQuaternionRotationAxis(&rot, &XVector, D3DX_PI/2.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("pulsator"), rot));

			place.items.push_back(&GetShop().GetSlot("fireGun"));

			offset = D3DXVECTOR3(0.85f, 0.8f, 0.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("drobilka"), NullQuaternion, offset));

			place.items.push_back(&GetShop().GetSlot("torpedaWeapon"));

			D3DXQuaternionRotationAxis(&rot, &XVector, D3DX_PI/2.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("turel"), rot));

			D3DXQuaternionRotationAxis(&rot, &XVector, D3DX_PI/2.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("phaseImpulse"), rot));

			D3DXQuaternionRotationAxis(&rot, &YVector, -D3DX_PI/24.0f);
			offset = D3DXVECTOR3(0.85f, 0.6f, 0.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("mortira"), rot, offset));

			D3DXQuaternionRotationAxis(&rot, &XVector, D3DX_PI/2.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("hyperBlaster"), rot));

			car->SetSlot(Player::stWeapon3, place);
		}

		//boss
		Car* boss = AddCar();
		boss->Assign(car);
		boss->SetRecord(_race->GetDB()->GetRecord(MapObjLib::ctCar, "gusenizaBoss"));
		boss->ClearBodies();
		boss->AddBody(_race->GetMesh("Car\\gusenizaBoss.r3d"), _race->GetTexture("Car\\gusenizaBoss.tga"), true, 0);
		boss->AddBody(_race->GetMesh("Car\\gusenizaBoss.r3d"), _race->GetTexture("Car\\gusenizaChain.tga"), false, 1);
	}

	//podushka
	{
		Car* car = AddCar();
		car->SetName(_SC(scPodushka));
		car->SetDesc(_SC(scPodushkaInfo));
		car->SetRecord(_race->GetDB()->GetRecord(MapObjLib::ctCar, "podushka"));
		car->AddBody(_race->GetMesh("Car\\podushka.r3d"), _race->GetTexture("Car\\podushka.tga"), true);
		car->SetWheels("Data\\Car\\podushkaWheel.txt");
		car->SetCost(165000);

		FillStandartSlots(car, "", true, &D3DXVECTOR3(-1.5f, 0.0f, 0.35f), false, false, false, false, true, false);

		{
			PlaceSlot place;
			place.active = true;
			place.show = true;
			place.pos = D3DXVECTOR3(-0.27f, 0.0f, 0.65f);

			place.items.push_back(&GetShop().GetSlot("sphereGun"));
			place.defItem = place.items.front().slot;
			place.lock = true;

			car->SetSlot(Player::stWeapon1, place);
		}
		{
			PlaceSlot place;
			place.active = true;
			place.show = true;
			place.pos = D3DXVECTOR3(0.2f, 0.6f, 0.25f);
			D3DXQUATERNION rot;
			D3DXVECTOR3 offset;

			D3DXQuaternionRotationAxis(&rot, &XVector, -D3DX_PI/3.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("rocketLauncher"), rot));

			D3DXQuaternionRotationAxis(&rot, &XVector, -D3DX_PI/3.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("pulsator"), rot));

			D3DXQuaternionRotationAxis(&rot, &XVector, -D3DX_PI);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("fireGun"), rot));

			//offset = D3DXVECTOR3(1.2f, -0.05f, -0.1f);
			//place.items.push_back(PlaceItem(&GetShop().GetSlot("drobilka"), NullQuaternion, offset));

			place.items.push_back(&GetShop().GetSlot("torpedaWeapon"));

			D3DXQuaternionRotationAxis(&rot, &XVector, -D3DX_PI/3.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("turel"), rot));

			D3DXQuaternionRotationAxis(&rot, &XVector, -D3DX_PI/3.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("phaseImpulse"), rot));

			D3DXQuaternionRotationAxis(&rot, &YVector, -D3DX_PI/24.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("mortira"), rot));

			D3DXQuaternionRotationAxis(&rot, &XVector, -D3DX_PI/3.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("hyperBlaster"), rot));

			car->SetSlot(Player::stWeapon2, place);
		}
		{
			PlaceSlot place;
			place.active = true;
			place.show = true;
			place.pos = D3DXVECTOR3(0.2f, -0.6f, 0.25f);
			D3DXQUATERNION rot;
			D3DXVECTOR3 offset;

			D3DXQuaternionRotationAxis(&rot, &XVector, D3DX_PI/3.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("rocketLauncher"), rot));

			D3DXQuaternionRotationAxis(&rot, &XVector, D3DX_PI/3.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("pulsator"), rot));

			place.items.push_back(&GetShop().GetSlot("fireGun"));

			//offset = D3DXVECTOR3(1.2f, 0.05f, -0.1f);
			//place.items.push_back(PlaceItem(&GetShop().GetSlot("drobilka"), NullQuaternion, offset));

			place.items.push_back(&GetShop().GetSlot("torpedaWeapon"));

			D3DXQuaternionRotationAxis(&rot, &XVector, D3DX_PI/3.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("turel"), rot));

			D3DXQuaternionRotationAxis(&rot, &XVector, D3DX_PI/3.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("phaseImpulse"), rot));

			D3DXQuaternionRotationAxis(&rot, &YVector, -D3DX_PI/24.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("mortira"), rot));

			D3DXQuaternionRotationAxis(&rot, &XVector, D3DX_PI/3.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("hyperBlaster"), rot));

			car->SetSlot(Player::stWeapon3, place);
		}
	}

	//monstertruck
	{
		Car* car = AddCar();
		car->SetName(_SC(scMonsterTruck));
		car->SetDesc(_SC(scMonsterTruckInfo));
		car->SetRecord(_race->GetDB()->GetRecord(MapObjLib::ctCar, "monstertruck"));
		car->AddBody(_race->GetMesh("Car\\monstertruck.r3d"), _race->GetTexture("Car\\monstertruck.tga"), true);
		car->SetWheel(_race->GetMesh("Car\\monstertruckWheel.r3d"));		
		car->SetWheels("Data\\Car\\monstertruckWheelGUI.txt");
		car->SetCost(200000);

		FillStandartSlots(car, "wheel", true, &D3DXVECTOR3(-1.5f, 0.0f, 0.35f), false, false, false, true, false, false);

		{
			PlaceSlot place;
			place.active = true;
			place.show = true;
			place.pos = D3DXVECTOR3(-0.8f, 0.0f, 0.65f);

			place.items.push_back(&GetShop().GetSlot("asyncFrost"));
			place.defItem = place.items.front().slot;
			place.lock = true;

			car->SetSlot(Player::stWeapon1, place);
		}

		{
			PlaceSlot place;
			place.active = true;
			place.show = true;
			place.pos = D3DXVECTOR3(0.3f, 0.8f, 0.4f);
			D3DXQUATERNION rot;
			D3DXVECTOR3 offset;

			D3DXQuaternionRotationAxis(&rot, &XVector, -D3DX_PI/2.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("rocketLauncher"), rot));

			D3DXQuaternionRotationAxis(&rot, &XVector, -D3DX_PI/2.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("pulsator"), rot));

			D3DXQuaternionRotationAxis(&rot, &XVector, -D3DX_PI);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("fireGun"), rot));

			offset = D3DXVECTOR3(0.95f, -0.1f, -0.1f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("drobilka"), NullQuaternion, offset));

			place.items.push_back(&GetShop().GetSlot("torpedaWeapon"));

			D3DXQuaternionRotationAxis(&rot, &XVector, -D3DX_PI/2.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("turel"), rot));

			D3DXQuaternionRotationAxis(&rot, &XVector, -D3DX_PI/2.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("phaseImpulse"), rot));

			D3DXQuaternionRotationAxis(&rot, &YVector, -D3DX_PI/24.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("mortira"), rot));

			D3DXQuaternionRotationAxis(&rot, &XVector, -D3DX_PI/2.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("hyperBlaster"), rot));

			car->SetSlot(Player::stWeapon2, place);
		}
		{
			PlaceSlot place;
			place.active = true;
			place.show = true;
			place.pos = D3DXVECTOR3(0.3f, -0.8f, 0.4f);
			D3DXQUATERNION rot;
			D3DXVECTOR3 offset;

			D3DXQuaternionRotationAxis(&rot, &XVector, D3DX_PI/2.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("rocketLauncher"), rot));

			D3DXQuaternionRotationAxis(&rot, &XVector, D3DX_PI/2.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("pulsator"), rot));

			place.items.push_back(&GetShop().GetSlot("fireGun"));

			offset = D3DXVECTOR3(0.95f, 0.1f, -0.1f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("drobilka"), NullQuaternion, offset));

			place.items.push_back(&GetShop().GetSlot("torpedaWeapon"));

			D3DXQuaternionRotationAxis(&rot, &XVector, D3DX_PI/2.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("turel"), rot));

			D3DXQuaternionRotationAxis(&rot, &XVector, D3DX_PI/2.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("phaseImpulse"), rot));

			D3DXQuaternionRotationAxis(&rot, &YVector, -D3DX_PI/24.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("mortira"), rot));

			D3DXQuaternionRotationAxis(&rot, &XVector, D3DX_PI/2.0f);
			place.items.push_back(PlaceItem(&GetShop().GetSlot("hyperBlaster"), rot));

			car->SetSlot(Player::stWeapon3, place);
		}

		{
			PlaceSlot place;
			place.active = true;
			place.show = true;
			place.pos = D3DXVECTOR3(1.4f, 0.0f, 0.6f);
			D3DXVECTOR3 offset;
			D3DXQUATERNION rot;

			place.items.push_back(&GetShop().GetSlot("rocketLauncher"));

			place.items.push_back(&GetShop().GetSlot("pulsator"));

			//place.items.push_back(&GetShop().GetSlot("fireGun"));
			//
			//offset = D3DXVECTOR3(0.1f, 0.0f, -0.15f);
			//place.items.push_back(PlaceItem(&GetShop().GetSlot("drobilka"), NullQuaternion, offset));

			place.items.push_back(&GetShop().GetSlot("torpedaWeapon"));

			place.items.push_back(&GetShop().GetSlot("turel"));

			place.items.push_back(&GetShop().GetSlot("phaseImpulse"));

			car->SetSlot(Player::stWeapon4, place);
		}

		//boss
		Car* boss = AddCar();
		boss->Assign(car);
		boss->SetRecord(_race->GetDB()->GetRecord(MapObjLib::ctCar, "monstertruckBoss"));
		boss->AddBody(_race->GetMesh("Car\\monstertruckBoss.r3d"), _race->GetTexture("Car\\monstertruckBoss.tga"), true);		
	}

	//devildriver
	{
		Car* car = AddCar();
		car->SetName(_SC(scPodushka));
		car->SetDesc(_SC(scPodushkaInfo));
		car->SetRecord(_race->GetDB()->GetRecord(MapObjLib::ctCar, "devildriver"));
		car->AddBody(_race->GetMesh("Car\\devildriver.r3d"), _race->GetTexture("Car\\devildriver.tga"), true);
		car->SetWheels("Data\\Car\\devildriverWheel.txt");
		car->SetCost(300000);

		FillStandartSlots(car, "", false, &D3DXVECTOR3(-1.5f, 0.0f, 0.35f), false, false, false, false, true, false);

		{
			PlaceSlot place;
			place.active = true;
			place.show = true;
			place.pos = D3DXVECTOR3(-0.27f, 0.0f, 0.65f);

			place.items.push_back(&GetShop().GetSlot("sonar"));
			place.defItem = place.items.front().slot;
			place.lock = true;

			car->SetSlot(Player::stWeapon1, place);
		}

		//boss
		Car* boss = AddCar();
		boss->Assign(car);
		boss->SetRecord(_race->GetDB()->GetRecord(MapObjLib::ctCar, "devildriverBoss"));
		boss->AddBody(_race->GetMesh("Car\\devildriverBoss.r3d"), _race->GetTexture("Car\\devildriverBoss.tga"), true);		
	}

	//buggi
	{
		Car* car = AddCar();
		car->SetName(_SC(scBuggi));
		car->SetDesc(_SC(scBuggiInfo));
		car->SetRecord(_race->GetDB()->GetRecord(MapObjLib::ctCar, "buggi"));
		car->AddBody(_race->GetMesh("Car\\buggi.r3d"), _race->GetTexture("Car\\buggi.tga"), true);
		car->SetWheel(_race->GetMesh("Car\\buggiWheel.r3d"));
		car->SetWheels("Data\\Car\\buggiWheel.txt");
		car->SetCost(18000);

		FillStandartSlots(car, "wheel", true, 0, true, true, false, false, false, true);

		{
			PlaceSlot place;
			place.active = true;
			place.show = true;
			place.pos = D3DXVECTOR3(1.2f, 0.02f, 0.53f);

			place.items.push_back(&GetShop().GetSlot("bulletGun"));
			place.defItem = place.items.front().slot;
			place.lock = true;

			car->SetSlot(Player::stWeapon1, place);
		}		
	}

	//tankchetti
	{
		Car* car = AddCar();
		car->SetName(_SC(scTankchetti));
		car->SetDesc(_SC(scTankchettiInfo));
		car->SetRecord(_race->GetDB()->GetRecord(MapObjLib::ctCar, "tankchetti"));
		car->AddBody(_race->GetMesh("Car\\tankchetti.r3d"), _race->GetTexture("Car\\tankchetti.tga"), true);
		car->SetWheel(_race->GetMesh("Car\\tankchettiWheel.r3d"));
		car->SetWheels("Data\\Car\\tankchettiWheel.txt");
		car->SetCost(20000);

		FillStandartSlots(car, "wheel", true, 0, true, true, false, false, false, true);

		{
			PlaceSlot place;
			place.active = true;
			place.show = true;
			place.pos = D3DXVECTOR3(1.88f, 0.0f, -0.22f);

			place.items.push_back(&GetShop().GetSlot("rifleWeapon"));
			place.defItem = place.items.front().slot;
			place.lock = true;

			car->SetSlot(Player::stWeapon1, place);
		}
		{
			PlaceSlot place;
			place.active = true;
			place.show = true;
			place.pos = D3DXVECTOR3(-0.41f, 0.08f, 1.1f);

			place.items.push_back(&GetShop().GetSlot("rocketLauncher"));
			place.defItem = place.items.front().slot;
			place.lock = true;

			car->SetSlot(Player::stWeapon2, place);
		}
	}
}

void Garage::LoadLib()
{
	DeleteCars();

	lsl::RootNode rootNode("garageRoot", _race);

	try
	{		
		SerialFileXML xml;		
		xml.LoadNodeFromFile(rootNode, "garage.xml");

		rootNode.Load(this);
	}
	catch (const EUnableToOpen&)
	{
		LoadCars();
		rootNode.Save(this);

		SerialFileXML xml;
		xml.SaveNodeToFile(rootNode, "garage.xml");
	}
}

Workshop& Garage::GetShop()
{
	return _race->GetWorkshop();
}

void Garage::Save(lsl::SWriter* writer)
{
	Logic* logic = _race->GetGame()->GetWorld()->GetLogic();

	lsl::SWriter* cars = writer->NewDummyNode("cars");
	int i = 0;
	for (Cars::iterator iter = _cars.begin(); iter != _cars.end(); ++iter, ++i)
	{
		std::stringstream sstream;
		sstream << "car" << i;
		lsl::SWriter* child = cars->NewDummyNode(sstream.str().c_str());		

		(*iter)->SaveTo(child, this);
	}

	lsl::SWriteValue(writer, "touchBorderDamage", logic->GetTouchBorderDamage());	
	lsl::SWriteValue(writer, "touchBorderDamageForce", logic->GetTouchBorderDamageForce());
	lsl::SWriteValue(writer, "touchCarDamage", logic->GetTouchCarDamage());
	lsl::SWriteValue(writer, "touchCarDamageForce", logic->GetTouchCarDamageForce());
}

void Garage::Load(lsl::SReader* reader)
{
	Logic* logic = _race->GetGame()->GetWorld()->GetLogic();

	DeleteCars();

	lsl::SReader* cars = reader->ReadValue("cars");
	if (cars)
	{
		lsl::SReader* child = cars->FirstChildValue();
		while (child)
		{	
			Garage::Car* car = AddCar();
			car->LoadFrom(child, this);

			child = child->NextValue();
		}
	}

	D3DXVECTOR2 touchBorderDamage = NullVec2;
	D3DXVECTOR2 touchBorderDamageForce = NullVec2;
	D3DXVECTOR2 touchCarDamage = NullVec2;
	D3DXVECTOR2 touchCarDamageForce = NullVec2;

	lsl::SReadValue(reader, "touchBorderDamage", touchBorderDamage);
	lsl::SReadValue(reader, "touchBorderDamageForce", touchBorderDamageForce);
	lsl::SReadValue(reader, "touchCarDamage", touchCarDamage);
	lsl::SReadValue(reader, "touchCarDamageForce", touchCarDamageForce);

	logic->SetTouchBorderDamage(touchBorderDamage);
	logic->SetTouchBorderDamageForce(touchBorderDamageForce);
	logic->SetTouchCarDamage(touchCarDamage);
	logic->SetTouchCarDamageForce(touchCarDamageForce);
}

Garage::Cars::iterator Garage::FindCar(Car* car)
{
	for (Cars::iterator iter = _cars.begin(); iter != _cars.end(); ++iter)
		if (*iter == car)
			return iter;

	return _cars.end();
}

Garage::Car* Garage::FindCar(MapObjRec* record)
{
	for (Cars::iterator iter = _cars.begin(); iter != _cars.end(); ++iter)
		if ((*iter)->GetRecord() == record)
			return *iter;

	return 0;
}

Garage::Car* Garage::FindCar(const std::string& name)
{
	return name != "" ? FindCar(_race->GetDB()->GetRecord(MapObjLib::ctCar, name)) : NULL;
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
	for (Items::iterator iter = _items.begin(); iter != _items.end(); ++iter)
		(*iter)->Release();

	_items.clear();	
}

Garage::Items::iterator Garage::FindItem(Car* item)
{	
	for (Items::iterator iter = _items.begin(); iter != _items.end(); ++iter)
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
		const Garage::PlaceItem* placeItem = place.FindItem(slot);

		LSL_ASSERT(placeItem);

		pos = place.pos + placeItem->offset;		
		rot = placeItem->rot;
	}

	player->SetSlot(type, slot ? slot->GetRecord() : 0, pos, rot);

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
				weapon->SetCntCharge(chargeCount != -1 ? chargeCount : slot->GetItem<WeaponItem>().GetCntCharge());
			}
		}
	}

	return slotInst;
}

bool Garage::IsSlotSupported(Car* car, Player::SlotType type, Slot* slot)
{
	const PlaceSlot& place = car->GetSlot(type);

	return place.FindItem(slot) != NULL;
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
	if (isSupp && newPlace.defItem && newPlace.defItem != slot && slot->GetItem().GetCost() < newPlace.defItem->GetItem().GetCost())
		return false;

	return isSupp;
}

void Garage::GetSupportedSlots(Player* player, Car* car, std::pair<Slot*, int> (&slots)[Player::cSlotTypeEnd], bool includeDef)
{
	ZeroMemory(slots, sizeof(slots));

	Car* myCar = player->GetCar().record ? FindCar(player->GetCar().record) : NULL;
	
	for (int i = 0; i < Player::cSlotTypeEnd; ++i)
	{
		if (slots[i].first != NULL)
			continue;
		slots[i].second = -1;

		Player::SlotType type = (Player::SlotType)i;

		Record* slotRec = player->GetSlot(type);		
		Slot* slot = slotRec ? _race->GetWorkshop().FindSlot(slotRec) : NULL;
		if (slot == NULL)
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

				type = (Player::SlotType)j;
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
			Player::SlotType type = (Player::SlotType)i;
			const PlaceSlot& place = car->GetSlot(type);

			if (slots[i].first == NULL && place.active)
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
	}
	else
	{
		slots[Player::stWheel][0] = _race->GetSlot("wheel1");
		slots[Player::stWheel][1] = _race->GetSlot("wheel2");
		slots[Player::stWheel][2] = _race->GetSlot("wheel3");
	}

	slots[Player::stTruba][0] = _race->GetSlot("truba1");
	slots[Player::stTruba][1] = _race->GetSlot("truba2");
	slots[Player::stTruba][2] = _race->GetSlot("truba3");

	slots[Player::stArmor][0] = _race->GetSlot("armor1");
	slots[Player::stArmor][1] = _race->GetSlot("armor2");
	slots[Player::stArmor][2] = _race->GetSlot("armor3");

	slots[Player::stMotor][0] = _race->GetSlot("engine1");
	slots[Player::stMotor][1] = _race->GetSlot("engine2");
	slots[Player::stMotor][2] = _race->GetSlot("engine3");

	if (slots[type] && level >= 0 && level < cUpgCntLevel)
		return slots[type][level];
	return NULL;
}

Slot* Garage::GetUpgradeCar(Car* car, Player* player, Player::SlotType type, int level)
{
	if (car == NULL)
		return NULL;

	if (player && player->GetCar().record)
	{
		return _race->GetWorkshop().FindSlot(player->GetSlot(type));
	}
	else if (level == -1)
	{
		//return _race->IsCampaign() ? car->GetSlot(type).defItem : _race->GetWorkshop().FindSlot(GetUpgradeCar(car, type, _upgradeMaxLevel));
		return car->GetSlot(type).defItem;
	}
	else
	{
		Record* record = GetUpgradeCar(car, type, level);		

		return record ? _race->GetWorkshop().FindSlot(record) : NULL;
	}
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
		Player::SlotType type = (Player::SlotType)i;
		Record* slotRec = GetUpgradeCar(car, type, level);

		if (slotRec)
		{
			Slot* slot = _race->GetWorkshop().FindSlot(slotRec);

			const PlaceSlot& place = car->GetSlot(type);
			const Garage::PlaceItem* placeItem = place.FindItem(slot);
			if (placeItem != NULL)
				InstalSlot(player, type, car, slot);
		}

		if (instalMaxCharge)
		{		
			Slot* slot = player->GetSlotInst(type);
			WeaponItem* weapon = slot ? slot->GetItem().IsWeaponItem() : NULL;
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
	return func.maxTorque + std::max(func.longTire.extremumValue - 5.0f, 0.0f) * 300.0f + std::max(func.latTire.extremumValue - 1.5f, 0.0f) * 2000.0f;
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

float Garage::GetArmorSkill(Garage::Car* car, Player* player, float& armorVal, float& maxArmorVal)
{
	armorVal = maxArmorVal = 0;

	if (car == NULL)
		return 0.0f;

	float maxArmor = 0.0f;

	Slot* slot = GetUpgradeCar(car, NULL, Player::stArmor, cUpgMaxLevel);
	LSL_ASSERT(slot);

	ArmorItem* armor = &slot->GetItem<ArmorItem>();

	for (ArmorItem::CarFuncMap::const_iterator iter = armor->carFuncMap.begin(); iter != armor->carFuncMap.end(); ++iter)
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

float Garage::GetDamageSkill(Garage::Car* car, Player* player, float& damageVal, float& maxDamageVal)
{
	maxDamageVal = damageVal = 0;

	if (car == NULL)
		return 0.0f;

	float maxDamage = 0.0f;

	for (Cars::const_iterator iter = _cars.begin(); iter != _cars.end(); ++iter)
	{
		Car* car = *iter;
		float damage = 0.0f;

		for (int i = Player::stWeapon1; i <= Player::stWeapon4; ++i)
		{
			const PlaceSlot& place = car->GetSlot((Player::SlotType)i);
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
				for (PlaceItems::const_iterator iterItem = place.items.begin(); iterItem != place.items.end(); ++iterItem)
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
		Player::SlotType type = (Player::SlotType)i;
		Slot* slot = NULL;

		if (player)
			slot = player->GetSlotInst(type);
		else
			slot = car->GetSlot((Player::SlotType)i).defItem;

		if (slot)
		{
			carDamage += slot->GetItem<WeaponItem>().GetDamage(true);
		}
	}

	damageVal = carDamage;
	maxDamageVal = maxDamage;

	return maxDamageVal != 0 ? damageVal / maxDamageVal : 1.0f;
}

float Garage::GetSpeedSkill(Garage::Car* car, Player* player)
{
	if (car == NULL)
		return 0.0f;

	float maxSpeed = 0.0f;
	for (Cars::const_iterator iter = _cars.begin(); iter != _cars.end(); ++iter)
	{
		float speed = 0.0f;

		speed += GetMobilitySkill(*iter, NULL, Player::stMotor, cUpgMaxLevel);
		speed += GetMobilitySkill(*iter, NULL, Player::stTruba, cUpgMaxLevel);
		speed += GetMobilitySkill(*iter, NULL, Player::stWheel, cUpgMaxLevel);

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

bool Garage::BuyCar(Player* player, Garage::Car* car)
{
	LSL_ASSERT(player && car);

	Car* curCar = player->GetCar().record ? FindCar(player->GetCar().record) : NULL;

	if (curCar == car)
		return true;

	if (!_race->BuyItem(player, car->GetCost()))
		return false;

	if (player->IsHuman() && _race->IsCampaign() && curCar != NULL)
	{
		_race->SetCarChanged(true);
	}

	std::pair<Slot*, int> slots[Player::cSlotTypeEnd];
	GetSupportedSlots(player, car, slots, false);

	player->SetCar(car->GetRecord());

	for (int i = 0; i < Player::cSlotTypeEnd; ++i)
	{
		Player::SlotType type = Player::SlotType(i);
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
			player->SetSlot(type, NULL);
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

const Garage::Items& Garage::GetItems()	 const
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
			for (Race::PlayerList::const_iterator iter = _race->GetPlayerList().begin(); iter != _race->GetPlayerList().end(); ++iter)
			{
				if ((*iter)->GetCar().record)
					_race->GetGarage().MaxUpgradeCar(*iter);
			}

			_race->SendEvent(cUpgradeMaxLevelChanged, NULL);
		}
	}
}

int Garage::GetWeaponMaxLevel() const
{
	return _weaponMaxLevel;
}

void Garage::SetWeaponMaxLevel(int value)
{
	if (_weaponMaxLevel != value)
	{
		_weaponMaxLevel = value;

		if (_race->IsSkirmish())
		{
			for (Race::PlayerList::const_iterator iter = _race->GetPlayerList().begin(); iter != _race->GetPlayerList().end(); ++iter)
			{
				if ((*iter)->IsHuman())
				{					
					for (int i = _weaponMaxLevel; i < Garage::cWeaponMaxLevel; ++i)
						(*iter)->SetSlot((Player::SlotType)(i + Player::stWeapon1), NULL);
				}
				else
					_race->GetTournament().GetCurPlanet().StartPass(*iter);
			}

			_race->SendEvent(cWeaponMaxLevelChanged, NULL);
		}
	}
}




Planet::Planet(Race* race, int index): _race(race), _index(index), _name(scNull), _info(scNull), _mesh(0), _texture(0), _state(psUnavailable), _worldType(wtWorld1), _pass(-1)
{
}

Planet::~Planet()
{
	ClearCars();
	ClearSlots();

	SetTexture(0);
	SetMesh(0);
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
		player->SetCar(0);
		for (int i = 0; i < Player::cSlotTypeEnd; ++i)
			player->SetSlot(Player::SlotType(i), 0);

		for (Cars::const_iterator iterCar = iter->cars.begin(); iterCar != iter->cars.end(); ++iterCar)
			if (iterCar->pass == pass)
			{
				player->SetCar(iterCar->record);
				break;
			}

			Garage::Car* car = player->GetCar().record ? _race->GetGarage().FindCar(player->GetCar().record) : 0;

			for (Slots::const_iterator iterSlot = iter->slots.begin(); iterSlot != iter->slots.end(); ++iterSlot)
			{
				Slot* slot = _race->GetWorkshop().FindSlot(iterSlot->record);
				if (iterSlot->pass == pass && car && slot)
				{
					_race->GetGarage().InstalSlot(player, iterSlot->type, car, slot);
					if (player->GetSlotInst(iterSlot->type) && player->GetSlotInst(iterSlot->type)->GetItem().IsWeaponItem())
						player->GetSlotInst(iterSlot->type)->GetItem().IsWeaponItem()->SetCntCharge(iterSlot->charge);
				}
			}

			if (_race->IsSkirmish() && player->GetCar().record)
			{
				_race->GetGarage().MaxUpgradeCar(player);

				for (int i = _race->GetGarage().GetWeaponMaxLevel(); i < Garage::cWeaponMaxLevel; ++i)
					player->SetSlot((Player::SlotType)(i + Player::stWeapon1), NULL);
			}
	}
}

void Planet::StartPass(int pass)
{
	for (Race::PlayerList::const_iterator iter = _race->GetPlayerList().begin(); iter != _race->GetPlayerList().end(); ++iter)
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

void Planet::SaveSlots(lsl::SWriter* writer, const std::string& name, Slots& mSlots, Tournament* owner)
{
	lsl::SWriter* slots = writer->NewDummyNode(name.c_str());
	int i = 0;
	for (Slots::iterator iter = mSlots.begin(); iter != mSlots.end(); ++iter, ++i)
	{
		std::stringstream sstream;
		sstream << "slots" << i;
		lsl::SWriter* child = slots->NewDummyNode(sstream.str().c_str());

		RecordLib::SaveRecordRef(child, "record", iter->record);
		child->WriteValue("charge", iter->charge);
		if (iter->type != Player::cSlotTypeEnd)
			lsl::SWriteEnum(child, "type", iter->type, Player::cSlotTypeStr, Player::cSlotTypeEnd);
		child->WriteValue("pass", iter->pass);
	}
}

void Planet::LoadSlots(lsl::SReader* reader, const std::string& name, Slots& mSlots, Tournament* owner)
{
	lsl::SReader* slots = reader->ReadValue(name.c_str());
	if (slots)
	{
		lsl::SReader* child = slots->FirstChildValue();
		while (child)
		{
			SlotData slot;

			slot.record = RecordLib::LoadRecordRef(child, "record");
			child->ReadValue("charge", slot.charge);
			lsl::SReadEnum(child, "type", slot.type, Player::cSlotTypeStr, Player::cSlotTypeEnd);
			child->ReadValue("pass", slot.pass);

			mSlots.push_back(slot);

			child = child->NextValue();
		}
	}
}

void Planet::SaveCars(lsl::SWriter* writer, const std::string& name, Cars& mCars, Tournament* owner)
{
	lsl::SWriter* cars = writer->NewDummyNode(name.c_str());
	int i = 0;
	for (Cars::iterator iter = mCars.begin(); iter != mCars.end(); ++iter, ++i)
	{
		std::stringstream sstream;
		sstream << "car" << i;
		lsl::SWriter* child = cars->NewDummyNode(sstream.str().c_str());

		MapObjLib::SaveRecordRef(child, "record", iter->record);
		child->WriteValue("pass", iter->pass);
	}
}

void Planet::LoadCars(lsl::SReader* reader, const std::string& name, Cars& mCars, Tournament* owner)
{
	lsl::SReader* cars = reader->ReadValue(name.c_str());
	if (cars)
	{
		lsl::SReader* child = cars->FirstChildValue();
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
	TrackMap::iterator iter = _trackMap.find(pass);
	if (iter == _trackMap.end())
		iter = _trackMap.insert(iter, TrackMap::value_type(pass, Tracks()));

	Track* track = new Track(this);
	iter->second.push_back(track);
	track->_index = iter->second.size() - 1;

	_trackList.push_back(track);

	return track;
}

void Planet::ClearTracks()
{
	for (TrackMap::const_iterator iterMap = _trackMap.begin(); iterMap != _trackMap.end(); ++iterMap)
		for (Tracks::const_iterator iter = iterMap->second.begin(); iter != iterMap->second.end(); ++iter)
			delete *iter;

	_trackMap.clear();
	_trackList.clear();
}

Planet::Track* Planet::NextTrack(Track* track)
{
	const Tracks& tracks = GetTracks();
	Tracks::const_iterator iter2 = tracks.Find(track);

	return iter2 != --(tracks.end()) ? *(++iter2) : NULL;
}

Planet::TrackMap::const_iterator Planet::GetTracks(int pass) const
{
	int maxPass = 0;

	for (TrackMap::const_iterator iter = _trackMap.begin(); iter != _trackMap.end(); ++iter)
		if (maxPass <= iter->first)
			maxPass = iter->first;

	pass = abs(pass - 1) % maxPass + 1;

	Planet::TrackMap::const_iterator iter = _trackMap.find(pass);
	if (iter != _trackMap.end())
		return iter;

	return _trackMap.begin();
}

const Planet::Tracks& Planet::GetTracks() const
{
	LSL_ASSERT(_trackMap.size() > 0);

	if (_race->IsCampaign())
		return GetTracks(_pass)->second;
	else
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

void Planet::SaveTo(lsl::SWriter* writer, Tournament* owner)
{
	lsl::SWriteValue(writer, "name", _name);
	lsl::SWriteValue(writer, "info", _info);
	lsl::SWriteEnum(writer, "worldType", _worldType, cWorldTypeStr, cWorldTypeEnd);
	writer->WriteRef("mesh", _mesh);
	writer->WriteRef("texture", _texture);

	int i = 0;
	int j = 0;
	lsl::SWriter* trackMap = writer->NewDummyNode("trackMap");
	for (TrackMap::const_iterator iterMap = _trackMap.begin(); iterMap != _trackMap.end(); ++iterMap, ++i)
	{
		const Tracks& trackList = iterMap->second;
		
		lsl::SWriter* tracks = trackMap->NewDummyNode(lsl::StrFmt("tracks%d", i).c_str());
		tracks->WriteAttr("pass", iterMap->first);

		for (Tracks::const_iterator iter = trackList.begin(); iter != trackList.end(); ++iter, ++j)
		{
			Track* track = *iter;

			lsl::SWriter* child = tracks->NewDummyNode(lsl::StrFmt("track%d", j).c_str());

			child->WriteValue("level", track->level);
			child->WriteValue("numLaps", track->numLaps);
		}
	}

	lsl::SWriter* points = writer->NewDummyNode("points");
	i = 0;
	for (RequestPoints::iterator iter = _requestPoints.begin(); iter != _requestPoints.end(); ++iter, ++i)
	{
		std::stringstream sstream;
		sstream << "point" << i;
		lsl::SWriter* child = points->NewDummyNode(sstream.str().c_str());

		child->WriteValue("place", iter->first);
		child->WriteValue("value", iter->second);
	}

	lsl::SWriter* wheaters = writer->NewDummyNode("wheaters");
	i = 0;
	for (Wheaters::iterator iter = _wheaters.begin(); iter != _wheaters.end(); ++iter, ++i)
	{
		std::stringstream sstream;
		sstream << "wheater" << i;
		lsl::SWriter* child = wheaters->NewDummyNode(sstream.str().c_str());

		lsl::SWriteEnum(child, "type", iter->type, Environment::cWheaterStr, Environment::cWheaterEnd);
		child->WriteValue("chance", iter->chance);
	}

	lsl::SWriter* prices = writer->NewDummyNode("prices");
	i = 0;
	for (Prices::iterator iter = _prices.begin(); iter != _prices.end(); ++iter, ++i)
	{
		std::stringstream sstream;
		sstream << "price" << i;
		lsl::SWriter* child = prices->NewDummyNode(sstream.str().c_str());

		child->WriteValue("money", iter->money);
		child->WriteValue("points", iter->points);
	}

	SaveSlots(writer, "slots", _slots, owner);
	SaveCars(writer, "cars", _cars, owner);

	lsl::SWriter* players = writer->NewDummyNode("players");
	i = 0;
	for (Players::iterator iter = _players.begin(); iter != _players.end(); ++iter, ++i)
	{
		std::stringstream sstream;
		sstream << "player" << i;
		lsl::SWriter* child = players->NewDummyNode(sstream.str().c_str());

		child->WriteValue("id", iter->id);
		lsl::SWriteValue(child, "name", iter->name);
		lsl::SWriteValue(child, "bonus", iter->bonus);
		child->WriteRef("photo", iter->photo);		
		child->WriteValue("maxPass", iter->maxPass);

		SaveCars(child, "cars", iter->cars, owner);
		SaveSlots(child, "slots", iter->slots, owner);
	}
}

void Planet::LoadFrom(lsl::SReader* reader, Tournament* owner)
{
	_requestPoints.clear();
	_wheaters.clear();
	_prices.clear();
	ClearCars();
	ClearSlots();
	ClearPlayers();
	ClearTracks();	

	lsl::SReadValue(reader, "name", _name);
	lsl::SReadValue(reader, "info", _info);
	lsl::SReadEnum(reader, "worldType", _worldType, cWorldTypeStr, cWorldTypeEnd);

	lsl::Serializable::FixUpName fixUpName;
	if (reader->ReadRef("mesh", true, NULL, &fixUpName))
		SetMesh(fixUpName.GetCollItem<graph::IndexedVBMesh*>());

	if (reader->ReadRef("texture", true, NULL, &fixUpName))
		SetTexture(fixUpName.GetCollItem<graph::Tex2DResource*>());

	lsl::SReader* trackMap = reader->ReadValue("trackMap");
	if (trackMap)
	{
		lsl::SReader* childMap = trackMap->FirstChildValue();
		while (childMap)
		{
			int pass = 1;
			const lsl::SerialNode::ValueDesc* val = childMap->ReadAttr("pass");
			if (val)
				val->CastTo<int>(&pass);

//#ifdef _DEBUG
//			{
//				Track* track = AddTrack(pass);
//				track->level = "Data\\Map\\debugTrack.r3dMap";
//				track->numLaps = 99;
//			}
//#endif

			lsl::SReader* child = childMap->FirstChildValue();
			while (child)
			{
				Track* track = AddTrack(pass);

				child->ReadValue("level", track->level);
				child->ReadValue("numLaps", track->numLaps);

				child = child->NextValue();
			}

			childMap = childMap->NextValue();
		}
	}

	lsl::SReader* points = reader->ReadValue("points");
	if (points)
	{
		lsl::SReader* child = points->FirstChildValue();
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

	lsl::SReader* wheaters = reader->ReadValue("wheaters");
	if (wheaters)
	{
		lsl::SReader* child = wheaters->FirstChildValue();
		while (child)
		{
			Wheater wheater;

			lsl::SReadEnum(child, "type", wheater.type, Environment::cWheaterStr, Environment::cWheaterEnd);
			child->ReadValue("chance", wheater.chance);

			_wheaters.push_back(wheater);

			child = child->NextValue();
		}
	}

	lsl::SReader* prices = reader->ReadValue("prices");
	if (prices)
	{
		lsl::SReader* child = prices->FirstChildValue();
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

	lsl::SReader* players = reader->ReadValue("players");
	if (players)
	{
		lsl::SReader* child = players->FirstChildValue();
		while (child)
		{			
			PlayerData player;

			child->ReadValue("id", player.id);
			lsl::SReadValue(child, "name", player.name);
			lsl::SReadValue(child, "bonus", player.bonus);
			child->ReadValue("maxPass", player.maxPass);

			if (child->ReadRef("photo", true, NULL, &fixUpName))
				player.photo = fixUpName.GetCollItem<graph::Tex2DResource*>();			

			LoadCars(child, "cars", player.cars, owner);
			LoadSlots(child, "slots", player.slots, owner);

			InsertPlayer(player);

			child = child->NextValue();
		}
	}

	//deprecated block
	{
		lsl::SReader* tracks = reader->ReadValue("tracks");
		if (tracks)
		{
			lsl::SReader* child = tracks->FirstChildValue();
			while (child)
			{	
				Track* track = AddTrack(1);

				child->ReadValue("level", track->level);
				child->ReadValue("numLaps", track->numLaps);

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

void Planet::SetRequestPoints(const Planet::RequestPoints& value)
{
	_requestPoints = value;
}

int Planet::GetRequestPoints(int pass) const
{
	int numPlayers = 0;
	for (Race::PlayerList::const_iterator iter = _race->GetPlayerList().begin(); iter != _race->GetPlayerList().end(); ++iter)
		if ((*iter)->IsHuman() || (*iter)->IsOpponent())
			++numPlayers;

	numPlayers = std::max(numPlayers, 1);
	int points = 0;

	RequestPoints::const_iterator iter = _requestPoints.find(pass);
	if (iter != _requestPoints.end())
		points = iter->second;
	else if (_requestPoints.size() > 0)
		points = _requestPoints.rbegin()->second;
	else
		return -1;

	if (numPlayers > 3)
		return Ceil<int>(points * 1.35f / 100.0f) * 100;
	else if (numPlayers > 2)
		return Ceil<int>(points * 1.3f / 100.0f) * 100;
	else if (numPlayers > 1)
		return Ceil<int>(points * 1.3f / 100.0f) * 100;
	else
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
	Planet::Wheater maxWheater = Wheater(Environment::ewFair, 0);
	float maxChance = 0.0f;
	float summChance = 0.0f;

	for (Wheaters::const_iterator iter = _wheaters.begin(); iter != _wheaters.end(); ++iter)
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

	for (Wheaters::const_iterator iter = _wheaters.begin(); iter != _wheaters.end(); ++iter)
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
	if (Object::ReplaceRef(_mesh, value))
		_mesh = value;
}

graph::Tex2DResource* Planet::GetTexture()
{
	return _texture;
}

void Planet::SetTexture(graph::Tex2DResource* value)
{
	if (Object::ReplaceRef(_texture, value))
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

void Planet::InsertSlot(const lsl::string& name, int pass)
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

	for (Slots::const_iterator iter = value.begin(); iter != value.end(); ++iter)
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

void Planet::InsertCar(const lsl::string& name, int pass)
{
	InsertCar(_race->GetGarage().FindCar(name)->GetRecord(), pass);
}

void Planet::SetCars(const Cars& value)
{
	ClearCars();

	for (Cars::const_iterator iter = value.begin(); iter != value.end(); ++iter)
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
	for (Cars::const_iterator iter = data.cars.begin(); iter != data.cars.end(); ++iter)
		iter->record->AddRef();
	for (Slots::const_iterator iter = data.slots.begin(); iter != data.slots.end(); ++iter)
		iter->record->AddRef();

	_players.push_back(data);
}

void Planet::ClearPlayers()
{
	for (Players::iterator iter = _players.begin(); iter != _players.end(); ++iter)
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
	for (Players::const_iterator iter = _players.begin(); iter != _players.end(); ++iter)
		if (iter->id == id)
			return &(*iter);

	return NULL;
}

const Planet::PlayerData* Planet::GetPlayer(const std::string& name) const
{
	for (Players::const_iterator iter = _players.begin(); iter != _players.end(); ++iter)
		if (iter->name == name)
			return &(*iter);

	return NULL;
}

Planet::PlayerData Planet::GetBoss() const
{
	return _players.size() > 0 ? _players.front() : PlayerData();
}




Tournament::Tournament(Race* race, const std::string& name): _race(race), _curPlanet(0), _curTrack(0), _wheater(Environment::ewFair), _wheaterNightPass(false), _lapsCount(4)
{	
	SetName(name);
	SetOwner(race);

	LoadLib();
}

Tournament::~Tournament()
{
	SetCurTrack(0);
	SetCurPlanet(0);

	ClearGamers();
	ClearPlanets();
}

void Tournament::LoadPlanets()
{
	{
		Planet* planet = AddPlanet();
		planet->SetName(_SC(scIntaria));
		planet->SetInfo(_SC(scIntariaInfo));
		planet->SetWorldType(Planet::wtWorld1);

		Planet::Wheaters wheaters;
		wheaters.push_back(Planet::Wheater(Environment::ewFair, 70));
		wheaters.push_back(Planet::Wheater(Environment::ewClody, 20));
		wheaters.push_back(Planet::Wheater(Environment::ewRainy, 10));
		planet->SetWheaters(wheaters);

		Planet::RequestPoints points;
		points[1] = 1400;
		points[2] = 2000;
		planet->SetRequestPoints(points);

		Planet::Prices prices;
		prices.push_back(Planet::Price(10000, 400));
		prices.push_back(Planet::Price(7000, 200));
		prices.push_back(Planet::Price(4000, 100));
		planet->SetPrices(prices);

		planet->SetMesh(_race->GetMesh("GUI\\planet.r3d"));
		planet->SetTexture(_race->GetTexture("GUI\\intaria.dds"));

		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World1\\map1.r3dMap";
			track->numLaps = 4;
		}
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World1\\map2.r3dMap";
			track->numLaps = 4;
		}
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World1\\map3.r3dMap";
			track->numLaps = 4;
		}
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World1\\map4.r3dMap";
			track->numLaps = 4;
		}
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World1\\map5.r3dMap";
			track->numLaps = 4;
		}
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World1\\map6.r3dMap";
			track->numLaps = 4;
		}
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World1\\map7.r3dMap";
			track->numLaps = 4;
		}
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World1\\map8.r3dMap";
			track->numLaps = 4;
		}		

		planet->InsertCar("marauder", 0);		
		planet->InsertCar("dirtdevil", 0);		
		planet->InsertCar("manticora", 0);

		planet->InsertSlot("rocketLauncher", 0);		
		planet->InsertSlot("pulsator", 0);		

		//Player1
		{
			Planet::PlayerData data;
			data.id = Race::cComputer1;
			data.name = scTarquin;
			data.photo = _race->GetTexture("GUI\\Chars\\tarquin.png");			
			data.maxPass = 2;

			//first
			data.cars.push_back(Planet::CarData(_race->GetCar("manticoraBoss"), 1));			
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("engine1"), 0, Player::stMotor, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("armor1"), 0, Player::stArmor, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("wheel1"), 0, Player::stWheel, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("truba1"), 0, Player::stTruba, 1));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("blasterGun"), 3, Player::stWeapon1, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("rocketLauncher"), 2, Player::stWeapon2, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("mine1"), 2, Player::stMine, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("hyperdrive"), 3, Player::stHyper, 1));

			//second
			data.cars.push_back(Planet::CarData(_race->GetCar("manticoraBoss"), 2));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("engine2"), 0, Player::stMotor, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("armor1"), 0, Player::stArmor, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("wheel1"), 0, Player::stWheel, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("truba1"), 0, Player::stTruba, 2));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("blasterGun"), 4, Player::stWeapon1, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("rocketLauncher"), 4, Player::stWeapon2, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("mine1"), 3, Player::stMine, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("hyperdrive"), 3, Player::stHyper, 2));

			planet->InsertPlayer(data);
		}
		//Player2
		{
			Planet::PlayerData data;
			data.id = Race::cComputer2;
			data.name = scRip;
			data.photo = _race->GetTexture("GUI\\Chars\\rip.png");			
			data.maxPass = 2;

			//first
			data.cars.push_back(Planet::CarData(_race->GetCar("marauder"), 1));			
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("engine1"), 0, Player::stMotor, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("armor1"), 0, Player::stArmor, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("wheel1"), 0, Player::stWheel, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("truba1"), 0, Player::stTruba, 1));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("bulletGun"), 10, Player::stWeapon1, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("maslo"), 3, Player::stMine, 1));
			//data.slots.push_back(Planet::SlotData(_race->GetSlot("spring"), 1, Player::stHyper, 1));

			//second
			data.cars.push_back(Planet::CarData(_race->GetCar("marauder"), 2));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("engine2"), 0, Player::stMotor, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("armor1"), 0, Player::stArmor, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("wheel1"), 0, Player::stWheel, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("truba1"), 0, Player::stTruba, 2));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("bulletGun"), 15, Player::stWeapon1, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("maslo"), 3, Player::stMine, 2));
			//data.slots.push_back(Planet::SlotData(_race->GetSlot("spring"), 3, Player::stHyper, 2));

			planet->InsertPlayer(data);
		}
		//Player3
		{
			Planet::PlayerData data;
			data.id = Race::cComputer3;
			data.name = scSnake;
			data.photo = _race->GetTexture("GUI\\Chars\\snake.png");			
			data.maxPass = 2;

			//first
			data.cars.push_back(Planet::CarData(_race->GetCar("dirtdevil"), 1));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("engine1"), 0, Player::stMotor, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("armor1"), 0, Player::stArmor, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("wheel1"), 0, Player::stWheel, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("truba2"), 0, Player::stTruba, 1));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("rifleWeapon"), 4, Player::stWeapon1, 1));			
			data.slots.push_back(Planet::SlotData(_race->GetSlot("maslo"), 2, Player::stMine, 1));
			//data.slots.push_back(Planet::SlotData(_race->GetSlot("spring"), 1, Player::stHyper, 1));

			//second
			data.cars.push_back(Planet::CarData(_race->GetCar("marauder"), 2));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("engine2"), 0, Player::stMotor, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("armor1"), 0, Player::stArmor, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("wheel1"), 0, Player::stWheel, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("truba1"), 0, Player::stTruba, 2));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("bulletGun"), 10, Player::stWeapon1, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("maslo"), 3, Player::stMine, 2));
			//data.slots.push_back(Planet::SlotData(_race->GetSlot("spring"), 3, Player::stHyper, 2));

			planet->InsertPlayer(data);
		}		
	}

	{
		Planet* planet = AddPlanet();
		planet->SetName(_SC(scPatagonis));
		planet->SetInfo(_SC(scPatagonisInfo));
		planet->SetWorldType(Planet::wtWorld2);

		Planet::Wheaters wheaters;
		wheaters.push_back(Planet::Wheater(Environment::ewClody, 65));
		wheaters.push_back(Planet::Wheater(Environment::ewRainy, 25));
		wheaters.push_back(Planet::Wheater(Environment::ewNight, 10));
		planet->SetWheaters(wheaters);
		
		Planet::RequestPoints points;
		points[1] = 2000;
		points[2] = 2400;
		planet->SetRequestPoints(points);

		Planet::Prices prices;
		prices.push_back(Planet::Price(10000, 400));
		prices.push_back(Planet::Price(7000, 200));
		prices.push_back(Planet::Price(4000, 100));
		planet->SetPrices(prices);

		planet->SetMesh(_race->GetMesh("GUI\\planet.r3d"));
		planet->SetTexture(_race->GetTexture("GUI\\patagonis.dds"));

#if _DEBUG | DEBUG_PX
		Planet::Track* track0 = planet->AddTrack(1);
		track0->level = "Data\\Map\\debugTrack.r3dMap";
		track0->numLaps = 99;
#endif

		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World2\\map1.r3dMap";
			track->numLaps = 4;
		}
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World2\\map2.r3dMap";
			track->numLaps = 4;
		}
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World2\\map3.r3dMap";
			track->numLaps = 4;
		}
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World2\\map4.r3dMap";
			track->numLaps = 4;
		}
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World2\\map5.r3dMap";
			track->numLaps = 4;
		}
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World2\\map6.r3dMap";
			track->numLaps = 4;
		}
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World2\\map7.r3dMap";
			track->numLaps = 4;
		}
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World2\\map8.r3dMap";
			track->numLaps = 4;
		}

		planet->InsertCar("guseniza", 0);		

		planet->InsertSlot("fireGun", 0);		
		planet->InsertSlot("drobilka", 0);		
		planet->InsertSlot("torpedaWeapon", 0);		
		planet->InsertSlot("turel", 0);		

		//Player1
		{
			Planet::PlayerData data;
			data.id = Race::cComputer1;
			data.name = scTarquin;
			data.photo = _race->GetTexture("GUI\\Chars\\tarquin.png");			
			data.maxPass = 2;

			//first
			data.cars.push_back(Planet::CarData(_race->GetCar("gusenizaBoss"), 1));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("engine2"), 0, Player::stMotor, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("armor1"), 0, Player::stArmor, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("gusWheel1"), 0, Player::stWheel, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("truba1"), 0, Player::stTruba, 1));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("tankLaser"), 6, Player::stWeapon1, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("rocketLauncher"), 3, Player::stWeapon2, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("torpedaWeapon"), 1, Player::stWeapon3, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("mine2"), 3, Player::stMine, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("hyperdrive"), 4, Player::stHyper, 1));

			//second
			data.cars.push_back(Planet::CarData(_race->GetCar("gusenizaBoss"), 2));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("engine2"), 0, Player::stMotor, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("armor2"), 0, Player::stArmor, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("gusWheel1"), 0, Player::stWheel, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("truba2"), 0, Player::stTruba, 2));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("tankLaser"), 7, Player::stWeapon1, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("rocketLauncher"), 4, Player::stWeapon2, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("torpedaWeapon"), 1, Player::stWeapon3, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("mine2"), 4, Player::stMine, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("hyperdrive"), 5, Player::stHyper, 2));

			planet->InsertPlayer(data);
		}
		//Player2
		{
			Planet::PlayerData data;
			data.id = Race::cComputer2;
			data.name = scRip;
			data.photo = _race->GetTexture("GUI\\Chars\\rip.png");			
			data.maxPass = 2;

			//first
			data.cars.push_back(Planet::CarData(_race->GetCar("manticora"), 1));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("engine2"), 0, Player::stMotor, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("armor1"), 0, Player::stArmor, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("wheel1"), 0, Player::stWheel, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("truba1"), 0, Player::stTruba, 1));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("blasterGun"), 5, Player::stWeapon1, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("fireGun"), 3, Player::stWeapon2, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("rocketLauncher"), 3, Player::stWeapon3, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("mine1"), 3, Player::stMine, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("hyperdrive"), 3, Player::stHyper, 1));

			//second
			data.cars.push_back(Planet::CarData(_race->GetCar("manticora"), 2));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("engine2"), 0, Player::stMotor, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("armor2"), 0, Player::stArmor, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("wheel2"), 0, Player::stWheel, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("truba1"), 0, Player::stTruba, 2));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("blasterGun"), 5, Player::stWeapon1, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("fireGun"), 3, Player::stWeapon2, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("rocketLauncher"), 5, Player::stWeapon3, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("mine1"), 3, Player::stMine, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("hyperdrive"), 4, Player::stHyper, 2));

			planet->InsertPlayer(data);
		}
		//Player3
		{
			Planet::PlayerData data;
			data.id = Race::cComputer3;
			data.name = scTarquin;
			data.photo = _race->GetTexture("GUI\\Chars\\snake.png");			
			data.maxPass = 2;

			//first
			data.cars.push_back(Planet::CarData(_race->GetCar("marauder"), 1));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("engine2"), 0, Player::stMotor, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("armor2"), 0, Player::stArmor, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("wheel2"), 0, Player::stWheel, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("truba1"), 0, Player::stTruba, 1));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("bulletGun"), 15, Player::stWeapon1, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("maslo"), 5, Player::stMine, 1));
			//data.slots.push_back(Planet::SlotData(_race->GetSlot("spring"), 3, Player::stHyper, 1));

			//second
			data.cars.push_back(Planet::CarData(_race->GetCar("manticora"), 2));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("engine2"), 0, Player::stMotor, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("armor2"), 0, Player::stArmor, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("wheel1"), 0, Player::stWheel, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("truba1"), 0, Player::stTruba, 2));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("blasterGun"), 2, Player::stWeapon1, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("pulsator"), 7, Player::stWeapon2, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("mine1"), 2, Player::stMine, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("hyperdrive"), 3, Player::stHyper, 2));

			planet->InsertPlayer(data);
		}		
	}

	{
		Planet* planet = AddPlanet();
		planet->SetName(_SC(scHemi5));
		planet->SetInfo(_SC(scHemi5Info));
		planet->SetWorldType(Planet::wtWorld3);

		Planet::Wheaters wheaters;
		wheaters.push_back(Planet::Wheater(Environment::ewSahara, 100));
		//wheaters.push_back(Planet::Wheater(Environment::ewRainy, 25));
		//wheaters.push_back(Planet::Wheater(Environment::ewNight, 10));
		planet->SetWheaters(wheaters);

		Planet::RequestPoints points;
		points[1] = 2400;
		points[2] = 2800;
		planet->SetRequestPoints(points);

		Planet::Prices prices;
		prices.push_back(Planet::Price(10000, 400));
		prices.push_back(Planet::Price(7000, 200));
		prices.push_back(Planet::Price(4000, 100));
		planet->SetPrices(prices);

		planet->SetMesh(_race->GetMesh("GUI\\planet.r3d"));
		planet->SetTexture(_race->GetTexture("GUI\\chemi5.dds"));

		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World3\\map1.r3dMap";
			track->numLaps = 4;
		}
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World3\\map2.r3dMap";
			track->numLaps = 4;
		}
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World3\\map3.r3dMap";
			track->numLaps = 4;
		}
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World3\\map4.r3dMap";
			track->numLaps = 4;
		}
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World3\\map5.r3dMap";
			track->numLaps = 4;
		}
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World3\\map6.r3dMap";
			track->numLaps = 4;
		}
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World3\\map7.r3dMap";
			track->numLaps = 4;
		}
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World3\\map8.r3dMap";
			track->numLaps = 4;
		}		
		
		planet->InsertCar("podushka", 0);
		planet->InsertCar("monstertruck", 0);
		
		planet->InsertSlot("mortira", 0);		
		planet->InsertSlot("phaseImpulse", 0);		

		//Player1
		{
			Planet::PlayerData data;
			data.id = Race::cComputer1;
			data.name = scTarquin;
			data.photo = _race->GetTexture("GUI\\Chars\\tarquin.png");			
			data.maxPass = 2;

			//first
			data.cars.push_back(Planet::CarData(_race->GetCar("monstertruckBoss"), 1));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("engine3"), 0, Player::stMotor, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("armor2"), 0, Player::stArmor, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("wheel2"), 0, Player::stWheel, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("truba2"), 0, Player::stTruba, 1));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("asyncFrost"), 4, Player::stWeapon1, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("torpedaWeapon"), 2, Player::stWeapon2, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("fireGun"), 5, Player::stWeapon3, 1));			
			data.slots.push_back(Planet::SlotData(_race->GetSlot("mine2"), 4, Player::stMine, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("hyperdrive"), 6, Player::stHyper, 1));

			//second
			data.cars.push_back(Planet::CarData(_race->GetCar("monstertruckBoss"), 2));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("engine3"), 0, Player::stMotor, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("armor2"), 0, Player::stArmor, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("wheel3"), 0, Player::stWheel, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("truba2"), 0, Player::stTruba, 2));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("asyncFrost"), 5, Player::stWeapon1, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("torpedaWeapon"), 2, Player::stWeapon2, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("torpedaWeapon"), 1, Player::stWeapon3, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("turel"), 6, Player::stWeapon4, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("mine2"), 4, Player::stMine, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("hyperdrive"), 7, Player::stHyper, 2));

			planet->InsertPlayer(data);
		}
		//Player2
		{
			Planet::PlayerData data;
			data.id = Race::cComputer2;
			data.name = scRip;
			data.photo = _race->GetTexture("GUI\\Chars\\rip.png");			
			data.maxPass = 2;

			//first
			data.cars.push_back(Planet::CarData(_race->GetCar("guseniza"), 1));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("engine3"), 0, Player::stMotor, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("armor2"), 0, Player::stArmor, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("gusWheel2"), 0, Player::stWheel, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("truba2"), 0, Player::stTruba, 1));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("tankLaser"), 5, Player::stWeapon1, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("rocketLauncher"), 7, Player::stWeapon2, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("mortira"), 2, Player::stWeapon3, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("mine2"), 3, Player::stMine, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("hyperdrive"), 5, Player::stHyper, 1));

			//second
			data.cars.push_back(Planet::CarData(_race->GetCar("podushka"), 2));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("engine3"), 0, Player::stMotor, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("armor2"), 0, Player::stArmor, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("truba2"), 0, Player::stTruba, 2));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("sphereGun"), 6, Player::stWeapon1, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("turel"), 3, Player::stWeapon2, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("rocketLauncher"), 7, Player::stWeapon3, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("mine3"), 4, Player::stMine, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("hyperdrive"), 6, Player::stHyper, 2));

			planet->InsertPlayer(data);
		}
		//Player3
		{
			Planet::PlayerData data;
			data.id = Race::cComputer3;
			data.name = scSnake;
			data.photo = _race->GetTexture("GUI\\Chars\\snake.png");			
			data.maxPass = 2;

			//first
			data.cars.push_back(Planet::CarData(_race->GetCar("manticora"), 1));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("engine3"), 0, Player::stMotor, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("armor3"), 0, Player::stArmor, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("wheel2"), 0, Player::stWheel, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("truba2"), 0, Player::stTruba, 1));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("blasterGun"), 7, Player::stWeapon1, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("drobilka"), 7, Player::stWeapon2, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("mine1"), 6, Player::stMine, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("hyperdrive"), 5, Player::stHyper, 1));

			//second
			data.cars.push_back(Planet::CarData(_race->GetCar("guseniza"), 2));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("engine3"), 0, Player::stMotor, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("armor2"), 0, Player::stArmor, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("gusWheel2"), 0, Player::stWheel, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("truba3"), 0, Player::stTruba, 2));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("tankLaser"), 7, Player::stWeapon1, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("drobilka"), 7, Player::stWeapon2, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("pulsator"), 3, Player::stWeapon3, 2));			
			data.slots.push_back(Planet::SlotData(_race->GetSlot("mine2"), 4, Player::stMine, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("hyperdrive"), 5, Player::stHyper, 2));

			planet->InsertPlayer(data);
		}		
	}

	{
		Planet* planet = AddPlanet();
		planet->SetName(_SC(scNho));
		planet->SetInfo(_SC(scNhoInfo));
		planet->SetWorldType(Planet::wtWorld5);

		Planet::Wheaters wheaters;
		wheaters.push_back(Planet::Wheater(Environment::ewSnow, 65));
		//wheaters.push_back(Planet::Wheater(Environment::ewRainy, 25));
		//wheaters.push_back(Planet::Wheater(Environment::ewNight, 10));
		planet->SetWheaters(wheaters);

		Planet::RequestPoints points;
		points[1] = 2800;
		points[2] = 3200;
		planet->SetRequestPoints(points);

		Planet::Prices prices;
		prices.push_back(Planet::Price(10000, 400));
		prices.push_back(Planet::Price(7000, 200));
		prices.push_back(Planet::Price(4000, 100));
		planet->SetPrices(prices);

		planet->SetMesh(_race->GetMesh("GUI\\planet.r3d"));
		planet->SetTexture(_race->GetTexture("GUI\\nho.dds"));

#if _DEBUG | DEBUG_PX
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World5\\map0.r3dMap";
			track->numLaps = 99;
		}
#endif
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World5\\map1.r3dMap";
			track->numLaps = 4;
		}
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World5\\map2.r3dMap";
			track->numLaps = 4;
		}
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World5\\map3.r3dMap";
			track->numLaps = 4;
		}
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World5\\map4.r3dMap";
			track->numLaps = 4;
		}
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World5\\map5.r3dMap";
			track->numLaps = 4;
		}
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World5\\map6.r3dMap";
			track->numLaps = 4;
		}
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World5\\map7.r3dMap";
			track->numLaps = 4;
		}
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World5\\map8.r3dMap";
			track->numLaps = 4;
		}

		//Player1
		{
			Planet::PlayerData data;
			data.id = Race::cComputer1;
			data.name = scTarquin;
			data.photo = _race->GetTexture("GUI\\Chars\\tarquin.png");			
			data.maxPass = 2;

			//first
			data.cars.push_back(Planet::CarData(_race->GetCar("monstertruckBoss"), 1));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("engine3"), 0, Player::stMotor, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("armor2"), 0, Player::stArmor, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("wheel2"), 0, Player::stWheel, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("truba2"), 0, Player::stTruba, 1));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("asyncFrost"), 4, Player::stWeapon1, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("torpedaWeapon"), 2, Player::stWeapon2, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("fireGun"), 5, Player::stWeapon3, 1));			
			data.slots.push_back(Planet::SlotData(_race->GetSlot("mine2"), 4, Player::stMine, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("hyperdrive"), 6, Player::stHyper, 1));

			//second
			data.cars.push_back(Planet::CarData(_race->GetCar("monstertruckBoss"), 2));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("engine3"), 0, Player::stMotor, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("armor2"), 0, Player::stArmor, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("wheel3"), 0, Player::stWheel, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("truba2"), 0, Player::stTruba, 2));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("asyncFrost"), 5, Player::stWeapon1, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("torpedaWeapon"), 2, Player::stWeapon2, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("torpedaWeapon"), 1, Player::stWeapon3, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("turel"), 6, Player::stWeapon4, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("mine2"), 4, Player::stMine, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("hyperdrive"), 7, Player::stHyper, 2));

			planet->InsertPlayer(data);
		}
		//Player2
		{
			Planet::PlayerData data;
			data.id = Race::cComputer2;
			data.name = scRip;
			data.photo = _race->GetTexture("GUI\\Chars\\rip.png");			
			data.maxPass = 2;

			//first
			data.cars.push_back(Planet::CarData(_race->GetCar("guseniza"), 1));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("engine3"), 0, Player::stMotor, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("armor2"), 0, Player::stArmor, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("gusWheel2"), 0, Player::stWheel, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("truba2"), 0, Player::stTruba, 1));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("tankLaser"), 5, Player::stWeapon1, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("rocketLauncher"), 7, Player::stWeapon2, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("mortira"), 2, Player::stWeapon3, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("mine2"), 3, Player::stMine, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("hyperdrive"), 5, Player::stHyper, 1));

			//second
			data.cars.push_back(Planet::CarData(_race->GetCar("podushka"), 2));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("engine3"), 0, Player::stMotor, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("armor2"), 0, Player::stArmor, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("truba2"), 0, Player::stTruba, 2));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("sphereGun"), 6, Player::stWeapon1, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("turel"), 3, Player::stWeapon2, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("rocketLauncher"), 7, Player::stWeapon3, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("mine3"), 4, Player::stMine, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("hyperdrive"), 6, Player::stHyper, 2));

			planet->InsertPlayer(data);
		}
		//Player3
		{
			Planet::PlayerData data;
			data.id = Race::cComputer3;
			data.name = scSnake;
			data.photo = _race->GetTexture("GUI\\Chars\\snake.png");			
			data.maxPass = 2;

			//first
			data.cars.push_back(Planet::CarData(_race->GetCar("manticora"), 1));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("engine3"), 0, Player::stMotor, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("armor3"), 0, Player::stArmor, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("wheel2"), 0, Player::stWheel, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("truba2"), 0, Player::stTruba, 1));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("blasterGun"), 7, Player::stWeapon1, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("drobilka"), 7, Player::stWeapon2, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("mine1"), 6, Player::stMine, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("hyperdrive"), 5, Player::stHyper, 1));

			//second
			data.cars.push_back(Planet::CarData(_race->GetCar("guseniza"), 2));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("engine3"), 0, Player::stMotor, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("armor2"), 0, Player::stArmor, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("gusWheel2"), 0, Player::stWheel, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("truba3"), 0, Player::stTruba, 2));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("tankLaser"), 7, Player::stWeapon1, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("drobilka"), 7, Player::stWeapon2, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("pulsator"), 3, Player::stWeapon3, 2));			
			data.slots.push_back(Planet::SlotData(_race->GetSlot("mine2"), 4, Player::stMine, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("hyperdrive"), 5, Player::stHyper, 2));

			planet->InsertPlayer(data);
		}		
	}

	{
		Planet* planet = AddPlanet();
		planet->SetName(_SC(scInferno));
		planet->SetInfo(_SC(scInfernoInfo));
		planet->SetWorldType(Planet::wtWorld4);

		Planet::Wheaters wheaters;
		wheaters.push_back(Planet::Wheater(Environment::ewHell, 65));
		//wheaters.push_back(Planet::Wheater(Environment::ewRainy, 25));
		//wheaters.push_back(Planet::Wheater(Environment::ewNight, 10));
		planet->SetWheaters(wheaters);

		Planet::RequestPoints points;
		points[1] = 2800;
		points[2] = 3200;
		planet->SetRequestPoints(points);

		Planet::Prices prices;
		prices.push_back(Planet::Price(10000, 400));
		prices.push_back(Planet::Price(7000, 200));
		prices.push_back(Planet::Price(4000, 100));
		planet->SetPrices(prices);

		planet->SetMesh(_race->GetMesh("GUI\\planet.r3d"));
		planet->SetTexture(_race->GetTexture("GUI\\inferno.dds"));

		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World4\\map1.r3dMap";
			track->numLaps = 4;
		}
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World4\\map2.r3dMap";
			track->numLaps = 4;
		}
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World4\\map3.r3dMap";
			track->numLaps = 4;
		}
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World4\\map4.r3dMap";
			track->numLaps = 4;
		}
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World4\\map5.r3dMap";
			track->numLaps = 4;
		}
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World4\\map6.r3dMap";
			track->numLaps = 4;
		}
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World4\\map7.r3dMap";
			track->numLaps = 4;
		}
		{
			Planet::Track* track = planet->AddTrack(1);
			track->level = "Data\\Map\\World4\\map8.r3dMap";
			track->numLaps = 4;
		}

		planet->InsertCar("devildriver", 0);
		planet->InsertCar("buggi", 3);
		planet->InsertCar("tankchetti", 3);		

		planet->InsertSlot("hyperBlaster", 0);		

		//Player1
		{
			Planet::PlayerData data;
			data.id = Race::cComputer1;
			data.name = scTarquin;
			data.photo = _race->GetTexture("GUI\\Chars\\tarquin.png");			
			data.maxPass = 2;

			//first
			data.cars.push_back(Planet::CarData(_race->GetCar("devildriverBoss"), 1));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("engine3"), 0, Player::stMotor, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("armor3"), 0, Player::stArmor, 1));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("sonar"), 5, Player::stWeapon1, 1));			
			data.slots.push_back(Planet::SlotData(_race->GetSlot("mine3"), 6, Player::stMine, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("hyperdrive"), 7, Player::stHyper, 1));

			//second
			data.cars.push_back(Planet::CarData(_race->GetCar("devildriverBoss"), 2));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("engine3"), 0, Player::stMotor, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("armor3"), 0, Player::stArmor, 2));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("sonar"), 7, Player::stWeapon1, 2));			
			data.slots.push_back(Planet::SlotData(_race->GetSlot("mine3"), 7, Player::stMine, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("hyperdrive"), 7, Player::stHyper, 2));

			planet->InsertPlayer(data);
		}
		//Player2
		{
			Planet::PlayerData data;
			data.id = Race::cComputer2;
			data.name = scRip;
			data.photo = _race->GetTexture("GUI\\Chars\\rip.png");			
			data.maxPass = 2;

			//first
			data.cars.push_back(Planet::CarData(_race->GetCar("podushka"), 1));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("engine3"), 0, Player::stMotor, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("armor2"), 0, Player::stArmor, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("truba3"), 0, Player::stTruba, 1));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("sphereGun"), 7, Player::stWeapon1, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("pulsator"), 3, Player::stWeapon2, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("rocketLauncher"), 7, Player::stWeapon3, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("mine3"), 6, Player::stMine, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("hyperdrive"), 7, Player::stHyper, 1));

			//second
			data.cars.push_back(Planet::CarData(_race->GetCar("monstertruck"), 2));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("engine3"), 0, Player::stMotor, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("armor3"), 0, Player::stArmor, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("wheel3"), 0, Player::stWheel, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("truba3"), 0, Player::stTruba, 2));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("asyncFrost"), 7, Player::stWeapon1, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("mortira"), 3, Player::stWeapon2, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("rocketLauncher"), 7, Player::stWeapon3, 2));			
			data.slots.push_back(Planet::SlotData(_race->GetSlot("mine2"), 7, Player::stMine, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("hyperdrive"), 7, Player::stHyper, 2));

			planet->InsertPlayer(data);
		}
		//Player3
		{
			Planet::PlayerData data;
			data.id = Race::cComputer3;
			data.name = scSnake;
			data.photo = _race->GetTexture("GUI\\Chars\\snake.png");			
			data.maxPass = 2;

			//first
			data.cars.push_back(Planet::CarData(_race->GetCar("guseniza"), 1));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("engine3"), 0, Player::stMotor, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("armor2"), 0, Player::stArmor, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("gusWheel3"), 0, Player::stWheel, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("truba3"), 0, Player::stTruba, 1));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("tankLaser"), 7, Player::stWeapon1, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("drobilka"), 7, Player::stWeapon2, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("turel"), 4, Player::stWeapon3, 1));			
			data.slots.push_back(Planet::SlotData(_race->GetSlot("mine2"), 5, Player::stMine, 1));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("hyperdrive"), 6, Player::stHyper, 1));

			//second
			data.cars.push_back(Planet::CarData(_race->GetCar("podushka"), 2));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("engine3"), 0, Player::stMotor, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("armor2"), 0, Player::stArmor, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("truba3"), 0, Player::stTruba, 2));
			//
			data.slots.push_back(Planet::SlotData(_race->GetSlot("sphereGun"), 7, Player::stWeapon1, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("fireGun"), 7, Player::stWeapon2, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("turel"), 7, Player::stWeapon3, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("mine3"), 7, Player::stMine, 2));
			data.slots.push_back(Planet::SlotData(_race->GetSlot("hyperdrive"), 7, Player::stHyper, 2));

			planet->InsertPlayer(data);
		}		
	}
}

void Tournament::LoadGamers()
{
	{
		Planet* planet = AddGamer();
		planet->SetName(_SC(scEarth));
		planet->SetInfo(_SC(scEarthInfo));
		planet->SetMesh(_race->GetMesh("GUI\\planet.r3d"));
		planet->SetTexture(_race->GetTexture("GUI\\earth.dds"));

		{
			Planet::PlayerData data;
			data.id = Race::cHuman;
			data.name = scTyler;
			data.photo = _race->GetTexture("GUI\\Chars\\tyler.png");
			planet->InsertPlayer(data);
		}
	}
}

void Tournament::LoadLib()
{
	ClearGamers();
	ClearPlanets();

	lsl::RootNode rootNode("tournametRoot", _race);

	try
	{		
		SerialFileXML xml;		
		xml.LoadNodeFromFile(rootNode, "tournamet.xml");

		rootNode.Load(this);
	}
	catch (const EUnableToOpen&)
	{
		LoadGamers();
		LoadPlanets();
		rootNode.Save(this);

		SerialFileXML xml;
		xml.SaveNodeToFile(rootNode, "tournamet.xml");
	}
}

void Tournament::Save(lsl::SWriter* writer)
{
	lsl::SWriter* planets = writer->NewDummyNode("planets");
	int i = 0;
	for (Planets::iterator iter = _planets.begin(); iter != _planets.end(); ++iter, ++i)
	{
		std::stringstream sstream;
		sstream << "planet" << i;
		lsl::SWriter* child = planets->NewDummyNode(sstream.str().c_str());		

		(*iter)->SaveTo(child, this);
	}

	lsl::SWriter* gamers = writer->NewDummyNode("gamers");
	i = 0;
	for (Planets::iterator iter = _gamers.begin(); iter != _gamers.end(); ++iter, ++i)
	{
		std::stringstream sstream;
		sstream << "gamer" << i;
		lsl::SWriter* child = gamers->NewDummyNode(sstream.str().c_str());		

		(*iter)->SaveTo(child, this);
	}
}

void Tournament::Load(lsl::SReader* reader)
{
	ClearGamers();
	ClearPlanets();

	lsl::SReader* planets = reader->ReadValue("planets");
	if (planets)
	{
		lsl::SReader* child = planets->FirstChildValue();
		while (child)
		{	
			Planet* planet = AddPlanet();
			planet->LoadFrom(child, this); 

			child = child->NextValue();
		}
	}

	lsl::SReader* gamers = reader->ReadValue("gamers");
	if (gamers)
	{
		lsl::SReader* child = gamers->FirstChildValue();
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
	Planet* planet = new Planet(_race, _planets.size());
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
	for (Planets::iterator iter = _planets.begin(); iter != _planets.end(); ++iter)
		if (*iter == planet)
			return iter;

	return _planets.end();
}

Planet* Tournament::FindPlanet(const std::string& name)
{
	for (Planets::iterator iter = _planets.begin(); iter != _planets.end(); ++iter)
		if ((*iter)->GetName() == name)
			return *iter;

	return 0;
}

Planet* Tournament::NextPlanet(Planet* planet)
{
	Planets::iterator iter = FindPlanet(planet);
	
	if (iter != _planets.end() && iter != --_planets.end())
		return *(++iter);

	return 0;
}

Planet* Tournament::PrevPlanet(Planet* planet)
{
	Planets::iterator iter = FindPlanet(planet);
	
	if (iter != _planets.begin())	
		return *(--iter);

	return 0;
}

Planet* Tournament::GetPlanet(int index)
{
	if (index >= 0 && static_cast<unsigned>(index) < _planets.size())
		return _planets[index];
	return 0;
}

Planet* Tournament::AddGamer()
{
	Planet* gamer = new Planet(_race, _gamers.size());
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
	for (Planets::iterator iter = _gamers.begin(); iter != _gamers.end(); ++iter)
		if (*iter == gamer)
			return iter;

	return _gamers.end();
}

Planet* Tournament::GetGamer(int gamerId)
{
	for (Planets::iterator iter = _gamers.begin(); iter != _gamers.end(); ++iter)
		if ((*iter)->GetId() == gamerId)
			return *iter;

	return NULL;
}

void Tournament::CompleteTrack(int points, bool& passComplete, bool& passChampion, bool& planetChampion)
{
	LSL_ASSERT(_curPlanet);

	passComplete = false;
	passChampion = false;
	planetChampion = false;

	Planet::Track* nextTrack = NextTrack(_curTrack);	

	if (nextTrack == NULL)
	{
		passComplete = true;

		if (_curPlanet->HasRequestPoints(_curPlanet->GetPass(), points))
		{
			_curPlanet->NextPass();

			passChampion = true;
			planetChampion = _curPlanet->GetState() == Planet::psCompleted;
		}

		nextTrack = NextTrack(NULL);
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
	if (Object::ReplaceRef(_curPlanet, value))
	{
		_trackList.clear();
		_curPlanet = value;
		
		if (_curPlanet)
		{
			SetCurTrack(NextTrack(NULL));
			_curPlanet->StartPass();
		}
		else
			SetCurTrack(0);
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
	if (Object::ReplaceRef(_curTrack, value))
	{
		_curTrack = value;
		if (_curPlanet)
		{
			_wheater = _curPlanet->GenerateWheater(_race->GetWorld()->GetEnv()->GetLightQuality() >= Environment::eqMiddle && !_wheaterNightPass, !_race->IsTutorialCompleted()).type;
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

	Planet::Track* nextTrack = NULL;

	if (_race->IsCampaign())
	{	
		if (track != NULL)
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
			for (Planet::TrackMap::const_iterator iter = _curPlanet->GetTrackMap().begin(); iter != _curPlanet->GetTrackMap().end(); ++iter)
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
		
		nextTrack = _trackList.front();
		_trackList.erase(_trackList.begin());
	}

	return nextTrack;
}

Environment::Wheater Tournament::GetWheater() const
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
	for (Planets::const_iterator iter = _gamers.begin(); iter != _gamers.end(); ++iter)
	{
		const Planet::PlayerData* plrData = (*iter)->GetPlayer(id);
		if (plrData)
			return plrData;
	}

	return _curPlanet ? _curPlanet->GetPlayer(id) : NULL;
}

const Planet::PlayerData* Tournament::GetPlayerData(const std::string& name) const
{
	for (Planets::const_iterator iter = _gamers.begin(); iter != _gamers.end(); ++iter)
	{
		const Planet::PlayerData* plrData = (*iter)->GetPlayer(name);
		if (plrData)
			return plrData;
	}

	return _curPlanet ? _curPlanet->GetPlayer(name) : NULL;
}

Player* Tournament::GetBossPlayer()
{
	return _race->GetPlayerById(Race::cComputer1);
}

void Tournament::Reset()
{
	for (Planets::const_iterator iter = _planets.begin(); iter != _planets.end(); ++iter)
		(*iter)->Reset();

	SetCurPlanet(0);
}

const Tournament::Planets& Tournament::GetPlanets() const
{
	return _planets;
}

const Tournament::Planets& Tournament::GetGamers() const
{
	return _gamers;
}




Race::Race(GameMode* game, const std::string& name): _game(game), _profile(0), _lastProfile(NULL), _lastNetProfile(NULL), _startRace(false), _goRace(false), _human(0), _aiSystem(0), _lastLeadPlace(0), _lastThirdPlace(0), _planetChampion(false), _passChampion(false), _carChanged(false), _minDifficulty(cDifficultyEnd), _tutorialStage(0), _springBorders(true), _enableMineBug(true)
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
}

Race::~Race()
{
	ExitRace();
	ExitProfile();

	SetLastProfile(0);
	SetLastNetProfile(NULL);
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

Race::Profile::Profile(Race* race, const std::string& name): _race(race), _name(name), _netGame(false), _difficulty(gdNormal)
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
	lsl::RootNode node("profile", _race);

	node.BeginSave();
	lsl::SWriter* writer = &node;

	SaveGame(writer);
	lsl::SWriteEnum(writer, "dfficulty", _difficulty, cDifficultyStr, cDifficultyEnd);

	node.EndSave();	

	lsl::SerialFileXML file;
	file.SaveNode(node, stream);
}

void Race::Profile::LoadGame(std::istream& stream)
{
	Reset();

	lsl::RootNode node("profile", _race);	
	lsl::SerialFileXML file;
	file.LoadNode(node, stream);

	node.BeginLoad();
	lsl::SReader* reader = &node;

	LoadGame(&node);
	lsl::SReadEnum(reader, "dfficulty", _difficulty, cDifficultyStr, cDifficultyEnd);

	node.EndLoad();
}

void Race::Profile::SaveGameFile()
{
	std::ostream* stream = FileSystem::GetInstance()->NewOutStream("Profile\\" + _name + ".xml", FileSystem::omText, 0);
	SaveGame(*stream);
	FileSystem::GetInstance()->FreeStream(stream);
}

void Race::Profile::LoadGameFile()
{
	std::istream* stream = NULL;
	try
	{
		try
		{
			stream = FileSystem::GetInstance()->NewInStream("Profile\\" + _name + ".xml", FileSystem::omText, 0);
			LoadGame(*stream);
		}
		catch (const lsl::EUnableToOpen&) {}
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

void Race::SnProfile::SaveWorkshop(lsl::SWriter* writer)
{
	Workshop& shop = _race->GetWorkshop();

	lsl::SWriter* slots = writer->NewDummyNode("slots");
	int i = 0;
	for (Workshop::Items::const_iterator iter = shop.GetItems().begin(); iter != shop.GetItems().end(); ++iter, ++i)
	{
		std::stringstream stream;
		stream << "slot" << i;

		RecordLib::SaveRecordRef(slots, stream.str(), (*iter)->GetRecord());		
	}
}

void Race::SnProfile::LoadWorkshop(lsl::SReader* reader)
{
	Workshop& shop = _race->GetWorkshop();
	shop.ClearItems();

	lsl::SReader* slots = reader->ReadValue("slots");	
	lsl::SReader* slot = slots ? slots->FirstChildValue() : 0;
	while (slot)
	{
		Record* record = RecordLib::LoadRecordRefFrom(slot);		
		if (Slot* item = shop.FindSlot(record))
			shop.InsertItem(item);

		slot = slot->NextValue();
	}
}

void Race::SnProfile::SaveGarage(lsl::SWriter* writer)
{
	Garage& shop = _race->GetGarage();

	lsl::SWriter* cars = writer->NewDummyNode("cars");
	int i = 0;
	for (Garage::Items::const_iterator iter = shop.GetItems().begin(); iter != shop.GetItems().end(); ++iter, ++i)
	{
		MapObjLib::SaveRecordRef(cars, lsl::StrFmt("car%d", i).c_str(), (*iter)->GetRecord());
	}
}

void Race::SnProfile::LoadGarage(lsl::SReader* reader)
{
	Garage& shop = _race->GetGarage();
	shop.ClearItems();

	lsl::SReader* cars = reader->ReadValue("cars");	
	lsl::SReader* car = cars ? cars->FirstChildValue() : 0;
	while (car)
	{
		MapObjRec* record = MapObjLib::LoadRecordRefFrom(car);		
		if (Garage::Car* item = shop.FindCar(record))
			shop.InsertItem(item);

		car = car->NextValue();
	}
}

void Race::SnProfile::SaveTournament(lsl::SWriter* writer)
{
	Tournament& tournament = _race->GetTournament();

	for (unsigned i = 0; i < tournament.GetPlanets().size(); ++i)
	{
		lsl::SWriter* planet = writer->NewDummyNode(lsl::StrFmt("planet%d", i).c_str());
		planet->WriteValue("state", tournament.GetPlanets()[i]->GetState());
		planet->WriteValue("pass", tournament.GetPlanets()[i]->GetPass());
	}

	writer->WriteValue("planet", tournament.GetCurPlanetIndex());
	writer->WriteValue("track", tournament.GetCurTrackIndex());
}

void Race::SnProfile::LoadTournament(lsl::SReader* reader)
{
	Tournament& tournament = _race->GetTournament();
	int ind;

	for (unsigned i = 0; i < tournament.GetPlanets().size(); ++i)
	{
		int v;
		lsl::SReader* planet = reader->ReadValue(lsl::StrFmt("planet%d", i).c_str());
		if (planet)
		{
			if (planet->ReadValue("state", v))
				tournament.GetPlanets()[i]->SetState(Planet::State(v));
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

void Race::SnProfile::SavePlayer(Player* player, lsl::SWriter* writer)
{
	MapObjLib::SaveRecordRef(writer, "car", player->GetCar().record);
	writer->WriteValue("plrId", player->GetId());
	writer->WriteValue("gamerId", player->GetGamerId());
	writer->WriteValue("netSlot", player->GetNetSlot());
	lsl::SWriteValue(writer, "color", player->GetColor());
	writer->WriteValue("money", player->GetMoney());
	writer->WriteValue("points", player->GetPoints());

	for (int i = 0; i < Player::cSlotTypeEnd; ++i)
	{
		Slot* slot = player->GetSlotInst(Player::SlotType(i));
		WeaponItem* wpn = slot ? slot->GetItem().IsWeaponItem() : 0;

		lsl::SWriter* node = RecordLib::SaveRecordRef(writer, lsl::StrFmt("slot%d", i).c_str(), slot ? slot->GetRecord() : 0);
		if (wpn && node)
		{
			node->WriteAttr("charge", wpn->GetCntCharge());
		}
	}
}

void Race::SnProfile::LoadPlayer(Player* player, lsl::SReader* reader)
{
	lsl::string str;
	lsl::Serializable::FixUpName fixUp;
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

	if (lsl::SReadValue(reader, "color", color))
		player->SetColor(color);
	if (reader->ReadValue("money", numb))
		player->SetMoney(numb);
	if (reader->ReadValue("points", numb))
		player->SetPoints(numb);

	for (int i = 0; i < Player::cSlotTypeEnd; ++i)
	{
		lsl::SReader* node = reader->ReadValue(lsl::StrFmt("slot%d", i).c_str());
		if (node)
		{
			Record* slotRef = RecordLib::LoadRecordRefFrom(node);		
			Slot* slot = _race->GetWorkshop().FindSlot(slotRef);
			const lsl::SReader::ValueDesc* desc = node ? node->ReadAttr("charge") : 0;			
			
			if (car && slot)
			{
				Slot* inst = _race->GetGarage().InstalSlot(player, Player::SlotType(i), car, slot);
				
				WeaponItem* wpn = inst ? inst->GetItem().IsWeaponItem() : 0;
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

void Race::SnProfile::SaveHumans(lsl::SWriter* writer)
{
	lsl::SWriter* humans = writer->NewDummyNode("humans");

	int i = 0;	
	for (PlayerList::const_iterator iter = _race->GetPlayerList().begin(); iter != _race->GetPlayerList().end(); ++iter, ++i)
		if ((*iter)->IsHuman() || (*iter)->IsOpponent())
		{
			lsl::SWriter* child = humans->NewDummyNode(lsl::StrFmt("human%d", i).c_str());
			SavePlayer(*iter, child);
		}
}

void Race::SnProfile::LoadHumans(lsl::SReader* reader)
{
	lsl::SReader* humans = reader->ReadValue("humans");

	lsl::SReader* player = humans ? humans->FirstChildValue() : 0;
	int i = 0;
	while (player)
	{
		Player* item = _race->AddPlayer((i + 1) << cOpponentBit);
		LoadPlayer(item, player);

		player = player->NextValue();
		++i;
	}
}

void Race::SnProfile::SaveAIPlayers(lsl::SWriter* writer)
{
	lsl::SWriter* ai = writer->NewDummyNode("ai");

	int i = 0;
	for (AIPlayers::const_iterator iter = _race->GetAIPlayers().begin(); iter != _race->GetAIPlayers().end(); ++iter, ++i)
	{
		if (!(_race->GetHuman() && _race->GetHuman()->GetPlayer() == (*iter)->GetPlayer()))
		{
			lsl::SWriter* child = ai->NewDummyNode(lsl::StrFmt("player%d", i).c_str());
		}
	}
}

void Race::SnProfile::LoadAIPlayers(lsl::SReader* reader)
{
	lsl::SReader* ai = reader->ReadValue("ai");

	lsl::SReader* player = ai ? ai->FirstChildValue() : 0;
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

void Race::SnProfile::SaveGame(lsl::SWriter* writer)
{
	writer->WriteValue("carChanged", _race->_carChanged);
	writer->WriteValue("minDifficulty", _race->_minDifficulty);

	SaveTournament(writer);
	//SaveWorkshop(writer);
	//SaveGarage(writer);

	SaveHumans(writer);
	//SaveAIPlayers(writer);	
}

void Race::SnProfile::LoadGame(lsl::SReader* reader)
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
	for (Planets::const_iterator iter = _race->GetPlanetsCompleted().begin(); iter != _race->GetPlanetsCompleted().end(); ++iter)
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

void Race::SkProfile::SaveGame(lsl::SWriter* writer)
{
	//Tournament& tournament = _race->GetTournament();
	//
	//writer->WriteValue("planet", tournament.GetCurPlanetIndex());
	//writer->WriteValue("track", tournament.GetCurTrackIndex());
}

void Race::SkProfile::LoadGame(lsl::SReader* reader)
{
	EnterGame();

	//Tournament& tournament = _race->GetTournament();
	//int ind;
	//
	//if (reader->ReadValue("planet", ind))
	//{
	//	Planet* planet = tournament.GetPlanets()[ind];
	//	tournament.SetCurPlanet(planet);
	//
	//	if (planet && reader->ReadValue("track", ind))
	//		tournament.SetCurTrack(planet->GetTracks()[ind]);
	//}
}

void Race::DisposePlayer(Player* player)
{
	if (GetWorld()->GetCamera()->GetPlayer() == player)
		GetWorld()->GetCamera()->SetPlayer(0);
	
	SendEvent(cPlayerDispose, &EventData(player->GetId()));

	player->Release();
	delete player;
}

void Race::CompleteRace(Player* player)
{	
	//защита от двойного завершения трасы на всякий случай
	if (GetResult(player->GetId()) == NULL)
	{
		Result result;
		result.playerId = player->GetId();	
		result.place = !_results.empty() ? _results.back().place + 1 : 1;
		result.voiceNameDur = 1.5f;

		Planet::Price price = _tournament->GetCurPlanet().GetPrice(result.place);
		result.money = price.money;
		result.points = price.points;
		result.pickMoney = player->GetPickMoney();

		_results.push_back(result);

		player->SetFinished(true);
		player->SetPlace(result.place);
		player->ResetPickMoney();

		AIPlayer* aiPlayer = FindAIPlayer(player);
		if (aiPlayer)
			aiPlayer->FreeCar();

		player->SetBlockTime(0.3f);
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

	typedef lsl::List<MyPlayer> MyPlayers;

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

	for (PlayerList::iterator iter = _playerList.begin(); iter != _playerList.end(); ++iter)
	{
		Player* player = *iter;
		const Result* result = GetResult((*iter)->GetId());

		if (result == NULL)
		{
			MyPlayer plrRes;
			plrRes.inst = player;

			//человек который не завершил трасу всегда на последнем месте
			if (player->IsHuman() || player->IsOpponent())
				plrRes.placePos = -(int)_tournament->GetCurTrack().GetLapsCount() - 1.0f + player->GetCar().GetLap();
			else
				plrRes.placePos = player->GetCar().GetLap();

			players.push_back(plrRes);
		}
	}

	players.sort();

	//принудительно завершаем гонку для сотавшихся игроков
	for (MyPlayers::iterator iter = players.begin(); iter != players.end(); ++iter)	
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
			}
		}
	}

	bool passComplete;
	_tournament->CompleteTrack(GetTotalPoints(), passComplete, _passChampion, _planetChampion);

	if (passComplete)
	{
		for (PlayerList::iterator iter = _playerList.begin(); iter != _playerList.end(); ++iter)
		{
			Player* player = *iter;
			player->SetPoints(0);
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

	if (index == cTournamentPlanetCount - 1)
	{
		for (unsigned i = cTournamentPlanetCount; i < _tournament->GetPlanets().size(); ++i)
		{
			if (!_planetsCompleted.IsFind(i))
				_planetsCompleted.push_back(i);
		}
	}
}

void Race::SaveGame(lsl::SWriter* writer)
{
	lsl::StringVec vec;
	lsl::string str;

	vec.clear();
	str.clear();
	for (Profiles::const_iterator iter = _profiles.begin(); iter != _profiles.end(); ++iter)
		if (!(*iter)->netGame())
			vec.push_back((*iter)->GetName());
	lsl::StrLinkValues(vec, str);
	writer->WriteValue("profiles", str);

	vec.clear();
	str.clear();
	for (Profiles::const_iterator iter = _profiles.begin(); iter != _profiles.end(); ++iter)
		if ((*iter)->netGame())
			vec.push_back((*iter)->GetName());
	lsl::StrLinkValues(vec, str);
	writer->WriteValue("netProfiles", str);

	if (_lastProfile)
		writer->WriteValue("lastProfile", _lastProfile->GetName());
	if (_lastNetProfile)
		writer->WriteValue("lastNetProfile", _lastNetProfile->GetName());

	vec.clear();
	str.clear();
	for (Planets::const_iterator iter = _planetsCompleted.begin(); iter != _planetsCompleted.end(); ++iter)
		vec.push_back(lsl::StrFmt("%d", *iter));
	lsl::StrLinkValues(vec, str);
	writer->WriteValue("planetsCompleted", str);

	writer->WriteValue("tutorialStage", _tutorialStage);
}

void Race::LoadGame(lsl::SReader* reader)
{
	lsl::StringVec vec;
	lsl::string str;	

	ClearProfiles();
	_planetsCompleted.clear();

	vec.clear();
	str.clear();
	reader->ReadValue("profiles", str);
	lsl::StrExtractValues(str, vec);
	for (lsl::StringVec::const_iterator iter = vec.begin(); iter != vec.end(); ++iter)
		AddProfile(*iter)->netGame(false);

	vec.clear();
	str.clear();
	reader->ReadValue("netProfiles", str);
	lsl::StrExtractValues(str, vec);
	for (lsl::StringVec::const_iterator iter = vec.begin(); iter != vec.end(); ++iter)
		AddProfile(*iter)->netGame(true);

	if (reader->ReadValue("lastProfile", str))
	{
		Profiles::const_iterator iter = FindProfile(str);
		SetLastProfile(iter != _profiles.end() ? *iter : 0);
	}

	if (reader->ReadValue("lastNetProfile", str))
	{
		Profiles::const_iterator iter = FindProfile(str);
		SetLastNetProfile(iter != _profiles.end() ? *iter : 0);
	}

	vec.clear();
	str.clear();
	reader->ReadValue("planetsCompleted", str);
	lsl::StrExtractValues(str, vec);
	for (lsl::StringVec::const_iterator iter = vec.begin(); iter != vec.end(); ++iter)
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
		lsl::RootNode rootNode("raceRoot", _game->GetWorld());

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

	if (_aiSystem && _goRace)
		_aiSystem->OnProgress(deltaTime);
}

void Race::OnLateProgress(float deltaTime, bool pxStep)
{
	Player* lastLeader = !_playerPlaceList.empty() ? _playerPlaceList.front(): NULL;
	Player* lastThird = _playerPlaceList.size() >= 3 ? _playerPlaceList[2] : NULL;
	_playerPlaceList = _playerList;

	struct Pred 
	{
		bool operator()(const Player* p1, const Player* p2) const 
		{
			if (p1->GetFinished() && p2->GetFinished())
				return p1->GetPlace() < p2->GetPlace();
			else if (p1->GetFinished() && !p2->GetFinished())
				return true;
			else if (!p1->GetFinished() && p2->GetFinished())
				return false;
			else
				return p1->GetCar().GetLap() > p2->GetCar().GetLap();
		}
	};

	std::sort(_playerPlaceList.begin(), _playerPlaceList.end(), Pred());

	for (unsigned i = 0; i < _playerPlaceList.size(); ++i)
		_playerPlaceList[i]->SetPlace(i + 1);

	Player* leaderPlayer = !_playerPlaceList.empty() ? _playerPlaceList.front(): NULL;	
	Player* secondPlayer = _playerPlaceList.size() >= 2 ? _playerPlaceList[1] : NULL;
	Player* thirdPlayer = _playerPlaceList.size() >= 3 ? _playerPlaceList[2] : NULL;
	Player* nextLastPlayer = _playerPlaceList.size() >= 2 ? _playerPlaceList[_playerPlaceList.size() - 2] : NULL;
	Player* lastPlayer = _playerPlaceList.size() >= 2 ? _playerPlaceList.back() : NULL;

	if (leaderPlayer && lastLeader && leaderPlayer != lastLeader && leaderPlayer->GetCar().IsMainPath() && lastLeader->GetCar().IsMainPath())
	{
		float newLeadPlace = leaderPlayer->GetCar().GetLap(true);
		//проверяем также на пустой результат так как речь идет о лидере гонки
		if (leaderPlayer->GetCar().GetPathLength(true) * (newLeadPlace - _lastLeadPlace) > 300.0f && _results.empty())
		{
			SendEvent(cPlayerLeadChanged, &EventData(leaderPlayer->GetId()));
		}
		_lastLeadPlace = newLeadPlace;
	}

	if (thirdPlayer && lastThird && thirdPlayer != lastThird && thirdPlayer->GetCar().IsMainPath() && lastThird->GetCar().IsMainPath())
	{
		float newPlace = thirdPlayer->GetCar().GetLap(true);
		//проверяем также на пустой результат так как речь идет о лидере гонки
		if (thirdPlayer->GetCar().GetPathLength(true) * (newPlace - _lastThirdPlace) > 300.0f && _results.empty())
		{
			SendEvent(cPlayerThirdChanged, &EventData(thirdPlayer->GetId()));
		}
		_lastThirdPlace = newPlace;
	}

	if (lastPlayer && nextLastPlayer && lastPlayer->GetCar().IsMainPath() && nextLastPlayer->GetCar().IsMainPath() && lastPlayer->GetCar().GetPathLength(true) * (nextLastPlayer->GetCar().GetLap(true) - lastPlayer->GetCar().GetLap(true)) > 70.0f)
	{
		SendEvent(cPlayerLastFar, &EventData(lastPlayer->GetId()));
	}

	if (leaderPlayer && secondPlayer && leaderPlayer->GetCar().IsMainPath() && secondPlayer->GetCar().IsMainPath() && leaderPlayer->GetCar().GetPathLength(true) * (leaderPlayer->GetCar().GetLap(true) - secondPlayer->GetCar().GetLap(true)) > 70.0f)
	{
		if (_results.empty())
			SendEvent(cPlayerDomination, &EventData(leaderPlayer->GetId()));
	}

	if (secondPlayer && thirdPlayer && secondPlayer->GetCar().IsMainPath() && thirdPlayer->GetCar().IsMainPath() && thirdPlayer->GetCar().GetPathLength(true) * (secondPlayer->GetCar().GetLap(true) - thirdPlayer->GetCar().GetLap(true)) > 70.0f)
	{
		if (_results.empty())
			SendEvent(cPlayerThirdFar, &EventData(thirdPlayer->GetId()));
	}
}

Player* Race::AddPlayer(int plrId)
{
	const D3DXCOLOR color[cMaxPlayers] = {D3DXCOLOR(0xFF5B29A5), D3DXCOLOR(0xFF9E9E9E), D3DXCOLOR(0xFFFF80C0), D3DXCOLOR(0xFF83F7CC), D3DXCOLOR(0xFF83E500), D3DXCOLOR(0xFFD8E585), D3DXCOLOR(0xFF6100B9), D3DXCOLOR(0xFF006CA4)};

	Player* player = new Player(this);
	player->AddRef();	
	player->SetId(plrId);
	_playerList.push_back(player);

	_tournament->GetCurPlanet().StartPass(player);

	if (player->IsHuman() || player->IsOpponent())
		player->SetMoney(20000);
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
	for (PlayerList::iterator iter = _playerList.begin(); iter != _playerList.end(); ++iter)
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
	for (AIPlayers::iterator iter = _aiPlayers.begin(); iter != _aiPlayers.end(); ++iter)
		if ((*iter)->GetPlayer() == player)
			return *iter;

	return 0;
}

HumanPlayer* Race::CreateHuman(Player* player)
{
	FreeHuman();

	_human = new HumanPlayer(player);
	_human->AddRef();	

	return _human;
}

void Race::FreeHuman()
{
	if (_human)
	{
		_human->Release();
		delete _human;
		_human = 0;
	}
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

Player* Race::GetPlayerByMapObj(MapObj* mapObj)
{
	for (PlayerList::const_iterator iter = _playerList.begin(); iter != _playerList.end(); ++iter)
		if ((*iter)->GetCar().mapObj == mapObj)
			return *iter;
	return NULL;
}

Player* Race::GetPlayerById(int id) const
{
	if (id == cUndefPlayerId)
		return NULL;

	for (PlayerList::const_iterator iter = _playerList.begin(); iter != _playerList.end(); ++iter)
		if ((*iter)->GetId() == id)
			return *iter;

	return NULL;
}

Player* Race::GetPlayerByNetSlot(unsigned netSlot) const
{
	for (PlayerList::const_iterator iter = _playerList.begin(); iter != _playerList.end(); ++iter)
		if ((*iter)->GetNetSlot() == netSlot)
			return *iter;

	return NULL;
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
	Profiles::const_iterator iter = FindProfile(name);
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
		SetLastProfile(0);
	if (_lastNetProfile == profile)
		SetLastNetProfile(NULL);

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
	for (Profiles::const_iterator iter = _profiles.begin(); iter != _profiles.end(); ++iter)
		if (*iter == profile)
			return iter;

	return _profiles.end();
}

Race::Profiles::const_iterator Race::FindProfile(const std::string& name) const
{
	for (Profiles::const_iterator iter = _profiles.begin(); iter != _profiles.end(); ++iter)
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
		_profile = 0;

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

			Race::Profile* profile = AddProfile(name);
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
	return _profile != NULL;
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
	lsl::RootNode rootNode("raceRoot", _game->GetWorld());

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
	else
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

		Race::PlayerList::const_iterator iter = _playerList.Find(_human->GetPlayer());
		if (iter == _playerList.end() || (++iter) == _playerList.end())
			iter = _playerList.begin();
		Player* player = *iter;

		CreateHuman(player);

		AIPlayer* aiPlayer = _aiSystem->FindAIPlayer(player);
		if (aiPlayer)
			_aiSystem->CreateDebug(aiPlayer);
	}
}

void Race::ResetCarPos()
{
	const unsigned cRowLength = 4;
	const float rowSpace = 7.0f;
	
	D3DXVECTOR3 stPos = ZVector * 7.0f;
	D3DXVECTOR3 dirVec = XVector;
	D3DXVECTOR3 lineVec = YVector;
	float nodeWidth = 10.0f;

	if (!GetMap()->GetTrace().GetPoints().empty())
	{
		WayPoint* point = GetMap()->GetTrace().GetPoints().front();
		WayNode* node = !point->GetNodes().empty() ? point->GetNodes().front() : 0;

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

	for (unsigned i = 0; i < _playerList.size(); ++i)
	{
		if ((i % cRowLength) == 0)
		{
			unsigned count = std::min(_playerList.size() - i, 4U);
			plSize = 0.0f;
			stepY = 0;

			for (unsigned j = i; j < i + count; ++j)
			{
				Player* player = _playerList[j];

				MapObj* mapObj = player->GetCar().mapObj;
				GameObject* gameObj = player->GetCar().gameObj;
				if (mapObj)
					plSize += gameObj->GetGrActor().GetLocalAABB(true).GetSizes().y;
			}

			spaceY = std::max((nodeWidth - plSize) / (count + 1.0f), 0.0f);			
		}

		Player* player = _playerList[i];
		MapObj* mapObj = player->GetCar().mapObj;
		GameObject* gameObj = player->GetCar().gameObj;	

		if (mapObj)
		{
			AABB aabb = gameObj->GetGrActor().GetLocalAABB(true);
			D3DXVECTOR3 size = aabb.GetSizes();			

			//dirVec начинается с небольшого порога чтобы машина не оказалась на последнем тайле
			gameObj->SetWorldPos(stPos + dirVec * (0.1f - i / 4 * rowSpace) + lineVec * (-nodeWidth / 2.0f + spaceY + size.y/2.0f + stepY));
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
}

void Race::StartRace()
{
	const Environment::WorldType envWorldType[Planet::cWorldTypeEnd] = {Environment::wtWorld1, Environment::wtWorld2, Environment::wtWorld3, Environment::wtWorld4, Environment::wtWorld5, Environment::wtWorld6};
	const CameraManager::Style camStyle[GameMode::cPrefCameraEnd] = {CameraManager::csThirdPerson, CameraManager::csIsometric};

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

		for (PlayerList::iterator iter = _playerList.begin(); iter != _playerList.end(); ++iter)
		{
			(*iter)->CreateCar(true);
			(*iter)->SetHeadlight(wheater == Environment::ewNight ? ((*iter)->IsHuman() ? Player::hlmTwo : Player::hlmOne) : Player::hlmNone);
			(*iter)->ReloadWeapons();
			(*iter)->SetFinished(false);
			(*iter)->ResetBlock(false);
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

			for (PlayerList::iterator iter = _playerList.begin(); iter != _playerList.end(); ++iter)
			{
				if (plr->GetPlayer()->IsComputer() && *iter != plr->GetPlayer() && (*iter)->GetGamerId() == gamerId)
				{
					for (Tournament::Planets::const_iterator iter2 = _tournament->GetGamers().begin(); iter2 != _tournament->GetGamers().end(); ++iter2)
					{
						bool isFind = false;

						for (PlayerList::iterator iter3 = _playerList.begin(); iter3 != _playerList.end(); ++iter3)
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
			plr->CreateCar();
		}

		ResetCarPos();

		GetWorld()->GetCamera()->ChangeStyle(_tournament->GetCurPlanetIndex() < cTournamentPlanetCount ? camStyle[_game->GetPrefCamera()] : CameraManager::csThirdPerson);
		GetWorld()->GetCamera()->SetPlayer(_human ? _human->GetPlayer() : NULL);

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

		for (AIPlayers::iterator iter = _aiPlayers.begin(); iter != _aiPlayers.end(); ++iter)
		{
			(*iter)->FreeCar();
		}
		for (PlayerList::iterator iter = _playerList.begin(); iter != _playerList.end(); ++iter)
		{
			(*iter)->SetHeadlight(Player::hlmNone);
			(*iter)->FreeCar(true);
		}

		_achievment->ResetRaceState();

		GetWorld()->GetCamera()->SetPlayer(NULL);
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
		_human->GetPlayer()->ResetBlock(false);
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

bool Race::GetSpringBorders() const
{
	return _springBorders;
}

void Race::SetSpringBorders(bool value)
{
	_springBorders = value;
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

	if (player->GetCar().numLaps >= _tournament->GetCurTrack().GetLapsCount())
	{
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
		bool isRaceComplete = isHuman || (_results.size() >= _playerList.size() && GetPlayerById(cHuman) == NULL);
		if (isRaceComplete)
			SendEvent(cRaceFinish, NULL);
	}

	if (isHuman)
		SendEvent(cRacePassLap, &EventData(Race::cHuman));

	if (isHuman && player->GetCar().numLaps == _tournament->GetCurTrack().GetLapsCount() - 1)
	{
		Player* leader = player;

		for (PlayerList::const_iterator iter = _playerList.begin(); iter != _playerList.end(); ++iter)
			if ((*iter)->GetPlace() == 1)
			{
				leader = *iter;
				break;
			}

		SendEvent(cRaceLastLap, &EventData(leader->GetId()));
	}
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
	for (Results::const_iterator iter = _results.begin(); iter != _results.end(); ++iter)
	{
		if (iter->playerId == playerId)
			return &(*iter);
	}

	return NULL;
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

	for (PlayerList::const_iterator iter = _playerList.begin(); iter != _playerList.end(); ++iter)
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