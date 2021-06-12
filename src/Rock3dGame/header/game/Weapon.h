#pragma once

#include "GameObject.h"

namespace r3d
{

namespace game
{

class Proj: public GameObject
{
	typedef GameObject _MyBase;
public:
	enum Type {ptRocket = 0, ptHyper, ptTorpeda, ptLaser, ptMedpack, ptCharge, ptMoney, ptImmortal, ptSpeedArrow, ptLusha, ptMaslo, ptMine, ptMineRip, ptMinePiece, ptFire, ptDrobilka, ptSonar, ptSpring, ptFrostRay, ptMortira, ptCrater, ptImpulse, ptThunder, ptResonanse, ptMineProton, cProjTypeEnd = 0};

	struct Desc
	{
	private:
		//модель снаряда
		MapObjRec* _model;
		MapObjRec* _model2;
		MapObjRec* _model3;
		graph::LibMaterial* _libMat;
	public:
		Desc(): _model(0), _model2(0), _model3(0), _libMat(0), type(ptRocket), pos(NullVector), rot(NullQuaternion), size(NullVector), sizeAddPx(NullVector), offset(NullVector), modelSize(true), speed(0), speedRelativeMin(13.0f), speedRelative(false), angleSpeed(0), maxDist(0), mass(100.0f), minTimeLife(0), damage(0) {}
		
		Desc(const Desc& ref): _model(0), _model2(0), _model3(0), _libMat(0)
		{
			*this = ref;
		}
		
		~Desc()
		{
			SetModel(0);
			SetModel2(0);
			SetModel3(0);
			SetLibMat(0);
		}

		void SaveTo(lsl::SWriter* writer, lsl::Serializable* owner)
		{
			lsl::SWriteValue(writer, "type", type);
			lsl::SWriteValue(writer, "pos", pos);
			lsl::SWriteValue(writer, "rot", rot);			
			lsl::SWriteValue(writer, "size", size);
			lsl::SWriteValue(writer, "sizeAddPx", sizeAddPx);
			lsl::SWriteValue(writer, "offset", offset);			
			lsl::SWriteValue(writer, "modelSize", modelSize);
			lsl::SWriteValue(writer, "speed", speed);
			lsl::SWriteValue(writer, "speedRelativeMin", speedRelativeMin);
			lsl::SWriteValue(writer, "speedRelative", speedRelative);
			lsl::SWriteValue(writer, "angleSpeed", angleSpeed);
			lsl::SWriteValue(writer, "maxDist", maxDist);
			lsl::SWriteValue(writer, "mass", mass);
			lsl::SWriteValue(writer, "minTimeLife", minTimeLife);
			lsl::SWriteValue(writer, "damage", damage);
			
			MapObjLib::SaveRecordRef(writer, "model", _model);
			MapObjLib::SaveRecordRef(writer, "model2", _model2);
			MapObjLib::SaveRecordRef(writer, "model3", _model3);
			writer->WriteRef("libMat", _libMat);
		}

		void LoadFrom(lsl::SReader* reader, lsl::Serializable* owner)
		{
			int projType;
			lsl::SReadValue(reader, "type", projType);
			type = Proj::Type(projType);

			lsl::SReadValue(reader, "pos", pos);
			lsl::SReadValue(reader, "rot", rot);
			lsl::SReadValue(reader, "size", size);
			lsl::SReadValue(reader, "sizeAddPx", sizeAddPx);
			lsl::SReadValue(reader, "offset", offset);
			lsl::SReadValue(reader, "modelSize", modelSize);
			lsl::SReadValue(reader, "speed", speed);
			lsl::SReadValue(reader, "speedRelativeMin", speedRelativeMin);
			lsl::SReadValue(reader, "speedRelative", speedRelative);
			lsl::SReadValue(reader, "angleSpeed", angleSpeed);
			lsl::SReadValue(reader, "maxDist", maxDist);
			lsl::SReadValue(reader, "mass", mass);
			lsl::SReadValue(reader, "minTimeLife", minTimeLife);
			lsl::SReadValue(reader, "damage", damage);
			
			SetModel(MapObjLib::LoadRecordRef(reader, "model"));
			SetModel2(MapObjLib::LoadRecordRef(reader, "model2"));
			SetModel3(MapObjLib::LoadRecordRef(reader, "model3"));
			reader->ReadRef("libMat", true, owner, 0);
		}

		void OnFixUp(const FixUpName& fixUpName)
		{
			if (fixUpName.name == "libMat")
			{
				SetLibMat(fixUpName.GetCollItem<graph::LibMaterial*>());
			}
		}

