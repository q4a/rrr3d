#include "stdafx.h"

#include "gui\\GUI.h"
#include "graph\\SceneManager.h"

namespace r3d
{

namespace gui
{

Material::Material(): _color(clrWhite), _blending(bmOpaque), _alphaTest(asNone)
{
	_sampler.samplerStates.Set(graph::ssMinFilter, D3DTEXF_POINT);
	_sampler.samplerStates.Set(graph::ssMagFilter, D3DTEXF_POINT);
	_sampler.samplerStates.Set(graph::ssMipFilter, D3DTEXF_NONE);
	_sampler.samplerStates.Set(graph::ssAddressU, D3DTADDRESS_CLAMP);
	_sampler.samplerStates.Set(graph::ssAddressV, D3DTADDRESS_CLAMP);

	_sampler.SetColorMode(graph::Sampler2d::tmModulate);
	_sampler.SetAlphaMode(graph::Sampler2d::tmModulate);
}

D3DXVECTOR2 Material::GetImageSize()
{
	return GetSampler().GetSize();
}

const D3DXCOLOR& Material::GetColor() const
{
	return _color;
}

void Material::SetColor(const D3DXCOLOR& value)
{
	_color = value;
}

float Material::GetAlpha() const
{
	return _color.a;
}

void Material::SetAlpha(float value)
{
	_color.a = value;
}

Material::Blending Material::GetBlending() const
{
	return _blending;
}

void Material::SetBlending(Blending value)
{
	_blending = value;
}

Material::AlphaTest Material::GetAlphaTest() const
{
	return _alphaTest;
}

void Material::SetAlphaTest(AlphaTest value)
{
	_alphaTest = value;
}

graph::Sampler2d& Material::GetSampler()
{
	return _sampler;
}




Graphic::Graphic(Context* context): _context(context), _material(0), _pos(NullVec2), _size(100.0f, 100.0f), _active(true), _alpha(1.0f)
{
	GetOrCreateMaterial();
}

Graphic::~Graphic()
{
	SetMaterial(0);
}

Context& Graphic::GetContext()
{
	return *_context;
}

const D3DXVECTOR2& Graphic::GetPos() const
{
	return _pos;
}

void Graphic::SetPos(const D3DXVECTOR2& value)
{
	_pos = value;
	TransformChanged();
}

const D3DXVECTOR2& Graphic::GetSize() const
{
	return _size;
}

void Graphic::SetSize(const D3DXVECTOR2& value)
{
	_size = value;
	TransformChanged();
}

Material* Graphic::GetOrCreateMaterial()
{
	if (!_material)
	{
		_material = new Material();
		_material->AddRef();
		_createMat = true;
	}
	return _material;
}

Material* Graphic::GetMaterial()
{
	return _material;
}

void Graphic::SetMaterial(Material* value)
{
	if (ReplaceRef(_material, value))
	{
		if (_createMat)
		{
			_createMat = false;
			delete _material;
		}
		_material = value;
	}
}

bool Graphic::GetActive() const
{
	return _active;
}

void Graphic::SetActive(bool value)
{
	_active = value;
}

float Graphic::GetAlpha() const
{
	return _alpha;
}

void Graphic::SetAlpha(float value)
{
	_alpha = value;
}




Plane::Plane(Context* context): _MyBase(context)
{
}

void Plane::Draw()
{
	GetContext().DrawPlane(*this);
}




BaseText::BaseText(Context* context): _MyBase(context), _font(0), _horAlign(haCenter), _vertAlign(vaCenter), _textAABB(0.0f), _textAABBChanged(false), _wordWrap(false), _clipEnable(false), _vScroll(0)
{
}

BaseText::~BaseText()
{
	SetFont(0);
}

void BaseText::BuildTextAABB()
{
	if (_textAABBChanged && _font && _font->IsInit() && _font->GetEngine()->IsReset())
	{
		_textAABBChanged = false;
		_textAABB = AABB2(0.0f);

		DrawText(&_textAABB);
	}
}

void BaseText::TextAABBChanged()
{
	_textAABBChanged = true;
}

void BaseText::Draw()
{
	DrawText(0);
}

void BaseText::TransformChanged()
{
	TextAABBChanged();
}

graph::TextFont* BaseText::GetFont()
{
	return _font;
}

void BaseText::SetFont(graph::TextFont* value)
{
	if (ReplaceRef(_font, value))
	{
		_font = value;
		TextAABBChanged();
	}
}

BaseText::HorAlign BaseText::GetHorAlign() const
{
	return _horAlign;
}

void BaseText::SetHorAlign(HorAlign value)
{
	_horAlign = value;
	TextAABBChanged();
}

BaseText::VertAlign BaseText::GetVertAlign() const
{
	return _vertAlign;
}

void BaseText::SetVertAlign(VertAlign value)
{
	_vertAlign = value;
	TextAABBChanged();
}

bool BaseText::GetWordWrap() const
{
	return _wordWrap;
}

void BaseText::SetWordWrap(bool value)
{
	_wordWrap = value;
	TextAABBChanged();
}

bool BaseText::GetClipEnable() const
{
	return _clipEnable;
}

void BaseText::SetClipEnable(bool value)
{
	_clipEnable = value;
}

const AABB2& BaseText::GetTextAABB()
{
	BuildTextAABB();
	return _textAABB;
}

float BaseText::GetVScroll() const
{
	return _vScroll;
}

void BaseText::SetVScroll(float value)
{
	_vScroll = value;
}




Text::Text(Context* context): _MyBase(context)
{
}

void Text::DrawText(AABB2* aabb)
{
	GetContext().DrawText(*this, aabb);
}

const std::string& Text::GetText() const
{
	return _text;
}

void Text::SetText(const std::string& value)
{
	_text = value;
	TextAABBChanged();
}




TextW::TextW(Context* context): _MyBase(context)
{
}

void TextW::DrawText(AABB2* aabb)
{
	GetContext().DrawText(*this, aabb);
}

const std::wstring& TextW::GetText() const
{
	return _text;
}

void TextW::SetText(const std::wstring& value)
{
	_text = value;
	TextAABBChanged();
}




Graphic3d::Graphic3d(Context* context): _context(context), _parent(0), _pos(NullVector), _rot(NullQuaternion), _scale(IdentityVector), _material(0), _createMat(false), invertCullFace(false), _active(true)
{
	GetOrCreateMaterial();
}

Graphic3d::~Graphic3d()
{
	SetMaterial(0);

	DeleteChildren();

	SetParent(0);
}

void Graphic3d::InsertChild(Graphic3d* value)
{
	LSL_ASSERT(value->_parent == 0);

	_children.push_back(value);
	value->_parent = this;
}

void Graphic3d::RemoveChild(Children::const_iterator iter)
{
	LSL_ASSERT((*iter)->_parent == this);
	
	(*iter)->_parent = 0;
	_children.erase(iter);
}

void Graphic3d::RemoveChild(Graphic3d* value)
{
	RemoveChild(_children.Find(value));
}

void Graphic3d::MoveChildTo(Graphic3d* graphic, Children::const_iterator pos)
{
	Children::iterator newIter = _children.insert(pos, 0);
	Children::iterator oldIter = _children.Find(graphic);
	
	*newIter = graphic;
	RemoveChild(oldIter);
}

void Graphic3d::DeleteChildren()
{
	for (Children::iterator iter = _children.begin(); iter != _children.end(); ++iter)
	{
		(*iter)->_parent = 0;
		GetContext().ReleaseGraphic(*iter);
	}

	_children.clear();
}

Context& Graphic3d::GetContext()
{
	return *_context;
}

const D3DXVECTOR3& Graphic3d::GetPos() const
{
	return _pos;
}

void Graphic3d::SetPos(const D3DXVECTOR3& value)
{
	_pos = value;
	TransformChanged();
}

D3DXQUATERNION Graphic3d::GetRot() const
{
	return _rot;
}

void Graphic3d::SetRot(const D3DXQUATERNION& value)
{
	_rot = value;
	TransformChanged();
}

const D3DXVECTOR3& Graphic3d::GetScale() const
{
	return _scale;
}

void Graphic3d::SetScale(const D3DXVECTOR3& value)
{
	_scale = value;
	TransformChanged();
}

D3DXMATRIX Graphic3d::GetMat()
{
	D3DXMATRIX mat;
	BuildWorldMatrix(_pos, _scale, _rot, mat);

	return mat;
}

D3DXMATRIX Graphic3d::GetWorldMat()
{
	return _parent ? GetMat() * _parent->GetWorldMat() : GetMat();
}

AABB Graphic3d::GetChildAABB()
{
	AABB childAABB = AABB(0.0f);
	for (Children::const_iterator iter = _children.begin(); iter != _children.end(); ++iter)
	{
		Graphic3d* child = *iter;
		AABB aabb = child->GetLocalAABB(true);		
		aabb.Transform(child->GetMat());

		childAABB.Add(aabb);
	}

	return childAABB;
}

AABB Graphic3d::GetLocalAABB(bool includeChild)
{
	AABB aabb = LocalAABB();

	if (includeChild)	
		aabb.Add(GetChildAABB());

	return aabb;
}

AABB Graphic3d::GetWorldAABB(bool includeChild)
{
	AABB aabb = GetLocalAABB(includeChild);
	aabb.Transform(GetWorldMat());

	return aabb;
}

Graphic3d* Graphic3d::GetParent()
{
	return _parent;
}

void Graphic3d::SetParent(Graphic3d* value)
{
	if (_parent != value)
	{
		if (_parent)
			_parent->RemoveChild(value);
		if (value)
			value->InsertChild(value);
	}
}

const Graphic3d::Children& Graphic3d::GetChildren() const
{
	return _children;
}

Material* Graphic3d::GetOrCreateMaterial()
{
	if (!_material)
	{
		_material = new Material();
		_material->AddRef();
		_createMat = true;
	}
	return _material;
}

Material* Graphic3d::GetMaterial()
{
	return _material;
}

void Graphic3d::SetMaterial(Material* value)
{
	if (ReplaceRef(_material, value))
	{
		if (_createMat)
		{
			_createMat = false;
			delete _material;
		}
		_material = value;
	}
}

bool Graphic3d::GetActive() const
{
	return _active;
}

void Graphic3d::SetActive(bool value)
{
	_active = value;
}




Dummy3d::Dummy3d(Context* context): _MyBase(context)
{
}

Dummy3d::~Dummy3d()
{
}

AABB Dummy3d::LocalAABB() const
{
	return AABB(0.0f);
}

void Dummy3d::Draw()
{
	//Nothing
}




VBuf3d::VBuf3d(Context* context): _MyBase(context), _mesh(0), _createMesh(false)
{	
}

VBuf3d::~VBuf3d()
{
	SetMesh(0);
}

AABB VBuf3d::LocalAABB() const
{
	if (_mesh)
		return AABB(_mesh->GetMinPos(), _mesh->GetMaxPos());
	else
		return AABB(0);
}

void VBuf3d::Draw()
{
	GetContext().DrawVBuf3d(*this);
}

graph::VBMesh* VBuf3d::GetOrCreateMesh()
{
	if (!_mesh)
	{
		_mesh = new graph::VBMesh();
		_mesh->AddRef();
		_createMesh = true;
	}
	return _mesh;
}

graph::VBMesh* VBuf3d::GetMesh()
{
	return _mesh;
}

void VBuf3d::SetMesh(graph::VBMesh* value)
{
	if (ReplaceRef(_mesh, value))
	{
		if (_createMesh)
		{
			_createMesh = false;
			delete _mesh;
		}
		_mesh = value;
	}
}




Mesh3d::Mesh3d(Context* context): _MyBase(context), _mesh(0), _meshId(-1)
{
}

Mesh3d::~Mesh3d()
{
	SetMesh(0);
}

AABB Mesh3d::LocalAABB() const
{
	if (_mesh)
		return AABB(_mesh->GetMinPos(), _mesh->GetMaxPos());
	else
		return AABB(0);
}

void Mesh3d::Draw()
{	
	GetContext().DrawMesh3d(*this);
}

graph::IndexedVBMesh* Mesh3d::GetMesh()
{
	return _mesh;
}

void Mesh3d::SetMesh(graph::IndexedVBMesh* value)
{
	if (ReplaceRef(_mesh, value))
	{
		_mesh = value;
		StructureChanged();
	}
}

int Mesh3d::GetMeshId() const
{
	return _meshId;
}

void Mesh3d::SetMeshId(int value)
{
	_meshId = value;
}




Plane3d::Plane3d(Context* context): _MyBase(context), _size(IdentityVector)
{
}

AABB Plane3d::LocalAABB() const
{
	return AABB(D3DXVECTOR3(_size.x, _size.y, 0.0));
}

void Plane3d::Draw()
{
	GetContext().DrawPlane3d(*this);
}

const D3DXVECTOR2& Plane3d::GetSize()
{
	return _size;
}

void Plane3d::SetSize(const D3DXVECTOR2 value)
{
	_size = value;
	StructureChanged();
}




View3d::View3d(Context* context): _MyBase(context), _align(false)
{
	_box = GetContext().CreateDummy3d();
	_box->AddRef();
}

View3d::~View3d()
{
	_box->Release();
	GetContext().ReleaseGraphic(_box);
}

void View3d::Draw()
{
	GetContext().DrawView3d(*this);
}

Graphic3d* View3d::GetBox()
{
	return _box;
}

bool View3d::GetAlign() const
{
	return _align;
}

void View3d::SetAlign(bool value)
{
	_align = value;
}




Context::Context(graph::Engine* engine): _engine(engine), _invertY(false), _vpSize(0, 0)
{	
}

Context::~Context()
{
	DeleteAllGraphics3d();
	DeleteAllGraphics();
}

void Context::InsertGraphic(Graphic* value)
{
	_graphics.push_back(value);
}

void Context::RemoveGraphic(Graphic* value)
{
	_graphics.Remove(value);
}

void Context::DeleteAllGraphics()
{
	for (Graphics::iterator iter = _graphics.begin(); iter != _graphics.end(); ++iter)
		delete *iter;

	_graphics.clear();
}

void Context::InsertGraphic3d(Graphic3d* value)
{
	_graphics3d.push_back(value);
}

void Context::RemoveGraphic3d(Graphic3d* value)
{
	_graphics3d.Remove(value);
}

void Context::DeleteAllGraphics3d()
{
	for (Graphics3d::iterator iter = _graphics3d.begin(); iter != _graphics3d.end(); ++iter)
		delete *iter;

	_graphics3d.clear();
}

void Context::ApplyMaterial(Material& material, float alpha)
{
	graph::MaterialDesc mat;
	mat.ambient = mat.diffuse = mat.specular = clrBlack;
	mat.diffuse.a = material.GetColor().a * alpha;
	mat.emissive = material.GetColor();
	mat.power = 32.0f;

	GetCI().SetMaterial(mat);

	switch (material.GetBlending())
	{
	case Material::bmOpaque:
		//nothing
		break;

	case Material::bmTransparency:
		GetCI().SetRenderState(graph::rsAlphaBlendEnable, true);
		GetCI().SetRenderState(graph::rsSrcBlend, D3DBLEND_SRCALPHA);
		GetCI().SetRenderState(graph::rsDestBlend, D3DBLEND_INVSRCALPHA);
		break;

	case Material::bmAdditive:
		GetCI().SetRenderState(graph::rsAlphaBlendEnable, true);
		GetCI().SetRenderState(graph::rsSrcBlend, D3DBLEND_SRCALPHA);
		GetCI().SetRenderState(graph::rsDestBlend, D3DBLEND_ONE);
		break;

	default:
		LSL_ASSERT(false);
	}

	if (material.GetSampler().GetTex())
		material.GetSampler().Apply(GetEngine(), 0);
}

void Context::UnApplyMaterial(Material& material)
{
	if (material.GetSampler().GetTex())
		material.GetSampler().UnApply(GetEngine(), 0);

	GetCI().RestoreRenderState(graph::rsAlphaBlendEnable);
	GetCI().RestoreRenderState(graph::rsSrcBlend);
	GetCI().RestoreRenderState(graph::rsDestBlend);
}

void Context::BeginDrawGraphic(Graphic& graphic)
{
	if (graphic.GetMaterial())
		ApplyMaterial(*graphic.GetMaterial(), graphic.GetAlpha());

	GetCI().BeginDraw();
}

void Context::EndDrawGraphic(Graphic& graphic)
{
	GetCI().EndDraw(false);

	if (graphic.GetMaterial())
		UnApplyMaterial(*graphic.GetMaterial());
}

void Context::DrawGraphic3d(Graphic3d* graphic, const D3DXMATRIX& worldMat)
{
	if (graphic->GetMaterial())
		ApplyMaterial(*graphic->GetMaterial(), 1.0f);

	GetCI().SetWorldMat(graphic->GetWorldMat() * worldMat);
	if (graphic->invertCullFace)
		GetCI().SetRenderState(graph::rsCullMode, D3DCULL_CCW);

	GetCI().BeginDraw();

	graphic->Draw();

	GetCI().EndDraw(false);

	if (graphic->invertCullFace)
		GetCI().SetRenderState(graph::rsCullMode, D3DCULL_CW);

	if (graphic->GetMaterial())
		UnApplyMaterial(*graphic->GetMaterial());

	for (Graphic3d::Children::const_iterator iter = graphic->GetChildren().begin(); iter != graphic->GetChildren().end(); ++iter)
		if ((*iter)->GetActive())
			DrawGraphic3d(*iter, worldMat);
}

template<class _Text> void Context::DrawBaseText(_Text& text, AABB2* aabb)
{
	if (!text.GetFont())
		return;
	
	/*Plane plane(this);
	plane.SetSize(text.GetSize());
	plane.SetPos(text.GetPos());
	DrawPlane(plane);*/

	text.GetFont()->Init(GetEngine());

	DWORD format = DT_EXPANDTABS;
	//
	format |= text.GetHorAlign() == Text::haLeft ? DT_LEFT : 0;
	format |= text.GetHorAlign() == Text::haCenter ? DT_CENTER : 0;
	format |= text.GetHorAlign() == Text::haRight ? DT_RIGHT : 0;
	//
	format |= text.GetVertAlign() == Text::vaTop ? DT_TOP : 0;
	format |= text.GetVertAlign() == Text::vaCenter ? DT_VCENTER : 0;
	format |= text.GetVertAlign() == Text::vaBottom ? DT_BOTTOM : 0;
	//format |= text.GetVertAlign() == Text::vaScroll ? DT_VCENTER : 0;

	format |= text.GetWordWrap() ? DT_WORDBREAK : 0;
	format |= text.GetClipEnable() ? 0 : DT_NOCLIP;

	D3DXVECTOR2 pos = text.GetPos();
	if (aabb)
	{
		pos += NullVec2;
	}
	else
	{
		pos += D3DXVECTOR2(MatGetPos(GetCI().GetWorldMat()));
		//Оси y не совпадают
		if (_invertY)
			pos.y = GetVPSize().y - (pos.y + text.GetVScroll());
		else
			pos.y = pos.y + text.GetVScroll();
	}	

	D3DXVECTOR2 size = text.GetSize();
	size = size / 2;
	Point left(Floor<int>(pos.x - size.x - 0.5f), Floor<int>(pos.y - size.y - 0.5f));
	Point right(Ceil<int>(pos.x + size.x + 0.5f), Ceil<int>(pos.y + size.y + 0.5f));
	Rect rect(left.x, left.y, right.x, right.y);

	if (aabb)
	{
		text.GetFont()->DrawText(text.GetText(), rect, format | DT_CALCRECT, 0);
		aabb->min = D3DXVECTOR2(static_cast<float>(rect.left), static_cast<float>(rect.top));
		aabb->max = D3DXVECTOR2(static_cast<float>(rect.right), static_cast<float>(rect.bottom));
	}
	else
	{
		BeginDrawGraphic(text);
		text.GetFont()->DrawText(text.GetText(), rect, format, text.GetMaterial() ? text.GetMaterial()->GetColor() : clrWhite);
		EndDrawGraphic(text);
	}
}

graph::Engine& Context::GetEngine()
{
	return *_engine;
}

graph::RenderDriver& Context::GetDriver()
{
	return _engine->GetDriver();
}

graph::ContextInfo& Context::GetCI()
{
	return _engine->GetContext();
}

void Context::BeginDraw()
{
	D3DXVECTOR2 vpSize = GetVPSize();

	graph::CameraDesc desc;
	desc.style = _invertY ? graph::csViewPortInv : graph::csViewPort;
	desc.width = vpSize.x;
	desc.aspect = vpSize.x / vpSize.y;
	_camera.SetDesc(desc);

	GetCI().ApplyCamera(&_camera);
	SetTransform(IdentityMatrix);

	GetCI().SetRenderState(graph::rsZWriteEnable, false);
	GetCI().SetRenderState(graph::rsZEnable, false);

	GetCI().PushFrame(0.0f);
}

void Context::EndDraw()
{
	GetCI().PopFrame();

	GetCI().RestoreRenderState(graph::rsZWriteEnable);
	GetCI().RestoreRenderState(graph::rsZEnable);

	GetCI().UnApplyCamera(&_camera);
}

void Context::SetTransform(const D3DXMATRIX& value)
{
	GetCI().SetWorldMat(value);
}

void Context::DrawPlane(Plane& plane)
{
	D3DXVECTOR3 pos3(Vec3FromVec2(plane.GetPos()));
	D3DXVECTOR2 size = plane.GetSize()/2.0f;

	res::VertexPT planeBuf[4];
	if (_invertY)
	{
		planeBuf[0] = res::VertexPT(D3DXVECTOR3(Ceil<float>(size.x + pos3.x) - 0.5f, Floor<float>(-size.y + pos3.y) - 0.5f, 0), D3DXVECTOR2(1.0f, 1.0f));
		planeBuf[1] = res::VertexPT(D3DXVECTOR3(Ceil<float>(size.x + pos3.x) - 0.5f, Ceil<float>(size.y + pos3.y) - 0.5f, 0), D3DXVECTOR2(1.0f, 0.0f));
		planeBuf[2] = res::VertexPT(D3DXVECTOR3(Floor<float>(-size.x + pos3.x) - 0.5f, Floor<float>(-size.y + pos3.y) - 0.5f, 0), D3DXVECTOR2(0.0f, 1.0f));
		planeBuf[3] = res::VertexPT(D3DXVECTOR3(Floor<float>(-size.x + pos3.x) - 0.5f, Ceil<float>(size.y + pos3.y) - 0.5f, 0), D3DXVECTOR2(0.0f, 0.0f));
	}
	else
	{
		planeBuf[0] = res::VertexPT(D3DXVECTOR3(Ceil<float>(size.x + pos3.x) - 0.5f, Floor<float>(-size.y + pos3.y) - 0.5f, 0), D3DXVECTOR2(1.0f, 0.0f));		
		planeBuf[1] = res::VertexPT(D3DXVECTOR3(Floor<float>(-size.x + pos3.x) - 0.5f, Floor<float>(-size.y + pos3.y) - 0.5f, 0), D3DXVECTOR2(0.0f, 0.0f));
		planeBuf[2] = res::VertexPT(D3DXVECTOR3(Ceil<float>(size.x + pos3.x) - 0.5f, Ceil<float>(size.y + pos3.y) - 0.5f, 0), D3DXVECTOR2(1.0f, 1.0f));		
		planeBuf[3] = res::VertexPT(D3DXVECTOR3(Floor<float>(-size.x + pos3.x) - 0.5f, Ceil<float>(size.y + pos3.y) - 0.5f, 0), D3DXVECTOR2(0.0f, 1.0f));
	};

	BeginDrawGraphic(plane);

	GetDriver().GetDevice()->SetFVF(D3DFVF_XYZ | D3DFVF_TEX1);
	GetDriver().GetDevice()->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, planeBuf, sizeof(res::VertexPT));

