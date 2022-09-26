#pragma once

namespace r3d
{
	namespace game
	{
		class WayNode;
		class Trace;
		class WayPath;

		class WayPoint : public Object
		{
			friend WayNode;
		public:
			using Points = List<WayPoint*>;
			using Nodes = List<WayNode*>;
		private:
			Trace* _trace;
			unsigned _id;

			D3DXVECTOR3 _pos;
			float _size;
			D3DXVECTOR3 _off;

			Nodes _nodes;

			void InsertNode(WayNode* node);
			void RemoveNode(WayNode* node);

			void Changed() const;
		public:
			WayPoint(Trace* trace, unsigned id);
			~WayPoint() override;

			bool RayCast(const D3DXVECTOR3& rayPos, const D3DXVECTOR3& rayVec, float* dist = nullptr) const;
			bool IsContains(const D3DXVECTOR3& point, float* dist = nullptr) const;

			unsigned GetId() const;

			//�������, � ������� �����������
			const D3DXVECTOR3& GetPos() const;
			void SetPos(const D3DXVECTOR3& value);
			//������, � ���� �������� �����
			float GetSize() const;
			void SetSize(float value);
			//����������� �����, � ���� �������� ������������ �������
			const D3DXVECTOR3& GetOff() const;
			void SetOff(const D3DXVECTOR3& value);

			//����������� �� ����� ����
			bool IsFind(WayPath* path) const;
			//
			bool IsFind(WayNode* node, WayNode* ignore = nullptr) const;
			//�������� ��������� ���� �� ����������� � ignore
			WayNode* GetRandomNode(WayNode* ignore, bool hasNext) const;
			//
			const Nodes& GetNodes() const;
		};

		//����, ������������ ������� ��������� ������������ ��������������� ����. ���� �������� ����� � ����� ��������� �� ���
		class WayNode : public Object
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
				mutable D3DXVECTOR2 _dir;
				//���������� ����� �����
				mutable float _dirLength{};
				//������� � ������������� �������
				mutable D3DXVECTOR2 _norm;
				//
				mutable D3DXVECTOR3 _normLine;
				//������������ �����
				mutable D3DXVECTOR3 _dirLine;
				//������� ������������ ������
				mutable D3DXVECTOR2 _midDir;
				//������� � _midDir
				mutable D3DXVECTOR2 _midNorm;
				//���������� �������������� ����� ����� node
				mutable D3DXVECTOR3 _midNormLine;
				//������� � _midDir � ����������� � ����������� ���� (���� ������ � ������� ����)
				mutable D3DXVECTOR2 _edgeNorm;
				//������� ������ ���������� ������� ����������� ����
				mutable float _nodeRadius{};
				//����� ����� ���������� ���� �������� ��������������� _midNorm
				mutable D3DXVECTOR3 _edgeLine;
				//������ ���� �������� ���� (���� ������� ������), � ���
				mutable float _turnAngle{};
				//��������� �� ��������� ����
				mutable float _finishDist{};
				//��������� �� ���������� ����
				mutable float _startDist{};
				//���� ���������
				mutable bool _changed;

				//��������� ���������
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

				void Changed() const;
				//���� upVec ������, �� � 3� ����������� ����� � 2� �����������
				void GetVBuf(D3DXVECTOR3* vBuf, unsigned length, const D3DXVECTOR3* upVec) const;
				//������ � ���������
				//����� ������� ������������� ������� � 0 �� ������� ����� � ����������� ������� GetNorm
				//����� �� �������
				unsigned ComputeTrackInd(const D3DXVECTOR2& point) const;
				//������������� ������ �������� �� �������� ������� track ������������ point
				D3DXVECTOR2 ComputeTrackNormOff(const D3DXVECTOR2& point, unsigned track) const;

