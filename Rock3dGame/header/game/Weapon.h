#pragma once

#include "GameObject.h"

namespace r3d
{
	namespace game
	{
		extern unsigned int GAME_DIFF;

		class Proj : public GameObject
		{
			using _MyBase = GameObject;
		public:
			//         0             1        2         3          4         5        6          7            8            9        10        11          12         13       14          15             16         17         18        19           20         21         22         23           24        25        26         27          28      29         30      31          32         33           34            35       36      37      38        39        40          41        42      43      44              45           46            47           48          49         50       51        52          53            54       55         56      57      58           59        60        61      62          63          64        65        66          67         68            69           70           71        72        73          74      75       76         77            78            79            80                     
			enum Type
			{
				ptRocket = 0,
				ptHyper,
				ptSpring,
				ptSpring2,
				ptRipper,
				ptShell,
				ptBlaster,
				ptAirWeapon,
				ptRezonator,
				ptSonar,
				ptMiniGun,
				ptShotGun,
				ptTrinity,
				ptLaser,
				ptFrostRay,
				ptNewFrostRay,
				ptMortira,
				ptMolotov,
				ptCrater,
				ptArtillery,
				ptGrenade,
				ptTorpeda,
				ptImpulse,
				ptSphereGun,
				ptPlazma,
				ptBlazer,
				ptPhoenix,
				ptMagnetic,
				ptDisc,
				ptShocker,
				ptMine,
				ptMiniMine,
				ptMineRip,
				ptMinePiece,
				ptMineProton,
				ptMaslo,
				ptAcid,
				ptFire,
				ptSpikes,
				ptSmokes,
				ptBurnMine,
				ptBarrel,
				ptBoom,
				ptCore,
				ptForwardPoint,
				ptLeftPoint,
				ptRightPoint,
				ptLaserMine,
				ptAspirine,
				ptMedpack,
				ptMoney,
				ptCharge,
				ptImmortal,
				ptSpeedArrow,
				ptLusha,
				ptSpinner,
				ptSand,
				ptLava,
				ptFireBoost,
				ptDanger,
				ptBaloon,
				ptRock,
				ptPlatform,
				ptGraviton,
				ptSapper,
				ptIsoCam,
				ptThirdCam,
				ptCamFree,
				ptAutoCamOff,
				ptAutoCamOn,
				ptAltFinish,
				ptInRamp,
				ptAiJump,
				ptHyperFly,
				ptRage,
				ptLucky,
				ptUnLucky,
				ptRocketJump,
				ptAiLockJump,
				ptHeadLightOff,
				ptHeadLightOn,
				cProjTypeEnd = 0
			};

			struct Desc
			{
			private:
				//модель снаряда
				MapObjRec* _model;
				MapObjRec* _model2;
				MapObjRec* _model3;
				graph::LibMaterial* _libMat;
			public:
				Desc(): _model(nullptr), _model2(nullptr), _model3(nullptr), _libMat(nullptr), type(ptRocket),
				        pos(NullVector), rot(NullQuaternion), size(NullVector), sizeAddPx(NullVector),
				        offset(NullVector), modelSize(true), speed(0), speedRelativeMin(13.0f), speedRelative(false),
				        angleSpeed(0), maxDist(0), mass(100.0f), minTimeLife(0), damage(0)
				{
				}

				Desc(const Desc& ref): _model(nullptr), _model2(nullptr), _model3(nullptr), _libMat(nullptr)
				{
					*this = ref;
				}

				~Desc()
				{
					SetModel(nullptr);
					SetModel2(nullptr);
					SetModel3(nullptr);
					SetLibMat(nullptr);
				}

