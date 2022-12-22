#include "stdafx.h"
#include "game/ResourceManager.h"

#include "game/World.h"

namespace r3d
{
	namespace game
	{
		const unsigned ResourceManager::cGenMipLevel = 0;
		const float ResourceManager::cAlphaTestRef = 0.933f;

		const std::string cStringValueStr[cStringValueEnd] = {
			"svNull",

			"svNewGame",
			"svSingleGame",
			"svFriendship",
			"svNetGame",
			"svContinue",
			"svNetCreate",
			"svConnect",
			"svIsGamer",
			"svIsSpectator",
			"svRefresh",
			"svLoad",
			"svOptions",
			"svControls",
			"svGame",
			"svNetwork",
			"svAuthors",
			"svExit",
			"svProfile",

			"svGraphic",
			"svResolution",
			"svFiltering",
			"svMultisampling",
			"svShadow",
			"svEnv",
			"svLight",
			"svPostProcess",
			"svFixedFrameRate",
			"svSound",
			"svMusic",
			"svSoundFX",
			"svSoundDicter",
			"svLanguage",
			"svCommentator",
			"svCamera",
			"svSplitScreen",
			"svMaxPlayers",
			"svMaxComputers",
			"svLapsCount",
			"svStyleHUD",
			"svMinimapStyle",
			"svEnableMineBug",
			"svUpgradeMaxLevel",
			"svSelectedLevel",
			"svSurvivalMode",
			"svAutoCamera",
			"svSubjectView",
			"svWeaponUpgrades",
			"svCamLock",
			"svStaticCam",
			"svCamFov",
			"svCamProection",
			"svOilDestroyer",
			"svDifficulty",
			"svStartOptionsInfo",
			"svSelectItem",
			"svEnterIP",

			"svChampionship",
			"svSkirmish",

			"svOnePlayer",
			"svTwoPlayer",

			"svStartRace",
			"svReadyRace",
			"svCancelReadyRace",
			"svWorkshop",
			"svGarage",
			"svCasePlanet",
			"svHostLabel",

			"svBoss",
			"svDivision",
			"svRequest",
			"svPlayer",
			"svDeads",
			"svKills",
			"svWins",
			"svRaces",

			"svInfoPlayer",
			"svInfoPlayerText",
			"svInfoRace",
			"svInfoRaceText",
			"svCarCost",
			"svTraining",
			"svTrackMaps",
			"svLockedCarName",
			"svLockedCarInfo",

			"svApply",
			"svOk",
			"svCancel",
			"svBack",
			"svYes",
			"svNo",
			"svBuy",
			"svWarning",

			"svLow",
			"svMiddle",
			"svHigh",
			"svUltra",
			"svOn",
			"svOff",

			"svRussian",
			"svEnglish",

			"svCameraSecView",
			"svCameraOrtho",
			"svSplitHor",
			"svSplitVer",
			"svSplitQuad",
			"sv2d",
			"sv3d",
			"svMMoff",
			"svMMA",
			"svMMB",
			"svMMC",
			"svMMD",
			"svMME",
			"svMMF",
			"svMMG",
			"svMMH",
			"svMMI",
			"svMMJ",
			"svMMK",
			"svFov60",
			"svFov65",
			"svFov70",
			"svFov75",
			"svFov80",
			"svFov85",
			"svFov90",
			"svFov95",
			"svFov100",
			"svFov105",
			"svFov110",
			"svFov115",
			"svDef",
			"svAuto",
			"svPlayer1",
			"svPlayer2",
			"svPlayer3",
			"svPlayer4",
			"svPlayer5",
			"svPlayer6",
			"svPlayer7",
			"svStandart",
			"svSilver",
			"svClassic",
			"svAlternative",
			"svNatives",
			"svSepia",
			"svAlternatives",
			"svRed",
			"svGreen",
			"svBlue",

			"scWheaterFair",
			"scWheaterNight",
			"scWheaterClody",
			"scWheaterRainy",
			"scWheaterThunder",
			"scWheaterSnowfall",
			"scWheaterSandstorm",
			"scWheaterEagle",
			"scWheaterMapEdit",

			"scEarth",
			"scPatagonis",
			"scIntaria",
			"scHemi5",
			"scInferno",
			"scNho",
			//
			"scEarthInfo",
			"scPatagonisInfo",
			"scIntariaInfo",
			"scHemi5Info",
			"scInfernoInfo",
			"scNhoInfo",

			"scMarauder",
			"scBuggi",
			"scDirtdevil",
			"scTankchetti",
			"scManticora",
			"scAirblade",
			"scGuseniza",
			"scPodushka",
			"scMonsterTruck",
			"scMonsterTruckBoss",
			"scDevilDriver",
			"scMustang",
			//
			"scMarauderInfo",
			"scBuggiInfo",
			"scDirtdevilInfo",
			"scTankchettiInfo",
			"scManticoraInfo",
			"scAirbladeInfo",
			"scGusenizaInfo",
			"scPodushkaInfo",
			"scMonsterTruckInfo",
			"scDevilDriverInfo",
			"scMustangInfo",

			"scEngine1",
			"scEngine2",
			"scEngine3",
			"scEngine4",
			"scTrans1",
			"scTrans2",
			"scTrans3",
			"scTrans4",
			"scWheel1",
			"scWheel2",
			"scWheel3",
			"scWheel4",
			"scArmor1",
			"scArmor2",
			"scArmor3",
			"scArmor4",
			"scTruba1",
			"scTruba2",
			"scTruba3",
			"scTruba4",
			"scGuseniza1",
			"scGuseniza2",
			"scGuseniza3",
			"scGuseniza4",
			//
			"scEngine1Info",
			"scEngine2Info",
			"scEngine3Info",
			"scEngine4Info",
			"scTrans1Info",
			"scTrans2Info",
			"scTrans3Info",
			"scTrans4Info",
			"scWheel1Info",
			"scWheel2Info",
			"scWheel3Info",
			"scWheel4Info",
			"scArmor1Info",
			"scArmor2Info",
			"scArmor3Info",
			"scArmor4Info",
			"scTruba1Info",
			"scTruba2Info",
			"scTruba3Info",
			"scTruba4Info",
			"scGuseniza1Info",
			"scGuseniza2Info",
			"scGuseniza3Info",
			"scGuseniza4Info",

			"scBulletGun",
			"scAutoBulletGun",
			"scRifle",
			"scRifleUpg",
			"scAirWeapon",
			"scFastRockets",
			"scBlaster",
			"scBlasterUpg",
			"scTankLaser",
			"scAirLaser",
			"scLaserMine",
			"scBurnMine",
			"scSonar",
			"scRocket",
			"scPulsator",
			"scPlazmaGun",
			"scFireGun",
			"scRipper",
			"scSphereGun",
			"scTorpeda",
			"scSpikes",
			"scTrinity",
			"scShotGun",
			"scMagnetic",
			"scSapper",
			"scPhoenix",
			"scGrenade",
			"scDisc",
			"scMiniGun",
			"scArtillery",
			"scScatter",
			"scBarrel",
			"scGraviton",
			"scAspirine",
			"scSashka",
			"scShocker",
			"scAcid",
			"scMaslo",
			"scCrater",
			"scMineSpike",
			"scMineRip",
			"scMine",
			"scBlazer",
			"scHyper",
			"scHyperNitro",
			"scEazyNitro",
			"scShell",
			"scTurel",
			"scMortira",
			"scMolotov",
			"scPhaseImpulse",
			"scHyperBlaster",
			"scMinePropton",
			"scSpring",
			"scSpring2",
			"scAsyncFrost",
			"scAsyncFrost2",

			"scBulletGunInfo",
			"scRifleInfo",
			"scRifleUpgInfo",
			"scAirWeaponInfo",
			"scFastRocketsInfo",
			"scBlasterInfo",
			"scBlasterUpgInfo",
			"scTankLaserInfo",
			"scAirLaserInfo",
			"scLaserMineInfo",
			"scBurnMineInfo",
			"scSonarInfo",
			"scRocketInfo",
			"scPulsatorInfo",
			"scPlazmaGunInfo",
			"scFireGunInfo",
			"scRipperInfo",
			"scSphereGunInfo",
			"scTorpedaInfo",
			"scSpikesInfo",
			"scTrinityInfo",
			"scShotGunInfo",
			"scMagneticInfo",
			"scSapperInfo",
			"scPhoenixInfo",
			"scGrenadeInfo",
			"scDiscInfo",
			"scMiniGunInfo",
			"scArtilleryInfo",
			"scScatterInfo",
			"scBarrelInfo",
			"scGravitonInfo",
			"scAspirineInfo",
			"scSashkaInfo",
			"scShockerInfo",
			"scAcidInfo",
			"scMasloInfo",
			"scCraterInfo",
			"scMineSpikeInfo",
			"scMineRipInfo",
			"scMineInfo",
			"scBlazer",
			"scHyperInfo",
			"scHyperNitroInfo",
			"scEazyNitroInfo",
			"scShellInfo",
			"scTurelInfo",
			"scMortiraInfo",
			"scMolotovInfo",
			"scPhaseImpulseInfo",
			"scHyperBlasterInfo",
			"scMineProptonInfo",
			"scSpringInfo",
			"scSpring2Info",
			"scAsyncFrostInfo",
			"scAsyncFrost2Info",

			"scTyler",
			"scSnake",
			"scTarquin",
			"scRip",
			"scShred",
			"scButcher",
			"scGank",
			"scGerry",
			"scKJin",
			"scMardock",
			"scStinkle",
			"scViolet",
			"scViper",
			"scSpectator",

			"scTylerInfo",
			"scSnakeInfo",
			"scTarquinInfo",
			"scRipInfo",
			"scShredInfo",
			"scButcherInfo",
			"scGankInfo",
			"scGerryInfo",
			"scKJinInfo",
			"scMardockInfo",
			"scStinkleInfo",
			"scVioletInfo",
			"scViperInfo",
			"scSpectatorInfo",

			"svOpen",
			"svClosed",
			"svUnavailable",
			"svUnavailableTitulA",
			"svCompleted",
			"svRequestPoints",
			"svPrice",
			"svMoney",
			"svPoints",
			"svYourMoneys",
			"svLap",
			"svRewards",

			"svHintCantStart",
			"svHintCantContinue",
			"svHintCantNewGame",
			"svHintCantFlyPlanet",
			"svHintCantMoney",
			"svHintCantWpnUpg",
			"svHintCantPoints",
			"svHintNeedReload",
			"svHintExitRace",
			"svHintUpgrade",
			"svHintWeaponNotSupport",
			"svHintDisconnect",
			"svCriticalNetError",
			"svHintPlayersIsNotReady",
			"svHintYouCanFlyPlanet",
			"svHintYouCanFlyPlanetClient",
			"svHintYouCompletePass",
			"svHintYouSponsorMoney",
			"svHintYouNotCompletePass",
			"svHintConnecting",
			"svHintRefreshing",
			"svHintHostListEmpty",
			"svHintHostConnectionFailed",
			"svHintSetColorFailed",
			"svHintSetGamerFailed",
			"svHintPleaseWait",
			"svHintLeaversWillBeRemoved",

			"svBuyWeapon",
			"svSellWeapon",
			"svBuyCar",
			"svYouReadyFlyPlanet",
			"svYouReadyStayPlanet",
			"svBuyReward",

			"svCredits"
		};

		const std::string cLangCharsetStr[cLangCharsetEnd] = {"lcDefault", "lcEastEurope", "lcRussian", "lcBaltic"};
		const DWORD cLangCharsetCode[cLangCharsetEnd] = {
			DEFAULT_CHARSET, EASTEUROPE_CHARSET, RUSSIAN_CHARSET, BALTIC_CHARSET
		};


		ComplexMesh::ComplexMesh(ComplexMeshLib* owner): _owner(owner), _tag(-1), _enableTBN(false), _mesh(nullptr),
		                                                 _ivbMesh(nullptr), _meshX(nullptr), _pxMesh(nullptr)
		{
		}

		ComplexMesh::~ComplexMesh()
		{
			SetTag(-1);

			ReleaseAll();
		}

		void ComplexMesh::Load(bool loadData, bool initIVB)
		{
#ifndef _DEBUG
			if (loadData)
			{
				res::MeshData* data = GetOrCreateMesh();
				if (!_mesh->IsInit())
					data->Load();
			}

			if (initIVB)
			{
				graph::IndexedVBMesh* ivb = GetOrCreateIVBMesh();
				ivb->Init(*_owner->GetEngine());
			}
#endif
		}

		void ComplexMesh::Unload()
		{
#ifndef _DEBUG
			if (_ivbMesh)
				_ivbMesh->Free();
			if (_meshX)
				_meshX->Free();
			if (_pxMesh)
				LSL_ASSERT(_pxMesh->IsEmpty());
			if (_mesh)
				_mesh->Free();
#endif
		}

		res::MeshData* ComplexMesh::GetOrCreateMesh()
		{
			if (!_mesh)
			{
				_mesh = &_owner->_meshLib->Add();
				_mesh->SetName(_name);
				_mesh->SetFileName(_fileName);
			}

			if (_enableTBN && !_mesh->IsInit())
			{
				_mesh->Load();
				_mesh->CalcTangentSpace();
			}

			return _mesh;
		}

		void ComplexMesh::ReleaseMesh()
		{
			if (_mesh)
				_owner->_meshLib->Delete(_mesh);
			_mesh = nullptr;
		}

		graph::IndexedVBMesh* ComplexMesh::GetOrCreateIVBMesh()
		{
			if (!_ivbMesh)
			{
				_ivbMesh = &_owner->_ivbMeshLib->Add();
				_ivbMesh->SetName(GetName());
			}

			_ivbMesh->SetData(GetOrCreateMesh());

			return _ivbMesh;
		}

		void ComplexMesh::ReleaseIVBMesh()
		{
			if (_ivbMesh)
				_owner->_ivbMeshLib->Delete(_ivbMesh);
			_ivbMesh = nullptr;
		}

		graph::MeshX* ComplexMesh::GetOrCreateMeshX()
		{
			if (!_meshX)
			{
				_meshX = &_owner->_meshXLib->Add();
				_meshX->SetName(GetName());
				//_meshX->SetData(GetOrCreateMesh());
			}

			return _meshX;
		}

		void ComplexMesh::ReleaseMeshX()
		{
			if (_meshX)
				_owner->_meshXLib->Delete(_meshX);
			_meshX = nullptr;
		}

		px::TriangleMesh* ComplexMesh::GetOrCreatePxMesh()
		{
			if (!_pxMesh)
			{
				_pxMesh = &_owner->_pxMeshLib->Add();
				_pxMesh->SetName(GetName());
				_pxMesh->SetMeshData(GetOrCreateMesh());
			}

			return _pxMesh;
		}

		void ComplexMesh::ReleasePxMesh()
		{
			if (_pxMesh)
				_owner->_pxMeshLib->Delete(_pxMesh);
			_pxMesh = nullptr;
		}

		void ComplexMesh::ReleaseAll()
		{
			ReleasePxMesh();
			ReleaseIVBMesh();
			ReleaseMesh();
		}

		void ComplexMesh::Reload()
		{
			if (_mesh && _mesh->IsInit())
				_mesh->Reload();
			if (_ivbMesh && _ivbMesh->IsInit())
				_ivbMesh->Reload();
			if (_meshX && _meshX->IsInit())
				_meshX->Reload();
			//Не является ресурсом, значит операции недоступны
			//if (_pxMesh && _pxMesh->IsInit())
			//	_pxMesh->Reload();
		}

		int ComplexMesh::GetTag() const
		{
			return _tag;
		}

		void ComplexMesh::SetTag(int value)
		{
			if (_tag != value)
			{
				_owner->TagChanged(this, _tag, value);
				_tag = value;
			}
		}

		const std::string& ComplexMesh::GetName() const
		{
			return _name;
		}

		void ComplexMesh::SetName(const std::string& value)
		{
			_name = value;

			if (_mesh)
				_mesh->SetName(value);
			if (_ivbMesh)
				_ivbMesh->SetName(value);
			if (_meshX)
				_meshX->SetName(value);
			if (_pxMesh)
				_pxMesh->SetName(value);
		}

		const std::string& ComplexMesh::GetFileName() const
		{
			return _fileName;
		}

		void ComplexMesh::SetFileName(const std::string& value)
		{
			_fileName = value;
			if (_mesh)
				_mesh->SetFileName(value);
		}

		bool ComplexMesh::GetEnableTBN() const
		{
			return _enableTBN;
		}

		void ComplexMesh::SetEnableTBN(bool value)
		{
			_enableTBN = value;
		}


		ComplexMeshLib::ComplexMeshLib(graph::Engine* engine): _engine(engine)
		{
			_meshLib = new _MeshLib(this);
			_meshLib->SetName("meshLib");
			_meshLib->SetOwner(this);

			_ivbMeshLib = new _IVBMeshLib(this);
			_ivbMeshLib->SetName("ivbMeshLib");
			_ivbMeshLib->SetOwner(this);

			_meshXLib = new _MeshXLib(this);
			_meshXLib->SetName("meshXLib");
			_meshXLib->SetOwner(this);

			_pxMeshLib = new _PxMeshLib(this);
			_pxMeshLib->SetName("pxMeshLib");
			_pxMeshLib->SetOwner(this);
		}

		ComplexMeshLib::~ComplexMeshLib()
		{
			Clear();

			delete _pxMeshLib;
			delete _ivbMeshLib;
			delete _meshXLib;
			delete _meshLib;
		}

		void ComplexMeshLib::TagChanged(const Value& value, int prevVal, int newVal)
		{
			if (prevVal >= 0)
			{
				prevVal = prevVal & cTagIndexMask;
				_tagList[prevVal].Remove(value);
			}

			if (newVal >= 0)
			{
				newVal = newVal & cTagIndexMask;
				_tagList[newVal].push_back(value);
			}
		}

		void ComplexMeshLib::Save(SWriter* writer)
		{
			_MyBase::Save(writer);
			Component::Save(writer);
		}

		void ComplexMeshLib::Load(SReader* reader)
		{
			_MyBase::Load(reader);
			Component::Load(reader);
		}

		CollectionItem* ComplexMeshLib::ComplexFind(CollectionTraits* traits, const std::string& name)
		{
			ComplexMesh* complexMesh = Find(name);
			if (!complexMesh)
				return nullptr;

			if (traits == _meshLib)
				return complexMesh->GetOrCreateMesh();
			if (traits == _ivbMeshLib)
				return complexMesh->GetOrCreateIVBMesh();
			if (traits == _meshXLib)
				return complexMesh->GetOrCreateMeshX();
			if (traits == _pxMeshLib)
				return complexMesh->GetOrCreatePxMesh();

			LSL_ASSERT(false);

			return nullptr;
		}

		ComplexMesh* ComplexMeshLib::Find(const std::string& name)
		{
			for (auto iter = begin(); iter != end(); ++iter)
				if ((*iter)->GetName() == name)
					return *iter;

			return nullptr;
		}

		ComplexMesh& ComplexMeshLib::Get(const std::string& name)
		{
			ComplexMesh* mesh = Find(name);
			if (mesh == nullptr)
				throw Error("Complex mesh " + name + " does not exist");

			return *mesh;
		}

		const ComplexMeshLib::MeshList& ComplexMeshLib::GetMeshList(int tag)
		{
			return _tagList[tag];
		}

		ComplexMesh& ComplexMeshLib::LoadMesh(const std::string& name, bool buildTBN, bool loadData, bool initIVB,
		                                      int tag)
		{
			ComplexMesh& mesh = Add();
			mesh.SetTag(tag);
			mesh.SetName(name);
			mesh.SetFileName("Data\\" + name);
			mesh.SetEnableTBN(buildTBN);
			mesh.Load(loadData, initIVB);

			return mesh;
		}

		graph::Engine* ComplexMeshLib::GetEngine()
		{
			return _engine;
		}


		ComplexImage::ComplexImage(ComplexImageLib* owner): _owner(owner), _tag(-1), _levelCnt(1), _gui(false),
		                                                    _image(nullptr), _tex2d(nullptr), _cubeImage(nullptr),
		                                                    _cubeTex(nullptr)
		{
		}

		ComplexImage::~ComplexImage()
		{
			SetTag(-1);

			ReleaseAll();
		}

		void ComplexImage::Load(bool initTex2d, bool initCubeTex)
		{
#ifndef _DEBUG
			if (initTex2d)
			{
				graph::Tex2DResource* tex = GetOrCreateTex2d();
				tex->Init(*_owner->GetEngine());
			}

			if (initCubeTex)
			{
				graph::TexCubeResource* tex = GetOrCreateCubeTex();
				tex->Init(*_owner->GetEngine());
			}
#endif
		}

		void ComplexImage::Unload()
		{
#ifndef _DEBUG
			if (_tex2d)
				_tex2d->Free();
			if (_cubeTex)
				_cubeTex->Free();
			if (_image)
				_image->Free();
			if (_cubeImage)
				_cubeImage->Free();
#endif
		}

		res::ImageResource* ComplexImage::GetOrCreateImage()
		{
			if (!_image)
			{
				_image = &_owner->_imageLib->Add();
				_image->SetName(_name);
				_image->SetFileName(_fileName);
			}

			return _image;
		}

		void ComplexImage::ReleaseImage()
		{
			if (_image)
				_owner->_imageLib->Delete(_image);
			_image = nullptr;
		}

		graph::Tex2DResource* ComplexImage::GetOrCreateTex2d()
		{
			if (!_tex2d)
			{
				_tex2d = &_owner->_tex2dLib->Add();
				_tex2d->SetName(_name);
				_tex2d->SetLevelCnt(_levelCnt);
				if (_levelCnt != 1)
					_tex2d->SetUsage(D3DUSAGE_AUTOGENMIPMAP);
				_tex2d->SetGUI(_gui);

				_tex2d->SetData(GetOrCreateImage());
			}

			return _tex2d;
		}

		void ComplexImage::ReleaseTex2d()
		{
			if (_tex2d)
				_owner->_tex2dLib->Delete(_tex2d);
			_tex2d = nullptr;
		}

		res::CubeImageResource* ComplexImage::GetOrCreateCubeImage()
		{
			if (!_cubeImage)
			{
				_cubeImage = &_owner->_cubeImageLib->Add();
				_cubeImage->SetName(_name);
				_cubeImage->SetFileName(_fileName);
			}

			return _cubeImage;
		}

		void ComplexImage::ReleaseCubeImage()
		{
			if (_cubeImage)
				_owner->_cubeImageLib->Delete(_cubeImage);
			_cubeImage = nullptr;
		}

		graph::TexCubeResource* ComplexImage::GetOrCreateCubeTex()
		{
			LSL_ASSERT(_cubeImage);

			if (!_cubeTex)
			{
				_cubeTex = &_owner->_cubeTexLib->Add();
				_cubeTex->SetName(_name);
				//not supported while
				_cubeTex->SetLevelCnt(1);
				_cubeTex->SetData(GetOrCreateCubeImage());
			}

			return _cubeTex;
		}

		void ComplexImage::ReleaseCubeTex()
		{
			if (_cubeTex)
				_owner->_cubeTexLib->Delete(_cubeTex);
			_cubeTex = nullptr;
		}

		void ComplexImage::ReleaseAll()
		{
			ReleaseCubeTex();
			ReleaseCubeImage();
			ReleaseTex2d();
			ReleaseImage();
		}

		void ComplexImage::Reload()
		{
			if (_image && _image->IsInit())
				_image->Reload();
			if (_tex2d && _tex2d->IsInit())
				_tex2d->Reload();
			if (_cubeImage && _cubeImage->IsInit())
				_cubeImage->Reload();
			if (_cubeTex && _cubeTex->IsInit())
				_cubeTex->Reload();
		}

		int ComplexImage::GetTag() const
		{
			return _tag;
		}

		void ComplexImage::SetTag(int value)
		{
			if (_tag != value)
			{
				_owner->TagChanged(this, _tag, value);
				_tag = value;
			}
		}

		const std::string& ComplexImage::GetName() const
		{
			return _name;
		}

		void ComplexImage::SetName(const std::string& value)
		{
			_name = value;

			if (_image)
				_image->SetName(value);
			if (_tex2d)
				_tex2d->SetName(value);
			if (_cubeImage)
				_cubeImage->SetName(value);
			if (_cubeTex)
				_cubeTex->SetName(value);
		}

		const std::string& ComplexImage::GetFileName() const
		{
			return _fileName;
		}

		void ComplexImage::SetFileName(const std::string& value)
		{
			_fileName = value;
		}

		unsigned ComplexImage::GetLevelCnt() const
		{
			return _levelCnt;
		}

		void ComplexImage::SetLevelCnt(unsigned value)
		{
			_levelCnt = value;
		}

		bool ComplexImage::GetGUI() const
		{
			return _gui;
		}

		void ComplexImage::SetGUI(bool value)
		{
			_gui = value;
		}


		ComplexImageLib::ComplexImageLib(graph::Engine* engine): _engine(engine)
		{
			_imageLib = new _ImageLib(this);
			_imageLib->SetName("imageLib");
			_imageLib->SetOwner(this);

			_tex2dLib = new _Tex2dLib(this);
			_tex2dLib->SetName("tex2dLib");
			_tex2dLib->SetOwner(this);

			_cubeImageLib = new _CubeImageLib(this);
			_cubeImageLib->SetName("cubeImageLib");
			_cubeImageLib->SetOwner(this);

			_cubeTexLib = new _CubeTexLib(this);
			_cubeTexLib->SetName("cubeTexLib");
			_cubeTexLib->SetOwner(this);
		}

