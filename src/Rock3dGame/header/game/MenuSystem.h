#pragma once

namespace r3d
{

namespace game
{

class Menu;
		
class MenuFrame: public gui::Widget::Event
{
public:
	static const int cTopmostDef = 0;
	static const int cTopmostPopup = 1;
	static const int cTopmostModal = 2;
	static const int cTopmostCursor = 4;
	static const int cTopmostLoading = 5;
private:
	Menu* _menu;
	gui::Dummy* _root;
protected:
	virtual void OnShow(bool value) {}
	virtual void OnAdjustLayout(const D3DXVECTOR2& vpSize) {}
	virtual void OnInvalidate() {}

	graph::Tex2DResource* GetTexture(const std::string& name);
	graph::IndexedVBMesh* GetMesh(const std::string& name);
	graph::TextFont* GetFont(const std::string& name);
	const std::string& GetString(StringValue value);
	const std::string& GetString(const std::string& value);
	bool HasString(StringValue value) const;
	bool HasString(const std::string& value) const;

	Menu* menu() const;
	gui::Dummy* root() const;
	gui::Manager* uiRoot() const;

	Workshop& workshop() const;
	Garage& garage() const;
	Tournament& tournament() const;
	AchievmentModel& achievment() const;
	Player* player() const;
	NetGame* net() const;
	NetPlayer* netPlayer() const;
#ifdef STEAM_SERVICE
	SteamService* steamService() const;
#endif
public:
	MenuFrame(Menu* menu, gui::Widget* parent);
	virtual ~MenuFrame();

	void Show(bool value);
	void ShowModal(bool value, int level = cTopmostModal);
	bool visible() const;

	void AdjustLayout(const D3DXVECTOR2& vpSize);
	void Invalidate();

	void SetPos(const D3DXVECTOR2& pos, gui::Widget::Anchor align, const D3DXVECTOR2& size);
};

}

}