#include "stdafx.h"

#include "graph\\StdNode.h"
#include "lslSerialValue.h"

namespace r3d
{

namespace graph
{

const D3DXVECTOR3 MovCoordSys::arUp[3] = {XVector, YVector, ZVector};
const float MovCoordSys::arSize = 2.0f;
const D3DXVECTOR3 MovCoordSys::arPos[3] = {2 * XVector, 2 * YVector, 2 * ZVector};
const D3DXCOLOR MovCoordSys::arCol[3] = {clrRed, clrGreen, clrBlue};
const D3DXCOLOR MovCoordSys::colSel = clrYellow;

const D3DXVECTOR3 ScaleCoordSys::arUp[3] = {XVector, YVector, ZVector};
const float ScaleCoordSys::arSize = 2.0f;
const float ScaleCoordSys::plSize = 1.5f;
const D3DXCOLOR ScaleCoordSys::arCol[3] = {clrRed, clrGreen, clrBlue};
const D3DXCOLOR ScaleCoordSys::colSel = clrYellow;




MaterialNode::MaterialNode(): _libMat(0), _color(clrWhite), _offset(NullVector), _scale(IdentityVector), _rotate(NullQuaternion), _matChanged(true), _defMat(true), _cullMode((D3DCULL)0)
{
}

MaterialNode::~MaterialNode()
{
	Clear();
}

void MaterialNode::TransformationChanged() const
{
	_matChanged = true;
	_defMat = _offset == NullVector && _scale == IdentityVector && _rotate == NullQuaternion;
}

const D3DXMATRIX& MaterialNode::GetMatrix() const
{
	if (_matChanged)
	{
		_matChanged = false;

		_defMat = _offset == NullVector && _scale == IdentityVector && _rotate == NullQuaternion;
		_matrix = _defMat ? IdentityMatrix : BuildWorldMatrix(_offset, _scale, _rotate);
	}

	return _matrix;
}

void MaterialNode::Begin(Engine& engine)
{
	engine.GetContext().SetColor(_color);	

	if (!_defMat)
		engine.GetContext().PushTextureTransform(0, GetMatrix());
}

void MaterialNode::End(Engine& engine)
{
	engine.GetContext().SetColor(clrWhite);

	if (!_defMat)
		engine.GetContext().PopTextureTransform(0);
}

void MaterialNode::BeginAfter(Engine& engine)
{
	if (_cullMode != 0)
		engine.GetContext().SetRenderState(graph::rsCullMode, _cullMode);
}

void MaterialNode::EndAfter(Engine& engine)
{
	if (_cullMode != 0)
		engine.GetContext().RestoreRenderState(graph::rsCullMode);
}

void MaterialNode::Save(lsl::SWriter* writer, lsl::Serializable* owner)
{
	if (_libMat)
		writer->WriteValue("libMat", _libMat);

	if (_materials.size() > 0)
	{
		lsl::SWriter* child = writer->NewDummyNode("materials");
		int ind = 0;
		for (Materials::const_iterator iter = _materials.begin(); iter != _materials.end(); ++iter, ++ind)
		{
			if (_libMat && ind == 0)
				continue;

			std::stringstream sstream;
			sstream << "material_"<<ind;
			child->WriteRef(sstream.str().c_str(), *iter);
		}
	}

	lsl::SWriteValue(writer, "color", _color);
	lsl::SWriteValue(writer, "cullMode", (int)_cullMode);
}

void MaterialNode::Load(lsl::SReader* reader, lsl::Serializable* owner)
{
	int intVal = 0;

	Clear();

	if (lsl::SReader* child = reader->ReadValue("libMat"))
		child->LoadSerializable(GetOrCreate());

	if (lsl::SReader* child = reader->ReadValue("materials"))
	{
		lsl::SReader* value = child->FirstChildValue();
		while (value)
		{
			value->AddFixUp(true, owner, 0);
			value = value->NextValue();
		}
	}

	lsl::SReadValue(reader, "color", _color);
	lsl::SReadValue(reader, "cullMode", intVal);
	_cullMode = (D3DCULL)intVal;
}

void MaterialNode::OnFixUp(const lsl::Serializable::FixUpNames& fixUpNames, lsl::Serializable* owner)
{
	for (lsl::Serializable::FixUpNames::const_iterator iter = fixUpNames.begin(); iter != fixUpNames.end(); ++iter)
	{
		if (lsl::SReader* parent = iter->sender->GetOwnerValue())
			if (parent->GetMyName() == "materials")
				Insert(static_cast<graph::LibMaterial*>(iter->collItem));
	}
}

void MaterialNode::Insert(graph::LibMaterial* mat, Materials::const_iterator iter)
{
	_materials.insert(iter, mat);
	mat->AddRef();
}

void MaterialNode::Insert(graph::LibMaterial* mat)
{
	Insert(mat, _materials.end());
}

MaterialNode::Materials::iterator MaterialNode::Remove(Materials::const_iterator iter)
{
	(*iter)->Release();

	return _materials.erase(iter);
}

MaterialNode::Materials::iterator MaterialNode::Remove(graph::LibMaterial* mat)
{
	return Remove(_materials.Find(mat));
}

void MaterialNode::Clear()
{
	Set(NULL);

	for (Materials::iterator iter = _materials.begin(); iter != _materials.end(); ++iter)
		(*iter)->Release();

	_materials.clear();
}

int MaterialNode::Count() const
{
	return _materials.size();
}

void MaterialNode::Apply(Engine& engine, int index)
{
	Begin(engine);

	LibMaterial* mat = Get(index);
	if (mat)
		mat->Apply(engine);

	BeginAfter(engine);
}

void MaterialNode::UnApply(Engine& engine, int index)
{
	End(engine);

	LibMaterial* mat = Get(index);
	if (mat)
		mat->UnApply(engine);

	EndAfter(engine);
}

void MaterialNode::Apply(Engine& engine)
{
	Begin(engine);

	for (unsigned i = 0; i < _materials.size(); ++i)
		_materials[i]->Apply(engine);

	BeginAfter(engine);
}

void MaterialNode::UnApply(Engine& engine)
{
	End(engine);

	for (unsigned i = 0; i < _materials.size(); ++i)
		_materials[i]->UnApply(engine);

	EndAfter(engine);
}

graph::LibMaterial* MaterialNode::Get(unsigned index)
{
	return index < _materials.size() ? _materials[index] : NULL;
}

bool MaterialNode::Set(unsigned index, graph::LibMaterial* value)
{
	if (index == 0 && _libMat)
	{
		if (value == NULL)
			_materials.erase(_materials.begin());
		_libMat->Release();
		lsl::SafeDelete(_libMat);
	}

	if (value == NULL)
		return false;

	if (index < _materials.size())
	{
		lsl::Object::ReplaceRef(_materials[index], value);
		_materials[index] = value;
		return true;
	}

	return false;
}

graph::LibMaterial* MaterialNode::Get()
{
	return _materials.size() > 0 ? _materials[0] : NULL;
}

graph::LibMaterial* MaterialNode::GetOrCreate()
{
	if (Get())
		return Get();

	_libMat = new LibMaterial();
	_libMat->AddRef();
	_materials.push_back(_libMat);

	return _libMat;
}

void MaterialNode::Set(graph::LibMaterial* value)
{
	if (!Set(0, value) && value)
		Insert(value);
}

const MaterialNode::Materials& MaterialNode::GetList() const
{
	return _materials;
}

const D3DXCOLOR& MaterialNode::GetColor() const
{
	return _color;
}

void MaterialNode::SetColor(const D3DXCOLOR& value)
{
	_color = value;
}

const D3DXVECTOR3& MaterialNode::GetOffset() const
{
	return _offset;
}

void MaterialNode::SetOffset(const D3DXVECTOR3& value)
{
	_offset = value;
	TransformationChanged();
}

const D3DXVECTOR3& MaterialNode::GetScale() const
{
	return _scale;
}

void MaterialNode::SetScale(const D3DXVECTOR3& value)
{
	_scale = value;
	TransformationChanged();
}

const D3DXQUATERNION& MaterialNode::GetRotate() const
{
	return _rotate;
}

void MaterialNode::SetRotate(const D3DXQUATERNION& value)
{
	_rotate = value;
	TransformationChanged();
}

D3DCULL MaterialNode::GetCullMode() const
{
	return _cullMode;
}

void MaterialNode::SetCullMode(D3DCULL value)
{
	_cullMode = value;
}




IVBMeshNode::IVBMeshNode(): _mesh(0), _meshId(-1)
{
}

IVBMeshNode::~IVBMeshNode()
{
	SetMesh(0);
}

void IVBMeshNode::DoRender(Engine& engine)
{
	LSL_ASSERT(_mesh);

	//engine.GetContext().SetRenderState(graph::rsFillMode, D3DFILL_WIREFRAME);

	_mesh->Init(engine);
	
	int curMeshId = engine.GetContext().GetMeshId() != -1 ? engine.GetContext().GetMeshId() : _meshId;
	int meshIgnore = (curMeshId != -1 && (curMeshId & ContextInfo::cMeshIdIgnore)) ? (curMeshId & ~ContextInfo::cMeshIdIgnore) : -1;
	if (meshIgnore >= 0)
		curMeshId = -1;

	int numFaceGr = _mesh->GetSubsetCount();
	int numMat = material.Count();
	int cntPass = curMeshId > 0 ? 1 : std::min(numMat, numFaceGr);
	cntPass = std::max(cntPass, 1);

	for (int i = 0; i < cntPass; ++i)
	{
		//Индекс для субмеша
		//Если количество проходов равно единице то meshId = -1, т.е. рендер всех субмешей
		//Если определен субмеш, то рендерится только он
		int meshId = curMeshId < 0 ? (cntPass > 1 ? i : -1) : curMeshId;
		if (meshIgnore >= 0 && meshIgnore == meshId)
			continue;

		//Индекс для материала
		int matInd = std::min(i, numMat - 1);

		if (matInd >= 0)
			material.Apply(engine, matInd);
		else
		{
			D3DMATERIAL9 mat;
			mat.Ambient = clrWhite;
			mat.Diffuse = clrWhite;
			mat.Specular = clrWhite;
			mat.Emissive = clrBlack;
			mat.Power = 32.0f;
			engine.GetDriver().GetDevice()->SetMaterial(&mat);
		}

		do
		{
			engine.BeginDraw();
			
			if (meshId < 0)
			{
				if (meshIgnore < 0)
					_mesh->Draw();
				else
					for (int j = 0; j < numFaceGr; ++j)
						if (j != meshIgnore)
							_mesh->DrawSubset(j);
			}
			else
			{
				_mesh->DrawSubset(meshId);

				//Рендерим оставшиеся субмеши для послденего материала. Только если не выбран конкретный субмеш
				if (meshId < 0 && i == cntPass - 1)
					for (int j = i + 1; j < numFaceGr; ++j)
						_mesh->DrawSubset(j);
			}			
		}
		while (!engine.EndDraw(true));

		if (matInd >= 0)
			material.UnApply(engine, matInd);
	}

	//engine.GetContext().RestoreRenderState(graph::rsFillMode);
}

void IVBMeshNode::Save(lsl::SWriter* writer)
{
	_MyBase::Save(writer);

	writer->WriteRef("mesh", _mesh);
	writer->WriteValue("meshId", _meshId);

	material.Save(writer, this);
}

void IVBMeshNode::Load(lsl::SReader* reader)
{
	_MyBase::Load(reader);	

	reader->ReadRef("mesh", true, this, 0);
	reader->ReadValue("meshId", _meshId);

	material.Load(reader, this);
}

void IVBMeshNode::OnFixUp(const FixUpNames& fixUpNames)
{
	_MyBase::OnFixUp(fixUpNames);

	for (FixUpNames::const_iterator iter = fixUpNames.begin(); iter != fixUpNames.end(); ++iter)
	{
		if (iter->name == "mesh")	
		{
			SetMesh(static_cast<graph::IndexedVBMesh*>(iter->collItem));
			break;
		}
	}

	material.OnFixUp(fixUpNames, this);
}

void IVBMeshNode::Render(Engine& engine, int meshId)
{
	int tmp = _meshId;
	_meshId = meshId;

	Render(engine);

	_meshId = tmp;
}

AABB IVBMeshNode::LocalDimensions() const
{
	if (!_mesh)
		return NullAABB;

	D3DXVECTOR3 min = NullVector;
	D3DXVECTOR3 max = NullVector;
	if (_meshId < 0)	
	{
		min = _mesh->GetMinPos();
		max = _mesh->GetMaxPos();
	}
	else
	{
		if (!_mesh->GetData()->IsInit())
			_mesh->GetData()->Load();

		const res::FaceGroup& fg = _mesh->GetData()->faceGroups.at(_meshId);

		bool bMin = false;
		bool bMax = false;
		for (int i = 0; i < fg.faceCnt; ++i)		
			for (int j = 0; j < 3; ++j)
			{
				unsigned ind = _mesh->GetData()->fb.GetIndex(fg.sFace + i, j);
				D3DXVECTOR3 vec =  *(_mesh->GetData()->vb[ind].Pos3());

				if (!bMin)
				{
					min = vec;
					bMin = true;
				}
				else
					D3DXVec3Minimize(&min, &min, &vec);

				if (!bMax)
				{
					max = vec;
					bMax = true;
				}
				else
					D3DXVec3Maximize(&max, &max, &vec);
			}
	}

	return AABB(min, max);
}

graph::IndexedVBMesh* IVBMeshNode::GetMesh()
{
	return _mesh;
}

void IVBMeshNode::SetMesh(graph::IndexedVBMesh* value)
{
	if (ReplaceRef(_mesh, value))
	{
		_mesh = value;
		StructureChanged();
	}
}

int IVBMeshNode::GetMeshId() const
{
	return _meshId;
}

void IVBMeshNode::SetMeshId(int value)
{
	if (_meshId != value)
	{
		_meshId = value;
		StructureChanged();
	}
}

unsigned IVBMeshNode::GetSubsetCnt() const
{
	LSL_ASSERT(_mesh);

	return _mesh->GetSubsetCount();
}

MaterialNode* IVBMeshNode::GetMaterial()
{
	return &material;
}




MeshXNode::MeshXNode(): _mesh(0), _meshId(-1)
{
}

MeshXNode::~MeshXNode()
{
	SetMesh(0);
}

void MeshXNode::DoRender(Engine& engine)
{
	LSL_ASSERT(_mesh);

	//engine.GetContext().SetRenderState(graph::rsFillMode, D3DFILL_WIREFRAME);

	_mesh->Init(engine);

	int numFaceGr = _mesh->GetSubsetCount();
	int numMat = material.Count();
	int cntPass = _meshId > 0 ? 1 : std::min(numMat, numFaceGr);
	cntPass = std::max(cntPass, 1);

	for (int i = 0; i < cntPass; ++i)
	{
		//Индекс для субмеша
		//Если количество проходов равно единице то meshId = -1, т.е. рендер всех субмешей
		//Если определен субмеш, то рендерится только он
		int meshId = _meshId < 0 ? (cntPass > 1 ? i : -1) : _meshId;
		//Индекс для материала
		int matInd = std::min(i, numMat - 1);

		if (matInd >= 0)
			material.Apply(engine, matInd);
		else
		{
			D3DMATERIAL9 mat;
			mat.Ambient = clrWhite;
			mat.Diffuse = clrWhite;
			mat.Specular = clrWhite;
			mat.Emissive = clrBlack;
			mat.Power = 32.0f;
			engine.GetDriver().GetDevice()->SetMaterial(&mat);
		}

		do
		{
			engine.BeginDraw();

			if (meshId < 0)
			{
				_mesh->Draw();
			}
			else
			{
				_mesh->DrawSubset(meshId);

				//Рендерим оставшиеся субмеши для послденего материала. Только если не выбран конкретный субмеш
				if (meshId < 0 && i == cntPass - 1)
					for (int j = i + 1; j < numFaceGr; ++j)
						_mesh->DrawSubset(j);
			}			
		}
		while (!engine.EndDraw(true));

		if (matInd >= 0)
			material.UnApply(engine, matInd);
	}

	//engine.GetContext().RestoreRenderState(graph::rsFillMode);
}

void MeshXNode::Save(lsl::SWriter* writer)
{
	_MyBase::Save(writer);

	writer->WriteRef("mesh", _mesh);
	writer->WriteValue("meshId", _meshId);

	material.Save(writer, this);
}

void MeshXNode::Load(lsl::SReader* reader)
{
	_MyBase::Load(reader);	

	reader->ReadRef("mesh", true, this, 0);
	reader->ReadValue("meshId", _meshId);

	material.Load(reader, this);
}

void MeshXNode::OnFixUp(const FixUpNames& fixUpNames)
{
	_MyBase::OnFixUp(fixUpNames);

	for (FixUpNames::const_iterator iter = fixUpNames.begin(); iter != fixUpNames.end(); ++iter)
	{
		if (iter->name == "mesh")
		{
			SetMesh(static_cast<graph::MeshX*>(iter->collItem));
			break;
		}
	}

	material.OnFixUp(fixUpNames, this);
}

void MeshXNode::Render(Engine& engine, int meshId)
{
	int tmp = _meshId;
	_meshId = meshId;
	Render(engine);
	_meshId = tmp;
}

AABB MeshXNode::LocalDimensions() const
{
	if (!_mesh)
		return NullAABB;

	D3DXVECTOR3 min = NullVector;
	D3DXVECTOR3 max = NullVector;
	if (_meshId < 0)	
	{
		min = _mesh->GetMinPos();
		max = _mesh->GetMaxPos();
	}
	else
	{
		if (!_mesh->GetData()->IsInit())
			_mesh->GetData()->Load();

		const res::FaceGroup& fg = _mesh->GetData()->faceGroups.at(_meshId);

		bool bMin = false;
		bool bMax = false;
		for (int i = 0; i < fg.faceCnt; ++i)		
			for (int j = 0; j < 3; ++j)
			{
				unsigned ind = _mesh->GetData()->fb.GetIndex(fg.sFace + i, j);
				D3DXVECTOR3 vec =  *(_mesh->GetData()->vb[ind].Pos3());

				if (!bMin)
				{
					min = vec;
					bMin = true;
				}
				else
					D3DXVec3Minimize(&min, &min, &vec);

				if (!bMax)
				{
					max = vec;
					bMax = true;
				}
				else
					D3DXVec3Maximize(&max, &max, &vec);
			}
	}

	return AABB(min, max);
}

graph::MeshX* MeshXNode::GetMesh()
{
	return _mesh;
}

void MeshXNode::SetMesh(graph::MeshX* value)
{
	if (ReplaceRef(_mesh, value))
	{
		_mesh = value;
		StructureChanged();
	}
}

int MeshXNode::GetMeshId() const
{
	return _meshId;
}

void MeshXNode::SetMeshId(int value)
{
	if (_meshId != value)
	{
		_meshId = value;
		StructureChanged();
	}
}

unsigned MeshXNode::GetSubsetCnt() const
{
	LSL_ASSERT(_mesh);

	return _mesh->GetSubsetCount();
}

MaterialNode* MeshXNode::GetMaterial()
{
	return &material;
}




PlaneNode::PlaneNode(): _size(1.0f, 1.0f)
{
	UpdateMesh();
}

PlaneNode::~PlaneNode()
{	
}

void PlaneNode::UpdateMesh()
{
	_mesh.Free();
	FillDataPlane(*_mesh.GetOrCreateData(), _size.x, _size.y, _size.x, _size.y);
}

void PlaneNode::DoRender(Engine& engine)
{
	_mesh.Init(engine);
	material.Apply(engine);

	do
	{
		engine.BeginDraw();
		_mesh.Draw();
	}
	while (!engine.EndDraw(true));

	material.UnApply(engine);
}

AABB PlaneNode::LocalDimensions() const
{
	return AABB(_mesh.GetMinPos(), _mesh.GetMaxPos());
}

void PlaneNode::Save(lsl::SWriter* writer)
{
	_MyBase::Save(writer);
	
	lsl::SWriteValue(writer, "size", _size);
	
	material.Save(writer, this);
}

void PlaneNode::Load(lsl::SReader* reader)
{
	_MyBase::Load(reader);

	D3DXVECTOR2 size;
	lsl::SReadValue(reader, "size", size);
	SetSize(size);

	material.Load(reader, this);
}

void PlaneNode::OnFixUp(const FixUpNames& fixUpNames)
{
	_MyBase::OnFixUp(fixUpNames);

	material.OnFixUp(fixUpNames, this);
}

const D3DXVECTOR2& PlaneNode::GetSize() const
{
	return _size;
}

void PlaneNode::SetSize(const D3DXVECTOR2& value)
{
	if (_size != value)
	{
		_size = value;
		UpdateMesh();
	}
}




Box::Box()
{
}

void Box::RenderBox(Engine& engine)
{
	D3DXVECTOR3 vertBuf[72] = 
	{
		//
		D3DXVECTOR3(-0.5f, -0.5f, -0.5f), -ZVector,
		D3DXVECTOR3(0.5f, 0.5f, -0.5f),   -ZVector,
		D3DXVECTOR3(0.5f, -0.5f, -0.5f),  -ZVector,		
		D3DXVECTOR3(-0.5f, -0.5f, -0.5f), -ZVector,
		D3DXVECTOR3(-0.5f, 0.5f, -0.5f),  -ZVector,
		D3DXVECTOR3(0.5f, 0.5f, -0.5f),   -ZVector,

		//
		D3DXVECTOR3(-0.5f, -0.5f, 0.5f), ZVector,
		D3DXVECTOR3(0.5f, -0.5f, 0.5f),  ZVector,
		D3DXVECTOR3(0.5f, 0.5f, 0.5f),   ZVector,		
		D3DXVECTOR3(-0.5f, -0.5f, 0.5f), ZVector,
		D3DXVECTOR3(0.5f, 0.5f, 0.5f),   ZVector,
		D3DXVECTOR3(-0.5f, 0.5f, 0.5f),  ZVector,

		//
		D3DXVECTOR3(-0.5f, -0.5f, -0.5f), -YVector,
		D3DXVECTOR3(0.5f, -0.5f, -0.5f),  -YVector,
		D3DXVECTOR3(0.5f, -0.5f, 0.5f),   -YVector,		
		D3DXVECTOR3(-0.5f, -0.5f, -0.5f), -YVector,
		D3DXVECTOR3(0.5f, -0.5f, 0.5f),   -YVector,
		D3DXVECTOR3(-0.5f, -0.5f, 0.5f),  -YVector,

		//
		D3DXVECTOR3(0.5f, 0.5f, -0.5f),  YVector,
		D3DXVECTOR3(-0.5f, 0.5f, -0.5f), YVector,
		D3DXVECTOR3(-0.5f, 0.5f, 0.5f),  YVector,		
		D3DXVECTOR3(0.5f, 0.5f, -0.5f),  YVector,
		D3DXVECTOR3(-0.5f, 0.5f, 0.5f),  YVector,
		D3DXVECTOR3(0.5f, 0.5f, 0.5f),  YVector,

		D3DXVECTOR3(-0.5f, 0.5f, -0.5f),  -XVector,
		D3DXVECTOR3(-0.5f, -0.5f, -0.5f), -XVector,
		D3DXVECTOR3(-0.5f, -0.5f, 0.5f),  -XVector,		
		D3DXVECTOR3(-0.5f, 0.5f, -0.5f),  -XVector,
		D3DXVECTOR3(-0.5f, -0.5f, 0.5f),  -XVector,
		D3DXVECTOR3(-0.5f, 0.5f, 0.5f),   -XVector,

		//
		D3DXVECTOR3(0.5f, -0.5f, -0.5f), XVector,
		D3DXVECTOR3(0.5f, 0.5f, -0.5f),  XVector,
		D3DXVECTOR3(0.5f, 0.5f, 0.5f),   XVector,		
		D3DXVECTOR3(0.5f, -0.5f, -0.5f), XVector,
		D3DXVECTOR3(0.5f, 0.5f, 0.5f),   XVector,
		D3DXVECTOR3(0.5f, -0.5f, 0.5f),  XVector
	};

	engine.GetDriver().GetDevice()->SetFVF(D3DFVF_XYZ | D3DFVF_NORMAL);
	engine.GetDriver().GetDevice()->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 12, vertBuf, 2 * sizeof(D3DXVECTOR3));
}

void Box::DoRender(Engine& engine)
{
	material.Apply(engine);

	do
	{
		engine.BeginDraw();
		RenderBox(engine);
	}
	while (!engine.EndDraw(true));

	material.UnApply(engine);
}

AABB Box::LocalDimensions() const
{
	return AABB(1.0f);
}

void Box::Save(lsl::SWriter* writer)
{
	_MyBase::Save(writer);
	
	material.Save(writer, this);
}

void Box::OnFixUp(const FixUpNames& fixUpNames)
{
	_MyBase::OnFixUp(fixUpNames);

	material.OnFixUp(fixUpNames, this);
}

void Box::Load(lsl::SReader* reader)
{
	_MyBase::Load(reader);

	material.Load(reader, this);
}




Cylinder::Cylinder(): _color(clrWhite)
{
	_mesh = new graph::IndexedVBMesh();
	UpdateMesh();
}

Cylinder::~Cylinder()
{
	delete _mesh;
}

void Cylinder::UpdateMesh()
{
	FillDataCylinder(*_mesh->GetOrCreateData(), 0.25f, 0, 0.5f, 6, _color);
	_mesh->Free();
}

void Cylinder::DoRender(Engine& engine)
{
	_mesh->Init(engine);

	engine.GetContext().SetRenderState(graph::rsLighting, false);
	_mesh->Draw();
	engine.GetContext().RestoreRenderState(graph::rsLighting);
}

AABB Cylinder::LocalDimensions() const
{
	return AABB(_mesh->GetMinPos(), _mesh->GetMaxPos());
}

const D3DXCOLOR& Cylinder::GetColor() const
{
	return _color;
}

void Cylinder::SetColor(const D3DXCOLOR& value)
{
	_color = value;
	UpdateMesh();
}




Sprite::Sprite(): sizes(IdentityVec2), fixDirection(false)
{	
}

void Sprite::DoRender(Engine& engine)
{
	//Отрисовка
	material.Apply(engine);

	engine.RenderSpritePT(GetWorldPos(), D3DXVECTOR3(sizes.x, sizes.y, 1.0f), GetTurnAngle(), fixDirection ? &GetWorldDir() : 0, GetWorldScale());

	material.UnApply(engine);
}
	
AABB Sprite::LocalDimensions() const
{
	if (fixDirection)
	{
		D3DXVECTOR3 dir = GetDir();
		D3DXVECTOR3 up = GetUp();
		D3DXVECTOR3 max = dir * sizes.x/2.0f + up * sizes.y/2.0f;

		return AABB(-max, max);
	}
	else	
		return AABB(1.0f * D3DXVec2Length(&sizes));
}

void Sprite::Save(lsl::SWriter* writer)
{
	_MyBase::Save(writer);
	
	writer->WriteValue("sizes", sizes, 2);
	writer->WriteValue("fixDirection", fixDirection);
		
	material.Save(writer, this);
}

void Sprite::Load(lsl::SReader* reader)
{
	_MyBase::Load(reader);

	reader->ReadValue("sizes", sizes, 2);
	reader->ReadValue("fixDirection", fixDirection);
	
	material.Load(reader, this);
}

void Sprite::OnFixUp(const FixUpNames& fixUpNames)
{
	material.OnFixUp(fixUpNames, this);
}




ScreenSprite::ScreenSprite(): quadVertex(0, 0, 1, 1), uvVertex(0, 0, 1, 1)
{
}

void ScreenSprite::DoRender(Engine& engine)
{
	material.Apply(engine);

	DrawScreenQuad(engine, quadVertex, uvVertex.x, uvVertex.y, uvVertex.z, uvVertex.w, true);

	material.UnApply(engine);
}

void ScreenSprite::Save(lsl::SWriter* writer)
{
	BaseSceneNode::Save(writer);

	SWriteValue(writer, "quadVertex", quadVertex);
	SWriteValue(writer, "uvVertex", uvVertex);

	material.Save(writer, this);
}

void ScreenSprite::Load(lsl::SReader* reader)
{
	BaseSceneNode::Load(reader);

	SReadValue(reader, "quadVertex", quadVertex);
	SReadValue(reader, "uvVertex", uvVertex);

	material.Load(reader, this);
}

void ScreenSprite::OnFixUp(const FixUpNames& fixUpNames)
{
	BaseSceneNode::OnFixUp(fixUpNames);

	material.OnFixUp(fixUpNames, this);
}




MovCoordSys::MovCoordSys(): _curMove(dmNone)
{
	for (int i = 0; i < 3; ++i)
	{
		_arrows[i] = new Cylinder();
		_arrows[i]->SetParent(this);
		_arrows[i]->SetColor(arCol[i]);
		_arrows[i]->SetUp(arUp[i]);
		_arrows[i]->SetPos(arPos[i]);
	}
}

MovCoordSys::~MovCoordSys()
{
	for (int i = 0; i < 3; ++i)	
		delete _arrows[i];
}

MovCoordSys::DirMove MovCoordSys::CompDirMove(const D3DXVECTOR3& rayStart, const D3DXVECTOR3& rayVec)
{
	const D3DXPLANE planes[3] = {XPlane, YPlane, ZPlane};
	const D3DXVECTOR3 coordOff[3] = {XVector/2, YVector/2, ZVector/2};
	const DirMove moves[3] = {dmYZ, dmXZ, dmXY};

	D3DXVECTOR3 localRS;
	D3DXVECTOR3 localRV;
	WorldToLocalCoord(rayStart, localRS);
	WorldToLocalNorm(rayVec, localRV);

	//Ищем близлежащую плоскость выделения
	DirMove move = dmNone;
	float minT = 0;
	bool minTInit = false;
	for (int i = 0; i < 3; ++i)
	{
		float t;
		if (RayCastIntersectPlane(localRS, localRV, planes[i], t))
		{
			D3DXVECTOR3 pnt = localRS + localRV * t + coordOff[i];
			if (pnt > NullVector && pnt < IdentityVector && (!minTInit || minT > t))
			{
				minTInit = true;
				move = moves[i];
			}
		}
	}

	if (move == dmNone)
	{
		//Пересечениям с осями
		const DirMove axeMoves[3] = {dmX, dmY, dmZ};
		for (int i = 0; i < 3; ++i)
		{
			AABB aabb(1.0f);
			aabb.Include(NullVector);
			aabb.Include(arUp[i] * arSize);
			float tNear, tFar;
			if (aabb.RayCastIntersect(localRS, localRV, tNear, tFar))
				return _curMove = axeMoves[i];
		}
	}

	_curMove = move;
	return move;
}

void MovCoordSys::DoRender(Engine& engine)
{
	typedef D3DXVECTOR3 AxePlane[4];

	const DirMove planeMoves[3] = {dmXY, dmXZ, dmYZ};
	
	//Вершины плоскости
	const AxePlane cXYPlaneV = {NullVector, XVector, XVector + YVector, YVector};
	const AxePlane cXZPlaneV = {NullVector, XVector, XVector + ZVector, ZVector};
	const AxePlane cYZPlaneV = {NullVector, YVector, YVector + ZVector, ZVector};
	//Плоскость и цвета осей образующих её
	struct
	{
		const AxePlane* plane;
		D3DXCOLOR col1;
		D3DXCOLOR col2;
	} cPlanes[3] = {{&cXYPlaneV, clrRed, clrGreen}, {&cXZPlaneV, clrRed, clrBlue}, {&cYZPlaneV, clrGreen, clrBlue}};
	
	//Выеделенные оси
	const bool isAxe[3] = {_curMove == dmXY || _curMove == dmXZ || _curMove == dmX, _curMove == dmXY || _curMove == dmYZ || _curMove == dmY, _curMove == dmXZ || _curMove == dmYZ || _curMove == dmZ};




	//Скалим перед рендером чтобы не было дерганий
	float dist = D3DXVec3Length(&(engine.GetContext().GetCamera().GetDesc().pos - GetWorldPos()));
	float scaleF = dist / 15.0f;
	SetScale(scaleF);

	res::VertexPD lines[18];
	const AxePlane* planeVert = 0;
	for (int i = 0; i < 3; ++i)
	{
		//Вычисляем линии осей
		lines[2 * i] = res::VertexPD(NullVector, isAxe[i] ? colSel : arCol[i]);
		lines[2 * i + 1] = res::VertexPD(arPos[i], isAxe[i] ? colSel : arCol[i]);

		//Выеделенная плоскость
		bool isPlane = _curMove == planeMoves[i];

		//Цвета осей образующих плоскость
		D3DXCOLOR col1 = isPlane ? colSel : cPlanes[i].col1;
		D3DXCOLOR col2 = isPlane ? colSel : cPlanes[i].col2;

		//Вычисляем линии плоскостей
		lines[4 * i + 6 + 0] = res::VertexPD((*cPlanes[i].plane)[1], col1);
		lines[4 * i + 6 + 1] = res::VertexPD((*cPlanes[i].plane)[2], col1);
		lines[4 * i + 6 + 2] = res::VertexPD((*cPlanes[i].plane)[3], col2);
		lines[4 * i + 6 + 3] = res::VertexPD((*cPlanes[i].plane)[2], col2);

		//Запоминаем вершины выделенной плоскости
		if (isPlane)
			planeVert = cPlanes[i].plane;
	}




	engine.GetContext().SetRenderState(graph::rsZWriteEnable, false);
	engine.GetContext().SetRenderState(graph::rsZEnable, false);
	engine.GetContext().SetRenderState(graph::rsLighting, false);
	
	//Рисуем линии
	engine.GetDriver().GetDevice()->SetFVF(res::VertexPD::fvf);
	engine.GetDriver().GetDevice()->DrawPrimitiveUP(D3DPT_LINELIST, 9, lines, sizeof(res::VertexPD));

	//Рисуем выеделенную плоскость
	if (planeVert)
	{
		res::VertexPD plane[4] = {
			res::VertexPD((*planeVert)[0], colSel),
			res::VertexPD((*planeVert)[1], colSel),
			res::VertexPD((*planeVert)[3], colSel),
			res::VertexPD((*planeVert)[2], colSel)
		};
		engine.GetContext().SetRenderState(graph::rsCullMode, D3DCULL_NONE);
		engine.GetDriver().GetDevice()->SetFVF(res::VertexPD::fvf);
		engine.GetDriver().GetDevice()->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, plane, sizeof(res::VertexPD));
		engine.GetContext().RestoreRenderState(graph::rsCullMode);
	}

