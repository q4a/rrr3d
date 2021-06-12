#include "stdafx.h"

#include "graph\\VideoResource.h"

#include "graph\\Engine.h"

namespace r3d
{

namespace graph
{

VideoResource::VideoResource(): _engine(0), _autoInit(false)
{
}

VideoResource::~VideoResource()
{
	Free();
}

void VideoResource::SetEngine(Engine* value)
{
	if (_engine != value)
	{
		if (_engine)
			_engine->RemoveVideoRes(this);
		_engine = value;
		if (_engine)
			_engine->InsertVideoRes(this);
	}
}

void VideoResource::Init(Engine& engine)
{
	if (!IsInit())
	{
		SetEngine(&engine);

		if (_engine->IsReset())
			_MyBase::Init();
		else
			_autoInit = true;
	}
}

void VideoResource::Free()
{
	_MyBase::Free();

	SetEngine(0);
}

void VideoResource::Reload(Engine& engine)
{
	Free();
	Init(engine);
}

void VideoResource::Reload()
{
	LSL_ASSERT(_engine);

	Engine* engine = _engine;

	Free();
	Init(*engine);
}

void VideoResource::OnLostDevice()
{
	if (IsInit())
	{
		_MyBase::Free();
		_autoInit = true;
	}
	
}

void VideoResource::OnResetDevice()
{
	if (_autoInit)
	{
		_MyBase::Init();
		_autoInit = false;
	}
}

Engine* VideoResource::GetEngine()
{
	return _engine;
}




MemPoolResource::MemPoolResource(DWORD usage): _memoryPool(D3DPOOL_DEFAULT), _usage(usage)
{
}

void MemPoolResource::OnLostDevice()
{
	if (_memoryPool == D3DPOOL_DEFAULT)
		VideoResource::OnLostDevice();
}

void MemPoolResource::OnResetDevice()
{
	if (_memoryPool == D3DPOOL_DEFAULT)
		VideoResource::OnResetDevice();
}

D3DPOOL MemPoolResource::GetMemoryPool() const
{
	return _memoryPool;
}

void MemPoolResource::SetMemoryPool(D3DPOOL value)
{
	if (_memoryPool != value)
	{
		_memoryPool = value;
		Free();
	}
}

DWORD MemPoolResource::GetUsage() const
{
	return _usage;
}

void MemPoolResource::SetUsage(DWORD value)
{
	if (_usage != value)
	{
		_usage = value;
		Free();
	}
}




VBMesh::VBMesh(): MemPoolResource(D3DUSAGE_WRITEONLY), _data(0), _createData(false), _vb(0), primitiveType(D3DPT_TRIANGLELIST), _beginStream(0)
{
}

VBMesh::~VBMesh()
{
	Free();
	SetData(0);
}

void VBMesh::LoadData() const
{
	if (!_data->IsInit() && !GetDynamic())
		_data->Load();
}

void VBMesh::DoInit()
{
	LoadData();

	GetEngine()->GetDriver().GetDevice()->CreateVertexBuffer(_data->GetSize(), GetUsage(), _data->GetFVF(), GetMemoryPool(), &_vb, 0);	
}

void VBMesh::DoFree()
{
	_vb->Release();
}

void VBMesh::DoUpdate()
{
	void* data;
	_vb->Lock(0, 0, (void**)&data, 0);
	_data->CopyDataTo(data);
	_vb->Unlock();
}

void VBMesh::BeginStream(unsigned offsetInBytes, unsigned stride, unsigned streamNumber)
{
	LSL_ASSERT(IsInit());

	if (_beginStream == 0)
		GetEngine()->GetDriver().GetDevice()->SetStreamSource(streamNumber, _vb, offsetInBytes, stride);

	++_beginStream;
}

void VBMesh::BeginStream()
{
	BeginStream(0, _data->GetVertexSize());
}

void VBMesh::EndStream()
{
	LSL_ASSERT(_beginStream > 0);

	--_beginStream;
}

void VBMesh::Draw(unsigned startVertex,unsigned primitiveCount)
{
	BeginStream();
	GetEngine()->GetDriver().GetDevice()->SetFVF(_data->GetFVF());
	GetEngine()->GetDriver().GetDevice()->DrawPrimitive(primitiveType, startVertex, primitiveCount);
	EndStream();
}

void VBMesh::Draw()
{
	Draw(0, res::GetPrimitivesCount(primitiveType, _data->GetVertexCount()));
}

res::VertexData* VBMesh::GetData()
{
	return _data;
}

res::VertexData* VBMesh::GetOrCreateData()
{
	if (!_data)
	{
		_data = new res::VertexData();
		_data->AddRef();
		_createData = true;
	}

	return _data;
}

void VBMesh::SetData(res::VertexData* value)
{
	if (ReplaceRef(_data, value))
	{
		if (_createData)
		{
			delete _data;
			_createData = false;
		}
		_data = value;
	}
}

const D3DXVECTOR3& VBMesh::GetMinPos() const
{
	LoadData();

	return _data->GetMinPos();
}

const D3DXVECTOR3& VBMesh::GetMaxPos() const
{
	LoadData();

	return _data->GetMaxPos();
}

IDirect3DVertexBuffer9* VBMesh::GetVB() const
{
	return _vb;
}




IndexedVBMesh::IndexedVBMesh(): MemPoolResource(D3DUSAGE_WRITEONLY), _data(0), _createData(false), _beginStreamCnt(0), _vb(0), _ib(0)
{	
}

IndexedVBMesh::~IndexedVBMesh()
{	
	LSL_ASSERT(_beginStreamCnt == 0);

	Free();
	SetData(0);
}

void IndexedVBMesh::LoadData() const
{
	if (!_data->IsInit() && !GetDynamic())
		_data->Load();
}

void IndexedVBMesh::DoInit()
{
	LoadData();	

	GetEngine()->GetDriver().GetDevice()->CreateVertexBuffer(_data->vb.GetSize(), GetUsage(), _data->vb.GetFVF(), GetMemoryPool(), &_vb, 0);
	GetEngine()->GetDriver().GetDevice()->CreateIndexBuffer(_data->fb.GetSize(), GetUsage(), _data->fb.GetIndexFormat(), GetMemoryPool(), &_ib, 0);
}

void IndexedVBMesh::DoFree()
{
	_ib->Release();
	_vb->Release();
}

void IndexedVBMesh::DoUpdate()
{
	void* vbData;
	_vb->Lock(0, 0, (void**)&vbData, 0);
	_data->vb.CopyDataTo(vbData);
	_vb->Unlock();

	void* ibData;
	_ib->Lock(0, 0, (void**)&ibData, 0);
	_data->fb.CopyDataTo(ibData);	
	_ib->Unlock();
}

void IndexedVBMesh::BeginStream(int attribId, unsigned streamNumber)
{
	LSL_ASSERT(IsInit());

	if (_beginStreamCnt++ == 0)
	{
		const res::FaceGroup& fg = _data->faceGroups.at(attribId);
		
		GetEngine()->GetDriver().GetDevice()->SetStreamSource(streamNumber, _vb, fg.sVertex * _data->vb.GetVertexSize(), _data->vb.GetVertexSize());
		GetEngine()->GetDriver().GetDevice()->SetIndices(_ib);
		GetEngine()->GetDriver().GetDevice()->SetFVF(_data->vb.GetFVF());
	}
}

void IndexedVBMesh::BeginStream()
{	
	LSL_ASSERT(IsInit());

	if (_beginStreamCnt++ == 0)
	{
		GetEngine()->GetDriver().GetDevice()->SetStreamSource(0, _vb, 0, _data->vb.GetVertexSize());
		GetEngine()->GetDriver().GetDevice()->SetIndices(_ib);
		GetEngine()->GetDriver().GetDevice()->SetFVF(_data->vb.GetFVF());
	}
}

void IndexedVBMesh::EndStream()
{
	LSL_ASSERT(_beginStreamCnt > 0);

	--_beginStreamCnt;
}

void IndexedVBMesh::Draw()
{
	BeginStream();
	GetEngine()->GetDriver().GetDevice()->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, _data->vb.GetVertexCount(), 0, _data->fb.GetFaceCount());
	EndStream();
}

void IndexedVBMesh::DrawSubset(int attribId)
{
	const res::FaceGroup& fg = _data->faceGroups.at(attribId);

	BeginStream(attribId);	
	GetEngine()->GetDriver().GetDevice()->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, fg.sVertex, fg.vertexCnt, fg.sFace * 3, fg.faceCnt);
	EndStream();
}

