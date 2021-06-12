#include "stdafx.h"

#include "GraphManager.h"

#include "res\\R3DFile.h"
#include "res\\D3DXImageFile.h"

#include <DXGI.h>




#ifdef _DEBUG
	#define PLANAR_REFL_DEBUG
#endif

namespace r3d
{

const std::string GraphManager::cGraphOptionStr[GraphManager::cGraphOptionEnd] = {"goSkyBox", "goWater", "goSunShaft", "goBloom", "goHDR", "goShadow", "goGrassField", "goRefl", "goTrueRefl", "goPlanarRefl", "goPixelLighting", "goBumpMap", "goRefr", "goFog", "goPlaneFog", "goMagma"};

const std::string GraphManager::cGraphQualityStr[GraphManager::cGraphQualityEnd] = {"gqLow", "gqMiddle", "gqHigh"};




GraphManager::LightSrc::LightSrc(const LightDesc& desc): _enable(true), _shadowMap(0)
{
	_source = new graph::LightSource();

	SetDesc(desc);
}

GraphManager::LightSrc::~LightSrc()
{
	SetShadowMap(0);			

	delete _source;
}

graph::ShadowMapRender* GraphManager::LightSrc::GetShadowMap()
{
	return _shadowMap;
}

void GraphManager::LightSrc::SetShadowMap(graph::ShadowMapRender* value)
{
	if (ReplaceRef(_shadowMap, value))
	{
		if (_shadowMap)		
			_source->SetShadowMap(0);

		_shadowMap = value;

		if (_shadowMap)
			_source->SetShadowMap(_shadowMap->GetRT());
	}
}

void GraphManager::LightSrc::SetDesc(const LightDesc& desc)
{
	_desc = desc;
	_source->SetNear(_desc.nearDist);
	_source->SetFar(_desc.farDist);
}

const GraphManager::LightDesc& GraphManager::LightSrc::GetDesc() const
{
	return _desc;
}

graph::LightSource* GraphManager::LightSrc::GetSource()
{
	return _source;
}

bool GraphManager::LightSrc::GetEnable() const
{
	return _enable;
}

void GraphManager::LightSrc::SetEnable(bool value)
{
	_enable = value;	
}




GraphManager::GraphManager(HWND window, lsl::Point resolution, bool fullScreen): _camera(0), _clearSurfRef(0), _skyBoxRef(0), _depthSurfaceRef(0), _scRenderTexRef(0), _cleanScTexRef(0), _scRenderCubeTexRef(0), _scDepthMapRef(0), _refrEffRef(0), _sunShaft(0), _toneMapRef(0), _bloomEff(0), _hdrEff(0), _shadowMaxFar(0.0f), _shadowRef(0), _fogRef(0), _fogPlane(NULL), _fogPlaneActor(NULL), _cloudsTex(NULL), _sceneAmbient(clrWhite), _fogColor(clrWhite), _cloudColor(clrWhite), _cloudIntensivity(0.1f), _cloudHeight(0.0f), _guiMode(false), _groundAABB(1000.0f), _multisampling(0), _msRT(NULL), _msDS(NULL), _discreteVideoCard(false)
{
	_grassField = 0;
	_grassPlane = 0;

	_waterPlaneActor = 0;
	_waterPlane = 0;
	_waterNode = 0;

	_pixLightRef = 0;
	_pixLightShader = 0;

	_reflShader = 0;
	_reflBumpShader = 0;
	_reflRef = 0;
	_reflMappMode = rmColorLayer;
	_trueRefl = false;

	_planarReflRender = NULL;
	_planarReflShader = NULL;	
		
	_bumpMapShader = 0;

	_skyTex = "";
	_fogColor = clrWhite;
	_fogIntensivity = 0.5f;
	_orthoTarget.pos = NullVector;
	_orthoTarget.size = 0.0f;
	_cubeViewPos = NullVector;

	_engine = new graph::Engine(window, resolution, fullScreen, _multisampling);
	DetectCapabilities();

	r3d::res::R3DMeshFile::RegistredFile();
	r3d::res::D3DXImageFile::RegistredFile();
	r3d::res::D3DXCubeImageFile::RegistredFile();

	r3d::res::D3DXImageFile::driver = &_engine->GetDriver();
	r3d::res::D3DXCubeImageFile::driver = &_engine->GetDriver();

	_gui = new gui::Manager(_engine);

	_nodeScene = new graph::SceneManager();
	_preNodeScene = new graph::SceneManager();
	_actorManager = new graph::ActorManager(cOctreeSceneEnd);

	ZeroMemory(_graphOptions, sizeof(_graphOptions));
	ZeroMemory(_graphQuality, sizeof(_graphQuality));
}

GraphManager::~GraphManager()
{
	DisposeMultisampling();

	for (int i = cGraphOptionEnd - 1; i >= 0 ; --i)
		SetGraphOption(static_cast<GraphOption>(i), false);

	LSL_ASSERT(_actorManager->GetUserList().empty());

	SetCamera(0);
	ClearLightList();

	res::MeshData::GetResFormats().Clear();
	res::ImageResource::GetResFormats().Clear();
	res::CubeImageResource::GetResFormats().Clear();

	delete _actorManager;
	delete _nodeScene;
	delete _preNodeScene;
	delete _gui;
	delete _engine;
}

void GraphManager::DetectCapabilities()
{
	/*const unsigned cIntelVendorId = 0x8086;

	LSL_LOG("//--DISPLAY DEVICE--//");

	unsigned i = 0;
	DISPLAY_DEVICE displayDevice;
	ZeroMemory(&displayDevice, sizeof(displayDevice));
	displayDevice.cb = sizeof(displayDevice);

	struct VideoAdapter
	{
		lsl::string name;
		lsl::string desc;
		lsl::string id;
		bool primary;
		bool active;
		unsigned vendorId;
		unsigned deviceId;

		VideoAdapter(): primary(false), active(false) {}
	};

	std::vector<VideoAdapter> videoAdapters;
	VideoAdapter primaryVideoAdapter;

	while (EnumDisplayDevices(NULL, i, &displayDevice, 0) != 0)
	{
		LSL_LOG(lsl::StrFmt("device id=%d \n -name=%s \n -desc=%s \n -devId=%s \n -devKey=%s \n active=%d \n -mirroring=%d \n -modespuned=%d \n -primary=%d \n -removable=%d \n -vga=%d", i, displayDevice.DeviceName, displayDevice.DeviceString, displayDevice.DeviceID, displayDevice.DeviceKey, (displayDevice.StateFlags & DISPLAY_DEVICE_ACTIVE) != 0, (displayDevice.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER) != 0, (displayDevice.StateFlags & DISPLAY_DEVICE_MODESPRUNED) != 0, (displayDevice.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) != 0, (displayDevice.StateFlags & DISPLAY_DEVICE_REMOVABLE) != 0, (displayDevice.StateFlags & DISPLAY_DEVICE_VGA_COMPATIBLE) != 0));

		lsl::string devId = displayDevice.DeviceID;
		if (devId.empty())
		{
			++i;
			continue;
		}

		bool isDuplicate = false;

		for (unsigned j = 0; j < videoAdapters.size(); ++j)
			if (videoAdapters[j].id == devId)
			{
				isDuplicate = true;
				break;
			}

		VideoAdapter adapter;
		adapter.name = displayDevice.DeviceName;
		adapter.desc = displayDevice.DeviceString;
		adapter.id = devId;
		adapter.primary = (displayDevice.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) != 0;
		adapter.active = (displayDevice.StateFlags & DISPLAY_DEVICE_ACTIVE) != 0;

		{
			std::stringstream sstream(devId.substr(8, 4));
			std::hex(sstream);
			sstream >> adapter.vendorId;
		}
		{
			std::stringstream sstream(devId.substr(17, 4));
			std::hex(sstream);
			sstream >> adapter.deviceId;
		}

		if (!isDuplicate)
			videoAdapters.push_back(adapter);

		if (adapter.primary && !primaryVideoAdapter.primary)
			primaryVideoAdapter = adapter;
		else if ((adapter.active || i == 0) && !primaryVideoAdapter.primary && !primaryVideoAdapter.active)
			primaryVideoAdapter = adapter;

		++i;
	}

	LSL_LOG("//--DISPLAY DEVICE--//");*/

	/*LSL_LOG("//--DXGI--//");

	std::vector<VideoAdapter> dxActiveAdapters;
	VideoAdapter dxActiveAdapter;
	IDXGIFactory1* pFactory;
	HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)(&pFactory));

	if (hr == D3D_OK)
	{
		IDXGIAdapter1* pAdapter;
		unsigned i = 0;

		while (pFactory->EnumAdapters1(i, &pAdapter) != DXGI_ERROR_NOT_FOUND)
		{
			DXGI_ADAPTER_DESC1 desc;
			pAdapter->GetDesc1(&desc);

			LSL_LOG(lsl::StrFmt("dxgi adapter id=%d \n -desc=%s \n -vendorId=%d \n -deviceId=%d \n -subSysId=%d \n -revision=%d \n -videoMem=%d \n -sysMem=%d \n -sharedMem=%d \n -remote=%d", i, lsl::ConvertStrWToA(desc.Description).c_str(), desc.VendorId, desc.DeviceId, desc.SubSysId, desc.Revision, desc.DedicatedVideoMemory, desc.DedicatedSystemMemory, desc.SharedSystemMemory, (desc.Flags & DXGI_ADAPTER_FLAG_REMOTE) != 0));

			VideoAdapter adapter;			
			adapter.primary = false;			
			adapter.desc = lsl::ConvertStrWToA(desc.Description);
			adapter.id = lsl::StrFmt("%d&%d&%d&%d", desc.VendorId, desc.DeviceId, desc.SubSysId, desc.Revision);
			adapter.vendorId = desc.VendorId;
			adapter.deviceId = desc.DeviceId;

			unsigned j = 0;
			IDXGIOutput* pOutput;
			while(pAdapter->EnumOutputs(j, &pOutput) != DXGI_ERROR_NOT_FOUND)
			{
				DXGI_OUTPUT_DESC desc;
				pOutput->GetDesc(&desc);

				LSL_LOG(lsl::StrFmt(" -output id=%d name=%s attachedToDesktop(active)=%d", j, lsl::ConvertStrWToA(desc.DeviceName).c_str(), desc.AttachedToDesktop));

				adapter.name = lsl::ConvertStrWToA(desc.DeviceName);
				adapter.active = desc.AttachedToDesktop != 0;

				pOutput->Release();
				++j;
			}

			if (adapter.active)
			{
				bool isDuplicate = false;

				for (unsigned j = 0; j < dxActiveAdapters.size(); ++j)
				{
					if (dxActiveAdapters[i].id == adapter.id)
					{
						isDuplicate = true;
						break;
					}
				}

				if (!isDuplicate)
					dxActiveAdapters.push_back(adapter);

				if (!dxActiveAdapter.active)
					dxActiveAdapter = adapter;
			}
			else if ((!adapter.name.empty() || i == 0) && !dxActiveAdapter.active && dxActiveAdapter.name.empty())
				dxActiveAdapter = adapter;

			pAdapter->Release();
			++i;
		}
	}
	pFactory->Release();

	LSL_LOG("//--DXGI--//");

	if (dxActiveAdapters.size() >= 2)
	{
		_discreteVideoCard = true;
	}
	else if (videoAdapters.size() >= 2)
	{
		_discreteVideoCard = !(primaryVideoAdapter.vendorId == dxActiveAdapter.vendorId && primaryVideoAdapter.deviceId == dxActiveAdapter.deviceId);
	}
	else
	{
		unsigned vendorId = !dxActiveAdapter.id.empty() ? dxActiveAdapter.vendorId : primaryVideoAdapter.vendorId;

		if (vendorId == cIntelVendorId)
			_discreteVideoCard = false;
		else
			_discreteVideoCard = true;
	}*/
	_discreteVideoCard = true;

	LSL_LOG(lsl::StrFmt("discrete video detected = %d", _discreteVideoCard));

	D3DCAPS9 caps = _engine->GetDriver().GetCaps();

	LSL_LOG("//--CAPS--//");

	for (unsigned i = 0; i < _engine->GetDriver().GetAdapterCount(); ++i)
	{
		D3DADAPTER_IDENTIFIER9 adapter;
		_engine->GetDriver().GetAdapterIdentifier(i, 0, &adapter);

		LSL_LOG(lsl::StrFmt("adapter id=%d \n -driver=%s \n -desc=%s \n -name=%s \n -whql=%d \n -vendorId=%d \n -deviceId=%d \n -subSysId=%d \n -revision=%d", i, adapter.Driver, adapter.Description, adapter.DeviceName, adapter.WHQLLevel != 0, adapter.VendorId, adapter.DeviceId, adapter.SubSysId, adapter.Revision));
	}
	
	LSL_LOG(lsl::StrFmt("MaxAnisotropy = %d", caps.MaxAnisotropy));
	LSL_LOG(lsl::StrFmt("MaxUserClipPlanes = %d", caps.MaxUserClipPlanes));
	LSL_LOG(lsl::StrFmt("MaxTextureWidth = %d", caps.MaxTextureWidth));
	LSL_LOG(lsl::StrFmt("MaxTextureHeight = %d", caps.MaxTextureHeight));
	LSL_LOG(lsl::StrFmt("MaxTextureAspectRatio = %d", caps.MaxTextureAspectRatio));
	LSL_LOG(lsl::StrFmt("MaxTextureBlendStages = %d", caps.MaxTextureBlendStages));
	LSL_LOG(lsl::StrFmt("MaxSimultaneousTextures = %d", caps.MaxSimultaneousTextures));	
	LSL_LOG(lsl::StrFmt("MaxPrimitiveCount = %d", caps.MaxPrimitiveCount));
	LSL_LOG(lsl::StrFmt("MaxVertexIndex = %d", caps.MaxVertexIndex));
	LSL_LOG(lsl::StrFmt("MaxVertexShader30InstructionSlots = %d", caps.MaxVertexShader30InstructionSlots));
	LSL_LOG(lsl::StrFmt("MaxPixelShader30InstructionSlots = %d", caps.MaxPixelShader30InstructionSlots));

	LSL_LOG(lsl::StrFmt("VertexShaderVersion = %d.%d", D3DSHADER_VERSION_MAJOR(caps.VertexShaderVersion), D3DSHADER_VERSION_MINOR(caps.VertexShaderVersion)));
	LSL_LOG(lsl::StrFmt("PixelShaderVersion = %d.%d", D3DSHADER_VERSION_MAJOR(caps.PixelShaderVersion), D3DSHADER_VERSION_MINOR(caps.PixelShaderVersion)));

	LSL_LOG(lsl::StrFmt("D3DCAPS2_CANAUTOGENMIPMAP = %d", (caps.Caps2 & D3DCAPS2_CANAUTOGENMIPMAP) == D3DCAPS2_CANAUTOGENMIPMAP));
	LSL_LOG(lsl::StrFmt("D3DCAPS3_COPY_TO_SYSTEMMEM = %d", (caps.Caps3 & D3DCAPS3_COPY_TO_SYSTEMMEM) == D3DCAPS3_COPY_TO_SYSTEMMEM));
	LSL_LOG(lsl::StrFmt("D3DCAPS3_COPY_TO_VIDMEM = %d", (caps.Caps3 & D3DCAPS3_COPY_TO_VIDMEM) == D3DCAPS3_COPY_TO_VIDMEM));

	LSL_LOG(lsl::StrFmt("D3DTEXOPCAPS_BLENDTEXTUREALPHA = %d", (caps.TextureOpCaps & D3DTEXOPCAPS_BLENDTEXTUREALPHA) == D3DTEXOPCAPS_BLENDTEXTUREALPHA));
	LSL_LOG(lsl::StrFmt("D3DPTEXTURECAPS_SQUAREONLY = %d", (caps.TextureCaps & D3DPTEXTURECAPS_SQUAREONLY) == D3DPTEXTURECAPS_SQUAREONLY));
	LSL_LOG(lsl::StrFmt("D3DPTEXTURECAPS_POW2 = %d", (caps.TextureCaps & D3DPTEXTURECAPS_POW2) == D3DPTEXTURECAPS_POW2));
	LSL_LOG(lsl::StrFmt("D3DPTEXTURECAPS_NONPOW2CONDITIONAL = %d", (caps.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL) == D3DPTEXTURECAPS_NONPOW2CONDITIONAL));
	LSL_LOG(lsl::StrFmt("D3DFVFCAPS_TEXCOORDCOUNTMASK = %d", caps.FVFCaps & D3DFVFCAPS_TEXCOORDCOUNTMASK));
	LSL_LOG(lsl::StrFmt("D3DDEVCAPS2_CAN_STRETCHRECT_FROM_TEXTURES = %d", (caps.DevCaps2 & D3DDEVCAPS2_CAN_STRETCHRECT_FROM_TEXTURES) == D3DDEVCAPS2_CAN_STRETCHRECT_FROM_TEXTURES));

	LSL_LOG(lsl::StrFmt("D3DPRASTERCAPS_SCISSORTEST = %d", (caps.RasterCaps & D3DPRASTERCAPS_SCISSORTEST) == D3DPRASTERCAPS_SCISSORTEST));

	LSL_LOG(lsl::StrFmt("D3DPTEXTURECAPS_CUBEMAP = %d", (caps.TextureCaps & D3DPTEXTURECAPS_CUBEMAP) == D3DPTEXTURECAPS_CUBEMAP));
	LSL_LOG(lsl::StrFmt("D3DPTEXTURECAPS_CUBEMAP_POW2 = %d", (caps.TextureCaps & D3DPTEXTURECAPS_CUBEMAP_POW2) == D3DPTEXTURECAPS_CUBEMAP_POW2));
	LSL_LOG(lsl::StrFmt("D3DPTEXTURECAPS_MIPCUBEMAP = %d", (caps.TextureCaps & D3DPTEXTURECAPS_MIPCUBEMAP) == D3DPTEXTURECAPS_MIPCUBEMAP));

	LSL_LOG("//--CAPS END--//");
}

void GraphManager::InitClearSurf()
{
	if (_clearSurfRef++ == 0)
	{
		_clearSurf = new graph::ClearSurf();
		_clearSurf->shader.GetOrCreateData()->LoadFromFile("Data\\Shaders\\clearSurf.fx");
		_clearSurf->shader.Init(*_engine);
	}
}

void GraphManager::FreeClearSurf()
{
	LSL_ASSERT(_clearSurfRef > 0);

	if (--_clearSurfRef == 0)
	{
		delete _clearSurf;
	}
}

void GraphManager::InitSkyBox()
{
	if (_skyBoxRef == 0)
	{
		_skyBoxTex = new graph::TexCubeResource();
		_skyBoxTex->GetOrCreateData()->SetFileName(_skyTex);
		_skyBoxTex->Init(*_engine);

		_skyBox = new graph::SkyBox();
		_skyBox->SetSkyTex(_skyBoxTex);

		_skyBox->shader.GetOrCreateData()->LoadFromFile("Data\\Shaders\\skybox.fx");
		_skyBox->shader.Init(*_engine);
	}
	++_skyBoxRef;
}

void GraphManager::FreeSkyBox()
{
	LSL_ASSERT(_skyBoxRef > 0);

	if (--_skyBoxRef == 0)
	{
		lsl::SafeDelete(_skyBox);
		delete _skyBoxTex;
	}
}

bool GraphManager::IsSkyboxSupported()
{
	return true;
}

void GraphManager::InitWaterPlane()
{
	if (!_waterPlane && _graphQuality[goWater] != gqLow)
	{
		InitScDepthMap();

		_reflRender = new graph::ReflRender();
		graph::Tex2DResource* reflTex = _reflRender->GetOrCreateRT();
		reflTex->GetOrCreateData()->SetFormat(_engine->GetParams().BackBufferFormat);
		reflTex->SetScreenScale(D3DXVECTOR2(1.0f, 1.0f));
		reflTex->Init(*_engine);

		_waterTexNorm = new graph::Tex2DResource();
		_waterTexNorm->GetOrCreateData()->SetFileName("Data\\Misc\\water00.png");
		_waterTexNorm->Init(*_engine);

		_waterPlane = new graph::WaterPlane();
		_waterPlane->shader.GetOrCreateData()->LoadFromFile("Data\\Shaders\\water.fx");
		_waterPlane->shader.Init(*_engine);
		_waterPlane->SetDepthTex(_scDepthMap->GetRT());
		_waterPlane->SetReflTex(reflTex);
		_waterPlane->SetNormTex(_waterTexNorm);
		_waterPlane->SetColor(D3DXCOLOR(0.0f, 0.2f, 0.5f, 1.0f));
		_waterPlane->SetCloudIntens(0.1f);
	}
	else if (_waterPlane && _graphQuality[goWater] == gqLow)
	{
		lsl::SafeDelete(_waterPlane);
		delete _waterTexNorm;
		delete _reflRender;

		FreeScDepthMap();
	}

	if (!_waterNode && _graphQuality[goWater] == gqLow)
	{
		_waterPlaneActor = new graph::Actor();
		graph::Actor::GraphDesc desc;
		desc.props.set(graph::Actor::gpColor);
		desc.props.set(graph::Actor::gpDynamic);
		//desc.order = graph::Actor::goEffect;
		_waterPlaneActor->SetGraph(this, desc);	

		_waterColor = new graph::Tex2DResource();
		_waterColor->GetOrCreateData()->SetFileName("Data\\Effect\\waterColor.dds");
		_waterColor->Init(*_engine);

		_waterNode = &_waterPlaneActor->GetNodes().Add<graph::PlaneNode>();
		_waterNode->material.GetOrCreate()->samplers.Add2d(_waterColor);
		_waterNode->material.Get()->material.SetOption(graph::Material::moLighting, false);
	}
	else if (_waterNode && _graphQuality[goWater] != gqLow)
	{
		lsl::SafeDelete(_waterPlaneActor);

		delete _waterColor;
		_waterNode = NULL;
	}

	UpdateWaterPlane();
}

void GraphManager::FreeWaterPlane()
{
	if (_waterPlane)
	{
		lsl::SafeDelete(_waterPlane);
		delete _waterTexNorm;
		delete _reflRender;

		FreeScDepthMap();

	}
	if (_waterNode)
	{
		lsl::SafeDelete(_waterPlaneActor);

		delete _waterColor;
		_waterNode = NULL;
	}
}

void GraphManager::UpdateWaterPlane()
{
	const float scale = 4.0f;

	if (_waterPlaneActor)
	{
		_waterPlaneActor->SetPos(_groundAABB.GetCenter());
		_waterPlaneActor->SetScale(scale);
		_waterNode->SetSize(D3DXVECTOR2(_groundAABB.GetSizes()) / scale);
	}

	if (_waterPlane)
	{
		_waterPlane->SetPos(_groundAABB.GetCenter());
		_waterPlane->SetScale(scale);
		_waterPlane->SetSize(D3DXVECTOR2(_groundAABB.GetSizes()) / scale);		
	}
}

void GraphManager::InitGrassField()
{
	if (!_grassPlane)
	{
		_grassPlTex = new graph::Tex2DResource();
		_grassPlTex->GetOrCreateData()->SetFileName("Data\\Misc\\StadiumGrass1.dds");
		_grassPlTex->SetLevelCnt(0);
		_grassPlTex->SetUsage(D3DUSAGE_AUTOGENMIPMAP);
		_grassPlTex->Init(*_engine);

		_grassPlane = new graph::Actor();
		_grassPlaneNode = &_grassPlane->GetNodes().Add<graph::PlaneNode>();
		graph::LibMaterial* libMat = _grassPlaneNode->material.GetOrCreate();
		libMat->material.SetOption(graph::Material::moLighting, false);

		libMat->samplers.Add2d(_grassPlTex);
		libMat->samplers.back().SetFiltering(graph::BaseSampler::sfAnisotropic);

		graph::Actor::GraphDesc desc;
		desc.props.set(graph::Actor::gpColor);
		desc.props.set(graph::Actor::gpDynamic);
		desc.props.set(graph::Actor::gpShadowApp);
		//Осторожно. libMat->material.SetOption(graph::Material::moLighting, false);!!!. Только СТД освещение
		desc.lighting = graph::Actor::glStd;
		_grassPlane->SetGraph(this, desc);
	}

	if (!_grassField && _graphQuality[goGrassField] != gqLow)
	{
		_grassTex = new graph::Tex2DResource();
		_grassTex->GetOrCreateData()->SetFileName("Data\\Misc\\flower2.dds");
		_grassTex->SetLevelCnt(0);
		_grassTex->SetUsage(D3DUSAGE_AUTOGENMIPMAP);
		_grassTex->Init(*_engine);

		_grassMat = new graph::LibMaterial();
		_grassMat->samplers.Add2d(_grassTex).SetFiltering(graph::BaseSampler::sfAnisotropic);
		
		graph::GrassField::GrassList list;
		graph::GrassField::GrassDesc desc(_grassMat);
		desc.tiles.clear();
		desc.tiles.push_back(graph::GrassField::GrassTile(2, D3DXVECTOR4(0, 0.5f, 0.5f, 0)));
		desc.tiles.push_back(graph::GrassField::GrassTile(1, D3DXVECTOR4(0.5f, 0.5f, 1.0f, 0)));
		desc.tiles.push_back(graph::GrassField::GrassTile(10, D3DXVECTOR4(0, 1.0f, 0.5f, 0.5f)));
		desc.tiles.push_back(graph::GrassField::GrassTile(1, D3DXVECTOR4(0.5, 1.0f, 1.0f, 0.5f)));
		list.push_back(desc);

		_grassField = new graph::GrassField();
		_grassField->SetGrassList(list);
		_grassField->SetDensity(1.0f);
		_grassField->SetScale(1.5f);

		_grassField->shader.GetOrCreateData()->LoadFromFile("Data\\Shaders\\grassFiled.fx");
		_grassField->shader.SetTech("techGrassField");
	}

	UpdateGrassPlane();
}

void GraphManager::FreeGrassField()
{
	if (_grassPlane)
	{
		lsl::SafeDelete(_grassPlane);
		delete _grassPlTex;
	}

	if (_grassField)
	{
		lsl::SafeDelete(_grassField);	

		delete _grassMat;
		delete _grassTex;
	}
}

void GraphManager::UpdateGrassPlane()
{
	D3DXVECTOR3 pos = _groundAABB.GetCenter();
	pos.x = 0;
	D3DXVECTOR2 size = D3DXVECTOR2(_groundAABB.GetSizes());

	if (_grassPlane)
	{
		_grassPlaneNode->SetPos(pos);
		_grassPlaneNode->SetSize(size);
	}
	if (_grassField)
	{
		_grassField->SetPos(D3DXVECTOR3(pos.x, pos.y, 0.9f));
		_grassField->SetWidth(size.x);
		_grassField->SetHeight(size.y);
	}
}

void GraphManager::InitDepthSurface()
{
	++_depthSurfaceRef;

	if (_depthSurfaceRef == 1)
	{
		_depthSurface = new graph::DepthStencilSurfaceResource(); 
		_depthSurface->SetScreenScale(D3DXVECTOR2(1.0f, 1.0f));
		_depthSurface->SetFormat(_engine->GetParams().AutoDepthStencilFormat);
		_depthSurface->Init(*_engine);
	}
}

void GraphManager::FreeDepthSurface()
{
	LSL_ASSERT(_depthSurfaceRef > 0);

	if (--_depthSurfaceRef == 0)	
		delete _depthSurface;
}

void GraphManager::InitScRenderTex()
{
	++_scRenderTexRef;

	if (_scRenderTexRef == 1)
	{
		_scRenderTex = new graph::RenderToTexture();
		graph::Tex2DResource* tex =  _scRenderTex->GetOrCreateRT();
		tex->GetOrCreateData()->SetFormat(D3DFMT_A16B16G16R16F);		
		tex->SetScreenScale(D3DXVECTOR2(1.0f, 1.0f));		
		tex->Init(*_engine);

		ApplyMultisampling();
	}
}

void GraphManager::FreeScRenderTex()
{
	LSL_ASSERT(_scRenderTexRef > 0);

	if (--_scRenderTexRef == 0)	
	{
		delete _scRenderTex;

		ApplyMultisampling();
	}
}

void GraphManager::InitCleanScTex()
{
	if (_cleanScTexRef++ == 0)
	{
		_cleanScTex = new graph::RenderToTexture();
		graph::Tex2DResource* tex =  _cleanScTex->GetOrCreateRT();
		tex->GetOrCreateData()->SetFormat(D3DFMT_A16B16G16R16F);
		tex->SetScreenScale(D3DXVECTOR2(1.0f, 1.0f));		
		tex->Init(*_engine);
	}
}

void GraphManager::FreeCleanScTex()
{
	LSL_ASSERT(_cleanScTexRef > 0);

	if (--_cleanScTexRef == 0)	
		delete _cleanScTex;
}

void GraphManager::InitScRenderCubeTex()
{
	if (_scRenderCubeTexRef == 0)
	{
		_scRenderCubeTex = new graph::RenderToCubeTex();
		graph::TexCubeResource* tex =  _scRenderCubeTex->GetOrCreateRT();
		tex->GetOrCreateData()->SetFormat(_engine->GetParams().BackBufferFormat);		
		tex->GetData()->SetWidth(512 * 6);
		tex->Init(*_engine);		
	}

	++_scRenderCubeTexRef;
}

void GraphManager::FreeScRenderCubeTex()
{
	LSL_ASSERT(_scRenderCubeTexRef > 0);

	if (--_scRenderCubeTexRef == 0)
		delete _scRenderCubeTex;
}

void GraphManager::InitScDepthMap()
{
	if (_scDepthMapRef == 0)
	{
		//InitClearSurf();

		_scDepthMap = new graph::DepthMapRender();
		graph::Tex2DResource* tex = _scDepthMap->GetOrCreateRT();
		tex->GetOrCreateData()->SetFormat(D3DFMT_R32F);
		tex->SetScreenScale(D3DXVECTOR2(1.0f, 1.0f));
		tex->Init(*_engine);

		_scDepthMap->shader.GetOrCreateData()->LoadFromFile("Data\\Shaders\\sunDepthMap.fx");
		_scDepthMap->shader.Init(*_engine);
	}
	++_scDepthMapRef;
}

void GraphManager::FreeScDepthMap()
{
	LSL_ASSERT(_scDepthMapRef > 0);

	--_scDepthMapRef;

	if (_scDepthMapRef == 0)
	{
		//FreeClearSurf();

		delete _scDepthMap;
	}
}

void GraphManager::InitRefrEff()
{
	if (_refrEffRef++ == 0)
	{
		InitScRenderTex();
		InitCleanScTex();

		_refrShader = new graph::RefrShader();
		_refrShader->GetOrCreateData()->LoadFromFile("Data\\Shaders\\refract.fx");
		_refrShader->Init(*_engine);

		_refrShader->SetTexture("sceneTex", _cleanScTex->GetRT());		
	}
}

void GraphManager::FreeRefrEff()
{
	LSL_ASSERT(_refrEffRef > 0);

	if (--_refrEffRef == 0)
	{
		delete _refrShader;

		FreeCleanScTex();
		FreeScRenderTex();
	}
}

void GraphManager::InitSunShaft()
{
	if (!_sunShaft)
	{
		InitScRenderTex();
		InitScDepthMap();

		_sunShaft = new graph::SunShaftRender();
		_sunShaft->SetRT(_scRenderTex->GetRT());
		_sunShaft->SetColorTex(_scRenderTex->GetRT());
		_sunShaft->SetDepthTex(_scDepthMap->GetRT());

		_sunShaft->shader.GetOrCreateData()->LoadFromFile("Data\\Shaders\\sunShaft.fx");
		_sunShaft->shader.Init(*_engine);
	}
}

void GraphManager::FreeSunShaft()
{
	if (_sunShaft)
	{
		lsl::SafeDelete(_sunShaft);

		FreeScDepthMap();
		FreeScRenderTex();
	}
}

void GraphManager::InitToneMap()
{
	if (_toneMapRef == 0)
	{
		InitScRenderTex();

		_toneMap = new graph::ToneMapping();
		_toneMap->SetRT(_scRenderTex->GetRT());
		_toneMap->SetColorTex(_scRenderTex->GetRT());
		
		_toneMap->shader.GetOrCreateData()->LoadFromFile("Data\\Shaders\\toneMapping.fx");
		_toneMap->shader.Init(*_engine);
	}

	++_toneMapRef;
}

void GraphManager::FreeToneMap()
{
	LSL_ASSERT(_toneMapRef > 0);

	--_toneMapRef;

	if (_toneMapRef == 0)
	{
		delete _toneMap;

		FreeScRenderTex();
	}
}

void GraphManager::InitBloomEff()
{
	if (!_bloomEff)
	{
		InitScRenderTex();
		InitToneMap();

		_bloomEff = new graph::BloomRender();
		_bloomEff->GetOrCreateRT()->Init(*_engine);

		_bloomEff->SetColorTex(_scRenderTex->GetRT());

		_bloomEff->shader.GetOrCreateData()->LoadFromFile("Data\\Shaders\\bloom.fx");
		_bloomEff->shader.Init(*_engine);

		_toneMap->SetBloomTex(_bloomEff->GetRT());

		if (_hdrEff)
			_bloomEff->SetLumTex(_hdrEff->GetRT());

		ApplyHDRParams();
	}
}

void GraphManager::FreeBloomEff()
{
	if (_bloomEff)
	{
		_toneMap->SetBloomTex(0);

		lsl::SafeDelete(_bloomEff);

		FreeToneMap();
		FreeScRenderTex();
	}
}

void GraphManager::InitHDREff()
{
	if (!_hdrEff)
	{
		InitScRenderTex();
		InitToneMap();

		_hdrEff = new graph::HDRRender();
		_hdrEff->GetOrCreateRT()->Init(*_engine);
		_hdrEff->SetColorTex(_scRenderTex->GetRT());

		_hdrEff->shader.GetOrCreateData()->LoadFromFile("Data\\Shaders\\hdr.fx");
		_hdrEff->shader.Init(*_engine);

		_toneMap->SetHDRTex(_hdrEff->GetRT());

		if (_bloomEff)
			_bloomEff->SetLumTex(_hdrEff->GetRT());

		ApplyHDRParams();
	}
}

void GraphManager::FreeHDREff()
{
	if (_hdrEff)
	{
		if (_bloomEff)
			_bloomEff->SetLumTex(0);
		_toneMap->SetHDRTex(0);

		lsl::SafeDelete(_hdrEff);

		FreeToneMap();
		FreeScRenderTex();
	}
}

void GraphManager::ApplyHDRParams()
{
	if (_bloomEff)
	{
		_bloomEff->shader.SetValue("lumKey", _hdrParams.lumKey);
		_bloomEff->shader.SetValue("BRIGHT_THRESHOLD", _hdrParams.brightThreshold);
	}
	if (_hdrEff)
	{
		_toneMap->shader.SetValue("fGaussianScalar", _hdrParams.gaussianScalar);
		_toneMap->shader.SetValue("exposure", _hdrParams.exposure);
		_toneMap->shader.SetValue("colorCorrection", _hdrParams.colorCorrection);
	}
}

void GraphManager::SetLightShadow(LightSrc* light)
{
	if (_shadowRef && light->GetDesc().shadow)
	{
		graph::ShadowMapRender* shadowMap = light->GetShadowMap();
		if (shadowMap == NULL)
		{
			shadowMap = new graph::ShadowMapRender();

			graph::Tex2DResource* tex = shadowMap->GetOrCreateRT();
			tex->SetScreenScale(D3DXVECTOR2(1.0f, 1.0f));
			tex->Init(*_engine);

			shadowMap->shader.GetOrCreateData()->LoadFromFile("Data\\Shaders\\ShadowMap.fx");
			shadowMap->shader.Init(*_engine);

			shadowMap->depthRender.shader.GetOrCreateData()->LoadFromFile("Data\\Shaders\\depthMap.fx");
			shadowMap->depthRender.shader.Init(*_engine);

			light->SetShadowMap(shadowMap);
		}

		shadowMap->SetNumSplits(light->GetDesc().shadowNumSplit);
		shadowMap->SetDisableCropLight(light->GetDesc().shadowDisableCropLight);
	}
	else if (_shadowRef && !light->GetDesc().shadow)
	{
		FreeLightShadow(light);
	}
}

void GraphManager::FreeLightShadow(LightSrc* light)
{
	if (light->GetShadowMap())
	{
		graph::ShadowMapRender* shadowMap = light->GetShadowMap();

		light->SetShadowMap(0);
		
		delete shadowMap;
	}
}

void GraphManager::InitShadowMap()
{
	if (_shadowRef++ == 0)
	{
		for (LightList::iterator iter = _lightList.begin(); iter != _lightList.end(); ++iter)
			SetLightShadow(*iter);
	}
}

void GraphManager::FreeShadowMap()
{
	LSL_ASSERT(_shadowRef > 0);

	if (--_shadowRef == 0)
	{
		for (LightList::iterator iter = _lightList.begin(); iter != _lightList.end(); ++iter)
			FreeLightShadow(*iter);
	}
}

void GraphManager::InitPixLight()
{
	if (_pixLightRef++ == 0)
	{
		_pixLightShader = new graph::LightShader();
		_pixLightShader->SetTech("techPixLightSpot");
		_pixLightShader->GetOrCreateData()->LoadFromFile("Data\\Shaders\\pixLight.fx");
		_pixLightShader->Init(*_engine);
	}
}

void GraphManager::FreePixLight()
{
	LSL_ASSERT(_pixLightRef > 0);

	if (--_pixLightRef == 0)
	{
		delete _pixLightShader;
		_pixLightShader = 0;
	}
}

void GraphManager::InitRefl()
{
	if (_reflRef == 0)
	{
		InitSkyBox();
		_reflTex = _skyBoxTex;
		_reflTex->AddRef();

		_reflShader = new graph::ReflMappShader();
		_reflShader->GetOrCreateData()->LoadFromFile("Data\\Shaders\\reflMapp.fx");
		_reflShader->Init(*_engine);
		_reflShader->SetReflTex(_reflTex);

		_reflBumpShader = new graph::ReflBumbMappShader();
		_reflBumpShader->GetOrCreateData()->LoadFromFile("Data\\Shaders\\reflBumpMapp.fx");
		_reflBumpShader->Init(*_engine);
		_reflBumpShader->SetReflTex(_reflTex);
	}

	++_reflRef;

	RefreshRefl();
}

void GraphManager::FreeRefl()
{
	LSL_ASSERT(_reflRef > 0);

	if (--_reflRef == 0)
	{
		delete _reflBumpShader;
		delete _reflShader;

		_reflBumpShader = NULL;
		_reflShader = NULL;
		_reflTex->Release();
		FreeSkyBox();
	}
}

void GraphManager::RefreshRefl()
{
	if (_reflRef > 0)
	{
		_reflShader->SetTech(_reflMappMode == rmColorLayer ? "techReflMapp" : "techReflMappMenu");
		_reflShader->SetReflectivity(_reflMappMode == rmColorLayer ? 0.4f : 0.4f);
	}
}

void GraphManager::InitTrueRefl()
{
	if (!_trueRefl)
	{
		_trueRefl = true;

		InitRefl();
		InitScRenderCubeTex();

		_reflShader->SetReflTex(_scRenderCubeTex->GetRT());
		_reflBumpShader->SetReflTex(_scRenderCubeTex->GetRT());
	}
}

void GraphManager::FreeTrueRefl()
{
	if (_trueRefl)
	{
		_trueRefl = false;

		_reflBumpShader->SetReflTex(_reflTex);
		_reflShader->SetReflTex(_reflTex);

		FreeScRenderCubeTex();
		FreeRefl();
	}
}

#ifdef PLANAR_REFL_DEBUG

graph::Actor* _planarActor = NULL;

#endif

void GraphManager::InitPlanarRefl()
{
	if (_planarReflRender == NULL)
	{
		InitDepthSurface();

		_planarReflRender = new graph::ReflRender();
		graph::Tex2DResource* reflTex = _planarReflRender->GetOrCreateRT();
		reflTex->GetOrCreateData()->SetFormat(_engine->GetParams().BackBufferFormat);
		reflTex->SetScreenScale(D3DXVECTOR2(1.0f, 1.0f));
		reflTex->Init(*_engine);

		_planarReflShader = new graph::PlanarReflMappShader();
		_planarReflShader->GetOrCreateData()->LoadFromFile("Data\\Shaders\\planarReflMapp.fx");
		_planarReflShader->Init(*_engine);
		_planarReflShader->SetReflTex(reflTex);		

#ifdef PLANAR_REFL_DEBUG
		_planarActor = new graph::Actor();
		graph::PlaneNode* planeNode = &_planarActor->GetNodes().Add<graph::PlaneNode>();		
		planeNode->SetScale(100.0f);

		graph::Actor::GraphDesc desc;
		desc.lighting = graph::IActor::glPlanarRefl;
		desc.order = graph::IActor::goDefault;
		desc.props.set(graph::IActor::gpColor);
		desc.props.set(graph::IActor::gpReflWater);
		desc.props.set(graph::IActor::gpReflScene);
		desc.props.set(graph::IActor::gpShadowCast);
		desc.props.set(graph::IActor::gpShadowApp);
		desc.props.set(graph::Actor::gpDynamic);
		//_planarActor->SetGraph(this, desc);
#endif
	}
}

void GraphManager::FreePlanarRefl()
{
	if (_planarReflRender)
	{

#ifdef PLANAR_REFL_DEBUG
		lsl::SafeDelete(_planarActor);
#endif
		lsl::SafeDelete(_planarReflShader);
		lsl::SafeDelete(_planarReflRender);

		FreeDepthSurface();
	}
}

void GraphManager::InitBumpMap()
{
	if (!_bumpMapShader)
	{
		_bumpMapShader = new graph::BumpMapShader();
		_bumpMapShader->GetOrCreateData()->LoadFromFile("Data\\Shaders\\bumpMap.fx");
		_bumpMapShader->Init(*_engine);
	}
}

void GraphManager::FreeBumpMap()
{
	if (_bumpMapShader)
	{
		lsl::SafeDelete(_bumpMapShader);
	}
}

void GraphManager::InitFog()
{
	++_fogRef;
}

void GraphManager::FreeFog()
{
	LSL_ASSERT(_fogRef > 0);

	--_fogRef;
}

void GraphManager::InitPlaneFog(lsl::string texture, const D3DXVECTOR2& tileScale, float speed, GraphQuality quality)
{
	if (_cloudsTex == NULL)
	{
		_cloudsTex = new graph::Tex2DResource();
		_cloudsTex->GetOrCreateData()->SetFileName(texture);
		_cloudsTex->SetLevelCnt(0);
		_cloudsTex->SetUsage(D3DUSAGE_AUTOGENMIPMAP);
		_cloudsTex->Init(*_engine);

		_cloudsMat = new graph::LibMaterial();
		_cloudsMat->samplers.Add2d(_cloudsTex).SetFiltering(graph::BaseSampler::sfAnisotropic);
		_cloudsMat->material.SetOption(graph::Material::moLighting, false);
		_cloudsMat->material.SetDiffuse(_cloudColor);
	}
	else
	{
		_cloudsTex->GetOrCreateData()->SetFileName(texture);
		_cloudsTex->Reload();
	}

	if (quality == gqLow)
	{
		if (_fogPlaneActor == NULL)
		{
			_fogPlaneActor = new graph::Actor();
			graph::PlaneNode& plane = _fogPlaneActor->GetNodes().Add<graph::PlaneNode>();
			plane.material.Set(_cloudsMat);

			graph::Actor::GraphDesc desc;
			desc.props.set(graph::Actor::gpColor);
			desc.props.set(graph::Actor::gpDynamic);
			desc.props.set(graph::Actor::gpShadowApp);
			//Осторожно. libMat->material.SetOption(graph::Material::moLighting, false);!!!. Только СТД освещение
			desc.lighting = graph::Actor::glStd;
			_fogPlaneActor->SetGraph(this, desc);
		}
	}
	else
		lsl::SafeDelete(_fogPlaneActor);

	if (quality != gqLow)
	{
		if (_fogPlane == NULL)
		{
			InitScDepthMap();

			_fogPlane = new graph::FogPlane();		
				
			_fogPlane->SetDepthTex(_scDepthMap->GetRT());
			_fogPlane->SetCloudsMat(_cloudsMat);
			_fogPlane->SetColor(_cloudColor);
			_fogPlane->SetCloudIntens(_cloudIntensivity);			
			_fogPlane->shader.GetOrCreateData()->LoadFromFile("Data\\Shaders\\fogPlane.fx");
			_fogPlane->shader.Init(*_engine);
		}

		_fogPlane->SetSpeed(speed);
	}
	else if (_fogPlane != NULL)
	{	
		lsl::SafeDelete(_fogPlane);
		FreeScDepthMap();
	}

	_tileScale = tileScale;	

	UpdateFogPlane();
}

void GraphManager::FreePlaneFog()
{
	if (_fogPlane)
	{
		lsl::SafeDelete(_fogPlane);
		FreeScDepthMap();
	}

	lsl::SafeDelete(_fogPlaneActor);

	if (_cloudsTex)
	{
		delete _cloudsMat;
		lsl::SafeDelete(_cloudsTex);
	}
}

void GraphManager::UpdateFogPlane()
{
	D3DXVECTOR3 pos = _groundAABB.GetCenter();
	pos.z = _cloudHeight;

	if (_fogPlane)
	{		
		_fogPlane->SetPos(pos);
		_fogPlane->SetScale(D3DXVECTOR3(_tileScale.x, _tileScale.y, 1.0f));
		_fogPlane->SetSize(D3DXVECTOR2(_groundAABB.GetSizes()) / _tileScale);
	}

	if (_fogPlaneActor)
	{
		_fogPlaneActor->SetPos(_groundAABB.GetCenter());
		_fogPlaneActor->SetScale(D3DXVECTOR3(_tileScale.x, _tileScale.y, 1));

		static_cast<graph::PlaneNode&>(_fogPlaneActor->GetNodes().front()).SetSize(D3DXVECTOR2(_groundAABB.GetSizes()) / _tileScale);
	}
}

void GraphManager::ApplyFog()
{
	LSL_ASSERT(_camera);

	if (_camera->GetStyle() == graph::csOrtho)
		return;

	//float fogStart = 0.0f;
	//float fogEnd = 0.9f * _cameraFar;
	//fogStart = (fogEnd - fogStart) * (1.0f - lsl::ClampValue(_fogIntensivity, 0.0f, 1.0f));

	float fogStart = _camera->GetFar() * (1.0f - lsl::ClampValue(_fogIntensivity, 0.0f, 1.0f));
	float fogEnd = _camera->GetFar();

	if (_fogRef)
	{
		_engine->GetContext().SetRenderState(graph::rsFogStart, *(DWORD*)(&fogStart));
		_engine->GetContext().SetRenderState(graph::rsFogEnd, *(DWORD*)(&(fogEnd)));
		_engine->GetContext().SetRenderState(graph::rsFogTableMode, D3DFOG_LINEAR);
		_engine->GetContext().SetRenderState(graph::rsFogColor, _fogColor);
		_engine->GetContext().SetRenderState(graph::rsFogEnable, true);
	}
}

void GraphManager::UnApplyFog()
{
	if (_fogRef)		
		_engine->GetContext().RestoreRenderState(graph::rsFogEnable);	
}

void GraphManager::ApplyMultisampling()
{
	if (_multisampling == UINT_MAX)
		return;

	if (_multisampling > 0 && _scRenderTexRef > 0)
	{
		if (_engine->GetMultisampling() != 0)
			Reset(0);

		D3DMULTISAMPLE_TYPE msType;
		unsigned msQuality;
		graph::Engine::ToMultisampling(_multisampling, msType, msQuality);

		if (_msRT == NULL)
		{
			_msRT = new graph::RenderTargetResource(); 
			_msRT->SetScreenScale(D3DXVECTOR2(1.0f, 1.0f));
			_msRT->SetFormat(_scRenderTex->GetRT()->GetData()->GetFormat());
		}
		_msRT->SetMultisampleType(msType);
		_msRT->SetMultisampleQuality(msQuality);
		_msRT->Init(*_engine);

		if (_msDS == NULL)
		{
			InitDepthSurface();

			_msDS = new graph::DepthStencilSurfaceResource(); 
			_msDS->SetScreenScale(D3DXVECTOR2(1.0f, 1.0f));
			_msDS->SetFormat(_engine->GetParams().AutoDepthStencilFormat);
		}
		_msDS->SetMultisampleType(msType);
		_msDS->SetMultisampleQuality(msQuality);
		_msDS->Init(*_engine);
	}
	else
	{
		lsl::SafeDelete(_msRT);

		if (_msDS)
		{
			lsl::SafeDelete(_msDS);
			FreeDepthSurface();
		}

		if (_engine->GetMultisampling() != _multisampling)
			Reset(_multisampling);
	}
}

void GraphManager::DisposeMultisampling()
{
	_multisampling = UINT_MAX;
	lsl::SafeDelete(_msRT);

	if (_msDS)
	{
		lsl::SafeDelete(_msDS);

		FreeDepthSurface();
	}
}

void GraphManager::PrepareActor(graph::Actor* actor, graph::ActorManager::UserDesc& resDesc)
{
	using graph::Actor;

	const graph::Actor::GraphDesc& desc = actor->GetGraphDesc();
	const graph::Actor::GraphProps props = desc.props;
	const graph::Actor::Lighting lighting = desc.lighting;
	const graph::Actor::Order order = desc.order;

	resDesc.actor = actor;
	resDesc.culling = !actor->GetGraphDesc().props.test(Actor::gpMorph);
	resDesc.dynamic = actor->GetGraphDesc().props.test(Actor::gpDynamic);

	for (unsigned ind = 0; ind < cOctreeSceneEnd; ++ind)
	{	
		switch (ind)
		{
		case osReflWater:
			resDesc.scenes[ind] = props.test(Actor::gpReflWater);
			break;

		case osViewCubeMap:
			resDesc.scenes[ind] = props.test(Actor::gpReflScene);
			break;

		case osViewDepth:
		{
			bool res = props.test(Actor::gpColor) && order == Actor::goDefault && lighting != Actor::glRefr;
			resDesc.scenes[ind] = res;
			break;
		}

		case osShadowCast:
			resDesc.scenes[ind] = props.test(Actor::gpShadowCast);
			break;

		case osShadowMapp:
			resDesc.scenes[ind] = props.test(Actor::gpShadowApp) && props.test(Actor::gpColor);
			break;

		case osColorPix:
			resDesc.scenes[ind] = props.test(Actor::gpColor) && lighting == Actor::glPix;

			//Пока типы порядка отличный по умолчанию не поддерживаются
			LSL_ASSERT(!(resDesc.scenes[ind] && order != Actor::goDefault));
			break;

		case osColorRefl:
			resDesc.scenes[ind] = props.test(Actor::gpColor) && lighting == Actor::glRefl;

			//Пока типы порядка отличный по умолчанию не поддерживаются
			LSL_ASSERT(!(resDesc.scenes[ind] && order != Actor::goDefault));
			break;

		case osColorBump:
			resDesc.scenes[ind] = props.test(Actor::gpColor) && lighting == Actor::glBump;

			//Пока типы порядка отличный по умолчанию не поддерживаются
			LSL_ASSERT(!(resDesc.scenes[ind] && order != Actor::goDefault));
			break;

		case osColorRefr:
			resDesc.scenes[ind] = props.test(Actor::gpColor) && lighting == Actor::glRefr;

			//Пока типы порядка отличный по умолчанию не поддерживаются
			LSL_ASSERT(!(resDesc.scenes[ind] && order != Actor::goDefault));
			break;

		case osColorPlanarRefl:
			resDesc.scenes[ind] = props.test(Actor::gpColor) && lighting == Actor::glPlanarRefl;

			//Пока типы порядка отличный по умолчанию не поддерживаются
			LSL_ASSERT(!(resDesc.scenes[ind] && order != Actor::goDefault));
			break;

		case osColorDef:
			resDesc.scenes[ind] = props.test(Actor::gpColor) && order == Actor::goDefault && lighting == Actor::glStd;
			break;

		case osColorEffect:
			resDesc.scenes[ind] = props.test(Actor::gpColor) && order == Actor::goEffect;
			break;

		case osColorOpacity:
			resDesc.scenes[ind] = props.test(Actor::gpColor) && order == Actor::goOpacity;
			break;

		case osColorCullOpacity:
			resDesc.scenes[ind] = props.test(Actor::gpColor) && props.test(Actor::gpCullOpacity);

			//Пока planar refl не поддерживаются
			LSL_ASSERT(!(resDesc.scenes[ind] && lighting == Actor::glPlanarRefl));
			break;

		case osColorLast:
			resDesc.scenes[ind] = props.test(Actor::gpColor) && order == Actor::goLast;
			break;
		}
	}
}

void GraphManager::InsertActor(graph::Actor* value)
{
	graph::ActorManager::UserDesc desc(cOctreeSceneEnd);
	PrepareActor(value, desc); 

	_actorManager->InsertActor(desc);
}

void GraphManager::RemoveActor(graph::Actor* value)
{
	_actorManager->RemoveActor(value);
}

bool LineCastIntersPlane(const D3DXVECTOR3& rayStart, const D3DXVECTOR3& rayVec, const D3DXPLANE& plane, float& outT)
{
	const float EPSILON = 1.0e-10f;

	float d = D3DXPlaneDotNormal(&plane, &rayVec);	
	if (abs(d) > EPSILON)
	{
		outT = -D3DXPlaneDotCoord(&plane, &rayStart) / d;
		return true;
	}
	return false;
}

unsigned PlaneBBIntersect(const BoundBox& bb, const D3DXPLANE& plane, D3DXVECTOR3 points[])
{
	//конечные вершины ребер для каждого вертекса
	const int lines[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};

	unsigned res = 0;

	for (int i = 0; i < 12; ++i)
	{
		D3DXVECTOR3 v1 = bb.v[lines[i][0]];
		D3DXVECTOR3 v2 = bb.v[lines[i][1]];

		D3DXVECTOR3 vec = v2 - v1;
		float vec3Len = D3DXVec3Length(&vec);
		D3DXVec3Normalize(&vec, &vec);
		float dist;
		//есть пересечение
		if (LineCastIntersPlane(v1, vec, plane, dist) && dist > 0.0f && dist < vec3Len)
		{
			points[res] = v1 + vec * dist;
			//больше 4-х точек не может быть
			if (++res > 3)
				return res;
		}
	}

	return res;
}

unsigned PlaneAABBIntersect(const AABB& aabb, const D3DXPLANE& plane, D3DXVECTOR3 points[])
{
	BoundBox bb(aabb);

	return PlaneBBIntersect(bb, plane, points);
}

bool ComputeZBounds(graph::Engine& engine, const graph::CameraCI& camera, const AABB& aabb, float& minZ, float& maxZ)
{
	bool res = false;

	D3DXPLANE posNearPlane;
	D3DXPlaneFromPointNormal(&posNearPlane, &camera.GetDesc().pos,  &camera.GetDesc().dir);

	D3DXVECTOR3 rayVec[4] = {D3DXVECTOR3(-1.0f, -1.0f, 1.0f), D3DXVECTOR3(1.0f, -1.0f, 1.0f), D3DXVECTOR3(-1.0f, 1.0f, 1.0f), D3DXVECTOR3(1.0f, 1.0f, 1.0f)};
	D3DXVECTOR3 rayPos[4] = {D3DXVECTOR3(-1.0f, -1.0f, 0.0f), D3DXVECTOR3(1.0f, -1.0f, 0.0f), D3DXVECTOR3(-1.0f, 1.0f, 0.0f), D3DXVECTOR3(1.0f, 1.0f, 0.0f)};
	
	for (int i = 0; i < 4; ++i)
	{
		D3DXVec3TransformCoord(&rayVec[i], &rayVec[i], &camera.GetInvViewProj());
		D3DXVec3TransformCoord(&rayPos[i], &rayPos[i], &camera.GetInvViewProj());
		D3DXVec3Normalize(&rayVec[i], &(rayVec[i] - rayPos[i]));

		float tNear, tFar;
		if (aabb.LineCastIntersect(rayPos[i], rayVec[i], tNear, tFar))
		{
			tNear = D3DXPlaneDotCoord(&posNearPlane, &(rayPos[i] + rayVec[i] * tNear));
			tFar = D3DXPlaneDotCoord(&posNearPlane, &(rayPos[i] + rayVec[i] * tFar));

			if (tNear < minZ || !res)
				minZ = tNear;			
			if (tFar > maxZ || !res)			
				maxZ = tFar;

			res = true;
		}
	}

	return res;
}

void GraphManager::RenderDebug()
{
	/*static graph::Camera* oldCamera = 0;
	if (_camera->GetStyle() == graph::csOrtho)
		oldCamera = _camera;
	if (oldCamera)
	{
		float minZ, maxZ;
		ComputeZBounds(*_engine, oldCamera->GetContextInfo(), _actorManager->GetWorldAABB(), minZ, maxZ);
		oldCamera->SetNear(std::max(minZ, 1.0f));
		oldCamera->SetFar(std::max(maxZ, 0.0f));
		//oldCamera->SetPos();

		oldCamera->Render(*_engine);
	}
	_actorManager->RenderDebug(*_engine);*/

	if (_scRenderTexRef > 0)
	{
		_engine->GetContext().SetTexture(0, _scRenderTex->GetRT()->GetTex());
		DrawScreenQuad(*_engine, D3DXVECTOR4(0.0f, 0.0f, 0.2f, 0.2f));
	}
	/*if (_planarReflRender)
	{
		_engine->GetContext().SetTexture(0, _planarReflRender->GetRT()->GetTex());
		DrawScreenQuad(*_engine, D3DXVECTOR4(0.0f, 0.0f, 0.4f, 0.4f));
	}*/
	if (_scDepthMapRef)
	{
		_engine->GetContext().SetTexture(0, _scDepthMap->GetRT()->GetTex());
		DrawScreenQuad(*_engine, D3DXVECTOR4(0.2f, 0.0f, 0.4f, 0.2f));
	}
	/*if (_sunShaft)
	{
		_engine->GetContext().SetTexture(0, _sunShaft->_blurTex[0].GetTex());
		DrawScreenQuad(*_engine, D3DXVECTOR4(0.0f, 0.2f, 0.2f, 0.4f));
	}*/
	/*if (_bloomEff)
	{
		_engine->GetContext().SetTexture(0, _bloomEff->GetRT()->GetTex());
		DrawScreenQuad(*_engine, D3DXVECTOR4(0.0f, 0.4f, 0.2f, 0.6f));
	}
	if (_hdrEff)
	{
		_engine->GetContext().SetTexture(0, _hdrEff->GetRT()->GetTex());
		DrawScreenQuad(*_engine, D3DXVECTOR4(0.0f, 0.6f, 0.2f, 0.8f));
	}*/
	
	/*if (_shadowRef)
	{
		unsigned i = 0;
		for (LightList::iterator iter = _lightList.begin(); iter != _lightList.end(); ++iter, ++i)
			if ((*iter)->GetShadowMap())
			{
				for (unsigned j = 0; j < (*iter)->GetShadowMap()->_shadowVec.size(); ++j)
				{
					_engine->GetContext().SetTexture(0, (*iter)->GetShadowMap()->_shadowVec[i]->GetTex());
					DrawScreenQuad(*_engine, D3DXVECTOR4(0.0f, 0.2f * i, 0.2f,  0.2f * (i + 1)));
					++i;
				}

				_engine->GetContext().SetTexture(0, (*iter)->GetShadowMap()->GetRT()->GetTex());
				DrawScreenQuad(*_engine, D3DXVECTOR4(0.0f, 0.2f * i, 0.2f,  0.2f * (i + 1)));
				break;
			}
	}*/

	/*if (_pixelLightRef > 0)
	{
		_engine->GetContext().SetTexture(0, _pixelLightMap->GetRT()->GetTex());
		DrawScreenQuad(*_engine, D3DXVECTOR4(0.2f, 0.8f, 0.4f, 1.0f));
	}*/

	/*if (_cleanScTex)
	{
		_engine->GetContext().SetTexture(0, _cleanScTex->GetRT()->GetTex());
		DrawScreenQuad(*_engine, D3DXVECTOR4(0.2f, 0.8f, 0.4f, 1.0f));
	}*/

	//_engine->GetContext().SetWorldMat(IdentityMatrix);
	//graph::SceneNode::RenderBB(*_engine, _actorManager->GetWorldAABB(), clrRed);

	//_camera->Render(*_engine);
	//for (int i = 0; i < cNumLights; ++i)
	//	_light[i]->Render(*_engine);
}

void GraphManager::OctreeRender(OctreeScene scene, bool ignoreRayUsers, bool planarRefl)
{
	_actorManager->Render(*_engine, scene, ignoreRayUsers);

	switch (scene)
	{
	case osReflWater:
	case osViewCubeMap:
		if (!planarRefl && _skyBoxRef)
			_skyBox->Render(*_engine);
		break;
	}
}

bool GraphManager::RenderWithShader(OctreeScene scene, bool ignoreRayUsers, graph::Shader* shader, bool checkLight, bool defRender)
{
	if (shader && (!checkLight || !_engine->GetContext().GetLights().empty()))
	{
		shader->Apply(*_engine);
		OctreeRender(scene, ignoreRayUsers);
		shader->UnApply(*_engine);

		return true;
	}
	else if (defRender)
	{
		OctreeRender(scene, ignoreRayUsers);
		return true;
	}
	else
		return false;
}

void GraphManager::RenderPlanarReflScene(graph::CameraCI& camera)
{
	typedef graph::RenderTarget<graph::Tex2DResource>::RtFlags RtFlags;
	typedef graph::ActorManager::ActorList ActorList;

	struct PlanarNode
	{
		graph::ActorManager::Planar planar;
		ActorList actors;
	};

	typedef std::map<unsigned, PlanarNode> PlanarNodes;

	if (_planarReflRender)
	{
		ActorList tracks;
		_actorManager->Culling(osColorPlanarRefl, &camera, true, tracks);

		PlanarNodes planarNodes;

		for (ActorList::const_iterator iter = tracks.begin(); iter != tracks.end(); ++iter)
		{
			graph::Actor* actor = *iter;
			const graph::ActorManager::Planar& planar = _actorManager->GetPlanar(actor);

			PlanarNode& node = planarNodes[planar.id];
			node.planar = planar;
			node.actors.push_back(actor);
		}

		for (PlanarNodes::const_iterator iter = planarNodes.begin(); iter != planarNodes.end(); ++iter)
		{
			IDirect3DSurface9* rTarget;
			_engine->GetDriver().GetDevice()->GetRenderTarget(0, &rTarget);
			IDirect3DSurface9* rDS;
			_engine->GetDriver().GetDevice()->GetDepthStencilSurface(&rDS);

			_planarReflRender->SetReflPlane(iter->second.planar.plane);
			_engine->GetDriver().GetDevice()->SetDepthStencilSurface(_depthSurface->GetSurface());

			_planarReflRender->BeginRT(*_engine, RtFlags(0, D3DCLEAR_ZBUFFER | D3DCLEAR_TARGET));
			OctreeRender(osReflWater, 0, true);
			_planarReflRender->EndRT(*_engine);

			_engine->GetDriver().GetDevice()->SetRenderTarget(0, rTarget);
			rTarget->Release();
			_engine->GetDriver().GetDevice()->SetDepthStencilSurface(rDS);
			rDS->Release();

			_planarReflShader->Apply(*_engine);

			for (ActorList::const_iterator iter2 = iter->second.actors.begin(); iter2 != iter->second.actors.end(); ++iter2)
			{
				/*if (!_engine->GetContext().IsNight())
				{
					D3DXVECTOR3 norm = (*iter2)->vec1();
					D3DXVECTOR3 lightDir = _engine->GetContext().GetLights().front()->GetDesc().dir;
					float dot = D3DXVec3Dot(&norm, &ZVector);

					if (dot < 0.99f)
					{
						D3DXVECTOR3 right1, right2;
						D3DXVec3Cross(&right1, &norm, &ZVector);
						D3DXVec3Cross(&right2, &(-lightDir), &ZVector);

						float b = D3DXVec3Dot(&right1, &right2) > 0;
						dot = 1.0f + 0.25f * (-1.0f + 2.0f * b);

						_planarReflShader->SetTexDiffK(dot);
					}
				}*/

				_engine->GetContext().SetMeshId(static_cast<int>((*iter2)->vec3().x));
				(*iter2)->Render(*_engine);
				_engine->GetContext().SetMeshId(-1);

				//_planarReflShader->SetTexDiffK(1.0f);
			}

			_planarReflShader->UnApply(*_engine);

			if (_pixLightShader)
				_pixLightShader->Apply(*_engine);

			for (ActorList::const_iterator iter2 = iter->second.actors.begin(); iter2 != iter->second.actors.end(); ++iter2)
			{
				_engine->GetContext().SetMeshId(static_cast<int>((*iter2)->vec3().x) | graph::ContextInfo::cMeshIdIgnore);
				(*iter2)->Render(*_engine);
				_engine->GetContext().SetMeshId(-1);
			}

			if (_pixLightShader)
				_pixLightShader->UnApply(*_engine);
		}
	}
	else
		RenderWithShader(osColorPlanarRefl, true, _pixLightShader, true, true);

	/*typedef graph::RenderTarget<graph::Tex2DResource>::RtFlags RtFlags;

	if (_planarReflRender)
	{
		typedef graph::ActorManager::ActorList ActorList;
		ActorList tracks;
		_actorManager->Culling(osColorPlanarRefl, &camera, tracks);

		typedef std::list<D3DXPLANE> Planes;
		Planes planes;

		for (ActorList::const_iterator iter = tracks.begin(); iter != tracks.end(); ++iter)
		{
			graph::Actor* actor = *iter;

			D3DXVECTOR3 norm = actor->vec1();
			float cosb = D3DXVec3Dot(&norm, &ZVector);
			float sinb = sqrt(1.0f - cosb * cosb);
			float height = -actor->vec1().w;
			actor->LocalToWorldNorm(norm, norm);
			D3DXVECTOR3 pos = actor->GetWorldPos();

			D3DXPLANE plane;
			D3DXPlaneFromPointNormal(&plane, &(pos + norm * height), &norm);

			_planarReflRender->SetReflPlane(plane);

			D3DXVECTOR4 border = actor->vec2();
			AABB aabb = actor->GetLocalAABB(true);
			aabb.min.x += border.x;
			aabb.min.y += border.z;
			aabb.min.z = pos.z + (height + aabb.min.x * sinb) / cosb;
			aabb.max.x += border.y;			
			aabb.max.y += border.w;
			aabb.max.z = pos.z + (height + aabb.max.x * sinb) / cosb;
			AABB localAABB = aabb;
			actor->LocalToWorldCoord(aabb.min, aabb.min);
			actor->LocalToWorldCoord(aabb.max, aabb.max);
			D3DXVECTOR3 right = actor->GetWorldRight();
			D3DXVECTOR3 dir = *D3DXVec3Cross(&dir, &right, &norm);
			graph::ReflRender::ClipPlanes clipPlanes;

			D3DXPlaneFromPointNormal(&plane, &aabb.min, &dir);
			clipPlanes.push_back(plane);

			D3DXPlaneFromPointNormal(&plane, &aabb.min, &right);
			clipPlanes.push_back(plane);

			D3DXPlaneFromPointNormal(&plane, &aabb.max, &(-dir));
			clipPlanes.push_back(plane);

			D3DXPlaneFromPointNormal(&plane, &aabb.max, &(-right));
			clipPlanes.push_back(plane);

			_planarReflRender->SetClipPlanes(clipPlanes);

#ifdef PLANAR_REFL_DEBUG
			lsl::SafeDelete(actor->renderBB);
#endif

			_planarReflRender->BeginRT(*_engine, RtFlags(0, iter == tracks.begin() ? D3DCLEAR_ZBUFFER | D3DCLEAR_TARGET : 0));
			OctreeRender(osReflWater, 0, true);
			_planarReflRender->EndRT(*_engine);

#ifdef PLANAR_REFL_DEBUG
			_planarActor->SetPos(D3DXVECTOR3(0, 0, pos.z));
			if (actor->renderBB == NULL)
				actor->renderBB = new AABB(0.0f);
			*actor->renderBB = localAABB;
#endif
		}
	}*/
}

void GraphManager::RenderEnvReflScene(graph::CameraCI& camera)
{
	typedef graph::ActorManager::ActorList ActorList;

	if (_reflShader && !_engine->GetContext().GetLights().empty())
	{
		ActorList actors, bumpActors;

		_actorManager->Culling(osColorRefl, &camera, true, actors);

		if (actors.size() > 0)
		{
			_reflShader->Apply(*_engine);

			for (ActorList::const_iterator iter = actors.begin(); iter != actors.end(); ++iter)
			{			
				(*iter)->Render(*_engine);

				if ((*iter)->vec3().x != 0.0f)
					bumpActors.push_back(*iter);
			}

			_reflShader->UnApply(*_engine);
		}

		if (bumpActors.size() > 0)
		{
			_reflBumpShader->Apply(*_engine);

			for (ActorList::const_iterator iter = bumpActors.begin(); iter != bumpActors.end(); ++iter)
			{
				(*iter)->Render(*_engine);
			}

			_reflBumpShader->UnApply(*_engine);
		}
	}
	else
		OctreeRender(osColorRefl, true);
}

void GraphManager::RenderScenes(graph::CameraCI& camera)
{
	ApplyFog();
	
	if (_pixLightShader)
		_pixLightShader->SetViewPos(_camera->GetPos());
	if (_bumpMapShader)
		_bumpMapShader->SetViewPos(_camera->GetPos());
	if (_reflShader)
		_reflShader->SetViewPos(_camera->GetPos());
	if (_reflBumpShader)
		_reflBumpShader->SetViewPos(_camera->GetPos());
	if (_planarReflShader)
		_planarReflShader->SetViewPos(_camera->GetPos());
	
	RenderWithShader(osColorPix, true, _pixLightShader, true, true);

	if (!RenderWithShader(osColorBump, true, _bumpMapShader, true, false))
		RenderWithShader(osColorBump, true, _pixLightShader, true, true);

	RenderEnvReflScene(camera);

	RenderPlanarReflScene(camera);

	OctreeRender(osColorDef, true);

	_engine->GetContext().SetRenderState(graph::rsMultiSampleAntialias, false);

	if (_skyBoxRef)
		_skyBox->Render(*_engine);
	if (_grassField)
		_grassField->Render(*_engine);
	if (_fogPlane)
		_fogPlane->Render(*_engine);
	if (_waterPlane)
		_waterPlane->Render(*_engine);

	OctreeRender(osColorOpacity, false);
	
	//Рендерим с отключенной запись
	_engine->GetContext().SetRenderState(graph::rsZWriteEnable, false);

	//также и объекты которые попали в отсечение камерой, чтобы не скрывать спецэффекты
	_actorManager->RenderRayUsers(*_engine, 0.3f);
	//Спецэффекты в обязательо порядке с откл. записью
	OctreeRender(osColorEffect, false);
	
	_engine->GetContext().RestoreRenderState(graph::rsZWriteEnable);	

	OctreeRender(osColorLast, false);

	_engine->GetContext().RestoreRenderState(graph::rsMultiSampleAntialias);

	UnApplyFog();
}

void GraphManager::ProgressTime(float deltaTime, bool pause)
{
	_engine->ProgressTime(deltaTime, pause);

	//for (graph::ActorManager::UserList::const_iterator iter = _actorManager->GetUserList().begin(); iter != _actorManager->GetUserList().end(); ++iter)
	//	(*iter)->GetActor()->OnProgress(!pause ? deltaTime : 0.0f);

	graph::ProgressTimer::I().Progress(deltaTime, pause);
}

void GraphManager::AdjustViewOrtho(graph::CameraCI& camera)
{
	if (camera.GetDesc().style == graph::csOrtho)
	{
		float minZ = 0;
		float maxZ = 0;

		bool computeView = false;
		if (_actorManager->IsBuildOctree())	
		{
			AABB aabb = _actorManager->GetWorldAABB();
			aabb.Add(_groundAABB);
			computeView = camera.ComputeZBounds(aabb, minZ, maxZ);
		}

		if (computeView)
		{
			float offZ = std::min(minZ - _camera->GetNear(), 0.0f);
			float maxNear = -offZ + minZ;
			float maxFar = -offZ + std::max(maxZ, _camera->GetFar());

			graph::CameraDesc desc = camera.GetDesc();
			desc.pos = _camera->GetPos() + _camera->GetDir() * offZ;
			desc.nearDist = maxNear;
			desc.farDist = maxFar;
			camera.SetDesc(desc);

			//_camera->SetNear(maxNear);
			//_camera->SetFar(maxFar);
			//_camera->SetPos(desc.pos);
		}
	}
}

void GraphManager::RenderShadow(graph::CameraCI& camera)
{
	typedef graph::RenderTarget<graph::Tex2DResource>::RtFlags RtFlags;

	if (_shadowRef && !_lightList.empty())
	{
		graph::CameraDesc camDesc = camera.GetDesc();		

		bool disableCrop = true;
		for (LightList::iterator iter = _lightList.begin(); iter != _lightList.end(); ++iter)
		{
			if (!(*iter)->GetDesc().shadowDisableCropLight)
			{
				disableCrop = false;
				break;
			}
		}

		//выравниваем, исключая ortho
		if (_actorManager->IsBuildOctree() && camDesc.style != graph::csOrtho && !disableCrop)
			camera.AdjustNearFarPlane(_actorManager->GetWorldAABB(), camDesc.nearDist, camDesc.farDist);

		_engine->GetContext().ApplyCamera(&camera);
		
		for (LightList::iterator iter = _lightList.begin(); iter != _lightList.end(); ++iter)
		{
			LightSrc* light = *iter;

			graph::ShadowMapRender* shadowMap = light->GetShadowMap();
			
			if (light->GetEnable() && shadowMap)
			{
				shadowMap->SetSplitSchemeLambda(camDesc.style == graph::csOrtho ? 0.1f : 0.7f);

				float maxFar = _shadowMaxFar;
				if (camDesc.style == graph::csOrtho)
				{
					D3DXVECTOR3 pos = _camera->GetPos() + _camera->GetDir() * _shadowMaxFar;
					maxFar = std::min(D3DXVec3Length(&(pos - camDesc.pos)), camDesc.farDist);
				}

				shadowMap->SetMaxFar(maxFar);

				if (_actorManager->IsBuildOctree() && !disableCrop)
					light->GetSource()->AdjustNearFarPlane(_actorManager->GetWorldAABB(), light->GetDesc().nearDist, light->GetDesc().farDist);
					
				light->GetSource()->Apply(*_engine, 0);

				shadowMap->iLight = 0;
					
				shadowMap->BeginRT(*_engine, RtFlags(0, D3DCLEAR_ZBUFFER | D3DCLEAR_TARGET, 0xFFFFFFFF));
					
				shadowMap->BeginShadowCaster(*_engine);
				do
				{
					OctreeRender(osShadowCast, 0);
				}
				while (!shadowMap->EndShadowCaster(*_engine, true));

				shadowMap->BeginShadowMapp(*_engine);
				do
				{
					//исключаем отскаемы орто камерой объекты, чтобы тени не обрезались (были видны за ними)
					OctreeRender(osShadowMapp, true);
				}
				while (!shadowMap->EndShadowMapp(*_engine, true));
					
				shadowMap->EndRT(*_engine);
				
				light->GetSource()->UnApply(*_engine, 0);
				light->GetSource()->SetNear(light->GetDesc().nearDist);
				light->GetSource()->SetFar(light->GetDesc().farDist);
			}
		}
		
		_engine->GetContext().UnApplyCamera(&camera);		
		
		//При рендере сцены не требуется оптмизированный фрустум, наоборот он может вызывать артефакты с небом, дождем... 
		if (camDesc.style != graph::csOrtho)
			camera.SetDesc(camDesc);
	}
}

void GraphManager::RenderCubeMap(graph::CameraCI& camera)
{
	typedef graph::RenderTarget<graph::TexCubeResource>::RtFlags RtCubeFlags;

	if (_scRenderCubeTexRef)
	{
		_scRenderCubeTex->SetViewPos(_cubeViewPos);

		_scRenderCubeTex->BeginRT(*_engine, RtCubeFlags(static_cast<D3DCUBEMAP_FACES
>(0), 0, D3DCLEAR_ZBUFFER));
		_scRenderCubeTex->BeginCubeSurf(*_engine);
		do
		{
			OctreeRender(osViewCubeMap, 0);	
		}
		while (!_scRenderCubeTex->EndCubeSurf(*_engine, true));
		_scRenderCubeTex->EndRT(*_engine);
	}
}

void GraphManager::RenderWaterRef(graph::CameraCI& camera)
{
	typedef graph::RenderTarget<graph::Tex2DResource>::RtFlags RtFlags;

	if (_waterPlane)
	{
		_waterPlane->SetViewPos(_camera->GetPos());

		_reflRender->BeginRT(*_engine, RtFlags(0, D3DCLEAR_ZBUFFER));
		OctreeRender(osReflWater, 0);
		_reflRender->EndRT(*_engine);
	}
}

void GraphManager::RenderDepthScene(graph::CameraCI& camera)
{
	typedef graph::RenderTarget<graph::Tex2DResource>::RtFlags RtFlags;

	if (_scDepthMapRef)
	{
		_scDepthMap->SetViewProjMat(_engine->GetContext().GetCamera().GetViewProj());
		//_clearSurf->SetRT(_scDepthMap->GetRT());
		//_clearSurf->SetMode(graph::ClearSurf::cmMaxDepth);
		//_clearSurf->Render(*_engine);
		//_clearSurf->SetRT(0);

		_scDepthMap->BeginRT(*_engine, RtFlags(0, D3DCLEAR_ZBUFFER | D3DCLEAR_TARGET, 0xFFFFFFFF));

		OctreeRender(osViewDepth, 0);

		_scDepthMap->EndRT(*_engine);	
	}
}

void GraphManager::MainThread()
{
	_engine->CheckReset();
}

bool GraphManager::Render(float deltaTime, bool pause)
{
	typedef graph::RenderTarget<graph::Tex2DResource>::RtFlags RtFlags;

	if (_guiMode)
	{
		ProgressTime(deltaTime, pause);

		if (_engine->BeginScene())
		{
			_engine->BeginBackBufOut(D3DCLEAR_ZBUFFER | D3DCLEAR_TARGET, _guiMode ? clrBlack : _fogColor);
			_gui->Draw();
			_engine->EndBackBufOut();
		}

		return _engine->EndScene();
	}

	LSL_ASSERT(_camera);

	const D3DXVECTOR3* rayTarget = _camera->GetStyle() == graph::csOrtho && _actorManager->IsBuildOctree() ? &_orthoTarget.pos : 0;

	if (_engine->BeginScene())
	{
		if (_msRT)
		{
			_engine->GetDriver().GetDevice()->SetDepthStencilSurface(_depthSurface->GetSurface());
		}
		else
		{
			_engine->GetDriver().GetDevice()->SetDepthStencilSurface(_engine->GetDSSurf());
		}

		ProgressTime(deltaTime, pause);

		graph::CameraCI camera;
		camera.SetDesc(_camera->GetContextInfo().GetDesc());

		//выравнивание Ortho
		AdjustViewOrtho(camera);

		if (rayTarget)
			_actorManager->PullRayUsers(osColorCullOpacity, &camera, *rayTarget, _orthoTarget.size);

		_engine->GetContext().SetRenderState(graph::rsMultiSampleAntialias, false);

		//рендер ShadowMap
		RenderShadow(camera);

		_engine->GetContext().ApplyCamera(&camera);
		
		//Осветляем сцену для спец. эффектов (например для четкого кубемапа)
		_engine->GetContext().SetRenderState(graph::rsAmbient, clrWhite);
		//Рендер кубемапы сцены
		RenderCubeMap(camera);
		//Рендер текстуры водных отражений
		RenderWaterRef(camera);		
		//Рендер глубины сцены
		RenderDepthScene(camera);

		_engine->GetContext().RestoreRenderState(graph::rsMultiSampleAntialias);

		//Глобальное фоновое освещение, если включены тени то для объектов он расчитывается через рендер теней в лихт мапе(чтобы скрывать артефакты в тенях)
		_engine->GetContext().SetRenderState(graph::rsAmbient, _sceneAmbient);
		
		//
		unsigned i = 0;
		for (LightList::iterator iter = _lightList.begin(); iter != _lightList.end(); ++iter, ++i)
			if ((*iter)->GetEnable())
				(*iter)->GetSource()->Apply(*_engine, i);

		//Рендер с постпроцессингом
		if (_scRenderTexRef)
		{
			if (_msRT)
			{
				_engine->GetDriver().GetDevice()->SetRenderTarget(0, _msRT->GetSurface());
				_engine->GetDriver().GetDevice()->SetDepthStencilSurface(_msDS->GetSurface());
				_engine->GetDriver().GetDevice()->Clear(0, NULL, D3DCLEAR_ZBUFFER | D3DCLEAR_TARGET, _fogColor, 1.0f, 0);
			}
			else
				_scRenderTex->BeginRT(*_engine, RtFlags(0, D3DCLEAR_ZBUFFER | D3DCLEAR_TARGET, _fogColor));

			_preNodeScene->Render(*_engine);

			RenderScenes(camera);

			if (_cleanScTexRef)
			{
				IDirect3DSurface9* surf1;
				IDirect3DSurface9* surf2;
				
				if (_msRT)
					surf1 = _msRT->GetSurface();
				else
					_scRenderTex->GetRT()->GetTex()->GetSurfaceLevel(0, &surf1);					
				_cleanScTex->GetRT()->GetTex()->GetSurfaceLevel(0, &surf2);

				_engine->GetDriver().GetDevice()->StretchRect(surf1, 0, surf2, 0, D3DTEXF_NONE);
				//_engine->GetDriver().GetDevice()->SetRenderTarget(1, 0);
				//_engine->GetDriver().GetDevice()->UpdateTexture

				if (_msRT == NULL)
					surf1->Release();
				surf2->Release();
			}

			if (_refrEffRef)
			{
				_refrShader->Apply(*_engine);
				OctreeRender(osColorRefr, 0);
				_refrShader->UnApply(*_engine);
			}

			if (_msRT)
			{
				IDirect3DSurface9* surfTex;
				_scRenderTex->GetRT()->GetTex()->GetSurfaceLevel(0, &surfTex);

				_engine->GetDriver().GetDevice()->StretchRect(_msRT->GetSurface(), 0, surfTex, 0, D3DTEXF_NONE);

				surfTex->Release();

				_engine->GetDriver().GetDevice()->SetRenderTarget(0, NULL);
				_engine->GetDriver().GetDevice()->SetDepthStencilSurface(_engine->GetDSSurf());
			}
			else
				_scRenderTex->EndRT(*_engine);

			if (_hdrEff)
				_hdrEff->Render(*_engine);
			if (_bloomEff)
				_bloomEff->Render(*_engine);
			if (_toneMapRef)
				_toneMap->Render(*_engine);
			if (_sunShaft && _engine->GetContext().GetCamera().GetDesc().style == graph::csPerspective)
			{
				//наложение
				D3DXVECTOR3 lightPos = _engine->GetContext().GetLight(0).GetDesc().pos;
				D3DXVECTOR3 radVec;
				D3DXVec3Normalize(&radVec, &(lightPos - _actorManager->GetWorldAABB().GetCenter()));
				D3DXVec3Cross(&radVec, &D3DXVECTOR3(0, 0, 1), &radVec);
				if (D3DXVec3Length(&radVec) < 0.1f)
					radVec = D3DXVECTOR3(1, 0, 0);
				D3DXQUATERNION radQuat;
				D3DXQuaternionRotationAxis(&radQuat, &radVec, D3DX_PI/2.5f);
				Vec3Rotate(D3DXVECTOR3(0, 0, 1), radQuat, radVec);
				radVec = radVec * 1000.0f;

				_sunShaft->SetSunPos(radVec);
				_sunShaft->Render(*_engine);
			}

			_engine->BeginBackBufOut(0, 0);

			_engine->GetContext().SetTexture(0, _scRenderTex->GetRT()->GetTex());
			DrawScreenQuad(*_engine);

			_nodeScene->Render(*_engine);
			//RenderDebug();
			_gui->Draw();			

			_engine->EndBackBufOut();
		}
		//Прямой рендер в задний буффер
		else
		{
			LSL_ASSERT(!_cleanScTexRef);

			_engine->BeginBackBufOut(D3DCLEAR_ZBUFFER | D3DCLEAR_TARGET, _fogColor);

			_preNodeScene->Render(*_engine);

			RenderScenes(camera);
			
			_nodeScene->Render(*_engine);
			_gui->Draw();
			//RenderDebug();

			_engine->EndBackBufOut();
		}

		i = 0;
		for (LightList::iterator iter = _lightList.begin(); iter != _lightList.end(); ++iter, ++i)
			if ((*iter)->GetEnable())
				(*iter)->GetSource()->UnApply(*_engine, i);

		_engine->GetContext().UnApplyCamera(&camera);

		_actorManager->ResetCache();
	}
	return _engine->EndScene();
}

void GraphManager::GPUSync()
{
	_engine->GPUSync();
}

bool GraphManager::IsSyncSupported()
{
	return _engine->IsSyncSupported();
}

bool GraphManager::Present()
{
	return _engine->Present();
}

void GraphManager::Reset(HWND window, lsl::Point resolution, bool fullScreen, unsigned multisampling, bool resetDevice)
{
	_engine->Reset(window, resolution, fullScreen, multisampling, resetDevice);
}

void GraphManager::Reset(HWND window, lsl::Point resolution, bool fullScreen, bool resetDevice)
{
	Reset(window, resolution, fullScreen, _multisampling, resetDevice);
}

void GraphManager::Reset(unsigned multisamling, bool resetDevice)
{
	Reset(_engine->GetParams().hDeviceWindow, lsl::Point(_engine->GetParams().BackBufferWidth, _engine->GetParams().BackBufferHeight), !_engine->GetParams().Windowed, multisamling, resetDevice);
}

void GraphManager::InsertPreNode(graph::BaseSceneNode* value)
{
	_preNodeScene->InsertObject(value);
}

void GraphManager::RemovePreNode(graph::BaseSceneNode* value)
{
	_preNodeScene->RemoveObject(value);
}

void GraphManager::InsertScNode(graph::BaseSceneNode* value)
{
	_nodeScene->InsertObject(value);
}

void GraphManager::RemoveScNode(graph::BaseSceneNode* value)
{
	_nodeScene->RemoveObject(value);
}

GraphManager::LightSrc* GraphManager::AddLight(const LightDesc& desc)
{
	LightSrc* light = new LightSrc(desc);
	light->AddRef();

	_lightList.push_back(light);

	SetLightShadow(light);

	return light;
}

void GraphManager::DelLight(LightSrc* value)
{
	LSL_ASSERT(_lightList.Find(value) != _lightList.end());

	FreeLightShadow(value);

	_lightList.Remove(value);
	value->Release();
	delete value;
}

void GraphManager::ClearLightList()
{
	while (!_lightList.empty())
		DelLight(*_lightList.begin());
}

void GraphManager::SetLight(LightSrc* light, const LightDesc& desc)
{
	SetLightShadow(light);
	light->SetDesc(desc);
}

const GraphManager::LightList& GraphManager::GetLightList() const
{
	return _lightList;
}

void GraphManager::BuildOctree()
{
	AABB aabb;
	bool setAABB = false;

	for (graph::ActorManager::UserList::const_iterator iter = _actorManager->GetUserList().begin(); iter != _actorManager->GetUserList().end(); ++iter)	
		if ((*iter)->GetActor() != _waterPlaneActor && (*iter)->GetActor() != _grassPlane && (*iter)->GetActor() != _fogPlaneActor)
		{
			if (!setAABB)
			{
				aabb = (*iter)->GetAABB();
				setAABB = true;
			}
			else
				aabb.Add((*iter)->GetAABB());

			D3DXVECTOR2 texDiffK = D3DXVECTOR2((*iter)->GetActor()->vec3().y, (*iter)->GetActor()->vec3().z);

			if ((abs(texDiffK.x) + abs(texDiffK.y)) > 0.0f && _lightList.size() > 0 && _lightList.front()->GetSource()->GetType() == D3DLIGHT_DIRECTIONAL)
			{	
				D3DXVECTOR3 norm = (*iter)->GetActor()->vec1();
				D3DXVECTOR3 lightDir = _lightList.front()->GetSource()->GetDir();
				float dot = D3DXVec3Dot(&norm, &ZVector);

				if (dot < 0.99f)
				{
					(*iter)->GetActor()->LocalToWorldNorm(norm, norm);
					D3DXVECTOR3 right = (*iter)->GetActor()->GetWorldRight();
					D3DXVECTOR3 binormal;
					D3DXVec3Cross(&binormal, &right, &ZVector);

					float d1 = D3DXVec3Dot(&binormal, &norm);
					float d2 = D3DXVec3Dot(&binormal, &(-lightDir));

					int b = (d1 > 0.0f) == (d2 > 0.0f);
					dot = 1.0f + texDiffK[b];
				}
				else
					dot = 1.0f;

				(*iter)->GetActor()->texDiffK(dot);
			}
		}

	_actorManager->RebuildOctree(aabb);
	_actorManager->BuildPlanar(osColorPlanarRefl);

	_groundAABB = AABB(D3DXVECTOR3(300.0f + aabb.GetSizes().x, 300.0f + aabb.GetSizes().y, 0.0f));
	_groundAABB.Offset(D3DXVECTOR3(aabb.GetCenter().x, aabb.GetCenter().y, 0.0f));

	UpdateWaterPlane();
	UpdateGrassPlane();
	UpdateFogPlane();
}

D3DXVECTOR3 GraphManager::ScreenToWorld(const lsl::Point& coord, const float z)
{
	LSL_ASSERT(_camera);

	//Получаем размеры окна куда выводится рендер
	float width = static_cast<float>(GetWndRect().Width());
	float height = static_cast<float>(GetWndRect().Height());

	D3DXVECTOR3 screenVec(coord.x / width, coord.y / height, 0);
	//Приводим к диапазону [-1, 1]
	screenVec = screenVec * 2 - IdentityVector;
	screenVec.z = z;
	//Ось Y у экрана и у заднего буфера(или иначе говоря экранной D3D поверхности) не совпадают
	screenVec.y = -screenVec.y;

	 //Переводим в мировое пространство(домножая на инв. матрицу), что соотв. точке на near плоскости камеры
	D3DXVec3TransformCoord(&screenVec, &screenVec, &_camera->GetContextInfo().GetInvViewProj());

	return screenVec;




	/*//Алгоритм с импользованием D3DXVec3Unproject
	D3DVIEWPORT9 viewPort;
	_engine->GetDriver().GetDevice()->GetViewport(&viewPort);
	float width = static_cast<float>(GetWndWidth());
	float height = static_cast<float>(GetWndHeight());

	D3DXVECTOR3 screenVec(coord.x / width * viewPort.Width, coord.y / height * viewPort.Height, z);
	
	D3DXVec3Unproject(&screenVec, &screenVec, &viewPort, &_camera->GetContextInfo().GetProjMat(),  &_camera->GetContextInfo().GetViewMat(), &IdentityMatrix);

	return screenVec;*/
}

lsl::Point GraphManager::WorldToScreen(const D3DXVECTOR3& coord)
{
	return lsl::Point(0, 0);
}

void GraphManager::ScreenToRay(const lsl::Point& coord, D3DXVECTOR3& rayStart, D3DXVECTOR3& rayVec)
{
	LSL_ASSERT(_camera);

	rayStart = ScreenToWorld(coord, 0);
	rayVec = ScreenToWorld(coord, 1) - rayStart;
	D3DXVec3Normalize(&rayVec, &rayVec);
}

bool GraphManager::ScreenPixelRayCastWithPlaneXY(const lsl::Point& coord, D3DXVECTOR3& outVec)
{
	D3DXVECTOR3 rayStart;
	D3DXVECTOR3 rayVec;
	ScreenToRay(coord, rayStart, rayVec);

	return RayCastIntersectPlane(rayStart, rayVec, ZPlane, outVec);
}

graph::Engine& GraphManager::GetEngine()
{
	return *_engine;
}

gui::Manager& GraphManager::GetGUI()
{
	return *_gui;
}

Rect GraphManager::GetWndRect() const
{
	RECT rc;
	::GetClientRect(_engine->GetParams().hDeviceWindow, &rc);
	return Rect(rc.left, rc.top, rc.right, rc.bottom);
}

bool GraphManager::GetGraphOption(GraphOption option) const
{
	return _graphOptions[option];
}

GraphManager::GraphQuality GraphManager::GetGraphQuality(GraphOption option) const
{
	return _graphQuality[option];
}

void GraphManager::SetGraphOption(GraphOption option, bool value, GraphQuality quality)
{
	if (_graphOptions[option] != value)
	{
		LSL_LOG(lsl::StrFmt("GraphManager::SetGraphOption option=%s enable=%d quality=%s", cGraphOptionStr[option].c_str(), value, cGraphQualityStr[quality].c_str()));

		if (value && !IsGraphOptionSupported(option, quality))
		{
			LSL_LOG("GraphManager::SetGraphOption not supported");
			return;
		}

		_graphOptions[option] = value;
		_graphQuality[option] = quality;

		switch (option)
		{
		case goSkyBox:
			value ? InitSkyBox() : FreeSkyBox();
			break;

		case goWater:
			value ? InitWaterPlane() : FreeWaterPlane();
			break;

		case goSunShaft:
			value ? InitSunShaft() : FreeSunShaft();
			break;

		case goBloom:
			value ? InitBloomEff() : FreeBloomEff();
			break;

		case goHDR:
			value ? InitHDREff() : FreeHDREff();
			break;

		case goShadow:			
			value ? InitShadowMap() : FreeShadowMap();
			break;

		case goGrassField:
			value ? InitGrassField() : FreeGrassField();
			break;

		case goRefl:			
			value ? InitRefl() : FreeRefl();
			break;
			
		case goTrueRefl:			
			value ? InitTrueRefl() : FreeTrueRefl();
			break;

		case goPlanarRefl:
			value ? InitPlanarRefl() : FreePlanarRefl();
			break;

		case goPixelLighting:
			value ? InitPixLight() : FreePixLight();			
			break;

		case goBumpMap:
			value ? InitBumpMap() : FreeBumpMap();
			break;

		case goRefr:
			value ? InitRefrEff() : FreeRefrEff();
			break;

		case goFog:
			value ? InitFog() : FreeFog();
			break;

		case goPlaneFog:
			value ? InitPlaneFog("Data\\Effect\\clouds.dds", D3DXVECTOR2(50.0f, 50.0f), 0.02f, quality) : FreePlaneFog();
			break;

		case goMagma:
			value ? InitPlaneFog("Data\\Effect\\magma.dds", D3DXVECTOR2(25.0f, 25.0f), 0.01f, quality) : FreePlaneFog();
			break;
		}
	}
	else if (value && _graphQuality[option] != quality)
	{
		SetGraphOption(option, false);
		SetGraphOption(option, true, quality);
	}
}

void GraphManager::SetGraphOption(GraphOption option, bool value)
{
	SetGraphOption(option, value, _graphQuality[option]);
}

bool GraphManager::IsGraphOptionSupported(GraphOption option, GraphQuality quality) const
{
	D3DCAPS9 caps = _engine->GetDriver().GetCaps();

	bool vs3 = D3DSHADER_VERSION_MAJOR(caps.VertexShaderVersion) >= 3;
	bool ps3 = D3DSHADER_VERSION_MAJOR(caps.PixelShaderVersion) >= 3;
	unsigned texCoordCount = caps.FVFCaps & D3DFVFCAPS_TEXCOORDCOUNTMASK;
	bool tbn = texCoordCount >= 3;

	bool rtTex = _engine->GetDriver().CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, _engine->GetParams().BackBufferFormat) == D3D_OK;
	bool rtCubeTex = _engine->GetDriver().CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DUSAGE_RENDERTARGET, D3DRTYPE_CUBETEXTURE, _engine->GetParams().BackBufferFormat) == D3D_OK;
	bool rtABGR16f = _engine->GetDriver().CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, D3DFMT_A16B16G16R16F) == D3D_OK;
	bool rtR32f = _engine->GetDriver().CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, D3DFMT_R32F) == D3D_OK;

	bool sceneTex = rtABGR16f;
	bool depthTex = vs3 && ps3 && rtR32f;
	bool sceneCubeTex = rtCubeTex;
	bool sceneReflTex = rtTex && caps.MaxUserClipPlanes >= 1;

	switch (option)
	{
	case goSkyBox:
		return vs3 && ps3;

	case goWater:
		return quality == gqLow || (vs3 && ps3 && sceneReflTex);

	case goSunShaft:
		return vs3 && ps3 && sceneTex && depthTex;

	case goBloom:
		return vs3 && ps3 && sceneTex && rtABGR16f;

	case goHDR:
		return vs3 && ps3 && sceneTex && rtABGR16f;

	case goShadow:
		return vs3 && ps3 && rtTex && depthTex && IsGraphOptionSupported(goPixelLighting, quality);

	case goGrassField:
		return quality == gqLow || (vs3 && ps3);

	case goRefl:
		return vs3 && ps3 && IsGraphOptionSupported(goSkyBox, quality);

	case goTrueRefl:
		return vs3 && ps3 && sceneCubeTex && IsGraphOptionSupported(goRefl, quality);

	case goPlanarRefl:
		return vs3 && ps3 && sceneReflTex;

	case goPixelLighting:
		return vs3 && ps3;

	case goBumpMap:
		return vs3 && ps3 && tbn;

	case goRefr:
		return vs3 && ps3 && sceneTex;

	case goFog:
		return true;

	case goPlaneFog:
		return quality == gqLow || (vs3 && ps3 && depthTex);

	case goMagma:
		return quality == gqLow || (vs3 && ps3 && depthTex);
	}

	return false;
}