		ComplexImageLib::~ComplexImageLib()
		{
			Clear();

			delete _cubeTexLib;
			delete _cubeImageLib;
			delete _tex2dLib;
			delete _imageLib;
		}

		void ComplexImageLib::TagChanged(const Value& value, int prevVal, int newVal)
		{
			if (prevVal >= 0)
			{
				prevVal = prevVal & cTagIndexMask;
				_tagList[prevVal].Remove(value);
			}

			if (newVal >= 0)
			{
				newVal = newVal & cTagIndexMask;
				_tagList[newVal].push_back(value);
			}
		}

		void ComplexImageLib::Save(SWriter* writer)
		{
			_MyBase::Save(writer);
			Component::Save(writer);
		}

		void ComplexImageLib::Load(SReader* reader)
		{
			_MyBase::Load(reader);
			Component::Load(reader);
		}

		CollectionItem* ComplexImageLib::ComplexFind(CollectionTraits* traits, const std::string& name)
		{
			ComplexImage* complexImage = Find(name);
			if (!complexImage)
				return nullptr;

			if (traits == _imageLib)
				return complexImage->GetOrCreateImage();
			if (traits == _tex2dLib)
				return complexImage->GetOrCreateTex2d();
			if (traits == _cubeImageLib)
				return complexImage->GetOrCreateCubeImage();
			if (traits == _cubeTexLib)
				return complexImage->GetOrCreateCubeTex();

			LSL_ASSERT(false);

			return nullptr;
		}

		ComplexImage* ComplexImageLib::Find(const std::string& name)
		{
			for (auto iter = begin(); iter != end(); ++iter)
				if ((*iter)->GetName() == name)
					return *iter;

			return nullptr;
		}

		ComplexImage& ComplexImageLib::Get(const std::string& name)
		{
			ComplexImage* image = Find(name);
			if (image == nullptr)
				throw Error("Complex image " + name + " does not exist");

			return *image;
		}

		ComplexImage& ComplexImageLib::LoadImage(const std::string& name, unsigned levelCnt, bool initTex2d,
		                                         bool initCubeTex, int tag, bool gui)
		{
			ComplexImage& image = Add();
			image.SetName(name);
			image.SetTag(tag);
			image.SetLevelCnt(levelCnt);
			image.SetGUI(gui);
			image.SetFileName("Data\\" + name);
			image.Load(initTex2d, initCubeTex);

			return image;
		}

		graph::Engine* ComplexImageLib::GetEngine()
		{
			return _engine;
		}


		ComplexMatLib::ComplexMatLib(ResourceManager* resManager): _resManager(resManager)
		{
			LSL_ASSERT(_resManager);
		}

		graph::LibMaterial& ComplexMatLib::Get(const std::string& name)
		{
			graph::LibMaterial* libMat = Find(name);
			if (libMat == nullptr)
				throw Error("LibMaterial" + name + " does not exist");

			return *libMat;
		}

		graph::LibMaterial& ComplexMatLib::LoadLibMat(const std::string& name, bool sprite,
		                                              graph::Material::Blending blending, const FloatRange& alpha,
		                                              const ColorRange& color, const ColorRange& ambient)
		{
			graph::LibMaterial& libMat = Add();
			libMat.SetName(name);
			libMat.material.SetBlending(blending);
			libMat.material.SetAlpha(alpha);
			libMat.material.SetDiffuse(color);
			libMat.material.SetAmbient(ambient);

			if (sprite)
			{
				libMat.material.SetOption(graph::Material::moLighting, false);
				libMat.material.SetOption(graph::Material::moZWrite, false);
				libMat.material.SetOption(graph::Material::moIgnoreFog, true);
			}

			return libMat;
		}

		void ComplexMatLib::SetAlphaTestTo(graph::LibMaterial& libMat, graph::Material::AlphaTest alphaTest,
		                                   const FloatRange& alphaRef)
		{
			libMat.material.SetAlphaTest(alphaTest);
			libMat.material.SetAlphaRef(alphaRef);
		}

		graph::Sampler2d& ComplexMatLib::AddSampler2dTo(graph::LibMaterial& libMat, const std::string& imgName,
		                                                graph::Sampler2d::Mode alphaMode,
		                                                graph::BaseSampler::Filtering filtering)
		{
			graph::Tex2DResource* tex2d = nullptr;
			if (!imgName.empty())
				tex2d = _resManager->GetImageLib().Get(imgName).GetOrCreateTex2d();
			graph::Sampler2d& res = libMat.samplers.Add2d(tex2d);

			res.SetAlphaMode(alphaMode);
			res.SetFiltering(filtering);

			return res;
		}


		ComplexTextFontLibrary::ComplexTextFontLibrary(ResourceManager* resManager): _resManager(resManager)
		{
		}

		graph::TextFont& ComplexTextFontLibrary::Get(const std::string& name)
		{
			graph::TextFont* font = Find(name);
			if (font == nullptr)
				throw Error("TextFont" + name + " does not exist");

			return *font;
		}

		graph::TextFont& ComplexTextFontLibrary::LoadFont(const std::string& name, int height, unsigned weight,
		                                                  bool italic, DWORD charset, const std::string& faceName)
		{
			graph::TextFont& font = Add();
			font.SetName(name);

			graph::TextFont::Desc desc;
			desc.height = height;
			desc.weight = weight;
			desc.italic = italic;
			desc.charSet = charset;
			desc.pFacename = faceName;
			font.SetDesc(desc);

			return font;
		}


		StringLibrary::StringLibrary(ResourceManager* resManager): _resManager(resManager)
		{
		}

		void StringLibrary::Save(std::ostream& stream)
		{
			for (StringMap::const_iterator iter = _stringMap.begin(); iter != _stringMap.end(); ++iter)
				stream << iter->first << " \"" << iter->second << "\"\n";
		}

		void StringLibrary::Load(std::istream& stream)
		{
			string id;

			stream.seekg(0, std::ios::end);
			unsigned size = stream.tellg();
			stream.seekg(0, std::ios::beg);

			auto data = new char[size];
			stream.read(data, size);

			std::stringstream sstream(ConvertStrWToA((wchar_t*)data, size / 2, CP_THREAD_ACP));
			sstream.get();
			LSL_ASSERT(!sstream.fail());

			delete[] data;

			while (sstream)
			{
				string str;
				sstream >> str;

				if (!str.empty() && str[0] == '\"')
				{
					char cur;
					str.erase(0, 1);
					int endPos = str.find('\"');
					if (endPos >= 0)
						str.erase(endPos, 1);
					else
						while (sstream.get(cur) && cur != '\"')
							str.push_back(cur);

					if (!id.empty())
					{
						while (true)
						{
							int pos = str.find("\\n");
							if (pos < 0)
								break;
							str.replace(pos, 2, 1, '\n');
						}

						Set(id, str);
						id.clear();
					}
				}
				else
					id = str;
			}
		}

		void StringLibrary::SaveToFile(const string& file)
		{
			std::ostream* stream = FileSystem::GetInstance()->NewOutStream(file, FileSystem::omText, 0);
			if (stream)
			{
				try
				{
					Save(*stream);
				}
				LSL_FINALLY(lsl::FileSystem::GetInstance()->FreeStream(stream);)
			}
		}

		void StringLibrary::LoadFromFile(const string& file)
		{
			std::istream* stream = nullptr;
			try
			{
				stream = FileSystem::GetInstance()->NewInStream(file, FileSystem::omBinary, 0);
			}
			catch (EUnableToOpen&) { return; }

			if (stream)
			{
				try
				{
					Load(*stream);
				}
				LSL_FINALLY(lsl::FileSystem::GetInstance()->FreeStream(stream);)
			}
		}

		const std::string& StringLibrary::Get(const std::string& id) const
		{
			auto iter = _stringMap.find(id);
			if (iter != _stringMap.end() && iter->second != "")
				return iter->second;

			return id;
		}

		void StringLibrary::Set(const std::string& id, const string& value)
		{
			_stringMap[id] = value;
		}

		bool StringLibrary::Has(const std::string& id) const
		{
			auto iter = _stringMap.find(id);

			return iter != _stringMap.end() && iter->second != "";
		}

		const std::string& StringLibrary::Get(StringValue value) const
		{
			return Get(cStringValueStr[value]);
		}

		void StringLibrary::Set(StringValue id, const string& value)
		{
			Set(cStringValueStr[id], value);
		}

		bool StringLibrary::Has(StringValue value) const
		{
			return Has(cStringValueStr[value]);
		}


		ResourceManager::ResourceManager(World* world): _world(world), _fontCharset(lcDefault),
		                                                _worldType(Planet::cWorldTypeEnd)
		{
			LSL_ASSERT(_world);

			_meshLib = new MeshLib(&_world->GetGraph()->GetEngine());
			_meshLib->SetName("meshLib");
			_meshLib->SetOwner(this);

			_imageLib = new ImageLib(&_world->GetGraph()->GetEngine());
			_imageLib->SetName("imageLib");
			_imageLib->SetOwner(this);

			_shaderLib = new ShaderLib();
			_shaderLib->SetName("shaderLib");
			_shaderLib->SetOwner(this);
			ShaderLib::ClassList* shClasses = _shaderLib->GetOrCreateClassList();
			//shClasses->Add<graph::MappingShaderReflection>(stReflection);
			//shClasses->Add<graph::BumpMapShader>(stBumMap);

			_matLib = new ComplexMatLib(this);
			_matLib->SetName("matLib");
			_matLib->SetOwner(this);

			_textFontLib = new TextFontLib(this);
			_textFontLib->SetName("textFontLib");
			_textFontLib->SetOwner(this);

			_stringLib = new StringLib(this);
			_stringLib->SetName("stringLib");
			_stringLib->SetOwner(this);

			_soundLib = world->GetAudio()->CreateSoundLib();
			_soundLib->SetName("soundLib");
			_soundLib->SetOwner(this);
		}

		ResourceManager::~ResourceManager()
		{
			_world->GetAudio()->ReleaseSoundLib(_soundLib);
			delete _stringLib;
			delete _textFontLib;
			delete _matLib;
			delete _shaderLib;
			delete _imageLib;
			delete _meshLib;
		}

		ComplexMesh& ResourceManager::LoadMesh(const std::string& name, bool buildTBN, bool loadData, bool initIVB,
		                                       int tag)
		{
			return _meshLib->LoadMesh(name, buildTBN, loadData, initIVB, tag);
		}

		ComplexImage& ResourceManager::LoadImage(const std::string& name, unsigned levelCnt, bool initTex2d,
		                                         bool initCubeTex, int tag, bool gui)
		{
			return _imageLib->LoadImage(name, levelCnt, initTex2d, initCubeTex, tag, gui);
		}

		graph::Sampler2d& ResourceManager::AddSampler2dTo(graph::LibMaterial& libMat, const std::string& imgName,
		                                                  graph::Sampler2d::Mode alphaMode,
		                                                  graph::BaseSampler::Filtering filtering)
		{
			return _matLib->AddSampler2dTo(libMat, imgName, alphaMode, filtering);
		}

		void ResourceManager::BuildAnimByOff(graph::Sampler2d& sampler, const Vec2Range& texCoord,
		                                     const Point2U& tileCnt)
		{
			//ОСТОРОЖНО! При загрузке анимированной текстуры необходимо подгрузить изображение чтобы получить размеры, что конечно змедляет загрузку. Необходимо кеширвоать данные, что в свою очередь требует вести сериализацию. Поэтому по так...
			if (texCoord.GetMin() != NullVec2 || texCoord.GetMax() != IdentityVec2 || tileCnt.x != 1 || tileCnt.y != 1)
			{
				if (!sampler.GetTex()->IsInit())
					sampler.GetTex()->Init(_world->GetGraph()->GetEngine());
				sampler.BuildAnimByOff(texCoord, tileCnt);
			}
		}

		graph::LibMaterial& ResourceManager::LoadLibMat(const std::string& name, bool sprite,
		                                                graph::Material::Blending blending, const FloatRange& alpha,
		                                                const ColorRange& color, const ColorRange& ambient)
		{
			return _matLib->LoadLibMat(name, sprite, blending, alpha, color, ambient);
		}

		graph::LibMaterial& ResourceManager::LoadImage2dLibMat(const std::string& name, const std::string& imgName,
		                                                       bool sprite, graph::Material::Blending blending,
		                                                       const FloatRange& alpha, const ColorRange& color,
		                                                       const Vec2Range& texCoord, const Point2U& tileCnt)
		{
			graph::LibMaterial& libMat = LoadLibMat(name, sprite, blending, alpha, color);
			//режим альфы по умолчанию , modulate в виду наличия прозрачности при отсечении
			graph::Sampler2d& sampler = AddSampler2dTo(libMat, imgName, graph::Sampler2d::tmModulate,
			                                           sprite
				                                           ? graph::BaseSampler::sfLinear
				                                           : graph::BaseSampler::sfAnisotropic);

			BuildAnimByOff(sampler, texCoord, tileCnt);

			return libMat;
		}

		graph::LibMaterial& ResourceManager::LoadImage2dLibMatAnim(const std::string& name, const std::string& imgName,
		                                                           bool sprite, graph::Material::Blending blending,
		                                                           const FloatRange& alpha, const ColorRange& color,
		                                                           const Vec3Range& texOffset,
		                                                           const Vec3Range& texScale, const QuatRange& texRot)
		{
			graph::LibMaterial& libMat = LoadLibMat(name, sprite, blending, alpha, color);
			//режим альфы по умолчанию , modulate в виду наличия прозрачности при отсечении
			graph::Sampler2d& sampler = AddSampler2dTo(libMat, imgName, graph::Sampler2d::tmModulate,
			                                           sprite
				                                           ? graph::BaseSampler::sfLinear
				                                           : graph::BaseSampler::sfAnisotropic);

			sampler.SetOffset(texOffset);
			sampler.SetScale(texScale);
			sampler.SetRotate(texRot);

			return libMat;
		}

		graph::LibMaterial& ResourceManager::LoadAlphaTestLibMat(const std::string& name, const std::string& imgName,
		                                                         bool sprite, float alphaRef)
		{
			graph::LibMaterial& libMat = LoadImage2dLibMat(name, imgName,
			                                               sprite/*, graph::Material::bmTransparency, 1.0f*/);
			_matLib->SetAlphaTestTo(libMat, graph::Material::asLessOrEq, alphaRef);

			return libMat;
		}

		graph::LibMaterial& ResourceManager::LoadSpecLibMat(const std::string& name, const std::string& imgName,
		                                                    bool sprite)
		{
			graph::LibMaterial& libMat = LoadImage2dLibMat(name, imgName, sprite);
			libMat.material.SetSpecPower(80.0f);
			libMat.material.SetSpecular(D3DXCOLOR(1, 1, 1, 1));

			return libMat;
		}

		graph::LibMaterial& ResourceManager::LoadBumpLibMat(const std::string& name, const std::string& imgName,
		                                                    const std::string& normMap, bool sprite,
		                                                    graph::Material::Blending blending, float alpha)
		{
			graph::LibMaterial& libMat = LoadLibMat(name, sprite, blending, alpha);
			AddSampler2dTo(libMat, imgName, graph::Sampler2d::tmModulate,
			               sprite ? graph::BaseSampler::sfLinear : graph::BaseSampler::sfAnisotropic);

			AddSampler2dTo(libMat, normMap);
			libMat.samplers[1].stageStates.Set(graph::tssColorOp, D3DTOP_DISABLE);
			libMat.samplers[1].stageStates.Set(graph::tssAlphaOp, D3DTOP_DISABLE);

			//libMat.material.SetSpecPower(128.0f);
			//libMat.material.SetSpecular(D3DXCOLOR(0.5f, 0.5f, 0.5f, 1.0f));	

			return libMat;
		}

		graph::LibMaterial& ResourceManager::LoadAnimImage2dLibMat(const std::string& name, const std::string& imgName,
		                                                           const Vec3Range& offset, const Vec3Range& scale,
		                                                           const QuatRange& rot, bool sprite,
		                                                           graph::Material::Blending blending,
		                                                           const FloatRange& alpha, const ColorRange& color)
		{
			graph::LibMaterial& libMat = LoadLibMat(name, sprite, blending, alpha, color);
			//режим альфы по умолчанию , modulate в виду наличия прозрачности при отсечении
			graph::Sampler2d& sampler = AddSampler2dTo(libMat, imgName, graph::Sampler2d::tmModulate,
			                                           sprite
				                                           ? graph::BaseSampler::sfLinear
				                                           : graph::BaseSampler::sfAnisotropic);
			sampler.SetOffset(offset);
			sampler.SetScale(scale);
			sampler.SetRotate(rot);

			return libMat;
		}

		graph::TextFont& ResourceManager::LoadFont(const std::string& name, int height, unsigned weight, bool italic,
		                                           const std::string& faceName)
		{
			return _textFontLib->LoadFont(name, height, weight, italic, cLangCharsetCode[_fontCharset], faceName);
		}

		snd::Sound* ResourceManager::LoadSound(const std::string& name, const std::string& pathRoot,
		                                       const std::string& path, float volume, float distScaller, bool load,
		                                       bool loadCheckExists)
		{
			string pathFile = "Data\\" + pathRoot + path + name;

			snd::Sound* sound = _soundLib->Find(pathRoot + name);
			if (sound == nullptr)
				sound = &_soundLib->Add();

			sound->SetName(pathRoot + name);
			sound->SetFileName(pathFile);
			sound->SetVolume(volume);

#ifndef _DEBUG
			if (load && (!loadCheckExists || FileSystem::GetInstance()->FileExists(pathFile)))
				sound->Load();
#endif

			return sound;
		}

		snd::Sound* ResourceManager::LoadSound(const std::string& name, float volume, float distScaller, bool load,
		                                       bool loadCheckExists)
		{
			return LoadSound(name, "", "", volume, distScaller, load, loadCheckExists);
		}