		MapObjRec* GetModel() const
		{
			return _model;
		}
		void SetModel(MapObjRec* value)
		{
			if (ReplaceRef(_model, value))
				_model = value;
		}
		MapObjRec* GetModel2() const
		{
			return _model2;
		}
		void SetModel2(MapObjRec* value)
		{
			if (ReplaceRef(_model2, value))
				_model2 = value;
		}
		MapObjRec* GetModel3() const
		{
			return _model3;
		}
		void SetModel3(MapObjRec* value)
		{
			if (ReplaceRef(_model3, value))
				_model3 = value;
		}

		graph::LibMaterial* GetLibMat() const
		{
			return _libMat;
		}
		void SetLibMat(graph::LibMaterial* value)
		{
			if (ReplaceRef(_libMat, value))
				_libMat = value;
		}

		Desc& operator=(const Desc& ref)
		{
			type = ref.type;
			pos = ref.pos;
			rot = ref.rot;
			size = ref.size;
			sizeAddPx = ref.sizeAddPx;
			offset = ref.offset;
			modelSize = ref.modelSize;
			speed = ref.speed;
			speedRelativeMin = ref.speedRelativeMin;
			speedRelative = ref.speedRelative;
			angleSpeed = ref.angleSpeed;
			maxDist = ref.maxDist;
			mass = ref.mass;
			minTimeLife = ref.minTimeLife;
			damage = ref.damage;

			SetModel(ref._model);
			SetModel2(ref._model2);
			SetModel3(ref._model3);
			SetLibMat(ref._libMat);

			return *this;
		}

		//тип снаряда
		Type type;		
		//Локальная позиция снаряда
		D3DXVECTOR3 pos;
		//Локальный поворот снаряда
		D3DXQUATERNION rot;
		//размер и смещение бокса снаряда
		D3DXVECTOR3 size;
		D3DXVECTOR3 sizeAddPx;
		D3DXVECTOR3 offset;
		//прибавлять к размеру размер модели снаряда
		bool modelSize;
		//Скорость снаряда, 
		//- для гиппер драйва ускорение
		//- для мины подброс
		float speed;
		//минимально допустимая скорость относительно движения машины, 13 м/с
		float speedRelativeMin;
		//скорость относительно движения машины
		bool speedRelative;
		//Угловая скорость
		//-для разрывной мины время до разрыва
		//-для протонной мины время активации
		float angleSpeed;
		//максимальное дистанция поражения, относительно этой величины плюс скорости, расчитываются и время жизни
		//<=0 - неограничено
		float maxDist;
		//масса снаряда, не должна быть равной нулю
		float mass;
		//минимальное время жизни объекта, которое он проживет обязательно 
		FloatRange minTimeLife;
		//повреждение наносимое снарядом
		float damage;				
	};

	struct ShotDesc
	{
	private:
		MapObj* _targetMapObj;
	public:
		ShotDesc(): _targetMapObj(0), target(NullVector) {}

		ShotDesc(const ShotDesc& ref): _targetMapObj(0)
		{
			*this = ref;
		}

		~ShotDesc()
		{
			SetTargetMapObj(0);
		}

		MapObj* GetTargetMapObj() const
		{
			return _targetMapObj;
		}

		void SetTargetMapObj(MapObj* value)
		{
			if (ReplaceRef(_targetMapObj, value))
				_targetMapObj = value;
		}

		ShotDesc& operator=(const ShotDesc& ref)
		{
			target = ref.target;
			
			SetTargetMapObj(ref._targetMapObj);

			return *this;
		}

		D3DXVECTOR3 target;		
	};

	struct ShotContext
	{
		Logic* logic;		
		ShotDesc shot;

		NxMat34* projMat;

		ShotContext(): logic(NULL), projMat(NULL) {}
	};
private:
	Desc _desc;
	ShotDesc _shot;
	MapObj* _model;
	MapObj* _model2;

	//если является родителем (LinkToWeapon), то proj уничтожается вместе с ним, указан всегда если создается игроком из оружия
	GameObject* _weapon;
	int _playerId;
	px::BoxShape* _pxBox;
	bool _ignoreContactProj;
	graph::Sprite* _sprite;

	int _tick1;
	float _time1;
	bool _state1;
	D3DXVECTOR3 _vec1;

	void LocateProj(GameObject* weapon, bool pos, bool rot, const D3DXVECTOR3* speed);

	void InitModel();
	void FreeModel(bool remove);
	//
	void InitModel2();
	void FreeModel2(bool remove);

	px::Body* CreateBody(const NxBodyDesc& desc);
	graph::Sprite* CreateSprite();
	void FreeSprite();
	void InsertProjToGraph(GraphManager* graph);