				void SaveTo(SWriter* writer, Serializable* owner)
				{
					SWriteValue(writer, "type", type);
					SWriteValue(writer, "pos", pos);
					SWriteValue(writer, "rot", rot);
					SWriteValue(writer, "size", size);
					SWriteValue(writer, "sizeAddPx", sizeAddPx);
					SWriteValue(writer, "offset", offset);
					SWriteValue(writer, "modelSize", modelSize);
					SWriteValue(writer, "speed", speed);
					SWriteValue(writer, "speedRelativeMin", speedRelativeMin);
					SWriteValue(writer, "speedRelative", speedRelative);
					SWriteValue(writer, "angleSpeed", angleSpeed);
					SWriteValue(writer, "maxDist", maxDist);
					SWriteValue(writer, "mass", mass);
					SWriteValue(writer, "minTimeLife", minTimeLife);
					SWriteValue(writer, "damage", damage);

					MapObjLib::SaveRecordRef(writer, "model", _model);
					MapObjLib::SaveRecordRef(writer, "model2", _model2);
					MapObjLib::SaveRecordRef(writer, "model3", _model3);
					writer->WriteRef("libMat", _libMat);
				}

				void LoadFrom(SReader* reader, Serializable* owner)
				{
					int projType;
					SReadValue(reader, "type", projType);
					type = static_cast<Proj::Type>(projType);

					SReadValue(reader, "pos", pos);
					SReadValue(reader, "rot", rot);
					SReadValue(reader, "size", size);
					SReadValue(reader, "sizeAddPx", sizeAddPx);
					SReadValue(reader, "offset", offset);
					SReadValue(reader, "modelSize", modelSize);
					SReadValue(reader, "speed", speed);
					SReadValue(reader, "speedRelativeMin", speedRelativeMin);
					SReadValue(reader, "speedRelative", speedRelative);
					SReadValue(reader, "angleSpeed", angleSpeed);
					SReadValue(reader, "maxDist", maxDist);
					SReadValue(reader, "mass", mass);
					SReadValue(reader, "minTimeLife", minTimeLife);
					SReadValue(reader, "damage", damage);

					SetModel(MapObjLib::LoadRecordRef(reader, "model"));
					SetModel2(MapObjLib::LoadRecordRef(reader, "model2"));
					SetModel3(MapObjLib::LoadRecordRef(reader, "model3"));
					reader->ReadRef("libMat", true, owner, nullptr);
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
				//- сила прыжка и ускорение от нитро
				//- для кислоты скорость при замедлении
				//- для воздушного нитро скорость полёта
				float speed;
				//минимально допустимая скорость относительно движения машины, 13 м/с			
				//-для воздушного нитро скорость во время торможения
				float speedRelativeMin;
				//скорость относительно движения машины
				//для шокера - блокировка стрельбы
				//для лазерной мины - время жизни после активации
				//-для лазера вкл/откл поворот лазера
				//-для ядра скаттерпака время до активации
				//-для спутников скаттерпака задержка перед движением		
				//-для воздушного нитро вкл/откл динамическую скорость полёта
				bool speedRelative;
				//Угловая скорость
				//-для разрывной мины время до разрыва
				//-для протонной мины время активации
				//-для дробовика сила разброса
				//-для миниракет скорость и дальность раскрытия
				//-для магмы, поджигательной мины и сапера время действия 
				//-для артиллерии сила подброса снаряда вверх
				//-для лазера угол поворота луча
				//-для лазерной мины вкл/откл интервальный урон
				//-для прыжка повышенная устойчивость
				//-для ядра скаттерпака скорость оборотов
				//-для спутников скаттерпака время до остановки		
				float angleSpeed;
				//максимальное дистанция поражения, относительно этой величины плюс скорости, расчитываются и время жизни
				//<=0 - неограничено
				float maxDist;
				//масса снаряда, не должна быть равной нулю
				//для шокера - время действия
				//для кислоты - время замедления машины
				float mass;
				//минимальное время жизни объекта, которое он проживет обязательно 
				FloatRange minTimeLife;
				//повреждение наносимое снарядом
				//задержка между нитро
				//для аспирина % востановления здоровья
				//для магнитической пушки - скорость притяжения, относительно скорости снаряда (в %)
				//для артиллерии допольнительный расброс снарядов
				//для воздушного нитро время действия
				//и т.д...
				float damage;
			};

			struct ShotDesc
			{
			private:
				MapObj* _targetMapObj;
			public:
				ShotDesc(): _targetMapObj(nullptr), target(NullVector)
				{
				}

				ShotDesc(const ShotDesc& ref): _targetMapObj(nullptr)
				{
					*this = ref;
				}

