#include "stdafx.h"

#include "graph\\MaterialLibrary.h"

namespace r3d
{

namespace graph
{

Samplers::ClassList Samplers::classList;




BaseSampler::BaseSampler(Type type): _type(type), _tex(0), _disabled(false), _offset(NullVector), _scale(IdentityVector), _rotate(NullQuaternion), _matChanged(true), _matFrame(0), _defMat(true), _filtering(sfDefault), _filteringLevel(0)
{
}

BaseSampler::~BaseSampler()
{
	SetTex(0);
}

void BaseSampler::TransformationChanged() const
{
	_matChanged = true;
	_defMat = _offset == NullVector && _scale == IdentityVector && _rotate == NullQuaternion;
}

const D3DXMATRIX& BaseSampler::GetMatrix(float frame) const
{
	if (_matChanged || _matFrame != frame)
	{
		_matFrame = frame;
		_matChanged = false;

		D3DXVECTOR3 offset = _offset.GetValue(_matFrame);
		D3DXVECTOR3 scale = _scale.GetValue(_matFrame);
		D3DXQUATERNION rotate = _rotate.GetValue(_matFrame);

		_defMat = offset == NullVector && scale == IdentityVector && rotate == NullQuaternion;

		_matrix = _defMat ? IdentityMatrix : BuildWorldMatrix(offset, scale, rotate);
	}

	return _matrix;
}

void BaseSampler::ApplyFiltering()
{
	switch (_filtering)
	{
	case sfDefault:
		samplerStates.Restore(ssMinFilter);
		samplerStates.Restore(ssMagFilter);
		samplerStates.Restore(ssMipFilter);
		samplerStates.Restore(ssMaxAnisotropy);
		break;

	case sfPoint:
		samplerStates.Set(ssMinFilter, D3DTEXF_POINT);
		samplerStates.Set(ssMagFilter, D3DTEXF_POINT);
		samplerStates.Set(ssMipFilter, D3DTEXF_POINT);
		samplerStates.Restore(ssMaxAnisotropy);
		break;

	case sfLinear:
		samplerStates.Set(ssMinFilter, D3DTEXF_LINEAR);
		samplerStates.Set(ssMagFilter, D3DTEXF_LINEAR);
		samplerStates.Set(ssMipFilter, D3DTEXF_LINEAR);
		samplerStates.Restore(ssMaxAnisotropy);
		break;

	case sfAnisotropic:
		if (_filteringLevel > 0)
		{
			samplerStates.Set(ssMinFilter, D3DTEXF_ANISOTROPIC);
			samplerStates.Set(ssMagFilter, D3DTEXF_ANISOTROPIC);
			samplerStates.Set(ssMipFilter, D3DTEXF_LINEAR);
			samplerStates.Set(ssMaxAnisotropy, _filteringLevel);
		}
		else
		{
			samplerStates.Set(ssMinFilter, D3DTEXF_LINEAR);
			samplerStates.Set(ssMagFilter, D3DTEXF_LINEAR);
			samplerStates.Set(ssMipFilter, D3DTEXF_LINEAR);
			samplerStates.Restore(ssMaxAnisotropy);
		}
		break;
	}
}

void BaseSampler::SetTex(TexResource* value)
{
	_tex.Reset(value);
}

void BaseSampler::Apply(Engine& engine, DWORD stage)
{
	if (_tex)
	{
		_tex->Init(engine);

		engine.GetContext().SetTexture(stage, GetTexSrc());
	}

	D3DXMATRIX mat = GetMatrix(engine.GetContext().GetFrame());
	if (!_defMat)
	{
		engine.GetContext().PushTextureTransform(stage, mat);
		engine.GetContext().SetTextureStageState(stage, graph::tssTextureTransformFlags, D3DTTFF_COUNT2);
	}

	if (_filteringLevel != engine.GetFiltering())
	{
		_filteringLevel = engine.GetFiltering();
		if (_filtering == sfAnisotropic)
			ApplyFiltering();
	}

	samplerStates.Apply(engine, stage);
	stageStates.Apply(engine, stage);
}

void BaseSampler::UnApply(Engine& engine, DWORD stage)
{
	if (!_defMat)
	{
		engine.GetContext().RestoreTextureStageState(stage, graph::tssTextureTransformFlags);
		engine.GetContext().PopTextureTransform(stage);
	}

	stageStates.UnApply(engine, stage);
	samplerStates.UnApply(engine, stage);

	engine.GetContext().SetTexture(stage, 0);
}

BaseSampler::Type BaseSampler::GetType() const
{
	return _type;
}

TexResource* BaseSampler::GetTex()
{
	return _tex.Pnt();
}

const TexResource* BaseSampler::GetTex() const
{
	return _tex.Pnt();
}

IDirect3DBaseTexture9* BaseSampler::GetTexSrc()
{
	return _tex ? _tex->GetTex() : 0;
}

void BaseSampler::SetColorMode(Mode value)
{
	switch (value)
	{
	case tmDecal:
		stageStates.Set(tssColorOp, D3DTOP_BLENDTEXTUREALPHA);
		stageStates.Set(tssColorArg1, D3DTA_TEXTURE);
		stageStates.Set(tssColorArg2, D3DTA_CONSTANT);
		break;

	case tmModulate:
		stageStates.Set(tssColorOp, D3DTOP_MODULATE);
		stageStates.Set(tssColorArg1, D3DTA_TEXTURE);
		stageStates.Set(tssColorArg2, D3DTA_CURRENT);			
		break;
	
	case tmReplace:
		stageStates.Set(tssColorOp, D3DTOP_SELECTARG1);
		stageStates.Set(tssColorArg1, D3DTA_TEXTURE);
		stageStates.Restore(tssColorArg2);
		break;

	case tmLight:
		stageStates.Set(tssColorOp, D3DTOP_MODULATE);
		stageStates.Set(tssColorArg1, D3DTA_CURRENT);
		stageStates.Set(tssColorArg2, D3DTA_DIFFUSE);
		break;

	case tmDefault:
		stageStates.Restore(tssColorOp);
		stageStates.Restore(tssColorArg1);
		stageStates.Restore(tssColorArg2);
		break;

	default:
		LSL_ASSERT(false);
	}
}

void BaseSampler::SetAlphaMode(Mode value)
{		
	switch (value)
	{
	case tmDecal:
		stageStates.Set(tssAlphaOp, D3DTOP_SELECTARG1);
		stageStates.Set(tssAlphaArg1, D3DTA_DIFFUSE);
		stageStates.Restore(tssAlphaArg2);
		break;

	case tmModulate:
		stageStates.Set(tssAlphaOp, D3DTOP_MODULATE);
		stageStates.Set(tssAlphaArg1, D3DTA_TEXTURE);
		stageStates.Set(tssAlphaArg2, D3DTA_CURRENT);			
		break;
	
	case tmReplace:
		stageStates.Set(tssAlphaOp, D3DTOP_SELECTARG1);
		stageStates.Set(tssAlphaArg1, D3DTA_TEXTURE);
		stageStates.Restore(tssAlphaArg2);
		break;

	case tmDefault:
		stageStates.Restore(tssAlphaOp);
		stageStates.Restore(tssAlphaArg1);
		stageStates.Restore(tssAlphaArg2);
		break;

	default:
		LSL_ASSERT(false);
	}
}

D3DXCOLOR BaseSampler::GetColor() const
{
	return stageStates.Get(tssConstant);
}

void BaseSampler::SetColor(const D3DXCOLOR& value)
{
	stageStates.Set(tssConstant, value);
}

const Vec3Range& BaseSampler::GetOffset() const
{
	return _offset;
}

void BaseSampler::SetOffset(const Vec3Range& value)
{
	_offset = value;
	TransformationChanged();
}

const Vec3Range& BaseSampler::GetScale() const
{
	return _scale;
}

void BaseSampler::SetScale(const Vec3Range& value)
{
	_scale = value;
	TransformationChanged();
}

const QuatRange& BaseSampler::GetRotate() const
{
	return _rotate;
}

void BaseSampler::SetRotate(const QuatRange& value)
{
	_rotate = value;
	TransformationChanged();
}

BaseSampler::Filtering BaseSampler::GetFiltering() const
{
	return _filtering;
}

void BaseSampler::SetFiltering(Filtering value)
{
	_filtering = value;
	ApplyFiltering();
}




Sampler2d::Sampler2d(): _MyBase(st2d)
{
}

void Sampler2d::BuildAnimByOff(const Vec2Range& texCoord, const Point2U& tileCnt, const D3DXVECTOR2& pixOff)
{
	D3DXVECTOR2 fTileCnt(static_cast<float>(tileCnt.x), static_cast<float>(tileCnt.y));

	D3DXVECTOR2 coordOff(pixOff.x / GetWidth(), pixOff.y / GetHeight());
	D3DXVECTOR2 coordLen(texCoord.GetMax() - texCoord.GetMin());
	D3DXVECTOR2 tileSize((coordLen - 2 * coordOff) / fTileCnt);	

	D3DXVECTOR2 stTile(texCoord.GetMin() + coordOff);
	D3DXVECTOR2 endTile(tileSize * (fTileCnt - IdentityVec2) + stTile);

	SetScale(D3DXVECTOR3(tileSize.x, tileSize.y, 1.0f));
	SetOffset(Vec3Range(D3DXVECTOR3(stTile.x, stTile.y, 0), D3DXVECTOR3(endTile.x, endTile.y, 0), Vec3Range::vdVolume, Point3U(tileCnt.x, tileCnt.y, 1)));
}

void Sampler2d::BuildAnimByTile(const Vec2Range& texCoord, const Point2U& tileCnt, const D3DXVECTOR2& tileSize)
{
	D3DXVECTOR2 fTileCnt(static_cast<float>(tileCnt.x), static_cast<float>(tileCnt.y));

	D3DXVECTOR2 coordLen(texCoord.GetMax() - texCoord.GetMin());
	D3DXVECTOR2 imgSize(coordLen.x * GetWidth(), coordLen.y * GetHeight());
	
	D3DXVECTOR2 pixOff = tileSize/2 - ((fTileCnt + IdentityVec2) * tileSize - imgSize) / 2;

	BuildAnimByOff(texCoord, tileCnt, pixOff);
}

IDirect3DTexture9* Sampler2d::GetTexSrc()
{
	return GetTex() ? GetTex()->GetTex() : 0;
}

unsigned Sampler2d::GetWidth() const
{
	return GetTex() ? GetTex()->GetData()->GetWidth() : 0;
}

unsigned Sampler2d::GetHeight() const
{
	return GetTex() ? GetTex()->GetData()->GetHeight() : 0;
}

unsigned Sampler2d::GetFormat() const
{
	return GetTex() ? GetTex()->GetData()->GetFormat() : 0;
}

D3DXVECTOR2 Sampler2d::GetSize()
{
	return GetTex() ? GetTex()->GetSize() : NullVec2;
}




SamplerCube::SamplerCube(): _MyBase(stCube)
{
}

IDirect3DCubeTexture9* SamplerCube::GetTexSrc()
{
	return GetTex() ? GetTex()->GetTex() : 0;
}

unsigned SamplerCube::GetWidth() const
{
	return GetTex()->GetData()->GetWidth();
}

unsigned SamplerCube::GetHeight() const
{
	return GetTex()->GetData()->GetHeight();
}

unsigned SamplerCube::GetFormat() const
{
	return GetTex()->GetData()->GetFormat();
}




Samplers::Samplers()
{
	InitClassList();
}

Samplers::Samplers(const Samplers& ref)
{
	*this = ref;
}

Samplers::~Samplers()
{
	Clear();
}

void Samplers::InitClassList()
{
	static bool initClassList = false;
	
	if (!initClassList)
	{
		initClassList = true;

		classList.Add<Sampler2d>(BaseSampler::st2d);
		classList.Add<SamplerCube>(BaseSampler::stCube);
	}
}

BaseSampler& Samplers::Add(SamplerType type)
{
	BaseSampler* sampler = classList.CreateInst(type);
	_cont.push_back(sampler);

	return *sampler;
}

Sampler2d& Samplers::Add2d(Tex2DResource* tex)
{
	Sampler2d& res = Add<Sampler2d>();
	res.SetTex(tex);

	return res;
}

SamplerCube& Samplers::AddCube(TexCubeResource* tex)
{
	SamplerCube& res = Add<SamplerCube>();
	res.SetTex(tex);

	return res;
}

Samplers::iterator Samplers::Delete(iterator iter)
{
	BaseSampler* sampler = *iter;	
	iterator res = _cont.erase(iter);

	delete sampler;

	return res;
}

Samplers::iterator Samplers::Delete(BaseSampler* item)
{
	return Delete(std::find(_cont.begin(), _cont.end(), item));
}

void Samplers::Delete(iterator sIter, iterator eIter)
{
	for (iterator iter = sIter; iter != eIter; ++iter)
	{
		delete *iter;
	}

	_cont.clear();
}

void Samplers::Clear()
{
	Delete(_cont.begin(), _cont.end());
}

Samplers::iterator Samplers::begin()
{
	return _cont.begin();
}

Samplers::iterator Samplers::end()
{
	return _cont.end();
}

Samplers::const_iterator Samplers::begin() const
{
	return _cont.begin();
}

Samplers::const_iterator Samplers::end() const
{
	return _cont.end();
}

BaseSampler& Samplers::front()
{
	return *_cont.front();
}

BaseSampler& Samplers::back()
{
	return *_cont.back();
}

unsigned Samplers::Size() const
{
	return _cont.size();
}

BaseSampler& Samplers::operator[](unsigned index)
{
	return *_cont[index];
}

const BaseSampler& Samplers::operator[](unsigned index) const
{
	return *_cont[index];
}

Samplers& Samplers::operator=(const Samplers& ref)
{
	Clear();

	for (Samplers::const_iterator iter = ref.begin(); iter != ref.end(); ++iter)
	{
		switch ((*iter)->GetType())
		{
		case BaseSampler::st2d:
		{
			Sampler2d& sampler = Add2d();
			sampler = static_cast<Sampler2d&>(**iter);
			break;
		}

		case BaseSampler::stCube:
		{
			SamplerCube& sampler = AddCube();
			sampler = static_cast<SamplerCube&>(**iter);
			break;
		}

		default:
			LSL_ASSERT(false);

		}
	}

	return *this;
}




Material::Material(): _ambient(clrWhite), _diffuse(clrWhite), _emissive(clrBlack), _specular(clrBlack), _specPower(128), _blending(bmOpaque), _alpha(1.0f), _ignoreFog(false), _lastIgnFog(true), _alphaTest(asNone), _alphaRef(1.0f), _faceCulling(fcCullCW), _polygonMode(pmFill)
{
	SetOption(moLighting, true);
	SetOption(moZWrite, true);
	SetOption(moZTest, true);
	SetOption(moIgnoreFog, false);
}

void Material::ApplyBlending(Blending value)
{
	renderStates.Restore(graph::rsAlphaBlendEnable);
	renderStates.Restore(graph::rsSrcBlend);
	renderStates.Restore(graph::rsDestBlend);

	switch (value)
	{
	case bmOpaque:
		//Nothing
		break;

	case bmTransparency:
		renderStates.Set(graph::rsAlphaBlendEnable, true);
		renderStates.Set(graph::rsSrcBlend, D3DBLEND_SRCALPHA);
		renderStates.Set(graph::rsDestBlend, D3DBLEND_INVSRCALPHA);
		break;

	case bmAdditive:
		renderStates.Set(graph::rsAlphaBlendEnable, true);
		renderStates.Set(graph::rsSrcBlend, D3DBLEND_SRCALPHA);
		renderStates.Set(graph::rsDestBlend, D3DBLEND_ONE);
		break;

	default:
		LSL_ASSERT(false);
	}
}

void Material::ApplyAlphaTest(AlphaTest mode)
{
	renderStates.Restore(graph::rsAlphaTestEnable);
	renderStates.Restore(graph::rsAlphaRef);
	renderStates.Restore(graph::rsAlphaFunc);

	switch (mode)
	{
	case asNone:
		//nothing
		break;

	case asLessOrEq:
		renderStates.Set(graph::rsAlphaTestEnable, true);
		renderStates.Set(graph::rsAlphaFunc, D3DCMP_GREATEREQUAL);
		break;

	default:
		LSL_ASSERT(false);
	}	
}

void Material::Apply(Engine& engine)
{
	MaterialDesc d3dMat;
	float frame = engine.GetContext().GetFrame();
	float alpha = _alpha.GetValue(frame);
	float alphaRef = _alphaRef.GetValue(frame);

	//С освещением
	if (_options.test(moLighting))
	{
		d3dMat.ambient = _ambient.GetValue(frame);
		d3dMat.diffuse = _diffuse.GetValue(frame);
		d3dMat.diffuse.a = alpha;
		d3dMat.emissive = _emissive.GetValue(frame);
		d3dMat.specular = _specular.GetValue(frame);
		d3dMat.power = _specPower;

		if (_specPower > 0 && _specular != clrBlack)
			engine.GetContext().SetRenderState(rsSpecularEnable, true);
	}
	//Без освещения
	else
	{		
		//Direct3d не поддерживает материал отдельно от освещения, поэтому делается через emissive
		d3dMat.ambient = d3dMat.diffuse = d3dMat.specular = clrBlack;
		d3dMat.diffuse.a = alpha;
		d3dMat.emissive = _diffuse.GetValue(frame);
	}

	if (engine.GetContext().IsCullOpacity())	
		d3dMat.diffuse.a *= engine.GetContext().GetCullOpacity();

	engine.GetContext().SetMaterial(d3dMat);

	renderStates.Set(graph::rsAlphaRef, static_cast<DWORD>((1.0f - alphaRef) * 255.0f));
	renderStates.Apply(engine);

	if (_ignoreFog)
	{
		_lastIgnFog = engine.GetContext().GetRenderState(graph::rsFogEnable) > 0 ? true : false;
		engine.GetContext().SetRenderState(graph::rsFogEnable, false);
	}

	if (engine.GetContext().IsCullOpacity())
	{
		engine.GetContext().SetRenderState(graph::rsAlphaBlendEnable, true);
		engine.GetContext().SetRenderState(graph::rsSrcBlend, D3DBLEND_SRCALPHA);
		engine.GetContext().SetRenderState(graph::rsDestBlend, D3DBLEND_INVSRCALPHA);
	}
}

void Material::UnApply(Engine& engine)
{
	if (engine.GetContext().IsCullOpacity())
	{
		engine.GetContext().RestoreRenderState(graph::rsAlphaBlendEnable);
		engine.GetContext().RestoreRenderState(graph::rsSrcBlend);
		engine.GetContext().RestoreRenderState(graph::rsDestBlend);
	}

	if (_ignoreFog)
	{
		engine.GetContext().SetRenderState(graph::rsFogEnable, _lastIgnFog);
	}

	renderStates.UnApply(engine);

	engine.GetContext().RestoreRenderState(rsSpecularEnable);
}

const ColorRange& Material::GetAmbient() const
{
	return _ambient;
}

void Material::SetAmbient(const ColorRange& value)
{
	_ambient = value;
}

const ColorRange& Material::GetDiffuse() const
{
	return _diffuse;
}

void Material::SetDiffuse(const ColorRange& value)
{
	_diffuse = value;
}

const ColorRange& Material::GetEmissive() const
{
	return _emissive;
}

void Material::SetEmissive(const ColorRange& value)
{
	_emissive = value;
}

const ColorRange& Material::GetSpecular() const
{
	return _specular;
}

void Material::SetSpecular(const ColorRange& value)
{
	_specular = value;
}

float Material::GetSpecPower() const
{
	return _specPower;
}

void Material::SetSpecPower(float value)
{
	_specPower = value;
}

Material::Blending Material::GetBlending() const
{
	return _blending;
}

void Material::SetBlending(Blending value)
{
	if (_blending != value)
	{
		_blending = value;
		ApplyBlending(_blending);
	}
}

const FloatRange& Material::GetAlpha() const
{
	return _alpha;
}

void Material::SetAlpha(const FloatRange& value)
{
	_alpha = value;
}

Material::AlphaTest Material::GetAlphaTest() const
{
	return _alphaTest;
}

void Material::SetAlphaTest(AlphaTest value)
{
	if (_alphaTest != value)
	{
		_alphaTest = value;
		ApplyAlphaTest(_alphaTest);
	}
}

FloatRange Material::GetAlphaRef() const
{
	return _alphaRef;
}

void Material::SetAlphaRef(const FloatRange& value)
{
	_alphaRef = value;
}

bool Material::GetOption(Option option) const
{
	return  _options.test(option);
}

void Material::SetOption(Option option, bool value)
{
	if (_options[option] != value)
	{
		_options.set(option, value);

		//renderStates.Restore(graph::rsLighting);
		//renderStates.Restore(graph::rsZWriteEnable);
		//renderStates.Restore(graph::rsZEnable);
		_ignoreFog = false;

		switch (option)
		{
		case moLighting:
			//Direct3d не поддерживает материал отдельно от освещения, поэтому обходной путь
			//renderStates.Set(graph::rsLighting, false);
			break;
			
		case moZWrite:
			renderStates.Set(graph::rsZWriteEnable, value);
			break;
			
		case moZTest:
			renderStates.Set(graph::rsZEnable, value);
			break;
			
		case moIgnoreFog:
			_ignoreFog = value;
			break;

		default:
			LSL_ASSERT(false);
		}
	}
}

Material::FaceCulling Material::GetFaceCulling() const
{
	return _faceCulling;
}

void Material::SetFaceCulling(FaceCulling value)
{
	const D3DCULL cullMode[cFaceCullingEnd] = {D3DCULL_CW, D3DCULL_CCW, D3DCULL_NONE};
	
	if (_faceCulling != value)
	{
		_faceCulling = value;
		
		renderStates.Set(graph::rsFillMode, cullMode[_faceCulling]);
	}
}

Material::PolygonMode Material::GetPolygonMode() const
{
	return _polygonMode;
}

void Material::SetPolygonMode(PolygonMode value)
{
	const D3DFILLMODE fillMode[cPolygonModeEnd] = {D3DFILL_SOLID, D3DFILL_WIREFRAME, D3DFILL_POINT};

	if (_polygonMode != value)
	{
		_polygonMode = value;

		renderStates.Set(graph::rsFillMode, fillMode[_polygonMode]);
	}
}




LibMaterial::LibMaterial(): _shader(0)
{
}

LibMaterial::~LibMaterial()
{
	SetShader(0);
}

void LibMaterial::Save(lsl::SWriter* writer)
{
}

void LibMaterial::Load(lsl::SReader* reader)
{
}

void LibMaterial::Apply(Engine& engine)
{
	if (!engine.GetContext().GetIgnoreMaterial())
	{
		material.Apply(engine);
		for (unsigned i = 0; i < samplers.Size(); ++i)
			samplers[i].Apply(engine, i);

		if (_shader)		
			_shader->Apply(engine);
	}
}

void LibMaterial::UnApply(Engine& engine)
{
	if (!engine.GetContext().GetIgnoreMaterial())
	{
		if (_shader)		
			_shader->UnApply(engine);

		for (unsigned i = 0; i < samplers.Size(); ++i)
			samplers[i].UnApply(engine, i);
		material.UnApply(engine);
	}
}

Shader* LibMaterial::GetShader()
{
	return _shader.Pnt();
}

void LibMaterial::SetShader(Shader* value)
{
	_shader.Reset(value);
}

graph::LibMaterial& LibMaterial::SetAnisoFlt()
{
	for (Samplers::iterator iter = samplers.begin(); iter != samplers.end(); ++iter)
		(*iter)->SetFiltering(BaseSampler::sfAnisotropic);
	return *this;
}




void DrawScreenQuad(Engine& engine, const D3DXVECTOR4& quadVert, float fLeftU, float fTopV, float fRightU, float fBottomV, bool disableZBuf)
{
	D3DSURFACE_DESC surfDesc;
	IDirect3DSurface9* curRTSurf; 
	engine.GetDriver().GetDevice()->GetRenderTarget(0, &curRTSurf);
	curRTSurf->GetDesc(&surfDesc);
	curRTSurf->Release();

	//Закоментированные смещения вносят искажения при совмещении текстур
	float fPosX = quadVert.x * surfDesc.Width - 0.5f;
	float fPosY = quadVert.y * surfDesc.Height - 0.5f;
	float fWidth5 = surfDesc.Width * quadVert.z - 0.5f;
	float fHeight5 = surfDesc.Height * quadVert.w - 0.5f;

	// Draw the quad
	res::ScreenVertex svQuad[4];

	svQuad[0].pos = D3DXVECTOR4(fPosX, fPosY, 0.5f, 1.0f);
	svQuad[0].tex = D3DXVECTOR2(fLeftU, fTopV);

	svQuad[1].pos = D3DXVECTOR4(fWidth5, fPosY, 0.5f, 1.0f);
	svQuad[1].tex = D3DXVECTOR2(fRightU, fTopV);

	svQuad[2].pos = D3DXVECTOR4(fPosX, fHeight5, 0.5f, 1.0f); 
	svQuad[2].tex = D3DXVECTOR2(fLeftU, fBottomV);

	svQuad[3].pos = D3DXVECTOR4(fWidth5, fHeight5, 0.5f, 1.0f);
	svQuad[3].tex = D3DXVECTOR2(fRightU, fBottomV);

	if (!disableZBuf)
	{
		engine.GetContext().SetRenderState(rsZEnable, false);
		engine.GetContext().SetRenderState(rsZWriteEnable, false);
	}

	engine.GetContext().SetRenderState(rsCullMode, D3DCULL_CCW);
	engine.GetDriver().GetDevice()->SetFVF(res::ScreenVertex::fvf);
	engine.BeginDraw();
	engine.GetDriver().GetDevice()->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, svQuad, sizeof(res::ScreenVertex));
	engine.EndDraw(false);
	engine.GetContext().RestoreRenderState(rsCullMode);

	if (!disableZBuf)
	{
		engine.GetContext().RestoreRenderState(rsZWriteEnable);
		engine.GetContext().RestoreRenderState(rsZEnable);
	}
}

}

}