	EndDrawGraphic(plane);
}

void Context::DrawText(Text& text, AABB2* aabb)
{
	DrawBaseText(text, aabb);
}

void Context::DrawText(TextW& text, AABB2* aabb)
{
	DrawBaseText(text, aabb);
}

void Context::DrawVBuf3d(VBuf3d& vBuf3d)
{
	if (!vBuf3d.GetMesh())
		return;

	vBuf3d.GetMesh()->Init(GetEngine());

	vBuf3d.GetMesh()->Draw();
}

void Context::DrawMesh3d(Mesh3d& mesh3d)
{
	if (!mesh3d.GetMesh())
		return;

	mesh3d.GetMesh()->Init(GetEngine());
	int meshId = mesh3d.GetMeshId();

	if (meshId < 0)
		mesh3d.GetMesh()->Draw();
	else
		mesh3d.GetMesh()->DrawSubset(meshId);
}

void Context::DrawPlane3d(Plane3d& plane3d)
{
	D3DXVECTOR2 size = plane3d.GetSize();

	const res::VertexPT planeBuf[4] = 
	{
		res::VertexPT(D3DXVECTOR3(size.x, -size.y, 0), D3DXVECTOR2(1.0f, 1.0f)),
		res::VertexPT(D3DXVECTOR3(size.x, size.y, 0), D3DXVECTOR2(1.0f, 0.0f)),
		res::VertexPT(D3DXVECTOR3(-size.x, -size.y, 0), D3DXVECTOR2(0.0f, 1.0f)),
		res::VertexPT(D3DXVECTOR3(-size.x, size.y, 0), D3DXVECTOR2(0.0f, 0.0f))
	};

	GetDriver().GetDevice()->SetFVF(D3DFVF_XYZ | D3DFVF_TEX1);
	GetDriver().GetDevice()->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, planeBuf, sizeof(res::VertexPT));
}

void Context::DrawView3d(View3d& view3d)
{
	//отстраиваем размер относительно локального AABB. Трансформации GetBox() применяются при DrawGraphic3d
	AABB aabb = view3d.GetBox()->GetLocalAABB(true);
	
	/*Plane plane(this);
	plane.SetSize(view3d.GetSize());
	plane.SetPos(view3d.GetPos());
	DrawPlane(plane);*/

	//максимальный осевой размер меша
	float maxScale = D3DXVec3Length(&aabb.GetSizes());
	//с учетом нецентрированности
	if (!view3d.GetAlign())
		maxScale += D3DXVec3Length(&aabb.GetCenter());
	//размер поля в котором он отображается
	D3DXVECTOR3 viewSize = D3DXVECTOR3(view3d.GetSize().x, view3d.GetSize().y, 0.0f);
	//размер по оси z вычисляет с прикидкой
	viewSize.z = (viewSize.x + viewSize.y) / 2.0f;

	//растягиваем меш до размера поля
	D3DXVECTOR3 scale = viewSize / maxScale;
	D3DXVECTOR3 pos = NullVector;
	//центрируем
	if (view3d.GetAlign())
	{
		D3DXVec3TransformCoord(&pos, &aabb.GetCenter(), &view3d.GetBox()->GetMat());
		pos = pos * scale;
	}

	//мировая матрица меша
	D3DXMATRIX worldMat = GetCI().GetWorldMat();
	if (!_invertY)
		worldMat._22 = -worldMat._22;
	D3DXMATRIX localMat;
	BuildWorldMatrix(-pos, scale, NullQuaternion, localMat);

	//
	GetCI().SetRenderState(graph::rsZWriteEnable, true);
	GetCI().SetRenderState(graph::rsZEnable, true);
	GetDriver().GetDevice()->Clear(0, 0, D3DCLEAR_ZBUFFER, 0, 1.0f, 0);

	DrawGraphic3d(view3d.GetBox(), localMat * worldMat);

	GetCI().SetRenderState(graph::rsZWriteEnable, false);
	GetCI().SetRenderState(graph::rsZEnable, false);	

	GetCI().SetWorldMat(worldMat);
}

Plane* Context::CreatePlane()
{
	Plane* res = new Plane(this);
	InsertGraphic(res);
	return res;
}

Text* Context::CreateText()
{
	Text* res = new Text(this);
	InsertGraphic(res);
	return res;
}

TextW* Context::CreateTextW()
{
	TextW* res = new TextW(this);
	InsertGraphic(res);
	return res;
}

Dummy3d* Context::CreateDummy3d()
{
	Dummy3d* graphic = new Dummy3d(this);
	InsertGraphic3d(graphic);
	return graphic;
}

VBuf3d* Context::CreateVBuf3d()
{
	VBuf3d* graphic = new VBuf3d(this);
	InsertGraphic3d(graphic);
	return graphic;
}

Mesh3d* Context::CreateMesh3d()
{
	Mesh3d* graphic = new Mesh3d(this);
	InsertGraphic3d(graphic);
	return graphic;
}

Plane3d* Context::CreatePlane3d()
{
	Plane3d* graphic = new Plane3d(this);
	InsertGraphic3d(graphic);
	return graphic;
}

View3d* Context::CreateView3d()
{
	View3d* graphic = new View3d(this);
	InsertGraphic(graphic);
	return graphic;
}

void Context::ReleaseGraphic(Graphic* value)
{
	RemoveGraphic(value);
	delete value;
}

void Context::ReleaseGraphic(Graphic3d* value)
{
	RemoveGraphic3d(value);
	delete value;
}

bool Context::GetInvertY() const
{
	return _invertY;
}

void Context::SetInvertY(bool value)
{
	_invertY = value;
}

const D3DXVECTOR2& Context::GetVPSize()
{
	return _vpSize;
}

void Context::SetVPSize(const D3DXVECTOR2& value)
{
	_vpSize = value;
}




Widget::Widget(Manager* manager): _manager(manager), _topmostLevel(0), _visible(true), _data(0), _parent(0), _anchor(waNone), _align(waNone), _enabled(true), _pos(NullVec2), _scale(IdentityVec2), _rot(0), _coord3d(false), _pos3d(NullVector), _size(NullVec2), _alignChanged(false), _isMouseDown(false), _isMouseOver(false), _isMouseEnter(false), _focused(false), _disposed(false), _modal(false), _tag(0)
{
	ignorePause(true);

	_matrixChanges.set();
	_aabbChanges.set();

	//_flags.set(wfCathMouseMessages);
	_flags.set(wfMouseOnClick);
}

Widget::~Widget()
{
	SetData(0);
	SetFlag(wfTopmost, false);
	ShowModal(false);

	SetParent(0);

	ClearChildren();
	DeleteAllGraphics();
}

void Widget::Dispose()
{
	_disposed = true;

	ClearEvents();

	SetEvent(0);
	SetData(0);
	SetFlag(wfTopmost, false);
}

void Widget::DoRemoveChild(Widget* child)
{
	child->Release();
	child->_parent = 0;
	child->WorldTransformChanged();
	child->StructureChanged(scWorld);
}

void Widget::BuildMatrix(MatrixChange change) const
{
	if (!_matrixChanges[change])
		return;
	_matrixChanges[change] = false;

	switch (change)
	{
	case mcLocal:
	{
		if (_coord3d)
		{
			_pos = _manager->WorldToView(_pos3d);
			if (_parent)
				_pos = _parent->WorldToLocalCoord(_pos);
		} 
		else if (_anchor != waNone)
		{
			_anchorVP = _manager->GetVPSize();

			switch (_anchor)
			{
			case waLeftTop:
				_pos.x = 0;
				_pos.y = 0;
				break;
			case waTop:
				_pos.x = _anchorVP.x/2;
				_pos.y = 0;
				break;
			case waRightTop:
				_pos.x = _anchorVP.x;
				_pos.y = 0;
				break;
			case waLeft:
				_pos.x = 0;
				_pos.y = _anchorVP.y/2;
				break;
			case waCenter:
				_pos.x = _anchorVP.x/2;
				_pos.y = _anchorVP.y/2;
				break;
			case waRight:
				_pos.x = _anchorVP.x;
				_pos.y = _anchorVP.y/2;
				break;
			case waLeftBottom:
				_pos.x = 0;
				_pos.y = _anchorVP.y;
				break;
			case waBottom:
				_pos.x = _anchorVP.x/2;
				_pos.y = _anchorVP.y;
				break;
			case waRightBottom:
				_pos.x = _anchorVP.x;
				_pos.y = _anchorVP.y;
				break;
			}

			if (_manager->GetInvertY())
				_pos.y = _anchorVP.y - _pos.y;

			if (_parent)
				_pos = _parent->WorldToLocalCoord(_pos);
		}

		D3DXQUATERNION rot;
		D3DXQuaternionRotationAxis(&rot, &ZVector, GetRot());

		D3DXVECTOR3 pos = Vec3FromVec2(GetPos());

		BuildWorldMatrix(pos, D3DXVECTOR3(_scale.x, _scale.y, 1.0f), rot, _matrix[mcLocal]);
		break;
	}

	case mcWorld:
		_matrix[mcWorld] = _parent ? GetMat() * _parent->GetWorldMat() : GetMat();
		break;
		
	case mcInvLocal:
		D3DXMatrixInverse(&_matrix[mcInvLocal], 0, &GetMat());
		break;
		
	case mcInvWorld:
		D3DXMatrixInverse(&_matrix[mcInvWorld], 0, &GetWorldMat());
		break;
	}
}

void Widget::BuildLocalAABB() const
{
	if (_aabbChanges[acLocalAABB])
	{
		_aabbChanges.reset(acLocalAABB);

		D3DXVECTOR2 center = GetAlignPos();
		D3DXVECTOR2 size = GetSize();
		_localAABB = AABB2(center - size/2, center + size/2);
	}
}

void Widget::BuildWorldAABB() const
{
	if (_aabbChanges[acWorldAABB])
	{
		_aabbChanges.reset(acWorldAABB);

		AABB2::Transform(GetLocalAABB(false), GetWorldMat(), _worldAABB);
	}
}

void Widget::BuildChildAABB() const
{
	if (_aabbChanges[acChildAABB])
	{
		_aabbChanges.reset(acChildAABB);

		_childAABB = AABB2(0.0f);
		for (Children::const_iterator iter = _children.begin(); iter != _children.end(); ++iter)
		{
			Widget* child = *iter;
			
			AABB2 childAABB = child->GetLocalAABB(true);
			childAABB.Transform(child->GetMat());
			
			_childAABB.Add(childAABB);
		}
	}
}

void Widget::BuildLocalChildAABB() const
{
	if (_aabbChanges[acLocalChildAABB])
	{
		_aabbChanges.reset(acLocalChildAABB);

		AABB2::Add(GetLocalAABB(false), GetChildAABB(), _localChildAABB);
	}
}

void Widget::BuildWorldChildAABB() const
{
	if (_aabbChanges[acWorldChildAABB])
	{
		_aabbChanges.reset(acWorldChildAABB);
		AABB2::Transform(GetChildAABB(), GetMat(), _worldChildAABB);

		_worldChildAABB.Add(GetWorldAABB(false));
	}
}

void Widget::AABBChanged(StructChange change)
{
	switch (change)
	{
	case scLocal:
		_aabbChanges.set(acLocalAABB);
		_aabbChanges.set(acWorldAABB);
		_aabbChanges.set(acLocalChildAABB);
		_aabbChanges.set(acWorldChildAABB);
		break;
		
	case scWorld:
		_aabbChanges.set(acWorldAABB);
		_aabbChanges.set(acWorldChildAABB);
		break;
		
	case scChild:
		_aabbChanges.set(acChildAABB);
		_aabbChanges.set(acLocalChildAABB);
		_aabbChanges.set(acWorldChildAABB);
		break;
	}
}

bool Widget::ApplyMouseEnter(bool wasReset)
{
	bool mouseEnter = (_focused || _isMouseOver) && _enabled;

	if (_isMouseEnter != mouseEnter)
	{
		_isMouseEnter = mouseEnter;

		if (mouseEnter)
		{
			return OnMouseEnter(_manager->GetMouseMove(this));
		}
		else
		{			
			OnMouseLeave(wasReset);
			return true;
		}
	}

	return false;
}