				~ShotDesc()
				{
					SetTargetMapObj(nullptr);
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

				ShotContext(): logic(nullptr), projMat(nullptr)
				{
				}
			};

		private:
			Desc _desc;
			ShotDesc _shot;
			MapObj* _model;
			MapObj* _model2;
			MapObj* _model3;

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

			void RandomizeLocateProj(GameObject* weapon, bool pos, bool rot, const D3DXVECTOR3* speed);
			void VariabilityLocateProj(GameObject* weapon, bool pos, bool rot, const D3DXVECTOR3* speed);
			void LocateProj(GameObject* weapon, bool pos, bool rot, const D3DXVECTOR3* speed);

			void InitModel();
			void FreeModel(bool remove);
			//
			void InitModel2();
			void FreeModel2(bool remove);

			void InitModel3();
			void FreeModel3(bool remove);

			px::Body* CreateBody(const NxBodyDesc& desc) const;
			graph::Sprite* CreateSprite();
			void FreeSprite() const;
			void InsertProjToGraph(GraphManager* graph) const;

			AABB ComputeAABB(bool onlyModel) const;
			void CreatePxBox(NxCollisionGroup group = px::Scene::cdgShot);
			static void AddContactForce(GameObject* target, const D3DXVECTOR3& point, const D3DXVECTOR3& force,
			                            NxForceMode mode);
			void AddContactForce(GameObject* target, const px::Scene::OnContactEvent& contact, const D3DXVECTOR3& force,
			                     NxForceMode mode) const;

			void SetWeapon(GameObject* weapon);
			//прилинковать к владельцу. Уничтожается вместе с ним
			void LinkToWeapon();
			void SetIgnoreContactProj(bool value);
			bool GetIgnoreContactProj() const;
			void SetShot(const ShotDesc& value);
			void DamageTarget(GameObject* target, float damage, DamageType damageType = dtSimple) const;
			MapObj* FindNextTaget(float viewAngle) const;
			//
			static void EnableFilter(GameObject* target, unsigned mask);
			static void DisableFilter(GameObject* target);

			D3DXVECTOR3 CalcSpeed(GameObject* weapon) const;
			D3DXVECTOR3 CalcFlySpeed(GameObject* weapon) const;
			D3DXVECTOR3 BrakeFlySpeed(GameObject* weapon) const;
			D3DXVECTOR3 GetFixedBackSpeed(GameObject* weapon) const;
			D3DXVECTOR3 GetDoubleSpeed(GameObject* weapon) const;

