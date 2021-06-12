#ifndef R3D_GRAPH_MANAGER
#define R3D_GRAPH_MANAGER

#include "graph\\FogPlane.h"
#include "graph\\ClearSurf.h"
#include "graph\\GrassField.h"
#include "graph\\WaterPlane.h"
#include "graph\\MappingShaders.h"
#include "graph\\ShadowMapRender.h"
#include "graph\\HDREffect.h"
#include "graph\\BloomEffect.h"
#include "graph\\ToneMapping.h"
#include "graph\\DepthMap.h"
#include "graph\\SunShaft.h"
#include "graph\\SkyBox.h"
#include "graph\\RenderToTexture.h"

#include "graph\\StdNode.h"
#include "graph\\OctreeRender.h"
#include "graph\\Actor.h"
#include "graph\\ActorManager.h"

#include "gui\\GUI.h"

namespace r3d
{

struct Rect
{
	int left;
	int top;
	int right;
	int bottom;

	Rect();
	Rect(int mLeft, int mTop, int mRight, int mBottom): left(mLeft), top(mTop), right(mRight), bottom(mBottom) {}

	int Width() const
	{
		return right - left;
	}
	int Height() const
	{
		return bottom - top;
	}
};

class GraphManager: public lsl::Component
{
	friend graph::Actor;
private:
	enum ShaderType {stWater, stSkyBox};
	typedef lsl::ResourceCollection<graph::Shader, ShaderType, void, void> ShaderLib;

	//В порядке рендера
	enum OctreeScene
	{
		osReflWater = 0,
		osViewCubeMap,
		osViewDepth,		

		osShadowCast,
		osShadowMapp,

		//Спец. группы. Порядок не поддерживается. Если такая поддержка поребуется то необходимо устранить подобные группы, а шейдеры передавать в виде параметров менеджеру актеров
		osColorPix,
		osColorRefl,
		osColorBump,
		osColorRefr,
		osColorPlanarRefl,

		//Сортировка по порядку
		osColorDef,
		osColorOpacity,
		osColorCullOpacity,
		osColorEffect,
		osColorLast,

		cOctreeSceneEnd
	};

	static const D3DXVECTOR3 posLights[8];
public:
	typedef std::list<graph::Actor*> ActorList;

	struct LightDesc
	{
		LightDesc(): shadow(false), nearDist(1.0f), farDist(400.0f), shadowNumSplit(4), shadowDisableCropLight(false) {}

		float nearDist;
		float farDist;
		bool shadow;
		int shadowNumSplit;
		bool shadowDisableCropLight;
	};

	class LightSrc: public lsl::Object
	{
		friend class GraphManager;
	private:
		LightDesc _desc;
		bool _enable;

		graph::LightSource* _source;
		graph::ShadowMapRender* _shadowMap;
	protected:
		graph::ShadowMapRender* GetShadowMap();
		void SetShadowMap(graph::ShadowMapRender* value);

		void SetDesc(const LightDesc& desc);
	public:
		LightSrc(const LightDesc& desc);
		virtual ~LightSrc();

		const LightDesc& GetDesc() const;
		graph::LightSource* GetSource();

		bool GetEnable() const;
		void SetEnable(bool value);
	};
	typedef lsl::List<LightSrc*> LightList;

	enum GraphOption {goSkyBox = 0, goWater, goSunShaft, goBloom, goHDR, goShadow, goGrassField, goRefl, goTrueRefl, goPlanarRefl, goPixelLighting, goBumpMap, goRefr, goFog, goPlaneFog, goMagma, cGraphOptionEnd};
	enum GraphQuality {gqLow = 0, gqMiddle, gqHigh, cGraphQualityEnd};

	static const std::string cGraphOptionStr[cGraphOptionEnd];
	static const std::string cGraphQualityStr[cGraphQualityEnd];

	struct OrthoTarget
	{
		D3DXVECTOR3 pos;
		float size;
	};

	struct HDRParams
	{
		HDRParams(): lumKey(4.0f), brightThreshold(4.5f), gaussianScalar(10.0f), exposure(3.0f), colorCorrection(1.0f, 0.0f) {}

		float lumKey;
		float brightThreshold;
		float gaussianScalar;
		float exposure;
		D3DXVECTOR2 colorCorrection;
	};

	enum ReflMappMode { rmColorLayer, rmLackLayer };
private:
	graph::Engine* _engine;
	gui::Manager* _gui;	
	
	bool _graphOptions[cGraphOptionEnd];
	GraphQuality _graphQuality[cGraphOptionEnd];

	std::string _skyTex;
	D3DXCOLOR _fogColor;
	float _fogIntensivity;
	D3DXCOLOR _cloudColor;
	float _cloudIntensivity;
	float _cloudHeight;

	graph::SceneManager* _preNodeScene;
	graph::SceneManager* _nodeScene;
	graph::ActorManager* _actorManager;