void Widget::ApplyAlign() const
{
	if (_alignChanged <= 0)
		return;
	_alignChanged = 0;

	if (_parent && IsAligned())
	{
		//AABB в локальных координатах родителя, без учета масштабирования
		AABB2 rect = _parent->GetLocalAABB(false);
		//без учета дочерей, поскольку это ведет к багу при изменении размера SetSize если дочери остаются вне его AABB (возвращается старый AABB вследствии чего изменение размера не происходит)
		AABB2 localRect = GetLocalAABB(false);
		localRect.Transform(GetMat());

		AABB2 newRect = localRect;
		D3DXVECTOR2 newSize = newRect.GetSize();
		if (_flags[wfAlignLeft])
		{
			newRect.min.x = rect.min.x;
			newRect.max.x = newRect.min.x + newSize.x;
			newSize.x = rect.GetSize().x;
		}
		if (_flags[wfAlignRight])
		{
			newRect.max.x = rect.max.x;
			newRect.min.x = newRect.max.x  - newSize.x;
		}
		if (_flags[wfAlignTop])
		{
			newRect.max.y = rect.max.y;
			newRect.min.y = newRect.max.y - newSize.y;
			newSize.y = rect.GetSize().y;
		}
		if (_flags[wfAlignBottom])
		{
			newRect.min.y = rect.min.y;
			newRect.max.y = newRect.min.y + newSize.y;
		}
		
		D3DXVECTOR2 size = newRect.GetSize();
		D3DXVECTOR2 pos = newRect.GetCenter();

		//В виде исключения
		Widget* mThis = const_cast<Widget*>(this);
		mThis->_pos = pos;
		mThis->_size = size;
		_alignChanged = false;

		_alignChanged -= 2;
		mThis->TransformChanged();
		mThis->StructureChanged(scLocal);
	}
}

void Widget::AlignChanged()
{
	++_alignChanged;
}

bool Widget::IsAligned() const
{
	return _flags[wfAlignLeft] || _flags[wfAlignTop] || _flags[wfAlignRight] || _flags[wfAlignBottom];
}

void Widget::TransformChanged()
{
	_matrixChanges.set(mcLocal);
	_matrixChanges.set(mcInvLocal);

	WorldTransformChanged();
	AlignChanged();
}

void Widget::WorldTransformChanged()
{
	if (!_matrixChanges[mcWorld])
	{
		_matrixChanges.set(mcWorld);
		_matrixChanges.set(mcInvWorld);

		AABBChanged(scWorld);
		
		for (Children::iterator iter = _children.begin(); iter != _children.end(); ++iter)		
			(*iter)->WorldTransformChanged();
	}
}

void Widget::StructureChanged(StructChange change)
{
	switch (change)
	{
	case scLocal:
		//В соотвествии с определением StructureChanged, уведомляем дочерние структуры только если произошло действительное изменение состояния acLocalAABB (можно сказать экономим на флагах StructChange об имзенении структуры)
		if (!_aabbChanges[acLocalAABB])
		{
			if (_parent)
				_parent->StructureChanged(scChild);

			for (Children::iterator iter = _children.begin(); iter != _children.end(); ++iter)
				(*iter)->StructureChanged(scWorld);
		}
		AABBChanged(change);
		AlignChanged();
		break;

	case scWorld:
		for (Children::iterator iter = _children.begin(); iter != _children.end(); ++iter)
			(*iter)->StructureChanged(scWorld);

		AlignChanged();
		break;
		
	case scChild:
		AABBChanged(change);

		if (_parent)
			_parent->StructureChanged(scChild);
		break;
	}

	for (EventList::Position pos = _events.First(); Event** iter = _events.Current(pos); _events.Next(pos))	
		(*iter)->OnStructureChanged(this, change);	
}

void Widget::FlagChanged(Flag flag, bool value)
{
	//nothing
}

bool Widget::OnMouseClick(const MouseClick& mClick)
{
	bool res = false;
	//bool mouseDown = _isMouseDown && mClick.key == mkLeft && mClick.state == ksUp;
	bool mouseDown = mClick.key == mkLeft && mClick.state == ksDown;
	_isMouseDown = true;

	for (EventList::Position pos = _events.First(); Event** iter = _events.Current(pos); _events.Next(pos))	
		res |= (*iter)->OnMouseClick(this, mClick);

	res |= mouseDown && _flags[wfMouseOnClick] && OnClick(mClick);

	if (mClick.state == ksUp)
		_isMouseDown = false;

	return res | _flags[wfCathMouseMessages];
}

bool Widget::OnMouseOver(const MouseMove& mMove)
{
	bool res = false;

	for (EventList::Position pos = _events.First(); Event** iter = _events.Current(pos); _events.Next(pos))	
		res |= (*iter)->OnMouseOver(this, mMove);

	if (!_isMouseOver)
	{
		_isMouseOver = true;
		res |= ApplyMouseEnter(false);
	}

	return res;
}

bool Widget::OnMouseEnter(const MouseMove& mMove)
{
	bool res = false;	

	for (EventList::Position pos = _events.First(); Event** iter = _events.Current(pos); _events.Next(pos))	
		res |= (*iter)->OnMouseEnter(this, mMove);

	return res | _flags[wfCathMouseMessages];
}

void Widget::OnMouseLeave(bool wasReset)
{
	for (EventList::Position pos = _events.First(); Event** iter = _events.Current(pos); _events.Next(pos))	
		(*iter)->OnMouseLeave(this, wasReset);
}

bool Widget::OnMouseDown(const MouseClick& mClick)
{
	bool res = false;

	for (EventList::Position pos = _events.First(); Event** iter = _events.Current(pos); _events.Next(pos))	
		res |= (*iter)->OnMouseDown(this, mClick);

	return res;
}

bool Widget::OnMouseMove(const MouseMove& mMove)
{
	bool res = false;

	if (_isMouseOver && !GetWorldAABB(false).ContainsPoint(mMove.worldCoord))	
	{
		_isMouseOver = false;
		ApplyMouseEnter(false);
	}

	for (EventList::Position pos = _events.First(); Event** iter = _events.Current(pos); _events.Next(pos))	
		res |= (*iter)->OnMouseMove(this, mMove);

	return res;
}

bool Widget::OnClick(const MouseClick& mClick)
{
	bool res = false;

	for (EventList::Position pos = _events.First(); Event** iter = _events.Current(pos); _events.Next(pos))	
		res |= (*iter)->OnClick(this, mClick);

	return res;
}

bool Widget::OnSelect(Object* item)
{
	bool res = false;

	for (EventList::Position pos = _events.First(); Event** iter = _events.Current(pos); _events.Next(pos))
		res |= (*iter)->OnSelect(this, item);

	return res;
}

void Widget::OnFocusChanged()
{
	for (EventList::Position pos = _events.First(); Event** iter = _events.Current(pos); _events.Next(pos))
		(*iter)->OnFocusChanged(this);
}

bool Widget::OnDrag(const MouseMove& mMove)
{
	bool res = false;

	for (EventList::Position pos = _events.First(); Event** iter = _events.Current(pos); _events.Next(pos))
		res |= (*iter)->OnDrag(this, mMove);

	return res;
}

bool Widget::disposed() const
{
	return _disposed;
}

void Widget::InsertGraphic(Graphic* graphic, Graphics::const_iterator place)
{
	graphic->AddRef();
	_graphics.insert(place, graphic);
}

void Widget::InsertGraphic(Graphic* graphic)
{
	InsertGraphic(graphic, _graphics.end());
}

void Widget::RemoveGraphic(Graphics::const_iterator iter)
{
	(*iter)->Release();
	_graphics.erase(iter);
}

void Widget::RemoveGraphic(Graphic* graphic)
{
	RemoveGraphic(_graphics.Find(graphic));
}

void Widget::MoveGraphicTo(Graphic* graphic, Graphics::const_iterator pos)
{
	Graphics::iterator newIter = _graphics.insert(pos, 0);
	Graphics::iterator oldIter = _graphics.Find(graphic);
	*newIter = graphic;
	RemoveGraphic(oldIter);
}

void Widget::ClearGraphics()
{
	for (Graphics::iterator iter = _graphics.begin(); iter != _graphics.end(); ++iter)
		(*iter)->Release();
	_graphics.clear();
}

void Widget::DeleteAllGraphics()
{
	for (Graphics::iterator iter = _graphics.begin(); iter != _graphics.end(); ++iter)
	{
		(*iter)->Release();
		GetContext().ReleaseGraphic(*iter);
	}
	_graphics.clear();
}

D3DXVECTOR2 Widget::LocalToWorldCoord(const D3DXVECTOR2& value) const
{
	D3DXVECTOR2 res;
	D3DXVec2TransformCoord(&res, &value, &GetWorldMat());

	return res;
}

D3DXVECTOR2 Widget::WorldToLocalCoord(const D3DXVECTOR2& value) const
{
	D3DXVECTOR2 res;
	D3DXVec2TransformCoord(&res, &value, &GetInvWorldMat());

	return res;
}

D3DXVECTOR2 Widget::LocalToWorldNorm(const D3DXVECTOR2& value) const
{
	D3DXVECTOR2 res;
	D3DXVec2TransformNormal(&res, &value, &GetWorldMat());

	return res;
}

D3DXVECTOR2 Widget::WorldToLocalNorm(const D3DXVECTOR2& value) const
{
	D3DXVECTOR2 res;
	D3DXVec2TransformNormal(&res, &value, &GetInvWorldMat());

	return res;
}

Manager& Widget::GetManager()
{
	return *_manager;
}

Context& Widget::GetContext()
{
	return _manager->GetContext();
}

bool Widget::GetFlag(Flag flag) const
{
	return _flags[flag];
}

void Widget::SetFlag(Flag flag, bool value)
{
	if (_flags[flag] != value)
	{
		_flags[flag] = value;

		switch (flag)
		{
		case wfTopmost:
			value ? GetManager().InsertTopmostWidget(this) : GetManager().RemoveTopmostWidget(this);
			break;

		case wfAlignLeft:
		case wfAlignTop:
		case wfAlignRight:
		case wfAlignBottom:
			StructureChanged();
		}

		_flags[flag] = value;
		FlagChanged(flag, value);
	}
}

int Widget::GetTopmostLevel() const
{
	return _topmostLevel;
}

void Widget::SetTopmostLevel(int value)
{
	if (_topmostLevel != value)
	{
		_topmostLevel = value;
		if (_flags.test(wfTopmost))
		{
			SetFlag(wfTopmost, false);
			SetFlag(wfTopmost, true);
		}
	}
}

bool Widget::GetVisible() const
{
	return _visible;
}

void Widget::SetVisible(bool value)
{
	_visible = value;
}

void Widget::ShowModal(bool show)
{
	if (_modal != show)
	{
		if (show)
			_manager->PushModalWidget(this);
		else
			_manager->PopModalWidget(this);

		_modal = show;
	}
}

bool Widget::modal() const
{
	return _modal;
}

void Widget::RegEvent(Event* value)
{
	LSL_ASSERT(value);

	if (_events.IsFind(value))
		return;

	value->AddRef();
	_events.Insert(value);
}

void Widget::UnregEvent(Event* value)
{
	LSL_ASSERT(value);

	if (value == NULL || !_events.IsFind(value))
		return;

	value->Release();
	_events.Remove(value);
}

void Widget::SetEvent(Event* value)
{
	ClearEvents();

	if (value)
		RegEvent(value);
}

void Widget::ClearEvents()
{
	for (EventList::const_iterator iter = _events.begin(); iter != _events.end(); ++iter)
		(*iter)->Release();	
	_events.Clear();
}

void Widget::InsertChild(Widget* child)
{
	LSL_ASSERT(child != this && child->_parent == 0);

	child->AddRef();
	_children.push_back(child);

	child->_parent = this;
	child->WorldTransformChanged();
	child->StructureChanged(scWorld);

	StructureChanged(scChild);
}

void Widget::RemoveChild(Widget* child)
{
	LSL_ASSERT(child != this && child->_parent == this);

	DoRemoveChild(child);
	_children.Remove(child);

	StructureChanged(scChild);
}

void Widget::ClearChildren()
{
	for (Children::iterator iter = _children.begin(); iter != _children.end(); ++iter)
		DoRemoveChild(*iter);
	_children.clear();

	StructureChanged(scChild);
}

void Widget::DeleteAllChildren()
{
	Children children = _children;
	for (Children::iterator iter = children.begin(); iter != children.end(); ++iter)
	{
		(*iter)->DeleteAllChildren();
		(*iter)->SetVisible(false);
		_manager->ReleaseWidget(*iter);
	}

	StructureChanged(scChild);
}

bool Widget::IsMouseDown() const
{
	return _isMouseDown;
}

bool Widget::IsMouseOver() const
{
	return _isMouseOver;
}

bool Widget::IsMouseEnter() const
{
	return _isMouseEnter;
}

void Widget::SetFocused(bool value, bool wasReset)
{
	if (_focused != value)
	{
		_focused = value;
		ApplyMouseEnter(wasReset);

		OnFocusChanged();
	}
}

bool Widget::IsFocused() const
{
	return _focused;
}

void Widget::Press()
{
	_manager->BeginSafeMode();

	MouseClick mClick = _manager->GetMouseClick(this);
	mClick.key = mkLeft;
	mClick.state = ksDown;
	OnClick(mClick);

	_manager->EndSafeMode();
}

const Widget::Graphics& Widget::GetGraphics() const
{
	return _graphics;
}

Widget* Widget::GetParent()
{
	return _parent;
}

void Widget::SetParent(Widget* value)
{
	if (_parent != value)
	{
		if (_parent)
			_parent->RemoveChild(this);
		if (value)
			value->InsertChild(this);
	}
}

const Widget::Children& Widget::GetChildren() const
{
	return _children;
}

Widget::Anchor Widget::GetAnchor() const
{
	return _anchor;
}

void Widget::SetAnchor(Anchor value)
{
	_anchor = value;
	TransformChanged();
}

Widget::Anchor Widget::GetAlign() const
{
	return _align;
}

void Widget::SetAlign(Anchor value)
{
	_align = value;
	StructureChanged(scLocal);
}

bool Widget::GetEnabled() const
{
	return _enabled;
}

void Widget::SetEnabled(bool value)
{
	if (_enabled == value)
		return;
	_enabled = value;

	if (_isMouseOver && !_enabled)
	{
		_isMouseOver = false;
		_focused = false;
		ApplyMouseEnter(true);
	}

	OnEnabled(value);
}

D3DXVECTOR2 Widget::GetAlignPos() const
{
	return GetAlignPos(_size, _align);
}

const D3DXVECTOR2& Widget::GetPos() const
{
	ApplyAlign();

	return _pos;
}

void Widget::SetPos(const D3DXVECTOR2& value)
{
	_pos = value;
	
	TransformChanged();
}

void Widget::SetPos(float x, float y)
{
	SetPos(D3DXVECTOR2(x, y));
}

const D3DXVECTOR2& Widget::GetScale() const
{
	return _scale;
}

void Widget::SetScale(const D3DXVECTOR2& value)
{
	_scale = value;

	TransformChanged();
}

void Widget::SetScale(float x, float y)
{
	SetScale(D3DXVECTOR2(x, y));
}

float Widget::GetRot() const
{
	return _rot;
}

void Widget::SetRot(float value)
{
	_rot = value;
	TransformChanged();
}

bool Widget::GetCoord3d() const
{
	return _coord3d;
}

void Widget::SetCoord3d(bool value)
{	
	_coord3d = value;

	TransformChanged();
}

const D3DXVECTOR3& Widget::GetPos3d() const
{
	return _pos3d;
}

void Widget::SetPos3d(const D3DXVECTOR3& value)
{
	_pos3d = value;

	TransformChanged();
}

D3DXVECTOR2 Widget::GetWorldPos() const
{
	return D3DXVECTOR2(GetWorldMat().m[3]);
}

void Widget::SetWorldPos(const D3DXVECTOR2& value)
{
	SetPos(_parent ? _parent->WorldToLocalCoord(value) : value);
}

const D3DXMATRIX& Widget::GetMat() const
{
	ApplyAlign();

	BuildMatrix(mcLocal);
	
	return _matrix[mcLocal];
}

const D3DXMATRIX& Widget::GetWorldMat() const
{
	BuildMatrix(mcWorld);
	
	return _matrix[mcWorld];
}

const D3DXMATRIX& Widget::GetInvMat() const
{
	BuildMatrix(mcInvLocal);
	
	return _matrix[mcInvLocal];
}

const D3DXMATRIX& Widget::GetInvWorldMat() const
{
	BuildMatrix(mcInvWorld);
	
	return _matrix[mcInvWorld];
}

const D3DXVECTOR2& Widget::GetSize() const
{
	ApplyAlign();

	return _size;
}

void Widget::SetSize(const D3DXVECTOR2& value)
{
	_size = value;
	StructureChanged(scLocal);
}

void Widget::SetSize(float szX, float szY)
{
	SetSize(D3DXVECTOR2(szX, szY));
}

const AABB2& Widget::GetLocalAABB(bool includeChild) const
{
	if (includeChild)
	{
		BuildLocalChildAABB();
		return _localChildAABB;
	}
	else
	{
		BuildLocalAABB();
		return _localAABB;
	}
}

const AABB2& Widget::GetWorldAABB(bool includeChild) const
{
	if (includeChild)
	{
		BuildWorldChildAABB();
		return _worldChildAABB;
	}
	else
	{
		BuildWorldAABB();
		return _worldAABB;
	}
}

const AABB2& Widget::GetChildAABB() const
{
	BuildChildAABB();

	return _childAABB;
}

int Widget::GetTag() const
{
	return _tag;
}

void Widget::SetTag(int value)
{
	_tag = value;
}

Object* Widget::GetData()
{
	return _data;
}

void Widget::SetData(Object* value)
{
	if (ReplaceRef(_data, value))
		_data = value;
}

