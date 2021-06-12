#pragma once

namespace r3d
{

namespace game
{

class WayNode;
class Trace;
class WayPath;

class WayPoint: public Object
{
	friend WayNode;
public:
	typedef lsl::List<WayPoint*> Points;
	typedef lsl::List<WayNode*> Nodes;
private:
	Trace* _trace;
	unsigned _id;

	D3DXVECTOR3 _pos;
	float _size;
	D3DXVECTOR3 _off;

	Nodes _nodes;

	void InsertNode(WayNode* node);
	void RemoveNode(WayNode* node);

	void Changed();
public:
	WayPoint(Trace* trace, unsigned id);
	virtual ~WayPoint();

	bool RayCast(const D3DXVECTOR3& rayPos, const D3DXVECTOR3& rayVec, float* dist = 0) const;
	bool IsContains(const D3DXVECTOR3& point, float* dist = 0) const;

	unsigned GetId() const;

	//Позиция, в мировых координатах
	const D3DXVECTOR3& GetPos() const;
	void SetPos(const D3DXVECTOR3& value);
	//Размер, в виде диаметра сферы
	float GetSize() const;
	void SetSize(float value);
	//Оптимальная точка, в виде смещения относительно позиции
	const D3DXVECTOR3& GetOff() const;
	void SetOff(const D3DXVECTOR3& value);

	//Принадлжеит ли точка путю
	bool IsFind(WayPath* path) const;
	//
	bool IsFind(WayNode* node, WayNode* ignore = NULL) const;
	//Получить случайный узел не совпадающий с ignore
	WayNode* GetRandomNode(WayNode* ignore, bool hasNext);
	//
	const Nodes& GetNodes() const;
};

//Узел, представляет понятие вейпойнта формирующего двунаправленный путь. Узлы образуют тайлы и могут ссылаться на них
class WayNode: public Object
{
	friend class WayPath;
private:
	//Тайл, начинается с узла _node
	//представляет собой 3д участок, ограниченный двумя узлами _node и _node->_next. Для некоторых расчетов для упрощения используется 2д система
	class Tile
	{
	private:
		WayNode* _node;

		//Направляющий вектор, образованный node, nextNode
		mutable D3DXVECTOR2 _dir;
		//продольгая длина тайла
		mutable float _dirLength;
		//Нормаль к направляющему вектору
		mutable D3DXVECTOR2 _norm;
		//
		mutable D3DXVECTOR3 _normLine;
		//направляющая линия
		mutable D3DXVECTOR3 _dirLine;
		//Средний направляющий вектор
		mutable D3DXVECTOR2 _midDir;
		//Нормаль к _midDir
		mutable D3DXVECTOR2 _midNorm;
		//Нормальная ограничивающие линия через node
		mutable D3DXVECTOR3 _midNormLine;
		//нормаль к _midDir в направлении к внутреннему углу (наче говоря к острому углу)
		mutable D3DXVECTOR2 _edgeNorm;
		//узловой радиус окружности целиком описывающий угол
		mutable float _nodeRadius;
		//линия через внутренний угол поворота перпендикулярно _midNorm
		mutable D3DXVECTOR3 _edgeLine;
		//острый угол поворота узла (двух смежных тайлов), в рад
		mutable float _turnAngle;
		//дистанцию до конечного узла
		mutable float _finishDist;
		//дистанция до начального узла
		mutable float _startDist;
		//флаг состояния
		mutable bool _changed;

		//Применить изменения
		void ApplyChanges() const;

		const D3DXVECTOR3& GetPos() const;
		float GetHeight() const;
		//
		const D3DXVECTOR3& GetNextPos() const;
		float GetNextHeight() const;
		const D3DXVECTOR2& GetPrevDir() const;
		const D3DXVECTOR2& GetNextMidNorm() const;
		const D3DXVECTOR3& GetNextNormLine() const;
		float GetNextNodeRadius() const;

		D3DXPLANE GetWayPlane() const;		
	public:
		Tile(WayNode* node);

		void Changed();
		//если upVec указан, то в 3д координатах иначе в 2д координатах
		void GetVBuf(D3DXVECTOR3* vBuf, unsigned length, const D3DXVECTOR3* upVec) const;
		//Работа с дорожками
		//Номер дорожки отсчитывается начиная с 0 от крайней грани в направлении нормали GetNorm
		//Номер из поизции
		unsigned ComputeTrackInd(const D3DXVECTOR2& point) const;
		//Наикратчайший вектор смещения до соседней дорожки track относительно point
		D3DXVECTOR2 ComputeTrackNormOff(const D3DXVECTOR2& point, unsigned track) const;

		bool RayCast(const D3DXVECTOR3& rayPos, const D3DXVECTOR3& rayVec, float* dist = 0) const;
		//lengthClamp - ограничивать ли тайл по длине
		bool IsContains(const D3DXVECTOR3& point, bool lengthClamp = true, float* dist = 0, float widthErr = 0.0f) const;
		bool IsZLevelContains(const D3DXVECTOR3& point, float* dist = 0) const;