res::MeshData* IndexedVBMesh::GetData()
{
	return _data;
}

res::MeshData* IndexedVBMesh::GetOrCreateData()
{
	if (!_data)
	{
		_data = new res::MeshData();
		_data->AddRef();
		_createData = true;
	}
	return _data;
}

void IndexedVBMesh::SetData(res::MeshData* value)
{
	if (ReplaceRef(_data, value))
	{
		if (_createData)
		{
			delete _data;
			_createData = false;
		}
		_data = value;
	}
}

unsigned IndexedVBMesh::GetSubsetCount() const
{
	LoadData();

	return _data->faceGroups.size();
}

const D3DXVECTOR3& IndexedVBMesh::GetMinPos() const
{
	LoadData();

	return _data->vb.GetMinPos();
}

const D3DXVECTOR3& IndexedVBMesh::GetMaxPos() const
{
	LoadData();

	return _data->vb.GetMaxPos();
}

IDirect3DVertexBuffer9* IndexedVBMesh::GetVB()
{
	return _vb;
}

IDirect3DIndexBuffer9* IndexedVBMesh::GetIB()
{
	return _ib;
}




MeshX::MeshX(): MemPoolResource(D3DUSAGE_WRITEONLY), _data(0), _createData(false), _d3dxMesh(0), _beginStreamCnt(0)
{	
}

