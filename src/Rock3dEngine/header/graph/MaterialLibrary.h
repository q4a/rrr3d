#ifndef MATERIAL_LIBRARY
#define MATERIAL_LIBRARY

#include "Shader.h"
#include "Engine.h"
#include "lslCollection.h"

namespace r3d
{

namespace graph
{

class BaseSampler
{
public:
	enum Mode {tmDecal, tmModulate, tmReplace, tmLight, tmDefault};	
	enum Type {st2d = 0, stCube, cSamplerTypeEnd};
	enum Filtering {sfDefault, sfPoint, sfLinear, sfAnisotropic};
private:
	Type _type;
	lsl::AutoRef<TexResource> _tex;
	bool _disabled;	
	Filtering _filtering;
	unsigned _filteringLevel;

	Vec3Range _offset;
	Vec3Range _scale;
	QuatRange _rotate;

	mutable D3DXMATRIX _matrix;
	mutable bool _matChanged;
	mutable float _matFrame;
	mutable bool _defMat;

	void TransformationChanged() const;
	const D3DXMATRIX& GetMatrix(float frame) const;

	void ApplyFiltering();
protected:
	BaseSampler(Type type);

	void SetTex(TexResource* value);
public:
	virtual ~BaseSampler();

	void Apply(Engine& engine, DWORD stage);
	void UnApply(Engine& engine, DWORD stage);

	Type GetType() const;	
	
	TexResource* GetTex();
	const TexResource* GetTex() const;
	IDirect3DBaseTexture9* GetTexSrc();

	void SetColorMode(Mode value);
	void SetAlphaMode(Mode value);

	D3DXCOLOR GetColor() const;
	void SetColor(const D3DXCOLOR& value);

	const Vec3Range& GetOffset() const;
	void SetOffset(const Vec3Range& value);

	const Vec3Range& GetScale() const;
	void SetScale(const Vec3Range& value);

	const QuatRange& GetRotate() const;
	void SetRotate(const QuatRange& value);

	Filtering GetFiltering() const;
	void SetFiltering(Filtering value);

	SamplerStateManager samplerStates;
	TextureStageStateManager stageStates;
};

template<class _Tex> class Sampler: public BaseSampler
{
private:
	typedef BaseSampler _MyBase;
private:
	bool _createTex;
protected:
	Sampler(Type type);
public:	
	virtual ~Sampler();

	_Tex* GetTex();
	const _Tex* GetTex() const;
	_Tex* GetOrCreateTex();
	void SetTex(_Tex* value);
};

class Sampler2d: public Sampler<Tex2DResource>
{
	typedef Sampler<Tex2DResource> _MyBase;
public:
	Sampler2d();

	void BuildAnimByOff(const Vec2Range& texCoord, const Point2U& tileCnt, const D3DXVECTOR2& pixOff = D3DXVECTOR2(0.5f, 0.5f));
	void BuildAnimByTile(const Vec2Range& texCoord, const Point2U& tileCnt, const D3DXVECTOR2& tileSize);

	IDirect3DTexture9* GetTexSrc();

	unsigned GetWidth() const;
	unsigned GetHeight() const;
	unsigned GetFormat() const;

	D3DXVECTOR2 GetSize();
};

class SamplerCube: public Sampler<TexCubeResource>
{
	typedef Sampler<TexCubeResource> _MyBase;
public:
	SamplerCube();

	IDirect3DCubeTexture9* GetTexSrc();

	unsigned GetWidth() const;
	unsigned GetHeight() const;
	unsigned GetFormat() const;
};

typedef std::vector<BaseSampler> BaseSamplers;

class Samplers
{
private:
	typedef std::vector<BaseSampler*> Cont;
	typedef BaseSampler::Type SamplerType;		
public:
	typedef lsl::ClassList<SamplerType, BaseSampler, void> ClassList;
	static ClassList classList;

	static void InitClassList();
private:
	Cont _cont;
public:
	typedef Cont::iterator iterator;
	typedef Cont::const_iterator const_iterator;
public:
	Samplers();
	Samplers(const Samplers& ref);
	~Samplers();

	BaseSampler& Add(SamplerType type);
	Sampler2d& Add2d(Tex2DResource* tex = 0);
	SamplerCube& AddCube(TexCubeResource* tex = 0);

	template<class _Type> _Type& Add()
	{
		ClassList::MyClassInst* classInst = classList.FindByClass<_Type>();
		if (!classInst)
			throw lsl::Error("_Type& Collection::Add()");

		return lsl::StaticCast<_Type&>(Add(classInst->GetKey()));
	}

	iterator Delete(iterator iter);
	iterator Delete(BaseSampler* item);
	void Delete(iterator sIter, iterator eIter);
	void Clear();

	iterator begin();
	iterator end();

	const_iterator begin() const;
	const_iterator end() const;

