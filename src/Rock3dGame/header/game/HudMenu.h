#pragma once

namespace r3d
{

namespace game
{

class Menu;
class HudMenu;

class PlayerStateFrame: public MenuFrame, public IGameUser
{
private:
	struct WeaponBox
	{
		WeaponBox(): box(0), view(0), slot(0), mesh(0), label(0) {}

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

		glm::vec2 center;
		float radius;
	};

	typedef List<Opponent> Opponents;

	struct PickItem
	{
		gui::PlaneFon* image;
		gui::PlaneFon* photo;
		gui::Label* label;

		float time;
		glm::vec2 pos;
	};

	typedef List<PickItem> PickItems;

	struct AchievmentItem
	{
		gui::PlaneFon* image;
		gui::PlaneFon* points;
		gui::PlaneFon* pointsK;

		float time;
		int lastIndex;
		float indexTime;
		glm::vec2 slotSize;
	};

	typedef List<AchievmentItem> AchievmentItems;

	enum WeaponType {wtHyper = 0, wtMine, wtWeapon1, wtWeapon2, wtWeapon3, wtWeapon4, cWeaponTypeEnd};

	struct CarLife
	{
		gui::PlaneFon* back;
		gui::ProgressBar* bar;
		Player* target;
		float timer;
		float timeMax;
	};

	enum CarLifeE {clOpponent = 0, clHuman, cCarLifeEnd};
private:
	Menu* _menu;
	HudMenu* _hudMenu;

	gui::PlaneFon* _raceState;
	WeaponBox _weaponBox[cWeaponTypeEnd];
	gui::Label* _place;
	gui::ProgressBar* _lifeBar;
	gui::PlaneFon* _lifeBack;
	Opponents _opponents;

	gui::PlaneFon* _guiTimer[5];
	CarLife _carLifes[cCarLifeEnd];

	PickItems _pickItemsBuffer;
	PickItems _pickItems;

	AchievmentItems _achievmentsBuffer;
	AchievmentItems _achievmentItems;

	void NewPickItem(Slot::Type slotType, GameObject::BonusType bonusType, int targetPlayerId, bool kill);
	void ProccessPickItems(float deltaTime);

	void NewAchievment(AchievmentCondition::MyEventData* data);
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

	virtual void OnAdjustLayout(const glm::vec2& vpSize);
	virtual void OnInvalidate();
public:
	PlayerStateFrame(Menu* menu, HudMenu* hudMenu, gui::Widget* parent);
	virtual ~PlayerStateFrame();

	void OnProgress(float deltaTime);
	virtual void OnProcessEvent(unsigned id, EventData* data);
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
	typedef lsl::List<PlayerPoint> Players;

	struct Node
	{
		Node() {}
		Node(const glm::vec2& mPos, float mSize): pos(mPos), size(mSize) {}
		//Node(const D3DXVECTOR3& mPos, float mSize): pos(mPos), size(mSize) {}
		Node(const D3DXVECTOR3& mPos, float mSize): pos(mPos.x, mPos.y), size(mSize) {} // remove after D3DXVECTOR3 replacement

		glm::vec2 pos;
		float size;

		glm::vec2 prevDir;
		glm::vec2 dir;
		glm::vec2 midDir;
		glm::vec2 midNorm;
		float cosDelta;
		float sinAlpha2;
		float nodeRadius;
		bool ccw;
		glm::vec2 edgeNorm;
	};
	typedef std::list<Node> Nodes;
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

	void AdjustLayout(const glm::vec2& vpSize);
	void Show(bool value);
	bool IsVisible() const;

	void OnProgress(float deltaTime);
	void OnDisconnectedPlayer(NetPlayer* sender);

	gui::Dummy* GetRoot();
};

class HudMenu: INetGameUser, gui::Widget::Event, ControlEvent
{
public:
	enum State {msMain = 0, cStateEnd};
private:
	Menu* _menu;
	Player* _player;
	State _state;

	gui::Dummy* _root;

	MiniMapFrame* _miniMapFrame;
	PlayerStateFrame* _playerStateFrame;

	void ApplyState(State state);
protected:
	virtual void OnDisconnectedPlayer(NetPlayer* sender);
	virtual bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick);
	virtual bool OnHandleInput(const InputMessage& msg);
public:
	HudMenu(Menu* menu, gui::Widget* parent, Player* player);
	virtual ~HudMenu();

	void AdjustLayout(const glm::vec2& vpSize);
	void Show(bool value);

	void OnProgress(float deltaTime);

	Player* GetPlayer();
	gui::Widget* GetRoot();

	State GetState() const;
	void SetState(State value);

	AABB2 GetMiniMapRect();

	glm::vec2 GetWeaponPos();
	glm::vec2 GetWeaponBoxPos();
	glm::vec2 GetWeaponLabelPos();

	glm::vec2 GetWeaponPosMine();
	glm::vec2 GetWeaponPosMineLabel();
	glm::vec2 GetWeaponPosHyper();
	glm::vec2 GetWeaponPosHyperLabel();

	glm::vec2 GetPlacePos();
	glm::vec2 GetLapPos();
	glm::vec2 GetLifeBarPos();
	glm::vec2 GetPickItemsPos();
	glm::vec2 GetAchievmentItemsPos();
	glm::vec2 GetCarLifeBarPos();
};

}

}