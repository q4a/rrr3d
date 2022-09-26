#pragma once

#include "ITrace.h"

#include "Utils.h"
#include "game/TraceGfx.h"

namespace r3d
{
	namespace edit
	{
		class Edit;

		class WayPoint : public IWayPoint, public ExternImpl<game::WayPoint>
		{
		protected:
			VirtImpl* GetImpl() override { return this; }
		public:
			WayPoint(Inst* inst);

			bool IsUsedByPath() const override;

			unsigned GetId() const override;

			float GetSize() const override;
			void SetSize(float value) override;

			game::Trace::Points::const_iterator traceIter;
		};

		class WayNode : public IWayNode, public ExternImpl<game::WayNode>
		{
		protected:
			VirtImpl* GetImpl() override { return this; }
		public:
			WayNode(Inst* inst);

			IWayPathRef GetPath() override;
		};

		class WayPath : public IWayPath, public ExternImpl<game::WayPath>
		{
		protected:
			VirtImpl* GetImpl() override { return this; }
		public:
			WayPath(Inst* inst);

			void Delete(IWayNodeRef& value) override;
			void Clear() override;

			IWayNodeRef First() override;
			void Next(IWayNodeRef& ref) override;

			game::Trace::Pathes::const_iterator traceIter;
		};

		class Trace : public ITrace, public ExternImpl<game::Trace>
		{
		private:
			class NodeControl : public IScNodeCont
			{
			private:
				Trace* _trace;
				game::WayPoint* _wayPoint;
				ControlEventRef _event;

				game::TraceGfx::PointLink* _link;
				D3DXVECTOR3 _dragRayPos;
				D3DXVECTOR3 _dragRayVec;

				void Reset(game::WayPoint* wayPoint);

				void NewLink(const D3DXVECTOR3& scrRayPos, const D3DXVECTOR3& scrRayVec);
				void FreeLink();
				bool CreateWay(game::WayNode* curNode, game::WayPoint* point, game::WayNode* node) const;
			public:
				NodeControl(Trace* trace, game::WayPoint* wayPoint, const ControlEventRef& mEvent);
				~NodeControl() override;

				void Select(bool active) override;
				bool RayCastInters(const D3DXVECTOR3& rayPos, const D3DXVECTOR3& rayVec) const override;
				bool Compare(const IMapObjRef& node) const override;

				void OnStartDrag(const D3DXVECTOR3& scrRayPos, const D3DXVECTOR3& scrRayVec) override;
				void OnEndDrag(const D3DXVECTOR3& scrRayPos, const D3DXVECTOR3& scrRayVec) override;
				void OnDrag(const D3DXVECTOR3& pos, const D3DXVECTOR3& scrRayPos,
				            const D3DXVECTOR3& scrRayVec) override;
				//
				void OnShiftAction(const D3DXVECTOR3& scrRayPos, const D3DXVECTOR3& scrRayVec) override;

				D3DXVECTOR3 GetPos() const override;
				void SetPos(const D3DXVECTOR3& value) override;
				//
				D3DXVECTOR3 GetLastPos() const override;
				void SetLastPos(const D3DXVECTOR3& value) override;
				//
				D3DXQUATERNION GetRot() const override;
				void SetRot(const D3DXQUATERNION& value) override;
				//
				D3DXQUATERNION GetLastRot() const override;
				void SetLastRot(const D3DXQUATERNION& value) override;
				//
				D3DXVECTOR3 GetScale() const override;
				void SetScale(const D3DXVECTOR3& value) override;
				//
				D3DXVECTOR3 GetLastScale() const override;
				void SetLastScale(const D3DXVECTOR3& value) override;
				//
				D3DXVECTOR3 GetDir() const override;
				D3DXVECTOR3 GetRight() const override;
				D3DXVECTOR3 GetUp() const override;

				D3DXMATRIX GetMat() const override;
				AABB GetAABB() const override;
			};

		private:
			Edit* _edit;
			game::TraceGfx* _traceGfx;

			IWayPathRef _selPath;
			IWayNodeRef _selNode;
			bool _enableVisualize;

			game::WayPoint* PickInstPoint(const D3DXVECTOR3& rayPos, const D3DXVECTOR3& rayVec);
		protected:
			VirtImpl* GetImpl() override { return this; }
		public:
			Trace(Inst* inst, Edit* edit);
			~Trace() override;

			IWayPointRef PickPoint(const Point& scrCoord) override;

			IWayPointRef AddPoint() override;
			void DelPoint(IWayPointRef& value) override;
			void ClearPoints() override;
			IScNodeContRef GetPointControl(const IWayPointRef& ref, const ControlEventRef& mEvent) override;

			IWayPathRef AddPath() override;
			void DelPath(IWayPathRef& value) override;
			void ClearPathes() override;
			IWayPathRef GetSelPath() override;
			void SelectPath(const IWayPathRef& value) override;
			//
			IWayNodeRef GetSelNode() override;
			void SelectNode(const IWayNodeRef& value) override;

			void Clear() override;

			IWayPointRef FirstPoint() override;
			void NextPoint(IWayPointRef& ref) override;

			IWayPathRef FirstPath() override;
			void NextPath(IWayPathRef& ref) override;

			bool GetEnableVisualize() const override;
			void EnableVisualize(bool value) override;
		};
	}
}