D3DXVECTOR2 Widget::GetAlignPos(const D3DXVECTOR2& size, Anchor align)
{
	D3DXVECTOR2 half = size/2;

	switch (align)
	{
	case waLeft:
		return D3DXVECTOR2(half.x, 0);
	case waRight:
		return D3DXVECTOR2(-half.x, 0);
	case waTop:
		return D3DXVECTOR2(0, half.y);
	case waBottom:
		return D3DXVECTOR2(0, -half.y);

	case waLeftTop:
		return D3DXVECTOR2(half.x, half.y);
	case waLeftBottom:
		return D3DXVECTOR2(half.x, -half.y);
	case waRightTop:
		return D3DXVECTOR2(-half.x, half.y);
	case waRightBottom:
		return D3DXVECTOR2(-half.x, -half.y);

	default:
		return NullVec2;
	}
}




Dummy::Dummy(Manager* manager): _MyBase(manager)
{
	SetSize(IdentityVec2);
}




WidgetCont::WidgetCont(Manager* manager): _MyBase(manager)
{
	_cont = GetManager().CreateDummy();
	_cont->AddRef();
	_cont->SetParent(this);
}

WidgetCont::~WidgetCont()
{
	_cont->Release();
	GetManager().ReleaseWidget(_cont);
}

Dummy* WidgetCont::GetCont()
{
	return _cont;
}




PlaneFon::PlaneFon(Manager* manager): _MyBase(manager)
{
	_plane = GetContext().CreatePlane();
	_plane->AddRef();
	InsertGraphic(_plane);
}

PlaneFon::~PlaneFon()
{
	_plane->Release();
}

void PlaneFon::StructureChanged(StructChange change)
{
	_MyBase::StructureChanged(change);

	if (change == scLocal)
	{
		_plane->SetSize(GetSize());
		_plane->SetPos(GetAlignPos());
	}
}

Material& PlaneFon::GetMaterial()
{
	return *_plane->GetOrCreateMaterial();
}

void PlaneFon::SetMaterial(Material* value)
{
	_plane->SetMaterial(value);
}




Label::Label(Manager* manager): _MyBase(manager), _font(0), _horAlign(Text::haCenter), _vertAlign(Text::vaCenter), _wordWrap(false), _text(0), _textW(0), _baseText(0), _vScroll(0)
{	
	_material = new Material();
}

Label::~Label()
{
	SetFont(0);
	if (_baseText)
		_baseText->SetMaterial(0);

	lsl::SafeRelease(_baseText);
	_text = 0;
	_textW = 0;

	delete _material;
}

void Label::CreateText()
{
	if (_text)
		return;
	DeleteText();
	
	_text = GetContext().CreateText();
	_text->AddRef();
	InsertGraphic(_text);
	_baseText = _text;

	ApplyText();
}

void Label::CreateTextW()
{
	if (_textW)
		return;
	DeleteText();

	_textW = GetContext().CreateTextW();
	_textW->AddRef();
	InsertGraphic(_textW);
	_baseText = _textW;

	ApplyText();
}

void Label::DeleteText()
{
	if (_baseText)
	{
		_baseText->Release();
		RemoveGraphic(_baseText);
		GetContext().ReleaseGraphic(_baseText);
		_baseText = 0;
		_text = 0;
		_textW = 0;
	}
}

void Label::ApplyText()
{	
	if (_baseText)
	{
		_baseText->SetSize(GetSize());
		_baseText->SetFont(_font);
		_baseText->SetMaterial(_material);
		_baseText->SetHorAlign(_horAlign);
		_baseText->SetVertAlign(_vertAlign);
		_baseText->SetWordWrap(_wordWrap);
		_baseText->SetClipEnable(GetFlag(wfClientClip));
		_baseText->SetVScroll(_vScroll);
	}
}

void Label::StructureChanged(StructChange change)
{
	_MyBase::StructureChanged(change);

	if (_baseText && change == scLocal)
	{
		_baseText->SetSize(GetSize());
		_baseText->SetPos(GetAlignPos());
	}
}

AABB2 Label::GetTextAABB()
{
	return _baseText ? _baseText->GetTextAABB() : AABB2(0);
}

void Label::AdjustSizeByText()
{
	SetSize(GetTextAABB().GetSize());
}

graph::TextFont* Label::GetFont()
{
	return _font;
}

void Label::SetFont(graph::TextFont* value)
{
	if (ReplaceRef(_font, value))
	{
		_font = value;
		if (_baseText)
		{
			_baseText->SetFont(_font);
			StructureChanged();
		}		
	}	
}

Material& Label::GetMaterial()
{
	return *_material;
}

std::string Label::GetText() const
{
	if (_text)
		return _text->GetText();
	else if (_textW)
		return lsl::ConvertStrWToA(_textW->GetText());

	return "";
}

void Label::SetText(const std::string& value)
{
	if (value != "")
	{
		CreateText();
		_text->SetText(value);
		StructureChanged();
	}
	else
	{
		DeleteText();
		StructureChanged();
	}
}

std::wstring Label::GetTextW() const
{
	if (_textW)
		return _textW->GetText();
	else if (_text)
		return lsl::ConvertStrAToW(_text->GetText());

	return L"";
}

void Label::SetTextW(const std::wstring& value)
{
	if (value != L"")
	{
		CreateTextW();
		_textW->SetText(value);
		StructureChanged();
	}
	else
	{
		DeleteText();
		StructureChanged();
	}
}

Text::HorAlign Label::GetHorAlign() const
{
	return _horAlign;
}

void Label::SetHorAlign(Text::HorAlign value)
{
	if (_horAlign != value)
	{
		_horAlign = value;
		if (_baseText)
		{
			_baseText->SetHorAlign(_horAlign);
			StructureChanged();
		}		
	}	
}

Text::VertAlign Label::GetVertAlign() const
{
	return _vertAlign;
}

void Label::SetVertAlign(Text::VertAlign value)
{
	if (_vertAlign != value)
	{
		_vertAlign = value;
		if (_baseText)
		{
			_baseText->SetVertAlign(value);
			StructureChanged();
		}
	}
}

bool Label::GetWordWrap() const
{
	return _wordWrap;
}

void Label::SetWordWrap(bool value)
{
	if (_wordWrap != value)
	{
		_wordWrap = value;
		if (_baseText)
		{
			_baseText->SetWordWrap(_wordWrap);
			StructureChanged();
		}
	}
}

float Label::GetVScroll() const
{
	return _vScroll;
}

void Label::SetVScroll(float value)
{
	_vScroll = value;
	if (_baseText)
		_baseText->SetVScroll(_vScroll);
}




Button::Button(Manager* manager): _MyBase(manager), _style(bsSimple), _selSize(0, 0), _selected(false), _fonMaterial(0), _selMaterial(0), _textMaterial(NULL), _textSelMaterial(NULL), _createFon(false), _createSel(false), _createText(false), _createTextSel(false), _selection(false), _fadeIn(-1), _fadeOut(-1)
{
	_fon = GetContext().CreatePlane();
	_fon->AddRef();
	_fon->SetMaterial(0);
	InsertGraphic(_fon);

	_sel = GetContext().CreatePlane();
	_sel->AddRef();
	_sel->SetMaterial(NULL);
	InsertGraphic(_sel);
	_sel->SetActive(false);

	_text = GetContext().CreateText();
	_text->AddRef();
	InsertGraphic(_text);

	SetSize(D3DXVECTOR2(100.0f, 25.0f));

	RegProgress();
}

Button::~Button()
{
	UnregProgress();

	SetSel(0);
	SetFon(0);
	SetTextMaterial(NULL);
	SetTextSelMaterial(NULL);

	_fon->Release();
	_sel->Release();
	_text->Release();

	DeleteAllGraphics();
}

void Button::ApplySelection(bool instant)
{
	if (_selection && _textSelMaterial)
		_text->SetMaterial(_textSelMaterial);
	else
		_text->SetMaterial(_textMaterial);

	switch (_style)
	{
		case bsSimple:
			_sel->SetActive(_selection);
			break;

		case bsSelAnim:
		{
			_fadeIn = -1;
			_fadeOut = -1;
			_sel->SetActive(true);

			if (_selection)
				_fadeIn = instant ? 1.0f : 0;
			else
				_fadeOut = instant ? 1.0f : 0;
			break;	
		}
	}
}

void Button::UpdateSelection(bool instant)
{
	bool selection = IsMouseEnter() || _selected;
	if (_selection == selection)
		return;
	_selection = selection;

	ApplySelection(instant);
}

void Button::StructureChanged(StructChange change)
{
	_MyBase::StructureChanged(change);

	if (change == scLocal)
	{
		_fon->SetSize(GetSize());
		_fon->SetPos(GetAlignPos());

		_sel->SetSize((_selSize.x == 0 && _selSize.y == 0) ? GetSize() : _selSize);
		_sel->SetPos(GetAlignPos());

		_text->SetSize(GetSize());
		_text->SetPos(GetAlignPos());
	}
}

void Button::OnProgress(float deltaTime)
{
	if (_fadeIn >= 0)
	{
		float alpha = lsl::ClampValue(_fadeIn / 0.25f, 0.0f, 1.0f);		
		if (_sel)
			_sel->SetAlpha(alpha);		

		if (alpha == 1.0f)
			_fadeIn = -1.0f;
		else
			_fadeIn += deltaTime;
	}

	if (_fadeOut >= 0)
	{
		float alpha = lsl::ClampValue(_fadeOut / 0.25f, 0.0f, 1.0f);
		if (_sel)
			_sel->SetAlpha(1.0f - alpha);		

		if (alpha == 1.0f)
			_fadeOut = -1.0f;
		else
			_fadeOut += deltaTime;
	}
}

bool Button::OnMouseEnter(const MouseMove& mMove)
{
	bool res = _MyBase::OnMouseEnter(mMove);

	UpdateSelection(false);

	return res;
}

void Button::OnMouseLeave(bool wasReset)
{
	_MyBase::OnMouseLeave(wasReset);

	UpdateSelection(wasReset);
}

Material* Button::GetOrCreateFon()
{
	if (!_fonMaterial)
	{
		_fonMaterial = new Material();
		_fonMaterial->AddRef();
		_createFon = true;
		_fon->SetMaterial(_fonMaterial);
	}
	return _fonMaterial;
}

Material* Button::GetFon()
{
	return _fonMaterial;
}

void Button::SetFon(Material* value)
{
	if (ReplaceRef(_fonMaterial, value))
	{
		_fon->SetMaterial(0);

		if (_createFon)
		{
			_createFon = false;			
			delete _fonMaterial;
		}
		_fonMaterial = value;
		_fon->SetMaterial(_fonMaterial);
	}
}

Material* Button::GetOrCreateSel()
{
	if (!_selMaterial)
	{
		_selMaterial = new Material();
		_selMaterial->AddRef();
		_createSel = true;
		_sel->SetMaterial(_selMaterial);
	}
	return _selMaterial;
}

Material* Button::GetSel()
{
	return _selMaterial;
}

void Button::SetSel(Material* value)
{
	if (ReplaceRef(_selMaterial, value))
	{
		_sel->SetMaterial(NULL);

		if (_createSel)
		{
			_createSel = false;			
			delete _selMaterial;
		}

		_selMaterial = value;
		_sel->SetMaterial(_selMaterial);
	}
}

Material* Button::GetOrCreateTextMaterial()
{
	if (!_textMaterial)
	{
		_textMaterial = new Material();
		_textMaterial->AddRef();
		_createText = true;

		ApplySelection(true);
	}

	return _textMaterial;
}

Material* Button::GetTextMaterial()
{
	return _textMaterial;
}

void Button::SetTextMaterial(Material* value)
{
	if (ReplaceRef(_textMaterial, value))
	{
		if (_text->GetMaterial() == _textMaterial)
			_text->SetMaterial(0);

		if (_createText)
		{
			_createText = false;			
			delete _textMaterial;
		}

		_textMaterial = value;

		ApplySelection(true);
	}
}

Material* Button::GetOrCreateTextSelMaterial()
{
	if (!_textSelMaterial)
	{
		_textSelMaterial = new Material();
		_textSelMaterial->AddRef();
		_createTextSel = true;

		ApplySelection(true);
	}

	return _textSelMaterial;
}

Material* Button::GetTextSelMaterial()
{
	return _textSelMaterial;
}

void Button::SetTextSelMaterial(Material* value)
{
	if (ReplaceRef(_textSelMaterial, value))
	{
		if (_text->GetMaterial() == _textSelMaterial)
			_text->SetMaterial(0);

		if (_createTextSel)
		{
			_createTextSel = false;			
			delete _textSelMaterial;
		}

		_textSelMaterial = value;

		ApplySelection(true);
	}
}

graph::TextFont* Button::GetFont()
{
	return _text->GetFont();
}

void Button::SetFont(graph::TextFont* value)
{
	_text->SetFont(value);
	StructureChanged();
}

const std::string& Button::GetText() const
{
	return _text->GetText();
}

void Button::SetText(const std::string& value)
{
	_text->SetText(value);
	StructureChanged();
}

const D3DXVECTOR2& Button::GetSelSize() const
{
	return _selSize;
}

void Button::SetSelSize(const D3DXVECTOR2& value)
{
	_selSize = value;
	StructureChanged();
}

Button::Style Button::GetStyle() const
{
	return _style;
}

void Button::SetStyle(Style value)
{
	_style = value;

	Select(IsMouseEnter(), true);
}

void Button::Select(bool value, bool instant)
{
	if (_selected != value)
	{
		_selected = value;
		UpdateSelection(instant);
	}
}

bool Button::IsSelected() const
{
	return _selected;
}




StepperBox::StepperBox(Manager* manager): Widget(manager), _selIndex(-1), _itemsLoop(false)
{
	_childEvent = new ChildEvent(this);

	_right = manager->CreateButton();
	_right->SetParent(this);
	_right->SetAlign(waLeft);
	_right->SetEvent(_childEvent);
	_right->SetStyle(Button::bsSelAnim);
	_right->SetRot(D3DX_PI);

	_left = manager->CreateButton();
	_left->SetParent(this);
	_left->SetAlign(waLeft);
	_left->SetEvent(_childEvent);
	_left->SetStyle(Button::bsSelAnim);

	_planeBg = GetContext().CreatePlane();
	InsertGraphic(_planeBg);

	_textField = GetContext().CreateText();
	InsertGraphic(_textField);

	ApplySelection();
}

StepperBox::~StepperBox()
{
	GetManager().ReleaseWidget(_right);
	GetManager().ReleaseWidget(_left);

	delete _childEvent;
}

StepperBox::ChildEvent::ChildEvent(StepperBox* owner): _owner(owner)
{
}

bool StepperBox::ChildEvent::OnClick(Widget* sender, const MouseClick& mClick)
{
	if (sender == _owner->_left)
	{
		if (_owner->_itemsLoop)
			_owner->SetSelIndex(_owner->GetSelIndex() > 0 ? _owner->GetSelIndex() - 1 : _owner->_items.size() - 1);
		else
			_owner->SetSelIndex(_owner->GetSelIndex() - 1);

		_owner->OnSelect(NULL);
		return true;
	}

	if (sender == _owner->_right)
	{
		if (_owner->_itemsLoop)
			_owner->SetSelIndex(_owner->GetSelIndex() + 1 < (int)_owner->_items.size() ? _owner->GetSelIndex() + 1 : 0);
		else
			_owner->SetSelIndex(_owner->GetSelIndex() + 1);

		_owner->OnSelect(NULL);
		return true;
	}

	return false;
}

void StepperBox::ApplySelection()
{
	if (_selIndex >= 0 && (unsigned)_selIndex < _items.size())
		_textField->SetText(_items[_selIndex]);
	else
		_textField->SetText("");

	if (_itemsLoop)
	{
		_left->SetEnabled(_items.size() > 0 && GetEnabled());
		_right->SetEnabled(_items.size() > 0 && GetEnabled());
	}
	else
	{
		_left->SetEnabled(_selIndex > 0 && (unsigned)_selIndex < _items.size() && GetEnabled());
		_right->SetEnabled(_selIndex >= 0 && (unsigned)_selIndex + 1 < _items.size() && GetEnabled());
	}	
}

void StepperBox::OnEnabled(bool value)
{
	Widget::OnEnabled(value);

	_left->SetEnabled(value);
	_right->SetEnabled(value);
}

void StepperBox::StructureChanged(StructChange change)
{
	Widget::StructureChanged(change);

	if (change == scLocal)
	{
		D3DXVECTOR2 pos = GetAlignPos();

		_planeBg->SetSize(GetSize());
		_planeBg->SetPos(pos);
		_planeBg->SetActive(false);

		if (_left->GetFon())
		{
			_left->SetSize(_left->GetFon()->GetImageSize());
			_right->SetSize(_left->GetSize());
		}
		if (_left->GetSel())
		{
			_left->SetSelSize(_left->GetSel()->GetImageSize());
			_right->SetSelSize(_left->GetSelSize());
		}

		_left->SetPos(D3DXVECTOR2(-GetSize().x/2, 0.0f) + pos);
		_right->SetPos(D3DXVECTOR2(GetSize().x/2, 0.0f) + pos);

		_textField->SetSize(D3DXVECTOR2(GetSize().x - _left->GetSize().x - _right->GetSize().x, GetSize().y));
		_textField->SetPos(pos);
	}
}

void StepperBox::Invalidate()
{
	StructureChanged();
}

Button* StepperBox::GetLeft()
{
	return _left;
}

Button* StepperBox::GetRight()
{
	return _right;
}

Material* StepperBox::GetOrCreateBg()
{
	return _planeBg->GetOrCreateMaterial();
}

Material* StepperBox::GetBg()
{
	return _planeBg->GetMaterial();
}

void StepperBox::SetBg(Material* value)
{
	_planeBg->SetMaterial(value);
}

Material* StepperBox::GetOrCreateArrow()
{
	Material* mat = _left->GetOrCreateFon();
	_right->SetFon(mat);

	return mat;
}

Material* StepperBox::GetArrow()
{
	return _left ? _left->GetFon() : NULL;
}

