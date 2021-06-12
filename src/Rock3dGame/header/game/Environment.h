#pragma once

#include "CameraManager.h"

namespace r3d
{

namespace game
{

class Environment: public GameObjListener
{
public:
	enum Wheater {ewFair = 0, ewNight, ewClody, ewRainy, ewSahara, ewHell, ewSnow, ewGarage, ewAngar, cWheaterEnd};

	enum WorldType {wtWorld1, wtWorld2, wtWorld3, wtWorld4, wtWorld5, wtWorld6, wtGarage, wtAngar};

	enum Quality {eqLow = 0, eqMiddle, eqHigh, cQualityEnd};

	enum Filtering {efLinear = 0, efAnisotropic2, efAnisotropic4, efAnisotropic8, efAnisotropic16, cFilteringEnd};

	enum Multisampling {emNone = 0, emSamples2, emSamples4, emSamples8, emSamples16, cMultisamplingEnd};

	enum SyncFrameRate {sfrNone = 0, sfrFixed, cSyncFrameRateEnd};

	static const std::string cWheaterStr[cWheaterEnd];
	static const lsl::string cSyncFrameRateStr[cSyncFrameRateEnd];

	static GraphManager::GraphQuality cGraphQuality[cQualityEnd];
	static int cFilteringLevel[cFilteringEnd];
	static int cMultisamplingLevel[cMultisamplingEnd];

	static std::pair<GraphManager::GraphOption, Quality> cShadowGraphMap[];
	static std::pair<GraphManager::GraphOption, Quality> cLightGraphMap[];
	static std::pair<GraphManager::GraphOption, Quality> cPostEffGraphMap[];
	static std::pair<GraphManager::GraphOption, Quality> cEnvGraphMap[];
private:
	World* _world;
	
	Wheater _wheater;
	WorldType _worldType;
	SyncFrameRate _syncFrameRate;
	
	Quality _shadowQuality;
	Quality _lightQuality;
	Quality _postEffQuality;
	Quality _envQuality;
	Filtering _filtering;
	Multisampling _multisampling;

	bool _editMode;
	bool _startScene;

	GraphManager::LightSrc* _sun;
	mutable D3DXVECTOR3 _sunPos;
	mutable D3DXQUATERNION _sunRot;

	GraphManager::LightSrc* _lamp[4];
	D3DXVECTOR3 _lampPos[4];
	D3DXQUATERNION _lampRot[4];
	D3DXCOLOR _lampColor[4];
	bool _lampSwitchOn[4];

	bool _enableRain;
	bool _isoRain;
	MapObj* _rain;	

	void CreateRain();
	void FreeRain();
	void ApplyRain();

	void EnableSun(bool enable, bool enableShadow = true);
	void EnableLamp(bool enable, int index, float farDist = 20.0f);
	void EnableLamps(bool enable);	
	void EnableWater(bool enable);
	void EnablePlanarRefl(bool enable);
	void EnableGrass(bool enable);
	void EnableGroundFog(bool enable);
	void EnableMagma(bool enable);
	void EnableRain(bool enable);

	bool CheckGraphMap(GraphManager::GraphOption option, Quality quality, std::pair<GraphManager::GraphOption, Quality> graphMap[], int count, Quality& resQuality);
	bool IsGraphMapSupported(Quality value, std::pair<GraphManager::GraphOption, Quality> graphMap[], int count);
	void SetGraphOption(GraphManager::GraphOption option, bool value);

	void ApplyCloudColor();
	void ApplyWheater();
	void ApplyWorldType();	

	GraphManager* GetGraph();
	CameraManager* GetCamera();
public:
	Environment(World* world);
	virtual ~Environment();

	void ApplyQuality();

	void StartScene();
	void ReleaseScene();
	void ProcessScene(float dt);

	const D3DXVECTOR3& GetSunPos() const;
	void SetSunPos(const D3DXVECTOR3& value);

	const D3DXQUATERNION& GetSunRot() const;
	void SetSunRot(const D3DXQUATERNION& value);

	const D3DXVECTOR3& GetLampPos(int index) const;
	void SetLampPos(const D3DXVECTOR3& value, int index);

	const D3DXQUATERNION& GetLampRot(int index) const;
	void SetLampRot(const D3DXQUATERNION& value, int index);

	const D3DXCOLOR& GetLampColor(int index) const;
	void SetLampColor(const D3DXCOLOR& value, int index);

	void SwitchOnLamp(int index, bool value);
	bool IsLampSwitchOn(int index);

	Wheater GetWheater() const;
	void SetWheater(Wheater value);
	
	WorldType GetWorldType() const;
	void SetWorldType(WorldType value);

	Quality GetShadowQuality() const;
	void SetShadowQuality(Quality value);
	bool IsShadowQualitySupported(Quality value);
	//
	Quality GetLightQuality() const;
	void SetLightQuality(Quality value);
	bool IsLightQualitySupported(Quality value);
	//
	Quality GetPostEffQuality() const;
	void SetPostEffQuality(Quality value);
	bool IsPostEffQualitySupported(Quality value);
	//
	Quality GetEnvQuality() const;
	void SetEnvQuality(Quality value);
	bool IsEnvQualitySupported(Quality value);
	//
	Filtering GetFiltering() const;
	void SetFiltering(Filtering value);
	bool IsFilteringSupported(Filtering value);
	//
	Multisampling GetMultisampling() const;
	void SetMultisampling(Multisampling value);
	bool IsMultisamplingSupported(Multisampling value);

	void AutodetectQuality();

	SyncFrameRate GetSyncFrameRate();
	void SetSyncFrameRate(SyncFrameRate value);

	float GetOrthoCameraFar() const;
	float GetPerspectiveCameraFar() const;

	bool GetGUIMode() const;
	void SetGUIMode(bool value);

	bool GetEditMode() const;
	void SetEditMode(bool value);
};

}

}