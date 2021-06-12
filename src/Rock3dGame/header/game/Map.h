#ifndef GAME_MAP
#define GAME_MAP

#include "MapObj.h"
#include "Trace.h"

namespace r3d
{

namespace game
{

class Map: public lsl::Component
{
private:
	typedef Component _MyBase;
public:	
	typedef std::map<unsigned, MapObj*> Objects;

	static const unsigned cDefMapObjId = 0;

	class MapObjList: public MapObjects
	{
		friend Map;
	private:
		typedef MapObjects _MyBase;
	private:
		Map* _owner;
	protected:
		virtual void InsertItem(const Value& value);
		virtual void RemoveItem(const Value& value);
	public:
		MapObjList(Map* owner);
	};
private:
	World* _world;
	Objects _objects;
	MapObjList* _mapObjList[MapObjLib::cCategoryEnd];	
	MapObj* _ground;
	game::Trace* _trace;

	unsigned _lastId;
protected:
	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
public:
	Map(World* world);
	virtual ~Map();

	void InsertMapObj(MapObj* value);

	MapObj& AddMapObj(MapObjRec* record);
	MapObj& AddMapObj(MapObj* ref);
	MapObj& AddMapObj(MapObjLib::Category category, MapObj::Type type);
	template<class _Type> MapObj& AddMapObj(MapObjLib::Category category);
	void DelMapObj(MapObj* value);
	void Clear();
	int GetMapObjCount(MapObjRec* record);

	const Objects& GetObjects() const;
	MapObjList& GetMapObjList(MapObjLib::Category type);
	MapObj* GetMapObj(unsigned id, bool includeDead = false);
	MapObj* GetSemaphore();

	World* GetWorld();
	game::Trace& GetTrace();
};




template<class _Type> MapObj& Map::AddMapObj(MapObjLib::Category category)
{
	return _mapObjList[category]->Add<_Type>();
}

}

}

#endif