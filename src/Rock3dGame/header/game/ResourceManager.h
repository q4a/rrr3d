#pragma once

#include "snd\\Audio.h"

namespace r3d
{

namespace game
{

class ResourceManager;

//Неполучается использовать ResourceCollection поскольку px::TriangleMesh не является Resource
template<class _Resource, class _ComplexLib> class ComplexResCollection: public ComCollection<_Resource, void, void, void>
{
private:
	typedef ComCollection<_Resource, void, void, void> _MyBase;
private:
	_ComplexLib* _complexLib;
public:
	ComplexResCollection(_ComplexLib* complexLib): _complexLib(complexLib) {}

	virtual Item* Find(const std::string& name)
	{
		Item* res = _MyBase::Find(name);
		if (res == 0)
			return lsl::StaticCast<_Resource*>(_complexLib->ComplexFind(this, name));

		return res;
	}
	virtual bool ValidateName(const std::string& name)
	{
		//Проверка осуществляется на более высоком уровне ComplexLib
		return true;
	}
};

class ComplexMeshLib;

class ComplexMesh
{
private:
	ComplexMeshLib* _owner;
	int _tag;
	std::string _name;
	std::string _fileName;
	bool _enableTBN;

	res::MeshData* _mesh;
	graph::IndexedVBMesh* _ivbMesh;
	graph::MeshX* _meshX;
	px::TriangleMesh* _pxMesh;
public:
	ComplexMesh(ComplexMeshLib* owner);
	~ComplexMesh();

	void Load(bool loadData, bool initIVB);
	void Unload();

	//mesh
	res::MeshData* GetOrCreateMesh();
	void ReleaseMesh();

	//ivbMesh
	graph::IndexedVBMesh* GetOrCreateIVBMesh();
	void ReleaseIVBMesh();

	//meshX
	graph::MeshX* GetOrCreateMeshX();
	void ReleaseMeshX();

	//pxMesh
	px::TriangleMesh* GetOrCreatePxMesh();
	void ReleasePxMesh();

	//release all
	void ReleaseAll();

	//Перезагружает ресурсы если они уже инициализированы (т.е. используются)
	void Reload();

	int GetTag() const;
	void SetTag(int value);

	//resource name
	const std::string& GetName() const;
	void SetName(const std::string& value);

	//resource file name
	const std::string& GetFileName() const;
	void SetFileName(const std::string& value);

	//
	bool GetEnableTBN() const;
	void SetEnableTBN(bool value);
};

class ComplexMeshLib: public lsl::Collection<ComplexMesh, void, ComplexMeshLib*, ComplexMeshLib*>, public lsl::Component
{
	friend ComplexMesh;
private:
	typedef lsl::Collection<ComplexMesh, void, ComplexMeshLib*, ComplexMeshLib*> _MyBase;

	typedef ComplexResCollection<res::MeshData, ComplexMeshLib> _MeshLib;
	typedef ComplexResCollection<graph::IndexedVBMesh, ComplexMeshLib> _IVBMeshLib;
	typedef ComplexResCollection<graph::MeshX, ComplexMeshLib> _MeshXLib;
	typedef ComplexResCollection<px::TriangleMesh, ComplexMeshLib> _PxMeshLib;
	typedef lsl::List<ComplexMesh*> MeshList;
public:
	static const int cTagLoadData = 1 << 8;
	static const int cTagInitIVB = 1 << 9;
	static const int cTagCount = 10;
	static const int cTagIndexMask = 0xFF;
private:
	graph::Engine* _engine;
	MeshList _tagList[cTagCount];

	_MeshLib* _meshLib;
	_IVBMeshLib* _ivbMeshLib;
	_MeshXLib* _meshXLib;
	_PxMeshLib* _pxMeshLib;

	void TagChanged(const Value& value, int prevVal, int newVal);
protected:
	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
public:
	ComplexMeshLib(graph::Engine* engine);
	virtual ~ComplexMeshLib();