				bool RayCast(const D3DXVECTOR3& rayPos, const D3DXVECTOR3& rayVec, float* dist = nullptr) const;
				//lengthClamp - ������������ �� ���� �� �����
				bool IsContains(const D3DXVECTOR3& point, bool lengthClamp = true, float* dist = nullptr,
				                float widthErr = 0.0f) const;
				bool IsZLevelContains(const D3DXVECTOR3& point, float* dist = nullptr) const;

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

				//coordX - ������������� ���������� ���������� �����
				float ComputeCoordX(float dist) const;
				float ComputeCoordX(const D3DXVECTOR2& point) const;
				//����� �����
				float ComputeLength(float coordX) const;
				//����� ������� � ������������ �����
				float ComputeWidth(float coordX) const;
				//������ ����� � ������������ �����, coordX = [0..1]
				float ComputeHeight(float coordX) const;
				//Z ���������� ����� � ����� coordX
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
			~WayNode() override;

			void SetPrev(WayNode* node);
			void SetNext(WayNode* node);
		public:
			void Changed() const;

			bool RayCast(const D3DXVECTOR3& rayPos, const D3DXVECTOR3& rayVec, float* dist = nullptr) const;
			bool IsContains2(const D3DXVECTOR2& point, float* dist = nullptr) const;
			bool IsContains(const D3DXVECTOR3& point, float* dist = nullptr) const;

			WayPath* GetPath() const;
			WayPoint* GetPoint() const;
			//����, ������� ���������� � ������� ����. ������������ ����� 2� ���������
			const Tile& GetTile() const;

			WayNode* GetPrev() const;
			WayNode* GetNext() const;

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
			bool _enclosed{};

			WayNode* _first;
			WayNode* _last;
			unsigned _count;

			WayPath(Trace* trace);
			~WayPath();
		public:
			//mWhere - ����� ����� mWhere, == 0 � �����
			WayNode* Add(WayPoint* point, WayNode* mWhere = nullptr);
			void Delete(WayNode* value);
			void Clear();

			bool IsEnclosed() const;
			void Enclosed(bool value);

			WayNode* RayCast(const D3DXVECTOR3& rayPos, const D3DXVECTOR3& rayVec, WayNode* mWhere = nullptr,
			                 float* dist = nullptr) const;
			//����� ����-����� ����������� point. ��� ������ ������������ �������� ����� � ������ ���������� ����
			WayNode* IsTileContains(const D3DXVECTOR3& point, WayNode* mWhere = nullptr) const;
			//���� upVec ������, �� � 3� ����������� ����� � 2� �����������
			void GetTriStripVBuf(res::VertexData& data, const D3DXVECTOR3* upVec) const;

			Trace* GetTrace() const;
			WayNode* GetFirst() const;
			WayNode* GetLast() const;
			unsigned GetCount() const;

			float GetLength() const;
		};

		class Trace : public Component
		{
			using _MyBase = Component;
		public:
			using Points = List<WayPoint*>;
			using Pathes = List<WayPath*>;
		private:
			Points _points;
			Pathes _pathes;

			unsigned _pointId;

			WayPoint* AddPoint(unsigned id);
		protected:
			void Save(SWriter* writer) override;
			void Load(SReader* reader) override;
		public:
			Trace(unsigned tracksCnt);
			~Trace() override;

			WayPoint* AddPoint();
			void DelPoint(WayPoint* value);
			void ClearPoints();

			WayPath* AddPath();
			void DelPath(WayPath* value);
			void ClearPathes();

			void Clear();
			WayNode* RayCast(const D3DXVECTOR3& rayPos, const D3DXVECTOR3& rayVec, float* dist = nullptr) const;
			WayNode* IsTileContains(const D3DXVECTOR3& point, WayNode* mWhere = nullptr) const;
			WayPoint* FindPoint(unsigned id) const;
			WayNode* FindClosestNode(const D3DXVECTOR3& point) const;

			const Points& GetPoints() const;
			const Pathes& GetPathes() const;

			const unsigned cTrackCnt;
		};
	}
}