		void ResourceManager::LoadEffects()
		{
			LSL_LOG("resourceManager load effects");

			LoadMesh("Effect\\pieces1.r3d", false, true, true);
			LoadMesh("CarDestr\\destrCar.r3d", false, true, true);
			LoadMesh("Effect\\wheel.r3d", false, true, true);
			LoadMesh("Effect\\truba.r3d", false, true, true);
			LoadMesh("Effect\\frost.r3d", false, true, true);
			LoadMesh("Effect\\sphere.r3d", false, true, true);


			LoadImage("Effect\\gunEff2.dds", 1, true, false);
			LoadImage("Effect\\explosion1.dds", 1, true, false);
			LoadImage("Effect\\explosion2.dds", 1, true, false);
			LoadImage("Effect\\explosion3.dds", 1, true, false);
			LoadImage("Effect\\explosion4.dds", 1, true, false);
			LoadImage("Effect\\engine1.dds", 1, true, false);
			LoadImage("Effect\\ring1.dds", 1, true, false);
			LoadImage("Effect\\asphaltMarks.dds", cGenMipLevel, true, false);
			LoadImage("Effect\\pieces.dds", cGenMipLevel, true, false);
			LoadImage("Effect\\smoke1.dds", 1, true, false);
			LoadImage("Effect\\smoke2.dds", 1, true, false);
			LoadImage("Effect\\smoke3.dds", 1, true, false);
			LoadImage("Effect\\smoke6.dds", 1, true, false);
			LoadImage("Effect\\smoke7.dds", 1, true, false);
			LoadImage("Effect\\eagle.dds", 1, true, false);
			LoadImage("Effect\\drop.dds", 1, true, false);
			LoadImage("Effect\\snowball.dds", 1, true, false);
			LoadImage("Effect\\sandy.dds", 1, true, false);
			LoadImage("Effect\\sandyIso.dds", 1, true, false);
			LoadImage("Effect\\snowballIso.dds", 1, true, false);
			LoadImage("Effect\\spark1.dds", 1, true, false);
			LoadImage("Effect\\spark2.dds", 1, true, false);
			LoadImage("CarDestr\\destrCar.dds", cGenMipLevel, true, false);
			LoadImage("Effect\\wheel.dds", cGenMipLevel, true, false);
			LoadImage("Effect\\truba.dds", cGenMipLevel, true, false);
			LoadImage("Effect\\fire1.dds", 1, true, false);
			LoadImage("Effect\\fire2.dds", 1, true, false);
			LoadImage("Effect\\flare1.dds", 1, true, false);
			LoadImage("Effect\\flare2.dds", 1, true, false);
			LoadImage("Effect\\flare5.dds", 1, true, false);
			LoadImage("Effect\\laser3-blue.dds", 1, true, false);
			LoadImage("Effect\\laser3-green.dds", 1, true, false);
			LoadImage("Effect\\laser3-red2.dds", 1, true, false);
			LoadImage("Effect\\laser4.dds", 1, true, false);
			LoadImage("Effect\\sonar.dds", 1, true, false);
			LoadImage("Effect\\shield1.dds", 1, true, false);
			LoadImage("Effect\\OnShock.dds", 1, true, false);
			LoadImage("Effect\\OnRage.dds", 1, true, false);
			LoadImage("Effect\\shield2.dds", 1, true, false);
			LoadImage("Effect\\shield3.dds", 1, true, false);
			LoadImage("Effect\\shell.dds", 1, true, false);
			LoadImage("Effect\\bullet.dds", 1, true, false);
			LoadImage("Effect\\rad_add.dds", 1, true, false);
			LoadImage("Effect\\heatTrail.dds", 1, true, false);
			LoadImage("Effect\\streak.dds", 1, true, false);
			LoadImage("Effect\\proton1.dds", 1, true, false);
			LoadImage("Effect\\flash1.dds", 1, true, false);
			LoadImage("Effect\\flash2.dds", 1, true, false);
			LoadImage("Effect\\blaster.dds", 1, true, false);
			LoadImage("Effect\\blasterUpg.dds", 1, true, false);
			LoadImage("Effect\\dust_smoke_06.dds", 1, true, false);
			LoadImage("Effect\\ExplosionRay.dds", 1, true, false);
			LoadImage("Effect\\ExplosionRing.dds", 1, true, false);
			LoadImage("Effect\\ShockRing.dds", 1, true, false);
			LoadImage("Effect\\lens1.dds", 1, true, false);
			LoadImage("Effect\\streak1.dds", 1, true, false);
			LoadImage("Effect\\blink.dds", 1, true, false);
			LoadImage("Effect\\lightning1.dds", 1, true, false);
			LoadImage("Effect\\flare3.dds", 1, true, false);
			LoadImage("Effect\\trail1.dds", 1, true, false);
			LoadImage("Effect\\j_swell.dds", cGenMipLevel, true, false);
			LoadImage("Effect\\frost.dds", cGenMipLevel, true, false);
			LoadImage("Effect\\frostLine.dds", 1, true, false);
			LoadImage("Effect\\frostRay.dds", 1, true, false);
			LoadImage("Effect\\frostSmoke.dds", 1, true, false);
			LoadImage("Effect\\firePatron.dds", 1, true, false);
			LoadImage("Effect\\fireTrail.dds", 1, true, false);
			LoadImage("Effect\\blaster2.dds", 1, true, false);
			LoadImage("Effect\\ring2.dds", 1, true, false);
			LoadImage("Effect\\flare4.dds", 1, true, false);
			LoadImage("Effect\\protonRay.dds", 1, true, false);
			LoadImage("Effect\\protonRing.dds", 1, true, false);
			LoadImage("Effect\\crater.dds", 1, true, false);
			LoadImage("Effect\\phaseRing.dds", 1, true, false);
			LoadImage("Effect\\thunder1.dds", 1, true, false);
			LoadImage("Effect\\blueboom1_add.dds", 1, true, false);
			LoadImage("Effect\\fireblast09anim2.dds", 1, true, false);
			LoadImage("Effect\\szikra_group_6.dds", 1, true, false);
			LoadImage("Effect\\szikra_group_7.dds", 1, true, false);
			LoadImage("Effect\\lazerbeam1_blue1_blend7b.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_White.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Blue.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Sea.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Gay.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Guy.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Turquoise.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_EasyViolet.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_BrightBrown.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Orange.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Peach.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_BrightYellow.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Khaki.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Kargo.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Gray.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Yellow.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Acid.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Salad.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Birch.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_BrightGreen.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Green.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Neo.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Brown.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Cherry.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Red.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Pink.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Barbie.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Violet.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_DarkGray.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Boss.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Rip.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Shred.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Kristy.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Bot1.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Bot2.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Bot3.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Bot4.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Bot5.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Bot6.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Bot7.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Bot8.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Bot9.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Bot10.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Bot11.dds", 1, true, false);
			LoadImage("Effect\\LaserUp_Bot12.dds", 1, true, false);
			LoadImage("Effect\\flare1_tc.dds", 1, true, false);
			LoadImage("Effect\\flare2b.dds", 1, true, false);
			LoadImage("Effect\\flare2a.dds", 1, true, false);


			LoadImage2dLibMat("Effect\\wheel", "Effect\\wheel.dds");
			LoadImage2dLibMat("Effect\\truba", "Effect\\truba.dds");
			LoadImage2dLibMat("CarDestr\\destrCar", "CarDestr\\destrCar.dds");
			LoadImage2dLibMat("Effect\\pieces", "Effect\\pieces.dds");
			LoadImage2dLibMat("Effect\\j_swell", "Effect\\j_swell.dds", true);
			LoadImage2dLibMat("Effect\\frost", "Effect\\frost.dds", false, graph::Material::bmTransparency,
			                  FloatRange(0.5f, 0.0f));

			LoadImage2dLibMat("Effect\\smoke1", "Effect\\smoke1.dds", true, graph::Material::bmTransparency,
			                  FloatRange(0.5f, 0.0f), clrWhite * 0.25f);
			LoadImage2dLibMat("Effect\\smoke2", "Effect\\smoke2.dds", true, graph::Material::bmTransparency,
			                  FloatRange(0.8f, 0.0f),
			                  ColorRange(D3DXCOLOR(0.5f, 0.32f, 0.25f, 1.0f), D3DXCOLOR(0.25f, 0.25f, 0.25f, 1.0f)));
			LoadImage2dLibMat("Effect\\smoke3", "Effect\\smoke3.dds", true, graph::Material::bmTransparency,
			                  FloatRange(1.0f, 0.0f), clrWhite);
			LoadImage2dLibMat("Effect\\smoke7", "Effect\\smoke7.dds", true, graph::Material::bmTransparency,
			                  FloatRange(1.0f, 0.0f), clrWhite);
			LoadImage2dLibMat("Effect\\asphaltMarks", "Effect\\asphaltMarks.dds", true, graph::Material::bmTransparency,
			                  FloatRange(1.0f, 0.0f), clrWhite).SetAnisoFlt();
			LoadImage2dLibMat("Effect\\eagle", "Effect\\eagle.dds", true, graph::Material::bmTransparency, 0.8f,
			                  clrWhite);
			LoadImage2dLibMat("Effect\\drop", "Effect\\drop.dds", true, graph::Material::bmTransparency, 0.8f,
			                  clrWhite);
			LoadImage2dLibMat("Effect\\snowball", "Effect\\snowball.dds", true, graph::Material::bmTransparency, 0.8f,
			                  clrWhite);
			LoadImage2dLibMat("Effect\\sandy", "Effect\\sandy.dds", true, graph::Material::bmTransparency, 0.8f,
			                  clrWhite);
			LoadImage2dLibMat("Effect\\sandyIso", "Effect\\sandyIso.dds", true, graph::Material::bmTransparency, 0.8f,
			                  clrWhite);
			LoadImage2dLibMat("Effect\\snowballIso", "Effect\\snowballIso.dds", true, graph::Material::bmTransparency,
			                  0.8f, clrWhite);
			LoadAnimImage2dLibMat("Effect\\frostRay", "Effect\\frostRay.dds", Vec3Range(NullVector, -XVector * 1.0f),
			                      IdentityVector, NullQuaternion, true, graph::Material::bmTransparency,
			                      FloatRange(0.0f, 1.0f));
			LoadImage2dLibMat("Effect\\frostSmoke", "Effect\\frostSmoke.dds", true, graph::Material::bmTransparency,
			                  FloatRange(0.0f, 0.5f));
			LoadImage2dLibMat("Effect\\frostHit", "Effect\\frostSmoke.dds", true, graph::Material::bmTransparency,
			                  FloatRange(1.0f, 0.0f));
			LoadImage2dLibMat("Effect\\crater", "Effect\\crater.dds", true, graph::Material::bmTransparency,
			                  FloatRange(1.0f, 0.0f));

			LoadImage2dLibMat("Effect\\flare1", "Effect\\flare1.dds", true, graph::Material::bmAdditive, 0.5f,
			                  D3DXCOLOR(1, 0.58f, 0.36f, 1.0f));
			LoadImage2dLibMat("Effect\\flare2", "Effect\\flare1.dds", true, graph::Material::bmAdditive,
			                  FloatRange(0.0f, 1.0f), clrRed);
			LoadImage2dLibMat("Effect\\flare3", "Effect\\flare1.dds", true, graph::Material::bmAdditive,
			                  FloatRange(1.0f, 0.0f), D3DXCOLOR(1, 0.58f, 0.36f, 1.0f));
			LoadImage2dLibMat("Effect\\flare4", "Effect\\flare2.dds", true, graph::Material::bmAdditive);
			LoadImage2dLibMat("Effect\\flare5", "Effect\\flare3.dds", true, graph::Material::bmAdditive);
			LoadImage2dLibMat("Effect\\flare6", "Effect\\flare4.dds", true, graph::Material::bmAdditive);
			LoadImage2dLibMat("Effect\\flare7Red", "Effect\\flare5.dds", true, graph::Material::bmAdditive, 1.0f,
			                  clrRed);
			LoadImage2dLibMat("Effect\\flare7White", "Effect\\flare5.dds", true, graph::Material::bmAdditive);
			LoadImage2dLibMat("Effect\\blaster", "Effect\\blaster.dds", true, graph::Material::bmAdditive,
			                  FloatRange(1.0f, 0.0f), clrWhite);
			LoadImage2dLibMat("Effect\\blasterUpg", "Effect\\blasterUpg.dds", true, graph::Material::bmAdditive,
			                  FloatRange(1.0f, 0.0f), clrWhite);
			LoadImage2dLibMat("Effect\\blaster2", "Effect\\blaster2.dds", true, graph::Material::bmAdditive);
			LoadImage2dLibMat("Effect\\heatTrail", "Effect\\heatTrail.dds", true, graph::Material::bmAdditive,
			                  FloatRange(0.8f, 0.0f), clrWhite);
			LoadImage2dLibMat("Effect\\bullet", "Effect\\bullet.dds", true, graph::Material::bmAdditive, 1.0f,
			                  clrWhite);
			LoadImage2dLibMat("Effect\\rad_add", "Effect\\rad_add.dds", true, graph::Material::bmAdditive, 1.0f,
			                  clrWhite);
			LoadImage2dLibMat("Effect\\laser3-red2", "Effect\\laser3-red2.dds", true, graph::Material::bmAdditive,
			                  FloatRange(0.0f, 1.0f), clrWhite);
			LoadImage2dLibMat("Effect\\flash1", "Effect\\flash1.dds", true, graph::Material::bmAdditive, 1.0f,
			                  clrWhite);
			LoadImage2dLibMat("Effect\\flash2", "Effect\\flash2.dds", true, graph::Material::bmAdditive, 1.0f,
			                  clrWhite);
			LoadImage2dLibMat("Effect\\dust_smoke_06", "Effect\\flare1.dds", true, graph::Material::bmAdditive,
			                  FloatRange(1.0f, 0.0f), D3DXCOLOR(0.2f, 0.2f, 1.0f, 1.0f));
			LoadImage2dLibMat("Effect\\sonar", "Effect\\sonar.dds", true, graph::Material::bmAdditive,
			                  FloatRange(1.0f, 0.0f));
			LoadImage2dLibMat("Effect\\ExplosionRay", "Effect\\ExplosionRay.dds", true, graph::Material::bmAdditive,
			                  FloatRange(1.0f, 0.0f), clrBlue);
			LoadImage2dLibMat("Effect\\ExplosionRing", "Effect\\ExplosionRing.dds", true, graph::Material::bmAdditive,
			                  FloatRange(1.0f, 0.0f), clrYellow);
			LoadImage2dLibMat("Effect\\ShockRing", "Effect\\ShockRing.dds", true, graph::Material::bmAdditive,
			                  FloatRange(1.0f, 0.0f), clrBlue);
			LoadImage2dLibMat("Effect\\laser3-blue", "Effect\\laser3-blue.dds", true, graph::Material::bmAdditive);
			LoadImage2dLibMat("Effect\\laser3-green", "Effect\\laser3-green.dds", true, graph::Material::bmAdditive);
			LoadImage2dLibMat("Effect\\lens1", "Effect\\lens1.dds", true, graph::Material::bmAdditive, 1.0f, clrBlue);
			LoadImage2dLibMat("Effect\\streak1", "Effect\\streak1.dds", true, graph::Material::bmAdditive,
			                  FloatRange(1.0f, 0.0f));
			LoadImage2dLibMat("Effect\\blink", "Effect\\blink.dds", true, graph::Material::bmAdditive,
			                  FloatRange(1.0f, 0.0f));
			LoadImage2dLibMat("Effect\\lightning1", "Effect\\lightning1.dds", true, graph::Material::bmAdditive,
			                  FloatRange(1.0f, 0.0f));
			LoadImage2dLibMat("Effect\\trail1", "Effect\\trail1.dds", true, graph::Material::bmAdditive,
			                  FloatRange(1.0f, 0.0f));
			LoadImage2dLibMat("Effect\\ring1", "Effect\\ring1.dds", true, graph::Material::bmAdditive,
			                  FloatRange(1.0f, 0.0f));
			LoadImage2dLibMat("Effect\\ring2", "Effect\\ring2.dds", true, graph::Material::bmAdditive,
			                  FloatRange(0.0f, 1.0f));
			LoadImage2dLibMat("Effect\\frostLine", "Effect\\frostLine.dds", true, graph::Material::bmAdditive,
			                  FloatRange(0.0f, 1.0f));
			LoadImage2dLibMat("Effect\\firePatron", "Effect\\firePatron.dds", true, graph::Material::bmAdditive,
			                  FloatRange(1.0f, 0.0f));
			LoadImage2dLibMat("Effect\\fireTrail", "Effect\\fireTrail.dds", true, graph::Material::bmAdditive,
			                  FloatRange(1.0f, 0.0f), ColorRange(clrWhite, clrRed));
			LoadImage2dLibMat("Effect\\protonRay", "Effect\\protonRay.dds", true, graph::Material::bmAdditive,
			                  FloatRange(1.0f, 0.0f));
			LoadImage2dLibMat("Effect\\protonRing", "Effect\\protonRing.dds", true, graph::Material::bmAdditive,
			                  FloatRange(1.0f, 0.0f));
			LoadImage2dLibMat("Effect\\phaseRing", "Effect\\phaseRing.dds", true, graph::Material::bmAdditive);
			LoadImage2dLibMat("Effect\\thunder1", "Effect\\thunder1.dds", true, graph::Material::bmAdditive,
			                  FloatRange(1.0f, 0.0f), ColorRange(D3DXCOLOR(0xFFEC008C), D3DXCOLOR(0xFF0000FF)));
			LoadImage2dLibMat("Effect\\flareLaser1", "Effect\\flare2b.dds", true, graph::Material::bmAdditive,
			                  FloatRange(0.7f, 0.0f));
			LoadImage2dLibMat("Effect\\flareLaser2", "Effect\\flare1_tc.dds", true, graph::Material::bmAdditive,
			                  FloatRange(1.0f, 0.0f));
			LoadImage2dLibMat("Effect\\flareLaser3", "Effect\\flare2a.dds", true, graph::Material::bmAdditive,
			                  FloatRange(1.0f, 0.0f));

			LoadImage2dLibMat("Effect\\smoke6", "Effect\\smoke6.dds", true, graph::Material::bmTransparency,
			                  FloatRange(1.0f, 0.1f), ColorRange(clrWhite, clrBlack), Vec2Range(NullVec2, IdentityVec2),
			                  Point2U(4, 1));
			LoadImage2dLibMat("Effect\\explosion2", "Effect\\explosion2.dds", true, graph::Material::bmAdditive, 1.0f,
			                  clrWhite, Vec2Range(NullVec2, IdentityVec2), Point2U(4, 4));
			//.material.SetOption(graph::Material::moZTest, false);
			LoadImage2dLibMat("Effect\\explosion3", "Effect\\explosion3.dds", true, graph::Material::bmAdditive, 1.0f,
			                  clrWhite, Vec2Range(NullVec2, IdentityVec2), Point2U(6, 6));
			LoadImage2dLibMat("Effect\\explosion4", "Effect\\explosion4.dds", true, graph::Material::bmAdditive, 1.0f,
			                  clrWhite, Vec2Range(NullVec2, IdentityVec2), Point2U(7, 7));
			LoadImage2dLibMat("Effect\\boom1", "Effect\\fireblast09anim2.dds", true, graph::Material::bmTransparency,
			                  1.0f, clrWhite, Vec2Range(NullVec2, IdentityVec2), Point2U(8, 4));
			LoadImage2dLibMat("Effect\\boom2", "Effect\\blueboom1_add.dds", true, graph::Material::bmTransparency, 1.0f,
			                  clrWhite, Vec2Range(NullVec2, IdentityVec2), Point2U(8, 8));
			LoadImage2dLibMat("Effect\\spark1", "Effect\\spark1.dds", true, graph::Material::bmAdditive,
			                  FloatRange(1.0f, 0.0f));
			LoadImage2dLibMat("Effect\\boomSpark1", "Effect\\szikra_group_6.dds", true, graph::Material::bmAdditive,
			                  FloatRange(1.0f, 0.0f));
			LoadImage2dLibMat("Effect\\boomSpark2", "Effect\\szikra_group_7.dds", true, graph::Material::bmAdditive,
			                  FloatRange(1.0f, 0.0f));
			LoadImage2dLibMat("Effect\\fire1", "Effect\\fire1.dds", true, graph::Material::bmAdditive,
			                  FloatRange(1.0f, 0.0f), ColorRange(clrWhite, clrRed), Vec2Range(NullVec2, IdentityVec2),
			                  Point2U(4, 3));
			LoadImage2dLibMat("Effect\\fire2", "Effect\\fire2.dds", true, graph::Material::bmAdditive, 1.0f,
			                  ColorRange(clrWhite, clrRed), Vec2Range(NullVec2, IdentityVec2), Point2U(5, 5));
			LoadImage2dLibMat("Effect\\gunEff2", "Effect\\gunEff2.dds", true, graph::Material::bmAdditive,
			                  FloatRange(1.0f, 0.0f), clrWhite, Vec2Range(NullVec2, D3DXVECTOR2(1.0f, 0.25f)),
			                  Point2U(4, 1));
			LoadImage2dLibMat("Effect\\engine1", "Effect\\engine1.dds", true, graph::Material::bmAdditive, 1.0f,
			                  clrWhite, Vec2Range(NullVec2, IdentityVec2), Point2U(5, 2));
			LoadImage2dLibMat("Effect\\shield1", "Effect\\shield1.dds", true, graph::Material::bmAdditive,
			                  FloatRange(0.0f, 1.0f), clrWhite, Vec2Range(NullVec2, IdentityVec2), Point2U(5, 2));
			LoadImage2dLibMat("Effect\\OnShock", "Effect\\OnShock.dds", true, graph::Material::bmAdditive,
			                  FloatRange(0.0f, 1.0f), clrWhite, Vec2Range(NullVec2, IdentityVec2), Point2U(5, 2));
			LoadImage2dLibMat("Effect\\OnRage", "Effect\\OnRage.dds", true, graph::Material::bmAdditive,
			                  FloatRange(0.0f, 1.0f), clrWhite, Vec2Range(NullVec2, IdentityVec2), Point2U(5, 2));
			LoadImage2dLibMat("Effect\\shield3", "Effect\\shield3.dds", true, graph::Material::bmAdditive,
			                  FloatRange(0.0f, 1.0f), clrWhite, Vec2Range(NullVec2, IdentityVec2), Point2U(5, 2));
			LoadImage2dLibMatAnim("Effect\\shield2", "Effect\\shield2.dds", true, graph::Material::bmAdditive, 0.4f,
			                      clrWhite, Vec3Range(NullVector, IdentityVector)).SetAnisoFlt();
			LoadImage2dLibMatAnim("Effect\\shield2Hor", "Effect\\shield2.dds", true, graph::Material::bmAdditive, 0.4f,
			                      clrWhite, Vec3Range(NullVector, D3DXVECTOR3(1, 0, 0))).SetAnisoFlt();
			LoadImage2dLibMatAnim("Effect\\shield2Vert", "Effect\\shield2.dds", true, graph::Material::bmAdditive, 0.4f,
			                      clrWhite, Vec3Range(NullVector, D3DXVECTOR3(0, 1, 0))).SetAnisoFlt();
			LoadImage2dLibMatAnim("Effect\\shell", "Effect\\shell.dds", true, graph::Material::bmAdditive, 0.4f,
			                      clrWhite, Vec3Range(NullVector, IdentityVector)).SetAnisoFlt();
			LoadImage2dLibMatAnim("Effect\\shellHor", "Effect\\shell.dds", true, graph::Material::bmAdditive, 0.4f,
			                      clrWhite, Vec3Range(NullVector, D3DXVECTOR3(1, 0, 0))).SetAnisoFlt();
			LoadImage2dLibMatAnim("Effect\\shellVert", "Effect\\shell.dds", true, graph::Material::bmAdditive, 0.4f,
			                      clrWhite, Vec3Range(NullVector, D3DXVECTOR3(0, 1, 0))).SetAnisoFlt();
			LoadImage2dLibMatAnim("Effect\\phaserBolt", "Effect\\shield2.dds", true, graph::Material::bmAdditive, 1.0f,
			                      clrWhite, Vec3Range(NullVector, IdentityVector)).SetAnisoFlt();

			LoadLibMat("Effect\\gravBall", true, graph::Material::bmOpaque, 1.0f, clrRed);
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay",
				                                               "Effect\\lazerbeam1_blue1_blend7b.dds", true,
				                                               graph::Material::bmAdditive, FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//white
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_White", "Effect\\LaserUp_White.dds",
				                                               true, graph::Material::bmAdditive,
				                                               FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Blue
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Blue", "Effect\\LaserUp_Blue.dds",
				                                               true, graph::Material::bmAdditive,
				                                               FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Sea
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Sea", "Effect\\LaserUp_Sea.dds", true,
				                                               graph::Material::bmAdditive, FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Gay
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Gay", "Effect\\LaserUp_Gay.dds", true,
				                                               graph::Material::bmAdditive, FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Guy
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Guy", "Effect\\LaserUp_Guy.dds", true,
				                                               graph::Material::bmAdditive, FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Turquoise
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Turquoise",
				                                               "Effect\\LaserUp_Turquoise.dds", true,
				                                               graph::Material::bmAdditive, FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//EasyViolet
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_EasyViolet",
				                                               "Effect\\LaserUp_EasyViolet.dds", true,
				                                               graph::Material::bmAdditive, FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//BrightBrown
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_BrightBrown",
				                                               "Effect\\LaserUp_BrightBrown.dds", true,
				                                               graph::Material::bmAdditive, FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Orange
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Orange", "Effect\\LaserUp_Orange.dds",
				                                               true, graph::Material::bmAdditive,
				                                               FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Peach
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Peach", "Effect\\LaserUp_Peach.dds",
				                                               true, graph::Material::bmAdditive,
				                                               FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//BrightYellow
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_BrightYellow",
				                                               "Effect\\LaserUp_BrightYellow.dds", true,
				                                               graph::Material::bmAdditive, FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Khaki
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Khaki", "Effect\\LaserUp_Khaki.dds",
				                                               true, graph::Material::bmAdditive,
				                                               FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Kargo
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Kargo", "Effect\\LaserUp_Kargo.dds",
				                                               true, graph::Material::bmAdditive,
				                                               FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Gray
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Gray", "Effect\\LaserUp_Gray.dds",
				                                               true, graph::Material::bmAdditive,
				                                               FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Yellow
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Yellow", "Effect\\LaserUp_Yellow.dds",
				                                               true, graph::Material::bmAdditive,
				                                               FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Acid
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Acid", "Effect\\LaserUp_Acid.dds",
				                                               true, graph::Material::bmAdditive,
				                                               FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Salad
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Salad", "Effect\\LaserUp_Salad.dds",
				                                               true, graph::Material::bmAdditive,
				                                               FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Birch
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Birch", "Effect\\LaserUp_Birch.dds",
				                                               true, graph::Material::bmAdditive,
				                                               FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//BrightGreen
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_BrightGreen",
				                                               "Effect\\LaserUp_BrightGreen.dds", true,
				                                               graph::Material::bmAdditive, FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Green
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Green", "Effect\\LaserUp_Green.dds",
				                                               true, graph::Material::bmAdditive,
				                                               FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Neo
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Neo", "Effect\\LaserUp_Neo.dds", true,
				                                               graph::Material::bmAdditive, FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Brown
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Brown", "Effect\\LaserUp_Brown.dds",
				                                               true, graph::Material::bmAdditive,
				                                               FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Cherry
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Cherry", "Effect\\LaserUp_Cherry.dds",
				                                               true, graph::Material::bmAdditive,
				                                               FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Red
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Red", "Effect\\LaserUp_Red.dds", true,
				                                               graph::Material::bmAdditive, FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Pink
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Pink", "Effect\\LaserUp_Pink.dds",
				                                               true, graph::Material::bmAdditive,
				                                               FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Barbie
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Barbie", "Effect\\LaserUp_Barbie.dds",
				                                               true, graph::Material::bmAdditive,
				                                               FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Violet
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Violet", "Effect\\LaserUp_Violet.dds",
				                                               true, graph::Material::bmAdditive,
				                                               FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//DarkGray
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_DarkGray",
				                                               "Effect\\LaserUp_DarkGray.dds", true,
				                                               graph::Material::bmAdditive, FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Boss
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Boss", "Effect\\LaserUp_Boss.dds",
				                                               true, graph::Material::bmAdditive,
				                                               FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Rip
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Rip", "Effect\\LaserUp_Rip.dds", true,
				                                               graph::Material::bmAdditive, FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Shred
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Shred", "Effect\\LaserUp_Shred.dds",
				                                               true, graph::Material::bmAdditive,
				                                               FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Kristy
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Kristy", "Effect\\LaserUp_Kristy.dds",
				                                               true, graph::Material::bmAdditive,
				                                               FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Bot1
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Bot1", "Effect\\LaserUp_Bot1.dds",
				                                               true, graph::Material::bmAdditive,
				                                               FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Bot2
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Bot2", "Effect\\LaserUp_Bot2.dds",
				                                               true, graph::Material::bmAdditive,
				                                               FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Bot3
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Bot3", "Effect\\LaserUp_Bot3.dds",
				                                               true, graph::Material::bmAdditive,
				                                               FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Bot4
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Bot4", "Effect\\LaserUp_Bot4.dds",
				                                               true, graph::Material::bmAdditive,
				                                               FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Bot5
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Bot5", "Effect\\LaserUp_Bot5.dds",
				                                               true, graph::Material::bmAdditive,
				                                               FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Bot6
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Bot6", "Effect\\LaserUp_Bot6.dds",
				                                               true, graph::Material::bmAdditive,
				                                               FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Bot7
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Bot7", "Effect\\LaserUp_Bot7.dds",
				                                               true, graph::Material::bmAdditive,
				                                               FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Bot8
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Bot8", "Effect\\LaserUp_Bot8.dds",
				                                               true, graph::Material::bmAdditive,
				                                               FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Bot9
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Bot9", "Effect\\LaserUp_Bot9.dds",
				                                               true, graph::Material::bmAdditive,
				                                               FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Bot10
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Bot10", "Effect\\LaserUp_Bot10.dds",
				                                               true, graph::Material::bmAdditive,
				                                               FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Bot11
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Bot11", "Effect\\LaserUp_Bot11.dds",
				                                               true, graph::Material::bmAdditive,
				                                               FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}

