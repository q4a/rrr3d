#ifndef RENDER_IO_DATA
#define RENDER_IO_DATA

#include "lslResource.h"
#include "r3dMath.h"
#include "d3d9.h" //��������

namespace r3d
{

namespace res
{

//enum VertexFormat {vfPosition, vfNormal, vfTex1, VERTEX_FORMAT_END};
//typedef lsl::Bitset<VERTEX_FORMAT_END> VertexFormats;

template<class _IndexType> struct TriFace
{
	_IndexType v1;
	_IndexType v2;
	_IndexType v3;

	TriFace();
	TriFace(_IndexType vert1, _IndexType vert2, _IndexType vert3);
};

typedef TriFace<unsigned short> TriFace16;
typedef TriFace<unsigned int> TriFace32;

struct VertexP
{
	static const DWORD fvf = D3DFVF_XYZ;

	VertexP();
	VertexP(const glm::vec3& mPos);

	glm::vec3 pos;
};

struct VertexPD
{
	static const DWORD fvf = D3DFVF_XYZ | D3DFVF_DIFFUSE;

	glm::vec3 pos;
	glm::vec4 diffuse;

	VertexPD();
	VertexPD(glm::vec3 position, glm::vec4 diffuseColor);
};

struct VertexPN
{
	static const DWORD fvf = D3DFVF_XYZ | D3DFVF_NORMAL;

	glm::vec3 pos;
	glm::vec3 norm;

	VertexPN();
	VertexPN(glm::vec3 position, glm::vec3 normal);
};

struct VertexPT
{
	static const DWORD fvf = D3DFVF_XYZ | D3DFVF_TEX1;

	glm::vec3 pos;
	glm::vec2 tex;

	VertexPT();
	VertexPT(glm::vec3 position, glm::vec2 texCoord);
};

struct VertexPNT
{
	static const DWORD fvf = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1;

	glm::vec3 pos;
	glm::vec3 norm;
	glm::vec2 tex;

	VertexPNT();
	VertexPNT(const glm::vec3& position, const glm::vec3& normal, const glm::vec2& texCoord);
};

struct ScreenVertex
{
	static const DWORD fvf = D3DFVF_XYZRHW | D3DFVF_TEX1;

	glm::vec4 pos;
	glm::vec2 tex;

	ScreenVertex();
	ScreenVertex(const glm::vec4& position, const glm::vec2& texCoord);
};

struct VertexIter;

class VertexData: public lsl::BufferResource
{
private:
	typedef BufferResource _MyBase;
public:
	enum Element {vtPos3 = 0, vtPos4, vtNormal, vtColor, vtTex0, vtTex1, vtTangent, vtBinormal, cElementEnd};
	typedef lsl::Bitset<cElementEnd> Format;

	static const int cElementSize[cElementEnd];

	typedef VertexIter iterator;
private:
	unsigned _vertexCount;
	Format _format;
	bool _screenRHW;

	unsigned _vertexSize;
	unsigned _elemOff[cElementEnd];
	glm::vec3 _minPos;
	glm::vec3 _maxPos;

	void CalcDimensions();
protected:
	virtual void DoUpdate();
public:
	VertexData();

	void Assign(const VertexData& value);
	void ChangeFormat(const Format& value);
	void CopyDataTo(void* target, int sVertex = 0, int vertCnt = -1) const;

	unsigned GetVertexCount() const;
	void SetVertexCount(unsigned value);
	//
	const Format& GetFormat() const;
	bool GetFormat(Element element) const;
	void SetFormat(const Format& value);
	void SetFormat(Element element, bool value = true);

	char* GetVertex(unsigned index);
	char* GetVertex(unsigned index, Element element);
	void SetVertex(unsigned index, const char* value);
	void SetVertex(unsigned index, Element element, const char* value);

	virtual unsigned GetSize() const;
	unsigned GetVertexSize() const;
	unsigned GetElemOff(Element value) const;
	DWORD GetFVF() const;

	//
	const glm::vec3& GetMinPos() const;
	const glm::vec3& GetMaxPos() const;

	bool GetScreenRHW() const;
	void SetScreenRHW(bool value);

	VertexIter begin();
	VertexIter end();

	VertexIter operator[](unsigned index);
};

struct VertexIter
{
	unsigned _index;
	VertexData* _owner;

	VertexIter();
	VertexIter(unsigned index, VertexData* owner);