	lsl::CollectionItem* ComplexFind(CollectionTraits* traits, const std::string& name);
	ComplexMesh* Find(const std::string& name);
	ComplexMesh& Get(const std::string& name);
	const MeshList& GetMeshList(int tag);

	ComplexMesh& LoadMesh(const std::string& name, bool buildTBN = false, bool loadData = false, bool initIVB = false, int tag = -1);

	graph::Engine* GetEngine();
};

class ComplexImageLib;

class ComplexImage
{
private:
	ComplexImageLib* _owner;
	int _tag;
	std::string _name;
	std::string _fileName;
	unsigned _levelCnt;
	bool _gui;

	res::ImageResource* _image;
	graph::Tex2DResource* _tex2d;

	res::CubeImageResource* _cubeImage;
	graph::TexCubeResource* _cubeTex;
public:
	ComplexImage(ComplexImageLib* owner);
	~ComplexImage();

	void Load(bool initTex2d, bool initCubeTex);
	void Unload();

	res::ImageResource* GetOrCreateImage();
	void ReleaseImage();

	graph::Tex2DResource* GetOrCreateTex2d();
	void ReleaseTex2d();

	res::CubeImageResource* GetOrCreateCubeImage();
	void ReleaseCubeImage();

	graph::TexCubeResource* GetOrCreateCubeTex();
	void ReleaseCubeTex();

	void ReleaseAll();
	//Перезагружает ресурсы если они уже инициализированы (т.е. используются)
	void Reload();

	int GetTag() const;
	void SetTag(int value);

	//resource name
	const std::string& GetName() const;
	void SetName(const std::string& value);

	//resource file name
	const std::string& GetFileName() const;
	void SetFileName(const std::string& value);

	//value = 0 - генерация последовательности mip уровней до 1x1
	unsigned GetLevelCnt() const;	
	void SetLevelCnt(unsigned value);

	bool GetGUI() const;	
	void SetGUI(bool value);
};

class ComplexImageLib: public lsl::Collection<ComplexImage, void, ComplexImageLib*, ComplexImageLib*>, public lsl::Component
{
	friend ComplexImage;
private:
	typedef lsl::Collection<ComplexImage, void, ComplexImageLib*, ComplexImageLib*> _MyBase;

	typedef ComplexResCollection<res::ImageResource, ComplexImageLib> _ImageLib;
	typedef ComplexResCollection<graph::Tex2DResource, ComplexImageLib> _Tex2dLib;
	typedef ComplexResCollection<res::CubeImageResource, ComplexImageLib> _CubeImageLib;
	typedef ComplexResCollection<graph::TexCubeResource, ComplexImageLib> _CubeTexLib;
	typedef lsl::List<ComplexImage*> ImageList;
public:
	static const int cTagInitTex2d = 1 << 8;
	static const int cTagInitCubeTex = 1 << 9;
	static const int cTagCount = 10;
	static const int cTagIndexMask = 0xFF;
private:
	graph::Engine* _engine;
	ImageList _tagList[cTagCount];

	_ImageLib* _imageLib;
	_Tex2dLib* _tex2dLib;
	_CubeImageLib* _cubeImageLib;
	_CubeTexLib* _cubeTexLib;

	void TagChanged(const Value& value, int prevVal, int newVal);
protected:
	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
public:
	ComplexImageLib(graph::Engine* engine);
	virtual ~ComplexImageLib();

	lsl::CollectionItem* ComplexFind(CollectionTraits* traits, const std::string& name);
	ComplexImage* Find(const std::string& name);
	ComplexImage& Get(const std::string& name);

	ComplexImage& LoadImage(const std::string& name, unsigned levelCnt, bool initTex2d = false, bool initCubeTex = false, int tag = -1, bool gui = false);

	graph::Engine* GetEngine();
};

class ComplexMatLib: public graph::MaterialLibrary
{
private:
	typedef graph::MaterialLibrary _MyBase;
private:
	ResourceManager* _resManager;
public:
	ComplexMatLib(ResourceManager* resManager);