	BaseSampler& front();
	BaseSampler& back();

	unsigned Size() const;

	BaseSampler& operator[](unsigned index);
	const BaseSampler& operator[](unsigned index) const;

	Samplers& operator=(const Samplers& ref);
};

class Material
{
public:
	enum Blending
	{
		bmOpaque = 0,
		bmTransparency, 
		bmAdditive
	};
	enum AlphaTest
	{
		asNone = 0,
		asLessOrEq
	};

	enum Option {moLighting = 0, moZWrite, moZTest, moIgnoreFog, cOptionEnd};
	typedef lsl::Bitset<cOptionEnd> Options;

	enum FaceCulling {fcCullCW = 0, fcCullCCW, fcNoCull, cFaceCullingEnd};
	enum PolygonMode {pmFill = 0, pmLines, pmPoints, cPolygonModeEnd};
private:
	ColorRange _ambient;
	ColorRange _diffuse;
	ColorRange _emissive;
	ColorRange _specular;
	float _specPower;

	Blending _blending;
	FloatRange _alpha;
	AlphaTest _alphaTest;
	FloatRange _alphaRef;

	Options _options;
	FaceCulling _faceCulling;
	PolygonMode _polygonMode;
	bool _ignoreFog;
	bool _lastIgnFog;

	void ApplyBlending(Blending value);
	void ApplyAlphaTest(AlphaTest mode);
public:
	Material();	

	void Apply(Engine& engine);
	void UnApply(Engine& engine);

	const ColorRange& GetAmbient() const;
	void SetAmbient(const ColorRange& value);
	const ColorRange& GetDiffuse() const;
	void SetDiffuse(const ColorRange& value);	
	const ColorRange& GetEmissive() const;
	void SetEmissive(const ColorRange& value);
	const ColorRange& GetSpecular() const;
	void SetSpecular(const ColorRange& value);
	float GetSpecPower() const;
	void SetSpecPower(float value);

	//
	Blending GetBlending() const;
	void SetBlending(Blending value);
	//значение альфы для блендинга
	//[0..1] --> [абсолютно прозрачно...непрозрачно]
	const FloatRange& GetAlpha() const;
	void SetAlpha(const FloatRange& value);

	AlphaTest GetAlphaTest() const;
	void SetAlphaTest(AlphaTest value);
	//
	FloatRange GetAlphaRef() const;
	void SetAlphaRef(const FloatRange& value);

	bool GetOption(Option option) const;
	void SetOption(Option option, bool value);

	FaceCulling GetFaceCulling() const;
	void SetFaceCulling(FaceCulling value);

	PolygonMode GetPolygonMode() const;
	void SetPolygonMode(PolygonMode value);

	RenderStateManager renderStates;
};

class LibMaterial: public lsl::CollectionItem, public lsl::Serializable
{
private:
	lsl::AutoRef<Shader> _shader;
protected:
	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
public:
	LibMaterial();
	virtual ~LibMaterial();

	//
	void Apply(Engine& engine);
	void UnApply(Engine& engine);

	Shader* GetShader();
	void SetShader(Shader* value);

	graph::LibMaterial& SetAnisoFlt();

	Material material;
	Samplers samplers;
};

class MaterialLibrary: public lsl::ComCollection<LibMaterial, void, void, void>
{
private:
	typedef lsl::ComCollection<LibMaterial, void, void, void> _MyBase;
};

void DrawScreenQuad(Engine& engine,  const D3DXVECTOR4& quadVert = D3DXVECTOR4(0.0f, 0.0f, 1.0f, 1.0f), float fLeftU = 0.0f, float fTopV = 0.0f, float fRightU = 1.0f, float fBottomV = 1.0f, bool disableZBuf = false);




template<class _Tex> Sampler<_Tex>::Sampler(Type type): _MyBase(type), _createTex(false)
{
}

template<class _Tex> Sampler<_Tex>::~Sampler()
{
	SetTex(0);
}

template<class _Tex> _Tex* Sampler<_Tex>::GetTex()
{
	return static_cast<_Tex*>(_MyBase::GetTex());
}

template<class _Tex> const _Tex* Sampler<_Tex>::GetTex() const
{
	return static_cast<const _Tex*>(_MyBase::GetTex());
}

template<class _Tex> _Tex* Sampler<_Tex>::GetOrCreateTex()
{
	if (!GetTex())
	{
		_MyBase::SetTex(new _Tex());
		_createTex = true;
	}

	return GetTex();	
}

template<class _Tex> void Sampler<_Tex>::SetTex(_Tex* value)
{
	if (GetTex() != value)
	{
		if (_createTex)
		{
			_createTex = false;
			_Tex* myTex = GetTex();
			_MyBase::SetTex(0);
			delete myTex;
		}

		_MyBase::SetTex(value);
	}
}

}

}

#endif