#ifndef GAME_MAP
#define GAME_MAP

#include "MapObj.h"
#include "Trace.h"

namespace r3d
{
	namespace game
	{
		class Map : public Component
		{
		private:
			using _MyBase = Component;
		public:
			using Objects = std::map<unsigned, MapObj*>;

			static const unsigned cDefMapObjId = 0;

			class MapObjList : public MapObjects
			{
				friend Map;
			private:
				using _MyBase = MapObjects;
			private:
				Map* _owner;
			protected:
				void InsertItem(const Value& value) override;
				void RemoveItem(const Value& value) override;
			public:
				MapObjList(Map* owner);
			};

		private:
			World* _world;
			Objects _objects;
			MapObjList* _mapObjList[MapObjLib::cCategoryEnd];
			MapObj* _ground;
			Trace* _trace;

			unsigned _lastId;
		protected:
			void Save(SWriter* writer) override;
			void Load(SReader* reader) override;
		public:
			Map(World* world);
			~Map() override;

			void InsertMapObj(MapObj* value);

			MapObj& AddMapObj(MapObjRec* record);
			MapObj& AddMapObj(MapObj* ref);
			MapObj& AddMapObj(MapObjLib::Category category, MapObj::Type type);
			template <class _Type>
			MapObj& AddMapObj(MapObjLib::Category category);
			void DelMapObj(MapObj* value);
			void Clear();
			int GetMapObjCount(MapObjRec* record);

			const Objects& GetObjects() const;
			MapObjList& GetMapObjList(MapObjLib::Category type);
			MapObj* GetMapObj(unsigned id, bool includeDead = false);
			MapObj* GetSemaphore();

			World* GetWorld();
			Trace& GetTrace();
		};


		template <class _Type>
		MapObj& Map::AddMapObj(MapObjLib::Category category)
		{
			return _mapObjList[category]->Add<_Type>();
		}
	}
}

#endif