void StepperBox::SetArrow(Material* value)
{
	_right->SetFon(value);
	_left->SetFon(value);
}

Material* StepperBox::GetOrCreateArrowSel()
{
	Material* mat = _left->GetOrCreateSel();
	_right->SetSel(mat);

	return mat;
}

Material* StepperBox::GetArrowSel()
{
	return _left->GetSel();
}

void StepperBox::SetArrowSel(Material* value)
{
	_right->SetSel(value);
	_left->SetSel(value);
}

Material* StepperBox::GetOrCreateText()
{
	return _textField->GetOrCreateMaterial();
}

Material* StepperBox::GetText()
{
	return _textField->GetMaterial();
}

void StepperBox::SetText(Material* value)
{
	_textField->SetMaterial(value);
}

graph::TextFont* StepperBox::GetFont()
{
	return _textField->GetFont();
}

void StepperBox::SetFont(graph::TextFont* value)
{
	_textField->SetFont(value);
}

const StringList& StepperBox::GetItems() const
{
	return _items;
}

void StepperBox::SetItems(const StringList& value)
{
	_items = value;
	ApplySelection();
}

int StepperBox::GetSelIndex() const
{
	return _selIndex;
}

void StepperBox::SetSelIndex(int index)
{
	if (_selIndex != index)
	{
		_selIndex = lsl::ClampValue(index, 0, (int)(_items.size() - 1));
		ApplySelection();
	}
}

bool StepperBox::GetItemsLoop() const
{
	return _itemsLoop;
}

void StepperBox::SetItemsLoop(bool value)
{
	_itemsLoop = value;
	ApplySelection();
}




DropBox::DropBox(Manager* manager): _MyBase(manager), _showItems(false), _fonMaterial(0), _textMaterial(0), _selMaterial(0), _itemsFon(0), _selInd(-1)
{
	_itemsEvent = new ItemsEvent(this);

	_fon = GetContext().CreatePlane();
	_fon->AddRef();
	InsertGraphic(_fon);

	_button = GetContext().CreatePlane();
	_button->AddRef();
	InsertGraphic(_button);
	
	_selItem = GetContext().CreateText();
	_selItem->AddRef();
	InsertGraphic(_selItem);
	_selItem->SetHorAlign(Text::haLeft);

	_itemsFon = GetManager().CreatePlaneFon();
	_itemsFon->AddRef();
	_itemsFon->SetParent(this);
	_itemsFon->SetFlag(wfTopmost, true);
	_itemsFon->SetEvent(_itemsEvent);
	_itemsFon->SetVisible(false);

	SetSize(100.0f, 20.0f);
}

DropBox::~DropBox()
{
	ShowItems(false);

	_itemsFon->Release();
	GetManager().ReleaseWidget(_itemsFon);

	_selItem->Release();
	_button->Release();
	_fon->Release();
	DeleteAllGraphics();

	lsl::SafeDelete(_selMaterial);
	lsl::SafeDelete(_textMaterial);
	lsl::SafeDelete(_fonMaterial);

	delete _itemsEvent;
}

DropBox::ItemsEvent::ItemsEvent(DropBox* dropBox): _dropBox(dropBox)
{
}

bool DropBox::ItemsEvent::OnMouseOver(Widget* sender, const MouseMove& mMove)
{
	_dropBox->FindSelTextItem(mMove.coord, true);

	return false;
}

bool DropBox::ItemsEvent::OnMouseDown(Widget* sender, const MouseClick& mClick)
{
	if (sender == _dropBox->_itemsFon && mClick.key == mkLeft && mClick.state == ksUp)
	{
		D3DXVECTOR2 localCoord = _dropBox->_itemsFon->WorldToLocalCoord(mClick.worldCoord);
		if (Text* text = _dropBox->FindSelTextItem(localCoord, false))
		{
			_dropBox->SetSelItem(text->GetText());
			_dropBox->OnSelect(0);
		}
		
		_dropBox->ShowItems(false);
		
		return true;
	}

	return false;
}

void DropBox::ShowItems(bool value)
{
	if (_showItems != value)
	{
		D3DXVECTOR2 size = GetSize();

		if (_showItems)
		{
			for (TextItems::iterator iter = _textItems.begin(); iter != _textItems.end(); ++iter)
			{			
				(*iter)->Release();
				_itemsFon->RemoveGraphic(*iter);
				GetContext().ReleaseGraphic(*iter);
			}
			_textItems.clear();
		}

		_showItems = value;

		if (_showItems)
		{
			int i = 0;
			for (StringList::iterator iter = _items.begin(); iter != _items.end(); ++iter, ++i)
			{
				Text* item = GetContext().CreateText();
				item->AddRef();
				_itemsFon->InsertGraphic(item);

				item->SetText(*iter);
				item->SetFont(_selItem->GetFont());
				item->SetSize(_selItem->GetSize());
				item->SetPos(D3DXVECTOR2(size.y/2.0f, -(i - _items.size()/2.0f + 0.5f) * size.y));
				item->SetHorAlign(Text::haLeft);
				item->SetMaterial(_textMaterial);
				
				_textItems.push_back(item);
			}
		}

		_itemsFon->SetVisible(_showItems);
	}
}

void DropBox::SelectItem(const std::string& item)
{
	_selItem->SetText(item);

	int i = 0;
	for (StringList::const_iterator iter = _items.begin(); iter != _items.end(); ++iter, ++i)
		if (*iter == item)
		{
			_selInd = i;
			return;
		}
	_selInd = -1;	
}

Text* DropBox::FindSelTextItem(const D3DXVECTOR2& point, bool deselect)
{
	Text* res = 0;

	for (TextItems::iterator iter = _textItems.begin(); iter != _textItems.end(); ++iter)
	{
		Text* item = (*iter);
		AABB2 aabb(item->GetSize());
		aabb.Offset(item->GetPos());

		if (aabb.ContainsPoint(point))
		{
			res = item;
			
			if (!deselect)
				return res;

			item->SetMaterial(_selMaterial ? _selMaterial : _textMaterial);
		}
		else
			item->SetMaterial(_textMaterial);
	}

	return res;
}

void DropBox::StructureChanged(StructChange change)
{
	_MyBase::StructureChanged(change);

	if (change == scLocal)
	{
		D3DXVECTOR2 size = GetSize();

		_fon->SetSize(size);
		D3DXVECTOR2 itemsSize = D3DXVECTOR2(size.x - size.y, size.y * _items.size());
		_itemsFon->SetSize(itemsSize);
		_itemsFon->SetPos(D3DXVECTOR2(-size.y/2.0f, -itemsSize.y/2.0f - size.y/2.0f));

		_button->SetPos(D3DXVECTOR2(size.x/2.0f - size.y/2.0f, 0.0f));
		_button->SetSize(D3DXVECTOR2(size.y, size.y));

		_selItem->SetSize(D3DXVECTOR2(size.x - size.y, size.y));

		ShowItems(false);
	}
}

bool DropBox::OnClick(const MouseClick& mClick)
{
	bool res = _MyBase::OnClick(mClick);

	if (!_showItems)
	{
		ShowItems(true);
	}

	return res;
}

const StringList& DropBox::GetItems() const
{
	return _items;
}

void DropBox::SetItems(const StringList& value)
{
	_items = value;
	StructureChanged();
}


int DropBox::GetSelInd() const
{
	return _selInd;
}

void DropBox::SetSelInd(int value)
{
	SelectItem(value >= 0 && value < static_cast<int>(_items.size()) ? _items[value] : "");
}

const std::string& DropBox::GetSelItem() const
{
	return _selItem->GetText();
}

void DropBox::SetSelItem(const std::string& value)
{
	SelectItem(value);
}

graph::TextFont* DropBox::GetFont()
{
	return _selItem->GetFont();
}

void DropBox::SetFont(graph::TextFont* value)
{
	_selItem->SetFont(value);
	StructureChanged();
}

Material& DropBox::GetFonMaterial()
{
	if (!_fonMaterial)
	{
		_fonMaterial = new Material();

		_fon->SetMaterial(_fonMaterial);
		_itemsFon->SetMaterial(_fonMaterial);
	}

	return *_fon->GetOrCreateMaterial();
}

Material& DropBox::GetButMaterial()
{
	return *_button->GetOrCreateMaterial();
}

Material& DropBox::GetTextMaterial()
{
	if (!_textMaterial)
	{
		_textMaterial = new Material();
		_selItem->SetMaterial(_textMaterial);
	}

	return *_textMaterial;
}

Material& DropBox::GetSelMaterial()
{
	if (!_selMaterial)	
		_selMaterial = new Material();

	return *_selMaterial;
}




TrackBar::TrackBar(Manager* manager): _MyBase(manager), _dragOff(0), _grag(false)
{
	_bar = GetContext().CreatePlane();
	_bar->AddRef();
	InsertGraphic(_bar);

	_button = GetContext().CreatePlane();
	_button->AddRef();
	InsertGraphic(_button);

	SetSize(200.0f, 25.0f);
}

TrackBar::~TrackBar()
{
	_button->Release();
	_bar->Release();
}

float TrackBar::ComputeBarPos(const D3DXVECTOR2& point) const
{
	return point.x/GetSize().x + 0.5f;
}

void TrackBar::StructureChanged(StructChange change)
{
	_MyBase::StructureChanged(change);

	if (change == scLocal)
	{
		D3DXVECTOR2 size = GetSize();

		_bar->SetSize(size);
		_button->SetSize(D3DXVECTOR2(size.y, size.y));
	}
}

bool TrackBar::OnMouseDown(const MouseClick& mClick)
{
	bool res = _MyBase::OnMouseDown(mClick);

	if (mClick.state == ksDown)
	{
		D3DXVECTOR2 localCoord = WorldToLocalCoord(mClick.worldCoord);
		AABB2 aabb(_button->GetSize());
		aabb.Offset(_button->GetPos());

		_dragOff = GetBarPos() - ComputeBarPos(localCoord);

		if (aabb.ContainsPoint(localCoord))
			_grag = true;
	}
	else
		_grag = false;

	return res;
}

bool TrackBar::OnMouseMove(const MouseMove& mMove)
{
	bool res = _MyBase::OnMouseMove(mMove);

	if (_grag)
	{
		D3DXVECTOR2 localCoord = WorldToLocalCoord(mMove.worldCoord);		
		SetBarPos(ComputeBarPos(localCoord) + _dragOff);

		OnDrag(mMove);
	}

	return res;
}

float TrackBar::GetBarPos() const
{
	return ComputeBarPos(_button->GetPos());
}

void TrackBar::SetBarPos(float value)
{
	_button->SetPos(D3DXVECTOR2((ClampValue(value, 0.0f, 1.0f) - 0.5f) * GetSize().x, 0.0f));
}

Material& TrackBar::GetBarMaterial()
{
	return *_bar->GetOrCreateMaterial();
}

Material& TrackBar::GetButMaterial()
{
	return *_button->GetOrCreateMaterial();
}




ViewPort3d::ViewPort3d(Manager* manager): _MyBase(manager), _style(msStatic), _setProgress(false)
{
	_view3d = GetContext().CreateView3d();
	_view3d->AddRef();
	InsertGraphic(_view3d);

	D3DXQuaternionRotationAxis(&_rot3dSpeed, &ZVector, 3.0f * D3DX_PI/4.0f);
}

ViewPort3d::~ViewPort3d()
{
	SetProgress(false);

	_view3d->Release();
}

void ViewPort3d::AnimProgress(float deltaTime)
{
	D3DXQUATERNION rot;
	D3DXQuaternionSlerp(&rot, &GetBox()->GetRot(), &(_rot3dSpeed * GetBox()->GetRot()), deltaTime);

	GetBox()->SetRot(rot);
}

void ViewPort3d::SetProgress(bool enable)
{
	if (_setProgress != enable)
	{
		if (enable)
			RegProgress();
		else
			UnregProgress();

		_setProgress = enable;
	}
}

void ViewPort3d::StructureChanged(StructChange change)
{
	_MyBase::StructureChanged(change);

	if (change == scLocal)
	{
		_view3d->SetSize(GetSize());
		_view3d->SetPos(GetAlignPos());
	}
}

bool ViewPort3d::OnMouseOver(const MouseMove& mMove)
{
	bool res = _MyBase::OnMouseOver(mMove);

	if (mMove.click.key == mkLeft && mMove.click.state == ksDown)
	{
		if (_style == msViewer)
		{
			//Вращение по двум осям
			/*D3DXQUATERNION rotY;
			D3DXQuaternionRotationAxis(&rotY, &YVector, D3DX_PI * mMove.dtCoord.x/100.0f);	
			D3DXQUATERNION rotX;
			D3DXQuaternionRotationAxis(&rotX, &XVector, -D3DX_PI * mMove.dtCoord.y/100.0f);
			SetRot3d(GetRot3d() * rotY * rotX);*/
			
			//Вращение по одной оси, совпадающией с up mesh
			D3DXQUATERNION rotZ;
			D3DXQuaternionRotationAxis(&rotZ, &ZVector, D3DX_PI * mMove.dtCoord.x/200.0f);
			GetBox()->SetRot(rotZ * GetBox()->GetRot());
		}
	}

	return res;
}

void ViewPort3d::OnProgress(float deltaTime)
{
	switch (_style)
	{
	case msAnim:
		AnimProgress(deltaTime);
		break;

	case msMouseOverAnim:
		if (IsMouseEnter())
			AnimProgress(deltaTime);
		break;
	}
}

Graphic3d* ViewPort3d::GetBox()
{
	return _view3d->GetBox();
}

const D3DXQUATERNION& ViewPort3d::GetRot3dSpeed() const
{
	return _rot3dSpeed;
}

void ViewPort3d::SetRot3dSpeed(const D3DXQUATERNION& value)
{
	_rot3dSpeed = value;
}

ViewPort3d::Style ViewPort3d::GetStyle() const
{
	return _style;
}

void ViewPort3d::SetStyle(Style value)
{
	if (_style != value)
	{
		_style = value;

		SetProgress(_style == msAnim || _style == msMouseOverAnim);
	}
}

bool ViewPort3d::GetAlign() const
{
	return _view3d->GetAlign();
}

void ViewPort3d::SetAlign(bool value)
{
	_view3d->SetAlign(value);
}




ScrollBox::ScrollBox(Manager* manager): _MyBase(manager), _arrowSize(20.0f, 20.0f), _arrowSpace(10.0f, 10.0f), _arrowScrollStep(20.0f, 20.0f)
{
	_myEvent = new MyEvent(this);

	_arrowMat = new Material();
	_arrowSelMat = new Material();

	ZeroMemory(_scrollBut, sizeof(_scrollBut));

	_clip = GetManager().CreateDummy();
	_clip->AddRef();
	_clip->SetFlag(wfClientClip, true);	
	_clip->SetParent(this);

	_box = GetManager().CreateDummy();
	_box->AddRef();
	_box->SetParent(_clip);
	_box->SetEvent(_myEvent);

	ApplyOptions();
}

ScrollBox::~ScrollBox()
{
	FreeArrows();

	_box->Release();
	GetManager().ReleaseWidget(_box);

	_clip->Release();
	GetManager().ReleaseWidget(_clip);

	delete _arrowSelMat;
	delete _arrowMat;

	delete _myEvent;
}

ScrollBox::MyEvent::MyEvent(ScrollBox* scrollBox): _scrollBox(scrollBox)
{	
}

void ScrollBox::MyEvent::OnStructureChanged(Widget* sender, StructChange change)
{
	if (sender == _scrollBox->_box && change == scChild)
		_scrollBox->ApplyScroll();
}

bool ScrollBox::MyEvent::OnClick(Widget* sender, const MouseClick& mClick)
{
	D3DXVECTOR2 scrollUp = _scrollBox->GetScroll() - _scrollBox->_arrowScrollStep;
	scrollUp /= _scrollBox->_arrowScrollStep;
	scrollUp.x = Round(scrollUp.x);
	scrollUp.y = Round(scrollUp.y);
	scrollUp *= _scrollBox->_arrowScrollStep;

	D3DXVECTOR2 scrollDown = _scrollBox->GetScroll() + _scrollBox->_arrowScrollStep;
	scrollDown /= _scrollBox->_arrowScrollStep;
	scrollDown.x = Round(scrollDown.x);
	scrollDown.y = Round(scrollDown.y);
	scrollDown *= _scrollBox->_arrowScrollStep;


	for (int i = 0; i < cScrollDirEnd; ++i) if (_scrollBox->_scrollBut[i] == sender)
		switch (i)
		{
		case sdLeft:
			return true;
		
		case sdDown:
			_scrollBox->SetScroll(D3DXVECTOR2(_scrollBox->GetScroll().x, scrollDown.y));
			return true;
			
		case sdRight:
			return true;
			
		case sdUp:
			_scrollBox->SetScroll(D3DXVECTOR2(_scrollBox->GetScroll().x, scrollUp.y));
			return true;
		}

	return false;
}

Button* ScrollBox::NewArrow()
{
	Button* arrow = GetManager().CreateButton(); 
	arrow->AddRef();
	arrow->SetParent(this);
	arrow->SetEvent(_myEvent);
	arrow->SetFon(_arrowMat);
	arrow->SetSel(_arrowSelMat);

	return arrow;
}

void ScrollBox::FreeArrows()
{
	for (int i = 0; i < cScrollDirEnd; ++i)
		if (_scrollBut[i])
		{
			_scrollBut[i]->Release();
			GetManager().ReleaseWidget(_scrollBut[i]);
			_scrollBut[i] = 0;
		}
}

void ScrollBox::ApplyOptions()
{
	FreeArrows();

	for (int i = 0; i < cOptionEnd; ++i) if (_options[i])	
		switch (i)
		{
		case soVert:
			_scrollBut[sdDown] = NewArrow();
			_scrollBut[sdUp] = NewArrow();
			_scrollBut[sdUp]->SetRot(D3DX_PI);
			break;
			
		case soHor:
			_scrollBut[sdLeft] = NewArrow();
			_scrollBut[sdLeft]->SetRot(-D3DX_PI/2.0f);
			_scrollBut[sdRight] = NewArrow();
			_scrollBut[sdRight]->SetRot(D3DX_PI/2.0f);
			break;
		}

	AdjustLayout();
}