	graph::LibMaterial& Get(const std::string& name);

	graph::LibMaterial& LoadLibMat(const std::string& name, bool sprite = false, graph::Material::Blending blending = graph::Material::bmOpaque, const FloatRange& alpha = 1.0f, const ColorRange& color = clrWhite, const ColorRange& ambient = clrWhite);
	void SetAlphaTestTo(graph::LibMaterial& libMat, graph::Material::AlphaTest alphaTest, const FloatRange& alphaRef);
	graph::Sampler2d& AddSampler2dTo(graph::LibMaterial& libMat, const std::string& imgName, graph::Sampler2d::Mode alphaMode = graph::Sampler2d::tmDefault, graph::BaseSampler::Filtering filtering = graph::BaseSampler::sfLinear);
};

class ComplexTextFontLibrary: public lsl::ComCollection<graph::TextFont, void, void, void>
{
	typedef lsl::ComCollection<graph::TextFont, void, void, void> _MyBase;
private:
	ResourceManager* _resManager;
public:
	ComplexTextFontLibrary(ResourceManager* resManager);

	graph::TextFont& Get(const std::string& name);

	graph::TextFont& LoadFont(const std::string& name, int height, unsigned weight, bool italic, DWORD charset, const std::string& faceName);
};

enum StringValue 
{
	svNull = 0,

	svNewGame,
	svSingleGame,
	svNetGame,
	svContinue, 
	svNetCreate,
	svConnect,
	svRefresh,
	svLoad, 
	svOptions,
	svControls, 
	svGame,
	svNetwork,
	svAuthors, 
	svExit,
	svProfile,

	svGraphic, 
	svResolution,
	svFiltering,
	svMultisampling,
	svShadow, 
	svEnv, 
	svLight, 
	svPostProcess,
	svFixedFrameRate,
	svSound, 
	svMusic, 
	svSoundFX, 
	svSoundDicter, 
	svLanguage,
	svCommentator,
	svCamera,
	svMaxPlayers,
	svMaxComputers,
	svLapsCount,
	svEnableHUD,
	svEnableMineBug,
	svUpgradeMaxLevel,
	svWeaponMaxLevel,
	svSpringBorders,
	svDifficulty,
	svStartOptionsInfo,
	svSelectItem,
	svEnterIP,

	svChampionship, 
	svSkirmish,

	svOnePlayer,
	svTwoPlayer,

	svStartRace,
	svReadyRace,
	svCancelReadyRace,
	svWorkshop,
	svGarage,
	svCasePlanet,
	svHostLabel,

	svBossName,
	svPassing,
	svTournament,
	svWeapons,
	svPlayer,

	svInfoPlayer,
	svInfoPlayerText,
	svInfoRace,
	svInfoRaceText,
	svCarCost,
	svPassInfo,
	svTournamentInfo,
	svLockedCarName,
	svLockedCarInfo,

	svApply,
	svOk, 
	svCancel, 
	svBack,
	svYes,
	svNo,
	svBuy,
	svWarning,

	svLow,
	svMiddle,
	svHigh,
	svOn,
	svOff,

	svRussian,
	svEnglish,

	svCameraSecView,
	svCameraOrtho,

	scWheaterFair,
	scWheaterNight,
	scWheaterClody,
	scWheaterRainy,
	scWheaterMapEdit,

	scEarth,
	scPatagonis,
	scIntaria,
	scHemi5,
	scInferno,
	scNho,
	//
	scEarthInfo,
	scPatagonisInfo,
	scIntariaInfo,
	scHemi5Info,
	scInfernoInfo,
	scNhoInfo,

