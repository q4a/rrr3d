#include "stdafx.h"
#include "game\World.h"

#include "game\Player.h"

namespace r3d
{

namespace game
{

const float Player::cHumanEasingMinDist[cDifficultyEnd] = {20.0f, 20.0f, 20.0f};
const float Player::cHumanEasingMaxDist[cDifficultyEnd] = {200.0f, 200.0f, 200.0f};
const float Player::cHumanEasingMinSpeed[cDifficultyEnd] = {95.0f * 1000/3600, 110.0f * 1000/3600, 125.0f * 1000/3600};
const float Player::cHumanEasingMaxSpeed[cDifficultyEnd] = {55.0f * 1000/3600, 65.0f * 1000/3600, 75.0f * 1000/3600};
const float Player::cCompCheatMinTorqueK[cDifficultyEnd] = {1.05f, 1.20f, 1.30f};
const float Player::cCompCheatMaxTorqueK[cDifficultyEnd] = {1.30f, 1.65f, 1.85f};

const float Player::cHumanArmorK[cDifficultyEnd] = {2.0f, 1.75f, 1.5f};

const std::string Player::cSlotTypeStr[cSlotTypeEnd] = {"stWheel", "stTruba", "stArmor", "stMotor", "stHyper", "stMine", "stWeapon1", "stWeapon2", "stWeapon3", "stWeapon4"};

const D3DXCOLOR Player::cLeftColors[Player::cColorsCount] = {clrWhite, clrBlue, clrRed, clrGreen, clrYellow, D3DXCOLOR(0xffff9000), clrGray20};

const D3DXCOLOR Player::cRightColors[Player::cColorsCount] = {D3DXCOLOR(0xff06affa), D3DXCOLOR(0xffb70bae), D3DXCOLOR(0xffb1c903), D3DXCOLOR(0xffa93900), D3DXCOLOR(0xff30373d), D3DXCOLOR(0xff009f15), D3DXCOLOR(0xff70769c)};

Slot::ClassList Slot::classList; 




SlotItem::SlotItem(Slot* slot): _name(_SC(svNull)), _info(_SC(svNull)), _slot(slot), _cost(0), _mesh(0), _texture(0), _pos(NullVector), _rot(NullQuaternion)
{
}

SlotItem::~SlotItem()
{
	SetMesh(0);
	SetTexture(0);
}

void SlotItem::RegProgressEvent()
{	
	if (GetPlayer())
		GetPlayer()->GetRace()->GetGame()->RegProgressEvent(this);
}

void SlotItem::UnregProgressEvent()
{
	if (GetPlayer())
		GetPlayer()->GetRace()->GetGame()->UnregProgressEvent(this);
}

void SlotItem::Save(SWriter* writer)
{
	writer->WriteValue("name", _name);
	writer->WriteValue("info", _info);
	writer->WriteValue("cost", _cost);
	writer->WriteRef("mesh", _mesh);
	writer->WriteRef("texture", _texture);

	lsl::SWriteValue(writer, "pos", _pos);
	lsl::SWriteValue(writer, "rot", _rot);
}

void SlotItem::Load(SReader* reader)
{
	reader->ReadValue("name", _name);
	reader->ReadValue("info", _info);
	reader->ReadValue("cost", _cost);	
	reader->ReadRef("mesh", true, this, 0);
	reader->ReadRef("texture", true, this, 0);

	lsl::SReadValue(reader, "pos", _pos);
	lsl::SReadValue(reader, "rot", _rot);
}

void SlotItem::OnFixUp(const FixUpNames& fixUpNames)
{
	for (FixUpNames::const_iterator iter = fixUpNames.begin(); iter != fixUpNames.end(); ++iter)
	{
		if (iter->name == "mesh")	
			SetMesh(static_cast<graph::IndexedVBMesh*>(iter->collItem));

		if (iter->name == "texture")	
			SetTexture(static_cast<graph::Tex2DResource*>(iter->collItem));
	}
}

void SlotItem::TransformChanged()
{
	//Nothing
}

const std::string& SlotItem::DoGetName() const
{
	return _name;
}

const std::string& SlotItem::DoGetInfo() const
{
	return _info;
}

int SlotItem::DoGetCost() const
{
	return _cost;
}

graph::IndexedVBMesh* SlotItem::DoGetMesh() const
{
	return _mesh;
}

graph::Tex2DResource* SlotItem::DoGetTexture() const
{
	return _texture;
}

WeaponItem* SlotItem::IsWeaponItem()
{
	return 0;
}

MobilityItem* SlotItem::IsMobilityItem()
{
	return 0;
}

Slot* SlotItem::GetSlot() const
{
	return _slot;
}

Player* SlotItem::GetPlayer() const
{
	return _slot ? _slot->GetPlayer() : NULL; 
}

const std::string& SlotItem::GetName() const
{
	return DoGetName();
}

void SlotItem::SetName(const std::string& value)
{
	_name = value;
}

const std::string& SlotItem::GetInfo() const
{
	return DoGetInfo();
}

void SlotItem::SetInfo(const std::string& value)
{
	_info = value;
}

int SlotItem::GetCost() const
{
	return DoGetCost();
}

void SlotItem::SetCost(int value)
{
	_cost = value;
}

graph::IndexedVBMesh* SlotItem::GetMesh() const
{
	return DoGetMesh();
}

void SlotItem::SetMesh(graph::IndexedVBMesh* value)
{
	if (Object::ReplaceRef(_mesh, value))
		_mesh = value;
}

graph::Tex2DResource* SlotItem::GetTexture() const
{
	return DoGetTexture();
}

void SlotItem::SetTexture(graph::Tex2DResource* value)
{
	if (Object::ReplaceRef(_texture, value))
		_texture = value;
}

const D3DXVECTOR3& SlotItem::GetPos() const
{
	return _pos;
}

void SlotItem::SetPos(const D3DXVECTOR3& value)
{
	_pos = value;
	TransformChanged();
}

const D3DXQUATERNION& SlotItem::GetRot() const
{
	return _rot;
}

void SlotItem::SetRot(const D3DXQUATERNION& value)
{
	_rot = value;
	TransformChanged();
}




MobilityItem::MobilityItem(Slot* slot): _MyBase(slot)
{
}

MobilityItem::CarFunc::CarFunc(): maxTorque(0.0f), life(0.0f), maxSpeed(0), tireSpring(0)
{
}

void MobilityItem::CarFunc::WriteTo(lsl::SWriter* writer)
{
	lsl::SWriter* wLongTire = writer->NewDummyNode("longTire");
	longTire.WriteTo(wLongTire);

	lsl::SWriter* wLatTire = writer->NewDummyNode("latTire");
	latTire.WriteTo(wLatTire);

	writer->WriteValue("maxTorque", maxTorque);	
	writer->WriteValue("life", life);
	writer->WriteValue("maxSpeed", maxSpeed);
	writer->WriteValue("tireSpring", tireSpring);
}

void MobilityItem::CarFunc::ReadFrom(lsl::SReader* reader)
{
	lsl::SReader* rLongTire = reader->ReadValue("longTire");
	if (rLongTire)
		longTire.ReadFrom(rLongTire);

	lsl::SReader* rLatTire = reader->ReadValue("latTire");
	if (rLatTire)
		latTire.ReadFrom(rLatTire);

	reader->ReadValue("maxTorque", maxTorque);
	reader->ReadValue("life", life);
	reader->ReadValue("maxSpeed", maxSpeed);
	reader->ReadValue("tireSpring", tireSpring);
}

void MobilityItem::Save(SWriter* writer)
{
	_MyBase::Save(writer);

	lsl::SWriter* funcMap = writer->NewDummyNode("carFuncMap");	
	int i = 0;

	for (CarFuncMap::iterator iter = carFuncMap.begin(); iter != carFuncMap.end(); ++iter, ++i)
	{
		std::stringstream sstream;
		sstream << "func" << i;
		lsl::SWriter* func = funcMap->NewDummyNode(sstream.str().c_str());

		MapObjLib::SaveRecordRef(func, "car", iter->first);
		iter->second.WriteTo(func);
	}
}

void MobilityItem::Load(SReader* reader)
{
	_MyBase::Load(reader);

	carFuncMap.clear();

	lsl::SReader* funcMap = reader->ReadValue("carFuncMap");
	if (funcMap)
	{
		SReader* func = funcMap->FirstChildValue();
		while (func)
		{
			MapObjRec* car = MapObjLib::LoadRecordRef(func, "car");
			CarFunc carFunc;
			carFunc.ReadFrom(func);

			carFuncMap[car] = carFunc;
			
			func = func->NextValue();
		}
	}
}

MobilityItem* MobilityItem::IsMobilityItem()
{
	return this;
}

void MobilityItem::ApplyChanges()
{
	if (GetPlayer())
		GetPlayer()->ApplyMobility();
}

float MobilityItem::CalcLife(const CarFunc& func)
{
	return func.life;
}




WheelItem::WheelItem(Slot* slot): _MyBase(slot)
{
}




MotorItem::MotorItem(Slot* slot): _MyBase(slot)
{	
}




TrubaItem::TrubaItem(Slot* slot): _MyBase(slot)
{
}




ArmorItem::ArmorItem(Slot* slot): _MyBase(slot), _armor4Installed(false)
{
}

Race* ArmorItem::GetRace() const
{
	if (GetPlayer())
		return GetPlayer()->GetRace();
	else
		return ((Race*)GetSlot()->GetRecord()->GetLib()->GetOwner());
}

const std::string& ArmorItem::DoGetName() const
{
	if (_armor4Installed || CheckArmor4(true))
		return _SC(scArmor4);

	return MobilityItem::DoGetName();
}

const std::string& ArmorItem::DoGetInfo() const
{
	if (_armor4Installed || CheckArmor4(true))
		return _SC(scArmor4Info);

	return MobilityItem::DoGetInfo();
}

int ArmorItem::DoGetCost() const
{
	return MobilityItem::DoGetCost();
}

graph::IndexedVBMesh* ArmorItem::DoGetMesh() const
{
	if (_armor4Installed || CheckArmor4(true))
		return GetRace()->GetMesh("Upgrade\\armor4.r3d");

	return MobilityItem::DoGetMesh();
}

graph::Tex2DResource* ArmorItem::DoGetTexture() const
{
	if (_armor4Installed || CheckArmor4(true))
		return GetRace()->GetTexture("Upgrade\\armor4.dds");

	return MobilityItem::DoGetTexture();
}

float ArmorItem::CalcLife(const CarFunc& func)
{
	return func.life + ((_armor4Installed || CheckArmor4()) ? 10.0f : 0.0f);
}

bool ArmorItem::CheckArmor4(bool ignorePlayers) const
{
	if (GetSlot()->GetRecord() && GetSlot()->GetRecord()->GetName() == "armor3")
		return GetRace()->GetAchievment().CheckAchievment("armor4") && (ignorePlayers || (GetPlayer() && GetPlayer()->IsHuman()));

	return false;
}

bool ArmorItem::IsArmor4Installed() const
{
	return _armor4Installed;
}

void ArmorItem::InstalArmor4(bool instal)
{
	_armor4Installed = instal;
}




WeaponItem::WeaponItem(Slot* slot): _MyBase(slot), _mapObj(0), _inst(0), _maxCharge(0), _cntCharge(0), _curCharge(0), _chargeStep(1), _damage(0), _chargeCost(0)
{
}

WeaponItem::~WeaponItem()
{
	lsl::SafeRelease(_inst);
	SetMapObj(0);
}

void WeaponItem::TransformChanged()
{
	if (_inst)
	{
		_inst->GetGameObj().SetPos(GetPos());
		_inst->GetGameObj().SetRot(GetRot());
	}
}

void WeaponItem::ApplyWpnDesc()
{
	if (GetWeapon())
		GetWeapon()->SetDesc(_wpnDesc);
}

void WeaponItem::OnCreateCar(MapObj* car)
{
	_MyBase::OnCreateCar(car);

	if (_mapObj)
	{
		_inst = &GetPlayer()->GetCar().gameObj->GetWeapons().Add(_mapObj);
		_inst->AddRef();

		_inst->GetGameObj().SetPos(GetPos());
		_inst->GetGameObj().SetRot(GetRot());

		ApplyWpnDesc();
	}
}

void WeaponItem::OnDestroyCar(MapObj* car)
{	
	lsl::SafeRelease(_inst);
}

void WeaponItem::Save(SWriter* writer)
{
	_MyBase::Save(writer);

	MapObjLib::SaveRecordRef(writer, "mapObj", _mapObj);
	writer->WriteValue("damage", _damage);
	writer->WriteValue("maxCharge", _maxCharge);
	writer->WriteValue("cntCharge", _cntCharge);
	writer->WriteValue("curCharge", _curCharge);
	writer->WriteValue("chargeStep", _chargeStep);
	writer->WriteValue("chargeCost", _chargeCost);

	_wpnDesc.SaveTo(writer, this);
}

void WeaponItem::Load(SReader* reader)
{
	_MyBase::Load(reader);

	SetMapObj(MapObjLib::LoadRecordRef(reader, "mapObj"));
	reader->ReadValue("damage", _damage);
	reader->ReadValue("maxCharge", _maxCharge);
	reader->ReadValue("cntCharge", _cntCharge);
	reader->ReadValue("curCharge", _curCharge);
	reader->ReadValue("chargeStep", _chargeStep);
	reader->ReadValue("chargeCost", _chargeCost);	

	_wpnDesc.LoadFrom(reader, this);
}

void WeaponItem::OnFixUp(const FixUpNames& fixUpNames)
{
	_MyBase::OnFixUp(fixUpNames);

	_wpnDesc.OnFixUp(fixUpNames);
}

WeaponItem* WeaponItem::IsWeaponItem()
{
	return this;
}

bool WeaponItem::Shot(const Proj::ShotContext& ctx, int newCharge, Weapon::ProjList* projList)
{
	bool res = false;

	if (_curCharge > 0 || _maxCharge == 0)
	{
		res = Weapon::CreateShot(GetWeapon(), _wpnDesc, ctx, projList);
		if (newCharge == -1)
			newCharge = res ? _curCharge - 1 : _curCharge;
	}

	_curCharge = std::max(newCharge, 0);

	return res;
}

void WeaponItem::Reload()
{
	SetCurCharge(GetCntCharge());
}

bool WeaponItem::IsReadyShot()
{
	return _inst && GetWeapon()->IsReadyShot();
}

MapObjRec* WeaponItem::GetMapObj()
{
	return _mapObj;
}

void WeaponItem::SetMapObj(MapObjRec* value)
{
	if (ReplaceRef(_mapObj, value))
	{
		_mapObj = value;		
	}
}

unsigned WeaponItem::GetMaxCharge()
{
	return _maxCharge;
}

void WeaponItem::SetMaxCharge(unsigned value)
{
	_maxCharge = value;
}

unsigned WeaponItem::GetCntCharge()
{
	return _cntCharge;
}

void WeaponItem::SetCntCharge(unsigned value)
{
	_cntCharge = value;
}

unsigned WeaponItem::GetCurCharge()
{
	return _curCharge;
}

void WeaponItem::SetCurCharge(unsigned value)
{
	_curCharge = value;
}

unsigned WeaponItem::GetChargeStep()
{
	return _chargeStep;
}

void WeaponItem::SetChargeStep(unsigned value)
{
	_chargeStep = value;
}

float WeaponItem::GetDamage(bool statDmg) const
{
	float damage = 0.0f;
	Proj::Type projType = Proj::cProjTypeEnd;

	for (Weapon::ProjDescList::const_iterator iter = _wpnDesc.projList.begin(); iter != _wpnDesc.projList.end(); ++iter)
	{
		damage += iter->damage;
		projType = iter->type;
	}

	//if (statDmg)
	//{
	//	if (projType == Proj::ptDrobilka || projType == Proj::ptTorpeda)
	//		damage = damage / 3.0f;
	//}

	return damage;
}

void WeaponItem::SetDamage(float value)
{
	//invalid
	_damage = value;
}

int WeaponItem::GetChargeCost() const
{
	return _chargeCost;
}

void WeaponItem::SetChargeCost(int value)
{
	_chargeCost = value;
}

const Weapon::Desc& WeaponItem::GetWpnDesc() const
{
	return _wpnDesc;
}

void WeaponItem::SetWpnDesc(const Weapon::Desc& value)
{
	_wpnDesc = value;
	ApplyWpnDesc();
}

Weapon* WeaponItem::GetWeapon()
{
	return _inst ? &_inst->GetGameObj<Weapon>() : 0;
}

Weapon::Desc WeaponItem::GetDesc()
{
	return GetWeapon() ? GetWeapon()->GetDesc() : _wpnDesc;
}




HyperItem::HyperItem(Slot* slot): _MyBase(slot)
{
}




MineItem::MineItem(Slot* slot): _MyBase(slot)
{
}




DroidItem::DroidItem(Slot* slot): WeaponItem(slot), _repairValue(5.0f), _repairPeriod(1.0f), _time(0.0f)
{	
}

DroidItem::~DroidItem()
{
	UnregProgressEvent();
}

void DroidItem::OnCreateCar(MapObj* car)
{
	WeaponItem::OnCreateCar(car);

	_time = 0.0f;
	RegProgressEvent();
}

void DroidItem::OnDestroyCar(MapObj* car)
{
	WeaponItem::OnDestroyCar(car);

	UnregProgressEvent();
}

void DroidItem::OnProgress(float deltaTime)
{
	if (GetPlayer() && GetPlayer()->GetCar().gameObj)
	{
		GameObject* car = GetPlayer()->GetCar().gameObj;

		if (car->GetLife() >= car->GetMaxLife() || car->GetLiveState() == GameObject::lsDeath)
		{
			_time = 0.0f;
		}
		else if ((_time += deltaTime ) > _repairPeriod)
		{
			_time -= _repairPeriod;
			GetPlayer()->GetCar().gameObj->Healt(5.0f);
		}
	}
}

void DroidItem::Save(SWriter* writer)
{
	WeaponItem::Save(writer);

	writer->WriteValue("repairValue", _repairValue);
	writer->WriteValue("repairPeriod", _repairPeriod);
}

void DroidItem::Load(SReader* reader)
{
	WeaponItem::Load(reader);

	reader->ReadValue("repairValue", _repairValue);
	reader->ReadValue("repairPeriod", _repairPeriod);
}

float DroidItem::GetRepairValue() const
{
	return _repairValue;
}

void DroidItem::SetRepairValue(float value)
{
	_repairValue = value;
}

float DroidItem::GetRepairPeriod() const
{
	return _repairPeriod;
}

void DroidItem::SetRepairPeriod(float value)
{
	_repairPeriod = value;
}




ReflectorItem::ReflectorItem(Slot* slot): WeaponItem(slot), _reflectValue(0.25f)
{	
}

ReflectorItem::~ReflectorItem()
{
}

void ReflectorItem::Save(SWriter* writer)
{
	WeaponItem::Save(writer);

	writer->WriteValue("reflectValue", _reflectValue);
}

void ReflectorItem::Load(SReader* reader)
{
	WeaponItem::Load(reader);

	reader->ReadValue("reflectValue", _reflectValue);
}

float ReflectorItem::GetReflectValue() const
{
	return _reflectValue;
}

void ReflectorItem::SetReflectValue(float value)
{
	_reflectValue = value;
}

float ReflectorItem::Reflect(float damage)
{
	return damage * lsl::ClampValue(1.0f - _reflectValue, 0.0f, 1.0f);
}




Slot::Slot(Player* player): _player(player), _type(cTypeEnd), _item(0), _record(0)
{
	InitClassList();

	CreateItem(stBase);
}

Slot::~Slot()
{
	SetRecord(0);

	FreeItem();
}

void Slot::InitClassList()
{
	static bool init = false;

	if (!init)
	{
		init = true;

		classList.Add<SlotItem>(stBase);
		classList.Add<WheelItem>(stWheel);
		classList.Add<TrubaItem>(stTruba);
		classList.Add<ArmorItem>(stArmor);
		classList.Add<MotorItem>(stMotor);
		classList.Add<HyperItem>(stHyper);
		classList.Add<MineItem>(stMine);
		classList.Add<WeaponItem>(stWeapon);
		classList.Add<DroidItem>(stDroid);
		classList.Add<ReflectorItem>(stReflector);
	}
}

void Slot::FreeItem()
{
	lsl::SafeDelete(_item);
}

void Slot::Save(SWriter* writer)
{
	if (_record)
		RecordLib::SaveRecordRef(writer, "record", _record);
	else
	{
		writer->WriteValue("type", _type);
		writer->WriteValue("item", _item);
	}
}

void Slot::Load(SReader* reader)
{
	Record* record = RecordLib::LoadRecordRef(reader, "record");

	if (record)
		SetRecord(record);
	else
	{
		int type = stBase;
		reader->ReadValue("type", type);
		CreateItem(Type(type));
		reader->ReadValue("item", _item);
	}
}

SlotItem& Slot::CreateItem(Type type)
{
	FreeItem();

	_type = type;
	_item = classList.CreateInst(type, this);

	return *_item;
}

SlotItem& Slot::GetItem()
{
	LSL_ASSERT(_item != 0);

	return *_item;
}

Player* Slot::GetPlayer()
{
	return _player;
}

Slot::Type Slot::GetType() const
{
	return _type;
}

Record* Slot::GetRecord()
{
	return _record;
}

void Slot::SetRecord(Record* value)
{
	if (Object::ReplaceRef(_record, value))
	{
		_record = value;

		if (_record)
			_record->Load(this);
	}
}




Player::Player(Race* race): cTimeRestoreCar(2.0f), _race(race), _carMaxSpeedBase(0), _carTireSpringBase(0), _timeRestoreCar(0), _headLight(hlmNone), _reflScene(true), _money(0), _points(0), _pickMoney(0), _id(cUndefPlayerId), _gamerId(-1), _netSlot(Race::cDefaultNetSlot), _netName(""), _place(0), _finished(false), _cheatEnable(cCheatDisable), _nextBonusProjId(cBonusProjUndef + 1), _nightFlare(NULL), _block(-1.0f)
{
	ZeroMemory(_lights, sizeof(_lights));
	ZeroMemory(_slot, sizeof(_slot));

	_car.owner = this;
}

Player::~Player()
{
	ClearBonusProjs();

	for (int i = 0; i < cSlotTypeEnd; ++i)
		lsl::SafeDelete(_slot[i]);

	SetHeadlight(hlmNone);
	SetCar(0);
	FreeColorMat();	
}

Player::CarState::CarState(): owner(NULL), record(0), colorMat(0), color(clrWhite), mapObj(0), gameObj(0), grActor(0), nxActor(0), track(0), curTile(0), curNode(0), lastNode(0), numLaps(0)
{
	pos = NullVec2;
	pos3 = NullVector;
	rot3 = NullQuaternion;	
	dir = IdentityVec2;
	dir3 = IdentityVector;
	worldMat = IdentityMatrix;
	dirLine = Line2FromDir(dir, pos);
	normLine = Line2FromNorm(dir, pos);
	radius = 0.0f;
	size = 0.0f;
	kSteerControl = 0.0f;
	lastNodeCoordX = 0.5f;

	track = 0;
	trackDirLine = NullVector;
	trackNormLine = NullVector;

	moveInverse = false;
	moveInverseStart = -1;

	maxSpeed = 0;
	maxSpeedTime = 0;

	cheatSlower = false;
	cheatFaster = false;
}

Player::CarState::~CarState()
{
	SetLastNode(0);
	SetCurNode(0);
	SetCurTile(0);
}

void Player::CarState::Update(float deltaTime)
{
	LSL_ASSERT(mapObj);

	NxMat34 mat34 = nxActor->getGlobalPose();

	worldMat = D3DXMATRIX(mat34.M(0, 0), mat34.M(0, 1), mat34.M(0, 2), 0,
		mat34.M(1, 0), mat34.M(1, 1), mat34.M(1, 2), 0,
		mat34.M(2, 0), mat34.M(2, 1), mat34.M(2, 2), 0,
		mat34.t[0], mat34.t[1], mat34.t[2], 1);

	pos3 = mat34.t.get();
	nxActor->getGlobalOrientationQuat().getXYZW(rot3);
	Vec3Rotate(XVector, rot3, dir3);

	pos = D3DXVECTOR2(pos3);
	dir = D3DXVECTOR2(dir3);
	speed = GameCar::GetSpeed(nxActor, dir3);
	D3DXVec2Normalize(&dir, &dir);	

	dirLine = Line2FromDir(dir, pos);
	normLine = Line2FromNorm(dir, pos);	

	WayNode* tile = owner->GetMap()->GetTrace().IsTileContains(pos3, curTile);
	//Существуем множество проблем если подбирать ближвйщий узел, например с lastNode, а целесообразность пока неясна, поэтому пока убрано
	//Тайл не найден
	//if (!tile)
		//Поиск ближайщего узла
	//	tile = owner->GetMap()->GetTrace().FindClosestNode(pos3);	
	SetCurTile(tile);
	//
	if (curTile && curTile->GetNext() && curTile->GetNext()->IsContains(pos3))
		SetCurNode(curTile->GetNext());
	else
		SetCurNode(curTile);
	//
	if (curTile)
	{
		Line2FromDir(curTile->GetTile().GetDir(), pos, trackDirLine);
		Line2FromNorm(curTile->GetTile().GetDir(), pos, trackNormLine);

		track = curTile->GetTile().ComputeTrackInd(pos);		
	}

	//
	bool chLastNode = curTile && lastNode != curTile;
	bool newLastNode1 = chLastNode && !lastNode;
	bool newLastNode2 = chLastNode && lastNode && ((lastNode->GetNext() && lastNode->GetNext()->GetPoint()->IsFind(curTile)) || lastNode->GetPoint()->IsFind(curTile, lastNode) || lastNode->GetPath()->GetFirst()->GetPoint()->IsFind(curTile, lastNode->GetPath()->GetFirst()));
	
	bool onLapPass = false;
	
	if (newLastNode1 || newLastNode2)
	{
		//круг пройден
		onLapPass = lastNode && curTile == lastNode->GetPath()->GetTrace()->GetPathes().front()->GetFirst();

		SetLastNode(curTile);
	}

	if (lastNode && lastNode->GetTile().IsContains(pos3))
		lastNodeCoordX = lastNode->GetTile().ComputeCoordX(pos);

	if (onLapPass)
		owner->OnLapPass();

	//инверсия движения
	if (curTile && D3DXVec2Dot(&curTile->GetTile().GetDir(), &dir) < 0)
	{
		if (moveInverseStart < 0)
		{
			moveInverseStart = GetDist();
		}
		if (!moveInverse && moveInverseStart - GetDist() > 20.0f)
		{
			moveInverse = true;
			owner->SendEvent(cPlayerMoveInverse);
		}
	}
	else if (curTile)
	{
		moveInverse = false;
		moveInverseStart = -1;
	}

	//контроль за скоростью	
	maxSpeedTime += deltaTime;
	if (speed > maxSpeed || maxSpeedTime > 1.0f)
	{
		maxSpeed = speed;
		maxSpeedTime = 0;
	}
	else if (abs(maxSpeed - speed) < 5.0f)
	{
		maxSpeedTime = 0;
	}
	if (maxSpeed > 0 && maxSpeed - speed > 80.0f && maxSpeedTime <= 1.0f)
	{
		maxSpeed = speed;
		maxSpeedTime = 0;
		owner->SendEvent(cPlayerLostControl);
	}

	/*//контроль за направляющим углом
	float dirAngle = acos(abs(D3DXVec3Dot(&lastDir, &dir3)));
	lastDir = dir3;
	summAngle += dirAngle;
	summAngleTime += deltaTime;
	if (dirAngle > D3DX_PI/24)
	{
		summAngleTime = 0;
	}
	else if (summAngleTime > 1.0f)
	{
		summAngle = 0;
		summAngleTime = 0;
	}
	if (summAngle > D3DX_PI/2 && summAngleTime <= 1.0f)
	{
		summAngle = 0;
		summAngleTime = 0;
		owner->SendEvent(Race::cPlayerLostControl);
	}*/
}

void Player::CarState::SetCurTile(WayNode* value)
{
	if (ReplaceRef(curTile, value))
		curTile = value;
}

void Player::CarState::SetCurNode(WayNode* value)
{
	if (ReplaceRef(curNode, value))
		curNode = value;
}

void Player::CarState::SetLastNode(WayNode* value)
{
	if (ReplaceRef(lastNode, value))
	{
		lastNode = value;
		lastNodeCoordX = 0.5f;
	}
}

WayNode* Player::CarState::GetCurTile(bool lastCorrect) const
{
	return ((lastCorrect || curTile == NULL) && lastNode) ? lastNode : curTile;
}

int Player::CarState::GetPathIndex(bool lastCorrect) const
{
	WayNode* tile = GetCurTile(lastCorrect);

	if (tile)
	{
		int i = 0;
		Trace* trace = tile->GetPath()->GetTrace();
		for (Trace::Pathes::const_iterator iter = trace->GetPathes().begin(); iter != trace->GetPathes().end(); ++iter, ++i)
			if (*iter == tile->GetPath())
				return i;
	}

	return -1;
}

bool Player::CarState::IsMainPath(bool lastCorrect) const
{
	return GetPathIndex(lastCorrect) == 0;
}

float Player::CarState::GetPathLength(bool lastCorrect) const
{
	WayNode* tile = GetCurTile(lastCorrect);
	if (tile)
		return tile->GetPath()->GetLength();

	Map* map = owner && owner->GetRace() ? owner->GetRace()->GetGame()->GetWorld()->GetMap() : NULL;	

	return map && map->GetTrace().GetPathes().size() > 0 ? map->GetTrace().GetPathes().front()->GetLength() : 1.0f;
}

float Player::CarState::GetDist(bool lastCorrect) const
{
	WayNode* tile = GetCurTile(lastCorrect);
	if (tile)
		return tile->GetPath()->GetLength() - (tile->GetTile().GetFinishDist() - tile->GetTile().GetLength(pos));

	return 0;
}

float Player::CarState::GetLap(bool lastCorectDist) const
{	
	float dist = GetDist(lastCorectDist);

	return numLaps + dist/GetPathLength(lastCorectDist);
}

D3DXVECTOR3 Player::CarState::GetMapPos() const
{
	D3DXVECTOR3 res = NullVector;
	if (curTile)
	{
		res = curTile->GetTile().GetPoint(curTile->GetTile().ComputeCoordX(pos));
	} 
	else if (lastNode)
	{
		res = lastNode->GetTile().GetPoint(lastNodeCoordX);
	}

	return res;
}

//Player::CarState::operator bool() const
//{
//	return (mapObj && curTile && curNode);
//}

void Player::InsertBonusProj(Proj* proj, int projId)
{
	BonusProj bonusProj;
	bonusProj.id = projId;
	_nextBonusProjId = projId + 1;

	bonusProj.proj = proj;
	proj->AddRef();
	proj->InsertListener(this);

	_bonusProjs.push_back(bonusProj);	
}

void Player::RemoveBonusProj(BonusProjs::const_iterator iter)
{
	iter->proj->Release();
	iter->proj->RemoveListener(this);
	_bonusProjs.erase(iter);	
}

void Player::RemoveBonusProj(Proj* proj)
{
	for (BonusProjs::iterator iter = _bonusProjs.begin(); iter != _bonusProjs.end(); ++iter)
		if (iter->proj == proj)
		{
			RemoveBonusProj(iter);
			return;
		}
}

void Player::ClearBonusProjs()
{
	while (_bonusProjs.size() > 0)
		RemoveBonusProj(_bonusProjs.begin());
}

void Player::InitLight(HeadLight headLight, const D3DXVECTOR3& pos, const D3DXQUATERNION& rot)
{
	if (!_lights[headLight])
	{
		GraphManager::LightDesc desc;
		desc.shadow = headLight == hlFirst && _race->GetWorld()->GetEnv()->GetShadowQuality() == Environment::eqHigh;
		desc.shadowNumSplit = 1;
		desc.shadowDisableCropLight = true;
		desc.nearDist = 1.0f;
		desc.farDist = 50.0f;
		_lights[headLight] = GetGraph()->AddLight(desc);
		_lights[headLight]->AddRef();
		_lights[headLight]->GetSource()->SetType(D3DLIGHT_SPOT);
		_lights[headLight]->GetSource()->SetAmbient(clrBlack);
		_lights[headLight]->GetSource()->SetDiffuse(clrWhite * 1.0f);
		_lights[headLight]->GetSource()->SetPhi(D3DX_PI/3.0f);
		_lights[headLight]->GetSource()->SetTheta(D3DX_PI/6.0f);
	}
	
	_lights[headLight]->GetSource()->SetPos(pos);
	_lights[headLight]->GetSource()->SetRot(rot);

	SetLightParent(_lights[headLight], _car.mapObj);
}

void Player::FreeLight(HeadLight headLight)
{
	if (_lights[headLight])
	{
		_lights[headLight]->Release();
		GetGraph()->DelLight(_lights[headLight]);
		_lights[headLight] = 0;
	}
}

void Player::SetLightParent(GraphManager::LightSrc* light, MapObj* mapObj)
{
	LSL_ASSERT(light);

	light->GetSource()->SetParent(mapObj ? &mapObj->GetGameObj().GetGrActor() : 0);
	light->SetEnable(mapObj ? true : false);
}

void Player::CreateNightLights(MapObj* mapObj)
{
	if (_nightFlare == NULL)
		return;

	_nightFlare->GetNodes().Clear();
	_nightFlare->SetParent(mapObj ? &mapObj->GetGameObj().GetGrActor().GetNodes().front() : NULL);

	if (mapObj == NULL)
		return;

	//{D3DXVECTOR3(1.7f, 0.4f, 0.02f), D3DXVECTOR2(1.5f, 1.5f), true},
	//{D3DXVECTOR3(1.7f, -0.4f, 0.02f), D3DXVECTOR2(1.5f, 1.5f), true},
	//{D3DXVECTOR3(-1.57f, 0.45f, 0.28f), D3DXVECTOR2(1.0f, 1.0f), false},
	//{D3DXVECTOR3(-1.57f, -0.45f, 0.28f), D3DXVECTOR2(1.0f, 1.0f), false}

	Garage::Car* car = _race->GetGarage().FindCar(_car.record);
	if (car == NULL)
		return;

	for (unsigned i = 0; i < car->GetNightLights().size(); ++i)
	{
		graph::Sprite& plane = _nightFlare->GetNodes().Add<graph::Sprite>();
		plane.fixDirection = false;
		plane.sizes = car->GetNightLights()[i].size;
		plane.SetPos(car->GetNightLights()[i].pos);
		plane.material.Set(&_race->GetWorld()->GetResManager()->GetMatLib().Get(car->GetNightLights()[i].head ? "Effect\\flare7White" : "Effect\\flare7Red"));
	}
}

void Player::SetLightsParent(MapObj* mapObj)
{
	for (int i = 0; i < cHeadLightEnd; ++i)
		if (_lights[i])
			SetLightParent(_lights[i], mapObj);

	CreateNightLights(mapObj);
}

void Player::ApplyReflScene()
{
	if (!_car.mapObj)
		return;

	graph::Actor::GraphDesc desc = _car.grActor->GetGraphDesc();
	desc.props.set(graph::Actor::gpReflScene, _reflScene);
	_car.grActor->SetGraph(_car.grActor->GetGraph(), desc);
}

void Player::ReleaseCar()
{
	LSL_ASSERT(_car.mapObj);

	for (int i = 0; i < cSlotTypeEnd; ++i)
		if (_slot[i])
			_slot[i]->GetItem().OnDestroyCar(_car.mapObj);

	SetLightsParent(0);

	_car.nxActor = NULL;
	lsl::SafeRelease(_car.grActor);
	lsl::SafeRelease(_car.gameObj);
	lsl::SafeRelease(_car.mapObj);	
}

void Player::ApplyMobility()
{
	MapObjRec* record = _car.record;
	GameCar* car = _car.gameObj;

	if (record && car)
	{
		CarMotorDesc motorDesc = car->GetMotorDesc();
		motorDesc.maxTorque = 0.0f;

		NxTireFunctionDesc longTireDesc = car->GetWheels().front().GetShape()->GetLongitudalTireForceFunction();
		longTireDesc.asymptoteSlip = longTireDesc.asymptoteValue = longTireDesc.extremumSlip = longTireDesc.extremumValue = longTireDesc.stiffnessFactor = 0.0f;
		
		NxTireFunctionDesc latTireDesc = car->GetWheels().front().GetShape()->GetLateralTireForceFunction();
		latTireDesc.asymptoteSlip = latTireDesc.asymptoteValue = latTireDesc.extremumSlip = latTireDesc.extremumValue = latTireDesc.stiffnessFactor = 0.0f;

		float maxLife = 0.0f;
		float maxSpeed = 0.0f;
		float tireSpring = _carTireSpringBase;
		
		for (int i = 0; i < cSlotTypeEnd; ++i)	
		{
			MobilityItem* mobi = _slot[i] ? _slot[i]->GetItem().IsMobilityItem() : 0;
			MobilityItem::CarFunc* carFunc = 0;
			if (mobi)
			{
				MobilityItem::CarFuncMap::iterator iter = mobi->carFuncMap.find(record);
				if (iter != mobi->carFuncMap.end())
					carFunc = &iter->second;
			}
			if (carFunc)
			{
				motorDesc.maxTorque += carFunc->maxTorque;				
				maxLife += mobi->CalcLife(*carFunc);
				maxSpeed = std::max(carFunc->maxSpeed, maxSpeed);
				tireSpring += carFunc->tireSpring;

				longTireDesc.extremumSlip += carFunc->longTire.extremumSlip;
				longTireDesc.extremumValue += carFunc->longTire.extremumValue;
				longTireDesc.asymptoteSlip += carFunc->longTire.asymptoteSlip;
				longTireDesc.asymptoteValue += carFunc->longTire.asymptoteValue;

				latTireDesc.extremumSlip += carFunc->latTire.extremumSlip;
				latTireDesc.extremumValue += carFunc->latTire.extremumValue;
				latTireDesc.asymptoteSlip += carFunc->latTire.asymptoteSlip;
				latTireDesc.asymptoteValue += carFunc->latTire.asymptoteValue;
			}
		}

		car->SetMotorDesc(motorDesc);

		for (GameCar::Wheels::iterator iter = car->GetWheels().begin(); iter != car->GetWheels().end(); ++iter)
		{
			(*iter)->GetShape()->SetLongitudalTireForceFunction(longTireDesc);
			(*iter)->GetShape()->SetLateralTireForceFunction(latTireDesc);
		}

		if (_race && _race->GetProfile() && (IsHuman() || IsOpponent()))
			maxLife *= cHumanArmorK[_race->GetProfile()->difficulty()];

		car->SetMaxLife(maxLife);
		car->SetMaxSpeed(maxSpeed + _carMaxSpeedBase);
		car->SetTireSpring(tireSpring);
	}
}

void Player::CreateColorMat(const graph::LibMaterial& colorMat)
{
	if (!_car.colorMat)
		_car.colorMat = new graph::LibMaterial(colorMat);
}

void Player::FreeColorMat()
{
	if (_car.colorMat)
	{
		delete _car.colorMat;
		_car.colorMat = 0;
	}
}

void Player::ApplyColorMat()
{
	FreeColorMat();	

	if (_car.mapObj && _car.gameObj->GetGrActor().GetNodes().Size() > 0 && _car.gameObj->GetGrActor().GetNodes().begin()->GetType() == graph::Actor::ntIVBMesh && !_car.gameObj->GetDisableColor())
	{
		graph::IVBMeshNode& mesh = static_cast<graph::IVBMeshNode&>(_car.gameObj->GetGrActor().GetNodes().front());
		CreateColorMat(*mesh.material.Get());

		mesh.material.Set(_car.colorMat);
	}

	ApplyColor();
}

void Player::ApplyColor()
{
	if (_car.colorMat && _car.colorMat->samplers.Size() > 0)
	{
		_car.colorMat->samplers[0].SetColor(_car.color);
	}
}

void Player::CheatUpdate(float deltaTime)
{	
	Player* opponent = NULL;
	float maxLapDist = 0;
	_car.cheatFaster = false;
	_car.cheatSlower = false;

	if (_cheatEnable)
	{
		for (Race::PlayerList::const_iterator iter = _race->GetPlayerList().begin(); iter != _race->GetPlayerList().end(); ++iter)
		{
			Player* player = *iter;
			if (this != player && (player->GetId() == Race::cHuman || (player->GetId() & Race::cOpponentMask)))
			{
				float dist = player->GetCar().GetLap() - _car.GetLap();
				if (maxLapDist < dist || opponent == NULL)
				{
					opponent = player;
					maxLapDist = dist;
				}
			}
		}
	}

	if (_cheatEnable && opponent)
	{
		float dist = abs(_car.GetLap() - opponent->GetCar().GetLap());
		dist = dist - floor(dist);
		dist = std::min(dist, 1.0f - dist);
		dist = dist * _car.GetPathLength();

		Difficulty diff = _race && _race->GetProfile() ? _race->GetProfile()->difficulty() : gdNormal;

		//human far
		if (_car.GetLap() > opponent->GetCar().GetLap() && dist > cHumanEasingMinDist[diff])
		{
			if (_cheatEnable & cCheatEnableSlower)
			{
				float speedLimit = ClampValue((dist - cHumanEasingMinDist[diff]) / (cHumanEasingMaxDist[diff] - cHumanEasingMinDist[diff]), 0.0f, 1.0f);
				speedLimit = cHumanEasingMinSpeed[diff] + (cHumanEasingMaxSpeed[diff] - cHumanEasingMinSpeed[diff]) * speedLimit;

				if (_car.speed > speedLimit)
					_car.cheatSlower = true;
			}
		}
		//human ahead
		else if (dist > cHumanEasingMinDist[diff])
		{
			if (_cheatEnable & cCheatEnableFaster)
			{
				float torqueK = ClampValue((dist - cHumanEasingMinDist[diff]) / (cHumanEasingMaxDist[diff] - cHumanEasingMinDist[diff]), 0.0f, 1.0f);
				torqueK = cCompCheatMinTorqueK[diff] + (cCompCheatMaxTorqueK[diff] - cCompCheatMinTorqueK[diff]) * torqueK;

				_car.cheatFaster = true;			
				SetCheatK(_car, torqueK, torqueK);
			}
		}
	}

	if (!_car.cheatFaster)
		SetCheatK(_car, 1, 1);
}

void Player::SetCheatK(const Player::CarState& car, float torqueK, float steerK)
{
	if (car.gameObj == NULL)
		return;

	if (abs(car.gameObj->GetMotorTorqueK() - torqueK) > 0.01f)
		car.gameObj->SetMotorTorqueK(torqueK);

	if (abs(car.gameObj->GetWheelSteerK() - steerK) > 0.01f)
		car.gameObj->SetWheelSteerK(steerK);
}

GraphManager* Player::GetGraph()
{
	return _race->GetWorld()->GetGraph();
}

WayNode* Player::GetLastNode()
{
	if (!_car.lastNode && !GetMap()->GetTrace().GetPathes().empty() && GetMap()->GetTrace().GetPathes().front()->GetCount() > 0)
	{
		_car.SetLastNode(GetMap()->GetTrace().GetPathes().front()->GetFirst());
	}

	return _car.lastNode;
}

void Player::SendEvent(unsigned id, EventData* data)
{
	if (data)
		data->playerId = _id;

	GetRace()->SendEvent(id, data ? data : &MyEventData(_id));
}

void Player::OnDestroy(GameObject* sender)
{
	if (_car.mapObj && sender == &_car.mapObj->GetGameObj())
		ReleaseCar();
	else if (sender->IsProj())
		RemoveBonusProj(sender->IsProj());
}

void Player::OnLowLife(GameObject* sender, Behavior* behavior)
{
	if (_car.mapObj && sender == &_car.mapObj->GetGameObj())
	{
		SendEvent(cPlayerLowLife);
	}
}

void Player::OnDeath(GameObject* sender, DamageType damageType, GameObject* target)
{
	if (_car.mapObj && sender == &_car.mapObj->GetGameObj())
	{
		switch (damageType)
		{
		case dtDeathPlane:
			SendEvent(cPlayerOverboard);
			break;

		case dtMine:
			SendEvent(cPlayerDeathMine);
			break;
		}		

		SendEvent(cPlayerDeath, &MyEventData(Slot::cTypeEnd, GameObject::cBonusTypeEnd, NULL, sender->GetTouchPlayerId(), damageType));
	}
}

void Player::OnLapPass()
{
	++_car.numLaps;
	ReloadWeapons();

	_race->OnLapPass(this);
}

void Player::OnProgress(float deltaTime)
{
	if (_car.mapObj)
	{
		_car.Update(deltaTime);

		CheatUpdate(deltaTime);
	}
	else
	{
		_timeRestoreCar += deltaTime;
		if (_timeRestoreCar > cTimeRestoreCar)
		{
			_timeRestoreCar = 0;
			CreateCar(false);
			ResetCar();			
		}

		_car.cheatFaster = false;
		_car.cheatSlower = false;
		SetCheatK(_car, 1, 1);
	}

	if (_block >= 0.0f)
	{
		_block = std::max(_block - deltaTime, 0.0f);
		if (_car.gameObj)
			_car.gameObj->SetMoveCar(_block == 0.0f ? GameCar::mcBrake : GameCar::mcNone);
	}
}

Player* Player::FindClosestEnemy(float viewAngle, bool zTest)
{
	//нет смысла искать противников
	if (!_car.mapObj)
		return 0;

	float minDist = 0;
	Player* enemy = 0;
	
	for (Race::PlayerList::const_iterator iter = _race->GetPlayerList().begin(); iter != _race->GetPlayerList().end(); ++iter)
	{
		Player* tPlayer = *iter;
		const CarState& tCar = tPlayer->GetCar();

		if (tCar.mapObj && tCar.mapObj != _car.mapObj)
		{
			if (zTest && _car.curTile && !_car.curTile->GetTile().IsZLevelContains(tCar.pos3))
			{
				continue;
			}

			D3DXVECTOR3 carPos = _car.pos3;
			D3DXVECTOR3 carDir = _car.dir3;
			D3DXVECTOR3 enemyPos = tCar.pos3;

			D3DXVECTOR3 dir;
			D3DXVec3Normalize(&dir, &(enemyPos - carPos));
			float angle = D3DXVec3Dot(&dir, &carDir);

			D3DXPLANE dirPlane;
			D3DXPlaneFromPointNormal(&dirPlane, &carPos, &carDir);
			float dist = D3DXPlaneDotCoord(&dirPlane, &enemyPos);
			float absDist = abs(dist);

			//Объект ближе
			bool b1 = enemy == NULL || absDist < minDist;
			//Объект расположен с правильной стороны относительно машины
			bool b2 = viewAngle == 0.0f || (viewAngle > 0 ? (angle >= cos(viewAngle)) : (angle <= cos(D3DX_PI/2 - viewAngle)));

			if (b1 && b2)
			{
				enemy = tPlayer;
				minDist = absDist;
			}
		}
	}

	return enemy;
}

float Player::ComputeCarBBSize()
{
	LSL_ASSERT(_car.mapObj);

	AABB aabb = _car.grActor->GetLocalAABB(false);
	aabb.Transform(_car.grActor->GetWorldScale());
	
	return D3DXVec3Length(&aabb.GetSizes());
}

void Player::CreateCar(bool newRace)
{
	if (!_car.mapObj)
	{
		LSL_ASSERT(_car.record);

		_car.mapObj = &GetMap()->AddMapObj(_car.record);
		_car.mapObj->AddRef();
		_car.mapObj->SetPlayer(this);
		_car.gameObj = &_car.mapObj->GetGameObj<game::RockCar>();
		_car.gameObj->AddRef();
		_car.grActor = &_car.gameObj->GetGrActor();
		_car.grActor->AddRef();
		_car.nxActor = _car.gameObj->GetPxActor().GetNxActor();

		_car.kSteerControl = _car.gameObj->GetKSteerControl();
		_car.size = ComputeCarBBSize();
		_car.radius = _car.size/2.0f;
		_car.moveInverse = false;
		_car.moveInverseStart = -1;
		_car.maxSpeed = 0;
		_car.maxSpeedTime = 0;
		
		_car.gameObj->SetImmortalFlag(_finished);
		_car.gameObj->InsertListener(this);

		_carMaxSpeedBase = _car.gameObj->GetMaxSpeed();
		_carTireSpringBase = _car.gameObj->GetTireSpring();

		SetLightsParent(_car.mapObj);
		ApplyReflScene();

		for (int i = 0; i < cSlotTypeEnd; ++i)
			if (_slot[i])
				_slot[i]->GetItem().OnCreateCar(_car.mapObj);

		ApplyColorMat();
		ApplyMobility();
	}

	if (newRace)
	{
		WayNode* node = GetMap()->GetTrace().GetPathes().front()->GetFirst();
		_car.SetCurTile(node);
		_car.SetCurNode(node);
		_car.SetLastNode(node);	
		ClearBonusProjs();
		_timeRestoreCar = 0;
		_nextBonusProjId = cBonusProjUndef + 1;
	}
}

void Player::FreeCar(bool freeState)
{
	if (_car.mapObj)
	{
		MapObj* mapObj = _car.mapObj;
		_car.gameObj->RemoveListener(this);

		ReleaseCar();

		GetMap()->DelMapObj(mapObj);
	}

	if (freeState)
	{
		_car.SetCurTile(0);
		_car.SetCurNode(0);
		_car.SetLastNode(0);
		_car.numLaps = 0;
	}
}

void Player::ResetCar()
{
	WayNode* lastNode = GetLastNode();

	if (lastNode && _car.mapObj)
	{
		/*D3DXVECTOR3 pos = lastNode->GetTile().GetPoint(_car.lastNodeCoordX) + ZVector * lastNode->GetTile().ComputeHeight(0.5f) * 0.5f;	
		D3DXVECTOR2 dir2 = lastNode->GetTile().GetDir();

		NxRay nxRay(NxVec3(pos), NxVec3(-ZVector));
		NxRaycastHit hit;		
		NxShape* hitShape = _car.gameObj->GetPxActor().GetScene()->GetNxScene()->raycastClosestShape(nxRay, NX_STATIC_SHAPES, hit, 1 << px::Scene::cdgTrackPlane, NX_MAX_F32, NX_RAYCAST_SHAPE);

		if (hitShape == NULL)
			pos = lastNode->GetTile().GetPoint(0.0f) + ZVector * lastNode->GetTile().ComputeHeight(0.5f) * 0.5f;*/

		D3DXVECTOR3 pos = NullVector;
		D3DXVECTOR2 dir2 = NullVec2;

		WayNode* node = lastNode;
		float distX = node->GetTile().ComputeLength(_car.lastNodeCoordX);
		float offs[3] = {0.0f, -2.0f, 2.0f};
		bool isDeathPlane = false;

		for (int i = 0; i < 5; ++i)
		{
			D3DXVECTOR3 newPos;
			D3DXVECTOR2 newDir2;
			bool isFind = false;			

			for (int j = 0; j < 3; ++j)
			{
				float coordX = node->GetTile().ComputeCoordX(distX + offs[j]);
				D3DXVECTOR3 rayPos = node->GetTile().GetPoint(coordX) + ZVector * node->GetTile().ComputeHeight(0.5f) * 0.5f;

				if (i == 0 && j == 0)
				{
					pos = rayPos;
					dir2 = node->GetTile().GetDir();
				}
				if (j == 0)
				{
					newPos = rayPos;
					newDir2 = node->GetTile().GetDir();
				}

				NxRay nxRay(NxVec3(rayPos), NxVec3(-ZVector));
				NxRaycastHit hit;		
				NxShape* hitShape = _car.gameObj->GetPxActor().GetScene()->GetNxScene()->raycastClosestShape(nxRay, NX_ALL_SHAPES, hit, 1 << px::Scene::cdgTrackPlane | 1 << px::Scene::cdgPlaneDeath | 1 << px::Scene::cdgDefault, NX_MAX_F32, NX_RAYCAST_SHAPE);
				GameObject* hitGameObj = GameObject::GetGameObjFromShape(hitShape);

				if (!isDeathPlane && i == 0 && j == 0 && (hitShape == NULL || hitShape->getGroup() == px::Scene::cdgPlaneDeath))
				{
					isDeathPlane = true;
					distX = 0.0f;
					j = -1;
				}
				else if ((hitShape == NULL || hitShape->getGroup() != px::Scene::cdgTrackPlane) && hitGameObj != _car.gameObj)
				{
					break;
				}
				else if (j == 2)
				{
					pos = newPos;
					dir2 = newDir2;
					isFind = true;
				}
			}

			if (isFind)
				break;

			float newDistX = distX - 6.0f;

			if (newDistX < 0.0f)
			{
				if (node->GetPrev())
				{
					node = node->GetPrev();
					distX = std::max(node->GetTile().GetDirLength() + newDistX, 0.0f);
				}
				else
					break;
			}
			else
				distX = newDistX;
		}

		_car.gameObj->SetWorldPos(pos);
		_car.gameObj->SetWorldRot(NullQuaternion);
		_car.gameObj->SetWorldDir(D3DXVECTOR3(dir2.x, dir2.y, 0.0f));		

		_car.gameObj->GetPxActor().GetNxActor()->setLinearVelocity(NxVec3(NullVector));
		_car.gameObj->GetPxActor().GetNxActor()->setLinearMomentum(NxVec3(NullVector));
		_car.gameObj->GetPxActor().GetNxActor()->setAngularMomentum(NxVec3(NullVector));
		_car.gameObj->GetPxActor().GetNxActor()->setAngularVelocity(NxVec3(NullVector));
	}
}

bool Player::Shot(const Proj::ShotContext& ctx, SlotType type, unsigned projId, int newCharge, Weapon::ProjList* projList)
{
	LSL_ASSERT(_slot[type]);

	WeaponItem& item = _slot[type]->GetItem<WeaponItem>();
	Proj* lastProj = NULL;
	bool res = false;

	if (projList)
	{
		res = item.Shot(ctx, newCharge, projList);
		if (projList->size() > 0)
			lastProj = projList->front();
	}
	else
	{
		Weapon::ProjList myProjList;
		res = item.Shot(ctx, newCharge, &myProjList);

		if (myProjList.size() > 0)
			lastProj = myProjList.front();
	}

	if (lastProj && type == stMine)
	{
		InsertBonusProj(lastProj, projId);
	}

	return res;
}

void Player::ReloadWeapons()
{
	for (int i = stHyper; i <= stWeapon4; ++i)
		if (_slot[i])
		{
			WeaponItem& item = _slot[i]->GetItem<WeaponItem>();
			item.Reload();
		}
}

unsigned Player::GetBonusProjId(game::Proj* proj)
{
	for (BonusProjs::const_iterator iter = _bonusProjs.begin(); iter != _bonusProjs.end(); ++iter)
		if (iter->proj == proj && iter->proj->GetLiveState() == GameObject::lsLive)
			return iter->id;
	return cBonusProjUndef;
}

Proj* Player::GetBonusProj(unsigned id)
{
	for (BonusProjs::const_iterator iter = _bonusProjs.begin(); iter != _bonusProjs.end(); ++iter)
		if (iter->id == id && iter->proj->GetLiveState() == GameObject::lsLive)
			return iter->proj;
	return NULL;
}

unsigned Player::GetNextBonusProjId() const
{
	return _nextBonusProjId;
}

Race* Player::GetRace()
{
	return _race;
}

Map* Player::GetMap()
{
	return _race->GetWorld()->GetMap();
}

const Player::CarState& Player::GetCar() const
{
	return _car;
}

void Player::SetCar(MapObjRec* record)
{
	if (ReplaceRef(_car.record, record))
	{
		FreeCar(true);
		_car.record = record;
	}
}

Player::HeadLightMode Player::GetHeadLight() const
{
	return _headLight;
}

void Player::SetHeadlight(HeadLightMode value)
{
	if (_headLight != value)
	{
		_headLight = value;

		switch (_headLight)
		{
			case hlmNone:
			{
				for (int i = 0; i < cHeadLightEnd; ++i)
					FreeLight(HeadLight(i));
				break;
			}

			case hlmOne:
			{
				InitLight(hlFirst, D3DXVECTOR3(0.3f, 0.0f, 3.190f), D3DXQUATERNION(0.0009f, 0.344f, -0.029f, 0.939f));
				FreeLight(hlSecond);
				break;
			}
				
			case hlmTwo:
			{
				InitLight(hlFirst, D3DXVECTOR3(0.3f, 1.0f, 3.190f), D3DXQUATERNION(0.0009f, 0.344f, -0.029f, 0.939f));
				InitLight(hlSecond, D3DXVECTOR3(0.3f, -1.0f, 3.190f), D3DXQUATERNION(0.0009f, 0.344f, -0.029f, 0.939f));
				break;
			}
		}

		if (_nightFlare == NULL && _headLight != hlmNone)
		{
			_nightFlare = new graph::Actor();
			_nightFlare->AddRef();

			graph::Actor::GraphDesc desc;
			desc.props.set(graph::Actor::gpColor);
			desc.props.set(graph::Actor::gpDynamic);
			desc.lighting = graph::Actor::glStd;
			desc.order = graph::Actor::goEffect;

			_nightFlare->SetGraph(GetGraph(), desc);

			CreateNightLights(_car.mapObj);
		}
		else if (_nightFlare && _headLight == hlmNone)
		{
			_nightFlare->Release();
			lsl::SafeDelete(_nightFlare);
		}
	}
}

bool Player::GetReflScene() const
{
	return _reflScene;
}

void Player::SetReflScene(bool value)
{
	if (_reflScene != value)
	{
		_reflScene = value;
		
		ApplyReflScene();
	}
}

Record* Player::GetSlot(SlotType type)
{
	return _slot[type] ? _slot[type]->GetRecord() : 0;
}

void Player::SetSlot(SlotType type, Record* record, const D3DXVECTOR3& pos, const D3DXQUATERNION& rot)
{
	lsl::SafeDelete(_slot[type]);

	if (record)
	{
		_slot[type] = new Slot(this);
		_slot[type]->SetRecord(record);
		_slot[type]->GetItem().SetPos(pos);
		_slot[type]->GetItem().SetRot(rot);

		if (_car.mapObj)
			_slot[type]->GetItem().OnCreateCar(_car.mapObj);
	}
}

Slot* Player::GetSlotInst(SlotType type)
{
	return _slot[type];
}

Slot* Player::GetSlotInst(Slot::Type type)
{
	for (int i = 0; i < cSlotTypeEnd; ++i)
		if (_slot[i] && _slot[i]->GetType() == type)
			return _slot[i];

	return NULL;
}

void Player::TakeBonus(GameObject* bonus, BonusType type, float value)
{
	MapObjRec* record = bonus->GetMapObj() ? bonus->GetMapObj()->GetRecord() : NULL;
	bonus->Death();	

	switch (type)
	{
		case btMoney:
			_pickMoney += static_cast<int>(value);
			SendEvent(cPlayerPickItem, &MyEventData(Slot::cTypeEnd, btMoney, record));
			break;

		case btCharge:
		{
			typedef std::vector<WeaponItem*> Weapons;

			Weapons weapons;
			for (int i = stHyper; i <= stWeapon4; ++i)
				if (_slot[i])
				{
					WeaponItem* item = &_slot[i]->GetItem<WeaponItem>();

					//только если пушка хотябы отчасти разряжена
					if (item->GetCurCharge() < item->GetCntCharge())
						weapons.push_back(&_slot[i]->GetItem<WeaponItem>());
				}

				if (!weapons.empty())
				{
					int index = static_cast<int>(Round((weapons.size() - 1) * Random()));
					WeaponItem* item = weapons[index];

					int charge = static_cast<int>(std::max(item->GetMaxCharge() * value, 1.0f));
					item->SetCurCharge(std::min(item->GetCurCharge() + charge, item->GetCntCharge()));

					SendEvent(cPlayerPickItem, &MyEventData(item->GetSlot()->GetType(), btCharge, record));
				}
				else
					SendEvent(cPlayerPickItem, &MyEventData(Slot::cTypeEnd, btCharge, record));
				break;
		}

		case btMedpack:
			if (_car.mapObj)
			{
				_car.gameObj->Healt(value);
				SendEvent(cPlayerPickItem, &MyEventData(Slot::cTypeEnd, btMedpack, record));
			}
			break;

		case btImmortal:
			if (_car.mapObj)
			{
				_car.gameObj->Immortal(value);
				SendEvent(cPlayerPickItem, &MyEventData(Slot::cTypeEnd, btImmortal, record));
			}
			break;
	}
}

int Player::GetMoney() const
{
	return _money;
}

void Player::SetMoney(int value)
{
	_money = value;
}

void Player::AddMoney(int value)
{
	SetMoney(_money + value);

#ifdef STEAM_SERVICE
	if (_race && _race->GetGame()->steamService()->isInit() && value > 0 && IsHuman())
		_race->GetGame()->steamService()->steamStats()->AddStat(SteamStats::stMoney, value);
#endif
}

int Player::GetPoints() const
{
	return _points;
}

void Player::SetPoints(int value)
{
	_points = value;
}

void Player::AddPoints(int value)
{
	_points += value;
}

int Player::GetPickMoney() const
{
	return _pickMoney;
}

void Player::ResetPickMoney()
{
	_pickMoney = 0;
}

const D3DXCOLOR& Player::GetColor() const
{
	return _car.color;
}

void Player::SetColor(const D3DXCOLOR& value)
{
	_car.color = value;
	
	ApplyColor();
}

int Player::GetId() const
{
	return _id;
}

void Player::SetId(int value)
{
	_id = value;
}

int Player::GetGamerId() const
{
	return _gamerId;
}

void Player::SetGamerId(int value)
{
	_gamerId = value;
}

unsigned Player::GetNetSlot() const
{
	return _netSlot;
}

void Player::SetNetSlot(unsigned value)
{
	_netSlot = value;
}

const lsl::string& Player::GetNetName() const
{
	return _netName;
}

void Player::SetNetName(const lsl::string& value)
{
	_netName = value;
}

int Player::GetPlace() const
{
	return _place;
}

void Player::SetPlace(int value)
{
	_place = value;
}

bool Player::GetFinished() const
{
	return _finished;
}

void Player::SetFinished(bool value)
{
	_finished = value;

	if (_car.gameObj)
		_car.gameObj->SetImmortalFlag(_finished);
}

unsigned Player::GetCheat() const
{
	return _cheatEnable;
}

void Player::SetCheat(unsigned value)
{
	_cheatEnable = value;
}

void Player::ResetBlock(bool block)
{
	_block = block ? 0.0f : -1.0f;
}

bool Player::IsBlock() const
{
	return _block >= 0.0f;
}

float Player::GetBlockTime() const
{
	return _block;
}

void Player::SetBlockTime(float value)
{
	_block = value;
}

graph::Tex2DResource* Player::GetPhoto()
{
	const Planet::PlayerData* plr = _race->GetTournament().GetPlayerData(_gamerId);
	
	return plr ? plr->photo : NULL;
}

const std::string& Player::GetName()
{
	const Planet::PlayerData* plr = _race->GetTournament().GetPlayerData(_gamerId);

	return !_netName.empty() ? _netName : (plr ? plr->name : scNull);
}

bool Player::IsHuman()
{
	return Race::IsHumanId(_id);
}

bool Player::IsComputer()
{
	return Race::IsComputerId(_id);
}

bool Player::IsOpponent()
{
	return Race::IsOpponentId(_id);
}

}

}