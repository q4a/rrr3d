#ifndef R3D_GRAPH_STDNODE
#define R3D_GRAPH_STDNODE

#include "SceneManager.h"
#include "MaterialLibrary.h"

namespace r3d
{

namespace graph
{

class MaterialNode
{
public:
	typedef lsl::Vector<graph::LibMaterial*> Materials;
private:
	Materials _materials;
	LibMaterial* _libMat;
	D3DXCOLOR _color;

	D3DXVECTOR3 _offset;
	D3DXVECTOR3 _scale;
	D3DXQUATERNION _rotate;
	D3DCULL _cullMode;

	mutable D3DXMATRIX _matrix;
	mutable bool _matChanged;
	mutable bool _defMat;

	void TransformationChanged() const;
	const D3DXMATRIX& GetMatrix() const;

	void Begin(Engine& engine);
	void End(Engine& engine);

	void BeginAfter(Engine& engine);
	void EndAfter(Engine& engine);
public:
	MaterialNode();
	~MaterialNode();

	void Save(lsl::SWriter* writer, lsl::Serializable* owner);
	void Load(lsl::SReader* reader, lsl::Serializable* owner);
	void OnFixUp(const lsl::Serializable::FixUpNames& fixUpNames, lsl::Serializable* owner);

	void Insert(graph::LibMaterial* mat, Materials::const_iterator iter);
	void Insert(graph::LibMaterial* mat);
	Materials::iterator Remove(Materials::const_iterator iter);
	Materials::iterator Remove(graph::LibMaterial* mat);
	void Clear();
	int Count() const;

	void Apply(Engine& engine, int index);
	void UnApply(Engine& engine, int index);

	void Apply(Engine& engine);
	void UnApply(Engine& engine);

	graph::LibMaterial* Get(unsigned index);
	bool Set(unsigned index, graph::LibMaterial* value);

	graph::LibMaterial* Get();
	graph::LibMaterial* GetOrCreate();
	void Set(graph::LibMaterial* value);

	const Materials& GetList() const;

	const D3DXCOLOR& GetColor() const;
	void SetColor(const D3DXCOLOR& value);

	const D3DXVECTOR3& GetOffset() const;
	void SetOffset(const D3DXVECTOR3& value);

	const D3DXVECTOR3& GetScale() const;
	void SetScale(const D3DXVECTOR3& value);

	const D3DXQUATERNION& GetRotate() const;
	void SetRotate(const D3DXQUATERNION& value);

	D3DCULL GetCullMode() const;
	void SetCullMode(D3DCULL value);
};

class IVBMeshNode: public BaseSceneNode
{
private:
	typedef BaseSceneNode _MyBase;
private:
	graph::IndexedVBMesh* _mesh;
	int _meshId;
protected:
	virtual void DoRender(Engine& engine);
	virtual AABB LocalDimensions() const;

	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
	virtual void OnFixUp(const FixUpNames& fixUpNames);
public:
	IVBMeshNode();
	virtual ~IVBMeshNode();

	using _MyBase::Render;
	void Render(Engine& engine, int meshId);

	graph::IndexedVBMesh* GetMesh();
	void SetMesh(graph::IndexedVBMesh* value);

	int GetMeshId() const;
	void SetMeshId(int value);

	unsigned GetSubsetCnt() const;

	virtual MaterialNode* GetMaterial();

	MaterialNode material;
};

class MeshXNode: public BaseSceneNode
{
private:
	typedef BaseSceneNode _MyBase;
private:
	graph::MeshX* _mesh;
	int _meshId;
protected:
	virtual void DoRender(Engine& engine);
	virtual AABB LocalDimensions() const;

	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
	virtual void OnFixUp(const FixUpNames& fixUpNames);
public:
	MeshXNode();
	virtual ~MeshXNode();

	using _MyBase::Render;
	void Render(Engine& engine, int meshId);

	graph::MeshX* GetMesh();
	void SetMesh(graph::MeshX* value);

	int GetMeshId() const;
	void SetMeshId(int value);

	unsigned GetSubsetCnt() const;

	virtual MaterialNode* GetMaterial();

	MaterialNode material;
};

class PlaneNode: public BaseSceneNode
{
	typedef BaseSceneNode _MyBase;	
private:
	graph::VBMesh _mesh;
	D3DXVECTOR2 _size;
 
	void DrawPlane(Engine& engine);
	void UpdateMesh();
protected:
	virtual void DoRender(Engine& engine);
	virtual AABB LocalDimensions() const;

	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
	virtual void OnFixUp(const FixUpNames& fixUpNames);
public:
	PlaneNode();
	virtual ~PlaneNode();