			bool FixRocketPrepare(GameObject* weapon, bool disableGravity = true, D3DXVECTOR3* speedVec = nullptr,
			                      NxCollisionGroup pxGroup = px::Scene::cdgShot);
			void FixRocketContact(const px::Scene::OnContactEvent& contact);
			//
			bool RocketPrepare(GameObject* weapon, bool disableGravity = true, D3DXVECTOR3* speedVec = nullptr,
			                   NxCollisionGroup pxGroup = px::Scene::cdgShot);
			void RocketContact(const px::Scene::OnContactEvent& contact);
			void RocketUpdate(float deltaTime);
			//
			bool HyperPrepare(GameObject* weapon);
			void HyperUpdate(float deltaTime);
			//
			bool SpringPrepare(GameObject* weapon);
			bool Spring2Prepare(GameObject* weapon);
			void Spring2Update(float deltaTime);
			//
			bool RipperPrepare(GameObject* weapon);
			void RipperContact(const px::Scene::OnContactEvent& contact);
			void RipperUpdate(float deltaTime);
			//
			bool ShellPrepare(GameObject* weapon);
			//
			void BlasterContact(const px::Scene::OnContactEvent& contact);
			//
			bool AirWeaponPrepare(GameObject* weapon, bool disableGravity = true, D3DXVECTOR3* speedVec = nullptr,
			                      NxCollisionGroup pxGroup = px::Scene::cdgShot);
			void AirWeaponUpdate(float deltaTime);
			//
			bool RezonatorPrepare(GameObject* weapon);
			void RezonatorContact(const px::Scene::OnContactEvent& contact);
			void RezonatorUpdate(float deltaTime);
			//
			bool SonarPrepare(GameObject* weapon);
			void SonarContact(const px::Scene::OnContactEvent& contact);
			void SonarUpdate(float deltaTime);
			//
			void MiniGunUpdate(float deltaTime) const;
			//
			bool ShotGunPrepare(GameObject* weapon, D3DXVECTOR3* speedVec = nullptr,
			                    NxCollisionGroup pxGroup = px::Scene::cdgShot);
			void ShotGunUpdate(float deltaTime);
			//
			bool TrinityPrepare(GameObject* weapon);
			void TrinityUpdate(float deltaTime);
			//
			bool LaserPrepare(GameObject* weapon);
			GameObject* LaserUpdate(float deltaTime, bool distort);
			GameObject* FrostUpdate(float deltaTime, bool distort) const;
			//
			bool FrostRayPrepare(GameObject* weapon);
			void FrostRayUpdate(float deltaTime) const;
			void NewFrostRayUpdate(float deltaTime) const;
			//
			bool MortiraPrepare(GameObject* weapon, D3DXVECTOR3* speedVec = nullptr,
			                    NxCollisionGroup pxGroup = px::Scene::cdgShotTrack);
			void MortiraContact(const px::Scene::OnContactEvent& contact);
			//	
			bool MolotovPrepare(GameObject* weapon, D3DXVECTOR3* speedVec = nullptr,
			                    NxCollisionGroup pxGroup = px::Scene::cdgShotTrack);
			void MolotovContact(const px::Scene::OnContactEvent& contact);
			//
			bool CraterPrepare(const ShotContext& ctx);
			void CraterContact(const px::Scene::OnContactEvent& contact) const;
			//
			bool ArtilleryPrepare(const ShotContext& ctx);
			void ArtilleryContact(const px::Scene::OnContactEvent& contact);
			void ArtilleryUpdate(float deltaTime) const;
			//
			bool GrenadePrepare(const ShotContext& ctx);
			void GrenadeContact(const px::Scene::OnContactEvent& contact);
			void GrenadeUpdate(float deltaTime);
			//	
			bool TorpedaPrepare(GameObject* weapon);
			void TorpedaUpdate(float deltaTime);
			void SphereUpdate(float deltaTime);
			//
			bool ImpulsePrepare(GameObject* weapon);
			void ImpulseContact(const px::Scene::OnContactEvent& contact);
			//
			void PlazmaContact(const px::Scene::OnContactEvent& contact);
			//
			void BlazerContact(const px::Scene::OnContactEvent& contact);
			void BlazerUpdate(float deltaTime);
			//
			void PhoenixContact(const px::Scene::OnContactEvent& contact) const;
			void PhoenixUpdate(float deltaTime);
			//
			bool MagneticPrepare(GameObject* weapon, D3DXVECTOR3* speedVec = nullptr,
			                     NxCollisionGroup pxGroup = px::Scene::cdgDefault);
			void MagneticContact(const px::Scene::OnContactEvent& contact);
			void MagneticUpdate(float deltaTime);
			//
			bool DiscPrepare(GameObject* weapon);
			void DiscContact(const px::Scene::OnContactEvent& contact);
			void DiscUpdate(float deltaTime);
			//
			bool ShockerPrepare(GameObject* weapon);
			void ShockerContact(const px::Scene::OnContactEvent& contact) const;
			void ShockerUpdate(float deltaTime);
			//
			bool MinePrepare(const ShotContext& ctx, bool lockMine);
			void MineContact(const px::Scene::OnContactEvent& contact, bool testLockMine);
			float MineUpdate(float deltaTime, float delay = 0.25f);
			//
			void MineRipUpdate(float deltaTime);
			//
			bool MinePiecePrepare(const ShotContext& ctx);
			//
			bool MasloPrepare(const ShotContext& ctx);
			void MasloContact(const px::Scene::OnContactEvent& contact);
			void MasloUpdate(float deltaTime);
			//
			void AcidContact(const px::Scene::OnContactEvent& contact);
			//
			bool FirePrepare(GameObject* weapon, bool disableGravity = true, D3DXVECTOR3* speedVec = nullptr,
			                 NxCollisionGroup pxGroup = px::Scene::cdgShot);
			void FireContact(const px::Scene::OnContactEvent& contact);
			void FireUpdate(float deltaTime) const;
			//
			bool SpikesPrepare(const ShotContext& ctx, bool lockMine);
			void SpikesContact(const px::Scene::OnContactEvent& contact);
			//
			void SmokesContact(const px::Scene::OnContactEvent& contact) const;
			//
			void BurnMineContact(const px::Scene::OnContactEvent& contact);
			//
			bool BarrelPrepare(const ShotContext& ctx);
			//
			bool BoomPrepare(const ShotContext& ctx);
			void BoomContact(const px::Scene::OnContactEvent& contact);
			//
			bool CorePrepare(GameObject* weapon, D3DXVECTOR3* speedVec = nullptr,
			                 NxCollisionGroup pxGroup = px::Scene::cdgShot);
			void CoreContact(const px::Scene::OnContactEvent& contact);
			void CoreUpdate(float deltaTime);
			//
			bool ScatterPrepare(GameObject* weapon, D3DXVECTOR3* speedVec = nullptr,
			                    NxCollisionGroup pxGroup = px::Scene::cdgShot);
			void ScatterContact(const px::Scene::OnContactEvent& contact);
			//
			void ForwardPointUpdate(float deltaTime);
			void LeftPointUpdate(float deltaTime);
			void RightPointUpdate(float deltaTime);
			//
			bool LaserMinePrepare(const ShotContext& ctx);
			void LaserMineContact(const px::Scene::OnContactEvent& contact);
			GameObject* LaserMineUpdate(float deltaTime, bool distort);
			//
			void AspirineContact(const px::Scene::OnContactEvent& contact);
			//
			bool HyperFlyPrepare(GameObject* weapon);
			void HyperFlyUpdate(float deltaTime);
			//	
			bool BonusPrepare(GameObject* weapon);
			void MedpackContact(const px::Scene::OnContactEvent& contact);
			//
			void MoneyContact(const px::Scene::OnContactEvent& contact);
			//
			void ChargeContact(const px::Scene::OnContactEvent& contact);
			//
			void ImmortalContact(const px::Scene::OnContactEvent& contact);
			//
			bool SpeedArrowPrepare(GameObject* weapon);
			void SpeedArrowContact(const px::Scene::OnContactEvent& contact) const;
			//
			bool LushaPrepare(const ShotContext& ctx);
			void LushaContact(const px::Scene::OnContactEvent& contact) const;
			//
			bool SpinnerPrepare(const ShotContext& ctx);
			void SpinnerContact(const px::Scene::OnContactEvent& contact) const;
			//
			void SandContact(const px::Scene::OnContactEvent& contact) const;
			//
			void LavaContact(const px::Scene::OnContactEvent& contact) const;
			//
			bool FireBoostPrepare(const ShotContext& ctx);
			void FireBoostContact(const px::Scene::OnContactEvent& contact) const;
			void DangerContact(const px::Scene::OnContactEvent& contact) const;
			//
			bool BaloonPrepare(const ShotContext& ctx);
			bool RockPrepare(const ShotContext& ctx);
			void PlatformContact(const px::Scene::OnContactEvent& contact);
			//
			void GravitonContact(const px::Scene::OnContactEvent& contact) const;
			//
			bool SapperPrepare(GameObject* weapon);
			//
			bool TriggerPrepare(const ShotContext& ctx);
			void IsoContact(const px::Scene::OnContactEvent& contact) const;
			void ThirdContact(const px::Scene::OnContactEvent& contact) const;
			void FreeContact(const px::Scene::OnContactEvent& contact) const;
			void AutoCamOffContact(const px::Scene::OnContactEvent& contact) const;
			void AutoCamOnContact(const px::Scene::OnContactEvent& contact) const;
			void AltFinishContact(const px::Scene::OnContactEvent& contact) const;
			void HeadLightOffContact(const px::Scene::OnContactEvent& contact) const;
			void HeadLightOnContact(const px::Scene::OnContactEvent& contact) const;
			void InRampContact(const px::Scene::OnContactEvent& contact) const;
			void AiJumpContact(const px::Scene::OnContactEvent& contact) const;
			void RageContact(const px::Scene::OnContactEvent& contact);
			void LuckyContact(const px::Scene::OnContactEvent& contact);
			void UnLuckyContact(const px::Scene::OnContactEvent& contact);
			//
			bool RJPrepare(GameObject* weapon);
			void RJContact(const px::Scene::OnContactEvent& contact);
			void AiLockJumpContact(const px::Scene::OnContactEvent& contact) const;
			//
		protected:
			void OnDestroy(GameObject* sender) override;
			void OnContact(const px::Scene::OnContactEvent& contact) override;