MeshX::~MeshX()
{
	LSL_ASSERT(_beginStreamCnt == 0);

	Free();
	SetData(0);
}

void MeshX::LoadData() const
{
	if (!_data->IsInit() && !GetDynamic())
		_data->Load();
}

void MeshX::DoInit()
{
	LoadData();	

	switch (_prefab)
	{
	case mpSphere:
		D3DXCreateSphere(GetEngine()->GetDriver().GetDevice(), _params.radius1, _params.slices, _params.stacks, &_d3dxMesh, NULL);
		break;
	}
}

void MeshX::DoFree()
{
	_d3dxMesh->Release();
	_d3dxMesh = 0;
}

void MeshX::DoUpdate()
{
	//void* vbData;
	//_vb->Lock(0, 0, (void**)&vbData, 0);
	//_data->vb.CopyDataTo(vbData);
	//_vb->Unlock();

	//void* ibData;
	//_ib->Lock(0, 0, (void**)&ibData, 0);
	//_data->fb.CopyDataTo(ibData);	
	//_ib->Unlock();
}

void MeshX::BeginStream(int attribId, unsigned streamNumber)
{
	LSL_ASSERT(IsInit());

	if (_beginStreamCnt++ == 0)
	{
		const res::FaceGroup& fg = _data->faceGroups.at(attribId);

		//GetEngine()->GetDriver().GetDevice()->SetStreamSource(streamNumber, _vb, fg.sVertex * _data->vb.GetVertexSize(), _data->vb.GetVertexSize());
		//GetEngine()->GetDriver().GetDevice()->SetIndices(_ib);
		//GetEngine()->GetDriver().GetDevice()->SetFVF(_data->vb.GetFVF());
	}
}

void MeshX::BeginStream()
{	
	LSL_ASSERT(IsInit());

	if (_beginStreamCnt++ == 0)
	{
		//GetEngine()->GetDriver().GetDevice()->SetStreamSource(0, _vb, 0, _data->vb.GetVertexSize());
		//GetEngine()->GetDriver().GetDevice()->SetIndices(_ib);
		//GetEngine()->GetDriver().GetDevice()->SetFVF(_data->vb.GetFVF());
	}
}

void MeshX::EndStream()
{
	LSL_ASSERT(_beginStreamCnt > 0);

	--_beginStreamCnt;
}

void MeshX::Draw()
{
	BeginStream();
	
	//GetEngine()->GetDriver().GetDevice()->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, _data->vb.GetVertexCount(), 0, _data->fb.GetFaceCount());
	_d3dxMesh->DrawSubset(0);
	
	EndStream();
}

void MeshX::DrawSubset(int attribId)
{
	//const res::FaceGroup& fg = _data->faceGroups.at(attribId);

	BeginStream(attribId);	
	
	//GetEngine()->GetDriver().GetDevice()->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, fg.sVertex, fg.vertexCnt, fg.sFace * 3, fg.faceCnt);
	_d3dxMesh->DrawSubset(attribId);
	
	EndStream();
}

res::MeshData* MeshX::GetData()
{
	return _data;
}

res::MeshData* MeshX::GetOrCreateData()
{
	if (!_data)
	{
		_data = new res::MeshData();
		_data->AddRef();
		_createData = true;
	}
	return _data;
}

void MeshX::SetData(res::MeshData* value)
{
	if (ReplaceRef(_data, value))
	{
		if (_createData)
		{
			delete _data;
			_createData = false;
		}
		_data = value;
	}
}

unsigned MeshX::GetSubsetCount() const
{
	LoadData();

	return _data->faceGroups.size();
}

const D3DXVECTOR3& MeshX::GetMinPos() const
{
	LoadData();

	return _data->vb.GetMinPos();
}

const D3DXVECTOR3& MeshX::GetMaxPos() const
{
	LoadData();

	return _data->vb.GetMaxPos();
}




TexResource::TexResource(DWORD usage): MemPoolResource(usage), _screenScale(0, 0), _levelCnt(1) //один основной уровень
{	
}

IDirect3DBaseTexture9* TexResource::GetTex()
{
	return GetBaseTex();
}

