#ifndef R3D_GAME_LOGICMANAGER
#define R3D_GAME_LOGICMANAGER

#include "GameObject.h"
#include "Map.h"
#include "Player.h"
#include "snd/Audio.h"

namespace r3d
{
	namespace game
	{
		class Map;
		class Race;
		class NetGame;
		class Logic;

		class LogicBehavior : public Object, public Serializable, protected IProgressEvent
		{
			friend class LogicBehaviors;
		private:
			LogicBehaviors* _owner;
		protected:
			void RegProgressEvent();
			void UnregProgressEvent();

			virtual void OnContact(const px::Scene::OnContactEvent& contact1, const px::Scene::OnContactEvent& contact2)
			{
			}

			void OnProgress(float deltaTime) override
			{
			}

			void Save(SWriter* writer) override
			{
			}

			void Load(SReader* reader) override
			{
			}

		public:
			LogicBehavior(LogicBehaviors* owner);

			LogicBehaviors* GetOwner() const;
			Logic* GetLogic() const;
			Map* GetMap() const;
		};

		class LogicEventEffect : public LogicBehavior
		{
			using _MyBase = LogicBehavior;
		private:
			class GameObjEvent : public GameObjListener
			{
			private:
				LogicEventEffect* _effect;
			public:
				GameObjEvent(LogicEventEffect* effect);

				void OnDestroy(GameObject* sender) override;
			};

			using EffObjList = List<MapObj*>;
		protected:
			struct EffectDesc
			{
				EffectDesc(): pos(NullVector)
				{
				}

				D3DXVECTOR3 pos;
			};

		private:
			GameObjEvent* _gameObjEvent;
			MapObjRec* _effect;
			EffObjList _effObjList;

			D3DXVECTOR3 _pos;

			void InsertEffObj(MapObj* mapObj);
			void RemoveEffObj(MapObj* mapObj);
			void ClearEffObjList();

			void DestroyEffObj(MapObj* mapObj) const;
		protected:
			virtual MapObj* CreateEffect(const EffectDesc& desc);
			MapObj* CreateEffect();
			virtual void DeleteEffect(MapObj* mapObj);

			virtual void OnDestroyEffect(MapObj* sender)
			{
			}

			void Save(SWriter* writer) override;
			void Load(SReader* reader) override;
		public:
			LogicEventEffect(LogicBehaviors* owner);
			~LogicEventEffect() override;

			MapObjRec* GetEffect() const;
			void SetEffect(MapObjRec* value);

			const D3DXVECTOR3& GetPos() const;
			void SetPos(const D3DXVECTOR3& value);
		};

		class PairPxContactEffect : public LogicEventEffect
		{
			using _MyBase = LogicEventEffect;
		private:
			struct Key
			{
				Key(px::Actor* mActor1, px::Actor* mActor2): actor1(mActor1), actor2(mActor2)
				{
				}

				px::Actor* actor1;
				px::Actor* actor2;

				bool operator<(const Key& key) const
				{
					//альтернатива, некорректна
					//return (unsigned)actor1 + (unsigned)actor2 < (unsigned)key.actor1 + (unsigned)key.actor2;

					//с учетом перемены мест слагаемое не должно изменяться
					return actor1 == key.actor1 ? actor2 < key.actor2 : actor1 < key.actor1;
				}
			};

			struct Contact
			{
				Contact(): shape1(nullptr), shape2(nullptr), point(NullVector), effect(nullptr), time(0)
				{
				}

				bool operator==(const Contact& contact) const
				{
					return effect == contact.effect;
				}

				NxShape* shape1;
				NxShape* shape2;
				D3DXVECTOR3 point;

				MapObj* effect;
				float time;
			};

			using ContactList = List<Contact>;

			struct ContactNode
			{
			private:
				ContactNode(const ContactNode& node);
				ContactNode& operator=(const ContactNode& contact);
			public:
				ContactNode(): source(nullptr) { last = list.end(); }

				ContactList list;
				ContactList::iterator last;
				snd::Source3d* source;
			};

			using ContactMap = std::map<Key, ContactNode*>;
		public:
			using Sounds = Vector<snd::Sound*>;
		private:
			Sounds _sounds;

			ContactMap _contactMap;

			ContactMap::iterator GetOrCreateContact(const Key& key);
			void InsertContact(ContactMap::iterator iter, NxShape* shape1, NxShape* shape2, const D3DXVECTOR3& point);
			ContactMap::iterator ReleaseContact(ContactMap::iterator iter, ContactList::iterator cIter1,
			                                    ContactList::iterator cIter2, bool death, float deltaTime = 0.0f,
			                                    float cRelTime = -1.0f);
			void ReleaseContacts(bool death);

			void RemoveContactByEffect(MapObj* effect) const;
		protected:
			void OnDestroyEffect(MapObj* sender) override;
			void OnContact(const px::Scene::OnContactEvent& contact1,
			               const px::Scene::OnContactEvent& contact2) override;
			void OnProgress(float deltaTime) override;
		public:
			PairPxContactEffect(LogicBehaviors* owner);
			~PairPxContactEffect() override;