	//Рисуем конусы осей
	for (int i = 0; i < 3; ++i)	
		_arrows[i]->Render(engine);

	engine.GetContext().RestoreRenderState(graph::rsLighting);
	engine.GetContext().RestoreRenderState(graph::rsZWriteEnable);
	engine.GetContext().RestoreRenderState(graph::rsZEnable);
}

AABB MovCoordSys::LocalDimensions() const
{
	return GetAABBOfChildren();
}

MovCoordSys::DirMove MovCoordSys::OnMouseMove(const D3DXVECTOR3& rayStart, const D3DXVECTOR3& rayVec)
{
	return CompDirMove(rayStart, rayVec);
}

MovCoordSys::DirMove MovCoordSys::OnMouseClick(const D3DXVECTOR3& rayStart, const D3DXVECTOR3& rayVec, lsl::KeyState state)
{
	return CompDirMove(rayStart, rayVec);
}




ScaleCoordSys::ScaleCoordSys(): _curMove(dmNone)
{
	for (int i = 0; i < 3; ++i)
	{
		_arrows[i] = new Sprite();
		_arrows[i]->SetParent(this);
		_arrows[i]->SetScale(0.25f);

		LibMaterial* libMat = _arrows[i]->material.GetOrCreate();
		libMat->material.SetDiffuse(arCol[i]);
		libMat->material.SetOption(graph::Material::moLighting, false);
	}
}