unsigned TexResource::GetLevelCnt() const
{
	return _levelCnt;
}

void TexResource::SetLevelCnt(unsigned value)
{
	if (_levelCnt != value)
	{
		_levelCnt = value;
		Free();
	}
}

const D3DXVECTOR2& TexResource::GetScreenScale() const
{
	return _screenScale;
}

void TexResource::SetScreenScale(D3DXVECTOR2 value)
{
	if (_screenScale != value)
	{
		_screenScale = value;
		Free();
	}
}




Tex2DResource::Tex2DResource(): TexResource(0), _texture(0), _data(0), _createData(false), _d3dxLoadUsed(false), _gui(false)
{
}

Tex2DResource::~Tex2DResource()
{
	Free();
	SetData(0);
}

IDirect3DBaseTexture9* Tex2DResource::GetBaseTex()
{
	return _texture;
}

void Tex2DResource::LoadData()
{
	if (_data->IsInit() || GetDynamic())
	{
		_d3dxLoadUsed = _data->GetSizeExt() != 0 && Engine::d3dxUse();
		return;
	}

	_d3dxLoadUsed = false;

	if (_data->GetFileName() != "" && Engine::d3dxUse())
	{
		D3DXIMAGE_INFO info;		
		HRESULT hr = D3DXGetImageInfoFromFileW(lsl::GetAppFilePath(_data->GetFileName()).c_str(), &info);

		if (hr == S_OK)
		{
			_d3dxLoadUsed = true;

			lsl::BinaryResource tmpBuff;
			tmpBuff.LoadFromFile(_data->GetFileName());

			_data->SetWidth(info.Width);
			_data->SetHeight(info.Height);
			_data->SetFormat(info.Format);

			return;
		}
	}

	_data->SetSize(0);
	_data->Load();
}

void Tex2DResource::DoInit()
{
	LoadData();

	unsigned screenSz[2] = {static_cast<unsigned>(GetScreenScale().x * GetEngine()->GetParams().BackBufferWidth), static_cast<unsigned>(GetScreenScale().y * GetEngine()->GetParams().BackBufferHeight)};

	HRESULT hr = D3D_OK;

	DWORD usage = GetUsage();
	if (((usage & D3DUSAGE_AUTOGENMIPMAP) && _data->IsCompressed()) || _d3dxLoadUsed)
		usage = usage & (~D3DUSAGE_AUTOGENMIPMAP);

	if (_d3dxLoadUsed)
	{
		hr = D3DXCreateTextureFromFileEx(GetEngine()->GetDriver().GetDevice(), _data->GetFileName().c_str(), _gui ? D3DX_DEFAULT_NONPOW2 : D3DX_DEFAULT, _gui ? D3DX_DEFAULT_NONPOW2 : D3DX_DEFAULT, GetLevelCnt(), usage, D3DFMT_UNKNOWN, GetMemoryPool(), D3DX_DEFAULT, D3DX_DEFAULT, 0, NULL, NULL, &_texture);

		//hr = D3DXCreateTextureFromFileInMemoryEx(GetEngine()->GetDriver().GetDevice(), _data->GetData(), _data->GetSize(), D3DX_DEFAULT_NONPOW2, D3DX_DEFAULT_NONPOW2, GetLevelCnt(), usage, D3DFMT_UNKNOWN, GetMemoryPool(), D3DX_DEFAULT, D3DX_DEFAULT, 0, NULL, NULL, &_texture);
	}
	else
	{
		hr = GetEngine()->GetDriver().GetDevice()->CreateTexture(screenSz[0] + _data->GetWidth(), screenSz [1] + _data->GetHeight(), GetLevelCnt(), usage, _data->GetFormat(), GetMemoryPool(), &_texture, 0);
	}

	if (hr != D3D_OK || !_texture)
	{
		if (hr == D3DERR_INVALIDCALL)
			throw lsl::Error("void Tex2DResource::DoInit(). D3DERR_INVALIDCALL");
		if (hr == D3DERR_OUTOFVIDEOMEMORY)
			throw lsl::Error("void Tex2DResource::DoInit(). D3DERR_OUTOFVIDEOMEMORY");
		if (hr == E_OUTOFMEMORY)
			throw lsl::Error("void Tex2DResource::DoInit(). E_OUTOFMEMORY");
		if (hr == D3DERR_DEVICENOTRESET)
			throw lsl::Error("void Tex2DResource::DoInit(). D3DERR_DEVICENOTRESET");
		if (hr == D3DERR_DEVICELOST)
			throw lsl::Error("void Tex2DResource::DoInit(). D3DERR_DEVICELOST");
		if (hr == D3DOK_NOAUTOGEN)
			throw lsl::Error("void Tex2DResource::DoInit(). D3DOK_NOAUTOGEN");
		if (hr == D3DERR_NOTAVAILABLE)
			throw lsl::Error("void Tex2DResource::DoInit(). D3DERR_NOTAVAILABLE");
		if (hr == D3DXERR_INVALIDDATA)
			throw lsl::Error("void Tex2DResource::DoInit(). D3DXERR_INVALIDDATA");
	}
}