unsigned GraphManager::GetFiltering() const
{
	return _engine->GetFiltering();
}

void GraphManager::SetFiltering(unsigned value)
{
	LSL_LOG(lsl::StrFmt("GraphManager::SetFiltering value=%d", value).c_str());

	_engine->SetFiltering(value);
}

bool GraphManager::IsFilteringSupported(unsigned value) const
{
	return value <= _engine->GetDriver().GetCaps().MaxAnisotropy;
}

unsigned GraphManager::GetMultisampling() const
{
	return _multisampling;
}

void GraphManager::SetMultisampling(unsigned value)
{
	if (_multisampling != value)
	{
		LSL_LOG(lsl::StrFmt("GraphManager::SetMultisampling value=%d", value).c_str());

		_multisampling = value;
		ApplyMultisampling();		
	}
}

bool GraphManager::IsMultisamplingSupported(unsigned value) const
{
	D3DMULTISAMPLE_TYPE type;
	unsigned quality;
	DWORD qualityLevels;

	_engine->ToMultisampling(value, type, quality);

	HRESULT hr = _engine->GetDriver().CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, _engine->GetParams().BackBufferFormat, false, type, &qualityLevels);

	return hr == D3D_OK && quality <= qualityLevels;
}

const std::string& GraphManager::GetSkyTex() const
{
	return _skyTex;
}