void ScrollBox::AdjustLayout()
{
	const D3DXVECTOR2 dim = GetSize()/2.0f - _arrowSize/2.0f;
	const D3DXVECTOR2 arrowPos[cScrollDirEnd] = {D3DXVECTOR2(-dim.x, 0.0f), D3DXVECTOR2(0, -dim.y), D3DXVECTOR2(dim.x, 0.0f), D3DXVECTOR2(0, dim.y)};

	for (int i = 0; i < cScrollDirEnd; ++i)
		if (_scrollBut[i])
		{
			_scrollBut[i]->SetSize(_arrowSize);
			_scrollBut[i]->SetPos(arrowPos[i]);
		}

	D3DXVECTOR2 scrollSz = GetSize();
	if (_options[soHor])
		scrollSz.x -= 2.0f * _arrowSize.x + _arrowSpace.x;
	if (_options[soVert])
		scrollSz.y -= 2.0f * _arrowSize.y + _arrowSpace.y;
	_clip->SetSize(scrollSz);
	_box->SetSize(scrollSz);
}

void ScrollBox::ApplyScroll()
{
	SetScroll(GetScroll());
}

void ScrollBox::StructureChanged(StructChange change)
{
	_MyBase::StructureChanged(change);

	if (change == scLocal)
	{
		AdjustLayout();
		ApplyScroll();
	}
}

Dummy* ScrollBox::GetClip()
{
	return _clip;
}

Widget* ScrollBox::GetBox()
{
	return _box;
}

bool ScrollBox::GetOption(Option option) const
{
	return _options[option];
}

void ScrollBox::SetOption(Option option, bool value)
{
	_options[option] = value;
	ApplyOptions();
}

const D3DXVECTOR2& ScrollBox::GetArrowSize() const
{
	return _arrowSize;
}

void ScrollBox::SetArrowSize(const D3DXVECTOR2& value)
{
	_arrowSize = value;
	StructureChanged();
}

const D3DXVECTOR2& ScrollBox::GetArrowSpace() const
{
	return _arrowSpace;
}

void ScrollBox::SetArrowSpace(const D3DXVECTOR2& value)
{
	_arrowSpace = value;
	StructureChanged();
}

const D3DXVECTOR2& ScrollBox::GetArrowScrollStep() const
{
	return _arrowScrollStep;
}

void ScrollBox::SetArrowScrollStep(const D3DXVECTOR2& value)
{
	_arrowScrollStep = value;
}

D3DXVECTOR2 ScrollBox::GetMaxScroll() const
{
	return _box->GetChildAABB().GetSize();
}

D3DXVECTOR2 ScrollBox::GetScroll()
{
	return _box->GetPos() - _box->GetChildAABB().max + _box->GetSize()/2.0f;
}

void ScrollBox::SetScroll(const D3DXVECTOR2& value)
{	
	D3DXVECTOR2 boxSize = _box->GetChildAABB().GetSize();

	//корректируем чтобы максимальной прокруткой был размер страницы
	D3DXVECTOR2 clampSize(boxSize.x, std::max(boxSize.y - _clip->GetSize().y, 0.0f));
	D3DXVECTOR2 scroll = value;
	D3DXVec2Maximize(&scroll, &scroll, &NullVec2);
	D3DXVec2Minimize(&scroll, &scroll, &clampSize);

	D3DXVECTOR2 pos = scroll + _box->GetChildAABB().max - _box->GetSize()/2.0f;
	if (_box->GetSize().x > boxSize.x)
		pos.x = 0;
	if (_box->GetSize().y > boxSize.y)
		pos.y = 0;

	_box->SetPos(pos);	

	if (_scrollBut[sdUp])
		_scrollBut[sdUp]->SetVisible(abs(scroll.y) > 0.001f);
	if (_scrollBut[sdDown])
		_scrollBut[sdDown]->SetVisible(abs(scroll.y) < clampSize.y);
}

Material& ScrollBox::GetArrowMaterial()
{
	return *_arrowMat;
}

Material& ScrollBox::GetArrowSelMaterial()
{
	return *_arrowSelMat;
}




ListBox::ListBox(Manager* manager): _MyBase(manager), _itemSize(30.0f, 30.0f), _itemSpace(5.0f, 5.0f), _selItem(0), _fon(0), _frameMaterial(0)
{	
	_event = new MyEvent(this);

	_scrollBox = GetManager().CreateScrollBox();
	_scrollBox->AddRef();
	_scrollBox->SetParent(this);
	_scrollBox->SetOption(ScrollBox::soVert, true);
	_scrollBox->GetClip()->SetEvent(_event);

	SetSize(150.0f, 150.0f);
}

ListBox::~ListBox()
{
	SelectItem(0);

	ClearItems();

	FreeFrame();
	FreeFon();

	_scrollBox->Release();
	//Чтобы не было опасного обновления
	_scrollBox->SetParent(0);
	GetManager().ReleaseWidget(_scrollBox);

	delete _event;
}

ListBox::Item::Item(ListBox* listBox): _listBox(listBox), _data(0), _frame(0)
{
}

ListBox::Item::~Item()
{
	LSL_ASSERT(_data == 0 && _frame == 0);
}

void ListBox::Item::ApplyFrame()
{
	if (_frame && _data)
	{
		LSL_ASSERT(!_data->GetGraphics().IsFind(_frame));

		_data->InsertGraphic(_frame, _data->GetGraphics().begin());		
	}
}

void ListBox::Item::RemoveFrame()
{
	if (_frame && _data)
	{
		Widget::Graphics::const_iterator iter = _data->GetGraphics().Find(_frame);

		LSL_ASSERT(iter != _data->GetGraphics().end());
		
		_data->RemoveGraphic(iter);
	}
}

ListBox* ListBox::Item::GetListBox()
{
	return _listBox;
}

Widget* ListBox::Item::GetData()
{
	return _data;
}

void ListBox::Item::SetData(Widget* value)
{
	if (lsl::Object::ReplaceRef(_data, value))
	{
		RemoveFrame();

		_data = value;
		ApplyFrame();
		if (_data)
			_data->SetParent(_listBox->_scrollBox->GetBox());
	}
}

Graphic* ListBox::Item::GetFrame()
{
	return _frame;
}

void ListBox::Item::SetFrame(Graphic* value)
{
	if (lsl::Object::ReplaceRef(_frame, value))
	{
		RemoveFrame();

		_frame = value;
		ApplyFrame();
	}
}

ListBox::MyEvent::MyEvent(ListBox* listBox): _listBox(listBox)
{
}

bool ListBox::MyEvent::OnClick(Widget* sender, const MouseClick& mClick)
{
	if (sender == _listBox->_scrollBox->GetClip())
	{
		bool res = false;

		Item* item = _listBox->PickItem(mClick.worldCoord);
		if (item)
		{
			_listBox->SelectItem(item);
			res |= _listBox->OnSelect(item);
		}

		return res;
	}

	return false;
}

Graphic* ListBox::NewFrame()
{
	if (_frameMaterial)
	{
		Plane* frame = GetContext().CreatePlane();
		frame->SetMaterial(_frameMaterial);

		return frame;
	}

	return 0;
}

void ListBox::FreeItemFrame(Item* item)
{
	if (item->GetFrame())
	{
		Graphic* frame = item->GetFrame();
		item->SetFrame(0);
		GetContext().ReleaseGraphic(frame);
	}
}

void ListBox::FreeItem(Item* item)
{
	if (item->GetData())
	{
		Widget* data = item->GetData();
		item->SetData(0);
		GetManager().ReleaseWidget(data);
	}

	FreeItemFrame(item);	
}

void ListBox::AlignItems()
{
	D3DXVECTOR2 size = _scrollBox->GetBox()->GetSize();

	int i = 0;
	for (Items::iterator iter = _items.begin(); iter != _items.end(); ++iter, ++i)
	{
		D3DXVECTOR2 itemStep = _itemSize + _itemSpace;
		float stripLen = itemStep.x * i;
		D3DXVECTOR2 clientSize = size / itemStep;
		clientSize.x = floor(clientSize.x);
		clientSize.y = floor(clientSize.y);
		clientSize *= itemStep;
		//необходимо учитывать дополнительный пробел _itemSpace
		D3DXVECTOR2 fracSize = size - clientSize + _itemSpace;

		D3DXVECTOR2 index(0, 0);
		if (abs(clientSize.x > 0.001f) && abs(itemStep.x > 0.001f))
			index.x = floor(fmod(stripLen, clientSize.x) / itemStep.x);
		if (abs(clientSize.x > 0.001f))
			index.y = floor(stripLen / clientSize.x);

		Widget* data = (*iter)->GetData();
		if (data)
		{
			Graphic* frame = (*iter)->GetFrame();
			if (frame)
			{
				frame->SetSize(data->GetLocalAABB(true).GetSize());
				frame->SetPos(data->GetLocalAABB(true).GetCenter());
			}

			D3DXVECTOR2 pos = -size/2.0f + D3DXVECTOR2(fracSize.x/2.0f, 0) + index * itemStep + _itemSize/2.0f;
			pos.y = -pos.y;

			data->SetPos(pos);
		}
	}
}

void ListBox::StructureChanged(StructChange change)
{
	_MyBase::StructureChanged(change);

	if (change == scLocal)
	{
		D3DXVECTOR2 size = GetSize();
		if (_fon)
			_fon->SetSize(size);
		_scrollBox->SetSize(size - IdentityVec2 * 10.0f);
		_scrollBox->SetArrowScrollStep(_itemSize + _itemSpace);
		_scrollBox->SetArrowSpace(_itemSpace);

		for (Items::iterator iter = _items.begin(); iter != _items.end(); ++iter)
		{
			Item* item = *iter;

			if (item->GetData())
				item->GetData()->SetSize(_itemSize);
			if (item->GetFrame())
				item->GetFrame()->SetSize(_itemSize);
		}

		AlignItems();
	}

	if (change == scChild)
	{
		AlignItems();
	}
}

void ListBox::AddItem(Widget* data)
{
	Item* item = new Item(this);
	_items.push_back(item);
	item->SetFrame(NewFrame());
	item->SetData(data);

	StructureChanged();
}

void ListBox::DelItem(Items::const_iterator iter)
{
	LSL_ASSERT((*iter)->GetListBox() == this);

	if (*iter == _selItem)
		SelectItem(0);

	FreeItem(*iter);
	delete *iter;
	_items.erase(iter);

	StructureChanged();
}

void ListBox::DelItem(Items::const_iterator sIter, Items::const_iterator eIter)
{
	while (!_items.empty())
		DelItem(_items.begin());
}

void ListBox::DelItem(Item* item)
{
	DelItem(_items.Find(item));
}

void ListBox::ClearItems()
{
	DelItem(_items.begin(), _items.end());
}

ListBox::Item* ListBox::PickItem(const D3DXVECTOR2& worldCoord)
{
	if (!_scrollBox->GetClip()->GetWorldAABB(false).ContainsPoint(worldCoord))
		return 0;

	for (Items::iterator iter = _items.begin(); iter != _items.end(); ++iter)
		if ((*iter)->GetData() && (*iter)->GetData()->GetWorldAABB(false).ContainsPoint(worldCoord))
			return *iter;

	return 0;
}

bool ListBox::PickItems(const D3DXVECTOR2& worldCoord)
{
	return _scrollBox->GetClip()->GetWorldAABB(false).ContainsPoint(worldCoord);
}

ListBox::Item* ListBox::FindItemByData(Widget* data)
{
	for (Items::iterator iter = _items.begin(); iter != _items.end(); ++iter)
		if ((*iter)->GetData() == data)		
			return *iter;

	return 0;
}

ListBox::Item* ListBox::GetSelItem()
{
	return _selItem; 
}

void ListBox::SelectItem(Item* item)
{
	if (ReplaceRef(_selItem, item))
	{
		_selItem = item;
	}
}

void ListBox::AlignSizeByItems(const D3DXVECTOR2& size)
{
	D3DXVECTOR2 newSize = size;
	newSize = (newSize - GetScrollSpace()) / GetItemPlaceSize();
	//Пои оси X не зачем выравнивать, поскольку прокрутка только вертикальная (и может привести к погрешности)
	//newSize.x = floor(newSize.x);
	newSize.y = floor(newSize.y);
	newSize *= GetItemPlaceSize();
	newSize += GetScrollSpace();

	SetSize(newSize);
}

const ListBox::Items& ListBox::GetItems() const
{
	return _items;
}

const D3DXVECTOR2& ListBox::GetItemSize() const
{
	return _itemSize;
}

void ListBox::SetItemSize(const D3DXVECTOR2& value)
{
	_itemSize = value;
	StructureChanged();
}

const D3DXVECTOR2& ListBox::GetItemSpace() const
{
	return _itemSpace;
}

void ListBox::SetItemSpace(const D3DXVECTOR2& value)
{
	_itemSpace = value;
	StructureChanged();
}

Material& ListBox::GetOrCreateFon()
{
	if (!_fon)
	{
		_fon = GetContext().CreatePlane();
		_fon->AddRef();
		InsertGraphic(_fon);
		
		StructureChanged();
	}

	return *_fon->GetOrCreateMaterial();	
}

void ListBox::FreeFon()
{
	if (_fon)
	{
		RemoveGraphic(_fon);
		_fon->Release();
		GetContext().ReleaseGraphic(_fon);
		_fon = 0;
	}
}

Material& ListBox::GetOrCreateFrame()
{
	if (!_frameMaterial)
	{
		_frameMaterial = new Material();

		for (Items::iterator iter = _items.begin(); iter != _items.end(); ++iter)
			if (!(*iter)->GetFrame())
				(*iter)->SetFrame(NewFrame());

		StructureChanged();
	}

	return *_frameMaterial;
}

void ListBox::FreeFrame()
{
	if (_frameMaterial)
	{
		for (Items::iterator iter = _items.begin(); iter != _items.end(); ++iter)
			FreeItemFrame(*iter);

		delete _frameMaterial;
		_frameMaterial = 0;
	}
}

Material& ListBox::GetArrowMaterial()
{
	return _scrollBox->GetArrowMaterial();
}

Material& ListBox::GetArrowSelMaterial()
{
	return _scrollBox->GetArrowSelMaterial();
}

const D3DXVECTOR2& ListBox::GetArrowSize() const
{
	return _scrollBox->GetArrowSize();
}

void ListBox::SetArrowSize(const D3DXVECTOR2& value)
{
	_scrollBox->SetArrowSize(value);
}

D3DXVECTOR2 ListBox::GetScroll() const
{
	return _scrollBox->GetScroll();
}

void ListBox::SetScroll(const D3DXVECTOR2& value)
{
	_scrollBox->SetScroll(value);
}

D3DXVECTOR2 ListBox::GetMaxScroll() const
{
	return _scrollBox->GetMaxScroll();
}

D3DXVECTOR2 ListBox::GetItemPlaceSize() const
{
	return _itemSize + _itemSpace;
}

D3DXVECTOR2 ListBox::GetScrollSpace() const
{
	return 2.0f * GetArrowSize() + _itemSpace;
}




ProgressBar::ProgressBar(Manager* manager): _MyBase(manager), _style(psHorizontal), _progress(1.0f), _backEnabled(false)
{
	_back = GetContext().CreatePlane();
	_back->AddRef();	
	SetBackEnabled(true);

	_front = GetContext().CreatePlane();
	_front->AddRef();
	InsertGraphic(_front);
}

ProgressBar::~ProgressBar()
{
	SetBackEnabled(false);
	_back->Release();
	GetContext().ReleaseGraphic(_back);

	_front->Release();
}

void ProgressBar::StructureChanged(StructChange change)
{
	_MyBase::StructureChanged(change);

	if (change == scLocal)
	{
		D3DXVECTOR2 pos = GetAlignPos();

		_back->SetSize(GetSize());
		_back->SetPos(pos);

		switch (_style)
		{
		case psHorizontal:
		{
			float barSize = GetSize().x * _progress;
			_front->SetSize(D3DXVECTOR2(barSize, GetSize().y));
			_front->SetPos(pos + D3DXVECTOR2(-GetSize().x/2.0f + barSize/2.0f, 0));
			_front->GetMaterial()->GetSampler().SetScale(D3DXVECTOR3(_progress, 1, 1));
			break;
		}
		case psVertical:
		{
			float barSize = GetSize().y * _progress;
			_front->SetSize(D3DXVECTOR2(GetSize().x, barSize));
			_front->SetPos(pos + D3DXVECTOR2(0, GetSize().y/2.0f - barSize/2.0f));
			_front->GetMaterial()->GetSampler().SetScale(D3DXVECTOR3(1, _progress, 1));
			break;
		}
		}		
	}
}

ProgressBar::Style ProgressBar::GetStyle() const
{
	return _style;
}

void ProgressBar::SetStyle(Style value)
{
	_style = value;
	StructureChanged();
}

Material& ProgressBar::GetFront()
{
	return *_front->GetOrCreateMaterial();
}

void ProgressBar::SetFront(Material* value)
{
	_front->SetMaterial(value);
}

Material& ProgressBar::GetBack()
{
	return *_back->GetOrCreateMaterial();
}

void ProgressBar::SetBack(Material* value)
{
	_back->SetMaterial(value);
}

float ProgressBar::GetProgress() const
{
	return _progress;
}

void ProgressBar::SetProgress(float value)
{
	_progress = lsl::ClampValue(value, 0.0f, 1.0f);
	StructureChanged();
}

bool ProgressBar::GetBackEnaled() const
{
	return _backEnabled;
}

void ProgressBar::SetBackEnabled(bool value)
{
	if (_backEnabled != value)
	{
		_backEnabled = value;

		if (_backEnabled)
			InsertGraphic(_back);
		else
			RemoveGraphic(_back);
	}
}