void Tex2DResource::DoFree()
{
	_d3dxLoadUsed = false;
	_texture->Release();
	_texture = 0;
}

void Tex2DResource::DoUpdate()
{
	if (_d3dxLoadUsed)
		return;

	bool manualFilter = GetLevelCnt() != 1 && (GetUsage() & D3DUSAGE_AUTOGENMIPMAP) && _data->IsCompressed();

	IDirect3DTexture9* tmpTex;
	bool _notDirAccess = GetMemoryPool() == D3DPOOL_DEFAULT && !(GetUsage() & D3DUSAGE_DYNAMIC);
	if (_notDirAccess)
		GetEngine()->GetDriver().GetDevice()->CreateTexture(_data->GetWidth(), _data->GetHeight(), manualFilter ? GetLevelCnt() : 1, 0, _data->GetFormat(), D3DPOOL_SYSTEMMEM, &tmpTex, 0);	
	else
		tmpTex = _texture;

	D3DLOCKED_RECT source;	
	tmpTex->LockRect(0, &source, 0, D3DLOCK_READONLY);

	switch (_data->GetFormat())
	{
	case D3DFMT_DXT1:
	case D3DFMT_DXT2:
	case D3DFMT_DXT3:
	case D3DFMT_DXT4:
	case D3DFMT_DXT5:
		{
			int rowSize = (_data->GetWidth() + 3) / 4 * _data->GetPixelSize();
			res::CopyPitchData(static_cast<char*>(source.pBits), source.Pitch, _data->GetData(), rowSize, rowSize, (_data->GetHeight() + 3) / 4);
			break;
		}

	default:
		{
			int rowSize = _data->GetWidth() * _data->GetPixelSize();
			res::CopyPitchData(static_cast<char*>(source.pBits), source.Pitch, _data->GetData(), rowSize, rowSize, _data->GetHeight());
			break;
		}
	}

	tmpTex->UnlockRect(0);

	if (manualFilter)
		D3DXFilterTexture(tmpTex, NULL, D3DX_DEFAULT, D3DX_DEFAULT);

	if (_notDirAccess)
	{
		GetEngine()->GetDriver().GetDevice()->UpdateTexture(tmpTex, _texture);
		tmpTex->Release();
	}
}

void Tex2DResource::SyncFrom(Tex2DResource* value)
{
	if (_data->GetWidth() != value->_data->GetWidth() || _data->GetHeight() != value->_data->GetHeight() || _data->GetFormat() != value->_data->GetFormat())
	{
		_data->SetWidth(value->_data->GetWidth());
		_data->SetHeight(value->_data->GetHeight());
		_data->SetFormat(value->_data->GetFormat());
		
		Free();
	}
}

bool Tex2DResource::GetGUI() const
{
	return _gui;
}

void Tex2DResource::SetGUI(bool value)
{
	if (_gui != value)
	{
		_gui = value;
		Free();
	}
}

res::ImageResource* Tex2DResource::GetData()
{
	return _data;
}

const res::ImageResource* Tex2DResource::GetData() const
{
	return _data;
}

res::ImageResource* Tex2DResource::GetOrCreateData()
{
	if (!_data)
	{
		_data = new res::ImageResource();
		_data->AddRef();
		_createData = true;
	}

	return _data;
}

void Tex2DResource::SetData(res::ImageResource* value)
{
	if (ReplaceRef(_data, value))
	{
		if (_createData)
		{
			delete _data;
			_createData = false;
		}

		_data = value;		
	}
}

IDirect3DTexture9* Tex2DResource::GetTex()
{
	LSL_ASSERT(IsInit());

	return _texture;
}

D3DXVECTOR2 Tex2DResource::GetSize()
{
	LSL_ASSERT(_data);

	LoadData();

	return D3DXVECTOR2(static_cast<float>(_data->GetWidth()), static_cast<float>(_data->GetHeight()));
}




TexCubeResource::TexCubeResource(): TexResource(0), _texture(0), _data(0), _createData(false), _d3dxLoadUsed(false)
{	
}

TexCubeResource::~TexCubeResource()
{
	Free();
	SetData(0);
}

IDirect3DBaseTexture9* TexCubeResource::GetBaseTex()
{
	return _texture;
}

