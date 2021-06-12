#ifndef GRASS_FIELD
#define GRASS_FIELD

#include "SceneManager.h"
#include "graph\\MaterialLibrary.h"

namespace r3d
{

namespace graph
{

class GrassField: public BaseSceneNode
{
private:
	struct _MeshBatch
	{
		unsigned vertOffs;
		unsigned primCnt;
	};
public:
	struct GrassTile
	{
		GrassTile(float mWeight, D3DXVECTOR4 mTexCoord);

		float weight;
		D3DXVECTOR4 texCoord;
	};

	struct GrassDesc
	{
		typedef std::vector<GrassTile> Tiles;

		GrassDesc(graph::LibMaterial* mLibMat, const GrassTile& tile = GrassTile(1, D3DXVECTOR4(0, 0, 1, 1)));

		graph::LibMaterial* libMat;
		Tiles tiles;
	};
	typedef std::vector<GrassDesc> GrassList;

	struct Field
	{
		D3DXVECTOR3 pos;
	};
	typedef lsl::Vector<Field> FieldList;

	static const int cMaxBufSize = 4194304;
private:
	float _width;
	float _height;
	float _density;
	float _disp;
	GrassList _grassList;

	VBMesh _mesh;
	std::vector<_MeshBatch> _batchList;
	float _fieldWidth;
	float _fieldHeight;
	FieldList _fieldList;
	
	void BuildField();
	void DrawField(graph::Engine& engine, const Field& field);
	void Rebuild();
protected:
	virtual AABB LocalDimensions() const;
	virtual void DoRender(graph::Engine& engine);
public:
	GrassField();
	virtual ~GrassField();

	//относительно начала координат
	//ширина поля
	float GetWidth() const;
	void SetWidth(float value);
	//высота поля
	float GetHeight() const;
	void SetHeight(float value);
	//общая плотность
	float GetDensity();
	void SetDensity(float value);
	//случайное рассеивание, в м
	float GetDisp();
	void SetDisp(float value);

	//набор растений
	const GrassList& GetGrassList();
	void SetGrassList(const GrassList& value);

	Shader shader;
};

}

}

#endif