void GraphManager::SetSkyTex(const std::string& value)
{
	if (_skyTex != value)
	{
		_skyTex = value;
		if (_skyBoxRef)
		{
			_skyBoxTex->GetOrCreateData()->SetFileName(_skyTex);
			_skyBoxTex->Reload();
		}
	}
}

const D3DXCOLOR& GraphManager::GetFogColor() const
{
	return _fogColor;
}

void GraphManager::SetFogColor(const D3DXCOLOR& value)
{
	if (_fogColor != value)
	{
		_fogColor = value;

		//if (_fogPlane)
		//	_fogPlane->SetColor(_fogColor);
	}
}

const D3DXCOLOR& GraphManager::GetSceneAmbient()
{
	return _sceneAmbient;
}

void GraphManager::SetSceneAmbient(const D3DXCOLOR& value)
{
	_sceneAmbient = value;
}

float GraphManager::GetFogIntensivity() const
{
	return _fogIntensivity;
}

void GraphManager::SetFogIntensivity(float value)
{
	if (_fogIntensivity != value)	
		_fogIntensivity = value;
}

const D3DXCOLOR& GraphManager::GetCloudColor() const
{
	return _cloudColor;
}

void GraphManager::SetCloudColor(const D3DXCOLOR& value)
{
	_cloudColor = value;

	if (_fogPlane)
		_fogPlane->SetColor(_cloudColor);
	if (_fogPlaneActor)
		_cloudsMat->material.SetDiffuse(_cloudColor);
}