	AABB ComputeAABB(bool onlyModel);
	void CreatePxBox(NxCollisionGroup group = px::Scene::cdgShot);
	void AddContactForce(GameObject* target, const D3DXVECTOR3& point, const D3DXVECTOR3& force, NxForceMode mode);
	void AddContactForce(GameObject* target, const px::Scene::OnContactEvent& contact, const D3DXVECTOR3& force, NxForceMode mode);

	void SetWeapon(GameObject* weapon);
	//прилинковать к владельцу. Уничтожается вместе с ним
	void LinkToWeapon();
	void SetIgnoreContactProj(bool value);
	void SetShot(const ShotDesc& value);
	void DamageTarget(GameObject* target, float damage, DamageType damageType = dtSimple);	
	MapObj* FindNextTaget(float viewAngle);
	//
	void EnableFilter(GameObject* target, unsigned mask);
	void DisableFilter(GameObject* target);

	D3DXVECTOR3 CalcSpeed(GameObject* weapon);

	bool RocketPrepare(GameObject* weapon, bool disableGravity = true, D3DXVECTOR3* speedVec = NULL, NxCollisionGroup pxGroup = px::Scene::cdgShot);
	void RocketContact(const px::Scene::OnContactEvent& contact);
	void RocketUpdate(float deltaTime);
	//
	bool HyperPrepare(GameObject* weapon);	
	//
	bool MedpackPrepare(GameObject* weapon);
	void MedpackContact(const px::Scene::OnContactEvent& contact);
	//
	bool ChargePrepare(GameObject* weapon);
	void ChargeContact(const px::Scene::OnContactEvent& contact);
	//
	bool MoneyPrepare(GameObject* weapon);
	void MoneyContact(const px::Scene::OnContactEvent& contact);
	//
	bool ImmortalPrepare(GameObject* weapon);
	void ImmortalContact(const px::Scene::OnContactEvent& contact);
	//
	bool SpeedArrowPrepare(GameObject* weapon);
	void SpeedArrowContact(const px::Scene::OnContactEvent& contact);
	//
	bool LushaPrepare(const ShotContext& ctx);
	void LushaContact(const px::Scene::OnContactEvent& contact);
	//
	bool MinePrepare(const ShotContext& ctx, bool lockMine);
	void MineContact(const px::Scene::OnContactEvent& contact, bool testLockMine);
	float MineUpdate(float deltaTime, float delay = 0.25f);
	//
	bool MasloPrepare(const ShotContext& ctx);
	void MasloContact(const px::Scene::OnContactEvent& contact);
	void MasloUpdate(float deltaTime);	
	//
	bool MineRipPrepare(const ShotContext& ctx);
	void MineRipContact(const px::Scene::OnContactEvent& contact);
	void MineRipUpdate(float deltaTime);
	//
	bool MinePiecePrepare(const ShotContext& ctx);
	void MinePieceContact(const px::Scene::OnContactEvent& contact);	
	//
	bool MineProtonPrepare(const ShotContext& ctx);
	void MineProtonContact(const px::Scene::OnContactEvent& contact);
	void MineProtonUpdate(float deltaTime);
	//
	bool TorpedaPrepare(GameObject* weapon);
	void TorpedaContact(const px::Scene::OnContactEvent& contact);
	void TorpedaUpdate(float deltaTime);
	//
	bool LaserPrepare(GameObject* weapon);
	GameObject* LaserUpdate(float deltaTime, bool distort);
	//
	bool FirePrepare(GameObject* weapon);
	void FireContact(const px::Scene::OnContactEvent& contact);
	void FireUpdate(float deltaTime);	
	//
	bool DrobilkaPrepare(GameObject* weapon);
	void DrobilkaContact(const px::Scene::OnContactEvent& contact);
	void DrobilkaUpdate(float deltaTime);
	//
	bool SonarPrepare(GameObject* weapon);
	void SonarContact(const px::Scene::OnContactEvent& contact);
	//
	bool SpringPrepare(GameObject* weapon);
	void SpringUpdate(float deltaTime);

	bool FrostRayPrepare(GameObject* weapon);
	void FrostRayUpdate(float deltaTime);

	bool MortiraPrepare(GameObject* weapon);
	void MortiraContact(const px::Scene::OnContactEvent& contact);

	bool CraterPrepare(const ShotContext& ctx);
	void CraterContact(const px::Scene::OnContactEvent& contact);

	bool ImpulsePrepare(GameObject* weapon);
	void ImpulseContact(const px::Scene::OnContactEvent& contact);
	void ImpulseUpdate(float deltaTime);

	bool ThunderPrepare(GameObject* weapon);
	void ThunderContact(const px::Scene::OnContactEvent& contact);	
	void ThunderUpdate(float deltaTime);

	bool ResonansePrepare(GameObject* weapon);
	void ResonanseContact(const px::Scene::OnContactEvent& contact);
	void ResonanseUpdate(float deltaTime);
protected:
	virtual void OnDestroy(GameObject* sender);
	virtual void OnContact(const px::Scene::OnContactEvent& contact);

