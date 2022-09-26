#pragma once

namespace r3d
{
	namespace game
	{
		class Menu;
		class HudMenu;

		class PlayerStateFrame : public MenuFrame, public IGameUser
		{
		private:
			struct WeaponBox
			{
				WeaponBox(): box(nullptr), view(nullptr), slot(nullptr), mesh(nullptr), label(nullptr)
				{
				}

				gui::PlaneFon* box;
				gui::ViewPort3d* view;
				Slot* slot;
				gui::Mesh3d* mesh;
				gui::Label* label;
			};

			struct Opponent
			{
				Player* player;

				gui::Widget* dummy;
				gui::PlaneFon* point;
				gui::Label* label;

				D3DXVECTOR2 center;
				float radius;
			};

			using Opponents = List<Opponent>;

			struct PickItem
			{
				gui::PlaneFon* image;
				gui::PlaneFon* photo;
				gui::Label* label;

				float time;
				D3DXVECTOR2 pos;
			};

			using PickItems = List<PickItem>;

			struct AchievmentItem
			{
				gui::PlaneFon* image;
				gui::PlaneFon* points;

				float time;
				int lastIndex;
				float indexTime;
				D3DXVECTOR2 slotSize;
			};

			using AchievmentItems = List<AchievmentItem>;

			enum WeaponType { wtHyper = 0, wtMine, wtWeapon1, wtWeapon2, wtWeapon3, wtWeapon4, cWeaponTypeEnd };

			struct CarLife
			{
				gui::PlaneFon* back;
				gui::ProgressBar* bar;
				Player* target;
				float timer;
				float timeMax;
			};

			enum CarLifeE { clOpponent = 0, clHuman, cCarLifeEnd };

		private:
			Menu* _menu{};
			HudMenu* _hudMenu;

			gui::PlaneFon* _raceState;
			gui::PlaneFon* _classicLifePanel;
			gui::PlaneFon* _bankBgr;
			WeaponBox _weaponBox[cWeaponTypeEnd];
			gui::Label* _place;
			gui::Label* _laps;
			gui::Label* _bank;
			gui::Label* _hypercount;
			gui::Label* _minescount;
			gui::Label* _weaponscount;
			gui::ProgressBar* _lifeBar;
			gui::ProgressBar* _hyperBar;
			gui::ProgressBar* _gun1Bar;
			gui::ProgressBar* _gun2Bar;
			gui::ProgressBar* _gun3Bar;
			gui::ProgressBar* _gun4Bar;
			gui::ProgressBar* _mineBar;
			gui::PlaneFon* _hyperBack;
			gui::PlaneFon* _gun1Back;
			gui::PlaneFon* _gun2Back;
			gui::PlaneFon* _gun3Back;
			gui::PlaneFon* _gun4Back;
			gui::PlaneFon* _mineBack;
			gui::PlaneFon* _lifeBack;
			Opponents _opponents;

			gui::PlaneFon* _guiTimer[5]{};
			CarLife _carLifes[cCarLifeEnd]{};

			PickItems _pickItemsBuffer;
			PickItems _pickItems;

			AchievmentItems _achievmentsBuffer;
			AchievmentItems _achievmentItems;

			void NewPickItem(Slot::Type slotType, GameObject::BonusType bonusType, int targetPlayerId, bool kill);
			void ProccessPickItems(float deltaTime);

			void NewAchievment(AchievmentCondition::MyEventData* data);
			void VPSIZE_GET();
			void ProccessAchievments(float deltaTime);

			void ShowCarLifeBar(CarLifeE type, int targetPlayerId, float carLifeTimeMax);
			void ProccessCarLifeBar(float deltaTime);
			const CarLife* GetCarLife(Player* target);

			void InsertSlot(WeaponType type, Slot* slot);
			void ClearSlot(WeaponType type);
			void ClearSlots();
			void UpdateSlots();

			void UpdateOpponents();
			void RemoveOpponent(Opponents::const_iterator iter);
			void RemoveOpponent(Player* player);
			void ClearOpponents();

			void UpdateState(float deltaTime);

			void OnAdjustLayout(const D3DXVECTOR2& vpSize) override;
			void OnInvalidate() override;
		public:
			PlayerStateFrame(Menu* menu, HudMenu* hudMenu, gui::Widget* parent);
			~PlayerStateFrame() override;

			void OnProgress(float deltaTime);
			void OnProcessEvent(unsigned id, EventData* data) override;
			void OnDisconnectedPlayer(NetPlayer* sender);
		};

		class MiniMapFrame
		{
		private:
			struct PlayerPoint
			{
				gui::Plane3d* plane;
				Player* player;
			};

			using Players = List<PlayerPoint>;

			struct Node
			{
				Node()
				{
				}