			void InsertSound(snd::Sound* value);
			Sounds::iterator RemoveSound(Sounds::const_iterator iter);
			void RemoveSound(snd::Sound* sound);
			void ClearSounds();

			const Sounds& GetSounds() const;
		};

		enum LogicBehaviorType { lbtPairPxContactEffect = 0, cLogicBehaviorTypeEnd };

		class LogicBehaviors : public Collection<LogicBehavior, LogicBehaviorType, LogicBehaviors*, LogicBehaviors*>
		{
			friend class Logic;

			using _MyBase = Collection<LogicBehavior, LogicBehaviorType, LogicBehaviors*, LogicBehaviors*>;
		public:
			using ClassList = ClassList;

			static ClassList classList;

			static void InitClassList();
		private:
			Logic* _logic;

			void OnContact(const px::Scene::OnContactEvent& contact1, const px::Scene::OnContactEvent& contact2);
		public:
			LogicBehaviors(Logic* logic);
			~LogicBehaviors() override;

			Logic* GetLogic() const;
		};

		class Map;

		class Logic : public Object, public Serializable
		{
		public:
			enum SndCategory { scMusic = 0, scEffects, scVoice, cSndCategoryEnd };

		private:
			using GameObjList = List<GameObject*>;

			class PxSceneUser : public px::SceneUser
			{
			private:
				Logic* _logic;
			protected:
				void OnContact(const px::Scene::OnContactEvent& contact1,
				               const px::Scene::OnContactEvent& contact2) override;
			public:
				PxSceneUser(Logic* logic);
			};

		private:
			World* _world;
			LogicBehaviors* _behaviors;
			PxSceneUser* _pxSceneUser;
			GameObjList _gameObjList;

			snd::SubmixVoice* _sndCat[cSndCategoryEnd]{};
			float _volume[cSndCategoryEnd]{};
			bool _mute[cSndCategoryEnd]{};
			bool _initSndCat;

			D3DXVECTOR2 _touchBorderDamage;
			D3DXVECTOR2 _touchBorderDamageForce;
			D3DXVECTOR2 _touchCarDamage;
			D3DXVECTOR2 _touchCarDamageForce;

			void InitSndCat();
			void FreeSndCat();
		protected:
			void Save(SWriter* writer) override;
			void Load(SReader* reader) override;
		public:
			Logic(World* world);
			~Logic() override;

			void RegFixedStepEvent(IFixedStepEvent* user) const;
			void UnregFixedStepEvent(IFixedStepEvent* user) const;

			void RegProgressEvent(IProgressEvent* user) const;
			void UnregProgressEvent(IProgressEvent* user) const;

			void RegLateProgressEvent(ILateProgressEvent* user) const;
			void UnregLateProgressEvent(ILateProgressEvent* user) const;

			void RegFrameEvent(IFrameEvent* user) const;
			void UnregFrameEvent(IFrameEvent* user) const;

			void RegGameObj(GameObject* gameObj);
			void CleanGameObjs();

			void Shot(Player* player, MapObj* target, Player::SlotType type);
			void Shot(Player* player, MapObj* target);
			void Damage(GameObject* sender, int senderPlayerId, GameObject* target, float value,
			            GameObject::DamageType damageType) const;
			bool TakeBonus(GameObject* sender, GameObject* bonus, GameObject::BonusType type, float value,
			               bool destroy) const;
			bool MineContact(Proj* sender, GameObject* target, const D3DXVECTOR3& point) const;

			snd::Source* CreateSndSource(SndCategory category);
			snd::Source3d* CreateSndSource3d(SndCategory category);
			void ReleaseSndSource(snd::Source* source) const;

			float GetVolume(SndCategory category);
			void SetVolume(SndCategory category, float value);
			void AutodetectVolume();
			void Mute(SndCategory category, bool value);

			const D3DXVECTOR2& GetTouchBorderDamage() const;
			void SetTouchBorderDamage(const D3DXVECTOR2& value);

			const D3DXVECTOR2& GetTouchBorderDamageForce() const;
			void SetTouchBorderDamageForce(const D3DXVECTOR2& value);

			const D3DXVECTOR2& GetTouchCarDamage() const;
			void SetTouchCarDamage(const D3DXVECTOR2& value);

			const D3DXVECTOR2& GetTouchCarDamageForce() const;
			void SetTouchCarDamageForce(const D3DXVECTOR2& value);

			void OnProgress(float deltaTime);

			bool IsNetGame() const;

			Map* GetMap() const;
			Race* GetRace() const;
			NetGame* GetNet() const;
			px::Scene* GetPxScene() const;
			snd::Engine* GetAudio() const;
			LogicBehaviors& GetBehaviors() const;
		};
	}
}

#endif
