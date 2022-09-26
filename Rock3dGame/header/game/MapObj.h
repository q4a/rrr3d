#ifndef R3D_GAME_GAMEDB
#define R3D_GAME_GAMEDB

#include "RecordLib.h"

namespace r3d
{
	namespace game
	{
		class GameObject;
		class MapObj;
		class Player;

		enum GameObjType
		{
			gotGameObj = 0,

			gotGameCar,
			gotRockCar,
			gotProj,
			gotWeapon,
			gotDestrObj,

			cGameObjTypeEnd
		};

		static const char* cGameObjTypeStr[cGameObjTypeEnd] =
		{
			"gotGameObj",
			"gotGameCar",
			"gotRockCar",
			"gotProj",
			"gotWeapon",
			"gotDestrObj"
		};

		class IMapObj
		{
		};

		class IMapObjLib
		{
		public:
			enum Category
			{
				ctCar = 0,
				ctWeapon,
				ctEffects,
				ctDecoration,
				ctTrack,
				ctBonus,
				ctTriggers,
				ctBonuz,
				ctTriggerz,
				ctWaypoint,
				cCategoryEnd
			};
		};

		static const char* IMapObjLib_cCategoryStr[IMapObjLib::cCategoryEnd] = {
			"ctCar", "ctWeapon", "ctEffects", "ctDecoration", "ctTrack", "ctBonus", "ctTriggers", "ctBonuz",
			"ctTriggerz", "ctWaypoint"
		};

		class MapObjRec : public Record
		{
			friend class MapObjLib;
		private:
			using _MyBase = Record;
		public:
			using Lib = MapObjLib;
		protected:
			MapObjRec(const Desc& desc);
		public:
			void Save(MapObj* mapObj);
			void Load(MapObj* mapObj);
			void AddProxy(MapObj* mapObj);

			MapObjLib* GetLib();
			IMapObjLib::Category GetCategory();
		};

		class MapObjLib : public RecordLib, public IMapObjLib
		{
		private:
			using _MyBase = RecordLib;
		public:
			static void SaveRecordRef(SWriter* writer, const std::string& name, MapObjRec* record);
			static MapObjRec* LoadRecordRefFrom(SReader* reader);
			static MapObjRec* LoadRecordRef(SReader* reader, const std::string& name);
		private:
			Category _category;
		protected:
			Record* CreateRecord(const Record::Desc& desc) override;
		public:
			MapObjLib(Category category, SerialNode* rootSrc);

			static void SaveToRec(MapObj* mapObj, MapObjRec* record);
			static void LoadFromRec(MapObj* mapObj, MapObjRec* record);
			static void AddProxyTo(MapObj* mapObj, MapObjRec* record);

			MapObjRec* FindRecord(const std::string& path);
			MapObjRec* GetOrCreateRecord(const std::string& name);

			Category GetCategory() const;
		};

		using MapObjRecList = RecordList<MapObjRec>;

		class MapObj : public Object
		{
			friend class MapObjects;
			friend class MapObjLib;
		public:
			using GameObj = GameObject;
			using Type = GameObjType;
			using ClassList = ClassList<Type, GameObj>;

			static ClassList classList;

			static void InitClassList();
		private:
			MapObjects* _owner;
			Player* _player;
			unsigned _id;

			GameObj* _gameObj;
			ClassList* _classList;
			bool _createClassList;
			Type _type;

			MapObjRec* _record;
			//Указывает что загрузка ведется из деск установленного в SetDesc, т.е. загружаться должна уже source часть
			bool _loadFromRecord;

			void CreateGameObj();
		protected:
			void SaveSource(SWriter* writer) const;
			void LoadSource(SReader* reader);
			void SaveProxy(SWriter* writer) const;
			void LoadProxy(SReader* reader) const;

			void Save(SWriter* writer) const;
			void Load(SReader* reader);
		public:
			MapObj(MapObjects* owner = nullptr);
			~MapObj() override;

			void SaveTo(SWriter* writer) const;
			void LoadFrom(SReader* reader);

			ClassList* GetClassList() const;
			ClassList* GetOrCreateClassList();
			void SetClassList(ClassList* value);

			MapObjects* GetOwner() const;

			Type GetType() const;
			void SetType(Type value);

			GameObj& GetGameObj();
			const GameObj& GetGameObj() const;
			GameObj& SetGameObj(Type type);

			template <class _Class>
			_Class& GetGameObj();
			template <class _Class>
			_Class& SetGameObj();

			const std::string& GetName() const;
			void SetName(const std::string& value);

			GameObj* GetParent();
			void SetParent(GameObj* value);

			MapObjRec* GetRecord() const;
			void SetRecord(MapObjRec* value);

			Player* GetPlayer() const;
			void SetPlayer(Player* player);

			unsigned GetId() const;
			void SetId(unsigned value);
		};

		class MapObjects : public Collection<MapObj, void, MapObjects*, MapObjects*>
		{
		private:
			using _MyBase = BaseCollection<MapObj, void>;
			using MapObjList = List<MapObj*>;
		private:
			Component* _root;
			GameObject* _owner;
			MapObjList _specialList;

			//заблокирвоать от операций изменяющих итераторы контейнера
			bool _lockCont;

			void LockCont();
			void UnlockCont();
			bool IsContLocked() const;

			void SpecialListChanged(const Value& value, bool remove);
		protected:
			void InsertItem(const Value& value) override;
			void RemoveItem(const Value& value) override;

			void SaveItem(SWriter* writer, iterator pItem, const std::string& aName) override;
			void LoadItem(SReader* reader) override;
		public:
			MapObjects(Component* root);
			MapObjects(GameObject* owner);

			void Death(int damageType, GameObject* target);

			void OnProgress(float deltaTime);
			void OnProgressSpecial(float deltaTime);

			MapObj& Add(GameObjType type, const std::string& baseName = "obj");
			MapObj& Add(MapObjRec* record);

			template <class _Class>
			MapObj& Add(const std::string& baseName = "obj")
			{
				MapObj::InitClassList();

				MapObj::ClassList::MyClassInst* classInst = MapObj::classList.FindByClass<_Class>();
				if (!classInst)
					throw Error("MapObject& Add(const std::string& name)");

				return Add(classInst->GetKey(), baseName);
			}

			Component* GetRoot() const;
			GameObject* GetOwner() const;
			void SetStoreOpt(bool storeSource, bool storeProxy);

			using _MyBase::LockDestr;
			using _MyBase::UnlockDestr;
		};


		template <class _Class>
		_Class& MapObj::GetGameObj()
		{
			LSL_ASSERT(_classList);

			if (!_classList->FindByClass<_Class>())
				throw Error("MapObj::GetGameObj()");

			return lsl::StaticCast<_Class&>(GetGameObj());
		}

		template <class _Class>
		_Class& MapObj::SetGameObj()
		{
			LSL_ASSERT(_classList);

			ClassList::MyClassInst* classInst = _classList->FindByClass<_Class>();
			if (!classInst)
				throw Error("MapObject::SetGameObj()");

			return lsl::StaticCast<_Class&>(SetGameObj(classInst->GetKey()));
		}
	}
}

#endif