ScaleCoordSys::~ScaleCoordSys()
{
	for (int i = 0; i < 3; ++i)
		delete _arrows[i];
}

void ScaleCoordSys::CompBBPlanes(const D3DXVECTOR3& camPos, D3DXVECTOR3* bbPlanes)
{
	for (int i = 0; i < 3; ++i)	
		bbPlanes[i] = camPos[i] > 0 ? arUp[i] : -arUp[i];
}

ScaleCoordSys::DirMove ScaleCoordSys::CompDirMove(const D3DXVECTOR3& rayStart, const D3DXVECTOR3& rayVec, const D3DXVECTOR3& camPos)
{
	D3DXVECTOR3 localRS;
	D3DXVECTOR3 localRV;
	WorldToLocalCoord(rayStart, localRS);
	WorldToLocalNorm(rayVec, localRV);

	D3DXVECTOR3 bbPlanes[3];
	CompBBPlanes(camPos, bbPlanes);

	//Пересечение с размерной плоскостью
	D3DXVECTOR3 plLine[3];
	for (int i = 0; i < 3; ++i)
		plLine[i] = bbPlanes[i] * plSize;
	D3DXPLANE upPlane;
	D3DXPlaneFromPoints(&upPlane, &plLine[0], &plLine[1], &plLine[2]);
	//
	D3DXVECTOR3 maxVec = NullVector;
	for (int i = 0; i < 3; ++i)
		maxVec += plLine[i];
	D3DXVECTOR3 pnt;
	RayCastIntersectPlane(localRS, localRV, upPlane, pnt);
	AABB aabb;
	aabb.FromPoints(NullVector, maxVec);
	if (aabb.ContainsPoint(pnt))
		return _curMove = dmXYZ;

	//Пересечениям с осями
	const DirMove axeMoves[3] = {dmX, dmY, dmZ};
	for (int i = 0; i < 3; ++i)
	{
		AABB aabb(1.0f);
		aabb.Include(NullVector);
		aabb.Include(bbPlanes[i] * arSize);
		float tNear, tFar;
		if (aabb.RayCastIntersect(localRS, localRV, tNear, tFar))
			return _curMove = axeMoves[i];
	}

	return _curMove = dmNone;
}