StreakBar::StreakBar(Manager* manager): _MyBase(manager), _streak(0), _createStreak(false), _streakMax(1), _streakCount(0)
{
	GetOrCreateStreak();
}

StreakBar::~StreakBar()
{
	ClearStreaks();

	SetStreak(0);
}

void StreakBar::UpdateStreaks()
{
	ClearStreaks();

	D3DXVECTOR2 size = GetSize();
	D3DXVECTOR2 streakSize(size.x / _streakMax, size.y);

	for (unsigned i = 0; i < _streakCount; ++i)
	{
		Plane* plane = GetContext().CreatePlane();
		plane->AddRef();
		plane->SetMaterial(_streak);
		plane->SetSize(streakSize);
		plane->SetPos(D3DXVECTOR2(-size.x/2.0f + (i + 0.5f) * streakSize.x, 0.0f));

		InsertGraphic(plane);
		_streaks.push_back(plane);
	}	
}

void StreakBar::ClearStreaks()
{
	for (Streaks::iterator iter = _streaks.begin(); iter != _streaks.end(); ++iter)
	{
		RemoveGraphic(*iter);
		(*iter)->Release();
		GetContext().ReleaseGraphic(*iter);
	}

	_streaks.clear();
}

void StreakBar::StructureChanged(StructChange change)
{
	_MyBase::StructureChanged(change);

	if (change == scLocal)
		UpdateStreaks();
}

Material* StreakBar::GetOrCreateStreak()
{
	if (!_streak)
	{
		_streak = new Material();
		_streak->AddRef();
		_createStreak = true;
	}
	return _streak;	
}

Material* StreakBar::GetStreak()
{
	return _streak;
}

void StreakBar::SetStreak(Material* value)
{
	if (ReplaceRef(_streak, value))
	{
		for (Streaks::iterator iter = _streaks.begin(); iter != _streaks.end(); ++iter)
			(*iter)->SetMaterial(0);

		if (_createStreak)
		{
			_createStreak = false;
			delete _streak;
		}
		_streak = value;

		for (Streaks::iterator iter = _streaks.begin(); iter != _streaks.end(); ++iter)
			(*iter)->SetMaterial(_streak);
	}
}

unsigned StreakBar::GetStreakMax()
{
	return _streakMax;
}

void StreakBar::SetStreakMax(unsigned value)
{
	_streakMax = std::max<unsigned>(value, 1);
	UpdateStreaks();
}

unsigned StreakBar::GetStreakCount()
{
	return _streakCount;
}

void StreakBar::SetStreakCount(unsigned value)
{
	_streakCount = value;
	UpdateStreaks();
}




ChargeBar::ChargeBar(Manager* manager): _MyBase(manager), _space(10.0f, 10.0f)
{
	_myEvent = new MyEvent(this);

	_frame = GetContext().CreatePlane();
	_frame->AddRef();
	InsertGraphic(_frame);

	_up = GetManager().CreateButton();
	_up->SetParent(this);
	_up->SetEvent(_myEvent);

	_bar = GetManager().CreateStreakBar();
	_bar->SetParent(this);

	SetFlag(wfMouseOnClick, false);
}

ChargeBar::~ChargeBar()
{
	GetManager().ReleaseWidget(_bar);
	GetManager().ReleaseWidget(_up);

	_frame->Release();

	delete _myEvent;
}

ChargeBar::MyEvent::MyEvent(ChargeBar* bar): _bar(bar)
{	
}

bool ChargeBar::MyEvent::OnClick(Widget* sender, const MouseClick& mClick)
{
	if (sender == _bar->_up && _bar->GetCharge() < _bar->GetChargeMax())
	{
		return _bar->OnClick(mClick);
	}

	return false;
}

void ChargeBar::AdjustLayout()
{
	D3DXVECTOR2 size = GetSize();

	_frame->SetSize(size);

	_up->SetSize(size.y, size.y);
	_up->SetPos(D3DXVECTOR2(size.x/2.0f - size.y/2.0f, 0.0f));

	_bar->SetSize(size.x - size.y - _space.x/2.0f, size.y - _space.y);
	_bar->SetPos(D3DXVECTOR2((-size.y + _space.x/2.0f)/2.0f, 0.0f));
}

void ChargeBar::StructureChanged(StructChange change)
{
	_MyBase::StructureChanged(change);

	if (change == scLocal)
		AdjustLayout();
}

Material& ChargeBar::GetStreak()
{
	return *_bar->GetOrCreateStreak();
}

Material& ChargeBar::GetFrame()
{
	return *_frame->GetOrCreateMaterial();
}

Material& ChargeBar::GetUp()
{
	return *_up->GetOrCreateFon();
}

Material& ChargeBar::GetUpSel()
{
	return *_up->GetOrCreateSel();	
}

Button* ChargeBar::GetUpButton()
{
	return _up;
}

const D3DXVECTOR2& ChargeBar::GetSpace() const
{
	return _space;
}

void ChargeBar::SetSpace(const D3DXVECTOR2& value)
{
	_space = value;
}

unsigned ChargeBar::GetChargeMax()
{
	return _bar->GetStreakMax();	
}

void ChargeBar::SetChargeMax(unsigned value)
{
	_bar->SetStreakMax(value);
}

unsigned ChargeBar::GetCharge()
{
	return _bar->GetStreakCount();
}

void ChargeBar::SetCharge(unsigned value)
{
	_bar->SetStreakCount(value);
}




VolumeBar::VolumeBar(Manager* manager): Widget(manager), _volume(0.0f), _stepCount(10)
{
	_childEvent = new ChildEvent(this);

	_right = manager->CreateButton();
	_right->SetParent(this);
	_right->SetAlign(waLeft);
	_right->SetEvent(_childEvent);
	_right->SetStyle(Button::bsSelAnim);
	_right->SetRot(D3DX_PI);

	_left = manager->CreateButton();
	_left->SetParent(this);
	_left->SetAlign(waLeft);
	_left->SetEvent(_childEvent);
	_left->SetStyle(Button::bsSelAnim);

	_planeBg = GetContext().CreatePlane();
	InsertGraphic(_planeBg);

	_bar = manager->CreateProgressBar();
	_bar->SetParent(this);

	ApplyVolume();
}

VolumeBar::~VolumeBar()
{
	delete _childEvent;
}

VolumeBar::ChildEvent::ChildEvent(VolumeBar* owner): _owner(owner)
{
}

bool VolumeBar::ChildEvent::OnClick(Widget* sender, const MouseClick& mClick)
{
	if (sender == _owner->_left)
	{
		_owner->SetStep(_owner->GetStep() - 1);
		_owner->OnClick(mClick);
		_owner->OnSelect(NULL);
		return true;
	}

	if (sender == _owner->_right)
	{
		_owner->SetStep(_owner->GetStep() + 1);
		_owner->OnClick(mClick);
		_owner->OnSelect(NULL);
		return true;
	}

	return false;
}

void VolumeBar::ApplyVolume()
{
	_bar->SetProgress(_volume);

	//_left->SetEnabled(_volume > 0 && _volume <= 1.0f);
	//_right->SetEnabled(_volume >= 0 && _volume < 1.0f);
}

void VolumeBar::StructureChanged(StructChange change)
{
	Widget::StructureChanged(change);

	if (change == scLocal)
	{
		D3DXVECTOR2 pos = GetAlignPos();

		_planeBg->SetSize(GetSize());
		_planeBg->SetPos(pos);
		_planeBg->SetActive(false);

		if (_left->GetFon())
		{
			_left->SetSize(_left->GetFon()->GetImageSize());
			_right->SetSize(_left->GetSize());
		}
		if (_left->GetSel())
		{
			_left->SetSelSize(_left->GetSel()->GetImageSize());
			_right->SetSelSize(_left->GetSelSize());
		}

		_left->SetPos(D3DXVECTOR2(-GetSize().x/2, 0.0f) + pos);
		_right->SetPos(D3DXVECTOR2(GetSize().x/2, 0.0f) + pos);

		//_bar->SetSize(D3DXVECTOR2(GetSize().x - _left->GetSize().x - _right->GetSize().x - 30.0f, GetSize().y));
		_bar->SetSize(_bar->GetFront().GetImageSize());
		_bar->SetPos(pos);
	}
}

void VolumeBar::Invalidate()
{
	StructureChanged();
}

Button* VolumeBar::GetLeft()
{
	return _left;
}

Button* VolumeBar::GetRight()
{
	return _right;
}

Material* VolumeBar::GetOrCreateBg()
{
	return _planeBg->GetOrCreateMaterial();
}

Material* VolumeBar::GetBg()
{
	return _planeBg->GetMaterial();
}

void VolumeBar::SetBg(Material* value)
{
	_planeBg->SetMaterial(value);
}

Material* VolumeBar::GetOrCreateArrow()
{
	Material* mat = _left->GetOrCreateFon();
	_right->SetFon(mat);

	return mat;
}

Material* VolumeBar::GetArrow()
{
	return _left->GetFon();
}

void VolumeBar::SetArrow(Material* value)
{
	_right->SetFon(value);
	_left->SetFon(value);
}

Material* VolumeBar::GetOrCreateArrowSel()
{
	Material* mat = _left->GetOrCreateSel();
	_right->SetSel(mat);

	return mat;
}

Material* VolumeBar::GetArrowSel()
{
	return _left->GetSel();
}

void VolumeBar::SetArrowSel(Material* value)
{
	_right->SetSel(value);
	_left->SetSel(value);
}

Material& VolumeBar::GetFront()
{
	return _bar->GetFront();
}

void VolumeBar::SetFront(Material* value)
{
	_bar->SetFront(value);
}

Material& VolumeBar::GetBack()
{
	return _bar->GetBack();
}

void VolumeBar::SetBack(Material* value)
{
	_bar->SetBack(value);
}

float VolumeBar::GetVolume() const
{
	return _volume;
}

void VolumeBar::SetVolume(float value)
{
	_volume = lsl::ClampValue(value, 0.0f, 1.0f);
	ApplyVolume();
}

int VolumeBar::GetStep() const
{
	return std::min(static_cast<int>(floor(_volume * _stepCount)), _stepCount - 1);
}

void VolumeBar::SetStep(int value)
{
	SetVolume(value / static_cast<float>(_stepCount - 1));
}

int VolumeBar::GetStepCount() const
{
	return _stepCount;
}

void VolumeBar::SetStepCount(int value)
{
	_stepCount = value;
	ApplyVolume();
}




ColorBox::ColorBox(Manager* manager): _MyBase(manager), _color(clrWhite), _select(false)
{
	_frame = GetContext().CreatePlane();
	_frame->AddRef();
	InsertGraphic(_frame);

	_box = GetContext().CreatePlane();
	_box->AddRef();
	InsertGraphic(_box);

	_check = GetContext().CreatePlane();
	_check->AddRef();
	InsertGraphic(_check);

	_sel = GetContext().CreatePlane();
	_sel->AddRef();
	InsertGraphic(_sel);

	ApplyColor();
	AdjustLayout();
	ApplySelect();
}

ColorBox::~ColorBox()
{
	_sel->Release();
	_check->Release();
	_box->Release();
	_frame->Release();
}

D3DXVECTOR2 ColorBox::WinToLocal(const D3DXVECTOR2& vec) const
{
	D3DXVECTOR2 size = GetColorBoxSize();
	D3DXVECTOR2 newSize = GetSize();

	float wScale = newSize.x / size.x;
	float hScale = newSize.y / size.y;
	float minScale = std::min(wScale, hScale);
	
	return minScale * vec;
}

void ColorBox::ApplyColor()
{
	_box->GetMaterial()->SetColor(_color);
	_check->GetMaterial()->SetColor(_color);
}

void ColorBox::ApplySelect()
{
	if (_select)
	{
		_sel->GetOrCreateMaterial();
		_sel->GetMaterial()->GetSampler().SetTex(_check->GetMaterial()->GetSampler().GetTex());
		_sel->GetMaterial()->SetBlending(Material::bmAdditive);
		_sel->GetMaterial()->SetColor(clrWhite);
	}
	else
		_sel->SetMaterial(0);
}

void ColorBox::AdjustLayout()
{
	D3DXVECTOR2 size = GetSize();
	AABB2 boxAABB = GetBoxAABB();
	AABB2 checkAABB = GetCheckAABB();

	_frame->SetSize(WinToLocal(GetColorBoxSize()));

	_box->SetPos(WinToLocal(boxAABB.GetCenter()));
	_box->SetSize(WinToLocal(boxAABB.GetSize()));

	_check->SetPos(WinToLocal(checkAABB.GetCenter()));
	_check->SetSize(WinToLocal(checkAABB.GetSize()));

	_sel->SetPos(WinToLocal(checkAABB.GetCenter()));
	_sel->SetSize(WinToLocal(checkAABB.GetSize()));
}

void ColorBox::StructureChanged(StructChange change)
{
	_MyBase::StructureChanged(change);

	if (change == scLocal)
		AdjustLayout();
}

Material& ColorBox::GetFrame()
{
	return *_frame->GetOrCreateMaterial();
}

void ColorBox::SetFrame(Material* value)
{
	_frame->SetMaterial(value);
}

Material& ColorBox::GetBox()
{
	return *_box->GetOrCreateMaterial();
}

Material& ColorBox::GetCheck()
{
	return *_check->GetOrCreateMaterial();
}

const D3DXCOLOR& ColorBox::GetColor() const
{
	return _color;
}

void ColorBox::SetColor(const D3DXCOLOR& value)
{
	_color = value;
	ApplyColor();
}

bool ColorBox::GetSelect() const
{
	return _select;
}

void ColorBox::SetSelect(bool value)
{
	_select = value;
	ApplySelect();
}

D3DXVECTOR2 ColorBox::GetColorBoxSize() const
{
	D3DXVECTOR2 size(190.0f, 40.0f);
	
	return size;
}

AABB2 ColorBox::GetBoxAABB() const
{
	D3DXVECTOR2 size(90.0f, 33.0f);
	D3DXVECTOR2 pos(-25.0f, 0.0f);

	AABB2 aabb(size);
	aabb.Offset(pos);

	return aabb;
}

AABB2 ColorBox::GetCheckAABB() const
{
	D3DXVECTOR2 size(34.0f, 34.0f);
	D3DXVECTOR2 pos(75.0f, 0.0f);

	AABB2 aabb(size);
	aabb.Offset(pos);

	return aabb;
}




ColorList::ColorList(Manager* manager): _MyBase(manager), _space(NullVec2)
{
	_myEvent = new MyEvent(this);
	
	_frame = new Material();
	_box = new Material();
	_check = new Material();

	SetFlag(wfMouseOnClick, false);
}

ColorList::~ColorList()
{
	SelectColor(0);
	ClearColors();

	delete _check;
	delete _box;
	delete _frame;

	delete _myEvent;
}

ColorList::MyEvent::MyEvent(ColorList* list): _list(list)
{
}

bool ColorList::MyEvent::OnClick(Widget* sender, const MouseClick& mClick)
{
	for (Colors::iterator iter = _list->_colors.begin(); iter != _list->_colors.end(); ++iter)
		if (iter->box == sender)
		{
			_list->SelectColor(*iter);
			_list->OnClick(mClick);
			return true;
		}

	return false;	
}

void ColorList::ApplyChanges()
{
	D3DXVECTOR2 size = GetSize();
	D3DXVECTOR2 boxSize = D3DXVECTOR2(size.x, size.y / _colors.size());

	int i = 0;
	for (Colors::iterator iter = _colors.begin(); iter != _colors.end(); ++iter, ++i)
	{
		iter->box->SetSize(boxSize - _space);
		iter->box->SetPos(D3DXVECTOR2(0.0f, size.y/2.0f - (i + 0.5f) * boxSize.y));
		
		iter->box->SetFrame(_frame);
		
		iter->box->GetBox().GetSampler().SetTex(_box->GetSampler().GetTex());
		iter->box->GetBox().SetBlending(_box->GetBlending());

		iter->box->GetCheck().GetSampler().SetTex(_check->GetSampler().GetTex());
		iter->box->GetCheck().SetBlending(_check->GetBlending());
	}
}

void ColorList::StructureChanged(StructChange change)
{
	_MyBase::StructureChanged(change);

	if (change == scLocal)
	{
		ApplyChanges();
	}
}

void ColorList::SelectColor(const MyCol& value)
{
	if (_select != value)
	{
		if (_select)
		{
			_select.box->Release();
			_select.box->SetSelect(false);
		}

		_select = value;

		if (_select)
		{
			_select.box->AddRef();
			_select.box->SetSelect(true);
		}
	}
}

Material& ColorList::GetFrame()
{
	return *_frame;
}

Material& ColorList::GetBox()
{
	return *_box;
}

Material& ColorList::GetCheck()
{
	return *_check;
}

void ColorList::InsertColor(const D3DXCOLOR& value)
{
	LSL_ASSERT(FindColor(value) == _colors.end());

	MyCol myCol;
	myCol.box = GetManager().CreateColorBox();
	myCol.box->AddRef();
	myCol.box->SetParent(this);
	myCol.box->SetEvent(_myEvent);
	myCol.SetVal(value);

	_colors.push_back(myCol);

	ApplyChanges();
}

void ColorList::RemoveColor(Colors::const_iterator iter)
{
	LSL_ASSERT(iter != _colors.end());

	iter->box->Release();
	GetManager().ReleaseWidget(iter->box);

	_colors.erase(iter);

	ApplyChanges();
}

void ColorList::RemoveColor(const D3DXCOLOR& value)
{
	RemoveColor(FindColor(value));
}

void ColorList::ClearColors()
{
	while (!_colors.empty())
		RemoveColor(_colors.begin());
}

ColorList::Colors::const_iterator ColorList::FindColor(const D3DXCOLOR& value) const
{
	for (Colors::const_iterator iter = _colors.begin(); iter != _colors.end(); ++iter)
	{
		D3DXCOLOR vec = iter->GetVal() - value;
		vec.r = abs(vec.r);
		vec.g = abs(vec.g);
		vec.b = abs(vec.b);
		vec.a = abs(vec.a);

		if (vec < 0.001f)
			return iter;
	}

	return _colors.end();
}