float GraphManager::GetCloudIntensivity() const
{
	return _cloudIntensivity;
}

void GraphManager::SetCloudIntensivity(float value)
{
	_cloudIntensivity = value;	

	if (_fogPlane)
		_fogPlane->SetCloudIntens(_cloudIntensivity);
}

float GraphManager::GetCloudHeight() const
{
	return _cloudHeight;
}

void GraphManager::SetCloudHeight(float value)
{
	_cloudHeight = value;

	if (_fogPlane)
	{
		D3DXVECTOR3 pos = _fogPlane->GetPos();
		pos.z = _cloudHeight;
		_fogPlane->SetPos(pos);
	}

	if (_fogPlaneActor)
	{
		D3DXVECTOR3 pos = _fogPlaneActor->GetPos();
		pos.z = _cloudHeight;
		_fogPlaneActor->SetPos(pos);
	}
}

graph::Camera* GraphManager::GetCamera()
{
	return _camera;
}

void GraphManager::SetCamera(graph::Camera* value)
{
	if (ReplaceRef(_camera, value))
		_camera = value;

	_gui->SetCamera3d(value);
}

const D3DXVECTOR3& GraphManager::GetCubeViewPos() const
{
	return _cubeViewPos;
}

void GraphManager::SetCubeViewPos(const D3DXVECTOR3& value)
{
	_cubeViewPos = value;	
}