		const D3DXVECTOR2& GetDir() const;
		float GetDirLength() const;
		const D3DXVECTOR2& GetNorm() const;
		const D3DXVECTOR3& GetDirLine() const;
		const D3DXVECTOR2& GetMidDir() const;
		const D3DXVECTOR2& GetMidNorm() const;
		const D3DXVECTOR3& GetMidNormLine() const;
		const D3DXVECTOR2& GetEdgeNorm() const;
		float GetNodeRadius() const;
		const D3DXVECTOR3& GetEdgeLine() const;
		float GetTurnAngle() const;
		float GetFinishDist() const;
		float GetStartDist() const;

		//coordX - относительная продольная координата тайла
		float ComputeCoordX(float dist) const;
		float ComputeCoordX(const D3DXVECTOR2& point) const;
		//длина тайла
		float ComputeLength(float coordX) const;
		//Длина нормали в определенной точке
		float ComputeWidth(float coordX) const;
		//Высота тайла в определенной точке, coordX = [0..1]
		float ComputeHeight(float coordX) const;
		//Z координата тайла в точке coordX
		float ComputeZCoord(float coordX) const;
		//
		D3DXVECTOR3 GetPoint(float coordX) const;
		//
		float GetLength(const D3DXVECTOR2& point) const;
		float GetHeight(const D3DXVECTOR2& point) const;
		float GetWidth(const D3DXVECTOR2& point) const;
		float GetZCoord(const D3DXVECTOR2& point) const;
		D3DXVECTOR3 GetCenter3() const;

		const unsigned cTrackCnt;
	};
private:
	WayPath* _path;
	WayPoint* _point;
	Tile* _tile;

	WayNode* _prev;
	WayNode* _next;

	WayNode(WayPath* path, WayPoint* point);
	~WayNode();

	void SetPrev(WayNode* node);
	void SetNext(WayNode* node);
public:
	void Changed();

	bool RayCast(const D3DXVECTOR3& rayPos, const D3DXVECTOR3& rayVec, float* dist = 0) const;
	bool IsContains2(const D3DXVECTOR2& point, float* dist = 0) const;
	bool IsContains(const D3DXVECTOR3& point, float* dist = 0) const;

	WayPath* GetPath();
	WayPoint* GetPoint();
	//трек, который начинается с данного узла. Представляет собой 2д плоскость
	const Tile& GetTile() const;

	WayNode* GetPrev();
	WayNode* GetNext();

	const D3DXVECTOR3& GetPos() const;
	D3DXVECTOR2 GetPos2() const;
	float GetSize() const;
	float GetRadius() const;
};

class WayPath
{
	friend WayNode;
	friend class Trace;
private:
	Trace* _trace;
	bool _enclosed;

	WayNode* _first;
	WayNode* _last;
	unsigned _count;

	WayPath(Trace* trace);
	~WayPath();
public:
	//mWhere - перед узлом mWhere, == 0 в конец
	WayNode* Add(WayPoint* point, WayNode* mWhere = 0);
	void Delete(WayNode* value);
	void Clear();

	bool IsEnclosed() const;
	void Enclosed(bool value);

	WayNode* RayCast(const D3DXVECTOR3& rayPos, const D3DXVECTOR3& rayVec, WayNode* mWhere = 0, float* dist = 0) const;
	//Поиск узла-тайла содержащего point. При поиске предпочтение отдается узлам с кторых начинается тайл
	WayNode* IsTileContains(const D3DXVECTOR3& point, WayNode* mWhere = 0) const;
	//если upVec указан, то в 3д координатах иначе в 2д координатах
	void GetTriStripVBuf(res::VertexData& data, const D3DXVECTOR3* upVec);

	Trace* GetTrace();
	WayNode* GetFirst();
	WayNode* GetLast();
	unsigned GetCount() const;

	float GetLength() const;
};

class Trace: public Component
{
	typedef Component _MyBase;
public:
	typedef lsl::List<WayPoint*> Points;
	typedef lsl::List<WayPath*> Pathes;
private:
	Points _points;
	Pathes _pathes;

	unsigned _pointId;

	WayPoint* AddPoint(unsigned id);
protected:
	virtual void Save(SWriter* writer);
	virtual void Load(SReader* reader);
public:
	Trace(unsigned tracksCnt);
	virtual ~Trace();

	WayPoint* AddPoint();
	void DelPoint(WayPoint* value);
	void ClearPoints();

	WayPath* AddPath();
	void DelPath(WayPath* value);
	void ClearPathes();

	void Clear();
	WayNode* RayCast(const D3DXVECTOR3& rayPos, const D3DXVECTOR3& rayVec, float* dist = 0) const;
	WayNode* IsTileContains(const D3DXVECTOR3& point, WayNode* mWhere = 0) const;
	WayPoint* FindPoint(unsigned id);
	WayNode* FindClosestNode(const D3DXVECTOR3& point);

	const Points& GetPoints() const;
	const Pathes& GetPathes() const;

	const unsigned cTrackCnt;
};

}

}