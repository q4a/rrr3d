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
			Menu* _menu{};
			HudMenu* _hudMenu;

			gui::PlaneFon* _raceState[8]{};
			gui::ProgressBar* _lifeBar;
			gui::ProgressBar* _lifeBar2;
			gui::PlaneFon* _guiTimer[5]{};

			void OnAdjustLayout(const D3DXVECTOR2& vpSize) override;
			void UpdateState(float deltaTime);
		public:
			PlayerStateFrame(Menu* menu, HudMenu* hudMenu, gui::Widget* parent);
			~PlayerStateFrame() override;

			void OnProgress(float deltaTime);
			virtual void OnProcessEvent(unsigned id, EventData* data);
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
		};
	}
}