	const D3DXVECTOR2& GetSize() const;
	void SetSize(const D3DXVECTOR2& value);

	MaterialNode material;
};

class Box: public BaseSceneNode
{
	typedef BaseSceneNode _MyBase;
private:
	void RenderBox(Engine& engine);
protected:
	virtual void DoRender(Engine& engine);
	virtual AABB LocalDimensions() const;

	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
	virtual void OnFixUp(const FixUpNames& fixUpNames);
public:
	Box();

	MaterialNode material;
};

class Cylinder: public BaseSceneNode
{
private:
	graph::IndexedVBMesh* _mesh;
	D3DXCOLOR _color;
protected:
	void UpdateMesh();

	virtual void DoRender(Engine& engine);
	virtual AABB LocalDimensions() const;

	virtual void Save(lsl::SWriter* writer) {}
	virtual void Load(lsl::SReader* reader) {}
public:
	Cylinder();
	virtual ~Cylinder();

	const D3DXCOLOR& GetColor() const;
	void SetColor(const D3DXCOLOR& value);
};

class Sprite: public BaseSceneNode
{	
private:
	typedef BaseSceneNode _MyBase;
protected:
	virtual void DoRender(Engine& engine);
	virtual AABB LocalDimensions() const;

	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
	virtual void OnFixUp(const FixUpNames& fixUpNames);
public:
	Sprite();

	MaterialNode material;

	D3DXVECTOR2 sizes;
	//«афиксировать направление, по умолчанию false
	bool fixDirection;
};

class ScreenSprite: public BaseSceneNode
{
protected:
	virtual void DoRender(Engine& engine);

	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
	virtual void OnFixUp(const FixUpNames& fixUpNames);
public:
	ScreenSprite();

	MaterialNode material;

	D3DXVECTOR4 quadVertex;
	D3DXVECTOR4 uvVertex;
};

class MovCoordSys: public BaseSceneNode
{
private:
	static const D3DXVECTOR3 arUp[3];
	static const float arSize;
	static const D3DXVECTOR3 arPos[3];
	static const D3DXCOLOR arCol[3];
	static const D3DXCOLOR colSel;
public:
	enum DirMove {dmNone, dmX, dmY, dmZ, dmXY, dmXZ, dmYZ, cDirMoveEnd};
private:
	Cylinder* _arrows[3];
	DirMove _curMove;
protected:
	DirMove CompDirMove(const D3DXVECTOR3& rayStart, const D3DXVECTOR3& rayVec);

	virtual void DoRender(Engine& engine);
	virtual AABB LocalDimensions() const;
public:
	MovCoordSys();
	virtual ~MovCoordSys();

	DirMove OnMouseMove(const D3DXVECTOR3& rayStart, const D3DXVECTOR3& rayVec);
	DirMove OnMouseClick(const D3DXVECTOR3& rayStart, const D3DXVECTOR3& rayVec, lsl::KeyState state);
};

class ScaleCoordSys: public BaseSceneNode
{
private:
	static const D3DXVECTOR3 arUp[3];
	static const float arSize;
	static const float plSize;
	static const D3DXCOLOR arCol[3];
	static const D3DXCOLOR colSel;
public:
	enum DirMove {dmNone, dmX, dmY, dmZ, dmXYZ, cDirMoveEnd};
private:
	Sprite* _arrows[3];
	DirMove _curMove;	
protected:
	void CompBBPlanes(const D3DXVECTOR3& camPos, D3DXVECTOR3* bbPlanes);
	DirMove CompDirMove(const D3DXVECTOR3& rayStart, const D3DXVECTOR3& rayVec, const D3DXVECTOR3& camPos);

	virtual void DoRender(Engine& engine);
	virtual AABB LocalDimensions() const;
public:
	ScaleCoordSys();
	virtual ~ScaleCoordSys();

	DirMove OnMouseMove(const D3DXVECTOR3& rayStart, const D3DXVECTOR3& rayVec, const D3DXVECTOR3& camPos);
	DirMove OnMouseClick(const D3DXVECTOR3& rayStart, const D3DXVECTOR3& rayVec, lsl::KeyState state, const D3DXVECTOR3& camPos);
};

void FillDataPlane(res::VertexData& vb, float width, float height, float u, float v);
//0...bot = min(slices + 1, 1) - вершины нижней грани
//bot...top = bot + min(slices + 1, 1) - вершины верхней грани
void FillDataCylinder(res::MeshData& mesh, float botRadius, float topRadius, float height, unsigned slices, const D3DXCOLOR& color);

}

}

#endif