	scMarauder,
	scBuggi,
	scDirtdevil,
	scTankchetti,
	scManticora,
	scAirblade,
	scGuseniza,
	scPodushka,
	scMonsterTruck,
	scDevilDriver,
	scMustang,
	//
	scMarauderInfo,
	scBuggiInfo,
	scDirtdevilInfo,
	scTankchettiInfo,
	scManticoraInfo,
	scAirbladeInfo,
	scGusenizaInfo,
	scPodushkaInfo,
	scMonsterTruckInfo,
	scDevilDriverInfo,
	scMustangInfo,

	scEngine1,
	scEngine2,
	scEngine3,
	scWheel1,
	scWheel2,
	scWheel3,
	scArmor1,
	scArmor2,
	scArmor3,
	scArmor4,
	scTruba1,
	scTruba2,
	scTruba3,
	scGuseniza1,
	scGuseniza2,
	scGuseniza3,
	//
	scEngine1Info,
	scEngine2Info,
	scEngine3Info,
	scWheel1Info,
	scWheel2Info,
	scWheel3Info,
	scArmor1Info,
	scArmor2Info,
	scArmor3Info,
	scArmor4Info,
	scTruba1Info,
	scTruba2Info,
	scTruba3Info,
	scGuseniza1Info,
	scGuseniza2Info,
	scGuseniza3Info,

	scBulletGun,
	scRifle,
	scAirWeapon,
	scBlaster,
	scTankLaser,
	scSonar,
	scRocket,
	scPulsator,
	scFireGun,
	scDrobilka,
	scSphereGun,
	scTorpeda,
	scMaslo,
	scMineSpike,
	scMineRip,
	scHyper,
	scTurel,
	scMortira,
	scPhaseImpulse,
	scHyperBlaster,
	scMinePropton,
	scSpring,
	scAsyncFrost,

	scBulletGunInfo,
	scRifleInfo,
	scAirWeaponInfo,
	scBlasterInfo,
	scTankLaserInfo,
	scSonarInfo,
	scRocketInfo,
	scPulsatorInfo,
	scFireGunInfo,
	scDrobilkaInfo,
	scSphereGunInfo,
	scTorpedaInfo,
	scMasloInfo,
	scMineSpikeInfo,
	scMineRipInfo,
	scHyperInfo,
	scTurelInfo,
	scMortiraInfo,
	scPhaseImpulseInfo,
	scHyperBlasterInfo,
	scMineProptonInfo,
	scSpringInfo,
	scAsyncFrostInfo,

	scTyler,
	scSnake,
	scTarquin,
	scRip,
	scButcher,
	scGank,
	scGerry,
	scKJin,
	scMardock,
	scStinkle,	
	scViolet,
	scViper,

	scTylerInfo,
	scSnakeInfo,
	scTarquinInfo,
	scRipInfo,
	scButcherInfo,
	scGankInfo,
	scGerryInfo,
	scKJinInfo,
	scMardockInfo,
	scStinkleInfo,	
	scVioletInfo,
	scViperInfo,

	svOpen,
	svClosed,
	svUnavailable,
	svUnavailableTitulA,
	svCompleted,
	svRequestPoints,
	svPrice,
	svMoney,
	svPoints,
	svYourMoneys,
	svLap,
	svRewards,

	svHintCantStart,
	svHintCantContinue,
	svHintCantNewGame,
	svHintCantFlyPlanet,
	svHintCantMoney,
	svHintCantPoints,
	svHintNeedReload,
	svHintExitRace,
	svHintUpgrade,
	svHintWeaponNotSupport,
	svHintDisconnect,
	svCriticalNetError,
	svHintPlayersIsNotReady,
	svHintYouCanFlyPlanet,
	svHintYouCanFlyPlanetClient,
	svHintYouCompletePass,
	svHintYouNotCompletePass,
	svHintConnecting,
	svHintRefreshing,
	svHintHostListEmpty,
	svHintHostConnectionFailed,
	svHintSetColorFailed,
	svHintSetGamerFailed,
	svHintPleaseWait,
	svHintLeaversWillBeRemoved,

	svBuyWeapon,
	svSellWeapon,
	svBuyCar,
	svYouReadyFlyPlanet,
	svYouReadyStayPlanet,
	svBuyReward,