void TexCubeResource::LoadData()
{
	if (_data->IsInit() || GetDynamic())
	{
		_d3dxLoadUsed = _data->GetSizeExt() != 0 && Engine::d3dxUse();
		return;
	}

	_d3dxLoadUsed = false;

	if (_data->GetFileName() != "" && Engine::d3dxUse())
	{
		D3DXIMAGE_INFO info;		
		HRESULT hr = D3DXGetImageInfoFromFileW(lsl::GetAppFilePath(_data->GetFileName()).c_str(), &info);

		if (hr == S_OK)
		{
			_d3dxLoadUsed = true;

			lsl::BinaryResource tmpBuff;
			tmpBuff.LoadFromFile(_data->GetFileName());

			_data->SetWidth(info.Width);
			_data->SetHeight(info.Height);
			_data->SetFormat(info.Format);

			return;
		}
	}

	_data->SetSize(0);
	_data->Load();
}

void TexCubeResource::DoInit()
{
	LoadData();

	unsigned screenSz[2] = {static_cast<unsigned>(GetScreenScale().x * GetEngine()->GetParams().BackBufferWidth), static_cast<unsigned>(GetScreenScale().y * GetEngine()->GetParams().BackBufferHeight)};

	HRESULT hr = D3D_OK;

	DWORD usage = GetUsage();
	if (((usage & D3DUSAGE_AUTOGENMIPMAP) && _data->IsCompressed()) || _d3dxLoadUsed)
		usage = usage & (~D3DUSAGE_AUTOGENMIPMAP);

	if (_d3dxLoadUsed)
	{
		hr = D3DXCreateCubeTextureFromFileEx(GetEngine()->GetDriver().GetDevice(), _data->GetFileName().c_str(), D3DX_DEFAULT, GetLevelCnt(), usage, D3DFMT_UNKNOWN, GetMemoryPool(), D3DX_DEFAULT, D3DX_DEFAULT, 0, NULL, NULL, &_texture);
	}
	else
	{
		hr = GetEngine()->GetDriver().GetDevice()->CreateCubeTexture(screenSz[0] + _data->GetWidth()/6, GetLevelCnt(), GetUsage(), _data->GetFormat(), GetMemoryPool(), &_texture, 0);
	}

	if (hr != D3D_OK || !_texture)
	{
		bool b = false;
		b = hr == D3DERR_INVALIDCALL;
		if (b)
			throw lsl::Error("void Tex2DResource::DoInit(). D3DERR_INVALIDCALL");
		b = hr == D3DERR_OUTOFVIDEOMEMORY;
		if (b)
			throw lsl::Error("void Tex2DResource::DoInit(). D3DERR_OUTOFVIDEOMEMORY");
		b = hr == E_OUTOFMEMORY;
		if (b)
			throw lsl::Error("void Tex2DResource::DoInit(). E_OUTOFMEMORY");

	}
}

void TexCubeResource::DoFree()
{
	_d3dxLoadUsed = false;
	_texture->Release();
	_texture = 0;
}

void TexCubeResource::DoUpdate()
{
	if (_d3dxLoadUsed)
		return;

	bool manualFilter = GetLevelCnt() != 1 && (GetUsage() & D3DUSAGE_AUTOGENMIPMAP) && _data->IsCompressed();

	IDirect3DCubeTexture9* tmpTex;
	bool _notDirAccess = GetMemoryPool() == D3DPOOL_DEFAULT && !(GetUsage() & D3DUSAGE_DYNAMIC);
	if (_notDirAccess)	
		GetEngine()->GetDriver().GetDevice()->CreateCubeTexture(_data->GetWidth()/6, manualFilter ? GetLevelCnt() : 1, 0, _data->GetFormat(), D3DPOOL_SYSTEMMEM, &tmpTex, 0);	
	else
		tmpTex = _texture;

	int numFace = 0;
	for (D3DCUBEMAP_FACES i = D3DCUBEMAP_FACE_POSITIVE_X; i != D3DCUBEMAP_FACE_FORCE_DWORD; res::operator++(i), ++numFace)
	{
		D3DLOCKED_RECT source;
		tmpTex->LockRect(i, 0, &source, 0, D3DLOCK_READONLY);

		switch (_data->GetFormat())
		{
		case D3DFMT_DXT1:
		case D3DFMT_DXT2:
		case D3DFMT_DXT3:
		case D3DFMT_DXT4:
		case D3DFMT_DXT5:
			{
				int width = (_data->GetWidth()/6 + 3) / 4;
				int height = (_data->GetHeight() + 3) / 4;
				int rowSize = width * _data->GetPixelSize();
				res::CopyPitchData(static_cast<char*>(source.pBits), source.Pitch, _data->GetData() + numFace * width * height * _data->GetPixelSize(), rowSize, rowSize, height);
				break;
			}

		default:
			{
				int rowSize = _data->GetWidth()/6 * _data->GetPixelSize();
				res::CopyPitchData(static_cast<char*>(source.pBits), source.Pitch, _data->GetData() + numFace * _data->GetWidth()/6 * _data->GetHeight() * _data->GetPixelSize(), rowSize, rowSize, _data->GetHeight());
				break;
			}
		}

		tmpTex->UnlockRect(i, 0);
	}

	if (manualFilter)
		D3DXFilterTexture(tmpTex, NULL, D3DX_DEFAULT, D3DX_DEFAULT);

	if (_notDirAccess)
	{
		GetEngine()->GetDriver().GetDevice()->UpdateTexture(tmpTex, _texture);
		tmpTex->Release();
	}
}

