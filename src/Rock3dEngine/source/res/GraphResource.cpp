#include "stdafx.h"

#include "res\\GraphResource.h"

namespace r3d
{

namespace res
{

const int VertexData::cElementSize[cElementEnd] = 
{
	sizeof(D3DXVECTOR3), 
	sizeof(D3DXVECTOR4),
	sizeof(D3DXVECTOR3), 
	sizeof(D3DCOLOR),
	sizeof(D3DXVECTOR2),
	sizeof(D3DXVECTOR2),
	sizeof(D3DXVECTOR3),
	sizeof(D3DXVECTOR3)
};

MeshData::ResFormats meshResFormats;
ImageResource::ResFormats imageResFormats;
CubeImageResource::ResFormats cubeImageResFormats;




VertexP::VertexP()
{
}

VertexP::VertexP(const D3DXVECTOR3& mPos): pos(mPos)
{
}




VertexPD::VertexPD()
{
}

VertexPD::VertexPD(D3DXVECTOR3 position, D3DCOLOR diffuseColor): pos(position), diffuse(diffuseColor)
{
}




VertexPN::VertexPN()
{
}

VertexPN::VertexPN(D3DXVECTOR3 position, D3DXVECTOR3 normal): pos(position), norm(normal)
{
}




VertexPT::VertexPT()
{
}

VertexPT::VertexPT(D3DXVECTOR3 position, D3DXVECTOR2 texCoord): pos(position), tex(texCoord)
{
}




VertexPNT::VertexPNT()
{
}

VertexPNT::VertexPNT(const D3DXVECTOR3& position, const D3DXVECTOR3& normal, const D3DXVECTOR2& texCoord): pos(position), norm(normal), tex(texCoord)
{
}




ScreenVertex::ScreenVertex()
{
}

ScreenVertex::ScreenVertex(const D3DXVECTOR4& position, const D3DXVECTOR2& texCoord): pos(position), tex(texCoord)
{
}




VertexData::VertexData(): _vertexCount(0), _vertexSize(0), _screenRHW(false)
{
}

void VertexData::CalcDimensions()
{
	if (_vertexCount > 0 && _format.test(vtPos3))
	{
		VertexIter iter(0, this);
		_maxPos = _minPos = *iter.Pos3();
		++iter;
		for (unsigned i = 1; i < _vertexCount; ++i, ++iter)
		{
			D3DXVec3Minimize(&_minPos, &_minPos, iter.Pos3());
			D3DXVec3Maximize(&_maxPos, &_maxPos, iter.Pos3());
		}
	}
}

void VertexData::DoUpdate()
{
	CalcDimensions();
}

void VertexData::Assign(const VertexData& value)
{
	SetFormat(value._format);
	SetVertexCount(value._vertexCount);
	Init();
	CopyDataFrom(value.GetData());
	_minPos = value._minPos;
	_maxPos = value._maxPos;
}

void VertexData::ChangeFormat(const Format& value)
{
	VertexData tmpData;
	tmpData.Assign(*this);

	SetFormat(value);
	Init();

	for (unsigned i = 0; i < _vertexCount; ++i)	
		for (unsigned j = 0; j < cElementEnd; ++j)
			if (_format.test(j) && tmpData._format.test(j))
			{
				Element element = static_cast<Element>(j);
				SetVertex(i, element, tmpData.GetVertex(i, element));
			}
	_minPos = tmpData._minPos;
	_maxPos = tmpData._maxPos;
}

void VertexData::CopyDataTo(void* target, int sVertex, int vertCnt) const
{
	if (vertCnt == -1)
		vertCnt = _vertexCount;

	_MyBase::CopyDataTo(target, _vertexSize * sVertex, _vertexSize * vertCnt);
}

unsigned VertexData::GetVertexCount() const
{
	return _vertexCount;
}

void VertexData::SetVertexCount(unsigned value)
{
	_vertexCount = value;
	Free();
}

const VertexData::Format& VertexData::GetFormat() const
{
	return _format;
}

bool VertexData::GetFormat(Element element) const
{
	return _format.test(element);
}

void VertexData::SetFormat(const Format& value)
{
	if (_format != value)
	{
		ZeroMemory(_elemOff, sizeof(_elemOff));
		_format = value;

		unsigned off = 0;
		for (unsigned i = 0; i < cElementEnd; ++i)
			if (_format.test(i))
			{
				_elemOff[i] = off;
				off += cElementSize[i];
			}

		_vertexSize = off;

		Free();
	}
}

void VertexData::SetFormat(Element element, bool value)
{
	Format format(_format);
	format.set(element, value);
	SetFormat(format);
}

char* VertexData::GetVertex(unsigned index)
{
	LSL_ASSERT(index < _vertexCount);

	return GetData() + index * _vertexSize;
}

char* VertexData::GetVertex(unsigned index, Element element)
{
	return _format.test(element) ? GetVertex(index) + GetElemOff(element) : 0;
}

void VertexData::SetVertex(unsigned index, const char* value)
{
	memcpy(GetVertex(index), value, _vertexSize);
}

void VertexData::SetVertex(unsigned index, Element element, const char* value)
{
	memcpy(GetVertex(index, element), value, cElementSize[element]);
}

unsigned VertexData::GetSize() const
{
	return _vertexCount * _vertexSize;
}

unsigned VertexData::GetVertexSize() const
{
	return _vertexSize;
}

unsigned VertexData::GetElemOff(Element value) const
{
	LSL_ASSERT(_format.test(value));

	return _elemOff[value];
}

DWORD VertexData::GetFVF() const
{
	DWORD fvf = 0;
	unsigned texCnt = 0;
	DWORD texSize = 0;

	for (unsigned i = 0; i < cElementEnd; ++i)
		if (_format.test(i))		
			switch (i)
			{
			case vtPos3:
				fvf |= D3DFVF_XYZ;
				break;

			case vtPos4:
				fvf |= _screenRHW ? D3DFVF_XYZRHW : D3DFVF_XYZW;
				break;

			case vtColor:
				fvf |= D3DFVF_DIFFUSE;
				break;

			case vtTex0:
				++texCnt;
				texSize |= D3DFVF_TEXCOORDSIZE2(0);
				break;

			case vtTex1:
				++texCnt;
				texSize |= D3DFVF_TEXCOORDSIZE2(1);				
				break;

			case vtNormal:
				fvf |= D3DFVF_NORMAL;
				break;

			case vtTangent:
				++texCnt;
				texSize |= D3DFVF_TEXCOORDSIZE3(1);
				break;

			case vtBinormal:
				++texCnt;
				texSize |= D3DFVF_TEXCOORDSIZE3(2);
				break;
			}

	static const unsigned texFVFCnt[8] = {D3DFVF_TEX1, D3DFVF_TEX2, D3DFVF_TEX3, D3DFVF_TEX4, D3DFVF_TEX5, D3DFVF_TEX6, D3DFVF_TEX7, D3DFVF_TEX8};
	if (texCnt > 0)
		fvf |= texFVFCnt[texCnt - 1] | texSize;

	return fvf;
}

const D3DXVECTOR3& VertexData::GetMinPos() const
{
	return _minPos;
}

const D3DXVECTOR3& VertexData::GetMaxPos() const
{
	return _maxPos;
}

bool VertexData::GetScreenRHW() const
{
	return _screenRHW;
}

void VertexData::SetScreenRHW(bool value)
{
	_screenRHW = value;
}

VertexIter VertexData::begin()
{
	return VertexIter(0, this);
}

VertexIter VertexData::end()
{
	return VertexIter(_vertexCount, this);
}

VertexIter VertexData::operator[](unsigned index)
{
	return VertexIter(index, this);
}




VertexIter::VertexIter(): _index(0), _owner(0)
{
}

VertexIter::VertexIter(unsigned index, VertexData* owner): _index(index), _owner(owner)
{
}

const D3DXVECTOR3* VertexIter::Pos3() const
{
	return reinterpret_cast<const D3DXVECTOR3*>(_owner->GetVertex(_index, VertexData::vtPos3));
}

D3DXVECTOR3* VertexIter::Pos3()
{
	return reinterpret_cast<D3DXVECTOR3*>(_owner->GetVertex(_index, VertexData::vtPos3));
}

D3DXVECTOR4* VertexIter::Pos4()
{
	return reinterpret_cast<D3DXVECTOR4*>(_owner->GetVertex(_index, VertexData::vtPos4));
}

D3DCOLOR* VertexIter::Color()
{
	return reinterpret_cast<D3DCOLOR*>(_owner->GetVertex(_index, VertexData::vtColor));
}

D3DXVECTOR2* VertexIter::Tex0()
{
	return reinterpret_cast<D3DXVECTOR2*>(_owner->GetVertex(_index, VertexData::vtTex0));
}

D3DXVECTOR2* VertexIter::Tex1()
{
	return reinterpret_cast<D3DXVECTOR2*>(_owner->GetVertex(_index, VertexData::vtTex1));
}

D3DXVECTOR3* VertexIter::Normal()
{
	return reinterpret_cast<D3DXVECTOR3*>(_owner->GetVertex(_index, VertexData::vtNormal));
}

D3DXVECTOR3* VertexIter::Tangent()
{
	return reinterpret_cast<D3DXVECTOR3*>(_owner->GetVertex(_index, VertexData::vtTangent));
}

D3DXVECTOR3* VertexIter::Binormal()
{
	return reinterpret_cast<D3DXVECTOR3*>(_owner->GetVertex(_index, VertexData::vtBinormal));
}

const char* VertexIter::GetElem(VertexData::Element element) const
{
	return _owner->GetVertex(_index, element);
}

void VertexIter::SetElem(VertexData::Element element, const char* value)
{
	_owner->SetVertex(_index, element, value);
}

VertexIter& VertexIter::operator+=(int value)
{
	_index += value;
	return *this;
}

VertexIter& VertexIter::operator-=(int value)
{
	_index -= value;
	return *this;
}

VertexIter& VertexIter::operator++()
{
	++_index;
	return *this;
}

VertexIter& VertexIter::operator--()
{
	--_index;
	return *this;
}

bool VertexIter::operator==(const VertexIter& value)
{
	return _index == value._index;
}

bool VertexIter::operator!=(const VertexIter& value)
{
	return _index != value._index;
}




IndexData::IndexData(): _indexCount(0), _indexFormat(D3DFMT_UNKNOWN)
{
}

unsigned IndexData::GetIndexSize() const
{
	return GetElementFormatSize(_indexFormat);
}

unsigned IndexData::GetIndexCount() const
{
	return _indexCount;
}

void IndexData::SetIndexCount(unsigned value)
{
	_indexCount = value;
	Free();
}

D3DFORMAT IndexData::GetIndexFormat() const
{
	return _indexFormat;
}

void IndexData::SetIndexFormat(D3DFORMAT value)
{
	_indexFormat = value;
	Free();
}

unsigned IndexData::GetSize() const
{
	return _indexCount * GetIndexSize();
}




TriFaceData::TriFaceData(): _faceCount(0), _indexFormat(D3DFMT_UNKNOWN)
{
}

unsigned TriFaceData::GetFaceCount() const
{
	return _faceCount;
}

void TriFaceData::SetFaceCount(unsigned value)
{
	_faceCount = value;
	Free();
}

D3DFORMAT TriFaceData::GetIndexFormat() const
{
	return _indexFormat;
}

void TriFaceData::SetIndexFormat(D3DFORMAT value)
{
	_indexFormat = value;
	Free();
}

unsigned TriFaceData::GetIndex(unsigned face, unsigned indTri) const
{
	switch (_indexFormat)
	{
	case D3DFMT_INDEX16:
		return *reinterpret_cast<const unsigned short*>(GetData() + sizeof(unsigned short) * (face * 3 + indTri));
		
	case D3DFMT_INDEX32:
		return *reinterpret_cast<const unsigned*>(GetData() + sizeof(unsigned) * (face * 3 + indTri));
		
	default:
		LSL_ASSERT(false);
		return 0;
	}	
}

unsigned TriFaceData::GetSize() const
{
	return _faceCount * GetFaceSize();
}

unsigned TriFaceData::GetFaceSize() const
{
	return GetElementFormatSize(_indexFormat) * 3;
}





FaceGroup::FaceGroup(): minPos(NullVector), maxPos(NullVector)
{
}

FaceGroup::FaceGroup(int startFace, int faceCount, int startVertex, int vertexCount): sFace(startFace), faceCnt(faceCount), sVertex(startVertex), vertexCnt(vertexCount), minPos(NullVector), maxPos(NullVector)
{
}




MeshData::ResFormats& MeshData::GetResFormats()
{
	return meshResFormats;
}




MeshData::MeshData()
{
}

MeshData::~MeshData()
{
	Free();
}

void CalcTangentBasis(const D3DXVECTOR3 &p1, const D3DXVECTOR3 &p2, const D3DXVECTOR3 &p3, const D3DXVECTOR2 &t1, const D3DXVECTOR2 &t2, const D3DXVECTOR2 &t3, D3DXVECTOR3& tangent, D3DXVECTOR3& binormal)
{
	D3DXVECTOR3 e1  = p2 - p1;
	D3DXVECTOR3 e2  = p3 - p1;
	D3DXVECTOR2 et1 = t2 - t1;
	D3DXVECTOR2 et2 = t3 - t1;
	
	float tmp = 0.0;
	if (fabsf(et1.x*et2.y - et1.y*et2.x)<0.0001f)
		tmp = 1.0f;
	else
		tmp = 1.0f / (et1.x*et2.y - et1.y*et2.x);
	
	tangent  = (e1 * et2.y - e2 * et1.y) * tmp;
	binormal = (e2 * et1.x - e1 * et2.x) * tmp;
	
	D3DXVec3Normalize(&tangent, &tangent);
	D3DXVec3Normalize(&binormal, &binormal);	
}

void MeshData::DoInit()
{
	vb.Init();
	fb.Init();
}

void MeshData::DoFree()
{
	vb.Free();
	fb.Free();
}

void MeshData::DoUpdate()
{
	vb.Update();
	fb.Update();

	for (unsigned i = 0; i < faceGroups.size(); ++i)
	{
		FaceGroup& fg = faceGroups[i];

		if (fg.vertexCnt == 0 || !vb.GetFormat().test(VertexData::vtPos3))
		{
			fg.minPos = fg.maxPos = NullVector;
			continue;
		}

		for (int j = 0; j < fg.faceCnt; ++j)
			for (int k = 0; k < 3; ++k)
			{
				int triIndex = fb.GetIndex(fg.sFace + j, k);
				VertexIter iter = vb[triIndex];
				const D3DXVECTOR3* pos = iter.Pos3();

				if (j == 0 && k == 0)
					fg.minPos = fg.maxPos = *pos;
				else
				{
					D3DXVec3Minimize(&fg.minPos, &fg.minPos, pos);
					D3DXVec3Maximize(&fg.maxPos, &fg.maxPos, pos);
				}
			}
	}
}
	
void MeshData::DoLoadFromStream(std::istream& stream, const std::string& fileExt)
{
	GetResFormats().GetInstance(fileExt).LoadFromStream(*this, stream);
}

void MeshData::CalcTangentSpace()
{
	res::VertexData::Format format = vb.GetFormat();
	format.set(res::VertexData::vtTangent);
	format.set(res::VertexData::vtBinormal);
	vb.ChangeFormat(format);

	LSL_ASSERT(vb.GetFormat(VertexData::vtPos3) && vb.GetFormat(VertexData::vtTex0) && vb.GetFormat(VertexData::vtTangent) && vb.GetFormat(VertexData::vtBinormal));
	
	for (unsigned i = 0; i < vb.GetVertexCount(); ++i)
	{
		*vb[i].Tangent() = NullVector;
		*vb[i].Binormal() = NullVector;
	}

	for (unsigned i = 0; i < fb.GetFaceCount(); ++i)
	{
		unsigned a = fb.GetIndex(i, 0);
		unsigned b = fb.GetIndex(i, 1);
		unsigned c = fb.GetIndex(i, 2);
		
		D3DXVECTOR3 bin, tan;
		CalcTangentBasis(*vb[a].Pos3(), *vb[b].Pos3(), *vb[c].Pos3(), *vb[a].Tex0(), *vb[b].Tex0(), *vb[c].Tex0(), tan, bin);

		*vb[a].Tangent() += tan;
		*vb[b].Tangent() += tan;
		*vb[c].Tangent() += tan;

		*vb[a].Binormal() += bin;
		*vb[b].Binormal() += bin;
		*vb[c].Binormal() += bin;
	}

	for (unsigned i = 0; i < vb.GetVertexCount(); ++i)
	{
		D3DXVec3Normalize(vb[i].Tangent(), vb[i].Tangent());
		D3DXVec3Normalize(vb[i].Binormal(), vb[i].Binormal());

		D3DXVECTOR3 tmpT = *vb[i].Tangent();
		D3DXVECTOR3 tmpB = *vb[i].Binormal();
		D3DXVECTOR3 tmpN = *vb[i].Normal();

		D3DXVECTOR3 newT = tmpT - (D3DXVec3Dot(&tmpN, &tmpT) * tmpN);
		D3DXVECTOR3 newB = tmpB - (D3DXVec3Dot(&tmpN, &tmpB) * tmpN) - (D3DXVec3Dot(&newT, &tmpB) * newT);		
		D3DXVec3Normalize(&newT, &newT);
		D3DXVec3Normalize(&newB, &newB);
		*vb[i].Tangent() = newT;
		*vb[i].Binormal() = newB;

		float lenT = D3DXVec3Length(&newT);
		float lenB = D3DXVec3Length(&newB);

		if (lenT <= 0.0001 || lenB <= 0.0001)
		{
			if (lenT > 0.5)
				D3DXVec3Cross(vb[i].Binormal(), &tmpN, vb[i].Tangent());
			else 
				if (lenB > 0.5)
					D3DXVec3Cross(vb[i].Tangent(), vb[i].Binormal(), &tmpN);
				else
				{
					D3DXVECTOR3 startAxis;
					if (D3DXVec3Dot(&XVector, &tmpN) < D3DXVec3Dot(&YVector, &tmpN))
						startAxis = XVector;
					else
						startAxis = YVector;
					D3DXVec3Cross(vb[i].Tangent(), &tmpN, &startAxis);
					D3DXVec3Cross(vb[i].Binormal(), &tmpN, vb[i].Tangent());
				}
		}
		else 
			if (D3DXVec3Dot(vb[i].Binormal(), vb[i].Tangent()) > 0.9999f)
				D3DXVec3Cross(vb[i].Binormal(), &tmpN, vb[i].Tangent());
	}
}




ImageResource::ResFormats& ImageResource::GetResFormats()
{
	return imageResFormats;
}




ImageResource::ImageResource(): _size(0), _width(0), _height(0), _format(D3DFMT_UNKNOWN)
{
}

void ImageResource::DoLoadFromStream(std::istream& stream, const std::string& fileExt)
{
	GetResFormats().GetInstance(fileExt).LoadFromStream(*this, stream);
}

unsigned ImageResource::GetSize() const
{
	if (_size != 0)
		return _size;

	switch (_format)
	{
	case D3DFMT_DXT1:
	case D3DFMT_DXT2:
	case D3DFMT_DXT3:
	case D3DFMT_DXT4:
	case D3DFMT_DXT5:
		return (_width + 3) / 4 * (_height + 3) / 4 * GetPixelSize();

	default:
		return _width * _height * GetPixelSize();
	}
}

unsigned ImageResource::GetSizeExt() const
{
	return _size;
}

void ImageResource::SetSize(unsigned value)
{
	if (_size != value)
	{
		_size = value;
		Free();
	}
}

unsigned ImageResource::GetWidth() const
{
	return _width;
}

void ImageResource::SetWidth(unsigned value)
{
	_width = value;
	Free();
}

unsigned ImageResource::GetHeight() const
{
	return _height;
}

void ImageResource::SetHeight(unsigned value)
{
	_height = value;
	Free();
}

D3DFORMAT ImageResource::GetFormat() const
{
	return _format;
}

void ImageResource::SetFormat(D3DFORMAT value)
{
	_format = value;
	Free();
}

unsigned ImageResource::GetPixelSize() const
{
	return GetElementFormatSize(_format);
}

bool ImageResource::IsCompressed() const
{
	switch (_format)
	{
		case D3DFMT_DXT1:
		case D3DFMT_DXT2:
		case D3DFMT_DXT3:
		case D3DFMT_DXT4:
		case D3DFMT_DXT5:
			return true;
	}

	return false;
}




CubeImageResource::ResFormats& CubeImageResource::GetResFormats()
{
	return cubeImageResFormats;
}




CubeImageResource::CubeImageResource()
{
}

void CubeImageResource::DoLoadFromStream(std::istream& stream, const std::string& fileExt)
{
	GetResFormats().GetInstance(fileExt).LoadFromStream(*this, stream);
}




unsigned GetVertexFVFSize(DWORD fvf)
{
	unsigned sz = 0;

	if ((fvf & D3DFVF_POSITION_MASK) == D3DFVF_XYZ)
		sz += sizeof(float) * 3;

	if ((fvf & D3DFVF_POSITION_MASK) == D3DFVF_XYZRHW)
		sz += sizeof(float) * 4;

	if ((fvf & D3DFVF_POSITION_MASK) == D3DFVF_XYZW)
		sz += sizeof(float) * 4;

	if (fvf & D3DFVF_NORMAL)
		sz += sizeof(float) * 3;

	if (fvf & D3DFVF_DIFFUSE)
		sz += sizeof(float);
	
	unsigned texCnt = ((fvf & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT);
	if (texCnt > 0)
		switch ((fvf >> 16) & 0x1)
		{
		case D3DFVF_TEXTUREFORMAT1:
			texCnt *= sizeof(float) * 1;
			break;

		case D3DFVF_TEXTUREFORMAT2:
			texCnt *= sizeof(float) * 2;
			break;

		case D3DFVF_TEXTUREFORMAT3:
			texCnt *= sizeof(float) * 3;
			break;

		case D3DFVF_TEXTUREFORMAT4:
			texCnt *= sizeof(float) * 4;
			break;
		}
	sz += texCnt;
	return sz;
}

unsigned GetElementFormatSize(D3DFORMAT format)
{
	switch (format)
	{
	case D3DFMT_R8G8B8:
		return 3;

	case D3DFMT_A8R8G8B8:
		return 4;

	case D3DFMT_X8R8G8B8:
		return 4;

	case D3DFMT_A16B16G16R16F:
		return 8;

	//block (4x4 pixel) size
	case D3DFMT_DXT1:
		return 8;

	//block (4x4 pixel) size
	case D3DFMT_DXT2:
	case D3DFMT_DXT3:
	case D3DFMT_DXT4:
	case D3DFMT_DXT5:
		return 16;

	case D3DFMT_INDEX16:
		return 2;

	case D3DFMT_INDEX32:
		return 4;

	default:
		LSL_ASSERT(false);
		return 0;
	}
}

unsigned GetPrimitivesCount(D3DPRIMITIVETYPE primitiveType, UINT numControlPoint)
{
	switch (primitiveType)
	{
	case D3DPT_POINTLIST:
		return numControlPoint;

	case D3DPT_LINELIST:
		return numControlPoint / 2;	

	case D3DPT_LINESTRIP:
		return numControlPoint;	

	case D3DPT_TRIANGLELIST:
		return numControlPoint / 3;	

	case D3DPT_TRIANGLESTRIP:
		return numControlPoint - 2;		

	case D3DPT_TRIANGLEFAN:
		return numControlPoint - 2;	

	default:
		LSL_ASSERT(false);
		return 0;
	}
}

void CopyPitchData(char* dest, unsigned destPitch, const char* src, unsigned srcPitch, unsigned srcRowSize, unsigned height)
{	
	//–азмер шага совпадает с байтовой длиной, замечательно!
	if (destPitch == srcPitch == srcRowSize)
		std::memcpy(dest, src, height * srcRowSize);
	//–азмер шага отличаетс€ от байтовой длины, плохо, издержки...
	else
		for (unsigned i = 0; i < height; ++i)
			std::memcpy(dest + destPitch * i, src + srcPitch * i, srcRowSize);
}

void operator++(D3DCUBEMAP_FACES& lvalue)
{
	switch (lvalue)
	{
	case D3DCUBEMAP_FACE_POSITIVE_X:
		lvalue = D3DCUBEMAP_FACE_NEGATIVE_X;
		break;

	case D3DCUBEMAP_FACE_NEGATIVE_X:
		lvalue = D3DCUBEMAP_FACE_POSITIVE_Y;
		break;

	case D3DCUBEMAP_FACE_POSITIVE_Y:
		lvalue = D3DCUBEMAP_FACE_NEGATIVE_Y;
		break;

	case D3DCUBEMAP_FACE_NEGATIVE_Y:
		lvalue = D3DCUBEMAP_FACE_POSITIVE_Z;
		break;

	case D3DCUBEMAP_FACE_POSITIVE_Z:
		lvalue = D3DCUBEMAP_FACE_NEGATIVE_Z;
		break;

	default:
		lvalue = D3DCUBEMAP_FACE_FORCE_DWORD;
		break;
	}
}

}

}