			void SaveSource(SWriter* writer) override;
			void LoadSource(SReader* reader) override;
			void OnFixUp(const FixUpNames& fixUpNames) override;
		public:
			Proj();
			~Proj() override;

			Proj* IsProj() override;

			void OnProgress(float deltaTime) override;
			bool PrepareProj(GameObject* weapon, const ShotContext& ctx);

			void MineContact(GameObject* target, const D3DXVECTOR3& point);

			const Desc& GetDesc() const;
			void SetDesc(const Desc& value);

			const ShotDesc& GetShot() const;

			GameObject* GetWeapon() const;
		};

		class AutoProj : public Proj
		{
			using _MyBase = Proj;
		private:
			bool _prepare;

			void InitProj();
			void FreeProj();
		protected:
			void LogicReleased() override;
			void LogicInited() override;

			void SaveSource(SWriter* writer) override;
			void LoadSource(SReader* reader) override;
		public:
			AutoProj();
			~AutoProj() override;
		};

		class Weapon : public GameObject
		{
			using _MyBase = GameObject;
		public:
			using ProjDesc = Proj::Desc;
			using ProjDescList = List<ProjDesc>;
			using ShotDesc = Proj::ShotDesc;

			struct Desc
			{
			private:
				ProjDesc _tmpProj;
			public:
				Desc(): shotDelay(0)
				{
				}