	virtual void SaveSource(lsl::SWriter* writer);
	virtual void LoadSource(lsl::SReader* reader);
	virtual void OnFixUp(const FixUpNames& fixUpNames);
public:
	Proj();
	virtual ~Proj();

	virtual Proj* IsProj();

	virtual void OnProgress(float deltaTime);
	bool PrepareProj(GameObject* weapon, const ShotContext& ctx);

	void MineContact(GameObject* target, const D3DXVECTOR3& point);

	const Desc& GetDesc() const;
	void SetDesc(const Desc& value);

	const ShotDesc& GetShot() const;

	GameObject* GetWeapon();
};

class AutoProj: public Proj
{
	typedef Proj _MyBase;
private:
	bool _prepare;

	void InitProj();
	void FreeProj();	
protected:
	virtual void LogicReleased();
	virtual void LogicInited();
	
	virtual void SaveSource(lsl::SWriter* writer);
	virtual void LoadSource(lsl::SReader* reader);	
public:
	AutoProj();
	virtual ~AutoProj();	
};

class Weapon: public GameObject
{
	typedef GameObject _MyBase;
public:
	typedef Proj::Desc ProjDesc;
	typedef lsl::List<ProjDesc> ProjDescList;
	typedef Proj::ShotDesc ShotDesc;

	struct Desc
	{
	private:
		ProjDesc _tmpProj;
	public:
		Desc(): shotDelay(0) {}

		Desc(const Desc& ref)
		{
			*this = ref;
		}

		void SaveTo(lsl::SWriter* writer, lsl::Serializable* owner)
		{
			lsl::SWriter* projListNode = writer->NewDummyNode("projList");
			int i = 0;
			for (ProjDescList::iterator iter = projList.begin(); iter != projList.end(); ++iter, ++i)
			{
				std::stringstream sstream;
				sstream << "proj" << i;
				lsl::SWriter* proj = projListNode->NewDummyNode(sstream.str().c_str());

				iter->SaveTo(proj, owner);
			}	

			lsl::SWriteValue(writer, "shotDelay", shotDelay);
		}

		void LoadFrom(lsl::SReader* reader, lsl::Serializable* owner)
		{
			projList.clear();

			lsl::SReader* projListNode = reader->ReadValue("projList");
			if (projListNode)
			{
				lsl::SReader* proj = projListNode->FirstChildValue();
				while (proj)
				{	
					ProjDesc projDesc;
					projDesc.LoadFrom(proj, owner);
					projList.push_back(projDesc);

					proj = proj->NextValue();
				}
			}

			lsl::SReadValue(reader, "shotDelay", shotDelay);
		}

		void OnFixUp(const Serializable::FixUpNames& fixUpNames)
		{
			ProjDescList::iterator iterProj = projList.begin();

			for (Serializable::FixUpNames::const_iterator iter = fixUpNames.begin(); iter != fixUpNames.end(); ++iter)
			{
				std::string projStr = iter->sender->GetOwnerValue()->GetMyName();
				projStr = projStr.substr(0, 4);

				if (projStr == "proj" && iterProj != projList.end())
				{
					iterProj->OnFixUp(*iter);
					++iterProj;
				}
			}
		}
		
		const ProjDesc& Front() const
		{
			return !projList.empty() ? projList.front() : _tmpProj;
		}

		Desc& operator=(const Desc& ref)
		{
			shotDelay = ref.shotDelay;
			projList = ref.projList;

			return *this;
		}

		//Задержка между выстрелами
		float shotDelay;

		ProjDescList projList;
	};
public:
	typedef lsl::List<Proj*> ProjList;
private:
	Desc _desc;	
	float _shotTime;
protected:
	virtual void SaveSource(lsl::SWriter* writer);
	virtual void LoadSource(lsl::SReader* reader);
	virtual void OnFixUp(const FixUpNames& fixUpNames);
public:
	Weapon();
	~Weapon();

	virtual void OnProgress(float deltaTime);

	bool Shot(const ShotDesc& shotDesc, ProjList* projList = NULL);
	bool Shot(const D3DXVECTOR3& target, ProjList* projList = NULL);
	bool Shot(MapObj* target, ProjList* projList = NULL);
	bool Shot(ProjList* projList = NULL);

	float GetShotTime();

	bool IsReadyShot(float delay) const;
	bool IsReadyShot() const;
	bool IsMaslo() const;

	const Desc& GetDesc() const;
	void SetDesc(const Desc& value);

	static bool CreateShot(Weapon* weapon, const Weapon::Desc& desc, const Proj::ShotContext& ctx, ProjList* projList);
};

}

}