	const glm::vec3* Pos3() const;
	glm::vec3* Pos3();
	glm::vec4* Pos4();
	glm::vec4* Color();
	glm::vec2* Tex0();
	glm::vec2* Tex1();
	glm::vec3* Normal();
	glm::vec3* Tangent();
	glm::vec3* Binormal();

	const char* GetElem(VertexData::Element element) const;
	void SetElem(VertexData::Element element, const char* value);

	//
	VertexIter& operator+=(int value);
	VertexIter& operator-=(int value);
	//
	VertexIter& operator++();
	VertexIter& operator--();
	//
	bool operator==(const VertexIter& value);
	bool operator!=(const VertexIter& value);
};

class IndexData: public lsl::BufferResource
{
private:
	unsigned _indexCount;
	D3DFORMAT _indexFormat;
public:
	IndexData();

	unsigned GetIndexSize() const;
	unsigned GetIndexCount() const;
	void SetIndexCount(unsigned value);
	D3DFORMAT GetIndexFormat() const;
	void SetIndexFormat(D3DFORMAT value);
	virtual unsigned GetSize() const;
};

class TriFaceData: public lsl::BufferResource
{
private:
	unsigned _faceCount;
	D3DFORMAT _indexFormat;
public:
	TriFaceData();

	unsigned GetFaceCount() const;
	void SetFaceCount(unsigned value);
	D3DFORMAT GetIndexFormat() const;
	void SetIndexFormat(D3DFORMAT value);

	unsigned GetIndex(unsigned face, unsigned indTri) const;

	virtual unsigned GetSize() const;
	unsigned GetFaceSize() const;
};

struct FaceGroup
{
	int sFace;
	int faceCnt;
	int sVertex;
	int vertexCnt;
	int mathId;

	glm::vec3 minPos;
	glm::vec3 maxPos;

	FaceGroup();
	FaceGroup(int startFace, int faceCount, int startVertex, int vertexCount);
};

class MeshData: public lsl::FileResource
{
public:
	typedef lsl::ClassMapList<std::string, lsl::IOResource<MeshData>> ResFormats;

	static ResFormats& GetResFormats();
protected:
	virtual void DoInit();
	virtual void DoFree();
	virtual void DoUpdate();
	virtual void DoLoadFromStream(std::istream& stream, const std::string& fileExt);
public:
	MeshData();
	virtual ~MeshData();

	void CalcTangentSpace();

	VertexData vb;
	TriFaceData fb;
	std::vector<FaceGroup> faceGroups;
};

class ImageResource: public lsl::BufferResource
{
public:
	typedef lsl::ClassMapList<std::string, lsl::IOResource<ImageResource>> ResFormats;

	static ResFormats& GetResFormats();
private:
	unsigned _size;
	unsigned _width;
	unsigned _height;
	D3DFORMAT _format;
protected:
	virtual void DoLoadFromStream(std::istream& stream, const std::string& fileExt);
public:
	ImageResource();

	virtual unsigned GetSize() const;
	unsigned GetSizeExt() const;
	void SetSize(unsigned value);

	unsigned GetWidth() const;
	void SetWidth(unsigned value);

	unsigned GetHeight() const;
	void SetHeight(unsigned value);

	D3DFORMAT GetFormat() const;
	void SetFormat(D3DFORMAT value);

	unsigned GetPixelSize() const;
	bool IsCompressed() const;
};

class CubeImageResource: public ImageResource
{
public:
	typedef lsl::ClassMapList<std::string, lsl::IOResource<CubeImageResource>> ResFormats;

	static ResFormats& GetResFormats();
protected:
	virtual void DoLoadFromStream(std::istream& stream, const std::string& fileExt);
public:
	CubeImageResource();
};

unsigned GetVertexFVFSize(DWORD fvf);
unsigned GetElementFormatSize(D3DFORMAT format);
unsigned GetPrimitivesCount(D3DPRIMITIVETYPE primitiveType, UINT numControlPoint);
void CopyPitchData(char* dest, unsigned destPitch, const char* src, unsigned srcPitch, unsigned srcRowSize, unsigned height);
void operator++(D3DCUBEMAP_FACES& lvalue);

template<class _IndexType> TriFace<_IndexType>::TriFace()
{
}

template<class _IndexType> TriFace<_IndexType>::TriFace(_IndexType vert1, _IndexType vert2, _IndexType vert3): v1(vert1), v2(vert2), v3(vert3)
{
}

}

}

#endif