				Desc(const Desc& ref)
				{
					*this = ref;
				}

				void SaveTo(SWriter* writer, Serializable* owner)
				{
					SWriter* projListNode = writer->NewDummyNode("projList");
					int i = 0;
					for (auto iter = projList.begin(); iter != projList.end(); ++iter, ++i)
					{
						std::stringstream sstream;
						sstream << "proj" << i;
						SWriter* proj = projListNode->NewDummyNode(sstream.str().c_str());

						iter->SaveTo(proj, owner);
					}

					SWriteValue(writer, "shotDelay", shotDelay);
				}

				void LoadFrom(SReader* reader, Serializable* owner)
				{
					projList.clear();

					SReader* projListNode = reader->ReadValue("projList");
					if (projListNode)
					{
						SReader* proj = projListNode->FirstChildValue();
						while (proj)
						{
							ProjDesc projDesc;
							projDesc.LoadFrom(proj, owner);
							projList.push_back(projDesc);

							proj = proj->NextValue();
						}
					}

					SReadValue(reader, "shotDelay", shotDelay);
				}

				void OnFixUp(const FixUpNames& fixUpNames)
				{
					auto iterProj = projList.begin();

					for (auto iter = fixUpNames.begin(); iter != fixUpNames.end(); ++iter)
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
			using ProjList = List<Proj*>;
		private:
			Desc _desc;
			float _shotTime;
		protected:
			void SaveSource(SWriter* writer) override;
			void LoadSource(SReader* reader) override;
			void OnFixUp(const FixUpNames& fixUpNames) override;
		public:
			Weapon();
			~Weapon() override;

			void OnProgress(float deltaTime) override;

			bool Shot(const ShotDesc& shotDesc, ProjList* projList = nullptr);
			bool Shot(const D3DXVECTOR3& target, ProjList* projList = nullptr);
			bool Shot(MapObj* target, ProjList* projList = nullptr);
			bool Shot(ProjList* projList = nullptr);

			float GetShotTime() const;

			bool IsReadyShot(float delay) const;
			bool IsReadyShot() const;
			bool IsAutoMine() const;

			const Desc& GetDesc() const;
			void SetDesc(const Desc& value);

			static bool CreateShot(Weapon* weapon, const Desc& desc, const Proj::ShotContext& ctx, ProjList* projList);
		};
	}
}