	svCredits,

	cStringValueEnd
};

typedef std::vector<StringValue> StringValList;
extern const std::string cStringValueStr[cStringValueEnd];
static const std::string scNull = cStringValueStr[svNull];
inline const std::string& _SC(StringValue id) {return cStringValueStr[id];}

enum LangCharset {lcDefault, lcEastEurope, lcRussian, lcBaltic, cLangCharsetEnd};
extern const std::string cLangCharsetStr[cLangCharsetEnd];
extern const DWORD cLangCharsetCode[cLangCharsetEnd];

struct Language
{
	lsl::string name;
	lsl::string file;
	lsl::string locale;
	LangCharset charset;
	int primId;
};

typedef lsl::Vector<Language> Languages;

struct CommentatorStyle
{
	lsl::string name;
};

typedef lsl::Vector<CommentatorStyle> CommentatorStyles;

class StringLibrary: public lsl::Component
{
public:
	static const wchar_t cInfinite = wchar_t(0x221E);

	typedef std::map<std::string, std::string> StringMap;
private:
	ResourceManager* _resManager;
	StringMap _stringMap;

	void Save(std::ostream& stream);
	void Load(std::istream& stream);
public:
	StringLibrary(ResourceManager* resManager);

	void SaveToFile(const lsl::string& file);
	void LoadFromFile(const lsl::string& file);	

	const std::string& Get(const std::string& id) const;
	void Set(const std::string& id, const lsl::string& value);
	bool Has(const std::string& id) const;

	const std::string& Get(StringValue value) const;
	void Set(StringValue id, const lsl::string& value);
	bool Has(StringValue value) const;
};

class ResourceManager: public lsl::Component
{
public:
	enum ShaderType {stReflection = 0, stBumMap, cShaderTypeEnd, cShaderTypeForce = 1000};

	typedef ComplexMeshLib MeshLib;
	typedef ComplexImageLib ImageLib;
	typedef lsl::ResourceCollection<graph::Shader, ShaderType, void, void> ShaderLib;
	typedef ComplexMatLib MatLib;
	typedef ComplexTextFontLibrary TextFontLib;
	typedef StringLibrary StringLib;
	typedef snd::SoundLib SoundLib;

	static const unsigned cGenMipLevel;
	static const float cAlphaTestRef;
private:
	World* _world;

	MeshLib* _meshLib;
	ImageLib* _imageLib;
	ShaderLib* _shaderLib;
	ComplexMatLib* _matLib;
	TextFontLib* _textFontLib;
	StringLib* _stringLib;
	SoundLib* _soundLib;

	LangCharset _fontCharset;
	int _worldType;

	ComplexMesh& LoadMesh(const std::string& name, bool buildTBN = false, bool loadData = false, bool initIVB = false, int tag = -1);
	ComplexImage& LoadImage(const std::string& name, unsigned levelCnt = 1, bool initTex2d = false, bool initCubeTex = false, int tag = -1, bool gui = false);

	graph::Sampler2d& AddSampler2dTo(graph::LibMaterial& libMat, const std::string& imgName, graph::Sampler2d::Mode alphaMode = graph::Sampler2d::tmDefault, graph::BaseSampler::Filtering filtering = graph::BaseSampler::sfLinear);
	void BuildAnimByOff(graph::Sampler2d& sampler, const Vec2Range& texCoord, const Point2U& tileCnt);