void ScaleCoordSys::DoRender(Engine& engine)
{
	const float stepSzAxe = 2.2f;

	D3DXVECTOR3 bbPlanes[3];
	CompBBPlanes(engine.GetContext().GetCamera().GetDesc().pos - GetWorldPos(), bbPlanes);
	
	D3DXVECTOR3 plLine[3];
	for (int i = 0; i < 3; ++i)
		plLine[i] = bbPlanes[i] * plSize;
	const DirMove axeMoves[3] = {dmX, dmY, dmZ};
	D3DXCOLOR plCol[3];
	for (int i = 0; i < 3; ++i)
		plCol[i] = (_curMove == axeMoves[i]) ? colSel : arCol[i];

	const res::VertexPD lines[18] = 
	{
		//Линии осей
		res::VertexPD(NullVector, plCol[0]), res::VertexPD(bbPlanes[0] * arSize, plCol[0]),
		res::VertexPD(NullVector, plCol[1]), res::VertexPD(bbPlanes[1] * arSize, plCol[1]),
		res::VertexPD(NullVector, plCol[2]), res::VertexPD(bbPlanes[2] * arSize, plCol[2]),
		//Линии плоскости
		res::VertexPD(plLine[0], arCol[0]), res::VertexPD(0.5f * (plLine[0] + plLine[2]), arCol[0]),
		res::VertexPD(plLine[0], arCol[0]), res::VertexPD(0.5f * (plLine[0] + plLine[1]), arCol[0]),
		res::VertexPD(plLine[1], arCol[1]), res::VertexPD(0.5f * (plLine[0] + plLine[1]), arCol[1]),
		res::VertexPD(plLine[1], arCol[1]), res::VertexPD(0.5f * (plLine[2] + plLine[1]), arCol[1]),
		res::VertexPD(plLine[2], arCol[2]), res::VertexPD(0.5f * (plLine[2] + plLine[1]), arCol[2]),
		res::VertexPD(plLine[2], arCol[2]), res::VertexPD(0.5f * (plLine[2] + plLine[0]), arCol[2])
	};
	//Вершины размерной плоскости
	const res::VertexPD plVertex[3] = 
	{
		res::VertexPD(plLine[0], colSel),
		res::VertexPD(plLine[1], colSel),
		res::VertexPD(plLine[2], colSel)
	};
	//Позиции спрайтов
	for (int i = 0; i < 3; ++i)	
		_arrows[i]->SetPos(bbPlanes[i] * arSize);

	engine.GetContext().SetRenderState(graph::rsZWriteEnable, false);
	engine.GetContext().SetRenderState(graph::rsZEnable, false);
	engine.GetContext().SetRenderState(graph::rsLighting, false);

	//Рисуем линии
	engine.GetDriver().GetDevice()->SetFVF(res::VertexPD::fvf);
	engine.GetDriver().GetDevice()->DrawPrimitiveUP(D3DPT_LINELIST, 9, lines, sizeof(res::VertexPD));

	//Рисуем выеделенную плоскость
	if (_curMove == dmXYZ)
	{
		engine.GetContext().SetRenderState(graph::rsCullMode, D3DCULL_NONE);
		engine.GetDriver().GetDevice()->SetFVF(res::VertexPD::fvf);
		engine.GetDriver().GetDevice()->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, plVertex, sizeof(res::VertexPD));
		engine.GetContext().RestoreRenderState(graph::rsCullMode);
	}

	engine.GetContext().RestoreRenderState(graph::rsLighting);

	//Рисуем конусы осей
	for (int i = 0; i < 3; ++i)	
		_arrows[i]->Render(engine);

	engine.GetContext().RestoreRenderState(graph::rsZWriteEnable);
	engine.GetContext().RestoreRenderState(graph::rsZEnable);

	float dist = D3DXVec3Length(&(engine.GetContext().GetCamera().GetDesc().pos - GetWorldPos()));
	float scaleF = dist / 15.0f;
	SetScale(scaleF);
}

