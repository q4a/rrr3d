#include "stdafx.h"
#include "game\Menu.h"

namespace r3d
{

namespace game
{

MenuFrame::MenuFrame(Menu* menu, gui::Widget* parent): _menu(menu)
{
	_root = _menu->GetGUI()->CreateDummy();
	_root->SetParent(parent);
	_root->SetVisible(false);
}

MenuFrame::~MenuFrame()
{
	_menu->GetGUI()->ReleaseWidget(_root);
}

graph::Tex2DResource* MenuFrame::GetTexture(const std::string& name)
{
	return menu()->GetTexture(name);
}

graph::IndexedVBMesh* MenuFrame::GetMesh(const std::string& name)
{
	return menu()->GetMesh(name);
}

graph::TextFont* MenuFrame::GetFont(const std::string& name)
{
	return menu()->GetFont(name);
}

const std::string& MenuFrame::GetString(StringValue value)
{
	return menu()->GetString(value);
}

const std::string& MenuFrame::GetString(const std::string& value)
{
	return menu()->GetString(value);
}

bool MenuFrame::HasString(StringValue value) const
{
	return menu()->HasString(value);
}

bool MenuFrame::HasString(const std::string& value) const
{
	return menu()->HasString(value);
}

Menu* MenuFrame::menu() const
{
	return _menu;
}

gui::Dummy* MenuFrame::root() const
{
	return _root;
}

gui::Manager* MenuFrame::uiRoot() const
{
	return _menu->GetGUI();
}

Workshop& MenuFrame::workshop() const
{
	return _menu->GetRace()->GetWorkshop();
}

Garage& MenuFrame::garage() const
{
	return _menu->GetRace()->GetGarage();
}

Tournament& MenuFrame::tournament() const
{
	return _menu->GetRace()->GetTournament();
}

AchievmentModel& MenuFrame::achievment() const
{
	return _menu->GetRace()->GetAchievment();
}

Player* MenuFrame::player() const
{
	return _menu->GetPlayer();
}

NetGame* MenuFrame::net() const
{
	return _menu->GetNet();
}

NetPlayer* MenuFrame::netPlayer() const
{
	return _menu->GetNet()->player();
}

#ifdef STEAM_SERVICE

SteamService* MenuFrame::steamService() const
{
	return _menu->GetSteamService();
}

#endif

void MenuFrame::Show(bool value)
{
	if (_root->GetVisible() == value)
		return;
	_root->SetVisible(value);

	OnShow(value);

	if (value)
		Invalidate();
}

void MenuFrame::ShowModal(bool value, int level)
{
	Show(value);

	_root->ShowModal(value);
	_root->SetFlag(gui::Widget::wfTopmost, value);
	_root->SetTopmostLevel(level);
}

bool MenuFrame::visible() const
{
	return _root->GetVisible();
}

void MenuFrame::AdjustLayout(const D3DXVECTOR2& vpSize)
{
	if (visible())
		OnAdjustLayout(vpSize);
}

void MenuFrame::Invalidate()
{
	OnInvalidate();
	AdjustLayout(menu()->GetGUI()->GetVPSize());
}

void MenuFrame::SetPos(const D3DXVECTOR2& pos, gui::Widget::Anchor align, const D3DXVECTOR2& size)
{
	D3DXVECTOR2 alignPos = gui::Widget::GetAlignPos(size, align);

	D3DXVECTOR2 pos2 = pos + alignPos;
	pos2.x = lsl::ClampValue(pos2.x, size.x/2 + 15.0f, root()->GetManager().GetVPSize().x - size.x/2 - 15.0f);
	pos2.y = lsl::ClampValue(pos2.y, size.y/2 + 15.0f, root()->GetManager().GetVPSize().y - size.y/2 - 15.0f);

	_root->SetPos(pos2);
}

}

}