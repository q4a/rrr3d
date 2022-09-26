#pragma once

#include "IMap.h"
#include "Utils.h"

namespace r3d
{
	namespace edit
	{
		class Edit;

		class MapObj : public IMapObj, public ExternImpl<game::MapObj>
		{
		protected:
			VirtImpl* GetImpl() override { return this; }
		public:
			MapObj(Inst* inst);

			const std::string& GetName() const override;
			IMapObjRecRef MapObj::GetRecord() override;

			D3DXVECTOR3 GetPos() const override;
			void SetPos(const D3DXVECTOR3& value) override;

			D3DXVECTOR3 GetScale() const override;
			void SetScale(const D3DXVECTOR3& value) override;

			D3DXQUATERNION GetRot() const override;
			void SetRot(const D3DXQUATERNION& value) override;

			game::Map::MapObjList::const_iterator mapIter;
		};

		class Map : public IMap
		{
			friend Edit;
		private:
			static const D3DXCOLOR bbColor;
			static const D3DXCOLOR selColor;

			using MapObjList = List<game::MapObj*>;

			class NodeControl : public ScNodeCont
			{
				using _MyBase = ScNodeCont;
			private:
				Map* _map;
				game::MapObj* _mapObj;
				ControlEventRef _event;
			public:
				NodeControl(game::MapObj* mapObj, Map* map, const ControlEventRef& mEvent);
				~NodeControl() override;

				void Select(bool active) override;
				void OnShiftAction(const D3DXVECTOR3& scrRayPos, const D3DXVECTOR3& scrRayVec) override;
			};

		public:
			using Inst = game::Map;
		private:
			Edit* _edit;
			ITrace* _trace;
			bool _showBB;

			game::MapObj* PickInstMapObj(const D3DXVECTOR3& rayPos, const D3DXVECTOR3& rayVec);

			void ApplyShowBB();
			void OnUpdateLevel();

			Inst* GetInst() const;
		public:
			Map(Edit* edit);
			~Map() override;

			IMapObjRef AddMapObj(const IMapObjRecRef& record) override;
			void DelMapObj(IMapObjRef& ref) override;
			void ClearMap() override;
			IMapObjRef PickMapObj(const Point& coord) override;
			IScNodeContRef GetMapObjControl(const IMapObjRef& ref, const ControlEventRef& mEvent) override;

			std::string GetCatName(unsigned i) const override;
			unsigned GetCatCount() override;
			//
			IMapObjRef GetFirst(unsigned cat) override;
			void GetNext(unsigned cat, IMapObjRef& ref) override;

			bool GetShowBBox() const override;
			void SetShowBBox(bool value) override;

			ITrace* GetTrace() override;
		};
	}
}
