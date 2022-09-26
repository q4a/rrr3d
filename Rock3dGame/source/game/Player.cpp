#include "stdafx.h"
#include "game/World.h"

#include "game/Player.h"

namespace r3d
{
	namespace game
	{
		const float Player::cHumanEasingMinDist[cDifficultyEnd] = {20.0f, 20.0f, 20.0f, 20.0f};
		const float Player::cHumanEasingMaxDist[cDifficultyEnd] = {200.0f, 200.0f, 200.0f, 200.0f};
		const float Player::cHumanEasingMinSpeed[cDifficultyEnd] = {
			95.0f * 1000 / 3600, 110.0f * 1000 / 3600, 125.0f * 1000 / 3600, 125.0f * 1000 / 3600
		};
		const float Player::cHumanEasingMaxSpeed[cDifficultyEnd] = {
			55.0f * 1000 / 3600, 65.0f * 1000 / 3600, 75.0f * 1000 / 3600, 75.0f * 1000 / 3600
		};
		const float Player::cCompCheatMinTorqueK[cDifficultyEnd] = {1.05f, 1.20f, 1.30f, 1.30f};
		const float Player::cCompCheatMaxTorqueK[cDifficultyEnd] = {1.30f, 1.65f, 1.85f, 1.85f};

		const float Player::cHumanArmorK[cDifficultyEnd] = {1.5f, 1.25f, 1.0f, 1.0f};

		const std::string Player::cSlotTypeStr[cSlotTypeEnd] = {
			"stWheel", "stTruba", "stArmor", "stMotor", "stHyper", "stMine", "stWeapon1", "stWeapon2", "stWeapon3",
			"stWeapon4", "stTrans"
		};

		//LEFT:
		const D3DXCOLOR Player::cLeftColors[cColorsCount] = {
			clrWhite, clrBlue, clrSea, clrGay, clrGuy, clrTurquoise, clrEasyViolet
		};
		const D3DXCOLOR Player::cLeftColors2[cColorsCount] = {
			clrBrightBrown, clrOrange, clrPeach, clrKhaki, clrKargo, clrBrightYellow, clrBirch
		};

		//RIGHT:
		const D3DXCOLOR Player::cRightColors[cColorsCount] = {
			clrYellow, clrAcid, clrSalad, clrBrightGreen, clrGreen, clrNeo, clrGray
		};
		const D3DXCOLOR Player::cRightColors2[cColorsCount] = {
			clrBrown, clrCherry, clrRed, clrPink, clrBarbie, clrViolet, clrDarkGray
		};


		Slot::ClassList Slot::classList;


		SlotItem::SlotItem(Slot* slot): _slot(slot), _name(_SC(svNull)), _info(_SC(svNull)), _cost(0), _inf(0),
		                                _damage(0), _id(0), _linkId(0), _modify(false), _hide(false), _pindex(0),
		                                _limit(false), _autoshot(false), _mesh(nullptr), _texture(nullptr),
		                                _pos(NullVector), _rot(NullQuaternion)
		{
		}

		SlotItem::~SlotItem()
		{
			SetMesh(nullptr);
			SetTexture(nullptr);
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
			writer->WriteValue("inf", _inf);
			writer->WriteValue("damage", _damage);
			writer->WriteValue("id", _id);
			writer->WriteValue("linkId", _linkId);
			writer->WriteValue("modify", _modify);
			writer->WriteValue("hide", _hide);
			writer->WriteValue("pindex", _pindex);
			writer->WriteValue("limit", _limit);
			writer->WriteValue("autoshot", _autoshot);
			writer->WriteRef("mesh", _mesh);
			writer->WriteRef("texture", _texture);

			SWriteValue(writer, "pos", _pos);
			SWriteValue(writer, "rot", _rot);
		}

		void SlotItem::Load(SReader* reader)
		{
			reader->ReadValue("name", _name);
			reader->ReadValue("info", _info);
			reader->ReadValue("cost", _cost);
			reader->ReadValue("inf", _inf);
			reader->ReadValue("damage", _damage);
			reader->ReadValue("id", _id);
			reader->ReadValue("linkId", _linkId);
			reader->ReadValue("modify", _modify);
			reader->ReadValue("hide", _hide);
			reader->ReadValue("pindex", _pindex);
			reader->ReadValue("limit", _limit);
			reader->ReadValue("autoshot", _autoshot);
			reader->ReadRef("mesh", true, this, nullptr);
			reader->ReadRef("texture", true, this, nullptr);

			SReadValue(reader, "pos", _pos);
			SReadValue(reader, "rot", _rot);
		}