	graph::Camera* _camera;
	D3DXVECTOR3 _cubeViewPos;
	OrthoTarget _orthoTarget;
	AABB _groundAABB;
	bool _guiMode;
	//
	LightList _lightList;

	unsigned _clearSurfRef;
	graph::ClearSurf* _clearSurf;

	graph::TexCubeResource* _skyBoxTex;
	graph::SkyBox* _skyBox;
	unsigned _skyBoxRef;

	graph::ReflRender* _reflRender;
	graph::Tex2DResource* _waterColor;
	graph::Tex2DResource* _waterTexNorm;	
	graph::WaterPlane* _waterPlane;
	graph::PlaneNode* _waterNode;
	graph::Actor* _waterPlaneActor;

	graph::GrassField* _grassField;
	graph::Actor* _grassPlane;
	graph::PlaneNode* _grassPlaneNode;
	graph::Tex2DResource* _grassPlTex;
	graph::Tex2DResource* _grassTex;
	graph::LibMaterial* _grassMat;

	graph::DepthStencilSurfaceResource* _depthSurface;
	unsigned _depthSurfaceRef;

	graph::RenderToTexture* _scRenderTex;
	unsigned _scRenderTexRef;

	unsigned _cleanScTexRef;
	graph::RenderToTexture* _cleanScTex;

	graph::RenderToCubeTex* _scRenderCubeTex;
	unsigned _scRenderCubeTexRef;
	
	graph::DepthMapRender* _scDepthMap;
	unsigned _scDepthMapRef;

	unsigned _refrEffRef;
	graph::RefrShader* _refrShader;

	graph::SunShaftRender* _sunShaft;

	graph::ToneMapping* _toneMap;
	unsigned _toneMapRef;

	graph::BloomRender* _bloomEff;

	graph::HDRRender* _hdrEff;
	HDRParams _hdrParams;

	float _shadowMaxFar;
	unsigned _shadowRef;

	graph::LightShader* _pixLightShader;
	unsigned _pixLightRef;

	graph::ReflMappShader* _reflShader;
	graph::ReflBumbMappShader* _reflBumpShader;
	graph::TexCubeResource* _reflTex;
	ReflMappMode _reflMappMode;
	unsigned _reflRef;

	bool _trueRefl;

	graph::ReflRender* _planarReflRender;
	graph::PlanarReflMappShader* _planarReflShader;	

	graph::BumpMapShader* _bumpMapShader;

	unsigned _fogRef;

	graph::FogPlane* _fogPlane;
	graph::Tex2DResource* _cloudsTex;
	graph::LibMaterial* _cloudsMat;
	D3DXVECTOR2 _tileScale;
	graph::Actor* _fogPlaneActor;

	D3DXCOLOR _sceneAmbient;

	unsigned _multisampling;
	graph::RenderTargetResource* _msRT;
	graph::DepthStencilSurfaceResource* _msDS;

	bool _discreteVideoCard;

	void DetectCapabilities();

	void InitClearSurf();
	void FreeClearSurf();

	void InitSkyBox();
	void FreeSkyBox();
	bool IsSkyboxSupported();

	void InitWaterPlane();
	void FreeWaterPlane();
	void UpdateWaterPlane();

	void InitGrassField();
	void FreeGrassField();
	void UpdateGrassPlane();

	void InitDepthSurface();
	void FreeDepthSurface();

	void InitScRenderTex();
	void FreeScRenderTex();

	void InitScRenderCubeTex();
	void FreeScRenderCubeTex();

	void InitCleanScTex();
	void FreeCleanScTex();

	void InitScDepthMap();
	void FreeScDepthMap();

	void InitRefrEff();
	void FreeRefrEff();

	void InitSunShaft();
	void FreeSunShaft();

	void InitToneMap();
	void FreeToneMap();

	void InitBloomEff();
	void FreeBloomEff();

	void InitHDREff();
	void FreeHDREff();
	void ApplyHDRParams();

	void SetLightShadow(LightSrc* light);
	void FreeLightShadow(LightSrc* light);
	void InitShadowMap();
	void FreeShadowMap();

	void InitPixLight();
	void FreePixLight();

	void InitRefl();
	void FreeRefl();
	void RefreshRefl();

	void InitTrueRefl();
	void FreeTrueRefl();

	void InitPlanarRefl();
	void FreePlanarRefl();

	void InitBumpMap();
	void FreeBumpMap();

	void InitFog();
	void FreeFog();

	void InitPlaneFog(lsl::string texture, const D3DXVECTOR2& tileScale, float speed, GraphQuality quality);
	void FreePlaneFog();
	void UpdateFogPlane();

	void ApplyFog();
	void UnApplyFog();

	void ApplyMultisampling();
	void DisposeMultisampling();

	void PrepareActor(graph::Actor* actor, graph::ActorManager::UserDesc& resDesc);
	void UpdateAllActor();

	void InsertActor(graph::Actor* value);
	void RemoveActor(graph::Actor* value);
	void UpdateActor(graph::Actor* value);