			//Bot12
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("Effect\\laserRay_Bot12", "Effect\\LaserUp_Bot12.dds",
				                                               true, graph::Material::bmAdditive,
				                                               FloatRange(1.0f, 1.0f));
				libMat.samplers[0].SetOffset(Vec3Range(NullVector, NullVector));
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
			}
		}

		void ResourceManager::LoadWorld1()
		{
			LSL_LOG("resourceManager load world1");

			LoadMesh("World1\\berge.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\osnova.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\columns.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\eldertree2.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\eldertree.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\lowpalma1.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\lowpalma2.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\lowpalma3.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\palma2.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\palma3.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\palma4.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\palma.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\paporotnik.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\stone1.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\stone2.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\stone3.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\wand.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\naves.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\zdaine1.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\zdaine2.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);

			LoadMesh("World1\\Track\\most.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\Track\\track1.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\Track\\track2.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\Track\\track3.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\Track\\track4.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\Track\\track5.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\Track\\tramp1.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\Track\\tramp2.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\Track\\jumper1.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\Track\\jumper2.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\Track\\cut1.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\Track\\cut2.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\Track\\cut3.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\Track\\long1.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\Track\\long2.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\Track\\long3.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\Track\\long4.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\Track\\long5.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\Track\\quad1.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\Track\\quad2.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\Track\\quad3.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\Track\\quad4.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\Track\\quad5.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\Track\\quad6.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\Track\\quad7.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\Track\\quad8.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\Track\\quad9.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\Track\\quad10.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\Track\\chunk1.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\Track\\chunk2.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\Track\\chunk3.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\Track\\piece1.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\Track\\piece2.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\Track\\piece3.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\Track\\wave1.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\Track\\wave2.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);
			LoadMesh("World1\\Track\\wave3.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagInitIVB);

			LoadMesh("World1\\Track\\pxTrack1.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagLoadData);
			LoadMesh("World1\\Track\\pxTrack2.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagLoadData);
			LoadMesh("World1\\Track\\pxTrack3.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagLoadData);
			LoadMesh("World1\\Track\\pxTrack4.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagLoadData);
			LoadMesh("World1\\Track\\pxTrack5.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagLoadData);
			LoadMesh("World1\\Track\\pxTramp1.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagLoadData);
			LoadMesh("World1\\Track\\pxTramp2.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagLoadData);
			LoadMesh("World1\\Track\\pxMost.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagLoadData);
			LoadMesh("World1\\Track\\pxCut1.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagLoadData);
			LoadMesh("World1\\Track\\pxCut2.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagLoadData);
			LoadMesh("World1\\Track\\pxCut3.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagLoadData);
			LoadMesh("World1\\Track\\pxLong1.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagLoadData);
			LoadMesh("World1\\Track\\pxLong2.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagLoadData);
			LoadMesh("World1\\Track\\pxLong3.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagLoadData);
			LoadMesh("World1\\Track\\pxLong4.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagLoadData);
			LoadMesh("World1\\Track\\pxLong5.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagLoadData);
			LoadMesh("World1\\Track\\pxQuad1.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagLoadData);
			LoadMesh("World1\\Track\\pxQuad2.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagLoadData);
			LoadMesh("World1\\Track\\pxQuad3.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagLoadData);
			LoadMesh("World1\\Track\\pxQuad4.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagLoadData);
			LoadMesh("World1\\Track\\pxQuad5.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagLoadData);
			LoadMesh("World1\\Track\\pxQuad6.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagLoadData);
			LoadMesh("World1\\Track\\pxQuad7.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagLoadData);
			LoadMesh("World1\\Track\\pxQuad8.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagLoadData);
			LoadMesh("World1\\Track\\pxQuad9.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagLoadData);
			LoadMesh("World1\\Track\\pxQuad10.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagLoadData);
			LoadMesh("World1\\Track\\pxChunk1.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagLoadData);
			LoadMesh("World1\\Track\\pxChunk2.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagLoadData);
			LoadMesh("World1\\Track\\pxChunk3.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagLoadData);
			LoadMesh("World1\\Track\\pxPiece1.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagLoadData);
			LoadMesh("World1\\Track\\pxPiece2.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagLoadData);
			LoadMesh("World1\\Track\\pxPiece3.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagLoadData);
			LoadMesh("World1\\Track\\pxWave1.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagLoadData);
			LoadMesh("World1\\Track\\pxWave2.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagLoadData);
			LoadMesh("World1\\Track\\pxWave3.r3d", false, false, false, Planet::wtWorld1 | MeshLib::cTagLoadData);


			LoadImage("World1\\Texture\\berge.dds", cGenMipLevel, false, false,
			          Planet::wtWorld1 | ImageLib::cTagInitTex2d);
			LoadImage("World1\\Texture\\columns.dds", cGenMipLevel, false, false,
			          Planet::wtWorld1 | ImageLib::cTagInitTex2d);
			LoadImage("World1\\Texture\\eldertree.dds", cGenMipLevel, false, false,
			          Planet::wtWorld1 | ImageLib::cTagInitTex2d);
			LoadImage("World1\\Texture\\lowpalma.dds", cGenMipLevel, false, false,
			          Planet::wtWorld1 | ImageLib::cTagInitTex2d);
			LoadImage("World1\\Texture\\naves.dds", cGenMipLevel, false, false,
			          Planet::wtWorld1 | ImageLib::cTagInitTex2d);
			LoadImage("World1\\Texture\\palma.dds", cGenMipLevel, false, false,
			          Planet::wtWorld1 | ImageLib::cTagInitTex2d);
			LoadImage("World1\\Texture\\paporotnik.dds", cGenMipLevel, false, false,
			          Planet::wtWorld1 | ImageLib::cTagInitTex2d);
			LoadImage("World1\\Texture\\stone.dds", cGenMipLevel, false, false,
			          Planet::wtWorld1 | ImageLib::cTagInitTex2d);
			LoadImage("World1\\Texture\\wand.dds", cGenMipLevel, false, false,
			          Planet::wtWorld1 | ImageLib::cTagInitTex2d);
			LoadImage("World1\\Texture\\zdanie1.dds", cGenMipLevel, false, false,
			          Planet::wtWorld1 | ImageLib::cTagInitTex2d);
			//LoadImage("World1\\Texture\\zdanie1Normals.dds", false, false, Planet::wtWorld1 | ImageLib::cTagInitTex2d);
			LoadImage("World1\\Texture\\zdanie2.dds", cGenMipLevel, false, false,
			          Planet::wtWorld1 | ImageLib::cTagInitTex2d);
			//LoadImage("World1\\Texture\\zdanie2Normals.dds", false, false, Planet::wtWorld1 | ImageLib::cTagInitTex2d);

			LoadImage("World1\\Track\\Texture\\track1.dds", cGenMipLevel, false, false,
			          Planet::wtWorld1 | ImageLib::cTagInitTex2d);
			LoadImage("World1\\Track\\Texture\\track4.dds", cGenMipLevel, false, false,
			          Planet::wtWorld1 | ImageLib::cTagInitTex2d);
			LoadImage("World1\\Track\\Texture\\track5.dds", cGenMipLevel, false, false,
			          Planet::wtWorld1 | ImageLib::cTagInitTex2d);
			LoadImage("World1\\Track\\Texture\\most.dds", cGenMipLevel, false, false,
			          Planet::wtWorld1 | ImageLib::cTagInitTex2d);


			LoadImage2dLibMat("World1\\stone", "World1\\Texture\\stone.dds");
			LoadImage2dLibMat("World1\\wand", "World1\\Texture\\wand.dds");
			LoadImage2dLibMat("World1\\columns", "World1\\Texture\\columns.dds");
			LoadImage2dLibMat("World1\\Track\\most", "World1\\Track\\Texture\\most.dds");
			LoadImage2dLibMat("World1\\Track\\track1", "World1\\Track\\Texture\\track1.dds");
			LoadImage2dLibMat("World1\\Track\\track4", "World1\\Track\\Texture\\track4.dds");
			LoadImage2dLibMat("World1\\Track\\track5", "World1\\Track\\Texture\\track5.dds");
			LoadAlphaTestLibMat("World1\\zdanie1", "World1\\Texture\\zdanie1.dds");
			LoadAlphaTestLibMat("World1\\zdanie2", "World1\\Texture\\zdanie2.dds");

			LoadAlphaTestLibMat("World1\\berge", "World1\\Texture\\berge.dds");
			LoadAlphaTestLibMat("World1\\eldertree", "World1\\Texture\\eldertree.dds");
			LoadAlphaTestLibMat("World1\\lowpalma", "World1\\Texture\\lowpalma.dds");
			LoadAlphaTestLibMat("World1\\naves", "World1\\Texture\\naves.dds");
			LoadAlphaTestLibMat("World1\\palma", "World1\\Texture\\palma.dds");
			LoadAlphaTestLibMat("World1\\paporotnik", "World1\\Texture\\paporotnik.dds");
		}

		void ResourceManager::LoadWorld2()
		{
			LSL_LOG("resourceManager load world2");

			LoadMesh("World2\\atom.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\bochki.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\deadtree1.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\deadtree2.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\deadtree3.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\deadtree3_b.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\elka.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\factory.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\haus1.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\haus1_2.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\haus3.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\machineFactory.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\metal1_1.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\metal1_2.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\naves1_1.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\naves1_2.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\poplar1_1.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\poplar1_2.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\pregrada.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\projector.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\pumpjack.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\semaphore.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\skelet1_1.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\skelet1_2.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\skelet1_3.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\strelka1_1.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\strelka1_2.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\tramplin1.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\truba1.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\truba2.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\truba3.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\truba4.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);

			LoadMesh("World2\\isle1.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\isle2.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\isle3.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\isle4.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);

			LoadMesh("World2\\Track\\jump.r3d", true, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\Track\\most.r3d", true, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\Track\\track1.r3d", true, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\Track\\track2.r3d", true, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\Track\\track3.r3d", true, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\Track\\track4.r3d", true, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\Track\\track6.r3d", true, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\Track\\tramp1.r3d", true, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\Track\\tramp2.r3d", true, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\Track\\jumper1.r3d", true, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);
			LoadMesh("World2\\Track\\jumper2.r3d", true, false, false, Planet::wtWorld2 | MeshLib::cTagInitIVB);

			LoadMesh("World2\\Track\\pxTrack1.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagLoadData);
			LoadMesh("World2\\Track\\pxTrack2.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagLoadData);
			LoadMesh("World2\\Track\\pxTrack3.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagLoadData);
			LoadMesh("World2\\Track\\pxTrack4.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagLoadData);
			LoadMesh("World2\\Track\\pxTrack6.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagLoadData);
			LoadMesh("World2\\Track\\pxTramp1.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagLoadData);
			LoadMesh("World2\\Track\\pxTramp2.r3d", false, false, false, Planet::wtWorld2 | MeshLib::cTagLoadData);


			LoadImage("World2\\Texture\\atom.dds", cGenMipLevel, false, false,
			          Planet::wtWorld2 | ImageLib::cTagInitTex2d);
			LoadImage("World2\\Texture\\bochki.dds", cGenMipLevel, false, false,
			          Planet::wtWorld2 | ImageLib::cTagInitTex2d);
			LoadImage("World2\\Texture\\deadtree1.dds", cGenMipLevel, false, false,
			          Planet::wtWorld2 | ImageLib::cTagInitTex2d);
			LoadImage("World2\\Texture\\deadtree2.dds", cGenMipLevel, false, false,
			          Planet::wtWorld2 | ImageLib::cTagInitTex2d);
			LoadImage("World2\\Texture\\deadtree3.dds", cGenMipLevel, false, false,
			          Planet::wtWorld2 | ImageLib::cTagInitTex2d);
			LoadImage("World2\\Texture\\elka.dds", cGenMipLevel, false, false,
			          Planet::wtWorld2 | ImageLib::cTagInitTex2d);
			LoadImage("World2\\Texture\\factory.dds", cGenMipLevel, false, false,
			          Planet::wtWorld2 | ImageLib::cTagInitTex2d);
			LoadImage("World2\\Texture\\haus1.dds", cGenMipLevel, false, false,
			          Planet::wtWorld2 | ImageLib::cTagInitTex2d);
			LoadImage("World2\\Texture\\Haus3.dds", cGenMipLevel, false, false,
			          Planet::wtWorld2 | ImageLib::cTagInitTex2d);
			LoadImage("World2\\Texture\\machineFactory.dds", cGenMipLevel, false, false,
			          Planet::wtWorld2 | ImageLib::cTagInitTex2d);
			LoadImage("World2\\Texture\\metal1.dds", cGenMipLevel, false, false,
			          Planet::wtWorld2 | ImageLib::cTagInitTex2d);
			LoadImage("World2\\Texture\\naves1.dds", cGenMipLevel, false, false,
			          Planet::wtWorld2 | ImageLib::cTagInitTex2d);
			LoadImage("World2\\Texture\\poplar1.dds", cGenMipLevel, false, false,
			          Planet::wtWorld2 | ImageLib::cTagInitTex2d);
			LoadImage("World2\\Texture\\pregrada.dds", cGenMipLevel, false, false,
			          Planet::wtWorld2 | ImageLib::cTagInitTex2d);
			LoadImage("World2\\Texture\\projektor.dds", cGenMipLevel, false, false,
			          Planet::wtWorld2 | ImageLib::cTagInitTex2d);
			LoadImage("World2\\Texture\\pumpjack.dds", cGenMipLevel, false, false,
			          Planet::wtWorld2 | ImageLib::cTagInitTex2d);
			LoadImage("World2\\Texture\\semaphore.dds", cGenMipLevel, false, false,
			          Planet::wtWorld2 | ImageLib::cTagInitTex2d);
			LoadImage("World2\\Texture\\skelet1.dds", cGenMipLevel, false, false,
			          Planet::wtWorld2 | ImageLib::cTagInitTex2d);
			LoadImage("World2\\Texture\\strelka1.dds", cGenMipLevel, false, false,
			          Planet::wtWorld2 | ImageLib::cTagInitTex2d);
			LoadImage("World2\\Texture\\tramplin1.dds", cGenMipLevel, false, false,
			          Planet::wtWorld2 | ImageLib::cTagInitTex2d);
			LoadImage("World2\\Texture\\truba1.dds", cGenMipLevel, false, false,
			          Planet::wtWorld2 | ImageLib::cTagInitTex2d);
			LoadImage("World2\\Texture\\truba2.dds", cGenMipLevel, false, false,
			          Planet::wtWorld2 | ImageLib::cTagInitTex2d);
			LoadImage("World2\\Texture\\truba3.dds", cGenMipLevel, false, false,
			          Planet::wtWorld2 | ImageLib::cTagInitTex2d);
			LoadImage("World2\\Texture\\truba4.dds", cGenMipLevel, false, false,
			          Planet::wtWorld2 | ImageLib::cTagInitTex2d);
			LoadImage("World2\\Texture\\isle1.dds", cGenMipLevel, false, false,
			          Planet::wtWorld2 | ImageLib::cTagInitTex2d);

			LoadImage("World2\\Track\\Texture\\track1.dds", cGenMipLevel, false, false,
			          Planet::wtWorld2 | ImageLib::cTagInitTex2d);
			LoadImage("World2\\Track\\Texture\\track1_norm.dds", 1, false, false,
			          Planet::wtWorld2 | ImageLib::cTagInitTex2d);
			//LoadImage("World2\\Track\\Texture\\track2road.dds", cGenMipLevel, false, false, Planet::wtWorld2 | ImageLib::cTagInitTex2d);
			//LoadImage("World2\\Track\\Texture\\track2road_norm.dds", 1, false, false, Planet::wtWorld2 | ImageLib::cTagInitTex2d);
			LoadImage("World2\\Track\\Texture\\most.dds", cGenMipLevel, false, false,
			          Planet::wtWorld2 | ImageLib::cTagInitTex2d);
			LoadImage("World2\\Track\\Texture\\most_norm.dds", 1, false, false,
			          Planet::wtWorld2 | ImageLib::cTagInitTex2d);


			LoadImage2dLibMat("World2\\atom", "World2\\Texture\\atom.dds");
			LoadImage2dLibMat("World2\\bochki", "World2\\Texture\\bochki.dds");
			LoadImage2dLibMat("World2\\deadtree3", "World2\\Texture\\deadtree3.dds");
			LoadImage2dLibMat("World2\\factory", "World2\\Texture\\factory.dds");
			LoadImage2dLibMat("World2\\haus1", "World2\\Texture\\haus1.dds");
			LoadImage2dLibMat("World2\\Haus3", "World2\\Texture\\Haus3.dds");
			LoadImage2dLibMat("World2\\machineFactory", "World2\\Texture\\machineFactory.dds");
			LoadImage2dLibMat("World2\\metal1", "World2\\Texture\\metal1.dds");
			LoadImage2dLibMat("World2\\naves1", "World2\\Texture\\naves1.dds");
			LoadImage2dLibMat("World2\\pregrada", "World2\\Texture\\pregrada.dds");
			LoadImage2dLibMat("World2\\projektor", "World2\\Texture\\projektor.dds");
			LoadImage2dLibMat("World2\\pumpjack", "World2\\Texture\\pumpjack.dds");
			LoadImage2dLibMat("World2\\semaphore", "World2\\Texture\\semaphore.dds");
			LoadImage2dLibMat("World2\\skelet1", "World2\\Texture\\skelet1.dds");
			LoadImage2dLibMat("World2\\strelka1", "World2\\Texture\\strelka1.dds");
			LoadImage2dLibMat("World2\\tramplin1", "World2\\Texture\\tramplin1.dds");
			LoadImage2dLibMat("World2\\truba1", "World2\\Texture\\truba1.dds");
			LoadImage2dLibMat("World2\\truba2", "World2\\Texture\\truba2.dds");
			LoadImage2dLibMat("World2\\truba3", "World2\\Texture\\truba3.dds");
			LoadImage2dLibMat("World2\\truba4", "World2\\Texture\\truba4.dds");
			LoadImage2dLibMat("World2\\isle1", "World2\\Texture\\isle1.dds");

			LoadAlphaTestLibMat("World2\\elka", "World2\\Texture\\elka.dds");
			LoadAlphaTestLibMat("World2\\deadtree1", "World2\\Texture\\deadtree1.dds");
			LoadAlphaTestLibMat("World2\\deadtree2", "World2\\Texture\\deadtree2.dds");
			LoadAlphaTestLibMat("World2\\poplar1", "World2\\Texture\\poplar1.dds");

			LoadBumpLibMat("World2\\Track\\track1", "World2\\Track\\Texture\\track1.dds",
			               "World2\\Track\\Texture\\track1_norm.dds");
			//LoadBumpLibMat("World2\\Track\\track2", "World2\\Track\\Texture\\track1.dds", "World2\\Track\\Texture\\track1_norm.dds");
			//LoadBumpLibMat("World2\\Track\\track2road", "World2\\Track\\Texture\\track2road.dds", "World2\\Track\\Texture\\track2road_norm.dds");
			LoadBumpLibMat("World2\\Track\\most", "World2\\Track\\Texture\\most.dds",
			               "World2\\Track\\Texture\\most_norm.dds");
		}

		void ResourceManager::LoadWorld3()
		{
			LSL_LOG("resourceManager load world3");

			LoadMesh("World3\\fabrika.r3d", false, false, false, Planet::wtWorld3 | MeshLib::cTagInitIVB);
			LoadMesh("World3\\naves.r3d", false, false, false, Planet::wtWorld3 | MeshLib::cTagInitIVB);
			LoadMesh("World3\\tower.r3d", false, false, false, Planet::wtWorld3 | MeshLib::cTagInitIVB);
			LoadMesh("World3\\grass1.r3d", false, false, false, Planet::wtWorld3 | MeshLib::cTagInitIVB);
			LoadMesh("World3\\grass2.r3d", false, false, false, Planet::wtWorld3 | MeshLib::cTagInitIVB);
			LoadMesh("World3\\tower2.r3d", false, false, false, Planet::wtWorld3 | MeshLib::cTagInitIVB);
			LoadMesh("World3\\ventil1.r3d", false, false, false, Planet::wtWorld3 | MeshLib::cTagInitIVB);
			LoadMesh("World3\\ventil2.r3d", false, false, false, Planet::wtWorld3 | MeshLib::cTagInitIVB);
			LoadMesh("World3\\windmil.r3d", false, false, false, Planet::wtWorld3 | MeshLib::cTagInitIVB);
			LoadMesh("World3\\helpborder.r3d", false, false, false, Planet::wtWorld3 | MeshLib::cTagInitIVB);
			LoadMesh("World3\\stone1.r3d", false, false, false, Planet::wtWorld3 | MeshLib::cTagInitIVB);
			LoadMesh("World3\\stone2.r3d", false, false, false, Planet::wtWorld3 | MeshLib::cTagInitIVB);
			LoadMesh("World3\\stone3.r3d", false, false, false, Planet::wtWorld3 | MeshLib::cTagInitIVB);

			LoadMesh("World3\\Track\\most.r3d", false, false, false, Planet::wtWorld3 | MeshLib::cTagInitIVB);
			LoadMesh("World3\\Track\\track1.r3d", false, false, false, Planet::wtWorld3 | MeshLib::cTagInitIVB);
			LoadMesh("World3\\Track\\track2.r3d", false, false, false, Planet::wtWorld3 | MeshLib::cTagInitIVB);
			LoadMesh("World3\\Track\\track3.r3d", false, false, false, Planet::wtWorld3 | MeshLib::cTagInitIVB);
			LoadMesh("World3\\Track\\track4.r3d", false, false, false, Planet::wtWorld3 | MeshLib::cTagInitIVB);
			LoadMesh("World3\\Track\\podjem1.r3d", false, false, false, Planet::wtWorld3 | MeshLib::cTagInitIVB);
			LoadMesh("World3\\Track\\podjem2.r3d", false, false, false, Planet::wtWorld3 | MeshLib::cTagInitIVB);

			LoadMesh("World3\\Track\\pxTrack1.r3d", false, false, false, Planet::wtWorld3 | MeshLib::cTagLoadData);
			LoadMesh("World3\\Track\\pxTrack2.r3d", false, false, false, Planet::wtWorld3 | MeshLib::cTagLoadData);
			LoadMesh("World3\\Track\\pxTrack3.r3d", false, false, false, Planet::wtWorld3 | MeshLib::cTagLoadData);
			LoadMesh("World3\\Track\\pxTrack4.r3d", false, false, false, Planet::wtWorld3 | MeshLib::cTagLoadData);
			LoadMesh("World3\\Track\\pxPodjem1.r3d", false, false, false, Planet::wtWorld3 | MeshLib::cTagLoadData);
			LoadMesh("World3\\Track\\pxPodjem2.r3d", false, false, false, Planet::wtWorld3 | MeshLib::cTagLoadData);

			LoadImage("World3\\Texture\\fabrika.dds", cGenMipLevel, false, false,
			          Planet::wtWorld3 | ImageLib::cTagInitTex2d);
			LoadImage("World3\\Texture\\grass.dds", cGenMipLevel, false, false,
			          Planet::wtWorld3 | ImageLib::cTagInitTex2d);
			LoadImage("World3\\Texture\\tower.dds", cGenMipLevel, false, false,
			          Planet::wtWorld3 | ImageLib::cTagInitTex2d);
			LoadImage("World3\\Texture\\tower2.dds", cGenMipLevel, false, false,
			          Planet::wtWorld3 | ImageLib::cTagInitTex2d);
			LoadImage("World3\\Texture\\ventil1.dds", cGenMipLevel, false, false,
			          Planet::wtWorld3 | ImageLib::cTagInitTex2d);
			LoadImage("World3\\Texture\\ventil2.dds", cGenMipLevel, false, false,
			          Planet::wtWorld3 | ImageLib::cTagInitTex2d);
			LoadImage("World3\\Texture\\windmil.dds", cGenMipLevel, false, false,
			          Planet::wtWorld3 | ImageLib::cTagInitTex2d);
			LoadImage("World3\\Texture\\stone.dds", cGenMipLevel, false, false,
			          Planet::wtWorld3 | ImageLib::cTagInitTex2d);

			LoadImage("World3\\Track\\Texture\\most.dds", cGenMipLevel, false, false,
			          Planet::wtWorld3 | ImageLib::cTagInitTex2d);
			LoadImage("World3\\Track\\Texture\\track1.dds", cGenMipLevel, false, false,
			          Planet::wtWorld3 | ImageLib::cTagInitTex2d);

			LoadImage2dLibMat("World3\\fabrika", "World3\\Texture\\fabrika.dds");
			LoadImage2dLibMat("World3\\naves", "World3\\Track\\Texture\\track1.dds");
			LoadImage2dLibMat("World3\\tower", "World3\\Texture\\tower.dds");
			LoadImage2dLibMat("World3\\tower2", "World3\\Texture\\tower2.dds");
			LoadImage2dLibMat("World3\\ventil1", "World3\\Texture\\ventil1.dds");
			LoadImage2dLibMat("World3\\ventil2", "World3\\Texture\\ventil2.dds");
			LoadImage2dLibMat("World3\\windmil", "World3\\Texture\\windmil.dds");
			LoadImage2dLibMat("World3\\stone", "World3\\Texture\\stone.dds");
			LoadAlphaTestLibMat("World3\\grass", "World3\\Texture\\grass.dds");

			LoadImage2dLibMat("World3\\Track\\track", "World3\\Track\\Texture\\track1.dds");
			LoadImage2dLibMat("World3\\Track\\most", "World3\\Track\\Texture\\most.dds");
		}

		void ResourceManager::LoadWorld4()
		{
			LSL_LOG("resourceManager load world4");

			LoadMesh("World4\\architect1.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagInitIVB);
			LoadMesh("World4\\architect2.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagInitIVB);
			LoadMesh("World4\\architect3_1.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagInitIVB);
			LoadMesh("World4\\architect3_2.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagInitIVB);
			LoadMesh("World4\\architect4.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagInitIVB);
			LoadMesh("World4\\build.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagInitIVB);
			LoadMesh("World4\\gora1.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagInitIVB);
			LoadMesh("World4\\gora2.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagInitIVB);
			LoadMesh("World4\\kolba.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagInitIVB);
			LoadMesh("World4\\lavaplace1.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagInitIVB);
			LoadMesh("World4\\lavaplace2.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagInitIVB);
			LoadMesh("World4\\lavaplace3.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagInitIVB);
			LoadMesh("World4\\lavaplace4.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagInitIVB);
			LoadMesh("World4\\lavaplace5.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagInitIVB);
			LoadMesh("World4\\pushka.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagInitIVB);
			LoadMesh("World4\\naves.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagInitIVB);
			LoadMesh("World4\\volcano.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagInitIVB);
			LoadMesh("World4\\crystals.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagInitIVB);

			LoadMesh("World4\\Track\\most.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagInitIVB);
			LoadMesh("World4\\Track\\most2.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagInitIVB);
			LoadMesh("World4\\Track\\track1.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagInitIVB);
			LoadMesh("World4\\Track\\track2.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagInitIVB);
			LoadMesh("World4\\Track\\track3.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagInitIVB);
			LoadMesh("World4\\Track\\track4.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagInitIVB);
			LoadMesh("World4\\Track\\track5.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagInitIVB);
			LoadMesh("World4\\Track\\podjem1.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagInitIVB);
			LoadMesh("World4\\Track\\podjem2.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagInitIVB);
			LoadMesh("World4\\Track\\jumper1.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagInitIVB);
			LoadMesh("World4\\Track\\jumper2.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagInitIVB);

			LoadMesh("World4\\Track\\pxTrack1.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagLoadData);
			LoadMesh("World4\\Track\\pxTrack2.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagLoadData);
			LoadMesh("World4\\Track\\pxTrack3.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagLoadData);
			LoadMesh("World4\\Track\\pxTrack4.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagLoadData);
			LoadMesh("World4\\Track\\pxTrack5.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagLoadData);
			LoadMesh("World4\\Track\\pxPodjem1.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagLoadData);
			LoadMesh("World4\\Track\\pxPodjem2.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagLoadData);
			LoadMesh("World4\\Track\\pxMost.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagLoadData);
			LoadMesh("World4\\Track\\pxMost2.r3d", false, false, false, Planet::wtWorld4 | MeshLib::cTagLoadData);

			LoadImage("World4\\Texture\\architect1.dds", cGenMipLevel, false, false,
			          Planet::wtWorld4 | ImageLib::cTagInitTex2d);
			LoadImage("World4\\Texture\\architect2.dds", cGenMipLevel, false, false,
			          Planet::wtWorld4 | ImageLib::cTagInitTex2d);
			LoadImage("World4\\Texture\\architect3.dds", cGenMipLevel, false, false,
			          Planet::wtWorld4 | ImageLib::cTagInitTex2d);
			LoadImage("World4\\Texture\\architect4.dds", cGenMipLevel, false, false,
			          Planet::wtWorld4 | ImageLib::cTagInitTex2d);
			LoadImage("World4\\Texture\\build.dds", cGenMipLevel, false, false,
			          Planet::wtWorld4 | ImageLib::cTagInitTex2d);
			LoadImage("World4\\Texture\\gora1.dds", cGenMipLevel, false, false,
			          Planet::wtWorld4 | ImageLib::cTagInitTex2d);
			LoadImage("World4\\Texture\\gora2.dds", cGenMipLevel, false, false,
			          Planet::wtWorld4 | ImageLib::cTagInitTex2d);
			LoadImage("World4\\Texture\\kolba.dds", cGenMipLevel, false, false,
			          Planet::wtWorld4 | ImageLib::cTagInitTex2d);
			LoadImage("World4\\Texture\\naves.dds", cGenMipLevel, false, false,
			          Planet::wtWorld4 | ImageLib::cTagInitTex2d);
			LoadImage("World4\\Texture\\lavaplace.dds", cGenMipLevel, false, false,
			          Planet::wtWorld4 | ImageLib::cTagInitTex2d);
			LoadImage("World4\\Texture\\pushka.dds", cGenMipLevel, false, false,
			          Planet::wtWorld4 | ImageLib::cTagInitTex2d);
			LoadImage("World4\\Texture\\volcano.dds", cGenMipLevel, false, false,
			          Planet::wtWorld4 | ImageLib::cTagInitTex2d);
			LoadImage("World4\\Texture\\crystals.dds", cGenMipLevel, false, false,
			          Planet::wtWorld4 | ImageLib::cTagInitTex2d);

			LoadImage("World4\\Track\\Texture\\track1.dds", cGenMipLevel, false, false,
			          Planet::wtWorld4 | ImageLib::cTagInitTex2d);
			LoadImage("World4\\Track\\Texture\\track2.dds", cGenMipLevel, false, false,
			          Planet::wtWorld4 | ImageLib::cTagInitTex2d);
			LoadImage("World4\\Track\\Texture\\most2.dds", cGenMipLevel, false, false,
			          Planet::wtWorld4 | ImageLib::cTagInitTex2d);

			LoadImage2dLibMat("World4\\architect1", "World4\\Texture\\architect1.dds");
			LoadImage2dLibMat("World4\\architect2", "World4\\Texture\\architect2.dds");
			LoadImage2dLibMat("World4\\architect3", "World4\\Texture\\architect3.dds");
			LoadImage2dLibMat("World4\\architect4", "World4\\Texture\\architect4.dds");
			LoadImage2dLibMat("World4\\build", "World4\\Texture\\build.dds");
			LoadImage2dLibMat("World4\\gora1", "World4\\Texture\\gora1.dds");
			LoadImage2dLibMat("World4\\gora2", "World4\\Texture\\gora2.dds");
			LoadImage2dLibMat("World4\\kolba", "World4\\Texture\\kolba.dds");
			LoadImage2dLibMat("World4\\naves", "World4\\Texture\\naves.dds");
			LoadImage2dLibMat("World4\\lavaplace", "World4\\Texture\\lavaplace.dds");
			LoadImage2dLibMat("World4\\pushka", "World4\\Texture\\pushka.dds");
			LoadImage2dLibMat("World4\\volcano", "World4\\Texture\\volcano.dds");
			LoadImage2dLibMat("World4\\crystals", "World4\\Texture\\crystals.dds");

			LoadImage2dLibMat("World4\\Track\\track1", "World4\\Track\\Texture\\track1.dds");
			LoadImage2dLibMat("World4\\Track\\track2", "World4\\Track\\Texture\\track2.dds");
			LoadAlphaTestLibMat("World4\\Track\\most2", "World4\\Track\\Texture\\most2.dds");
		}

		void ResourceManager::LoadWorld5()
		{
			LSL_LOG("resourceManager load world5");

			LoadMesh("World5\\cannon.r3d", false, false, false, Planet::wtWorld5 | MeshLib::cTagInitIVB);
			LoadMesh("World5\\cannon2.r3d", false, false, false, Planet::wtWorld5 | MeshLib::cTagInitIVB);
			LoadMesh("World5\\cannon3.r3d", false, false, false, Planet::wtWorld5 | MeshLib::cTagInitIVB);
			LoadMesh("World5\\mountain.r3d", false, false, false, Planet::wtWorld5 | MeshLib::cTagInitIVB);
			LoadMesh("World5\\naves.r3d", false, false, false, Planet::wtWorld5 | MeshLib::cTagInitIVB);
			LoadMesh("World5\\snowPlate.r3d", false, false, false, Planet::wtWorld5 | MeshLib::cTagInitIVB);
			LoadMesh("World5\\snowstone.r3d", false, false, false, Planet::wtWorld5 | MeshLib::cTagInitIVB);
			LoadMesh("World5\\snowstone2.r3d", false, false, false, Planet::wtWorld5 | MeshLib::cTagInitIVB);
			LoadMesh("World5\\snowTree.r3d", false, false, false, Planet::wtWorld5 | MeshLib::cTagInitIVB);
			LoadMesh("World5\\transportship.r3d", false, false, false, Planet::wtWorld5 | MeshLib::cTagInitIVB);
			LoadMesh("World5\\piece.r3d", false, false, false, Planet::wtWorld5 | MeshLib::cTagInitIVB);

			LoadMesh("World5\\Track\\most.r3d", false, false, false, Planet::wtWorld5 | MeshLib::cTagInitIVB);
			LoadMesh("World5\\Track\\podjem1.r3d", false, false, false, Planet::wtWorld5 | MeshLib::cTagInitIVB);
			LoadMesh("World5\\Track\\podjem2.r3d", false, false, false, Planet::wtWorld5 | MeshLib::cTagInitIVB);
			LoadMesh("World5\\Track\\podjem3.r3d", false, false, false, Planet::wtWorld5 | MeshLib::cTagInitIVB);
			LoadMesh("World5\\Track\\track1.r3d", false, false, false, Planet::wtWorld5 | MeshLib::cTagInitIVB);
			LoadMesh("World5\\Track\\track2.r3d", false, false, false, Planet::wtWorld5 | MeshLib::cTagInitIVB);
			LoadMesh("World5\\Track\\track3.r3d", false, false, false, Planet::wtWorld5 | MeshLib::cTagInitIVB);
			LoadMesh("World5\\Track\\track4.r3d", false, false, false, Planet::wtWorld5 | MeshLib::cTagInitIVB);
			LoadMesh("World5\\Track\\trackVentil.r3d", false, false, false, Planet::wtWorld5 | MeshLib::cTagInitIVB);
			LoadMesh("World5\\Track\\tramp1.r3d", false, false, false, Planet::wtWorld5 | MeshLib::cTagInitIVB);
			LoadMesh("World5\\Track\\tramp2.r3d", false, false, false, Planet::wtWorld5 | MeshLib::cTagInitIVB);

			LoadMesh("World5\\Track\\PXpodjem1.r3d", false, false, false, Planet::wtWorld5 | MeshLib::cTagLoadData);
			LoadMesh("World5\\Track\\PXpodjem2.r3d", false, false, false, Planet::wtWorld5 | MeshLib::cTagLoadData);
			LoadMesh("World5\\Track\\PXpodjem3.r3d", false, false, false, Planet::wtWorld5 | MeshLib::cTagLoadData);
			LoadMesh("World5\\Track\\PXtrack1.r3d", false, false, false, Planet::wtWorld5 | MeshLib::cTagLoadData);
			LoadMesh("World5\\Track\\PXtrack2.r3d", false, false, false, Planet::wtWorld5 | MeshLib::cTagLoadData);
			LoadMesh("World5\\Track\\PXtrack3.r3d", false, false, false, Planet::wtWorld5 | MeshLib::cTagLoadData);
			LoadMesh("World5\\Track\\PXtrack4.r3d", false, false, false, Planet::wtWorld5 | MeshLib::cTagLoadData);

			LoadImage("World5\\Texture\\cannon.dds", cGenMipLevel, false, false,
			          Planet::wtWorld5 | ImageLib::cTagInitTex2d);
			LoadImage("World5\\Texture\\cannon2.dds", cGenMipLevel, false, false,
			          Planet::wtWorld5 | ImageLib::cTagInitTex2d);
			LoadImage("World5\\Texture\\cannon3.dds", cGenMipLevel, false, false,
			          Planet::wtWorld5 | ImageLib::cTagInitTex2d);
			LoadImage("World5\\Texture\\piece.dds", cGenMipLevel, false, false,
			          Planet::wtWorld5 | ImageLib::cTagInitTex2d);
			LoadImage("World5\\Texture\\mountain.dds", cGenMipLevel, false, false,
			          Planet::wtWorld5 | ImageLib::cTagInitTex2d);
			LoadImage("World5\\Texture\\naves.dds", cGenMipLevel, false, false,
			          Planet::wtWorld5 | ImageLib::cTagInitTex2d);
			LoadImage("World5\\Texture\\snowPlate.dds", cGenMipLevel, false, false,
			          Planet::wtWorld5 | ImageLib::cTagInitTex2d);
			LoadImage("World5\\Texture\\snowstone.dds", cGenMipLevel, false, false,
			          Planet::wtWorld5 | ImageLib::cTagInitTex2d);
			LoadImage("World5\\Texture\\snowstone2.dds", cGenMipLevel, false, false,
			          Planet::wtWorld5 | ImageLib::cTagInitTex2d);
			LoadImage("World5\\Texture\\piece.dds", cGenMipLevel, false, false,
			          Planet::wtWorld5 | ImageLib::cTagInitTex2d);
			LoadImage("World5\\Texture\\transportship.dds", cGenMipLevel, false, false,
			          Planet::wtWorld5 | ImageLib::cTagInitTex2d);
			LoadImage("World5\\Texture\\treeSnow.dds", cGenMipLevel, false, false,
			          Planet::wtWorld5 | ImageLib::cTagInitTex2d);

			LoadImage("World5\\Track\\Texture\\most.dds", cGenMipLevel, false, false,
			          Planet::wtWorld5 | ImageLib::cTagInitTex2d);
			LoadImage("World5\\Track\\Texture\\track1.dds", cGenMipLevel, false, false,
			          Planet::wtWorld5 | ImageLib::cTagInitTex2d);

			LoadImage2dLibMat("World5\\snowPlate", "World5\\Texture\\snowPlate.dds");
			LoadImage2dLibMat("World5\\transportship", "World5\\Texture\\transportship.dds");
			LoadImage2dLibMat("World5\\cannon2", "World5\\Texture\\cannon2.dds");
			LoadImage2dLibMat("World5\\piece", "World5\\Texture\\piece.dds");
			LoadImage2dLibMat("World5\\snowstone2", "World5\\Texture\\snowstone2.dds");

			LoadAlphaTestLibMat("World5\\cannon", "World5\\Texture\\cannon.dds", false, cAlphaTestRef);
			LoadAlphaTestLibMat("World5\\cannon3", "World5\\Texture\\cannon3.dds", false, cAlphaTestRef);
			LoadAlphaTestLibMat("World5\\mountain", "World5\\Texture\\mountain.dds", false, cAlphaTestRef);
			LoadAlphaTestLibMat("World5\\naves", "World5\\Texture\\naves.dds", false, cAlphaTestRef);
			LoadAlphaTestLibMat("World5\\snowstone", "World5\\Texture\\snowstone.dds", false, cAlphaTestRef);
			LoadAlphaTestLibMat("World5\\treeSnow", "World5\\Texture\\treeSnow.dds", false, cAlphaTestRef);

			LoadImage2dLibMat("World5\\Track\\most", "World5\\Track\\Texture\\most.dds");
			LoadAlphaTestLibMat("World5\\Track\\track1", "World5\\Track\\Texture\\track1.dds", false, cAlphaTestRef);
		}

		void ResourceManager::LoadWorld6()
		{
			LSL_LOG("resourceManager load world6");

			LoadMesh("World6\\haus1.r3d", false, false, false, Planet::wtWorld6 | MeshLib::cTagInitIVB);
			LoadMesh("World6\\haus2.r3d", false, false, false, Planet::wtWorld6 | MeshLib::cTagInitIVB);
			LoadMesh("World6\\haus3.r3d", false, false, false, Planet::wtWorld6 | MeshLib::cTagInitIVB);
			LoadMesh("World6\\haus4.r3d", false, false, false, Planet::wtWorld6 | MeshLib::cTagInitIVB);
			LoadMesh("World6\\naves.r3d", false, false, false, Planet::wtWorld6 | MeshLib::cTagInitIVB);
			LoadMesh("World6\\nuke.r3d", false, false, false, Planet::wtWorld6 | MeshLib::cTagInitIVB);
			LoadMesh("World6\\stone1.r3d", false, false, false, Planet::wtWorld6 | MeshLib::cTagInitIVB);
			LoadMesh("World6\\stone2.r3d", false, false, false, Planet::wtWorld6 | MeshLib::cTagInitIVB);
			LoadMesh("World6\\stone3.r3d", false, false, false, Planet::wtWorld6 | MeshLib::cTagInitIVB);

			LoadMesh("World6\\Track\\block1.r3d", false, false, false, Planet::wtWorld6 | MeshLib::cTagInitIVB);
			LoadMesh("World6\\Track\\block2.r3d", false, false, false, Planet::wtWorld6 | MeshLib::cTagInitIVB);
			LoadMesh("World6\\Track\\entertunnel.r3d", false, false, false, Planet::wtWorld6 | MeshLib::cTagInitIVB);
			LoadMesh("World6\\Track\\jump.r3d", false, false, false, Planet::wtWorld6 | MeshLib::cTagInitIVB);
			LoadMesh("World6\\Track\\most.r3d", false, false, false, Planet::wtWorld6 | MeshLib::cTagInitIVB);
			LoadMesh("World6\\Track\\podjem1.r3d", false, false, false, Planet::wtWorld6 | MeshLib::cTagInitIVB);
			LoadMesh("World6\\Track\\track1.r3d", false, false, false, Planet::wtWorld6 | MeshLib::cTagInitIVB);
			LoadMesh("World6\\Track\\track2.r3d", false, false, false, Planet::wtWorld6 | MeshLib::cTagInitIVB);
			LoadMesh("World6\\Track\\track3.r3d", false, false, false, Planet::wtWorld6 | MeshLib::cTagInitIVB);
			LoadMesh("World6\\Track\\track4.r3d", false, false, false, Planet::wtWorld6 | MeshLib::cTagInitIVB);
			LoadMesh("World6\\Track\\track5.r3d", false, false, false, Planet::wtWorld6 | MeshLib::cTagInitIVB);
			LoadMesh("World6\\Track\\tramp1.r3d", false, false, false, Planet::wtWorld6 | MeshLib::cTagInitIVB);
			LoadMesh("World6\\Track\\tramp2.r3d", false, false, false, Planet::wtWorld6 | MeshLib::cTagInitIVB);
			LoadMesh("World6\\Track\\tunnel.r3d", false, false, false, Planet::wtWorld6 | MeshLib::cTagInitIVB);
			LoadMesh("World6\\Track\\most.r3d", false, false, false, Planet::wtWorld6 | MeshLib::cTagInitIVB);

			LoadMesh("World6\\Track\\pxEnterTunnel.r3d", false, false, false, Planet::wtWorld6 | MeshLib::cTagLoadData);
			LoadMesh("World6\\Track\\pxMost.r3d", false, false, false, Planet::wtWorld6 | MeshLib::cTagLoadData);
			LoadMesh("World6\\Track\\pxTunel.r3d", false, false, false, Planet::wtWorld6 | MeshLib::cTagLoadData);

			LoadImage("World6\\Texture\\stone.dds", cGenMipLevel, false, false,
			          Planet::wtWorld6 | ImageLib::cTagInitTex2d);
			LoadImage("World6\\Texture\\nuke.dds", cGenMipLevel, false, false,
			          Planet::wtWorld6 | ImageLib::cTagInitTex2d);
			LoadImage("World6\\Texture\\naves.dds", cGenMipLevel, false, false,
			          Planet::wtWorld6 | ImageLib::cTagInitTex2d);
			LoadImage("World6\\Texture\\haus4.dds", cGenMipLevel, false, false,
			          Planet::wtWorld6 | ImageLib::cTagInitTex2d);
			LoadImage("World6\\Texture\\haus3.dds", cGenMipLevel, false, false,
			          Planet::wtWorld6 | ImageLib::cTagInitTex2d);
			LoadImage("World6\\Texture\\haus2.dds", cGenMipLevel, false, false,
			          Planet::wtWorld6 | ImageLib::cTagInitTex2d);
			LoadImage("World6\\Texture\\haus1.dds", cGenMipLevel, false, false,
			          Planet::wtWorld6 | ImageLib::cTagInitTex2d);

			LoadImage("World6\\Track\\Texture\\tonnel.dds", cGenMipLevel, false, false,
			          Planet::wtWorld6 | ImageLib::cTagInitTex2d);
			LoadImage("World6\\Track\\Texture\\track1.dds", cGenMipLevel, false, false,
			          Planet::wtWorld6 | ImageLib::cTagInitTex2d);
			LoadImage("World6\\Track\\Texture\\tramplin3.dds", cGenMipLevel, false, false,
			          Planet::wtWorld6 | ImageLib::cTagInitTex2d);

			LoadImage2dLibMat("World6\\stone", "World6\\Texture\\stone.dds");
			LoadImage2dLibMat("World6\\nuke", "World6\\Texture\\nuke.dds");
			LoadImage2dLibMat("World6\\naves", "World6\\Texture\\naves.dds");
			LoadImage2dLibMat("World6\\haus4", "World6\\Texture\\haus4.dds");
			LoadImage2dLibMat("World6\\haus3", "World6\\Texture\\haus3.dds");
			LoadImage2dLibMat("World6\\haus2", "World6\\Texture\\haus2.dds");
			LoadImage2dLibMat("World6\\haus1", "World6\\Texture\\haus1.dds");

			LoadImage2dLibMat("World6\\Track\\tonnel", "World6\\Track\\Texture\\tonnel.dds");
			LoadImage2dLibMat("World6\\Track\\track1", "World6\\Track\\Texture\\track1.dds");
			LoadImage2dLibMat("World6\\Track\\tramplin3", "World6\\Track\\Texture\\tramplin3.dds");
		}

		void ResourceManager::LoadCrush()
		{
			LSL_LOG("resourceManager load crush");

			LoadMesh("Crush\\pregrada.r3d", false, true, true);
			LoadMesh("Crush\\crush1.r3d", false, true, true);
			LoadMesh("Crush\\crush2.r3d", false, true, true);
			LoadMesh("Crush\\bochka.r3d", false, true, true);
			LoadMesh("Crush\\dinamo.r3d", false, true, true);
			LoadMesh("Crush\\reklama.r3d", false, true, true);
			LoadMesh("Crush\\znak.r3d", false, true, true);
			LoadMesh("Crush\\box.r3d", false, true, true);

			LoadImage("Crush\\pregrada.dds", cGenMipLevel, true, false);
			LoadImage("Crush\\crush1.dds", cGenMipLevel, true, false);
			LoadImage("Crush\\crush2.dds", cGenMipLevel, true, false);
			LoadImage("Crush\\bochka.dds", cGenMipLevel, true, false);
			LoadImage("Crush\\dinamo.dds", cGenMipLevel, true, false);
			LoadImage("Crush\\reklama.dds", cGenMipLevel, true, false);
			LoadImage("Crush\\reklama2.dds", cGenMipLevel, true, false);
			LoadImage("Crush\\reklama3.dds", cGenMipLevel, true, false);
			LoadImage("Crush\\znak.dds", cGenMipLevel, true, false);
			LoadImage("Crush\\box.dds", cGenMipLevel, true, false);

			LoadImage2dLibMat("Crush\\pregrada", "Crush\\pregrada.dds");
			LoadImage2dLibMat("Crush\\crush1", "Crush\\crush1.dds");
			LoadImage2dLibMat("Crush\\crush2", "Crush\\crush2.dds");
			LoadImage2dLibMat("Crush\\bochka", "Crush\\bochka.dds");
			LoadImage2dLibMat("Crush\\dinamo", "Crush\\dinamo.dds");
			LoadImage2dLibMat("Crush\\reklama", "Crush\\reklama.dds");
			LoadImage2dLibMat("Crush\\reklama2", "Crush\\reklama2.dds");
			LoadImage2dLibMat("Crush\\reklama3", "Crush\\reklama3.dds");
			LoadImage2dLibMat("Crush\\znak", "Crush\\znak.dds");
			LoadImage2dLibMat("Crush\\box", "Crush\\box.dds");
		}

		void ResourceManager::LoadCars()
		{
			LSL_LOG("resourceManager load cars");

			LoadMesh("Car\\marauder.r3d");
			LoadMesh("Car\\marauderWheel.r3d");
			LoadMesh("Car\\buggi.r3d");
			LoadMesh("Car\\buggiWheel.r3d");
			LoadMesh("Car\\dirtdevil.r3d");
			LoadMesh("Car\\dirtdevilWheel.r3d");
			LoadMesh("Car\\tankchetti.r3d");
			LoadMesh("Car\\tankchettiWheel.r3d");
			LoadMesh("Car\\manticora.r3d");
			LoadMesh("Car\\manticoraWheel.r3d");
			LoadMesh("Car\\manticoraWheelBack.r3d");
			LoadMesh("Car\\manticoraGun.r3d");
			LoadMesh("Car\\airblade.r3d", false);
			LoadMesh("Car\\airbladeWheel.r3d", false);
			LoadMesh("Car\\guseniza.r3d");
			LoadMesh("Car\\gusenizaBoss.r3d");
			LoadMesh("Car\\monstertruck.r3d");
			LoadMesh("Car\\monstertruckWheel.r3d");
			LoadMesh("Car\\monstertruckBossWheel.r3d");
			LoadMesh("Car\\podushka.r3d");
			LoadMesh("Car\\podushkaBoss.r3d");
			LoadMesh("Car\\monstertruckBoss.r3d");
			LoadMesh("Car\\monstertruckBossWheel.r3d");
			LoadMesh("Car\\manticoraBoss.r3d");
			LoadMesh("Car\\devildriver.r3d");
			LoadMesh("Car\\devildriverBoss.r3d");
			LoadMesh("Car\\mustang.r3d");
			LoadMesh("Car\\mustangWheel.r3d");
			LoadMesh("Car\\xCar.r3d");
			LoadMesh("Car\\xCarWheel.r3d");

			LoadMesh("CarDestr\\marauderCrush.r3d", false, true, true);
			LoadMesh("CarDestr\\airbladeCrush.r3d", false, true, true);
			LoadMesh("CarDestr\\devildriverCrush.r3d", false, true, true);
			LoadMesh("CarDestr\\dirtdevilCrush.r3d", false, true, true);
			LoadMesh("CarDestr\\gusenizaCrush.r3d", false, true, true);
			LoadMesh("CarDestr\\manticoraCrush.r3d", false, true, true);
			LoadMesh("CarDestr\\monstertruckCrush.r3d", false, true, true);
			LoadMesh("CarDestr\\podushkaCrush.r3d", false, true, true);


			LoadImage("Car\\marauder.dds", cGenMipLevel);
			LoadImage("Car\\buggi.dds", cGenMipLevel);
			LoadImage("Car\\dirtdevil.dds", cGenMipLevel);
			LoadImage("Car\\tankchetti.dds", cGenMipLevel);
			LoadImage("Car\\manticora.dds", cGenMipLevel);
			LoadImage("Car\\airblade.dds", cGenMipLevel);
			LoadImage("Car\\guseniza.dds", cGenMipLevel);
			LoadImage("Car\\gusenizaChain.dds", cGenMipLevel);
			LoadImage("Car\\gusenizaBoss.dds", cGenMipLevel);
			LoadImage("Car\\monstertruck.dds", cGenMipLevel);
			LoadImage("Car\\podushka.dds", cGenMipLevel);
			LoadImage("Car\\podushkaBoss.dds", cGenMipLevel);
			LoadImage("Car\\monstertruckBoss.dds", cGenMipLevel);
			LoadImage("Car\\manticoraBoss.dds", cGenMipLevel);
			LoadImage("Car\\devildriver.dds", cGenMipLevel);
			LoadImage("Car\\devildriverBoss.dds", cGenMipLevel);
			LoadImage("Car\\mustang.dds", cGenMipLevel);
			LoadImage("Car\\xCar.dds", cGenMipLevel);

			LoadImage("CarDestr\\marauderCrush.dds", cGenMipLevel, true);
			LoadImage("CarDestr\\airbladeCrush.dds", cGenMipLevel, true);
			LoadImage("CarDestr\\devildriverCrush.dds", cGenMipLevel, true);
			LoadImage("CarDestr\\dirtdevilCrush.dds", cGenMipLevel, true);
			LoadImage("CarDestr\\gusenizaCrush.dds", cGenMipLevel, true);
			LoadImage("CarDestr\\manticoraCrush.dds", cGenMipLevel, true);
			LoadImage("CarDestr\\monstertruckCrush.dds", cGenMipLevel, true);
			LoadImage("CarDestr\\podushkaCrush.dds", cGenMipLevel, true);


			LoadCarLibMat("Car\\marauder", "Car\\marauder.dds");
			LoadCarLibMat("Car\\buggi", "Car\\buggi.dds");
			LoadCarLibMat("Car\\dirtdevil", "Car\\dirtdevil.dds");
			LoadCarLibMat("Car\\tankchetti", "Car\\tankchetti.dds");
			LoadCarLibMat("Car\\manticora", "Car\\manticora.dds");
			LoadCarLibMat("Car\\airblade", "Car\\airblade.dds"); //, "Car\\airbladeNorm.dds"
			LoadCarLibMat("Car\\guseniza", "Car\\guseniza.dds");
			LoadCarLibMat("Car\\gusenizaBoss", "Car\\gusenizaBoss.dds");
			LoadCarLibMat("Car\\monstertruck", "Car\\monstertruck.dds");
			LoadCarLibMat("Car\\podushka", "Car\\podushka.dds");
			LoadCarLibMat("Car\\podushkaBoss", "Car\\podushkaBoss.dds");
			LoadCarLibMat("Car\\monstertruckBoss", "Car\\monstertruckBoss.dds");
			LoadCarLibMat("Car\\manticoraBoss", "Car\\manticoraBoss.dds");
			LoadCarLibMat("Car\\devildriver", "Car\\devildriver.dds");
			LoadCarLibMat("Car\\devildriverBoss", "Car\\devildriverBoss.dds");
			LoadCarLibMat("Car\\mustang", "Car\\mustang.dds");
			LoadCarLibMat("Car\\xCar", "Car\\xCar.dds");

			LoadImage2dLibMat("Car\\marauderWheel", "Car\\marauder.dds");
			LoadImage2dLibMat("Car\\buggiWheel", "Car\\buggi.dds");
			LoadImage2dLibMat("Car\\dirtdevilWheel", "Car\\dirtdevil.dds");
			LoadImage2dLibMat("Car\\tankchettiWheel", "Car\\tankchetti.dds");
			LoadImage2dLibMat("Car\\manticoraWheel", "Car\\manticora.dds");
			LoadImage2dLibMat("Car\\airbladeWheel", "Car\\airblade.dds");
			{
				graph::LibMaterial& libMat = LoadAlphaTestLibMat("Car\\gusenizaChain", "Car\\gusenizaChain.dds", false);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressU, D3DTADDRESS_WRAP);
				libMat.samplers[0].samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);
				libMat.samplers[0].SetFiltering(graph::BaseSampler::sfAnisotropic);
			}
			LoadImage2dLibMat("Car\\monstertruckWheel", "Car\\monstertruck.dds");
			LoadImage2dLibMat("Car\\monstertruckBossWheel", "Car\\monstertruckBoss.dds");
			LoadImage2dLibMat("Car\\manticoraBossWheel", "Car\\manticoraBoss.dds");
			LoadImage2dLibMat("Car\\mustangWheel", "Car\\mustang.dds");

			LoadImage2dLibMat("CarDestr\\marauderCrush", "CarDestr\\marauderCrush.dds");
			LoadImage2dLibMat("CarDestr\\airbladeCrush", "CarDestr\\airbladeCrush.dds");
			LoadImage2dLibMat("CarDestr\\devildriverCrush", "CarDestr\\devildriverCrush.dds");
			LoadImage2dLibMat("CarDestr\\dirtdevilCrush", "CarDestr\\dirtdevilCrush.dds");
			LoadImage2dLibMat("CarDestr\\gusenizaCrush", "CarDestr\\gusenizaCrush.dds");
			LoadImage2dLibMat("CarDestr\\manticoraCrush", "CarDestr\\manticoraCrush.dds");
			LoadImage2dLibMat("CarDestr\\monstertruckCrush", "CarDestr\\monstertruckCrush.dds");
			LoadImage2dLibMat("CarDestr\\podushkaCrush", "CarDestr\\podushkaCrush.dds");

			//LoadMesh("Car\\monstertruck2.r3d");
			//LoadMesh("Car\\monstertruckWheel2.r3d");
			{
				graph::LibMaterial& libMat = GetMatLib().Add();
				libMat.SetName("Car\\blend");
				libMat.material.SetBlending(graph::Material::bmAdditive);
				libMat.material.SetAlpha(0.7f);
			}
		}

		void ResourceManager::LoadBonus()
		{
			LSL_LOG("resourceManager load bonus");

			LoadMesh("Bonus\\ammo.r3d", false, true, true);
			LoadMesh("Bonus\\medpack.r3d", false, true, true);
			LoadMesh("Bonus\\money.r3d", false, true, true);
			LoadMesh("Bonus\\shield.r3d", false, true, true);
			LoadMesh("Bonus\\rage.r3d", false, true, true);
			LoadMesh("Bonus\\lucky.r3d", false, true, true);
			LoadMesh("Bonus\\unlucky.r3d", false, true, true);
			LoadImage("Bonus\\ammo.dds", cGenMipLevel, true, false);
			LoadImage("Bonus\\medpack.dds", cGenMipLevel, true, false);
			LoadImage("Bonus\\money.dds", cGenMipLevel, true, false);
			LoadImage("Bonus\\shield.dds", cGenMipLevel, true, false);
			LoadImage("Bonus\\rage.dds", cGenMipLevel, true, false);
			LoadImage("Bonus\\lucky.dds", cGenMipLevel, true, false);
			LoadImage("Bonus\\unlucky.dds", cGenMipLevel, true, false);
			LoadImage("Bonus\\speedArrow.dds", cGenMipLevel, true, false);
			LoadImage("Bonus\\strelkaAnim.dds", cGenMipLevel, true, false);
			LoadImage("Bonus\\lusha.dds", cGenMipLevel, true, false);
			LoadImage("Bonus\\snowLusha.dds", cGenMipLevel, true, false);
			LoadImage("Bonus\\hellLusha.dds", cGenMipLevel, true, false);
			LoadImage("Bonus\\sand.dds", cGenMipLevel, true, false);
			LoadImage("Bonus\\graviton.dds", cGenMipLevel, true, false);
			LoadImage("Bonus\\spinner.dds", cGenMipLevel, true, false);
			LoadImage("Bonus\\maslo.dds", cGenMipLevel, true, false);
			LoadImage("Bonus\\maslo_top.dds", cGenMipLevel, true, false);
			LoadImage("Bonus\\acid.dds", cGenMipLevel, true, false);
			LoadImage("Bonus\\acid_top.dds", cGenMipLevel, true, false);
			LoadImage("Bonus\\camIso.dds", cGenMipLevel, true, false);
			LoadImage("Bonus\\camThird.dds", cGenMipLevel, true, false);
			LoadImage("Bonus\\camFree.dds", cGenMipLevel, true, false);
			LoadImage("Bonus\\autoCamOff.dds", cGenMipLevel, true, false);
			LoadImage("Bonus\\autoCamOn.dds", cGenMipLevel, true, false);
			LoadImage("Bonus\\altFinish.dds", cGenMipLevel, true, false);
			LoadImage("Bonus\\HeadLightOff.dds", cGenMipLevel, true, false);
			LoadImage("Bonus\\inRamp.dds", cGenMipLevel, true, false);
			LoadImage("Bonus\\aiJump.dds", cGenMipLevel, true, false);
			LoadImage("Bonus\\HeadLightOn.dds", cGenMipLevel, true, false);
			LoadImage("Bonus\\aiLockJump.dds", cGenMipLevel, true, false);
			LoadImage("Misc\\crack.dds", cGenMipLevel, true, false);


			LoadImage2dLibMat("Bonus\\ammo", "Bonus\\ammo.dds");
			LoadImage2dLibMat("Bonus\\medpack", "Bonus\\medpack.dds");
			LoadImage2dLibMat("Bonus\\money", "Bonus\\money.dds");
			LoadImage2dLibMat("Bonus\\shield", "Bonus\\shield.dds");
			LoadImage2dLibMat("Bonus\\rage", "Bonus\\rage.dds");
			LoadImage2dLibMat("Bonus\\lucky", "Bonus\\lucky.dds");
			LoadImage2dLibMat("Bonus\\unlucky", "Bonus\\unlucky.dds");
			LoadImage2dLibMat("Bonus\\speedArrow", "Bonus\\speedArrow.dds", true, graph::Material::bmTransparency).
				SetAnisoFlt();
			LoadImage2dLibMat("Bonus\\strelkaAnim", "Bonus\\strelkaAnim.dds", true, graph::Material::bmTransparency,
			                  1.0f, clrWhite, Vec2Range(NullVec2, IdentityVec2), Point2U(3, 2)).SetAnisoFlt();
			LoadImage2dLibMat("Bonus\\lusha", "Bonus\\lusha.dds", true, graph::Material::bmTransparency).SetAnisoFlt();
			LoadImage2dLibMat("Bonus\\spinner", "Bonus\\spinner.dds", true, graph::Material::bmTransparency).
				SetAnisoFlt();
			LoadImage2dLibMat("Bonus\\snowLusha", "Bonus\\snowLusha.dds", true, graph::Material::bmTransparency).
				SetAnisoFlt();
			LoadImage2dLibMat("Bonus\\hellLusha", "Bonus\\hellLusha.dds", true, graph::Material::bmTransparency).
				SetAnisoFlt();
			LoadImage2dLibMat("Bonus\\sand", "Bonus\\sand.dds", true, graph::Material::bmTransparency).SetAnisoFlt();
			LoadImage2dLibMat("Bonus\\graviton", "Bonus\\graviton.dds", true, graph::Material::bmTransparency).
				SetAnisoFlt();
			LoadImage2dLibMat("Bonus\\camIso", "Bonus\\camIso.dds", true, graph::Material::bmTransparency).
				SetAnisoFlt();
			LoadImage2dLibMat("Bonus\\camThird", "Bonus\\camThird.dds", true, graph::Material::bmTransparency).
				SetAnisoFlt();
			LoadImage2dLibMat("Bonus\\camFree", "Bonus\\camFree.dds", true, graph::Material::bmTransparency).
				SetAnisoFlt();
			LoadImage2dLibMat("Bonus\\autoCamOff", "Bonus\\autoCamOff.dds", true, graph::Material::bmTransparency).
				SetAnisoFlt();
			LoadImage2dLibMat("Bonus\\autoCamOn", "Bonus\\autoCamOn.dds", true, graph::Material::bmTransparency).
				SetAnisoFlt();
			LoadImage2dLibMat("Bonus\\altFinish", "Bonus\\altFinish.dds", true, graph::Material::bmTransparency).
				SetAnisoFlt();
			LoadImage2dLibMat("Bonus\\HeadLightOff", "Bonus\\HeadLightOff.dds", true,
			                  graph::Material::bmTransparency).SetAnisoFlt();
			LoadImage2dLibMat("Bonus\\inRamp", "Bonus\\inRamp.dds", true, graph::Material::bmTransparency).
				SetAnisoFlt();
			LoadImage2dLibMat("Bonus\\aiJump", "Bonus\\aiJump.dds", true, graph::Material::bmTransparency).
				SetAnisoFlt();
			LoadImage2dLibMat("Bonus\\HeadLightOn", "Bonus\\HeadLightOn.dds", true,
			                  graph::Material::bmTransparency).SetAnisoFlt();
			LoadImage2dLibMat("Bonus\\aiLockJump", "Bonus\\aiLockJump.dds", true, graph::Material::bmTransparency).
				SetAnisoFlt();
			LoadImage2dLibMat("Misc\\crack", "Misc\\crack.dds", true, graph::Material::bmTransparency).SetAnisoFlt();

			//maslo
			{
				graph::LibMaterial* libMat = &GetMatLib().Add();
				libMat->SetName("Bonus\\maslo");
				libMat->material.SetSpecPower(64.0f);
				libMat->material.SetSpecular(D3DXCOLOR(1, 1, 1, 1));
				libMat->material.SetBlending(graph::Material::bmTransparency);
				libMat->material.SetOption(graph::Material::moZWrite, false);

				AddSampler2dTo(*libMat, "Bonus\\maslo.dds", graph::Sampler2d::tmModulate);

				graph::Sampler2d& difSampler = AddSampler2dTo(*libMat, "Bonus\\maslo_top.dds");
				difSampler.stageStates.Set(graph::tssColorOp, D3DTOP_SELECTARG1);
				difSampler.stageStates.Set(graph::tssColorArg1, D3DTA_TEXTURE);
				difSampler.stageStates.Set(graph::tssTexCoordIndex, D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR);
			}

			//acid
			{
				graph::LibMaterial* libMat = &GetMatLib().Add();
				libMat->SetName("Bonus\\acid");
				libMat->material.SetSpecPower(64.0f);
				libMat->material.SetSpecular(D3DXCOLOR(1, 1, 1, 1));
				libMat->material.SetBlending(graph::Material::bmTransparency);
				libMat->material.SetOption(graph::Material::moZWrite, false);

				AddSampler2dTo(*libMat, "Bonus\\acid.dds", graph::Sampler2d::tmModulate);

				graph::Sampler2d& difSampler = AddSampler2dTo(*libMat, "Bonus\\acid_top.dds");
				difSampler.stageStates.Set(graph::tssColorOp, D3DTOP_SELECTARG1);
				difSampler.stageStates.Set(graph::tssColorArg1, D3DTA_TEXTURE);
				difSampler.stageStates.Set(graph::tssTexCoordIndex, D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR);
			}
		}

		void ResourceManager::LoadWeapons()
		{
			LSL_LOG("resourceManager load weapons");

			LoadMesh("Weapon\\blaster1.r3d");
			LoadMesh("Weapon\\pulsator.r3d");
			LoadMesh("Weapon\\plazma.r3d");
			LoadMesh("Weapon\\gun.r3d");
			LoadMesh("Weapon\\hyperdrive.r3d");
			LoadMesh("Weapon\\hyperNitro.r3d");
			LoadMesh("Weapon\\eazyNitro.r3d");
			LoadMesh("Weapon\\spikes.r3d");
			LoadMesh("Weapon\\core.r3d");
			LoadMesh("Weapon\\satellite.r3d");
			LoadMesh("Weapon\\barrel.r3d");
			LoadMesh("Weapon\\graviton.r3d");
			LoadMesh("Weapon\\aspirine.r3d");
			LoadMesh("Weapon\\shocker.r3d");
			LoadMesh("Weapon\\rocketGun.r3d");
			LoadMesh("Weapon\\shotGun.r3d");
			LoadMesh("Weapon\\artilleryGun.r3d");
			LoadMesh("Weapon\\magneticGun.r3d");
			LoadMesh("Weapon\\sapper.r3d");
			LoadMesh("Weapon\\phoenixGun.r3d");
			LoadMesh("Weapon\\grenadeGun.r3d");
			LoadMesh("Weapon\\discGun.r3d");
			LoadMesh("Weapon\\magmaGun.r3d");
			LoadMesh("Weapon\\sashka.r3d");
			LoadMesh("Weapon\\mine1.r3d");
			LoadMesh("Weapon\\mine2.r3d");
			LoadMesh("Weapon\\mine3.r3d");
			LoadMesh("Weapon\\mine4.r3d");
			LoadMesh("Weapon\\mine5.r3d");
			LoadMesh("Weapon\\burnmine.r3d");
			LoadMesh("Weapon\\mine2Piece.r3d");
			LoadMesh("Weapon\\rocket.r3d");
			LoadMesh("Weapon\\rocketAir.r3d");
			LoadMesh("Weapon\\rocketLauncher.r3d");
			LoadMesh("Weapon\\hyperBlaster.r3d");
			LoadMesh("Weapon\\torpeda.r3d");
			LoadMesh("Weapon\\bulletGun.r3d");
			LoadMesh("Weapon\\miniGun.r3d");
			LoadMesh("Weapon\\blasterGun.r3d");
			LoadMesh("Weapon\\tankLaser.r3d");
			LoadMesh("Weapon\\airLaser.r3d");
			LoadMesh("Weapon\\rifleProj.r3d");
			LoadMesh("Weapon\\rifleWeapon.r3d");
			LoadMesh("Weapon\\airWeapon.r3d");
			LoadMesh("Weapon\\maslo.r3d");
			LoadMesh("Weapon\\acid.r3d");
			LoadMesh("Weapon\\fireGun.r3d");
			LoadMesh("Weapon\\sphereGun.r3d");
			LoadMesh("Weapon\\ripper.r3d");
			LoadMesh("Weapon\\sonar.r3d");
			LoadMesh("Weapon\\shotBall.r3d");
			LoadMesh("Weapon\\turel.r3d");
			LoadMesh("Weapon\\spring.r3d");
			LoadMesh("Weapon\\shell.r3d");
			LoadMesh("Weapon\\asyncFrost.r3d");
			LoadMesh("Weapon\\asyncFrost2.r3d");
			LoadMesh("Weapon\\mortira.r3d");
			LoadMesh("Weapon\\molotov.r3d");
			LoadMesh("Weapon\\mortiraBall.r3d");
			LoadMesh("Weapon\\cartridge.r3d");
			LoadMesh("Weapon\\molotovFall.r3d");
			LoadMesh("Weapon\\phaseImpulse.r3d");
			LoadMesh("Weapon\\droid.r3d");
			LoadMesh("Weapon\\reflector.r3d");
			LoadMesh("Weapon\\rezonator.r3d");


			LoadImage("Weapon\\blaster1.dds", cGenMipLevel);
			LoadImage("Weapon\\pulsator.dds", cGenMipLevel);
			LoadImage("Weapon\\plazma.dds", cGenMipLevel);
			LoadImage("Weapon\\gun.dds", cGenMipLevel);
			LoadImage("Weapon\\rocketLauncher.dds", cGenMipLevel);
			LoadImage("Weapon\\rocket.dds", cGenMipLevel);
			LoadImage("Weapon\\rocketAir.dds", cGenMipLevel);
			LoadImage("Weapon\\torpeda.dds", cGenMipLevel);
			LoadImage("Weapon\\spikes.dds", cGenMipLevel);
			LoadImage("Weapon\\core.dds", cGenMipLevel);
			LoadImage("Weapon\\satellite.dds", cGenMipLevel);
			LoadImage("Weapon\\barrel.dds", cGenMipLevel);
			LoadImage("Weapon\\graviton.dds", cGenMipLevel);
			LoadImage("Weapon\\aspirine.dds", cGenMipLevel);
			LoadImage("Weapon\\shocker.dds", cGenMipLevel);
			LoadImage("Weapon\\rocketGun.dds", cGenMipLevel);
			LoadImage("Weapon\\shotGun.dds", cGenMipLevel);
			LoadImage("Weapon\\artilleryGun.dds", cGenMipLevel);
			LoadImage("Weapon\\magneticGun.dds", cGenMipLevel);
			LoadImage("Weapon\\sapper.dds", cGenMipLevel);
			LoadImage("Weapon\\phoenixGun.dds", cGenMipLevel);
			LoadImage("Weapon\\grenadeGun.dds", cGenMipLevel);
			LoadImage("Weapon\\discGun.dds", cGenMipLevel);
			LoadImage("Weapon\\magmaGun.dds", cGenMipLevel);
			LoadImage("Weapon\\sashka.dds", cGenMipLevel);
			LoadImage("Weapon\\mine1.dds", cGenMipLevel);
			LoadImage("Weapon\\mine2.dds", cGenMipLevel);
			LoadImage("Weapon\\mine3.dds", cGenMipLevel);
			LoadImage("Weapon\\mine4.dds", cGenMipLevel);
			LoadImage("Weapon\\mine5.dds", cGenMipLevel);
			LoadImage("Weapon\\burnmine.dds", cGenMipLevel);
			LoadImage("Weapon\\mine5White.dds", cGenMipLevel);
			LoadImage("Weapon\\mine5Blue.dds", cGenMipLevel);
			LoadImage("Weapon\\mine5Sea.dds", cGenMipLevel);
			LoadImage("Weapon\\mine5Gay.dds", cGenMipLevel);
			LoadImage("Weapon\\mine5Guy.dds", cGenMipLevel);
			LoadImage("Weapon\\mine5Turquoise.dds", cGenMipLevel);
			LoadImage("Weapon\\mine5EasyViolet.dds", cGenMipLevel);
			LoadImage("Weapon\\mine5BrightBrown.dds", cGenMipLevel);
			LoadImage("Weapon\\mine5Orange.dds", cGenMipLevel);
			LoadImage("Weapon\\mine5Peach.dds", cGenMipLevel);
			LoadImage("Weapon\\mine5BrightYellow.dds", cGenMipLevel);
			LoadImage("Weapon\\mine5Khaki.dds", cGenMipLevel);
			LoadImage("Weapon\\mine5Kargo.dds", cGenMipLevel);
			LoadImage("Weapon\\mine5Gray.dds", cGenMipLevel);
			LoadImage("Weapon\\mine5Yellow.dds", cGenMipLevel);
			LoadImage("Weapon\\mine5Acid.dds", cGenMipLevel);
			LoadImage("Weapon\\mine5Salad.dds", cGenMipLevel);
			LoadImage("Weapon\\mine5Birch.dds", cGenMipLevel);
			LoadImage("Weapon\\mine5BrightGreen.dds", cGenMipLevel);
			LoadImage("Weapon\\mine5Green.dds", cGenMipLevel);
			LoadImage("Weapon\\mine5Neo.dds", cGenMipLevel);
			LoadImage("Weapon\\mine5Brown.dds", cGenMipLevel);
			LoadImage("Weapon\\mine5Cherry.dds", cGenMipLevel);
			LoadImage("Weapon\\mine5Red.dds", cGenMipLevel);
			LoadImage("Weapon\\mine5Pink.dds", cGenMipLevel);
			LoadImage("Weapon\\mine5Barbie.dds", cGenMipLevel);
			LoadImage("Weapon\\mine5Violet.dds", cGenMipLevel);
			LoadImage("Weapon\\mine5DarkGray.dds", cGenMipLevel);
			LoadImage("Weapon\\hyperdrive.dds", cGenMipLevel);
			LoadImage("Weapon\\hyperNitro.dds", cGenMipLevel);
			LoadImage("Weapon\\eazyNitro.dds", cGenMipLevel);
			LoadImage("Weapon\\rifleProj.dds", cGenMipLevel);
			LoadImage("Weapon\\rifleProj2.dds", cGenMipLevel);
			LoadImage("Weapon\\hyperBlaster.dds", cGenMipLevel);
			LoadImage("Weapon\\maslo.dds", cGenMipLevel);
			LoadImage("Weapon\\acid.dds", cGenMipLevel);
			LoadImage("Weapon\\fireGun.dds", cGenMipLevel);
			LoadImage("Weapon\\miniGun.dds", cGenMipLevel);
			LoadImage("Weapon\\ripper.dds", cGenMipLevel);
			LoadImage("Weapon\\turel.dds", cGenMipLevel);
			LoadImage("Weapon\\spring.dds", cGenMipLevel);
			LoadImage("Weapon\\spring2.dds", cGenMipLevel);
			LoadImage("Weapon\\shell.dds", cGenMipLevel);
			LoadImage("Weapon\\shotBall.dds", cGenMipLevel);
			LoadImage("Weapon\\mortira.dds", cGenMipLevel);
			LoadImage("Weapon\\molotov.dds", cGenMipLevel);
			LoadImage("Weapon\\phaseImpulse.dds", cGenMipLevel);
			LoadImage("Weapon\\droid.dds", cGenMipLevel);
			LoadImage("Weapon\\reflector.dds", cGenMipLevel);
			LoadImage("Weapon\\rezonator.dds", cGenMipLevel);
			LoadImage("Weapon\\tankLaser.dds", cGenMipLevel);
			LoadImage("Weapon\\airLaser.dds", cGenMipLevel);


			LoadImage2dLibMat("Weapon\\blaster1", "Weapon\\blaster1.dds");
			LoadImage2dLibMat("Weapon\\pulsator", "Weapon\\pulsator.dds");
			LoadImage2dLibMat("Weapon\\plazma", "Weapon\\plazma.dds");
			LoadImage2dLibMat("Weapon\\gun", "Weapon\\gun.dds");
			LoadImage2dLibMat("Weapon\\hyperBlaster", "Weapon\\hyperBlaster.dds");
			LoadImage2dLibMat("Weapon\\rocketLauncher", "Weapon\\rocketLauncher.dds");
			LoadImage2dLibMat("Weapon\\rocket", "Weapon\\rocket.dds");
			LoadImage2dLibMat("Weapon\\rocketAir", "Weapon\\rocketAir.dds");
			LoadImage2dLibMat("Weapon\\torpeda", "Weapon\\torpeda.dds");
			LoadImage2dLibMat("Weapon\\spikes", "Weapon\\spikes.dds");
			LoadImage2dLibMat("Weapon\\core", "Weapon\\core.dds");
			LoadImage2dLibMat("Weapon\\satellite", "Weapon\\satellite.dds");
			LoadImage2dLibMat("Weapon\\barrel", "Weapon\\barrel.dds");
			LoadImage2dLibMat("Weapon\\graviton", "Weapon\\graviton.dds");
			LoadImage2dLibMat("Weapon\\aspirine", "Weapon\\aspirine.dds");
			LoadImage2dLibMat("Weapon\\shocker", "Weapon\\shocker.dds");
			LoadImage2dLibMat("Weapon\\rocketGun", "Weapon\\rocketGun.dds");
			LoadImage2dLibMat("Weapon\\shotGun", "Weapon\\shotGun.dds");
			LoadImage2dLibMat("Weapon\\artilleryGun", "Weapon\\artilleryGun.dds");
			LoadImage2dLibMat("Weapon\\magneticGun", "Weapon\\magneticGun.dds");
			LoadImage2dLibMat("Weapon\\sapper", "Weapon\\sapper.dds");
			LoadImage2dLibMat("Weapon\\phoenixGun", "Weapon\\phoenixGun.dds");
			LoadImage2dLibMat("Weapon\\grenadeGun", "Weapon\\grenadeGun.dds");
			LoadImage2dLibMat("Weapon\\discGun", "Weapon\\discGun.dds");
			LoadImage2dLibMat("Weapon\\magmaGun", "Weapon\\magmaGun.dds");
			LoadImage2dLibMat("Weapon\\sashka", "Weapon\\sashka.dds");
			LoadImage2dLibMat("Weapon\\mine1", "Weapon\\mine1.dds");
			LoadImage2dLibMat("Weapon\\mine2", "Weapon\\mine2.dds");
			LoadImage2dLibMat("Weapon\\mine3", "Weapon\\mine3.dds");
			LoadImage2dLibMat("Weapon\\mine4", "Weapon\\mine4.dds");
			LoadImage2dLibMat("Weapon\\mine5", "Weapon\\mine5.dds");
			LoadImage2dLibMat("Weapon\\burnmine", "Weapon\\burnmine.dds");
			LoadImage2dLibMat("Weapon\\mine5White", "Weapon\\mine5White.dds");
			LoadImage2dLibMat("Weapon\\mine5Blue", "Weapon\\mine5Blue.dds");
			LoadImage2dLibMat("Weapon\\mine5Sea", "Weapon\\mine5Sea.dds");
			LoadImage2dLibMat("Weapon\\mine5Gay", "Weapon\\mine5Gay.dds");
			LoadImage2dLibMat("Weapon\\mine5Guy", "Weapon\\mine5Guy.dds");
			LoadImage2dLibMat("Weapon\\mine5Turquoise", "Weapon\\mine5Turquoise.dds");
			LoadImage2dLibMat("Weapon\\mine5EasyViolet", "Weapon\\mine5EasyViolet.dds");
			LoadImage2dLibMat("Weapon\\mine5BrightBrown", "Weapon\\mine5BrightBrown.dds");
			LoadImage2dLibMat("Weapon\\mine5Orange", "Weapon\\mine5Orange.dds");
			LoadImage2dLibMat("Weapon\\mine5Peach", "Weapon\\mine5Peach.dds");
			LoadImage2dLibMat("Weapon\\mine5BrightYellow", "Weapon\\mine5BrightYellow.dds");
			LoadImage2dLibMat("Weapon\\mine5Khaki", "Weapon\\mine5Khaki.dds");
			LoadImage2dLibMat("Weapon\\mine5Kargo", "Weapon\\mine5Kargo.dds");
			LoadImage2dLibMat("Weapon\\mine5Gray", "Weapon\\mine5Gray.dds");
			LoadImage2dLibMat("Weapon\\mine5Yellow", "Weapon\\mine5Yellow.dds");
			LoadImage2dLibMat("Weapon\\mine5Acid", "Weapon\\mine5Acid.dds");
			LoadImage2dLibMat("Weapon\\mine5Salad", "Weapon\\mine5Salad.dds");
			LoadImage2dLibMat("Weapon\\mine5Birch", "Weapon\\mine5Birch.dds");
			LoadImage2dLibMat("Weapon\\mine5BrightGreen", "Weapon\\mine5BrightGreen.dds");
			LoadImage2dLibMat("Weapon\\mine5Green", "Weapon\\mine5Green.dds");
			LoadImage2dLibMat("Weapon\\mine5Neo", "Weapon\\mine5Neo.dds");
			LoadImage2dLibMat("Weapon\\mine5Brown", "Weapon\\mine5Brown.dds");
			LoadImage2dLibMat("Weapon\\mine5Cherry", "Weapon\\mine5Cherry.dds");
			LoadImage2dLibMat("Weapon\\mine5Red", "Weapon\\mine5Red.dds");
			LoadImage2dLibMat("Weapon\\mine5Pink", "Weapon\\mine5Pink.dds");
			LoadImage2dLibMat("Weapon\\mine5Barbie", "Weapon\\mine5Barbie.dds");
			LoadImage2dLibMat("Weapon\\mine5Violet", "Weapon\\mine5Violet.dds");
			LoadImage2dLibMat("Weapon\\mine5DarkGray", "Weapon\\mine5DarkGray.dds");
			LoadImage2dLibMat("Weapon\\hyperdrive", "Weapon\\hyperdrive.dds");
			LoadImage2dLibMat("Weapon\\hyperNitro", "Weapon\\hyperNitro.dds");
			LoadImage2dLibMat("Weapon\\eazyNitro", "Weapon\\eazyNitro.dds");
			LoadImage2dLibMat("Weapon\\rifleProj", "Weapon\\rifleProj.dds");
			LoadImage2dLibMat("Weapon\\rifleProj2", "Weapon\\rifleProj2.dds");
			LoadImage2dLibMat("Weapon\\maslo", "Weapon\\maslo.dds");
			LoadImage2dLibMat("Weapon\\acid", "Weapon\\acid.dds");
			LoadImage2dLibMat("Weapon\\shotBall", "Weapon\\shotBall.dds");
			LoadLibMat("Weapon\\mortiraBall", false, graph::Material::bmOpaque, 1.0f, clrBlack, clrBlack);
			LoadLibMat("Weapon\\cartridge", false, graph::Material::bmOpaque, 1.0f, clrBlack, clrBlack);
			LoadLibMat("Weapon\\molotovFall", false, graph::Material::bmOpaque, 1.0f, clrRed, clrRed);

			LoadImage2dLibMat("Weapon\\bulletGun", "Car\\marauder.dds");
			LoadImage2dLibMat("Weapon\\miniGun", "Weapon\\miniGun.dds");
			LoadImage2dLibMat("Weapon\\blasterGun", "Car\\manticora.dds");
			LoadImage2dLibMat("Weapon\\tankLaser", "Weapon\\tankLaser.dds");
			LoadImage2dLibMat("Weapon\\airLaser", "Weapon\\airLaser.dds");
			LoadImage2dLibMat("Weapon\\rifleWeapon", "Car\\dirtdevil.dds");
			LoadImage2dLibMat("Weapon\\airWeapon", "Car\\airblade.dds");
			LoadImage2dLibMat("Weapon\\fireGun", "Weapon\\fireGun.dds");
			LoadImage2dLibMat("Weapon\\ripper", "Weapon\\ripper.dds");
			LoadImage2dLibMat("Weapon\\turel", "Weapon\\turel.dds");
			LoadImage2dLibMat("Weapon\\spring", "Weapon\\spring.dds");
			LoadImage2dLibMat("Weapon\\spring2", "Weapon\\spring2.dds");
			LoadImage2dLibMat("Weapon\\shell", "Weapon\\shell.dds");
			LoadImage2dLibMat("Weapon\\asyncFrost", "Car\\monstertruck.dds");
			LoadImage2dLibMat("Weapon\\asyncFrost2", "Car\\monstertruckBoss.dds");
			LoadImage2dLibMat("Weapon\\mortira", "Weapon\\mortira.dds");
			LoadImage2dLibMat("Weapon\\molotov", "Weapon\\molotov.dds");
			LoadImage2dLibMat("Weapon\\phaseImpulse", "Weapon\\phaseImpulse.dds");
			LoadImage2dLibMat("Weapon\\droid", "Weapon\\droid.dds");
			LoadImage2dLibMat("Weapon\\reflector", "Weapon\\reflector.dds");
			LoadImage2dLibMat("Weapon\\rezonator", "Weapon\\rezonator.dds");
		}

		void ResourceManager::LoadUpgrades()
		{
			LSL_LOG("resourceManager load upgrades");

			LoadMesh("Upgrade\\armor1.r3d");
			LoadMesh("Upgrade\\armor2.r3d");
			LoadMesh("Upgrade\\armor3.r3d");
			LoadMesh("Upgrade\\armor4.r3d");
			LoadMesh("Upgrade\\engine1.r3d");
			LoadMesh("Upgrade\\engine2.r3d");
			LoadMesh("Upgrade\\engine3.r3d");
			LoadMesh("Upgrade\\engine4.r3d");
			LoadMesh("Upgrade\\trans1.r3d");
			LoadMesh("Upgrade\\trans2.r3d");
			LoadMesh("Upgrade\\trans3.r3d");
			LoadMesh("Upgrade\\trans4.r3d");
			LoadMesh("Upgrade\\gusWheel1.r3d");
			LoadMesh("Upgrade\\gusWheel2.r3d");
			LoadMesh("Upgrade\\gusWheel3.r3d");
			LoadMesh("Upgrade\\gusWheel4.r3d");
			LoadMesh("Upgrade\\wheel1.r3d");
			LoadMesh("Upgrade\\wheel2.r3d");
			LoadMesh("Upgrade\\wheel3.r3d");
			LoadMesh("Upgrade\\wheel4.r3d");
			LoadMesh("Upgrade\\truba1.r3d");
			LoadMesh("Upgrade\\truba2.r3d");
			LoadMesh("Upgrade\\truba3.r3d");
			LoadMesh("Upgrade\\truba4.r3d");

			LoadImage("Upgrade\\armor1.dds", cGenMipLevel);
			LoadImage("Upgrade\\armor2.dds", cGenMipLevel);
			LoadImage("Upgrade\\armor3.dds", cGenMipLevel);
			LoadImage("Upgrade\\armor4.dds", cGenMipLevel);
			LoadImage("Upgrade\\engine1.dds", cGenMipLevel);
			LoadImage("Upgrade\\engine2.dds", cGenMipLevel);
			LoadImage("Upgrade\\engine3.dds", cGenMipLevel);
			LoadImage("Upgrade\\engine4.dds", cGenMipLevel);
			LoadImage("Upgrade\\trans1.dds", cGenMipLevel);
			LoadImage("Upgrade\\trans2.dds", cGenMipLevel);
			LoadImage("Upgrade\\trans3.dds", cGenMipLevel);
			LoadImage("Upgrade\\trans4.dds", cGenMipLevel);
			LoadImage("Upgrade\\gusWheel1.dds", cGenMipLevel);
			LoadImage("Upgrade\\gusWheel3.dds", cGenMipLevel);
			LoadImage("Upgrade\\gusWheel4.dds", cGenMipLevel);
			LoadImage("Upgrade\\wheel1.dds", cGenMipLevel);
			LoadImage("Upgrade\\wheel2.dds", cGenMipLevel);
			LoadImage("Upgrade\\wheel3.dds", cGenMipLevel);
			LoadImage("Upgrade\\wheel4.dds", cGenMipLevel);
			LoadImage("Upgrade\\truba1.dds", cGenMipLevel);
			LoadImage("Upgrade\\truba2.dds", cGenMipLevel);
			LoadImage("Upgrade\\truba3.dds", cGenMipLevel);
			LoadImage("Upgrade\\truba4.dds", cGenMipLevel);
		}

		void ResourceManager::LoadGUI()
		{
			LSL_LOG("resourceManager load gui");

			LoadMesh("GUI\\planet.r3d", false, true, true);
			LoadMesh("GUI\\garage.r3d", false, true, true);
			LoadMesh("GUI\\angar.r3d", false, true, true);
			LoadMesh("GUI\\question.r3d", false, true, true);

			LoadImage("GUI\\final.dds");
			LoadImage("GUI\\loadingFrame.dds");
			LoadImage("GUI\\startLogo.dds");
			LoadImage("GUI\\yardLogo.png", 1, false, false, -1, true);
			LoadImage("GUI\\laboratoria24.png", 1, false, false, -1, true);
			LoadImage("GUI\\XRayDS.png", 1, false, false, -1, true);
			LoadImage("GUI\\HUD\\mapFrame.png", 1, false, false, -1, true);
			LoadImage("GUI\\HUD\\mapFrameDark.png", 1, false, false, -1, true);
			LoadImage("GUI\\HUD\\mapFrameClassic.png", 1, false, false, -1, true);
			LoadImage("GUI\\HUD\\mapFrameX.png", 1, false, false, -1, true);
			LoadImage("GUI\\HUD\\start.png", 1, false, false, -1, true);
			LoadImage("GUI\\HUD\\playerPoint2.png", 1, false, false, -1, true);
			LoadImage("GUI\\HUD\\tablo0.png", 1, false, false, -1, true);
			LoadImage("GUI\\HUD\\tablo1.png", 1, false, false, -1, true);
			LoadImage("GUI\\HUD\\tablo2.png", 1, false, false, -1, true);
			LoadImage("GUI\\HUD\\tablo3.png", 1, false, false, -1, true);
			LoadImage("GUI\\HUD\\tablo4.png", 1, false, false, -1, true);
			LoadImage("GUI\\cursor.png", 1, false, false, -1, true);

			LoadImage("GUI\\garage1.dds", cGenMipLevel, true, false);
			LoadImage("GUI\\garage2.dds", cGenMipLevel, true, false);
			LoadImage("GUI\\angar1.dds", cGenMipLevel, true, false);
			LoadImage("GUI\\angar2.dds", cGenMipLevel, true, false);
			LoadImage("GUI\\question.png", cGenMipLevel, true, false);

			LoadImage("GUI\\space2.dds", cGenMipLevel);
			LoadImage("GUI\\earth.dds", cGenMipLevel);
			LoadImage("GUI\\intaria.dds", cGenMipLevel);
			LoadImage("GUI\\patagonis.dds", cGenMipLevel);
			LoadImage("GUI\\chemi5.dds", cGenMipLevel);
			LoadImage("GUI\\inferno.dds", cGenMipLevel);
			LoadImage("GUI\\nho.dds", cGenMipLevel);
			LoadImage("GUI\\planetX.dds", cGenMipLevel);
			//
			LoadImage("GUI\\Planets\\christophPlanet.dds", cGenMipLevel);
			LoadImage("GUI\\Planets\\garryPlanet.dds", cGenMipLevel);
			LoadImage("GUI\\Planets\\snakePlanet.dds", cGenMipLevel);
			LoadImage("GUI\\Planets\\tarquinnPlanet.dds", cGenMipLevel);
			LoadImage("GUI\\Planets\\violetPlanet.dds", cGenMipLevel);
			LoadImage("GUI\\Planets\\viperPlanet.dds", cGenMipLevel);

			LoadImage("GUI\\Chars\\butcher.png", 1, false, false, -1, true);
			LoadImage("GUI\\Chars\\gank.png", 1, false, false, -1, true);
			LoadImage("GUI\\Chars\\gerry.png", 1, false, false, -1, true);
			LoadImage("GUI\\Chars\\k-jin.png", 1, false, false, -1, true);
			LoadImage("GUI\\Chars\\kristoph.png", 1, false, false, -1, true);
			LoadImage("GUI\\Chars\\mardock.png", 1, false, false, -1, true);
			LoadImage("GUI\\Chars\\rip.png", 1, false, false, -1, true);
			LoadImage("GUI\\Chars\\shred.png", 1, false, false, -1, true);
			LoadImage("GUI\\Chars\\snake.png", 1, false, false, -1, true);
			LoadImage("GUI\\Chars\\stinkle.png", 1, false, false, -1, true);
			LoadImage("GUI\\Chars\\tarquin.png", 1, false, false, -1, true);
			LoadImage("GUI\\Chars\\tyler.png", 1, false, false, -1, true);
			LoadImage("GUI\\Chars\\spectator.png", 1, false, false, -1, true);
			LoadImage("GUI\\Chars\\violet.png", 1, false, false, -1, true);
			LoadImage("GUI\\Chars\\viper.png", 1, false, false, -1, true);

			//hud
			LoadImage("GUI\\HUD\\placeMineHyper.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\AlphaBack.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\MnBgr.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\classicWeaponPanel.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\classicWeaponPanel2.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\classicWeaponPanel3.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\classicWeaponPanel4.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\SilverPlaceMineHyper.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\SilverSlot.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\SilverSlotSel.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\slot.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\zlot.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\slotSel.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\zlotSel.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\lap.png", 1, true, false, -1, true);


			LoadImage("GUI\\HUD\\lapz0.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\lapz1.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\lapz2.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\lapz3.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\lapz4.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\lapz5.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\lapz6.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\lapz7.png", 1, true, false, -1, true);


			LoadImage("GUI\\HUD\\lifeBar.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\lifeBar2.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\lifeBarSilver.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\AmmoBar.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\lifeBarBack.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\AmmoBack.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\Device.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\lifeBarClassicBack.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\lifeBarClassic.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\playerPoint.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\pickArmor.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\inRage.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\Lucky.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\Empty.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\UnLucky.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\pickNitro.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\pickMine.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\pickWeapon.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\pickMoney.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\pickImmortal.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\playerKill.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\pickArmorClassic.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\pickNitroClassic.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\pickMineClassic.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\pickWeaponClassic.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\pickMoneyClassic.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\pickImmortalClassic.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\playerKillClassic.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\WrongWay.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\LastLap.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\Skull.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\carLifeBar.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\carLifeBarX.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\carLifeBarSilver.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\carLifeBack.png", 1, true, false, -1, true);
			LoadImage("GUI\\HUD\\carLifeBackX.png", 1, true, false, -1, true);

			//race
			LoadImage("GUI\\bottomPanel.png", 1, false, false, -1, true);
			LoadImage("GUI\\topPanel.png", 1, false, false, -1, true);
			LoadImage("GUI\\moneyBg.png", 1, false, false, -1, true);
			LoadImage("GUI\\statFrame.png", 1, false, false, -1, true);
			LoadImage("GUI\\buttonBg1.png", 1, false, false, -1, true);
			LoadImage("GUI\\buttonBgSel1.png", 1, false, false, -1, true);
			LoadImage("GUI\\icoStart.png", 1, false, false, -1, true);
			LoadImage("GUI\\statBar.png", 1, false, false, -1, true);
			LoadImage("GUI\\imageFrame1.png", 1, false, false, -1, true);
			LoadImage("GUI\\chargeBar1.png", 1, false, false, -1, true);
			LoadImage("GUI\\icoWorkshop.png", 1, false, false, -1, true);
			LoadImage("GUI\\icoSpace.png", 1, false, false, -1, true);
			LoadImage("GUI\\icoGarage.png", 1, false, false, -1, true);
			LoadImage("GUI\\icoAchivment.png", 1, false, false, -1, true);
			LoadImage("GUI\\icoOptions.png", 1, false, false, -1, true);
			LoadImage("GUI\\icoExit.png", 1, false, false, -1, true);

			//race net	
			LoadImage("GUI\\netPlayerFrame.png", 1, false, false, -1, true);
			LoadImage("GUI\\netPlayerReadyState.png", 1, false, false, -1, true);
			LoadImage("GUI\\netPlayerReadyStateSel.png", 1, false, false, -1, true);
			LoadImage("GUI\\netPlayerKick.png", 1, false, false, -1, true);
			LoadImage("GUI\\netPlayerKickSel.png", 1, false, false, -1, true);

			//garage
			LoadImage("GUI\\arrow1.png", 1, false, false, -1, true);
			LoadImage("GUI\\arrowUp.png", 1, false, false, -1, true);
			LoadImage("GUI\\arrowDown.png", 1, false, false, -1, true);
			LoadImage("GUI\\arrowSel1.png", 1, false, false, -1, true);
			LoadImage("GUI\\arrowUpSel.png", 1, false, false, -1, true);
			LoadImage("GUI\\arrowDownSel.png", 1, false, false, -1, true);
			LoadImage("GUI\\bottomPanel2.png", 1, false, false, -1, true);
			LoadImage("GUI\\rightPanel2.png", 1, false, false, -1, true);
			LoadImage("GUI\\statFrame2.png", 1, false, false, -1, true);
			LoadImage("GUI\\topPanel2.png", 1, false, false, -1, true);
			LoadImage("GUI\\buttonBg2.png", 1, false, false, -1, true);
			LoadImage("GUI\\buttonBgSel2.png", 1, false, false, -1, true);
			LoadImage("GUI\\statBar2.png", 1, false, false, -1, true);
			LoadImage("GUI\\colorBox.png", 1, false, false, -1, true);
			LoadImage("GUI\\colorBoxBg.png", 1, false, false, -1, true);
			LoadImage("GUI\\colorBoxBgSel.png", 1, false, false, -1, true);
			LoadImage("GUI\\carBox.png", 1, false, false, -1, true);
			LoadImage("GUI\\carBoxSel.png", 1, false, false, -1, true);
			LoadImage("GUI\\header2.png", 1, false, false, -1, true);
			LoadImage("GUI\\lock.png", 1, false, false, -1, true);
			LoadImage("GUI\\buyButton_russian.png", 1, false, false, -1, true);
			LoadImage("GUI\\buyButtonSel_russian.png", 1, false, false, -1, true);
			LoadImage("GUI\\buyButton_english.png", 1, false, false, -1, true);
			LoadImage("GUI\\buyButtonSel_english.png", 1, false, false, -1, true);

			//shop
			LoadImage("GUI\\bottomPanel3.png", 1, false, false, -1, true);
			LoadImage("GUI\\chargeBar.png", 1, false, false, -1, true);
			LoadImage("GUI\\chargeBox.png", 1, false, false, -1, true);
			LoadImage("GUI\\chargeButton.png", 1, false, false, -1, true);
			LoadImage("GUI\\chargeButtonSel.png", 1, false, false, -1, true);
			LoadImage("GUI\\UpgBtn.png", 1, false, false, -1, true);
			LoadImage("GUI\\UpgBtnSel.png", 1, false, false, -1, true);
			LoadImage("GUI\\leftPanel3.png", 1, false, false, -1, true);
			LoadImage("GUI\\slot2.png", 1, false, false, -1, true);
			LoadImage("GUI\\slot2Frame.png", 1, false, false, -1, true);
			LoadImage("GUI\\topPanel3.png", 1, false, false, -1, true);
			LoadImage("GUI\\upLevel1.png", 1, false, false, -1, true);
			LoadImage("GUI\\upLevel2.png", 1, false, false, -1, true);
			LoadImage("GUI\\upLevel3.png", 1, false, false, -1, true);
			LoadImage("GUI\\upLevel4.png", 1, false, false, -1, true);
			LoadImage("GUI\\statBar2Plus.png", 1, false, false, -1, true);
			LoadImage("GUI\\mineSlot.png", 1, false, false, -1, true);
			LoadImage("GUI\\wpnSlot.png", 1, false, false, -1, true);
			LoadImage("GUI\\wpnSlotSel.png", 1, false, false, -1, true);
			LoadImage("GUI\\hyperSlot.png", 1, false, false, -1, true);

			//cars
			LoadImage("GUI\\Cars\\marauder.png", 1, false, false, -1, true);
			LoadImage("GUI\\Cars\\dirtdevil.png", 1, false, false, -1, true);
			LoadImage("GUI\\Cars\\manticora.png", 1, false, false, -1, true);
			LoadImage("GUI\\Cars\\airblade.png", 1, false, false, -1, true);
			LoadImage("GUI\\Cars\\guseniza.png", 1, false, false, -1, true);
			LoadImage("GUI\\Cars\\podushka.png", 1, false, false, -1, true);
			LoadImage("GUI\\Cars\\monstertruck.png", 1, false, false, -1, true);
			LoadImage("GUI\\Cars\\devildriver.png", 1, false, false, -1, true);
			LoadImage("GUI\\Cars\\buggi.png", 1, false, false, -1, true);
			LoadImage("GUI\\Cars\\tankchetti.png", 1, false, false, -1, true);
			LoadImage("GUI\\Cars\\mustang.png", 1, false, false, -1, true);
			LoadImage("GUI\\Cars\\manticoraBoss.png", 1, false, false, -1, true);
			LoadImage("GUI\\Cars\\gusenizaBoss.png", 1, false, false, -1, true);
			LoadImage("GUI\\Cars\\podushkaBoss.png", 1, false, false, -1, true);
			LoadImage("GUI\\Cars\\monstertruckBoss.png", 1, false, false, -1, true);
			LoadImage("GUI\\Cars\\devildriverBoss.png", 1, false, false, -1, true);
			LoadImage("GUI\\Cars\\xCar.png", 1, false, false, -1, true);

			//rewards
			LoadImage("GUI\\Rewards\\viperLock.png", 1, false, false, -1, true);
			LoadImage("GUI\\Rewards\\viper.png", 1, false, false, -1, true);
			LoadImage("GUI\\Rewards\\buggiLock.png", 1, false, false, -1, true);
			LoadImage("GUI\\Rewards\\buggi.png", 1, false, false, -1, true);
			LoadImage("GUI\\Rewards\\airbladeLock.png", 1, false, false, -1, true);
			LoadImage("GUI\\Rewards\\airblade.png", 1, false, false, -1, true);
			LoadImage("GUI\\Rewards\\reflectorLock.png", 1, false, false, -1, true);
			LoadImage("GUI\\Rewards\\reflector.png", 1, false, false, -1, true);
			LoadImage("GUI\\Rewards\\droid.png", 1, false, false, -1, true);
			LoadImage("GUI\\Rewards\\droidLock.png", 1, false, false, -1, true);
			LoadImage("GUI\\Rewards\\tankchetti.png", 1, false, false, -1, true);
			LoadImage("GUI\\Rewards\\tankchettiLock.png", 1, false, false, -1, true);
			LoadImage("GUI\\Rewards\\phaser.png", 1, false, false, -1, true);
			LoadImage("GUI\\Rewards\\phaserLock.png", 1, false, false, -1, true);
			LoadImage("GUI\\Rewards\\mustang.png", 1, false, false, -1, true);
			LoadImage("GUI\\Rewards\\mustangLock.png", 1, false, false, -1, true);
			LoadImage("GUI\\Rewards\\musicTrack.png", 1, false, false, -1, true);
			LoadImage("GUI\\Rewards\\musicTrackLock.png", 1, false, false, -1, true);

			//achievments	
			LoadImage("GUI\\Achievments\\points400.png", 1, true, false, -1, true);
			LoadImage("GUI\\Achievments\\points500.png", 1, true, false, -1, true);
			LoadImage("GUI\\Achievments\\points700.png", 1, true, false, -1, true);
			LoadImage("GUI\\Achievments\\points800.png", 1, true, false, -1, true);
			LoadImage("GUI\\Achievments\\points1000.png", 1, true, false, -1, true);
			LoadImage("GUI\\Achievments\\points1500.png", 1, true, false, -1, true);
			LoadImage("GUI\\Achievments\\armored.png", 1, true, false, -1, true);
			LoadImage("GUI\\Achievments\\breakRacer.png", 1, true, false, -1, true);
			LoadImage("GUI\\Achievments\\devastator.png", 1, true, false, -1, true);
			LoadImage("GUI\\Achievments\\doubleKill.png", 1, true, false, -1, true);
			//LoadImage("GUI\\Achievments\\singleKill.png", 1, true, false, -1, true);
			LoadImage("GUI\\Achievments\\megaKill.png", 1, true, false, -1, true);
			LoadImage("GUI\\Achievments\\monsterKill.png", 1, true, false, -1, true);
			LoadImage("GUI\\Achievments\\megaRacer.png", 1, true, false, -1, true);
			LoadImage("GUI\\Achievments\\moneyBags.png", 1, true, false, -1, true);
			LoadImage("GUI\\Achievments\\tripleKill.png", 1, true, false, -1, true);
			LoadImage("GUI\\Achievments\\survival.png", 1, true, false, -1, true);
			LoadImage("GUI\\Achievments\\firstBlood.png", 1, true, false, -1, true);
			LoadImage("GUI\\Achievments\\Bomber.png", 1, true, false, -1, true);
			LoadImage("GUI\\Achievments\\LastLap.png", 1, true, false, -1, true);

			//gamers
			LoadImage("GUI\\arrow2.png", 1, false, false, -1, true);
			LoadImage("GUI\\arrowSel2.png", 1, false, false, -1, true);
			LoadImage("GUI\\bottomPanel4.png", 1, false, false, -1, true);
			LoadImage("GUI\\wndLight4.png", 1, false, false, -1, true);
			LoadImage("GUI\\space1.dds");

			//main
			LoadImage("GUI\\bottomPanel5.png", 1, false, false, -1, true);
			LoadImage("GUI\\mainItemSel5.png", 1, false, false, -1, true);
			LoadImage("GUI\\topPanel5.png", 1, false, false, -1, true);
			LoadImage("GUI\\mainFrame.dds");

			//angar
			LoadImage("GUI\\bottomPanel6.png", 1, false, false, -1, true);
			LoadImage("GUI\\doorDown.png", 1, false, false, -1, true);
			LoadImage("GUI\\doorUp.png", 1, false, false, -1, true);
			LoadImage("GUI\\doorSlot.png", 1, false, false, -1, true);
			LoadImage("GUI\\doorSlotSel.png", 1, false, false, -1, true);
			LoadImage("GUI\\planetInfo.png", 1, false, false, -1, true);
			LoadImage("GUI\\wndLight6.png", 1, false, false, -1, true);
			LoadImage("GUI\\buttonBg6.png", 1, false, false, -1, true);
			LoadImage("GUI\\buttonBgSel6.png", 1, false, false, -1, true);

			//options	
			LoadImage("GUI\\arrow3.png", 1, false, false, -1, true);
			LoadImage("GUI\\arrowSel3.png", 1, false, false, -1, true);
			LoadImage("GUI\\buttonBg4.png", 1, false, false, -1, true);
			LoadImage("GUI\\buttonBgSel4.png", 1, false, false, -1, true);
			LoadImage("GUI\\buttonBg5.png", 1, false, false, -1, true);
			LoadImage("GUI\\optBar.png", 1, false, false, -1, true);
			LoadImage("GUI\\optBarBg.png", 1, false, false, -1, true);
			LoadImage("GUI\\optionsBg.png", 1, false, false, -1, true);
			LoadImage("GUI\\labelBg1.png", 1, false, false, -1, true);
			LoadImage("GUI\\labelBg2.png", 1, false, false, -1, true);
			LoadImage("GUI\\keyBg.png", 1, false, false, -1, true);
			LoadImage("GUI\\keyBgSel.png", 1, false, false, -1, true);
			LoadImage("GUI\\ctGamepad.png", 1, false, false, -1, true);
			LoadImage("GUI\\ctKeyboard.png", 1, false, false, -1, true);
			LoadImage("GUI\\startMenuBg.png", 1, false, false, -1, true);

			//dlg
			LoadImage("GUI\\HUD\\dlgWnd1.png", 1, false, false, -1, true);
			LoadImage("GUI\\HUD\\dlgWnd2.png", 1, false, false, -1, true);
			LoadImage("GUI\\HUD\\dlgWnd3.png", 1, false, false, -1, true);
			LoadImage("GUI\\HUD\\dlgWnd4.png", 1, false, false, -1, true);
			LoadImage("GUI\\HUD\\mWnd1.png", 1, false, false, -1, true);
			LoadImage("GUI\\HUD\\mWnd2.png", 1, false, false, -1, true);
			LoadImage("GUI\\HUD\\mWnd3.png", 1, false, false, -1, true);
			LoadImage("GUI\\HUD\\mWnd4.png", 1, false, false, -1, true);
			LoadImage("GUI\\HUD\\mWnd4.png", 1, false, false, -1, true);
			LoadImage("GUI\\dlgFrame3.png", 1, false, false, -1, true);
			LoadImage("GUI\\dlgFrame4.png", 1, false, false, -1, true);
			LoadImage("GUI\\HUD\\button1.png", 1, false, false, -1, true);
			LoadImage("GUI\\HUD\\button1Sel.png", 1, false, false, -1, true);
			LoadImage("GUI\\HUD\\button2.png", 1, false, false, -1, true);
			LoadImage("GUI\\HUD\\button2Sel.png", 1, false, false, -1, true);
			LoadImage("GUI\\HUD\\button3.png", 1, false, false, -1, true);
			LoadImage("GUI\\HUD\\button3Sel.png", 1, false, false, -1, true);
			LoadImage("GUI\\HUD\\button4.png", 1, false, false, -1, true);
			LoadImage("GUI\\HUD\\button4Sel.png", 1, false, false, -1, true);
			LoadImage("GUI\\HUD\\ibutton1.png", 1, false, false, -1, true);
			LoadImage("GUI\\HUD\\ibutton1Sel.png", 1, false, false, -1, true);

			//finish
			LoadImage("GUI\\playerLeftFrame.png", 1, false, false, -1, true);
			LoadImage("GUI\\playerRightFrame.png", 1, false, false, -1, true);
			LoadImage("GUI\\playerLineFrame.png", 1, false, false, -1, true);
			LoadImage("GUI\\cup1.dds");
			LoadImage("GUI\\cup2.dds");
			LoadImage("GUI\\cup3.dds");

			//ach
			LoadImage("GUI\\achievmentBg.dds");
			LoadImage("GUI\\achievmentBottomPanel.png", 1, false, false, -1, true);
			LoadImage("GUI\\achievmentPanel.png", 1, false, false, -1, true);
			LoadImage("GUI\\closeBut.png", 1, false, false, -1, true);
			LoadImage("GUI\\okBut.png", 1, false, false, -1, true);
			LoadImage("GUI\\okButSel.png", 1, false, false, -1, true);

			//slides
			LoadImage("GUI\\Slides\\slide1.dds");
			LoadImage("GUI\\Slides\\slide2.dds");
			LoadImage("GUI\\Slides\\slide3.dds");
			LoadImage("GUI\\Slides\\slide4.dds");
			LoadImage("GUI\\Slides\\slide5.dds");
			LoadImage("GUI\\Slides\\slide6.dds");
			LoadImage("GUI\\Slides\\slide7.dds");
			LoadImage("GUI\\Slides\\slide8.dds");
			LoadImage("GUI\\Slides\\slide9.dds");

			LoadSpecLibMat("GUI\\question", "GUI\\question.png");
			LoadImage2dLibMat("GUI\\garage1", "GUI\\garage1.dds");
			LoadImage2dLibMat("GUI\\garage2", "GUI\\garage2.dds");
			LoadImage2dLibMat("GUI\\angar1", "GUI\\angar1.dds");
			LoadImage2dLibMat("GUI\\angar2", "GUI\\angar2.dds");
			{
				graph::LibMaterial& libMat = LoadImage2dLibMat("GUI\\space2", "GUI\\space2.dds");
				libMat.material.SetOption(graph::Material::moLighting, false);
				libMat.material.SetOption(graph::Material::moIgnoreFog, true);
			}

			//Шрифты добавлять тут. 
			LoadFont("SmallBold", 24, FW_BOLD, false, "Verdana");
			//
			LoadFont("ARIAL DEVICE", 54, 0, false, "Arial Black");
			//
			LoadFont("Arial Black52", 52, 0, false, "Arial Black");
			//
			LoadFont("Arial Black48", 48, 0, false, "Arial Black");
			//
			LoadFont("Arial Black40", 40, 0, false, "Arial Black");
			//
			LoadFont("Arial Black38", 38, 0, false, "Arial Black");
			//
			LoadFont("SmallPlus", 25, 0, false, "Verdana");
			//
			LoadFont("Header", 44, 0, false, "Verdana");
			//
			LoadFont("HeaderHard", 44, FW_BOLD, false, "Verdana");
			//
			LoadFont("Item", 32, 0, false, "Verdana");
			//
			 LoadFont("ItemHard", 32, FW_BOLD, false, "Verdana");
			//14
			LoadFont("Small", 24, 0, false, "Verdana");
			//12
			LoadFont("VerySmall", 18, FW_BOLD, false, "Verdana");
			//12
			LoadFont("VerySmallThink", 18, 0, false, "Verdana");
		}

		void ResourceManager::LoadCarLibMat(graph::LibMaterial* libMat, graph::Tex2DResource* tex,
		                                    const D3DXCOLOR& color, const std::string& normMap)
		{
			libMat->material.SetSpecPower(80.0f);
			libMat->material.SetSpecular(D3DXCOLOR(1, 1, 1, 1));

			libMat->samplers.Clear();

			graph::Sampler2d& colSampler = libMat->samplers.Add2d(tex);
			colSampler.SetFiltering(graph::BaseSampler::sfAnisotropic);
			colSampler.SetColorMode(graph::Sampler2d::tmDecal);
			colSampler.SetColor(color);

			graph::Sampler2d& difSampler = AddSampler2dTo(*libMat, "");
			difSampler.SetColorMode(graph::Sampler2d::tmLight);

			if (normMap != "")
			{
				graph::Sampler2d& normSampler = AddSampler2dTo(*libMat, normMap);
				normSampler.stageStates.Set(graph::tssColorOp, D3DTOP_DISABLE);
				normSampler.stageStates.Set(graph::tssAlphaOp, D3DTOP_DISABLE);
			}
		}

		void ResourceManager::LoadCarLibMat(graph::LibMaterial* libMat, const std::string& imgName,
		                                    const std::string& normMap)
		{
			LoadCarLibMat(libMat, GetImageLib().Get(imgName).GetOrCreateTex2d(), clrWhite, normMap);
		}

		graph::LibMaterial& ResourceManager::LoadCarLibMat(const std::string& name, const std::string& imgName,
		                                                   const std::string& normMap)
		{
			graph::LibMaterial& libMat = GetMatLib().Add();
			libMat.SetName(name);

			LoadCarLibMat(&libMat, imgName, normMap);

			return libMat;
		}

		void ResourceManager::LoadMusic()
		{
			LoadSound("Music\\Track1.ogg");
			LoadSound("Music\\Track2.ogg");
			LoadSound("Music\\Track3.ogg");
			LoadSound("Music\\Track4.ogg");
			LoadSound("Music\\Track5.ogg");
			LoadSound("Music\\Track6.ogg");
			LoadSound("Music\\Track7.ogg");
			LoadSound("Music\\Track8.ogg");
			LoadSound("Music\\Track9.ogg");
			LoadSound("Music\\Track10.ogg");
			LoadSound("Music\\Track11.ogg");
			LoadSound("Music\\Track12.ogg");
			LoadSound("Music\\Track13.ogg");
			LoadSound("Music\\Track14.ogg");
			LoadSound("Music\\Track15.ogg");
			LoadSound("Music\\TrackFinal.ogg");
		}

		void ResourceManager::LoadSounds()
		{
			LSL_LOG("resourceManager load sounds");

			LoadSound("Sounds\\light_impact01.ogg", 1.0f, 0.0f, true);
			LoadSound("Sounds\\light_impact02.ogg", 1.0f, 0.0f, true);
			LoadSound("Sounds\\light_impact03.ogg", 1.0f, 0.0f, true);
			LoadSound("Sounds\\light_impact04.ogg", 1.0f, 0.0f, true);
			LoadSound("Sounds\\light_impact05.ogg", 1.0f, 0.0f, true);

			LoadSound("Sounds\\SkidAsphalt.ogg", 1.0f, 0.0f, true);
			LoadSound("Sounds\\OilDestr.ogg", 2.5f, 0.0f, true);
			LoadSound("Sounds\\Recover.ogg", 2.5f, 0.0f, true);

			LoadSound("Sounds\\engine_player_heavy_mot.ogg", 1.0f, 0.0f, true);
			LoadSound("Sounds\\engine_player_heavy_tom.ogg", 1.0f, 0.0f, true);
			LoadSound("Sounds\\Motor_high02.ogg", 1.0f, 0.0f, true);
			LoadSound("Sounds\\podushka_move.ogg", 1.0f, 0.0f, true);
			LoadSound("Sounds\\podushka_stop.ogg", 1.0f, 0.0f, true);
			LoadSound("Sounds\\monstertruckstop.ogg", 1.0f, 0.0f, true);
			LoadSound("Sounds\\guseniza_stop.ogg", 1.0f, 0.0f, true);
			LoadSound("Sounds\\guseniza_move.ogg", 1.0f, 0.0f, true);
			LoadSound("Sounds\\fazowij_izluchatel.ogg", 1.0f, 0.0f, true);
			LoadSound("Sounds\\spherePulseDeath.ogg", 1.0f, 0.0f, true);
			LoadSound("Sounds\\gun_podushkat.ogg", 1.0f, 0.0f, true);
			LoadSound("Sounds\\laserGuseniza.ogg", 1.0f, 0.0f, true);
			LoadSound("Sounds\\mortira.ogg", 1.0f, 0.0f, true);
			LoadSound("Sounds\\pulsator.ogg", 1.0f, 0.0f, true);
			LoadSound("Sounds\\turel.ogg", 1.0f, 0.0f, true);
			LoadSound("Sounds\\ultrazwukovoi_blaser.ogg", 1.0f, 0.0f, true);
			LoadSound("Sounds\\frost_ray.ogg", 1.0f, 0.0f, true);

			LoadSound("Sounds\\carcrash05.ogg", 2.0f, 0.0f, true);
			LoadSound("Sounds\\cluster_rocket.ogg", 1.0f, 0.0f, true);
			LoadSound("Sounds\\missile_launch.ogg", 2.0f, 0.0f, true);
			LoadSound("Sounds\\shredder.ogg", 2.0f, 0.0f, true);
			LoadSound("Sounds\\icon_rail.ogg", 1.0f, 0.0f, true);
			LoadSound("Sounds\\shock.ogg", 2.0f, 0.0f, true);
			LoadSound("Sounds\\phalanx_shot_a.ogg", 1.0f, 0.0f, true);
			LoadSound("Sounds\\klicka5.ogg", 3.5f, 0.0f, true);
			LoadSound("Sounds\\burn.ogg", 4.5f, 0.0f, true);
			LoadSound("Sounds\\Cash.ogg", 2.5f, 0.0f, true);
			LoadSound("Sounds\\hidecar.ogg", 2.5f, 0.0f, true);
			LoadSound("Sounds\\Shell.ogg", 4.0f, 0.0f, true);
			LoadSound("Sounds\\Reload.ogg", 3.5f, 0.0f, true);
			LoadSound("Sounds\\exhaust_b_heavy.ogg", 2.0f, 0.0f, true);
			LoadSound("Sounds\\fireGun.ogg", 2.0f, 0.0f, true);
			LoadSound("Sounds\\sonar.ogg", 1.0f, 0.0f, true);
			LoadSound("Sounds\\shieldOn.ogg", 2.0f, 0.0f, true);
			LoadSound("Sounds\\rageOn.ogg", 4.0f, 0.0f, true);
			//LoadSound("Sounds\\bullets_hp_01.ogg", 2.0f, 0.0f, true);
			//LoadSound("Sounds\\bullets_hp_02.ogg", 2.0f, 0.0f, true);
			//LoadSound("Sounds\\bullets_hp_03.ogg", 2.0f, 0.0f, true);
			LoadSound("Sounds\\airBladeRocket.ogg", 1.0f, 0.0f, true);
			LoadSound("Sounds\\maslo.ogg", 1.0f, 0.0f, true);
			LoadSound("Sounds\\rezonator.ogg", 1.0f, 0.0f, true);

			LoadSound("Sounds\\UI\\changeOption.ogg", 1.0f, 0.0f, true);
			LoadSound("Sounds\\UI\\click.ogg", 1.0f, 0.0f, true);
			LoadSound("Sounds\\UI\\navedenie.ogg", 1.0f, 0.0f, true);
			LoadSound("Sounds\\UI\\warning.ogg", 1.0f, 0.0f, true);
			LoadSound("Sounds\\UI\\pickup_down.ogg", 1.0f, 0.0f, true);
			LoadSound("Sounds\\UI\\pickup_up.ogg", 1.0f, 0.0f, true);
			LoadSound("Sounds\\UI\\repaint.ogg", 1.0f, 0.0f, true);
			LoadSound("Sounds\\UI\\acception.ogg", 1.0f, 0.0f, true);
			LoadSound("Sounds\\UI\\showPlanet.ogg", 1.0f, 0.0f, true);
			LoadSound("Sounds\\UI\\getSponsor.ogg", 2.5f, 0.0f, true);
		}

		void ResourceManager::LoadCommentator(const CommentatorStyle& style)
		{
			LoadSound("finish1.ogg", "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);
			LoadSound("lowLuck1.ogg", "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);
			LoadSound("playerMoveInverse1.ogg", "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);
			LoadSound("overboard1.ogg", "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);
			LoadSound("death1.ogg", "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);
			LoadSound("thirdChanged1.ogg", "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);

			for (int i = 1; i <= 4; ++i)
				LoadSound(StrFmt("start%d.ogg", i), "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);
			for (int i = 1; i <= 7; ++i)
				LoadSound(StrFmt("lastLap%d.ogg", i), "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);
			for (int i = 1; i <= 7; ++i)
				LoadSound(StrFmt("lowLife%d.ogg", i), "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);
			for (int i = 1; i <= 3; ++i)
				LoadSound(StrFmt("playerLostControl%d.ogg", i), "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);
			for (int i = 1; i <= 6; ++i)
				LoadSound(StrFmt("leaderChanged%d.ogg", i), "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);
			for (int i = 1; i <= 6; ++i)
				LoadSound(StrFmt("leaderFinish%d.ogg", i), "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);
			for (int i = 1; i <= 3; ++i)
				LoadSound(StrFmt("secondFinish%d.ogg", i), "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);
			for (int i = 1; i <= 4; ++i)
				LoadSound(StrFmt("thirdFinish%d.ogg", i), "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);
			for (int i = 1; i <= 5; ++i)
				LoadSound(StrFmt("lastFinish%d.ogg", i), "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);
			for (int i = 1; i <= 3; ++i)
				LoadSound(StrFmt("playerKill%d.ogg", i), "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);
			for (int i = 1; i <= 7; ++i)
				LoadSound(StrFmt("finishFirst%d.ogg", i), "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);
			for (int i = 1; i <= 4; ++i)
				LoadSound(StrFmt("finishSecond%d.ogg", i), "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);
			for (int i = 1; i <= 6; ++i)
				LoadSound(StrFmt("finishLast%d.ogg", i), "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);
			for (int i = 1; i <= 10; ++i)
				LoadSound(StrFmt("lastFar%d.ogg", i), "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);
			for (int i = 1; i <= 5; ++i)
				LoadSound(StrFmt("domination%d.ogg", i), "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);
			for (int i = 1; i <= 5; ++i)
				LoadSound(StrFmt("finishThird%d.ogg", i), "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);
			for (int i = 1; i <= 3; ++i)
				LoadSound(StrFmt("accel%d.ogg", i), "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);

			LoadSound("rip.ogg", "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);
			LoadSound("snake.ogg", "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);
			LoadSound("tailer.ogg", "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);
			LoadSound("tarkvin.ogg", "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);
			LoadSound("gank.ogg", "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);
			LoadSound("jarry.ogg", "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);
			LoadSound("kristoph.ogg", "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);
			LoadSound("mardock.ogg", "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);
			LoadSound("shred.ogg", "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);
			LoadSound("stinkle.ogg", "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);
			LoadSound("violetta.ogg", "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);
			LoadSound("viper.ogg", "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);
			LoadSound("butcher.ogg", "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);
			LoadSound("kjin.ogg", "Voice\\", style.name + "\\", 1.0f, 0.0f, true, true);
		}

		void ResourceManager::LoadWorld(int worldType)
		{
			if (_worldType != worldType)
			{
				for (auto iter = _meshLib->begin(); iter != _meshLib->end(); ++iter)
				{
					int tag = (*iter)->GetTag() & MeshLib::cTagIndexMask;
					int masked = (*iter)->GetTag() & (~MeshLib::cTagIndexMask);

					if (tag == _worldType)
					{
						(*iter)->Unload();
					}
					else if (tag == worldType)
					{
						bool loadData = ((*iter)->GetTag() & MeshLib::cTagLoadData) != 0;
						bool initIVB = ((*iter)->GetTag() & MeshLib::cTagInitIVB) != 0;

						(*iter)->Load(loadData, initIVB);
					}
				}

				for (auto iter = _imageLib->begin(); iter != _imageLib->end(); ++iter)
				{
					int tag = (*iter)->GetTag() & ImageLib::cTagIndexMask;
					int masked = (*iter)->GetTag() & (~ImageLib::cTagIndexMask);

					if (tag == _worldType)
					{
						(*iter)->Unload();
					}
					else if (tag == worldType)
					{
						bool initTex2d = ((*iter)->GetTag() & ImageLib::cTagInitTex2d) != 0;
						bool initCubeTex = ((*iter)->GetTag() & ImageLib::cTagInitCubeTex) != 0;

						(*iter)->Load(initTex2d, initCubeTex);
					}
				}

				_worldType = worldType;
			}
		}

		void ResourceManager::Load()
		{
			LoadEffects();
			LoadWorld1();
			LoadWorld2();
			LoadWorld3();
			LoadWorld4();
			LoadWorld5();
			LoadWorld6();
			LoadCrush();
			LoadCars();
			LoadBonus();
			LoadWeapons();
			LoadUpgrades();
			LoadGUI();
			LoadSounds();
		}

		LangCharset ResourceManager::GetFontCharset() const
		{
			return _fontCharset;
		}

		void ResourceManager::SetFontCharset(LangCharset value)
		{
			if (_fontCharset != value)
			{
				_fontCharset = value;

				for (TextFontLib::const_iterator iter = _textFontLib->begin(); iter != _textFontLib->end(); ++iter)
				{
					graph::TextFont::Desc desc = (*iter)->GetDesc();
					desc.charSet = cLangCharsetCode[value];
					(*iter)->SetDesc(desc);
				}
			}
		}

		ResourceManager::MeshLib& ResourceManager::GetMeshLib()
		{
			return *_meshLib;
		}

		ResourceManager::ImageLib& ResourceManager::GetImageLib()
		{
			return *_imageLib;
		}

		ResourceManager::ShaderLib& ResourceManager::GetShaderLib()
		{
			return *_shaderLib;
		}

		ResourceManager::MatLib& ResourceManager::GetMatLib()
		{
			return *_matLib;
		}

		ResourceManager::TextFontLib& ResourceManager::GetTextFontLib()
		{
			return *_textFontLib;
		}

		ResourceManager::StringLib& ResourceManager::GetStringLib()
		{
			return *_stringLib;
		}

		ResourceManager::SoundLib& ResourceManager::GetSoundLib()
		{
			return *_soundLib;
		}
	}
}