AABB ScaleCoordSys::LocalDimensions() const
{
	return GetAABBOfChildren();
}

ScaleCoordSys::DirMove ScaleCoordSys::OnMouseMove(const D3DXVECTOR3& rayStart, const D3DXVECTOR3& rayVec, const D3DXVECTOR3& camPos)
{
	return CompDirMove(rayStart, rayVec, camPos - GetWorldPos());
}

ScaleCoordSys::DirMove ScaleCoordSys::OnMouseClick(const D3DXVECTOR3& rayStart, const D3DXVECTOR3& rayVec, lsl::KeyState state, const D3DXVECTOR3& camPos)
{
	return CompDirMove(rayStart, rayVec, camPos - GetWorldPos());
}




void FillDataPlane(res::VertexData& vb, float width, float height, float u, float v)
{
	vb.SetVertexCount(6);
	vb.SetFormat(res::VertexData::vtPos3);
	vb.SetFormat(res::VertexData::vtNormal);
	vb.SetFormat(res::VertexData::vtTex0);
	vb.Init();

	float x = width / 2.0f;
	float y = height / 2.0f;

	res::VertexPNT* vertex = reinterpret_cast<res::VertexPNT*>(vb.GetData());
	vertex[0] = res::VertexPNT(D3DXVECTOR3(-x, -y, 0.0f), ZVector, D3DXVECTOR2(0.0f,  0.0f));
	vertex[1] = res::VertexPNT(D3DXVECTOR3( x, -y, 0.0f), ZVector, D3DXVECTOR2(u,     0.0f));
	vertex[2] = res::VertexPNT(D3DXVECTOR3( x,  y, 0.0f), ZVector, D3DXVECTOR2(u,     v));
	vertex[3] = res::VertexPNT(D3DXVECTOR3(-x, -y, 0.0f), ZVector, D3DXVECTOR2(0.0f,  0.0f));
	vertex[4] = res::VertexPNT(D3DXVECTOR3( x,  y, 0.0f), ZVector, D3DXVECTOR2(u,     v));
	vertex[5] = res::VertexPNT(D3DXVECTOR3(-x,  y, 0.0f), ZVector, D3DXVECTOR2(0.0f,  v));
	vb.Update();
}