	void RenderDebug();

	void OctreeRender(OctreeScene scene, bool ignoreRayUsers, bool planarRefl = false);	
	bool RenderWithShader(OctreeScene scene, bool ignoreRayUsers, graph::Shader* shader, bool checkLight, bool defRender);
	void RenderPlanarReflScene(graph::CameraCI& camera);
	void RenderEnvReflScene(graph::CameraCI& camera);
	void RenderScenes(graph::CameraCI& camera);

	void ProgressTime(float deltaTime, bool pause);
	void AdjustViewOrtho(graph::CameraCI& camera);
	void RenderShadow(graph::CameraCI& camera);
	void RenderCubeMap(graph::CameraCI& camera);
	void RenderWaterRef(graph::CameraCI& camera);	
	void RenderDepthScene(graph::CameraCI& camera);
public:
	GraphManager(HWND window, lsl::Point resolution, bool fullScreen);
	virtual ~GraphManager();

	//Вызывается в главном потоке для внутренних нужд
	void MainThread();
	//Рендер может идти в отдельном потоке
	bool Render(float deltaTime, bool pause);
	//
	void GPUSync();
	bool IsSyncSupported();
	//
	bool Present();

	void Reset(HWND window, lsl::Point resolution, bool fullScreen, unsigned multisamling, bool resetDevice = true);
	void Reset(HWND window, lsl::Point resolution, bool fullScreen, bool resetDevice = true);
	void Reset(unsigned multisamling, bool resetDevice = true);

	void InsertPreNode(graph::BaseSceneNode* value);
	void RemovePreNode(graph::BaseSceneNode* value);

	void InsertScNode(graph::BaseSceneNode* value);
	void RemoveScNode(graph::BaseSceneNode* value);

	LightSrc* AddLight(const LightDesc& desc);
	void DelLight(LightSrc* value);
	void ClearLightList();
	void SetLight(LightSrc* light, const LightDesc& desc);
	const LightList& GetLightList() const;

	void BuildOctree();

	D3DXVECTOR3 ScreenToWorld(const lsl::Point& coord, const float z);
	lsl::Point WorldToScreen(const D3DXVECTOR3& coord);
	void ScreenToRay(const lsl::Point& coord, D3DXVECTOR3& rayStart, D3DXVECTOR3& rayVec);
	bool ScreenPixelRayCastWithPlaneXY(const lsl::Point& coord, D3DXVECTOR3& outVec);

	graph::Engine& GetEngine();
	gui::Manager& GetGUI();
	Rect GetWndRect() const;

	bool GetGraphOption(GraphOption option) const;
	GraphQuality GetGraphQuality(GraphOption option) const;
	void SetGraphOption(GraphOption option, bool value, GraphQuality quality);
	void SetGraphOption(GraphOption option, bool value);

	bool IsGraphOptionSupported(GraphOption option, GraphQuality quality) const;

	unsigned GetFiltering() const;
	void SetFiltering(unsigned value);
	bool IsFilteringSupported(unsigned value) const;

	unsigned GetMultisampling() const;
	void SetMultisampling(unsigned value);	
	bool IsMultisamplingSupported(unsigned value) const;

	//
	const std::string& GetSkyTex() const;
	void SetSkyTex(const std::string& value);
	//
	const D3DXCOLOR& GetFogColor() const;
	void SetFogColor(const D3DXCOLOR& value);
	//
	const D3DXCOLOR& GetSceneAmbient();
	void SetSceneAmbient(const D3DXCOLOR& value);
	//
	float GetFogIntensivity() const;
	void SetFogIntensivity(float value);
	//
	const D3DXCOLOR& GetCloudColor() const;
	void SetCloudColor(const D3DXCOLOR& value);
	//
	float GetCloudIntensivity() const;
	void SetCloudIntensivity(float value);
	//
	float GetCloudHeight() const;
	void SetCloudHeight(float value);

	//Камера
	graph::Camera* GetCamera();
	void SetCamera(graph::Camera* value);
	
	const D3DXVECTOR3& GetCubeViewPos() const;
	void SetCubeViewPos(const D3DXVECTOR3& value);
	//
	const OrthoTarget& GetOrthoTarget() const;
	void SetOrthoTarget(const D3DXVECTOR3& pos, float size);

	const HDRParams& GetHDRParams() const;
	void SetHDRParams(const HDRParams& value);

	ReflMappMode GetReflMappMode() const;
	void SetReflMappMode(ReflMappMode value);

	float GetShadowMaxFar() const;
	void SetShadowMaxFar(float value);

	bool GetGUIMode() const;
	void SetGUIMode(bool value);
	
	graph::DisplayMode GetDisplayMode() const;
	const graph::DisplayModes& GetDisplayModes() const;
	bool FindNearMode(const lsl::Point& resolution, graph::DisplayMode& mode, float aspect = 0) const;

	graph::DisplayMode GetScreenMode() const;

	bool discreteVideoCard() const;
};

}

#endif