const D3DXCOLOR* ColorList::GetSelColor()
{
	return _select ? &_select.GetVal() : 0;
}

void ColorList::SelectColor(const D3DXCOLOR* value)
{
	Colors::const_iterator iter = _colors.end();
	if (value)
		iter = FindColor(*value);

	MyCol val;
	if (iter != _colors.end())
		val = *iter;

	SelectColor(val);
}

const ColorList::Colors& ColorList::GetColors() const
{
	return _colors;
}

const D3DXVECTOR2& ColorList::GetSpace() const
{
	return _space;
}

void ColorList::SetSpace(const D3DXVECTOR2& value)
{
	_space = value;
	ApplyChanges();
}




Grid::Grid(Manager* manager): Widget(manager), _style(gsHorizontal), _cellSize(IdentityVec2), _maxCellsOnLine(0), _hideInvisible(true)
{
}

Grid::~Grid()
{
}

void Grid::Reposition()
{
	int childCount = 0;

	for (Children::const_iterator iter = GetChildren().begin(); iter != GetChildren().end(); ++iter)
	{
		if (_hideInvisible && !(*iter)->GetVisible())
			continue;

		++childCount;
	}

	D3DXVECTOR2 size = D3DXVECTOR2(_maxCellsOnLine != 0 ? std::min(childCount, _maxCellsOnLine) : (float)childCount,
		_maxCellsOnLine != 0 ? ceil(childCount / (float)_maxCellsOnLine) : std::min(childCount, 1));

	switch (_style)
	{
	case gsHorizontal:
		size = size * _cellSize;
		SetSize(size);
		break;
	case gsVertical:
		SetSize(size.y * _cellSize.x, size.x * _cellSize.y);
		size = GetSize();
		break;
	}

	int i = 0;
	for (Children::const_iterator iter = GetChildren().begin(); iter != GetChildren().end(); ++iter)
	{
		Widget* child = *iter;
		if (_hideInvisible && !child->GetVisible())
			continue;

		int x = _maxCellsOnLine != 0 ? i % _maxCellsOnLine : i;
		int y = _maxCellsOnLine != 0 ? i / _maxCellsOnLine : 0;

		D3DXVECTOR2 pos = NullVec2;

		switch (_style)
		{
		case gsHorizontal:
			pos.x = x * _cellSize.x;
			pos.y = y * _cellSize.y;
			break;
		case gsVertical:
			pos.x = y * _cellSize.x;
			pos.y = x * _cellSize.y;
			break;
		}

		child->SetPos(-size/2 + _cellSize/2 + pos + GetAlignPos());
		++i;
	}
}

Grid::Style Grid::style() const
{
	return _style;
}

void Grid::style(Style value)
{
	_style = value;
}

D3DXVECTOR2 Grid::cellSize() const
{
	return _cellSize;
}

void Grid::cellSize(const D3DXVECTOR2& value)
{
	_cellSize = value;
}

int Grid::maxCellsOnLine() const
{
	return _maxCellsOnLine;
}

void Grid::maxCellsOnLine(int value)
{
	_maxCellsOnLine = value;
}

bool Grid::hideInvisible() const
{
	return _hideInvisible;
}

void Grid::hideInvisible(bool value)
{
	_hideInvisible = value;
}




Manager::Manager(graph::Engine* engine): _engine(engine), _camera3d(0), _root(0), _clipWidget(0), _safeMode(false), _invertY(true)
{
	LSL_ASSERT(_engine);

	_engine->AddRef();
	_context = new Context(_engine);

	_root = CreateDummy();
	_root->AddRef();
}

Manager::~Manager()
{
	LSL_ASSERT(!IsSafeMode());

	_root->Release();
	ReleaseWidget(_root);

	LSL_ASSERT(_widgetList.Empty() && _topmostWidgets.empty() && _modalWidgets.empty());

	delete _context;
	_engine->Release();
}

void Manager::BeginSafeMode()
{
	LSL_ASSERT(!_safeMode);

	_safeMode = true;
}

void Manager::EndSafeMode()
{
	LSL_ASSERT(_safeMode);

	_safeMode = false;

	for (WidgetList::iterator iter = _safeList.begin(); iter != _safeList.end(); ++iter)
	{
		(*iter)->Release();
		delete *iter;
	}
	_safeList.Clear();
}

bool Manager::IsSafeMode() const
{
	return _safeMode;
}

void Manager::InsertWidget(Widget* widget)
{
	widget->AddRef();
	_widgetList.Insert(widget);

	if (_root)
		_root->InsertChild(widget);
}

void Manager::InsertTopmostWidget(Widget* value)
{
	LSL_ASSERT(_topmostWidgets.IsFind(value) == false);

	Widgets::const_iterator place = _topmostWidgets.end();
	for (Widgets::const_iterator iter = _topmostWidgets.begin(); iter != _topmostWidgets.end(); ++iter)
		if ((*iter)->GetTopmostLevel() > value->GetTopmostLevel())
		{
			place = iter;
			break;
		}

	_topmostWidgets.insert(place, value);
	value->AddRef();
}

void Manager::RemoveTopmostWidget(Widget* value)
{
	LSL_ASSERT(_topmostWidgets.IsFind(value) == true);

	value->Release();
	_topmostWidgets.Remove(value);
}

void Manager::PushModalWidget(Widget* value)
{
	LSL_ASSERT(_modalWidgets.IsFind(value) == NULL);

	value->AddRef();
	_modalWidgets.push_back(value);
}

void Manager::PopModalWidget(Widget* value)
{
	LSL_ASSERT(_modalWidgets.IsFind(value) != NULL);

	value->Release();
	_modalWidgets.Remove(value);
}

void Manager::DoDrawWidget(Widget* widget)
{
	if (!widget->GetVisible())
		return;

	if (widget->GetFlag(Widget::wfClientClip))
	{
		//ограничение, только один clipWidget
		LSL_ASSERT(_clipWidget == 0);

		_clipWidget = widget;
		_clipWidget->AddRef();

		AABB2 aabb = widget->GetWorldAABB(false);

		_engine->GetContext().SetRenderState(graph::rsScissorTestEnable, true);
		RECT rc;
		rc.left = static_cast<int>(aabb.min.x);		
		rc.right = static_cast<int>(aabb.max.x);
		if (_invertY)
		{
			rc.top = static_cast<int>(_context->GetVPSize().y - aabb.max.y);
			rc.bottom = static_cast<int>(_context->GetVPSize().y - aabb.min.y);
		}
		else
		{
			rc.top = static_cast<int>(aabb.min.y);
			rc.bottom = static_cast<int>(aabb.max.y);
		}
		_engine->GetDriver().GetDevice()->SetScissorRect(&rc);
	}

	bool drawWidget = true;
	bool drawChild = true;
	if (_clipWidget)
	{
		AABB2 clip = _clipWidget->GetWorldAABB(false);
		AABB2 widgetAABB = widget->GetWorldAABB(false);
		AABB2 childAABB = widget->GetChildAABB();
		childAABB.Transform(widget->GetWorldMat());

		drawWidget = clip.ContainsAABB(widgetAABB) != AABB2::scNoOverlap;
		drawChild = clip.ContainsAABB(childAABB) != AABB2::scNoOverlap;
	}
	if (drawWidget)
	{
		//убираем флаг IsMouseOver если вдруг произошло залипание
		if (widget->IsMouseOver() && !widget->GetWorldAABB(false).ContainsPoint(_mMove.worldCoord))
		{
			widget->_isMouseOver = false;
			widget->ApplyMouseEnter(true);
		}

		if (widget->_coord3d || widget->_anchorVP != _context->GetVPSize())
			widget->TransformChanged();
		_context->SetTransform(widget->GetWorldMat());

		for (Widget::Graphics::const_iterator iter = widget->GetGraphics().begin(); iter != widget->GetGraphics().end(); ++iter)
			if ((*iter)->GetActive() && (*iter)->GetMaterial())
				(*iter)->Draw();
	}
	if (drawChild)
	{
		for (Widget::Children::const_iterator iter = widget->GetChildren().begin(); iter != widget->GetChildren().end(); ++iter)
			if ((*iter)->GetFlag(Widget::wfTopmost) == false)
				DrawWidget(*iter);
	}

	if (widget->GetFlag(Widget::wfClientClip))
	{
		_engine->GetContext().RestoreRenderState(graph::rsScissorTestEnable);

		lsl::SafeRelease(_clipWidget);
	}
}

void Manager::DrawWidget(Widget* widget)
{
	if (widget->GetFlag(Widget::wfTopmost) || widget->modal())
		return;

	DoDrawWidget(widget);	
}

bool Manager::OnWidgetMouseDown(Widget* widget, const MouseClick& mClick)
{
	if (!widget->GetVisible())
		return false;

	Widget* test = dynamic_cast<ListBox*>(widget);

	if (widget->GetFlag(Widget::wfClientClip) && !widget->GetWorldAABB(false).ContainsPoint(mClick.coord))
		return false;

	for (Widget::Children::const_reverse_iterator iter = widget->GetChildren().rbegin(); iter != widget->GetChildren().rend(); ++iter)	
		if (OnWidgetMouseDown(*iter, mClick))
			return true;

	MouseClick click = GetMouseClick(widget);

	if (widget->GetEnabled() && widget->OnMouseDown(click))
		return true;

	if (widget->GetEnabled() && widget->GetLocalAABB(false).ContainsPoint(click.coord) && widget->OnMouseClick(click))
		return true;

	return false;
}

bool Manager::OnWidgetMouseMove(Widget* widget, const MouseMove& mMove)
{
	if (!widget->GetVisible())
		return false;
	if (widget->GetFlag(Widget::wfClientClip) && !widget->GetWorldAABB(false).ContainsPoint(mMove.coord))
		return false;

	for (Widget::Children::const_reverse_iterator iter = widget->GetChildren().rbegin(); iter != widget->GetChildren().rend(); ++iter)
		if (OnWidgetMouseMove(*iter, mMove))
			return true;

	MouseMove locMove = GetMouseMove(widget);
	//
	//Порядок именно такой
	if (widget->GetEnabled() && widget->OnMouseMove(_mMove))
		return true;
	//
	if (widget->GetEnabled() && widget->GetLocalAABB(false).ContainsPoint(locMove.coord) && widget->OnMouseOver(locMove))
		return true;

	return false;
}

void Manager::Draw()
{
	_context->SetInvertY(_invertY);
	_context->SetVPSize(GetVPSize());
	_context->BeginDraw();
	
	DrawWidget(_root);

	for (Widgets::iterator iter = _topmostWidgets.begin(); iter != _topmostWidgets.end(); ++iter)
		DoDrawWidget(*iter);

	_context->EndDraw();
}

bool Manager::GetInvertY() const
{
	return _invertY;
}

void Manager::SetInvertY(bool value)
{
	if (_invertY == value)
		return;

	_invertY = value;

	for (Widgets::const_iterator iter = _widgetList.begin(); iter != _widgetList.end(); ++iter)
		if ((*iter)->GetAnchor() != Widget::waNone)
			(*iter)->TransformChanged();
}

graph::Camera* Manager::GetCamera3d()
{
	return _camera3d;
}

void Manager::SetCamera3d(graph::Camera* value)
{
	if (Object::ReplaceRef(_camera3d, value))
		_camera3d = value;
}

D3DXVECTOR2 Manager::ScreenToView(const Point& point)
{
	if (_invertY)
		return D3DXVECTOR2(static_cast<float>(point.x), static_cast<float>(GetVPSize().y - point.y));
	else
		return D3DXVECTOR2(static_cast<float>(point.x), static_cast<float>(point.y));
}

D3DXVECTOR2 Manager::WorldToView(const D3DXVECTOR3& coord)
{
	if (_camera3d == NULL)
		return NullVec2;

	D3DXVECTOR2 vec = GetVPSize();
	vec = _camera3d->GetContextInfo().WorldToScreen(coord, vec);

	if (_invertY)
		vec.y = GetVPSize().y - vec.y;

	return vec;
}

MouseClick Manager::GetMouseClick(Widget* widget) const
{
	MouseClick mClick = _mClick;
	mClick.coord = widget->WorldToLocalCoord(_mClick.coord);

	return mClick;
}

MouseMove Manager::GetMouseMove(Widget* widget) const
{
	MouseMove mMove = _mMove;
	mMove.coord = widget->WorldToLocalCoord(_mMove.coord);

	return mMove;
}

bool Manager::OnMouseClickEvent(MouseKey key, KeyState state, const Point& coord, bool shift, bool ctrl)
{
	bool res = false;

	BeginSafeMode();
	try
	{
		D3DXVECTOR2 vpSize = GetVPSize();

		_mClick.key = key;
		_mClick.state = state;
		_mClick.coord = ScreenToView(coord);
		_mClick.worldCoord = _mClick.coord;
		_mClick.shift1 = shift;

		if (_modalWidgets.size() > 0)
		{
			OnWidgetMouseDown(_modalWidgets.back(), _mClick);
			res = true;
		}

		if (!res)
			for (Widgets::reverse_iterator iter = _topmostWidgets.rbegin(); iter != _topmostWidgets.rend(); ++iter)
				if (OnWidgetMouseDown(*iter, _mClick))
				{
					res = true;
					break;
				}

		if (!res)
			res = OnWidgetMouseDown(_root, _mClick);
	}
	LSL_FINALLY(EndSafeMode();)

	return res;
}

bool Manager::OnMouseMoveEvent(const Point& coord, bool shift, bool ctrl)
{
	bool res = false;

	BeginSafeMode();
	try
	{
		D3DXVECTOR2 vpSize = GetVPSize();
		D3DXVECTOR2 fCoord = ScreenToView(coord);

		_mMove.shift1 = shift;
		_mMove.dtCoord = fCoord - _mMove.coord;
		_mMove.offCoord = fCoord - _mClick.coord;
		_mMove.coord = fCoord;
		_mMove.worldCoord = _mMove.coord;
		_mMove.click = _mClick;

		if (_modalWidgets.size() > 0)
		{
			OnWidgetMouseMove(_modalWidgets.back(), _mMove);
			res = true;
		}

		if (!res)
			for (Widgets::reverse_iterator iter = _topmostWidgets.rbegin(); iter != _topmostWidgets.rend(); ++iter)
				if (OnWidgetMouseMove(*iter, _mMove))
				{
					res = true;
					break;
				}

		if (!res)
			res = OnWidgetMouseMove(_root, _mMove);
	}
	LSL_FINALLY(EndSafeMode();)

	return res;
}

Dummy* Manager::CreateDummy()
{
	Dummy* res = new Dummy(this);
	InsertWidget(res);
	return res;
}

WidgetCont* Manager::CreateWidgetCont()
{
	WidgetCont* res = new WidgetCont(this);
	InsertWidget(res);
	return res;
}

PlaneFon* Manager::CreatePlaneFon()
{
	PlaneFon* res = new PlaneFon(this);
	InsertWidget(res);
	return res;
}

Label* Manager::CreateLabel()
{
	Label* res = new Label(this);
	InsertWidget(res);
	return res;
}

Button* Manager::CreateButton()
{
	Button* res = new Button(this);
	InsertWidget(res);
	return res;
}

StepperBox* Manager::CreateStepperBox()
{
	StepperBox* res = new StepperBox(this);
	InsertWidget(res);
	return res;
}

VolumeBar* Manager::CreateVolumeBar()
{
	VolumeBar* res = new VolumeBar(this);
	InsertWidget(res);
	return res;
}

DropBox* Manager::CreateDropBox()
{
	DropBox* res = new DropBox(this);
	InsertWidget(res);
	return res;
}

TrackBar* Manager::CreateTrackBar()
{
	TrackBar* res = new TrackBar(this);
	InsertWidget(res);
	return res;
}

ViewPort3d* Manager::CreateViewPort3d()
{
	ViewPort3d* res = new ViewPort3d(this);
	InsertWidget(res);
	return res;
}

ScrollBox* Manager::CreateScrollBox()
{
	ScrollBox* res = new ScrollBox(this);
	InsertWidget(res);
	return res;
}

ListBox* Manager::CreateListBox()
{
	ListBox* listBox = new ListBox(this);
	InsertWidget(listBox);
	return listBox;
}

ProgressBar* Manager::CreateProgressBar()
{
	ProgressBar* progressBar = new ProgressBar(this);
	InsertWidget(progressBar);
	return progressBar;
}

StreakBar* Manager::CreateStreakBar()
{
	StreakBar* streakBar = new StreakBar(this);
	InsertWidget(streakBar);
	return streakBar;
}

ChargeBar* Manager::CreateChargeBar()
{
	ChargeBar* chargeBar = new ChargeBar(this);
	InsertWidget(chargeBar);
	return chargeBar;
}

ColorBox* Manager::CreateColorBox()
{
	ColorBox* colorBox = new ColorBox(this);
	InsertWidget(colorBox);
	return colorBox;
}

ColorList* Manager::CreateColorList()
{
	ColorList* colorList = new ColorList(this);
	InsertWidget(colorList);
	return colorList;
}

Grid* Manager::CreateGrid()
{
	Grid* grid = new Grid(this);
	InsertWidget(grid);
	return grid;
}

void Manager::ReleaseWidget(Widget* widget)
{
	Widgets::iterator iter = _widgetList.Find(widget);
	if (iter != _widgetList.end())
	{
		_widgetList.Remove(iter);
		widget->Dispose();

		if (IsSafeMode())	
			_safeList.Insert(widget);
		else
		{
			widget->Release();
			delete widget;
		}
	}
}

Context& Manager::GetContext()
{
	return *_context;
}

Dummy* Manager::GetRoot()
{
	return _root;
}

D3DXVECTOR2 Manager::GetVPSize()
{
	return D3DXVECTOR2(static_cast<float>(_engine->GetParams().BackBufferWidth), static_cast<float>(_engine->GetParams().BackBufferHeight));
}

D3DXVECTOR2 Manager::GetMousePos()
{
	return _mMove.coord;
}

}

}