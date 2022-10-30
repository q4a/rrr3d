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

	glm::vec3 _pos;
	float _size;
	glm::vec3 _off;

	Nodes _nodes;

	void InsertNode(WayNode* node);
	void RemoveNode(WayNode* node);

	void Changed();
public:
	WayPoint(Trace* trace, unsigned id);
	virtual ~WayPoint();

	bool RayCast(const glm::vec3& rayPos, const glm::vec3& rayVec, float* dist = 0) const;
	bool IsContains(const glm::vec3& point, float* dist = 0) const;

	unsigned GetId() const;

	//�������, � ������� �����������
	const glm::vec3& GetPos() const;
	void SetPos(const glm::vec3& value);
	//������, � ���� �������� �����
	float GetSize() const;
	void SetSize(float value);
	//����������� �����, � ���� �������� ������������ �������
	const glm::vec3& GetOff() const;
	void SetOff(const glm::vec3& value);

	//����������� �� ����� ����
	bool IsFind(WayPath* path) const;
	//
	bool IsFind(WayNode* node, WayNode* ignore = NULL) const;
	//�������� ��������� ���� �� ����������� � ignore
	WayNode* GetRandomNode(WayNode* ignore, bool hasNext);
	//
	const Nodes& GetNodes() const;
};

//����, ������������ ������� ��������� ������������ ��������������� ����. ���� �������� ����� � ����� ��������� �� ���
class WayNode: public Object
{
	friend class WayPath;
private:
	//����, ���������� � ���� _node
	//������������ ����� 3� �������, ������������ ����� ������ _node � _node->_next. ��� ��������� �������� ��� ��������� ������������ 2� �������
	class Tile
	{
	private:
		WayNode* _node;

		//������������ ������, ������������ node, nextNode
		mutable glm::vec2 _dir;
		//���������� ����� �����
		mutable float _dirLength;
		//������� � ������������� �������
		mutable glm::vec2 _norm;
		//
		mutable glm::vec3 _normLine;
		//������������ �����
		mutable glm::vec3 _dirLine;
		//������� ������������ ������
		mutable glm::vec2 _midDir;
		//������� � _midDir
		mutable glm::vec2 _midNorm;
		//���������� �������������� ����� ����� node
		mutable glm::vec3 _midNormLine;
		//������� � _midDir � ����������� � ����������� ���� (���� ������ � ������� ����)
		mutable glm::vec2 _edgeNorm;
		//������� ������ ���������� ������� ����������� ����
		mutable float _nodeRadius;
		//����� ����� ���������� ���� �������� ��������������� _midNorm
		mutable glm::vec3 _edgeLine;
		//������ ���� �������� ���� (���� ������� ������), � ���
		mutable float _turnAngle;
		//��������� �� ��������� ����
		mutable float _finishDist;
		//��������� �� ���������� ����
		mutable float _startDist;
		//���� ���������
		mutable bool _changed;

		//��������� ���������
		void ApplyChanges() const;

		const glm::vec3& GetPos() const;
		float GetHeight() const;
		//
		const glm::vec3& GetNextPos() const;
		float GetNextHeight() const;
		const glm::vec2& GetPrevDir() const;
		const glm::vec2& GetNextMidNorm() const;
		const glm::vec3& GetNextNormLine() const;
		float GetNextNodeRadius() const;

		glm::vec4 GetWayPlane() const;
	public:
		Tile(WayNode* node);

		void Changed();
		//���� upVec ������, �� � 3� ����������� ����� � 2� �����������
		void GetVBuf(glm::vec3* vBuf, unsigned length, const glm::vec3* upVec) const;
		//������ � ���������
		//����� ������� ������������� ������� � 0 �� ������� ����� � ����������� ������� GetNorm
		//����� �� �������
		unsigned ComputeTrackInd(const glm::vec2& point) const;
		//������������� ������ �������� �� �������� ������� track ������������ point
		glm::vec2 ComputeTrackNormOff(const glm::vec2& point, unsigned track) const;

		bool RayCast(const glm::vec3& rayPos, const glm::vec3& rayVec, float* dist = 0) const;
		//lengthClamp - ������������ �� ���� �� �����
		bool IsContains(const glm::vec3& point, bool lengthClamp = true, float* dist = 0, float widthErr = 0.0f) const;
		bool IsZLevelContains(const glm::vec3& point, float* dist = 0) const;

		const glm::vec2& GetDir() const;
		float GetDirLength() const;
		const glm::vec2& GetNorm() const;
		const glm::vec3& GetDirLine() const;
		const glm::vec2& GetMidDir() const;
		const glm::vec2& GetMidNorm() const;
		const glm::vec3& GetMidNormLine() const;
		const glm::vec2& GetEdgeNorm() const;
		float GetNodeRadius() const;
		const glm::vec3& GetEdgeLine() const;
		float GetTurnAngle() const;
		float GetFinishDist() const;
		float GetStartDist() const;

		//coordX - ������������� ���������� ���������� �����
		float ComputeCoordX(float dist) const;
		float ComputeCoordX(const glm::vec2& point) const;
		//����� �����
		float ComputeLength(float coordX) const;
		//����� ������� � ������������ �����
		float ComputeWidth(float coordX) const;
		//������ ����� � ������������ �����, coordX = [0..1]
		float ComputeHeight(float coordX) const;
		//Z ���������� ����� � ����� coordX
		float ComputeZCoord(float coordX) const;
		//
		glm::vec3 GetPoint(float coordX) const;
		//
		float GetLength(const glm::vec2& point) const;
		float GetHeight(const glm::vec2& point) const;
		float GetWidth(const glm::vec2& point) const;
		float GetZCoord(const glm::vec2& point) const;
		glm::vec3 GetCenter3() const;

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

	bool RayCast(const glm::vec3& rayPos, const glm::vec3& rayVec, float* dist = 0) const;
	bool IsContains2(const glm::vec2& point, float* dist = 0) const;
	bool IsContains(const glm::vec3& point, float* dist = 0) const;

	WayPath* GetPath();
	WayPoint* GetPoint();
	//����, ������� ���������� � ������� ����. ������������ ����� 2� ���������
	const Tile& GetTile() const;

	WayNode* GetPrev();
	WayNode* GetNext();

	const glm::vec3& GetPos() const;
	glm::vec2 GetPos2() const;
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
	//mWhere - ����� ����� mWhere, == 0 � �����
	WayNode* Add(WayPoint* point, WayNode* mWhere = 0);
	void Delete(WayNode* value);
	void Clear();

	bool IsEnclosed() const;
	void Enclosed(bool value);

	WayNode* RayCast(const glm::vec3& rayPos, const glm::vec3& rayVec, WayNode* mWhere = 0, float* dist = 0) const;
	//����� ����-����� ����������� point. ��� ������ ������������ �������� ����� � ������ ���������� ����
	WayNode* IsTileContains(const glm::vec3& point, WayNode* mWhere = 0) const;
	//���� upVec ������, �� � 3� ����������� ����� � 2� �����������
	void GetTriStripVBuf(res::VertexData& data, const glm::vec3* upVec);

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
	WayNode* RayCast(const glm::vec3& rayPos, const glm::vec3& rayVec, float* dist = 0) const;
	WayNode* IsTileContains(const glm::vec3& point, WayNode* mWhere = 0) const;
	WayPoint* FindPoint(unsigned id);
	WayNode* FindClosestNode(const glm::vec3& point);

	const Points& GetPoints() const;
	const Pathes& GetPathes() const;

	const unsigned cTrackCnt;
};

}

}