void FillDataCylinder(res::MeshData& mesh, float botRadius, float topRadius, float height, unsigned slices, const D3DXCOLOR& color)
{
	bool isBot = botRadius != 0;
	bool isTop = topRadius != 0;

	LSL_ASSERT(isBot || isTop && slices > 2 && height > 0);

	float aStep = 2 * D3DX_PI / slices;
	unsigned botVCnt = isBot * slices + 1;
	unsigned topVCnt = isTop * slices + 1;
	unsigned botFCnt = isBot * slices;
	unsigned topFCnt = isTop * slices;
	
	unsigned sz = sizeof(res::VertexPD);
	mesh.vb.SetFormat(res::VertexData::vtPos3);
	mesh.vb.SetFormat(res::VertexData::vtColor);
	
	mesh.vb.SetVertexCount(botVCnt + topVCnt);
	mesh.vb.Init();
	res::VertexPD* vertex = reinterpret_cast<res::VertexPD*>(mesh.vb.GetData());

	mesh.fb.SetIndexFormat(D3DFMT_INDEX16);
	mesh.fb.SetFaceCount(slices * (1 + (isTop && isBot)) + botFCnt + topFCnt);
	mesh.fb.Init();	
	res::TriFace16* indices = reinterpret_cast<res::TriFace16*>(mesh.fb.GetData());

	//Нижняя грань
	unsigned centBotInd = botVCnt - 1;
	if (isBot)
		for (unsigned i = 0; i < slices; ++i)
		{
			vertex[i].pos.x = cos(aStep * i) * botRadius;
			vertex[i].pos.y = sin(aStep * i) * botRadius;
			vertex[i].pos.z = 0;

			//vertex[i].norm = -ZVector;
			vertex[i].diffuse = color;

			indices[i].v2 = centBotInd;
			indices[i].v1 = i;
			//Одна вершина в круге при обходе повторяется
			indices[i].v3 = (i < slices - 1) ? i + 1 : 0;
		}
	//Центр круга
	vertex[centBotInd].pos = NullVector;
	vertex[centBotInd].diffuse = color;

	//Верхняя грань
	unsigned centTopInd = botVCnt + topVCnt - 1;
	if (isTop)	
		for (unsigned i = 0; i < slices; ++i)
		{
			vertex[botVCnt + i].pos.x = cos(aStep * i) * topRadius;
			vertex[botVCnt + i].pos.y = sin(aStep * i) * topRadius;
			vertex[botVCnt + i].pos.z = height;

			//vertex[botVCnt + i].norm = ZVector;
			vertex[botVCnt + i].diffuse = color;

			indices[botFCnt + i].v1 = centTopInd;
			indices[botFCnt + i].v2 = botVCnt + i;
			//Одна вершина в круге при обходе повторяется
			indices[botFCnt + i].v3 = (i < slices - 1) ? botVCnt + i + 1 : botVCnt;
		}
	//Центр круга
	vertex[centTopInd].pos = ZVector * height;
	vertex[centTopInd].diffuse = color;

	//Боковая грань
	unsigned curF = botFCnt + topFCnt;
	for (unsigned i = 0; i < slices; ++i)
	{
		unsigned v1 = isBot ? i : 0;
		unsigned v4 = isTop ? ((i < slices - 1) ? botVCnt + i + 1 : botVCnt) : botVCnt;

		if (isBot)
		{
			unsigned v2 = (i < slices - 1) ? i + 1 : 0;

			indices[curF].v1 = v1;
			indices[curF].v2 = v2;
			indices[curF].v3 = v4;
			++curF;
		}
		if (isTop)
		{
			unsigned v3 = botVCnt + i;

			indices[curF].v1 = v1;
			indices[curF].v2 = v4;
			indices[curF].v3 = v3;
			++curF;
		}
	}
	mesh.vb.Update();
	mesh.fb.Update();

	mesh.Init();	
}


}

}