const GraphManager::OrthoTarget& GraphManager::GetOrthoTarget() const
{
	return _orthoTarget;
}

void GraphManager::SetOrthoTarget(const D3DXVECTOR3& pos, float size)
{
	_orthoTarget.pos = pos;
	_orthoTarget.size = size;
}

const GraphManager::HDRParams& GraphManager::GetHDRParams() const
{
	return _hdrParams;
}

void GraphManager::SetHDRParams(const HDRParams& value)
{
	_hdrParams = value;
	ApplyHDRParams();
}

GraphManager::ReflMappMode GraphManager::GetReflMappMode() const
{
	return _reflMappMode;
}

void GraphManager::SetReflMappMode(ReflMappMode value)
{
	if (_reflMappMode != value)
	{
		_reflMappMode = value;
		RefreshRefl();
	}
}

float GraphManager::GetShadowMaxFar() const
{
	return _shadowMaxFar;
}

void GraphManager::SetShadowMaxFar(float value)
{
	_shadowMaxFar = value;
}

bool GraphManager::GetGUIMode() const
{
	return _guiMode;
}

void GraphManager::SetGUIMode(bool value)
{
	_guiMode = value;
}

graph::DisplayMode GraphManager::GetDisplayMode() const
{
	return _engine->GetDriver().GetDisplayMode();
}