res::CubeImageResource* TexCubeResource::GetData()
{
	return _data;
}

const res::CubeImageResource* TexCubeResource::GetData() const
{
	return _data;
}

res::CubeImageResource* TexCubeResource::GetOrCreateData()
{
	if (!_data)
	{
		_data = new res::CubeImageResource();
		_data->AddRef();
		_createData = true;
	}

	return _data;	
}

void TexCubeResource::SetData(res::CubeImageResource* value)
{
	if (ReplaceRef(_data, value))
	{
		if (_createData)
		{
			delete _data;
			_createData = false;
		}

		_data = value;		
	}
}

IDirect3DCubeTexture9* TexCubeResource::GetTex()
{
	LSL_ASSERT(IsInit());

	return _texture;
}




RenderTargetResource::RenderTargetResource(): _surface(0), _width(0), _height(0), _screenScale(0, 0), _format(D3DFMT_UNKNOWN), _lockable(false), _multisampleType(D3DMULTISAMPLE_NONE), _multisampleQuality(0)
{
	SetDynamic(true);
}

RenderTargetResource::~RenderTargetResource()
{
	Free();
}

void RenderTargetResource::DoInit()
{
	unsigned screenSz[2] = {static_cast<unsigned>(_screenScale.x * GetEngine()->GetParams().BackBufferWidth), static_cast<unsigned>(_screenScale.y * GetEngine()->GetParams().BackBufferHeight)};

	HRESULT hr = GetEngine()->GetDriver().GetDevice()->CreateRenderTarget(screenSz[0] + _width, screenSz[1] + _height, _format, _multisampleType, _multisampleQuality, _lockable, &_surface, NULL);

	LSL_ASSERT(hr == D3D_OK);
}

void RenderTargetResource::DoFree()
{
	_surface->Release();	
}

void RenderTargetResource::DoUpdate()
{
	//Nothing
}

IDirect3DSurface9* RenderTargetResource::GetSurface()
{
	return _surface;
}

unsigned RenderTargetResource::GetWidth() const
{
	return _width;
}

void RenderTargetResource::SetWidth(unsigned value)
{
	_width = value;	
}

unsigned RenderTargetResource::GetHeight() const
{
	return _height;	
}

void RenderTargetResource::SetHeight(unsigned value)
{
	_height = value;	
}

D3DFORMAT RenderTargetResource::GetFormat() const
{
	return _format;	
}

void RenderTargetResource::SetFormat(D3DFORMAT value)
{
	_format = value;	
}

bool RenderTargetResource::GetLockable() const
{
	return _lockable;
}

void RenderTargetResource::SetLockable(bool value)
{
	_lockable = value;
}

D3DMULTISAMPLE_TYPE RenderTargetResource::GetMultisampleType() const
{
	return _multisampleType;
}

void RenderTargetResource::SetMultisampleType(D3DMULTISAMPLE_TYPE value)
{
	if (_multisampleType != value)
	{
		_multisampleType = value;
		Free();
	}
}

unsigned RenderTargetResource::GetMultisampleQuality() const
{
	return _multisampleQuality;
}

void RenderTargetResource::SetMultisampleQuality(unsigned value)
{
	if (_multisampleQuality != value)
	{
		_multisampleQuality = value;
		Free();
	}
}

const D3DXVECTOR2& RenderTargetResource::GetScreenScale() const
{
	return _screenScale;
}

void RenderTargetResource::SetScreenScale(D3DXVECTOR2 value)
{
	if (_screenScale != value)
	{
		_screenScale = value;
		Free();
	}
}




DepthStencilSurfaceResource::DepthStencilSurfaceResource(): _surface(0), _width(0), _height(0), _screenScale(0, 0), _format(D3DFMT_UNKNOWN), _discard(true), _multisampleType(D3DMULTISAMPLE_NONE), _multisampleQuality(0)
{
	SetDynamic(true);
}