				Node(const D3DXVECTOR2& mPos, float mSize): pos(mPos), size(mSize)
				{
				}

				Node(const D3DXVECTOR3& mPos, float mSize): pos(mPos), size(mSize)
				{
				}

				D3DXVECTOR2 pos;
				float size;

				D3DXVECTOR2 prevDir;
				D3DXVECTOR2 dir;
				D3DXVECTOR2 midDir;
				D3DXVECTOR2 midNorm;
				float cosDelta;
				float sinAlpha2;
				float nodeRadius;
				bool ccw;
				D3DXVECTOR2 edgeNorm;
			};

			using Nodes = std::list<Node>;
		private:
			Menu* _menu;
			HudMenu* _hudMenu;

			gui::Dummy* _root;
			gui::ViewPort3d* _map;
			Players _players;
			gui::Label* _lap;
			gui::PlaneFon* _lapBack;

			void CreatePlayers();
			void DelPlayer(Players::const_iterator iter);
			void DelPlayer(Player* player);
			void ClearPlayers();
			void UpdatePlayers(float deltaTime);

			void ComputeNode(Nodes::iterator sIter, Nodes::iterator eIter, Nodes::iterator iter);
			void AlignNode(const Node& src, Node& dest, float cosErr, float sizeErr);
			void AlignMidNodes(Node& node1, Node& node2, float cosErr, float sizeErr);
			void BuildPath(WayPath& path, res::VertexData& data);
			void UpdateMap();

			Trace* GetTrace();
		public:
			MiniMapFrame(Menu* menu, HudMenu* hudMenu, gui::Widget* parent);
			virtual ~MiniMapFrame();

			void AdjustLayout(const D3DXVECTOR2& vpSize);
			void Show(bool value);
			bool IsVisible() const;

			void OnProgress(float deltaTime);
			void OnDisconnectedPlayer(NetPlayer* sender);

			gui::Dummy* GetRoot();
		};

		class HudMenu : INetGameUser, gui::Widget::Event, ControlEvent
		{
		public:
			enum State { msMain = 0, cStateEnd };

		private:
			Menu* _menu;
			Player* _player;
			State _state;

			gui::Dummy* _root;

			MiniMapFrame* _miniMapFrame;
			PlayerStateFrame* _playerStateFrame;

			void ApplyState(State state);
		protected:
			void OnDisconnectedPlayer(NetPlayer* sender) override;
			bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick) override;
			bool OnHandleInput(const InputMessage& msg) override;
		public:
			HudMenu(Menu* menu, gui::Widget* parent, Player* player);
			~HudMenu() override;

			void AdjustLayout(const D3DXVECTOR2& vpSize);
			void Show(bool value);

			void OnProgress(float deltaTime);

			Player* GetPlayer();
			gui::Widget* GetRoot();

			State GetState() const;
			void SetState(State value);

			static AABB2 GetMiniMapRectA();
			static AABB2 GetMiniMapRectB();
			static AABB2 GetMiniMapRectC();
			static AABB2 GetMiniMapRectD();
			static AABB2 GetMiniMapRectE();
			static AABB2 GetMiniMapRectF();
			static AABB2 GetMiniMapRectG();
			static AABB2 GetMiniMapRectH();
			static AABB2 GetMiniMapRectI();
			static AABB2 GetMiniMapRectJ();
			static AABB2 GetMiniMapRectK();

			static D3DXVECTOR2 GetWeaponPos();
			static D3DXVECTOR2 GetWeaponBoxPos();
			static D3DXVECTOR2 GetWeaponLabelPos();
			static D3DXVECTOR2 GetWeaponLabelAltPos();

			static D3DXVECTOR2 GetWeaponPosMine();
			static D3DXVECTOR2 GetWeaponPosMineLabel();
			static D3DXVECTOR2 GetWeaponPosHyper();
			static D3DXVECTOR2 GetWeaponPosHyperLabel();
			static D3DXVECTOR2 GetHyperCountLabelClassic();
			static D3DXVECTOR2 GetMinesCountLabelClassic();
			static D3DXVECTOR2 GetAllWeaponCountLabelClassic();

			static D3DXVECTOR2 GetPlacePos();
			static D3DXVECTOR2 GetClassicLapsPos();
			static D3DXVECTOR2 GetLapPos();
			static D3DXVECTOR2 GetLifeBarPos();
			static D3DXVECTOR2 GetHyperBarPos();
			static D3DXVECTOR2 GetPickItemsPos();
			static D3DXVECTOR2 GetClassicItemsPos();
			static D3DXVECTOR2 GetAltItemsPos();
			D3DXVECTOR2 GetAchievmentItemsPos();
			static D3DXVECTOR2 GetCarLifeBarPos();
			static D3DXVECTOR2 GetCarLifeBarXPos();
		};
	}
}