const graph::DisplayModes& GraphManager::GetDisplayModes() const
{
	return _engine->GetDriver().GetDisplayModes();
}

bool GraphManager::FindNearMode(const lsl::Point& resolution, graph::DisplayMode& mode, float aspect) const
{
	return _engine->GetDriver().FindNearMode(resolution, mode, aspect);
}

graph::DisplayMode GraphManager::GetScreenMode() const
{
	return _engine->GetDriver().GetScreenMode();
}

bool GraphManager::discreteVideoCard() const
{
	return _discreteVideoCard;
}

/*void Test()
{
	HDEVINFO deviceInfoSet;
	GUID *guidDev = (GUID*) &GUID_DEVCLASS_DISPLAY; 
	deviceInfoSet = SetupDiGetClassDevs(guidDev, NULL, NULL, DIGCF_PRESENT | DIGCF_PROFILE);
	TCHAR buffer [4000];
	DWORD buffersize =4000;
	int memberIndex = 0;
	while (true) {
		SP_DEVINFO_DATA deviceInfoData;
		ZeroMemory(&deviceInfoData, sizeof(SP_DEVINFO_DATA));
		deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
		if (SetupDiEnumDeviceInfo(deviceInfoSet, memberIndex, &deviceInfoData) == FALSE) {
			if (GetLastError() == ERROR_NO_MORE_ITEMS) {
				break;
			}
		}
		DWORD nSize=0 ;
		SetupDiGetDeviceInstanceId (deviceInfoSet, &deviceInfoData, buffer, sizeof(buffer), &nSize);
		buffer [nSize] ='\0';

		DEVPROPKEY keys[256];
		SetupDiGetDevicePropertyKeys(deviceInfoSet, &deviceInfoData, keys, 256, &nSize, 0);

		for (int i = 0; i < nSize; ++i)
		{
			DEVPROPTYPE type;
			SetupDiGetDeviceProperty(deviceInfoSet, &deviceInfoData, keys[i], &type, 
		}

		//_tprintf (_T("%s\n"), buffer);
		memberIndex++;
	}
	if (deviceInfoSet) {
		SetupDiDestroyDeviceInfoList(deviceInfoSet);
	}

	return;
}*/

}