DepthStencilSurfaceResource::~DepthStencilSurfaceResource()
{
	Free();
}

void DepthStencilSurfaceResource::DoInit()
{
	unsigned screenSz[2] = {static_cast<unsigned>(_screenScale.x * GetEngine()->GetParams().BackBufferWidth), static_cast<unsigned>(_screenScale.y * GetEngine()->GetParams().BackBufferHeight)};

	HRESULT hr = GetEngine()->GetDriver().GetDevice()->CreateDepthStencilSurface(screenSz[0] + _width, screenSz[1] + _height, _format, _multisampleType, _multisampleQuality, _discard, &_surface, 0);

	LSL_ASSERT(hr == D3D_OK);
}

void DepthStencilSurfaceResource::DoFree()
{
	_surface->Release();	
}

void DepthStencilSurfaceResource::DoUpdate()
{
	//Nothing
}

IDirect3DSurface9* DepthStencilSurfaceResource::GetSurface()
{
	return _surface;
}

unsigned DepthStencilSurfaceResource::GetWidth() const
{
	return _width;
}

void DepthStencilSurfaceResource::SetWidth(unsigned value)
{
	_width = value;	
}

unsigned DepthStencilSurfaceResource::GetHeight() const
{
	return _height;	
}

void DepthStencilSurfaceResource::SetHeight(unsigned value)
{
	_height = value;	
}

D3DFORMAT DepthStencilSurfaceResource::GetFormat() const
{
	return _format;	
}

void DepthStencilSurfaceResource::SetFormat(D3DFORMAT value)
{
	_format = value;	
}

bool DepthStencilSurfaceResource::GetDiscard() const
{
	return _discard;
}

void DepthStencilSurfaceResource::SetDiscard(bool value)
{
	_discard = value;
}

D3DMULTISAMPLE_TYPE DepthStencilSurfaceResource::GetMultisampleType() const
{
	return _multisampleType;
}

void DepthStencilSurfaceResource::SetMultisampleType(D3DMULTISAMPLE_TYPE value)
{
	if (_multisampleType != value)
	{
		_multisampleType = value;
		Free();
	}
}

unsigned DepthStencilSurfaceResource::GetMultisampleQuality() const
{
	return _multisampleQuality;
}

void DepthStencilSurfaceResource::SetMultisampleQuality(unsigned value)
{
	if (_multisampleQuality != value)
	{
		_multisampleQuality = value;
		Free();
	}
}

const D3DXVECTOR2& DepthStencilSurfaceResource::GetScreenScale() const
{
	return _screenScale;
}

void DepthStencilSurfaceResource::SetScreenScale(D3DXVECTOR2 value)
{
	if (_screenScale != value)
	{
		_screenScale = value;
		Free();
	}
}




TextFont::TextFont(): _font(0)
{
}

TextFont::~TextFont()
{
	Free();
}

void TextFont::DoInit()
{
	HRESULT hr = D3DXCreateFont(GetEngine()->GetDriver().GetDevice(), _desc.height, _desc.width, _desc.weight, _desc. mipLevels, _desc.italic, _desc.charSet, _desc.outputPrecision, _desc.quality, _desc.pitchAndFamily, _desc.pFacename.c_str(), &_font);

	if (hr != D3D_OK)
		throw lsl::Error("void TextFont::DoInit(). hr != D3D_OK");
}

void TextFont::DoFree()
{
	lsl::SafeRelease(_font);
}

void TextFont::DoUpdate()
{
}

void TextFont::DrawText(const std::string& text, lsl::Rect& rect, DWORD format, const D3DCOLOR& color)
{
	RECT mtRect;
	SetRect(&mtRect, rect.left, rect.top, rect.right, rect.bottom);

	_font->DrawTextA(0, text.c_str(), text.size(), &mtRect, format, color);
	
	rect = lsl::Rect(mtRect.left, mtRect.top, mtRect.right, mtRect.bottom);
}

void TextFont::DrawText(const std::wstring& text, lsl::Rect& rect, DWORD format, const D3DCOLOR& color)
{
	RECT mtRect;
	SetRect(&mtRect, rect.left, rect.top, rect.right, rect.bottom);

	_font->DrawTextW(0, text.c_str(), text.size(), &mtRect, format, color);

	rect = lsl::Rect(mtRect.left, mtRect.top, mtRect.right, mtRect.bottom);
}

void TextFont::OnLostDevice()
{
	_font->OnLostDevice();
}

void TextFont::OnResetDevice()
{
	_font->OnResetDevice();
}

const TextFont::Desc& TextFont::GetDesc() const
{
	return _desc;
}

void TextFont::SetDesc(const Desc& value)
{
	_desc = value;
	
	if (IsInit())
		Reload();
}

}

}