	graph::LibMaterial& LoadLibMat(const std::string& name, bool sprite = false, graph::Material::Blending blending = graph::Material::bmOpaque, const FloatRange& alpha = 1.0f, const ColorRange& color = clrWhite, const ColorRange& ambient = clrWhite);
	graph::LibMaterial& LoadImage2dLibMat(const std::string& name, const std::string& imgName, bool sprite = false, graph::Material::Blending blending = graph::Material::bmOpaque, const FloatRange& alpha = 1.0f, const ColorRange& color = clrWhite, const Vec2Range& texCoord = Vec2Range(NullVec2, IdentityVec2), const Point2U& tileCnt = Point2U(1, 1));
	graph::LibMaterial& LoadImage2dLibMatAnim(const std::string& name, const std::string& imgName, bool sprite = false, graph::Material::Blending blending = graph::Material::bmOpaque, const FloatRange& alpha = 1.0f, const ColorRange& color = clrWhite, const Vec3Range& texOffset = NullVector, const Vec3Range& texScale = IdentityVector, const QuatRange& texRot = NullQuaternion);
	graph::LibMaterial& LoadAlphaTestLibMat(const std::string& name, const std::string& imgName, bool sprite = false, float alphaRef = cAlphaTestRef);
	graph::LibMaterial& LoadSpecLibMat(const std::string& name, const std::string& imgName, bool sprite = false);
	graph::LibMaterial& LoadBumpLibMat(const std::string& name, const std::string& imgName, const std::string& normMap, bool sprite = false, graph::Material::Blending blending = graph::Material::bmOpaque, float alpha = 1.0f);	
	graph::LibMaterial& LoadAnimImage2dLibMat(const std::string& name, const std::string& imgName, const Vec3Range& offset = NullVector, const Vec3Range& scale = IdentityVector, const QuatRange& rot = NullQuaternion, bool sprite = false, graph::Material::Blending blending = graph::Material::bmOpaque, const FloatRange& alpha = 1.0f, const ColorRange& color = clrWhite);

	graph::TextFont& LoadFont(const std::string& name, int height, unsigned weight, bool italic, const std::string& faceName);

	snd::Sound* LoadSound(const std::string& name, const std::string& pathRoot, const std::string& path, float volume = 1.0f, float distScaller = 0.0f, bool load = false, bool loadCheckExists = false);
	snd::Sound* LoadSound(const std::string& name, float volume = 1.0f, float distScaller = 0.0f, bool load = false, bool loadCheckExists = false);

	void LoadEffects();
	void LoadWorld1();
	void LoadWorld2();
	void LoadWorld3();
	void LoadWorld4();
	void LoadWorld5();
	void LoadWorld6();
	void LoadCrush();
	void LoadCars();
	void LoadBonus();
	void LoadWeapons();
	void LoadUpgrades();
	void LoadGUI();	
public:
	ResourceManager(World* world);
	virtual ~ResourceManager();

	void LoadCarLibMat(graph::LibMaterial* libMat, graph::Tex2DResource* tex, const D3DXCOLOR& color, const std::string& normMap = "");
	void LoadCarLibMat(graph::LibMaterial* libMat, const std::string& imgName, const std::string& normMap = "");
	graph::LibMaterial& LoadCarLibMat(const std::string& name, const std::string& imgName, const std::string& normMap = "");
	
	void LoadMusic();
	void LoadSounds();
	void LoadCommentator(const CommentatorStyle& style);
	void LoadWorld(int worldType);
	void Load();

	LangCharset GetFontCharset() const;
	void SetFontCharset(LangCharset value);

	MeshLib& GetMeshLib();
	ImageLib& GetImageLib();
	ShaderLib& GetShaderLib();	
	MatLib& GetMatLib();
	TextFontLib& GetTextFontLib();
	StringLib& GetStringLib();
	SoundLib& GetSoundLib();
};

inline void StrPrint(std::string& str, const std::string& value)
{
	const char* strLabel = "%s";

	unsigned ind = str.size();
	ind = str.find(strLabel);
	if (ind < str.size())
		str.replace(str.begin() + ind, str.begin() + ind + 2, value);
};

inline void StrPrint(std::string& str, int value)
{
	std::stringstream sstream;
	sstream << value;

	return StrPrint(str, sstream.str());
}

inline void StrPrint(std::string& str, float value)
{
	std::stringstream sstream;
	sstream << value;

	return StrPrint(str, sstream.str());
}

}

}