		void SlotItem::OnFixUp(const FixUpNames& fixUpNames)
		{
			for (auto iter = fixUpNames.begin(); iter != fixUpNames.end(); ++iter)
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

		float SlotItem::DoGetInf() const
		{
			return _inf;
		}

		int SlotItem::DoGetDamage() const
		{
			return _damage;
		}

		int SlotItem::DoGetId() const
		{
			return _id;
		}

		int SlotItem::DoGetLinkId() const
		{
			return _linkId;
		}

		bool SlotItem::DoGetModify() const
		{
			return _modify;
		}

		bool SlotItem::DoGetHide() const
		{
			return _hide;
		}

		int SlotItem::DoGetPIndex() const
		{
			return _pindex;
		}

		bool SlotItem::DoGetLimit() const
		{
			return _limit;
		}

		bool SlotItem::DoGetAutoShot() const
		{
			return _autoshot;
		}

		graph::IndexedVBMesh* SlotItem::DoGetMesh() const
		{
			return _mesh;
		}

		graph::Tex2DResource* SlotItem::DoGetTexture() const
		{
			return _texture;
		}

		ArmoredItem* SlotItem::IsArmoredItem()
		{
			return nullptr;
		}

		WeaponItem* SlotItem::IsWeaponItem()
		{
			return nullptr;
		}

		MobilityItem* SlotItem::IsMobilityItem()
		{
			return nullptr;
		}

		Slot* SlotItem::GetSlot() const
		{
			return _slot;
		}

		Player* SlotItem::GetPlayer() const
		{
			return _slot ? _slot->GetPlayer() : nullptr;
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

		float SlotItem::GetInf() const
		{
			return DoGetInf();
		}

		void SlotItem::SetInf(float value)
		{
			_inf = value;
		}

		int SlotItem::GetDamage() const
		{
			return DoGetDamage();
		}

		void SlotItem::SetDamage(int value)
		{
			_damage = value;
		}

		int SlotItem::GetId() const
		{
			return DoGetId();
		}

		void SlotItem::SetId(int value)
		{
			_id = value;
		}

		int SlotItem::GetLinkId() const
		{
			return DoGetLinkId();
		}

		void SlotItem::SetLinkId(int value)
		{
			_linkId = value;
		}

		bool SlotItem::GetModify() const
		{
			return DoGetModify();
		}

		void SlotItem::SetModify(bool value)
		{
			_modify = value;
		}

		bool SlotItem::IsHide() const
		{
			return DoGetHide();
		}

		void SlotItem::SetHide(bool value)
		{
			_hide = value;
		}

		int SlotItem::GetPIndex() const
		{
			return DoGetPIndex();
		}

		void SlotItem::SetPIndex(int value)
		{
			_pindex = value;
		}

		bool SlotItem::GetLimit() const
		{
			return DoGetLimit();
		}

		void SlotItem::SetLimit(bool value)
		{
			_limit = value;
		}

		bool SlotItem::GetAutoShot() const
		{
			return DoGetAutoShot();
		}

		void SlotItem::SetAutoShot(bool value)
		{
			_autoshot = value;
		}

		graph::IndexedVBMesh* SlotItem::GetMesh() const
		{
			return DoGetMesh();
		}

		void SlotItem::SetMesh(graph::IndexedVBMesh* value)
		{
			if (ReplaceRef(_mesh, value))
				_mesh = value;
		}

		graph::Tex2DResource* SlotItem::GetTexture() const
		{
			return DoGetTexture();
		}

		void SlotItem::SetTexture(graph::Tex2DResource* value)
		{
			if (ReplaceRef(_texture, value))
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

		MobilityItem::CarFunc::CarFunc(): maxTorque(0.0f), maxSpeed(0), tireSpring(0)
		{
		}

		void MobilityItem::CarFunc::WriteTo(SWriter* writer)
		{
			SWriter* wLongTire = writer->NewDummyNode("longTire");
			longTire.WriteTo(wLongTire);

			SWriter* wLatTire = writer->NewDummyNode("latTire");
			latTire.WriteTo(wLatTire);

			writer->WriteValue("maxTorque", maxTorque);
			writer->WriteValue("maxSpeed", maxSpeed);
			writer->WriteValue("tireSpring", tireSpring);
		}

		void MobilityItem::CarFunc::ReadFrom(SReader* reader)
		{
			SReader* rLongTire = reader->ReadValue("longTire");
			if (rLongTire)
				longTire.ReadFrom(rLongTire);

			SReader* rLatTire = reader->ReadValue("latTire");
			if (rLatTire)
				latTire.ReadFrom(rLatTire);

			reader->ReadValue("maxTorque", maxTorque);
			reader->ReadValue("maxSpeed", maxSpeed);
			reader->ReadValue("tireSpring", tireSpring);
		}

		void MobilityItem::Save(SWriter* writer)
		{
			_MyBase::Save(writer);

			SWriter* funcMap = writer->NewDummyNode("carFuncMap");
			int i = 0;

			for (auto iter = carFuncMap.begin(); iter != carFuncMap.end(); ++iter, ++i)
			{
				std::stringstream sstream;
				sstream << "func" << i;
				SWriter* func = funcMap->NewDummyNode(sstream.str().c_str());

				MapObjLib::SaveRecordRef(func, "car", iter->first);
				iter->second.WriteTo(func);
			}
		}

		void MobilityItem::Load(SReader* reader)
		{
			_MyBase::Load(reader);

			carFuncMap.clear();

			SReader* funcMap = reader->ReadValue("carFuncMap");
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


		ArmoredItem::ArmoredItem(Slot* slot): _MyBase(slot)
		{
		}

		ArmoredItem::CarFunc::CarFunc(): life(0.0f)
		{
		}

		void ArmoredItem::CarFunc::WriteTo(SWriter* writer)
		{
			writer->WriteValue("life", life);
		}

		void ArmoredItem::CarFunc::ReadFrom(SReader* reader)
		{
			reader->ReadValue("life", life);
		}

		void ArmoredItem::Save(SWriter* writer)
		{
			_MyBase::Save(writer);

			SWriter* funcMap = writer->NewDummyNode("carFuncMap");
			int i = 0;

			for (auto iter = carFuncMap.begin(); iter != carFuncMap.end(); ++iter, ++i)
			{
				std::stringstream sstream;
				sstream << "func" << i;
				SWriter* func = funcMap->NewDummyNode(sstream.str().c_str());

				MapObjLib::SaveRecordRef(func, "car", iter->first);
				iter->second.WriteTo(func);
			}
		}

		void ArmoredItem::Load(SReader* reader)
		{
			_MyBase::Load(reader);

			carFuncMap.clear();

			SReader* funcMap = reader->ReadValue("carFuncMap");
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

		ArmoredItem* ArmoredItem::IsArmoredItem()
		{
			return this;
		}

		void ArmoredItem::ApplyChanges()
		{
			if (GetPlayer())
				GetPlayer()->ApplyArmored();
		}

		float ArmoredItem::CalcLife(const CarFunc& func)
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


		ArmorItem::ArmorItem(Slot* slot): _MyBase(slot)
		{
		}

		Race* ArmorItem::GetRace() const
		{
			if (GetPlayer())
				return GetPlayer()->GetRace();
			return static_cast<Race*>(GetSlot()->GetRecord()->GetLib()->GetOwner());
		}

		const std::string& ArmorItem::DoGetName() const
		{
			return ArmoredItem::DoGetName();
		}

		const std::string& ArmorItem::DoGetInfo() const
		{
			return ArmoredItem::DoGetInfo();
		}

		int ArmorItem::DoGetCost() const
		{
			return ArmoredItem::DoGetCost();
		}

		graph::IndexedVBMesh* ArmorItem::DoGetMesh() const
		{
			return ArmoredItem::DoGetMesh();
		}

		graph::Tex2DResource* ArmorItem::DoGetTexture() const
		{
			return ArmoredItem::DoGetTexture();
		}

		float ArmorItem::CalcLife(const CarFunc& func)
		{
			return func.life;
		}


		WeaponItem::WeaponItem(Slot* slot): _MyBase(slot), _mapObj(nullptr), _inst(nullptr), _maxCharge(0),
		                                    _weaponBonusC(0), _springBonusC(0), _mineBonusC(0), _cntCharge(0),
		                                    _inflation(1.0f), _curCharge(0), _chargeStep(1), _damage(0), _chargeCost(0)
		{
		}

		WeaponItem::~WeaponItem()
		{
			SafeRelease(_inst);
			SetMapObj(nullptr);
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
			SafeRelease(_inst);
		}

		void WeaponItem::Save(SWriter* writer)
		{
			_MyBase::Save(writer);

			MapObjLib::SaveRecordRef(writer, "mapObj", _mapObj);
			writer->WriteValue("damage", _damage);
			writer->WriteValue("maxCharge", _maxCharge);
			writer->WriteValue("weaponBonusC", _weaponBonusC);
			writer->WriteValue("springBonusC", _springBonusC);
			writer->WriteValue("mineBonusC", _mineBonusC);
			writer->WriteValue("cntCharge", _cntCharge);
			writer->WriteValue("inflation", _inflation);
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
			reader->ReadValue("weaponBonusC", _weaponBonusC);
			reader->ReadValue("springBonusC", _springBonusC);
			reader->ReadValue("mineBonusC", _mineBonusC);
			reader->ReadValue("cntCharge", _cntCharge);
			reader->ReadValue("inflation", _inflation);
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
			if (GetPlayer() != nullptr)
			{
				if (GetPlayer()->GetWeaponBonus())
				{
					if (GetPlayer()->GetSpringBonus())
					{
						if (GetPlayer()->GetMineBonus())
							return _maxCharge + _weaponBonusC + _springBonusC + _mineBonusC;
						return _maxCharge + _weaponBonusC + _springBonusC;
					}
					if (GetPlayer()->GetMineBonus())
						return _maxCharge + _weaponBonusC + _mineBonusC;
					return _maxCharge + _weaponBonusC;
				}
				if (GetPlayer()->GetSpringBonus())
				{
					if (GetPlayer()->GetMineBonus())
						return _maxCharge + _springBonusC + _mineBonusC;
					return _maxCharge + _springBonusC;
				}
				if (GetPlayer()->GetMineBonus())
					return _maxCharge + _mineBonusC;
				return _maxCharge;
			}
			return _maxCharge;
		}

		void WeaponItem::SetMaxCharge(unsigned value)
		{
			_maxCharge = value;
		}

		unsigned WeaponItem::GetWpnBonusCharge()
		{
			return _weaponBonusC;
		}

		void WeaponItem::SetWpnBonusCharge(unsigned value)
		{
			_weaponBonusC = value;
		}

		unsigned WeaponItem::BonusSpringCharge()
		{
			return _springBonusC;
		}

		void WeaponItem::BonusSpringCharge(unsigned value)
		{
			_springBonusC = value;
		}

		unsigned WeaponItem::GetMineBonusCharge()
		{
			return _mineBonusC;
		}

		void WeaponItem::SetMineBonusCharge(unsigned value)
		{
			_mineBonusC = value;
		}

		unsigned WeaponItem::GetCntCharge()
		{
			if (GetPlayer() != nullptr)
			{
				if (_cntCharge == _chargeStep)
				{
					if (GetPlayer()->GetWeaponBonus() == true && GetPlayer()->GetSpringBonus() == true)
					{
						if (GetPlayer()->GetMineBonus())
							return _cntCharge + _weaponBonusC + _springBonusC + _mineBonusC;
						return _cntCharge + _weaponBonusC + _springBonusC;
					}
					if (GetPlayer()->GetWeaponBonus() == true && GetPlayer()->GetSpringBonus() == false)
					{
						if (GetPlayer()->GetMineBonus())
							return _cntCharge + _weaponBonusC + _mineBonusC;
						return _cntCharge + _weaponBonusC;
					}
					if (GetPlayer()->GetWeaponBonus() == false && GetPlayer()->GetSpringBonus() == true)
					{
						if (GetPlayer()->GetMineBonus())
							return _cntCharge + _springBonusC + _mineBonusC;
						return _cntCharge + _springBonusC;
					}
					if (GetPlayer()->GetMineBonus())
						return _cntCharge + _mineBonusC;
					return _cntCharge;
				}
				return _cntCharge;
			}
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

		void WeaponItem::MinusCurCharge(unsigned value)
		{
			_curCharge -= value;
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

			for (auto iter = _wpnDesc.projList.begin(); iter != _wpnDesc.projList.end(); ++iter)
			{
				damage += iter->damage;
				projType = iter->type;
			}

			return damage;
		}

		void WeaponItem::SetDamage(float value)
		{
			//invalid
			_damage = value;
		}

		int WeaponItem::GetChargeCost() const
		{
			//конечная цена зарядов:
			return static_cast<int>(_chargeCost + (_chargeCost * (((_cntCharge / _chargeStep) - 1) * _inflation)));
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
			return _inst ? &_inst->GetGameObj<Weapon>() : nullptr;
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
				else if ((_time += deltaTime) > _repairPeriod)
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
			return damage * ClampValue(1.0f - _reflectValue, 0.0f, 1.0f);
		}


		TransItem::TransItem(Slot* slot): _MyBase(slot)
		{
		}

		Slot::Slot(Player* player): _player(player), _type(cTypeEnd), _item(nullptr), _record(nullptr)
		{
			InitClassList();

			CreateItem(stBase);
		}

		Slot::~Slot()
		{
			SetRecord(nullptr);

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
				classList.Add<TransItem>(stTrans);
			}
		}

		void Slot::FreeItem()
		{
			SafeDelete(_item);
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
				CreateItem(static_cast<Type>(type));
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
			if (ReplaceRef(_record, value))
			{
				_record = value;

				if (_record)
					_record->Load(this);
			}
		}


		Player::Player(Race* race): cTimeRestoreCar(1.5f), _race(race), _carMaxSpeedBase(0), _carTireSpringBase(0),
		                            _timeRestoreCar(0), _timeRespAfter(0), _freezeShotTime(0), _freezeMineTime(0),
		                            _freezeTurnTime(0), _maxBurnTime(0), _invTurnTime(0), _jumpTime(0), _rageTime(0),
		                            _nextBonusProjId(cBonusProjUndef + 1), _headLight(hlmNone), _nightFlare(nullptr),
		                            _reflScene(true), _money(0), _sponsorMoney(0), _points(0), _killstotal(0),
		                            _deadstotal(0), _racesTotal(0), _winsTotal(0), _passTrial(0), _OnSuicide(false),
		                            _skipResults(false), _overBoardD(false), _raceTime(0.0f), _raceSeconds(0.0f),
		                            _raceMSeconds(0), _raceMinutes(0), _spectator(false), _gamer(false), _inRace(false),
		                            _hyperDelay(false), _liveTime(0), _pickMoney(0), _id(cUndefPlayerId), _gamerId(-1),
		                            _subj(false), _inRamp(false), _camstatus(0), _emptyWpn(false),
		                            _netSlot(Race::cDefaultNetSlot), _netName(""), _place(0), _finished(false),
		                            _shotFreeze(false), _mineFreeze(true), _cheatEnable(cCheatDisable), _block(-1.0f),
		                            _recoveryTime(2.0f), _backMoveTime(0)
		{
			ZeroMemory(_lights, sizeof(_lights));
			ZeroMemory(_slot, sizeof(_slot));

			_car.owner = this;
		}

		Player::~Player()
		{
			ClearBonusProjs();

			for (int i = 0; i < cSlotTypeEnd; ++i)
				SafeDelete(_slot[i]);

			SetHeadlight(hlmNone);
			SetCar(nullptr);
			FreeColorMat();
		}

		Player::CarState::CarState(): owner(nullptr), record(nullptr), colorMat(nullptr), color(clrWhite),
		                              mapObj(nullptr), gameObj(nullptr), grActor(nullptr), nxActor(nullptr),
		                              curTile(nullptr), curNode(nullptr), lastNode(nullptr), track(0), numLaps(0)
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
			SetLastNode(nullptr);
			SetCurNode(nullptr);
			SetCurTile(nullptr);
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
			bool newLastNode2 = chLastNode && lastNode && ((lastNode->GetNext() && lastNode->GetNext()->GetPoint()->
					IsFind(curTile)) || lastNode->GetPoint()->IsFind(curTile, lastNode) || lastNode->GetPath()->
				GetFirst()->
				GetPoint()->IsFind(curTile, lastNode->GetPath()->GetFirst()));

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
				if (!moveInverse && moveInverseStart - GetDist() > 40.0f)
				{
					moveInverse = true;
					if (owner->GetCar().gameObj != nullptr)
					{
						if (HUD_STYLE == 3)
							owner->GetCar().gameObj->GetLogic()->TakeBonus(
								owner->GetCar().gameObj, owner->GetCar().gameObj, btWrong, 1.0f, false);
						owner->SendEvent(cPlayerMoveInverse);
					}
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
			return ((lastCorrect || curTile == nullptr) && lastNode) ? lastNode : curTile;
		}

		int Player::CarState::GetPathIndex(bool lastCorrect) const
		{
			WayNode* tile = GetCurTile(lastCorrect);

			if (tile)
			{
				int i = 0;
				Trace* trace = tile->GetPath()->GetTrace();
				for (auto iter = trace->GetPathes().begin(); iter != trace->GetPathes().end(); ++iter, ++i)
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

			Map* map = owner && owner->GetRace() ? owner->GetRace()->GetGame()->GetWorld()->GetMap() : nullptr;

			return map && map->GetTrace().GetPathes().size() > 0
				       ? map->GetTrace().GetPathes().front()->GetLength()
				       : 1.0f;
		}

		float Player::CarState::GetDist(bool lastCorrect) const
		{
			WayNode* tile = GetCurTile(lastCorrect);
			if (tile)
				return tile->GetPath()->GetLength() - (tile->GetTile().GetFinishDist() - tile->GetTile().
					GetLength(pos));

			return 0;
		}

		float Player::CarState::GetLap(bool lastCorectDist) const
		{
			float dist = GetDist(lastCorectDist);

			return numLaps + dist / GetPathLength(lastCorectDist);
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

		void Player::QuickFinish(Player* player)
		{
			_car.numLaps = 8;
			GetRace()->OnLapPass(this);
		}

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
			for (auto iter = _bonusProjs.begin(); iter != _bonusProjs.end(); ++iter)
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
				desc.shadow = headLight == hlFirst && _race->GetWorld()->GetEnv()->GetShadowQuality() ==
					Environment::eqHigh;
				desc.shadowNumSplit = 1;
				desc.shadowDisableCropLight = true;
				desc.nearDist = 1.0f;
				desc.farDist = 50.0f;
				_lights[headLight] = GetGraph()->AddLight(desc);
				_lights[headLight]->AddRef();
				_lights[headLight]->GetSource()->SetType(D3DLIGHT_SPOT);
				_lights[headLight]->GetSource()->SetAmbient(clrBlack);
				_lights[headLight]->GetSource()->SetDiffuse(clrWhite * 1.0f);
				_lights[headLight]->GetSource()->SetPhi(D3DX_PI / 3.0f);
				_lights[headLight]->GetSource()->SetTheta(D3DX_PI / 6.0f);
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
				_lights[headLight] = nullptr;
			}
		}

		void Player::SetLightParent(GraphManager::LightSrc* light, MapObj* mapObj)
		{
			LSL_ASSERT(light);

			light->GetSource()->SetParent(mapObj ? &mapObj->GetGameObj().GetGrActor() : nullptr);
			light->SetEnable(mapObj ? true : false);
		}

		void Player::CreateNightLights(MapObj* mapObj)
		{
			if (_nightFlare == nullptr)
				return;

			_nightFlare->GetNodes().Clear();
			_nightFlare->SetParent(mapObj ? &mapObj->GetGameObj().GetGrActor().GetNodes().front() : nullptr);

			if (mapObj == nullptr)
				return;

			//{D3DXVECTOR3(1.7f, 0.4f, 0.02f), D3DXVECTOR2(1.5f, 1.5f), true},
			//{D3DXVECTOR3(1.7f, -0.4f, 0.02f), D3DXVECTOR2(1.5f, 1.5f), true},
			//{D3DXVECTOR3(-1.57f, 0.45f, 0.28f), D3DXVECTOR2(1.0f, 1.0f), false},
			//{D3DXVECTOR3(-1.57f, -0.45f, 0.28f), D3DXVECTOR2(1.0f, 1.0f), false}

			Garage::Car* car = _race->GetGarage().FindCar(_car.record);
			if (car == nullptr)
				return;

			for (unsigned i = 0; i < car->GetNightLights().size(); ++i)
			{
				graph::Sprite& plane = _nightFlare->GetNodes().Add<graph::Sprite>();
				plane.fixDirection = false;
				plane.sizes = car->GetNightLights()[i].size;
				plane.SetPos(car->GetNightLights()[i].pos);
				plane.material.Set(&_race->GetWorld()->GetResManager()->GetMatLib().Get(
					car->GetNightLights()[i].head ? "Effect\\flare7White" : "Effect\\flare7Red"));
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

			SetLightsParent(nullptr);

			_car.nxActor = nullptr;
			SafeRelease(_car.grActor);
			SafeRelease(_car.gameObj);
			SafeRelease(_car.mapObj);
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
				longTireDesc.asymptoteSlip = longTireDesc.asymptoteValue = longTireDesc.extremumSlip = longTireDesc.
					extremumValue = longTireDesc.stiffnessFactor = 0.0f;

				NxTireFunctionDesc latTireDesc = car->GetWheels().front().GetShape()->GetLateralTireForceFunction();
				latTireDesc.asymptoteSlip = latTireDesc.asymptoteValue = latTireDesc.extremumSlip = latTireDesc.
					extremumValue = latTireDesc.stiffnessFactor = 0.0f;

				float maxSpeed = 0.0f;
				float tireSpring = _carTireSpringBase;

				for (int i = 0; i < cSlotTypeEnd; ++i)
				{
					MobilityItem* mobi = _slot[i] ? _slot[i]->GetItem().IsMobilityItem() : nullptr;
					MobilityItem::CarFunc* carFunc = nullptr;
					if (mobi)
					{
						auto iter = mobi->carFuncMap.find(record);
						if (iter != mobi->carFuncMap.end())
							carFunc = &iter->second;
					}
					if (carFunc)
					{
						motorDesc.maxTorque += carFunc->maxTorque;
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

					const unsigned int basicRPM = motorDesc.maxRPM;
					const float basicTorque = motorDesc.maxTorque;
					motorDesc.maxRPM = basicRPM + ((basicRPM * GetBonusRPM()) / 100);
					//дополнительный % макс. оборотов двигателя
					motorDesc.maxTorque = basicTorque + ((basicTorque * GetBonusTorque()) / 100);
					//дополнительный % макс. момента двигателя

					//задать параметры мотора для персонажа персонально:
					motorDesc.SEM = GetSEM();
					motorDesc.brakeTorque = GetBrakeTorque();
					motorDesc.restTorque = GetRestTorque();
					motorDesc.idlingRPM = GetIdlingRPM();
					motorDesc.gearDiff = GetGearDiff();
				}

				car->SetMotorDesc(motorDesc);

				for (auto iter = car->GetWheels().begin(); iter != car->GetWheels().end(); ++iter)
				{
					(*iter)->GetShape()->SetLongitudalTireForceFunction(longTireDesc);
					(*iter)->GetShape()->SetLateralTireForceFunction(latTireDesc);
				}
				//Дополнительный % скорости берём из TopSpeed стата:
				const float basicSpeed = maxSpeed + _carMaxSpeedBase;
				const float speedBonus = basicSpeed + (basicSpeed * GetBonusSpeed() / 100);

				car->SetMaxSpeed(speedBonus);
				car->SetTireSpring(tireSpring);
			}
		}

		void Player::ApplyArmored()
		{
			MapObjRec* record = _car.record;
			GameCar* car = _car.gameObj;

			if (record && car)
			{
				float maxLife = 0.0f;

				for (int i = 0; i < cSlotTypeEnd; ++i)
				{
					ArmoredItem* arm = _slot[i] ? _slot[i]->GetItem().IsArmoredItem() : nullptr;
					ArmoredItem::CarFunc* carFunc = nullptr;
					if (arm)
					{
						auto iter = arm->carFuncMap.find(record);
						if (iter != arm->carFuncMap.end())
							carFunc = &iter->second;
					}
					if (carFunc)
					{
						maxLife += arm->CalcLife(*carFunc);
					}
				}

				if (_race && _race->GetProfile() && (IsHuman() || IsOpponent()))
					maxLife *= cHumanArmorK[_race->GetProfile()->difficulty()];

				const float bonusArmor = (maxLife / 100) * this->GetArmorBonus(); //дополнительный % брони
				car->SetMaxLife(maxLife + bonusArmor);
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
				_car.colorMat = nullptr;
			}
		}

		void Player::ApplyColorMat()
		{
			FreeColorMat();

			if (_car.mapObj && _car.gameObj->GetGrActor().GetNodes().Size() > 0 && _car.gameObj->GetGrActor().GetNodes()
				.begin()->GetType() == graph::Actor::ntIVBMesh && !_car.gameObj->GetDisableColor())
			{
				auto& mesh = static_cast<graph::IVBMeshNode&>(_car.gameObj->GetGrActor().GetNodes().front());
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
			Player* opponent = nullptr;
			float maxLapDist = 0;
			_car.cheatFaster = false;
			_car.cheatSlower = false;

			if (_cheatEnable)
			{
				for (auto iter = _race->GetPlayerList().begin(); iter != _race->GetPlayerList().end(); ++iter)
				{
					Player* player = *iter;
					if (this != player && (player->GetId() == Race::cHuman || (player->GetId() & Race::cOpponentMask)))
					{
						float dist = player->GetCar().GetLap() - _car.GetLap();
						if (maxLapDist < dist || opponent == nullptr)
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
						float speedLimit = ClampValue(
							(dist - cHumanEasingMinDist[diff]) / (cHumanEasingMaxDist[diff] - cHumanEasingMinDist[
								diff]), 0.0f, 1.0f);
						speedLimit = cHumanEasingMinSpeed[diff] + (cHumanEasingMaxSpeed[diff] - cHumanEasingMinSpeed[
							diff]) * speedLimit;

						if (_car.speed > speedLimit)
							_car.cheatSlower = true;
					}
				}
				//human ahead
				else if (dist > cHumanEasingMinDist[diff])
				{
					if (_cheatEnable & cCheatEnableFaster)
					{
						float torqueK = ClampValue(
							(dist - cHumanEasingMinDist[diff]) / (cHumanEasingMaxDist[diff] - cHumanEasingMinDist[
								diff]), 0.0f, 1.0f);
						torqueK = cCompCheatMinTorqueK[diff] + (cCompCheatMaxTorqueK[diff] - cCompCheatMinTorqueK[diff])
							* torqueK;

						_car.cheatFaster = true;
						SetCheatK(_car, torqueK, torqueK);
					}
				}
			}

			if (!_car.cheatFaster)
				SetCheatK(_car, 1, 1);
		}

		void Player::SetCheatK(const CarState& car, float torqueK, float steerK)
		{
			if (car.gameObj == nullptr)
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
			if (!_car.lastNode && !GetMap()->GetTrace().GetPathes().empty() && GetMap()->GetTrace().GetPathes().front()
				->GetCount() > 0)
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
					sender->GetMapObj()->GetPlayer()->SetOverBoardD(true);
					break;

				case dtMine:
					SendEvent(cPlayerDeathMine);
					break;
				}

				sender->GetMapObj()->GetPlayer()->AddDeadsTotal(1);
				SendEvent(cPlayerDeath,
				          &MyEventData(Slot::cTypeEnd, cBonusTypeEnd, nullptr, sender->GetTouchPlayerId(), damageType));
			}
		}

		void Player::OnLapPass()
		{
			++_car.numLaps;
			ReloadWeapons();

			_race->OnLapPass(this);
		}

		void Player::ResetMaxSpeed()
		{
			MapObjRec* record = _car.record;
			GameCar* car = _car.gameObj;
			float maxSpeed = 0.0f;

			if (record && car)
			{
				for (int i = 0; i < cSlotTypeEnd; ++i)
				{
					MobilityItem* mobi = _slot[i] ? _slot[i]->GetItem().IsMobilityItem() : nullptr;
					MobilityItem::CarFunc* carFunc = nullptr;
					if (mobi)
					{
						auto iter = mobi->carFuncMap.find(record);
						if (iter != mobi->carFuncMap.end())
							carFunc = &iter->second;
					}
					if (carFunc)
					{
						maxSpeed = std::max(carFunc->maxSpeed, maxSpeed);
					}
				}
				//Дополнительный % скорости:
				const float basicSpeed = maxSpeed + _carMaxSpeedBase;
				const float speedBonus = basicSpeed + (basicSpeed * GetBonusSpeed() / 100);

				car->SetMaxSpeed(speedBonus);
			}
		}

		void Player::OnProgress(float deltaTime)
		{
			_freezeMineTime += deltaTime;
			_freezeTurnTime += deltaTime;
			_invTurnTime += deltaTime;

			if (_car.gameObj && _car.gameObj->OnJump())
				_jumpTime += deltaTime;
			else
				_jumpTime = 0;

			//теперь возможно не будет бага с отображением щита:
			if (_car.gameObj && _car.gameObj->IsImmortal() && _finished == false)
			{
				_car.gameObj->GetLogic()->Damage(_car.gameObj, GetId(), _car.gameObj, 0, _car.gameObj->dtPowerShell);
			}

			if (_finished == false && !this->IsBlock())
			{
				_raceTime += deltaTime;
				_raceSeconds += deltaTime;
				_raceMSeconds += (deltaTime * 10);

				if (_raceSeconds >= 60)
				{
					_raceSeconds = 0;
					_raceMinutes += 1;
				}

				if (_raceMSeconds >= 10)
					_raceMSeconds = 0;
			}

			if (_car.gameObj && _car.gameObj->InRage())
			{
				_rageTime += deltaTime;
				_car.gameObj->GetLogic()->Damage(_car.gameObj, GetId(), _car.gameObj, 0, _car.gameObj->dtRage);
				if (this->GetRageTime() >= 5.0f)
				{
					_rageTime = 0;
					_car.gameObj->InRage(false);
				}
			}
			else
				_rageTime = 0;

			//разблокируем второй прыжок после приземления с рампы
			if (_car.gameObj && _car.gameObj->IsAnyWheelContact() && _car.gameObj->OnJump() && inRamp())
				inRamp(false);

			//разблокируем прыжок, если машина после него приземлилась хотябы одним колесом:
			if (_car.gameObj && _jumpTime > 0.2f && _car.gameObj->IsAnyWheelContact())
			{
				_car.gameObj->OnJump(false);
				_car.gameObj->LockAIJump(false);
			}


			if (_car.gameObj)
			{
				if (_car.gameObj->Invisible())
					_car.grActor->SetVisible(false);
				else
					_car.grActor->SetVisible(true);
			}


			//запускаем таймер после респавна:
			if (_car.gameObj && _car.grActor && _car.gameObj->GetGhostEff() == true)
			{
				_timeRespAfter += deltaTime;
				//останавливаем таймер:
				if (_timeRespAfter >= 0)
				{
					_car.gameObj->SetGhostEff(false);
					_car.grActor->SetVisible(true);
				}
				else
				{
					//эффект "призрак":
					if (this->IsBlock() == false && _finished == false)
					{
						if (_timeRespAfter > -2.9 && _timeRespAfter < -2.7 || _timeRespAfter > -2.6 && _timeRespAfter <
							-2.4 || _timeRespAfter > -2.3 && _timeRespAfter < -2.1 || _timeRespAfter > -2.0 &&
							_timeRespAfter < -1.8 || _timeRespAfter > -1.7 && _timeRespAfter < -1.5)
							_car.gameObj->Invisible(true);
						else
							_car.gameObj->Invisible(false);
					}
					else
						_car.gameObj->Invisible(false);
				}
			}

			//таймер заморозки стрельбы (отдельно от таймера шокера):
			if (GetShotFreeze() == true)
			{
				_freezeShotTime += deltaTime;
				if (_freezeShotTime > 2)
				{
					SetShotFreeze(false);
					_freezeShotTime = 0.0f;
				}
			}

			if (GetMineFreeze() == true && _freezeMineTime > 1.5f)
			{
				SetMineFreeze(false);
				_freezeMineTime = 0.0f;
			}

			if (_car.mapObj)
			{
				_liveTime += deltaTime;
				if (_car.gameObj->GetTurnFreeze() == true && _freezeTurnTime > 2.0f)
				{
					_car.gameObj->SetTurnFreeze(false);
					_freezeTurnTime = 0.0f;
				}

				//время возрождения, если есть навык регенерации/ либо машина вылетела за трассу меньше.
				if (this->GetFixRecovery() || this->GetOverBoardD())
					_recoveryTime = cTimeRestoreCar;
				else
				{
					//время на возрождение зависит от позиции в гонке:
					if (GetPlace() == 1)
						_recoveryTime = 2.5;
					else if (GetPlace() == 2)
						_recoveryTime = 2.25;
					else if (GetPlace() == 3)
						_recoveryTime = 2.0;
					else if (GetPlace() == 4)
						_recoveryTime = 2.0;
					else if (GetPlace() == 4)
						_recoveryTime = 1.75;
					else
						_recoveryTime = 1.5;
				}

				//время действия шокера, указывать в воркшопе (_desc.mass):
				if (_car.gameObj && _car.gameObj->GetTurnInverse() == true && _invTurnTime > _car.gameObj->
					GetInverseTime())
				{
					_car.gameObj->SetTurnInverse(false);
					_invTurnTime = 0.0f;

					//время заморозки стрельбы шокером равно времени заморозки поворотов:
					if (GetShotFreeze() == true && _freezeShotTime > _car.gameObj->GetInverseTime())
					{
						SetShotFreeze(false);
						_freezeShotTime = 0.0f;
					}
				}
				if (_car.gameObj->IsBurn() == true)
				{
					_maxBurnTime += deltaTime;
					if (_maxBurnTime < 0)
						_car.gameObj->GetLogic()->Damage(_car.gameObj, GetId(), _car.gameObj,
						                                 _car.gameObj->GetBurnDamage() * deltaTime,
						                                 _car.gameObj->dtNull);
					else
					{
						_maxBurnTime = 0;
						_car.gameObj->SetBurn(false);
					}
				}

				//Если машина медленно едет:
				if (_car.gameObj->GetSlowRide() == true)
				{
					//Запускаем таймер (он действует столько секунд, сколько указано в воркшопе)
					_lowSpeedTime += deltaTime;
					if (_lowSpeedTime > _car.gameObj->GetMaxTimeSR())
					{
						_car.gameObj->GetMapObj()->GetPlayer()->ResetMaxSpeed();
						_car.gameObj->SetSlowRide(false);
					}
				}
				else
				{
					//Обнуляем таймер:
					_lowSpeedTime = 0.0f;
				}

				//время разминирования
				if (_car.gameObj->GetDemining() == true)
				{
					_DemTime += deltaTime;
					if (_DemTime > _car.gameObj->GetDeminingTime())
					{
						_car.gameObj->SetDemining(false);
					}
				}
				else
				{
					//Обнуляем таймер:
					_DemTime = 0.0f;
				}

				_car.Update(deltaTime);

				CheatUpdate(deltaTime);
			}
			else
			{
				_timeRestoreCar += deltaTime;

				if (IsGamer() && _timeRestoreCar > _recoveryTime)
				{
					_timeRestoreCar = 0;
					CreateCar(false);
					ResetCar();
					SetOverBoardD(false);
					_car.gameObj->SetGhostEff(false);
					_timeRespAfter = 0;
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

			//фикс машин после финиша + эффект затемнения.
			if (this->GetFinished())
			{
				_timeAfterFinish += deltaTime;
				if (_timeAfterFinish < 3.0f)
				{
					if (this->GetCar().gameObj->GetLastMoveState() == 1)
						this->GetCar().gameObj->SetMoveCar(GameCar::mcBrake);
					if (IsHuman() && IsGamer())
					{
						//Плавное затемнение экрана в изометрии по завершению гонки:
						GraphManager::HDRParams params = GetRace()->GetWorld()->GetGraph()->GetHDRParams();
						GraphManager::HDRParams newparams;
						newparams.lumKey = params.lumKey;
						newparams.brightThreshold = params.brightThreshold;
						newparams.gaussianScalar = params.gaussianScalar;
						newparams.exposure = params.exposure;
						newparams.colorCorrection = D3DXVECTOR2(1.0f - (_timeAfterFinish / 3), 0.0f);

						if (GetRace()->GetWorld()->GetCamera()->GetStyle() == CameraManager::csIsometric)
							GetRace()->GetWorld()->GetGraph()->SetHDRParams(newparams);
					}
				}
				else
				{
					this->GetCar().gameObj->SetMoveCar(GameCar::mcNone);
					this->GetCar().gameObj->SetLastMoveState(0);
					_timeAfterFinish = 0;
				}
			}

			if (IsHuman() && IsSpectator() && InRace())
			{
				//автоматическое перключение субъектов
				if (GetRace()->GetGame()->subjectView() == 9)
				{
					for (auto iter = GetRace()->GetPlayerList().begin(); iter != GetRace()->GetPlayerList().end(); ++
					     iter)
					{
						if ((*iter)->IsGamer() && (*iter)->GetCar().gameObj != nullptr && (*iter)->GetCar().gameObj->
							GetSpeed() > 0)
						{
							Player* current = GetRace()->GetWorld()->GetCamera()->GetPlayer();
							//на последнем круге смотрим за лидером
							if (current->GetCar().numLaps == current->GetRace()->GetTournament().GetCurTrack().numLaps -
								1)
							{
								if ((*iter)->GetPlace() == 1)
								{
									if (GetRace()->GetWorld()->GetCamera()->GetStyle() == CameraManager::csFreeView)
										GetRace()->GetWorld()->GetCamera()->FlyTo(
											(*iter)->GetCar().gameObj->GetPos() + ZVector * 9,
											(*iter)->GetCar().gameObj->GetRot(), 0.1f, true, true);
									else
									{
										if (GetRace()->GetWorld()->GetCamera()->GetPlayer() != (*iter))
										{
											if (GetRace()->GetWorld()->GetCamera()->GetStyle() ==
												CameraManager::csThirdPerson)
												GetRace()->GetWorld()->GetCamera()->FlyTo(
													(*iter)->GetCar().gameObj->GetPos() + ZVector * 2,
													(*iter)->GetCar().gameObj->GetRot(), 0.1f, true, true);
											GetRace()->GetWorld()->GetCamera()->SetPlayer((*iter));
										}
									}
								}
							}
							else
							{
								//в остальных кругах смотрим за вторым игроком
								if ((*iter)->GetPlace() == 2 && GetRace()->GetWorld()->GetCamera()->GetPlayer() != (*
									iter))
								{
									if (GetRace()->GetWorld()->GetCamera()->GetStyle() == CameraManager::csFreeView)
										GetRace()->GetWorld()->GetCamera()->FlyTo(
											(*iter)->GetCar().gameObj->GetPos() + ZVector * 9,
											(*iter)->GetCar().gameObj->GetRot(), 0.1f, true, true);
									else
									{
										if (GetRace()->GetWorld()->GetCamera()->GetPlayer() != (*iter))
										{
											if (GetRace()->GetWorld()->GetCamera()->GetStyle() ==
												CameraManager::csThirdPerson)
												GetRace()->GetWorld()->GetCamera()->FlyTo(
													(*iter)->GetCar().gameObj->GetPos() + ZVector * 2,
													(*iter)->GetCar().gameObj->GetRot(), 0.1f, true, true);
											GetRace()->GetWorld()->GetCamera()->SetPlayer((*iter));
										}
									}
								}
							}
						}
					}
				}
			}
		}

		Player* Player::FindClosestEnemy(float viewAngle, bool zTest)
		{
			//нет смысла искать противников
			if (!_car.mapObj)
				return nullptr;

			float minDist = 0;
			Player* enemy = nullptr;

			for (auto iter = _race->GetPlayerList().begin(); iter != _race->GetPlayerList().end(); ++iter)
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
					bool b1 = enemy == nullptr || absDist < minDist;
					//Объект расположен с правильной стороны относительно машины
					bool b2 = viewAngle == 0.0f || (viewAngle > 0
						                                ? (angle >= cos(viewAngle))
						                                : (angle <= cos(D3DX_PI / 2 - viewAngle)));

					if (b1 && b2)
					{
						//Боты стреляют только в человека:
						/*if (tPlayer->IsComputer() == false)
							enemy = tPlayer;
						else
							enemy = NULL;*/

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
				_freezeTurnTime = 0;
				_freezeShotTime = 0;
				_freezeMineTime = 0;
				_maxBurnTime = 0;
				_invTurnTime = 0;
				_jumpTime = 0;
				_rageTime = 0;
				_backMoveTime = 0;

				this->SetHyperDelay(false);
				this->SetCarLiveTime(0);

				_car.mapObj = &GetMap()->AddMapObj(_car.record);
				_car.mapObj->AddRef();
				_car.mapObj->SetPlayer(this);
				_car.gameObj = &_car.mapObj->GetGameObj<RockCar>();
				_car.gameObj->AddRef();
				_car.grActor = &_car.gameObj->GetGrActor();
				_car.grActor->AddRef();
				_car.nxActor = _car.gameObj->GetPxActor().GetNxActor();

				_car.kSteerControl = _car.gameObj->GetKSteerControl();
				_car.size = ComputeCarBBSize();
				_car.radius = _car.size / 2.0f;
				_car.moveInverse = false;
				_car.moveInverseStart = -1;
				_car.maxSpeed = 0;
				_car.maxSpeedTime = 0;

				_car.gameObj->SetImmortalFlag(_finished);
				_car.gameObj->SetShellFlag(_finished);
				_car.gameObj->InsertListener(this);
				_car.gameObj->SetSpinStatus(false);
				_car.gameObj->SetWastedControl(false);
				_car.gameObj->SetBurn(false);
				_car.gameObj->SetTurnFreeze(false);
				_car.gameObj->SetTurnInverse(false);
				_car.gameObj->SetInverseTime(0);
				_car.gameObj->SetLastMoveState(0);
				_car.gameObj->SetRespBlock(false);
				_car.gameObj->SetDamageStop(false);
				_car.gameObj->SetDemining(false);
				_car.gameObj->OnJump(false);
				_car.gameObj->LockAIJump(false);
				_car.gameObj->InFly(false);
				_car.gameObj->InRage(false);
				_car.gameObj->Invisible(false);
				_car.gameObj->SetSlowRide(false);
				_car.gameObj->StabilityMine(GetStabilityMine());
				_car.gameObj->StabilityShot(GetStabilityShot());

				_carMaxSpeedBase = _car.gameObj->GetMaxSpeed();
				_carTireSpringBase = _car.gameObj->GetTireSpring();

				SetLightsParent(_car.mapObj);
				ApplyReflScene();

				for (int i = 0; i < cSlotTypeEnd; ++i)
					if (_slot[i])
						_slot[i]->GetItem().OnCreateCar(_car.mapObj);

				ApplyColorMat();
				ApplyMobility();
				ApplyArmored();
			}

			if (newRace)
			{
				WayNode* node = GetMap()->GetTrace().GetPathes().front()->GetFirst();
				_car.SetCurTile(node);
				_car.SetCurNode(node);
				_car.SetLastNode(node);
				ClearBonusProjs();
				_timeRestoreCar = 0;
				_mineFreeze = true;
				_shotFreeze = false;
				_freezeShotTime = 0;
				_freezeMineTime = 0;
				_freezeTurnTime = 0;
				_nextBonusProjId = cBonusProjUndef + 1;
				SetShotFreeze(false);

				if (IsSpectator())
				{
					_car.gameObj->GetGrActor().SetVisible(false);
				}
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
				_car.SetCurTile(nullptr);
				_car.SetCurNode(nullptr);
				_car.SetLastNode(nullptr);
				_car.numLaps = 0;
			}
		}

		void Player::ResetCar()
		{
			WayNode* lastNode = GetLastNode();

			if (lastNode && _car.mapObj && _car.gameObj->GetRespBlock() == false && _car.gameObj->GetGhostEff() == false
				&& IsGamer())
			{
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
						D3DXVECTOR3 rayPos = node->GetTile().GetPoint(coordX) + ZVector * node->GetTile().
							ComputeHeight(0.5f) * 0.5f;

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
						NxShape* hitShape = _car.gameObj->GetPxActor().GetScene()->GetNxScene()->raycastClosestShape(
							nxRay, NX_ALL_SHAPES, hit,
							1 << px::Scene::cdgTrackPlane | 1 << px::Scene::cdgPlaneDeath | 1 << px::Scene::cdgDefault,
							NX_MAX_F32, NX_RAYCAST_SHAPE);
						GameObject* hitGameObj = GameObject::GetGameObjFromShape(hitShape);

						if (!isDeathPlane && i == 0 && j == 0 && (hitShape == nullptr || hitShape->getGroup() ==
							px::Scene::cdgPlaneDeath))
						{
							isDeathPlane = true;
							distX = 0.0f;
							j = -1;
						}
						else if ((hitShape == nullptr || hitShape->getGroup() != px::Scene::cdgTrackPlane) && hitGameObj
							!= _car.gameObj)
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

				_timeRespAfter = -GetCoolDown();
				_maxBurnTime = 0;
				this->SetCarLiveTime(0);

				_car.gameObj->SetWastedControl(false);
				_car.gameObj->SetBurn(false);
				_car.gameObj->SetTurnFreeze(false);
				_car.gameObj->SetDamageStop(true);
				_car.gameObj->SetDamageStop(false);
				_car.gameObj->SetLastMoveState(0);
				_car.gameObj->SetSpinStatus(false);
				_car.gameObj->SetDemining(false);
				_car.gameObj->OnJump(true);
				_car.gameObj->InFly(false);
				_car.gameObj->InRage(false);
				_car.gameObj->Invisible(false);
				//_car.gameObj->SetSlowRide(false);
				_car.gameObj->SetGhostEff(true);

				for (int i = 0; i < cSlotTypeEnd; ++i)
					if (_slot[i])
						_slot[i]->GetItem().OnCreateCar(_car.mapObj);
			}
		}

		float Player::GetTimeRespAfter()
		{
			return _timeRespAfter;
		}

		void Player::SetTimeRespAfter(float value)
		{
			_timeRespAfter = value;
		}

		float Player::GetFreezeShotTime()
		{
			return _freezeShotTime;
		}

		float Player::GetFreezeMineTime()
		{
			return _freezeMineTime;
		}

		void Player::SetFreezeMineTime(float value)
		{
			_freezeMineTime = value;
		}

		float Player::GetFreezeTurnTime()
		{
			return _freezeTurnTime;
		}

		void Player::SetFreezeTurnTime(float value)
		{
			_freezeTurnTime = value;
		}

		float Player::GetMaxBurnTime()
		{
			return _maxBurnTime;
		}

		void Player::SetMaxBurnTime(float value)
		{
			_maxBurnTime = value;
		}

		float Player::GetInverseTurnTime()
		{
			return _invTurnTime;
		}

		void Player::SetInverseTurnTime(float value)
		{
			_invTurnTime = value;
		}

		float Player::GetJumpTime()
		{
			return _jumpTime;
		}

		void Player::SetJumpTime(float value)
		{
			_jumpTime = value;
		}

		float Player::GetRageTime()
		{
			return _rageTime;
		}

		void Player::SetRageTime(float value)
		{
			_rageTime = value;
		}

		bool Player::Shot(const Proj::ShotContext& ctx, SlotType type, unsigned projId, int newCharge,
		                  Weapon::ProjList* projList)
		{
			LSL_ASSERT(_slot[type]);

			WeaponItem& item = _slot[type]->GetItem<WeaponItem>();
			Proj* lastProj = nullptr;
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

		unsigned Player::GetBonusProjId(Proj* proj)
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
			return nullptr;
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
							FreeLight(static_cast<HeadLight>(i));
						break;
					}

				case hlmOne:
					{
						InitLight(hlFirst, D3DXVECTOR3(0.3f, 0.0f, 3.190f),
						          D3DXQUATERNION(0.0009f, 0.344f, -0.029f, 0.939f));
						FreeLight(hlSecond);
						break;
					}

				case hlmTwo:
					{
						InitLight(hlFirst, D3DXVECTOR3(0.3f, 1.0f, 3.190f),
						          D3DXQUATERNION(0.0009f, 0.344f, -0.029f, 0.939f));
						InitLight(hlSecond, D3DXVECTOR3(0.3f, -1.0f, 3.190f),
						          D3DXQUATERNION(0.0009f, 0.344f, -0.029f, 0.939f));
						break;
					}
				}

				if (_nightFlare == nullptr && _headLight != hlmNone)
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
					SafeDelete(_nightFlare);
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
			return _slot[type] ? _slot[type]->GetRecord() : nullptr;
		}

		void Player::SetSlot(SlotType type, Record* record, const D3DXVECTOR3& pos, const D3DXQUATERNION& rot)
		{
			SafeDelete(_slot[type]);

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

			return nullptr;
		}

		void Player::TakeBonus(GameObject* bonus, BonusType type, float value, bool destroy)
		{
			MapObjRec* record = bonus->GetMapObj() ? bonus->GetMapObj()->GetRecord() : nullptr;
			if (destroy == true)
				bonus->Death();

			switch (type)
			{
			case btMoney:
				_pickMoney += 1000;
				SendEvent(cPlayerPickItem, &MyEventData(Slot::cTypeEnd, btMoney, record));
				break;

			case btCharge:
				{
					using Weapons = std::vector<WeaponItem*>;

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

			case btRage:
				if (_car.mapObj)
				{
					_car.gameObj->InRage(true);
					SendEvent(cPlayerPickItem, &MyEventData(Slot::cTypeEnd, btRage, record));
				}
				break;

			case btWrong:
				if (_car.mapObj)
				{
					SendEvent(cPlayerPickItem, &MyEventData(Slot::cTypeEnd, btWrong, record));
				}
				break;

			case btLastLap:
				if (_car.mapObj)
				{
					SendEvent(cPlayerPickItem, &MyEventData(Slot::cTypeEnd, btLastLap, record));
				}
				break;

			case btLucky:
				if (_car.mapObj)
				{
					SendEvent(cPlayerPickItem, &MyEventData(Slot::cTypeEnd, btLucky, record));
				}
				break;

			case btUnLucky:
				if (_car.mapObj)
				{
					SendEvent(cPlayerPickItem, &MyEventData(Slot::cTypeEnd, btUnLucky, record));
				}
				break;

			case btEmpty:
				if (_car.mapObj)
				{
					SendEvent(cPlayerPickItem, &MyEventData(Slot::cTypeEnd, btEmpty, record));
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

		int Player::GetSponsorMoney() const
		{
			return _sponsorMoney;
		}

		void Player::SetSponsorMoney(int value)
		{
			_sponsorMoney = value;
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

		int Player::GetKillsTotal() const
		{
			return _killstotal;
		}

		void Player::SetKillsTotal(int value)
		{
			_killstotal = value;
		}

		void Player::AddKillsTotal(int value)
		{
			_killstotal += value;
		}

		int Player::GetDeadsTotal() const
		{
			return _deadstotal;
		}

		void Player::SetDeadsTotal(int value)
		{
			_deadstotal = value;
		}

		void Player::AddDeadsTotal(int value)
		{
			_deadstotal += value;
		}

		int Player::GetRacesTotal() const
		{
			return _racesTotal;
		}

		void Player::SetRacesTotal(int value)
		{
			_racesTotal = value;
		}

		void Player::AddRacesTotal(int value)
		{
			_racesTotal += value;
		}

		int Player::GetWinsTotal() const
		{
			return _winsTotal;
		}

		void Player::SetWinsTotal(int value)
		{
			_winsTotal = value;
		}

		void Player::AddWinsTotal(int value)
		{
			_winsTotal += value;
		}

		int Player::GetPassTrial() const
		{
			return _passTrial;
		}

		void Player::SetPassTrial(int value)
		{
			_passTrial = value;
		}

		void Player::AddPassTrial(int value)
		{
			_passTrial += value;
		}

		bool Player::GetSuicide() const
		{
			return _OnSuicide;
		}

		void Player::SetSuicide(bool value)
		{
			_OnSuicide = value;
		}

		bool Player::GetAutoSkip() const
		{
			return _skipResults;
		}

		void Player::SetAutoSkip(bool value)
		{
			_skipResults = value;
		}

		bool Player::GetOverBoardD() const
		{
			return _overBoardD;
		}

		void Player::SetOverBoardD(bool value)
		{
			_overBoardD = value;
		}

		float Player::GetRaceTime() const
		{
			return _raceTime;
		}

		void Player::SetRaceTime(float value)
		{
			_raceTime = value;
		}

		float Player::GetRaceSeconds() const
		{
			return _raceSeconds;
		}

		void Player::SetRaceSeconds(float value)
		{
			_raceSeconds = value;
		}

		float Player::GetRaceMSeconds() const
		{
			return _raceMSeconds;
		}

		void Player::SetRaceMSeconds(float value)
		{
			_raceMSeconds = value;
		}

		int Player::GetRaceMinutes() const
		{
			return _raceMinutes;
		}

		void Player::SetRaceMinutes(int value)
		{
			_raceMinutes = value;
		}

		bool Player::IsSpectator() const
		{
			return _spectator;
		}

		void Player::IsSpectator(bool value)
		{
			_spectator = value;
		}

		bool Player::IsGamer() const
		{
			return _gamer;
		}

		void Player::IsGamer(bool value)
		{
			_gamer = value;
		}


		bool Player::InRace() const
		{
			return _inRace;
		}

		void Player::InRace(bool value)
		{
			_inRace = value;
		}

		bool Player::GetHyperDelay() const
		{
			return _hyperDelay;
		}

		void Player::SetHyperDelay(bool value)
		{
			_hyperDelay = value;
		}

		float Player::GetCarLiveTime() const
		{
			return _liveTime;
		}

		void Player::SetCarLiveTime(float value)
		{
			_liveTime = value;
		}

		int Player::GetPickMoney() const
		{
			return _pickMoney;
		}

		void Player::ResetPickMoney()
		{
			_pickMoney = 0;
		}

		void Player::AddPickMoney(int value)
		{
			_pickMoney += value;
		}

		const D3DXCOLOR& Player::GetColor() const
		{
			//для наблюдателей ставим всегда черный цвет:
			if (GetGamerId() >= SPECTATOR_ID_BEGIN)
				return clrBlack;
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

		bool Player::isSubject() const
		{
			return _subj;
		}

		void Player::isSubject(bool value)
		{
			_subj = value;
		}

		bool Player::inRamp() const
		{
			return _inRamp;
		}

		void Player::inRamp(bool value)
		{
			_inRamp = value;
		}

		int Player::GetCameraStatus() const
		{
			return _camstatus;
		}

		void Player::SetCameraStatus(int value)
		{
			_camstatus = value;
		}

		bool Player::IsEmptyWpn() const
		{
			return _emptyWpn;
		}

		void Player::IsEmptyWpn(bool value)
		{
			_emptyWpn = value;
		}

		unsigned Player::GetNetSlot() const
		{
			return _netSlot;
		}

		void Player::SetNetSlot(unsigned value)
		{
			_netSlot = value;
		}

		const string& Player::GetNetName() const
		{
			return _netName;
		}

		void Player::SetNetName(const string& value)
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
			{
				_car.gameObj->SetImmortalFlag(_finished);
				_car.gameObj->SetShellFlag(false);
			}
		}

		bool Player::GetShotFreeze() const
		{
			return _shotFreeze;
		}

		void Player::SetShotFreeze(bool value)
		{
			_shotFreeze = value;
		}

		bool Player::GetMineFreeze() const
		{
			return _mineFreeze;
		}

		void Player::SetMineFreeze(bool value)
		{
			_mineFreeze = value;
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

			return plr ? plr->photo : nullptr;
		}

		const std::string& Player::GetName()
		{
			const Planet::PlayerData* plr = _race->GetTournament().GetPlayerData(_gamerId);

			return !_netName.empty() ? _netName : (plr ? plr->name : scNull);
		}

		const std::string& Player::GetRealName()
		{
			const Planet::PlayerData* plr = _race->GetTournament().GetPlayerData(_gamerId);

			return plr ? plr->name : scNull;
		}

		unsigned Player::GetBonusSpeed() const
		{
			const Planet::PlayerData* plr = _race->GetTournament().GetPlayerData(_gamerId);
			return plr ? plr->bonusSpeed : 0;
		}

		unsigned Player::GetBonusRPM() const
		{
			const Planet::PlayerData* plr = _race->GetTournament().GetPlayerData(_gamerId);
			return plr ? plr->bonusRPM : 0;
		}

		unsigned Player::GetBonusTorque() const
		{
			const Planet::PlayerData* plr = _race->GetTournament().GetPlayerData(_gamerId);
			return plr ? plr->bonusTorque : 0;
		}

		float Player::GetSEM() const
		{
			const Planet::PlayerData* plr = _race->GetTournament().GetPlayerData(_gamerId);
			return plr ? plr->SEM : 0.7f;
		}

		unsigned int Player::GetIdlingRPM() const
		{
			const Planet::PlayerData* plr = _race->GetTournament().GetPlayerData(_gamerId);
			return plr ? plr->idlingRPM : 1000;
		}

		float Player::GetBrakeTorque() const
		{
			const Planet::PlayerData* plr = _race->GetTournament().GetPlayerData(_gamerId);
			return plr ? plr->brakeTorque : 7500;
		}

		float Player::GetRestTorque() const
		{
			const Planet::PlayerData* plr = _race->GetTournament().GetPlayerData(_gamerId);
			return plr ? plr->restTorque : 400.0f;
		}

		float Player::GetGearDiff() const
		{
			const Planet::PlayerData* plr = _race->GetTournament().GetPlayerData(_gamerId);
			return plr ? plr->gearDiff : 3.42f;
		}

		unsigned Player::GetArmorBonus() const
		{
			const Planet::PlayerData* plr = _race->GetTournament().GetPlayerData(_gamerId);
			return plr ? plr->armorBonus : NULL;
		}

		bool Player::GetWeaponBonus() const
		{
			const Planet::PlayerData* plr = _race->GetTournament().GetPlayerData(_gamerId);
			return plr ? plr->weaponBonus : false;
		}

		bool Player::GetMineBonus() const
		{
			const Planet::PlayerData* plr = _race->GetTournament().GetPlayerData(_gamerId);
			return plr ? plr->mineBonus : false;
		}

		bool Player::GetSpringBonus() const
		{
			const Planet::PlayerData* plr = _race->GetTournament().GetPlayerData(_gamerId);
			return plr ? plr->springBonus : false;
		}

		bool Player::GetNitroBonus() const
		{
			const Planet::PlayerData* plr = _race->GetTournament().GetPlayerData(_gamerId);
			return plr ? plr->nitroBonus : false;
		}

		bool Player::GetDoubleJump() const
		{
			const Planet::PlayerData* plr = _race->GetTournament().GetPlayerData(_gamerId);
			return plr ? plr->doubleJump : false;
		}

		float Player::GetJumpPower() const
		{
			const Planet::PlayerData* plr = _race->GetTournament().GetPlayerData(_gamerId);
			return plr ? plr->jumpPower : 1;
		}

		float Player::GetCoolDown() const
		{
			const Planet::PlayerData* plr = _race->GetTournament().GetPlayerData(_gamerId);
			return plr ? plr->cooldown : 1;
		}

		bool Player::GetFixRecovery() const
		{
			const Planet::PlayerData* plr = _race->GetTournament().GetPlayerData(_gamerId);
			return plr ? plr->fixRecovery : false;
		}

		float Player::GetDriftStrength() const
		{
			const Planet::PlayerData* plr = _race->GetTournament().GetPlayerData(_gamerId);
			return plr ? plr->driftStrength : 1;
		}

		float Player::GetBlowDamage() const
		{
			const Planet::PlayerData* plr = _race->GetTournament().GetPlayerData(_gamerId);
			return plr ? plr->blowDamage : 1;
		}

		bool Player::GetHardBorders() const
		{
			const Planet::PlayerData* plr = _race->GetTournament().GetPlayerData(_gamerId);
			return plr ? plr->hardBorders : true;
		}

		bool Player::GetBorderCluth() const
		{
			const Planet::PlayerData* plr = _race->GetTournament().GetPlayerData(_gamerId);
			return plr ? plr->borderCluth : false;
		}

		bool Player::GetMasloDrop() const
		{
			const Planet::PlayerData* plr = _race->GetTournament().GetPlayerData(_gamerId);
			return plr ? plr->masloDrop : false;
		}

		bool Player::GetStabilityMine() const
		{
			const Planet::PlayerData* plr = _race->GetTournament().GetPlayerData(_gamerId);
			return plr ? plr->stabilityMine : false;
		}

		bool Player::GetStabilityShot() const
		{
			const Planet::PlayerData* plr = _race->GetTournament().GetPlayerData(_gamerId);
			return plr ? plr->stabilityShot : false;
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
