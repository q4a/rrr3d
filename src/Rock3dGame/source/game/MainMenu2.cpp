#include "stdafx.h"
#include "game\Menu.h"

#include "game\MainMenu2.h"

namespace r3d
{

namespace game
{

namespace n
{

GameModeFrame::GameModeFrame(Menu* menu, MainMenu* mainMenu, gui::Widget* parent): MenuFrame(menu, parent), _mainMenu(mainMenu)
{	
}

void GameModeFrame::OnShow(bool value)
{
	const lsl::string menuItemsStr[cMenuItemEnd] = {_SC(svChampionship), _SC(svSkirmish), _SC(svBack)};

	if (value)
	{
		_mainMenu->SetItems(menuItemsStr, cMenuItemEnd, false, this);
		menu()->SetButtonEnabled(_mainMenu->GetItem(miSkirmish), menu()->GetRace()->IsTutorialCompletedFirstStage());
	}
}

bool GameModeFrame::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
{
	if (sender == _mainMenu->GetItem(miChampionship))
	{
		_mainMenu->PushState(MainMenu::msTournament);
		return true;
	}	

	if (sender == _mainMenu->GetItem(miSkirmish))
	{
		_mainMenu->PushState(MainMenu::msDifficulty);
		return true;
	}	

	if (sender == _mainMenu->GetItem(miBack))
	{
		_mainMenu->BackState();
		return true;
	}

	return false;
}




DifficultyFrame::DifficultyFrame(Menu* menu, MainMenu* mainMenu, gui::Widget* parent): MenuFrame(menu, parent), _mainMenu(mainMenu), _difficulty(gdNormal)
{
}

DifficultyFrame::~DifficultyFrame()
{
	menu()->UnregUser(this);
}

void DifficultyFrame::OnShow(bool value)
{
	lsl::string menuItems[cMenuItemEnd];

	for (int i = 0; i < cDifficultyEnd; ++i)
		menuItems[i] = cDifficultyStr[i];
	menuItems[cDifficultyEnd] = "svBack";

	if (value)
	{
		_mainMenu->SetItems(menuItems, cMenuItemEnd, false, this);

		menu()->RegUser(this);
	}
	else
		menu()->UnregUser(this);
}

bool DifficultyFrame::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
{
	Menu* menu = this->menu();

	if (sender == _mainMenu->GetItem(miBack))
	{
		_mainMenu->BackState();
		return true;
	}

	for (int i = 0; i < cDifficultyEnd; ++i)
		if (sender == _mainMenu->GetItem(i))
		{
			_difficulty = (Difficulty)i;

			if (_mainMenu->ContainsState(MainMenu::msTournament))
			{
#ifndef _DEBUG
				if (!_mainMenu->ContainsState(MainMenu::msNetwork) && !menu->GetDisableVideo())
				{
					menu->GetGUI()->GetRoot()->SetVisible(false);
					_mainMenu->GetRoot()->SetVisible(false);

					const Language* lang = menu->GetLanguageParam();

					if (lang && lang->charset == lcRussian)
						menu->PlayMovie("Data\\Video\\main.avi");
					else
						menu->PlayMovie("Data\\Video\\main_eng.avi");
				}
				else
#endif
				{
					_mainMenu->StartMatch(Race::rmChampionship, _difficulty, NULL);
				}
			}
			else
				_mainMenu->StartMatch(Race::rmSkirmish, _difficulty, NULL);

			return true;
		}

	return false;
}

void DifficultyFrame::OnProcessEvent(unsigned id, EventData* data)
{
	if (id == cVideoStopped)
	{
		menu()->GetGUI()->GetRoot()->SetVisible(true);
		_mainMenu->GetRoot()->SetVisible(true);

		_mainMenu->StartMatch(Race::rmChampionship, _difficulty, NULL);
	}
}




NetBrowserFrame::NetBrowserFrame(Menu* menu, MainMenu* mainMenu, gui::Widget* parent): MenuFrame(menu, parent), _mainMenu(mainMenu), _gridScroll(0)
{
	_info = menu->CreateLabel(svNull, root(), "Small");
	_info->SetText("");

	_grid = menu->CreateGrid(root(), NULL, gui::Grid::gsVertical);
	_grid->SetAlign(gui::Widget::waTop);

	_downArrow = menu->CreateArrow(root(), this);
	_downArrow->SetRot(-D3DX_PI/2);
	D3DXVECTOR2 size = _downArrow->GetSize();
	_downArrow->SetSize(menu->StretchImage(*_downArrow->GetFon(), D3DXVECTOR2(30.0f, 30.0f), true, true, true, false));
	_downArrow->SetSelSize(menu->GetImageSize(*_downArrow->GetSel()) * _downArrow->GetSize().x / size.x);

	_upArrow = menu->CreateArrow(root(), this);
	_upArrow->SetRot(D3DX_PI/2);
	size = _upArrow->GetSize();
	_upArrow->SetSize(menu->StretchImage(*_upArrow->GetFon(), D3DXVECTOR2(30.0f, 30.0f), true, true, true, false));
	_upArrow->SetSelSize(menu->GetImageSize(*_upArrow->GetSel()) * _upArrow->GetSize().x / size.x);
}

NetBrowserFrame::~NetBrowserFrame()
{
	menu()->ReleaseWidget(_upArrow);
	menu()->ReleaseWidget(_downArrow);
	menu()->ReleaseWidget(_grid);

	Show(false);

	menu()->ReleaseWidget(_info);
}

void NetBrowserFrame::UpdateGrid()
{
	_gridScroll = 0;
	D3DXVECTOR2 cellSize = NullVec2;
	unsigned count = net()->netService().endpointList().size();		
	gui::Widget::Children::const_iterator iter = _grid->GetChildren().begin();
	lsl::Vector<Menu::NavElement> navElements;
	lsl::Vector<gui::Widget*> gridItems;

	for (unsigned i = 0; i < count; ++i)
	{
		net::Endpoint endpoint = net()->netService().endpointList()[i];

		gui::Button* item = iter != _grid->GetChildren().end() ? static_cast<gui::Button*>(*iter) : NULL;
		if (item == NULL)
		{
			item = _mainMenu->CreateMenuButton(svNull, _grid, this);			
			iter = _grid->GetChildren().end();
		}
		else
			++iter;

		item->SetText(lsl::StrFmt("%s", endpoint.address.c_str()));
		cellSize = item->GetSize();

		gridItems.push_back(item);
		int itemCount = gridItems.size();
		int index = itemCount - 1;

		if (index >= 2)
		{
			Menu::NavElement navElement = {gridItems[index - 1], {NULL, NULL, gridItems[index - 2], gridItems[index]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
			navElements.push_back(navElement);
		}	

		if (i + 1 == count)
		{
			if (itemCount >= 2)
			{
				Menu::NavElement navElement = {gridItems[0], {NULL, NULL, _upArrow, gridItems[1]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
				navElements.push_back(navElement);
			}

			Menu::NavElement navElement2 = {gridItems[itemCount - 1], {NULL, NULL, itemCount >= 2 ? gridItems[itemCount - 2] : _upArrow, _downArrow}, {cVirtualKeyEnd, cVirtualKeyEnd}};
			navElements.push_back(navElement2);
		}
	}

	gui::Widget::Children children = _grid->GetChildren();	
	while (children.size() > count)
	{		
		menu()->ReleaseWidget(children.back());
		children.erase(--children.end());
	}

	_grid->cellSize(cellSize);	

	int itemCount = gridItems.size();
	{
		Menu::NavElement navElement = {_mainMenu->GetItem(miBack), {NULL, NULL, _downArrow, _upArrow}, {vkBack, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}
	{
		Menu::NavElement navElement = {_upArrow, {NULL, NULL, _mainMenu->GetItem(miBack), itemCount > 0 ? gridItems[0] : NULL}, {cVirtualKeyEnd, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}
	{
		Menu::NavElement navElement = {_downArrow, {NULL, NULL, itemCount > 0 ? gridItems[itemCount - 1] : NULL, _mainMenu->GetItem(miBack)}, {cVirtualKeyEnd, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}

	menu()->SetNavElements(_mainMenu->GetItem(miBack), true, &navElements[0], navElements.size());
}

void NetBrowserFrame::AdjustGrid(const D3DXVECTOR2& vpSize)
{
	_grid->SetPos(vpSize.x/2, vpSize.y/2 - 90.0f);

	int i = 0;
	int cellsOnLine = 1;
	int cellsGrid = 4;

	for (gui::Widget::Children::const_iterator iter = _grid->GetChildren().begin(); iter != _grid->GetChildren().end(); ++iter)
	{
		gui::Widget* child = *iter;
		//is disposed
		if (child->disposed())
			continue;

		if (i >= _gridScroll * cellsOnLine && i < _gridScroll * cellsOnLine + cellsGrid)
			child->SetVisible(true);
		else
			child->SetVisible(false);
		++i;
	}

	_grid->Reposition();

	menu()->SetButtonEnabled(_upArrow, _gridScroll > 0);
	menu()->SetButtonEnabled(_downArrow, _gridScroll < std::max((int)ceil(i / (float)cellsOnLine) - cellsGrid/cellsOnLine, 0));

	//_upArrow->SetVisible(_gridScroll > 0);
	//_downArrow->SetVisible(_gridScroll < std::max((int)ceil(i / (float)cellsOnLine) - cellsGrid/cellsOnLine, 0));
}

void NetBrowserFrame::ScrollGrid(int step)
{
	_gridScroll += step;
	AdjustGrid(menu()->GetGUI()->GetVPSize());
}

void NetBrowserFrame::StartWaiting(bool start, StringValue hint)
{
	for (int i = 0; i < cMenuItemEnd; ++i)
	{
		if (i == miBack)
			continue;

		menu()->SetButtonEnabled(_mainMenu->GetItem(i), !start);
	}

	_grid->SetVisible(!start);

	_info->SetVisible(hint != svNull);
	_info->SetText(GetString(hint));	
}

void NetBrowserFrame::Refresh()
{
	StartWaiting(true, svHintRefreshing);
	net()->PingHosts();
}

void NetBrowserFrame::OnShow(bool value)
{
	const lsl::string menuItemsStr[cMenuItemEnd] = {_SC(svBack)};

	if (value)
	{
		_mainMenu->SetItems(menuItemsStr, cMenuItemEnd, false, this);		

		StartWaiting(false);
		net()->RegUser(this);

		Refresh();
	}
	else
		net()->UnregUser(this);
}

void NetBrowserFrame::OnInvalidate()
{
	UpdateGrid();
}

void NetBrowserFrame::OnAdjustLayout(const D3DXVECTOR2& vpSize)
{
	_info->SetPos(D3DXVECTOR2(vpSize.x/2, vpSize.y/2));

	AdjustGrid(vpSize);

	_upArrow->SetPos(vpSize.x/2, vpSize.y/2 - 108.0f);
	_downArrow->SetPos(vpSize.x/2, vpSize.y/2 + 120.0f);
}

bool NetBrowserFrame::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
{
	//if (sender == _mainMenu->GetItem(miRefresh))
	//{		
	//	return true;
	//}

	if (sender == _mainMenu->GetItem(miBack))
	{
		net()->Close();
		net()->CancelPing();

		_mainMenu->BackState();
		return true;
	}

	if (sender == _downArrow)
	{
		ScrollGrid(1);
		return true;
	}

	if (sender == _upArrow)
	{
		ScrollGrid(-1);
		return true;
	}

	unsigned i = 0;
	for (gui::Widget::Children::const_iterator iter = _grid->GetChildren().begin(); iter != _grid->GetChildren().end(); ++iter, ++i)	
	{
		gui::Button* item = static_cast<gui::Button*>(*iter);

		if (sender == item)
		{
			_mainMenu->ConnectMatch(net()->netService().endpointList()[i]
#ifdef STEAM_SERVICE
			, CSteamID()
#endif
				);

			//if (menu()->ConnectMatch(net()->netService().endpointList()[i], true))
			//	StartWaiting(true, svHintConnecting);
			return true;
		}
	}

	return false;
}

void NetBrowserFrame::OnPingComplete()
{
	Invalidate();
	StartWaiting(false, _grid->GetChildren().size() == 0 ? svHintHostListEmpty : svNull);
}




NetIPAddressFrame::NetIPAddressFrame(Menu* menu, MainMenu* mainMenu, gui::Widget* parent): MenuFrame(menu, parent), _mainMenu(mainMenu)
{	
	//D3DXCOLOR color1(0xffafafaf);
	D3DXCOLOR color1 = Menu::cTextColor;

	StringValue strLabels[cLabelEnd] = {svNull, svEnterIP};
	std::string fontLabels[cLabelEnd] = {"Item", "Small"};
	gui::Text::HorAlign horLabels[cLabelEnd] = {gui::Text::haCenter, gui::Text::haCenter};
	gui::Text::VertAlign vertLabels[cLabelEnd] = {gui::Text::vaTop, gui::Text::vaTop};
	D3DXCOLOR colorLabels[cLabelEnd] = {color1, color1};

	gui::Widget* labelsParent[cLabelEnd] = {root(), root()};

	for (int i = 0; i < cLabelEnd; ++i)	
		_labels[i] = menu->CreateLabel(strLabels[i], labelsParent[i], fontLabels[i], NullVec2, horLabels[i], vertLabels[i], colorLabels[i]);
	_labels[mlInfo]->SetWordWrap(true);
}

NetIPAddressFrame::~NetIPAddressFrame()
{
	menu()->GetControl()->RemoveEvent(this);	

	for (int i = 0; i < cLabelEnd; ++i)
		menu()->ReleaseWidget(_labels[i]);
}

void NetIPAddressFrame::StartWaiting(bool start, StringValue hint)
{
	for (int i = 0; i < cMenuItemEnd; ++i)
	{
		if (i == miBack)
			continue;

		menu()->SetButtonEnabled(_mainMenu->GetItem(i), !start);
	}

	if (hint != svNull)
		_labels[mlInfo]->SetText(GetString(hint));
	else
		_labels[mlInfo]->SetText(GetString(svEnterIP));
}

void NetIPAddressFrame::PushLine(std::string text)
{
	if (text.size() > 0 && text[0] == '_')
		text.erase(0, 1);
	else if (text.empty())
		text = '_';
	else if (text.size() >= 2 && text[text.size() - 1] == text[text.size() - 2] && text[text.size() - 1] == '.')
		return;

	_labels[mlIPAdress]->SetText(text);
}

void NetIPAddressFrame::OnShow(bool value)
{
	const lsl::string menuItemsStr[cMenuItemEnd] = {_SC(svConnect), _SC(svBack)};

	if (value)
	{
		_mainMenu->SetItems(menuItemsStr, cMenuItemEnd, false, this);
		PushLine("");
		StartWaiting(false);

		menu()->GetControl()->InsertEvent(this);		
	}
	else
	{
		menu()->GetControl()->RemoveEvent(this);		
	}
}

void NetIPAddressFrame::OnAdjustLayout(const D3DXVECTOR2& vpSize)
{
	_labels[mlInfo]->SetSize(D3DXVECTOR2(325.0f, 50.0f));
	_labels[mlInfo]->SetPos(D3DXVECTOR2(vpSize.x/2, vpSize.y/2 + 15));

	_labels[mlIPAdress]->SetPos(D3DXVECTOR2(vpSize.x/2, vpSize.y/2 + 45.0f));
}

bool NetIPAddressFrame::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
{
	if (sender == _mainMenu->GetItem(miConnect))
	{
		_mainMenu->ConnectMatch(net::Endpoint(_labels[mlIPAdress]->GetText(), 0)
#ifdef STEAM_SERVICE
			, CSteamID()
#endif
			);

		//if (menu()->ConnectMatch(net::Endpoint(_labels[mlIPAdress]->GetText(), 0), true))
		//	StartWaiting(true, svHintConnecting);
		return true;
	}

	if (sender == _mainMenu->GetItem(miBack))
	{
		net()->Close();		

		_mainMenu->BackState();
		return true;
	}

	return false;
}

bool NetIPAddressFrame::OnHandleInput(const InputMessage& msg)
{
	if (msg.state != ksDown)
		return false;

	if (msg.key == vkButtonX)
	{
		lsl::string text = _labels[mlIPAdress]->GetText();

		if (text.size() > 0)
			PushLine(text.substr(0, text.size() - 1));

		return true;
	}
	else if (msg.key == menu()->GetControl()->CharToVirtualKey(VK_OEM_PERIOD))
	{
		lsl::string text = _labels[mlIPAdress]->GetText();
		PushLine(text + '.');
		return true;
	}

	if (msg.key == vkChar && isdigit((char)msg.unicode))
	{
		lsl::string text = _labels[mlIPAdress]->GetText();
		lsl::string input = lsl::ConvertStrWToA(lsl::stringW(1, msg.unicode));

		PushLine(text + input);

		return true;
	}
	else if ((char)msg.unicode == '.')
	{
		lsl::string text = _labels[mlIPAdress]->GetText();
		PushLine(text + '.');
		return true;
	}

	return false;
}




#ifdef STEAM_SERVICE

LobbyFrame::LobbyFrame(Menu* menu, MainMenu* mainMenu, gui::Widget* parent): MenuFrame(menu, parent), _mainMenu(mainMenu), _gridScroll(0)
{
	_info = menu->CreateLabel(svNull, root(), "Small");
	_info->SetText("");

	_grid = menu->CreateGrid(root(), NULL, gui::Grid::gsVertical);
	_grid->SetAlign(gui::Widget::waTop);

	_downArrow = menu->CreateArrow(root(), this);
	_downArrow->SetRot(-D3DX_PI/2);
	D3DXVECTOR2 size = _downArrow->GetSize();
	_downArrow->SetSize(menu->StretchImage(*_downArrow->GetFon(), D3DXVECTOR2(30.0f, 30.0f), true, true, true, false));
	_downArrow->SetSelSize(menu->GetImageSize(*_downArrow->GetSel()) * _downArrow->GetSize().x / size.x);

	_upArrow = menu->CreateArrow(root(), this);
	_upArrow->SetRot(D3DX_PI/2);
	size = _upArrow->GetSize();
	_upArrow->SetSize(menu->StretchImage(*_upArrow->GetFon(), D3DXVECTOR2(30.0f, 30.0f), true, true, true, false));
	_upArrow->SetSelSize(menu->GetImageSize(*_upArrow->GetSel()) * _upArrow->GetSize().x / size.x);
}

LobbyFrame::~LobbyFrame()
{
	menu()->ReleaseWidget(_upArrow);
	menu()->ReleaseWidget(_downArrow);
	menu()->ReleaseWidget(_grid);

	Show(false);

	menu()->ReleaseWidget(_info);
}

void LobbyFrame::UpdateGrid()
{
	_gridScroll = 0;
	D3DXVECTOR2 cellSize = NullVec2;	
	unsigned count = steamService()->lobby()->GetLobbyMembers().size();
	gui::Widget::Children::const_iterator iter = _grid->GetChildren().begin();
	lsl::Vector<Menu::NavElement> navElements;
	lsl::Vector<gui::Widget*> gridItems;
	gui::Widget* focusedItem = NULL;

	for (unsigned i = 0; i < count; ++i)
	{
		const SteamLobby::LobbyMember& member = steamService()->lobby()->GetLobbyMembers()[i];

		gui::Button* item = iter != _grid->GetChildren().end() ? static_cast<gui::Button*>(*iter) : NULL;
		if (item == NULL)
		{
			item = _mainMenu->CreateMenuButton(svNull, _grid, this);			
			iter = _grid->GetChildren().end();
		}
		else
			++iter;

		item->SetText(member.name);
		cellSize = item->GetSize();

		gridItems.push_back(item);
		int itemCount = gridItems.size();
		int index = itemCount - 1;

		if (index >= 2)
		{
			Menu::NavElement navElement = {gridItems[index - 1], {NULL, NULL, gridItems[index - 2], gridItems[index]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
			navElements.push_back(navElement);
		}	

		if (i + 1 == count)
		{
			if (itemCount >= 2)
			{
				Menu::NavElement navElement = {gridItems[0], {NULL, NULL, _upArrow, gridItems[1]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
				navElements.push_back(navElement);
			}

			Menu::NavElement navElement2 = {gridItems[itemCount - 1], {NULL, NULL, itemCount >= 2 ? gridItems[itemCount - 2] : _upArrow, _downArrow}, {cVirtualKeyEnd, cVirtualKeyEnd}};
			navElements.push_back(navElement2);
		}

		if (focusedItem == NULL && item->IsFocused())
			focusedItem = item;
	}

	gui::Widget::Children children = _grid->GetChildren();	
	while (children.size() > count)
	{		
		menu()->ReleaseWidget(children.back());
		children.erase(--children.end());
	}

	_grid->cellSize(cellSize);	

	int itemCount = gridItems.size();
	{
		Menu::NavElement navElement = {_mainMenu->GetItem(miBack), {NULL, NULL, _downArrow, _upArrow}, {vkBack, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}
	{
		Menu::NavElement navElement = {_upArrow, {NULL, NULL, _mainMenu->GetItem(miBack), itemCount > 0 ? gridItems[0] : NULL}, {cVirtualKeyEnd, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}
	{
		Menu::NavElement navElement = {_downArrow, {NULL, NULL, itemCount > 0 ? gridItems[itemCount - 1] : NULL, _mainMenu->GetItem(miBack)}, {cVirtualKeyEnd, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}

	menu()->SetNavElements(_mainMenu->GetItem(miStart), true, &navElements[0], navElements.size());

	if (focusedItem)
		focusedItem->SetFocused(true, true);
}

void LobbyFrame::AdjustGrid(const D3DXVECTOR2& vpSize)
{
	_grid->SetPos(vpSize.x/2, vpSize.y/2 - 50.0f);

	int i = 0;
	int cellsOnLine = 1;
	int cellsGrid = 4;

	for (gui::Widget::Children::const_iterator iter = _grid->GetChildren().begin(); iter != _grid->GetChildren().end(); ++iter)
	{
		gui::Widget* child = *iter;
		//is disposed
		if (child->disposed())
			continue;

		if (i >= _gridScroll * cellsOnLine && i < _gridScroll * cellsOnLine + cellsGrid)
			child->SetVisible(true);
		else
			child->SetVisible(false);
		++i;
	}

	_grid->Reposition();

	menu()->SetButtonEnabled(_upArrow, _gridScroll > 0);
	menu()->SetButtonEnabled(_downArrow, _gridScroll < std::max((int)ceil(i / (float)cellsOnLine) - cellsGrid/cellsOnLine, 0));

	//_upArrow->SetVisible(_gridScroll > 0);
	//_downArrow->SetVisible(_gridScroll < std::max((int)ceil(i / (float)cellsOnLine) - cellsGrid/cellsOnLine, 0));
}

void LobbyFrame::ScrollGrid(int step)
{
	_gridScroll += step;
	AdjustGrid(menu()->GetGUI()->GetVPSize());
}

void LobbyFrame::StartWaiting(bool start, StringValue hint)
{
	for (int i = 0; i < cMenuItemEnd; ++i)
	{
		if (i == miBack)
			continue;

		menu()->SetButtonEnabled(_mainMenu->GetItem(i), !start);
	}

	_grid->SetVisible(!start);

	_info->SetVisible(hint != svNull);
	_info->SetText(GetString(hint));	
}

void LobbyFrame::Refresh()
{
	StartWaiting(true, svHintRefreshing);
	//steamService()->lobby()->RequestLobbyList();
}

void LobbyFrame::OnShow(bool value)
{
	const lsl::string menuItemsStr[cMenuItemEnd] = {"svStartMatch", _SC(svBack)};

	if (value)
	{
		_mainMenu->SetItems(menuItemsStr, cMenuItemEnd, false, this);		

		menu()->RegUser(this);

		//Refresh();
	}
	else
		menu()->UnregUser(this);
}

void LobbyFrame::OnInvalidate()
{
	UpdateGrid();

	StartWaiting(false, _grid->GetChildren().size() == 0 ? svHintHostListEmpty : svNull);

	menu()->SetButtonEnabled(_mainMenu->GetItem(miStart), steamService()->lobby()->lobbyCreated());
}

void LobbyFrame::OnAdjustLayout(const D3DXVECTOR2& vpSize)
{
	_info->SetPos(D3DXVECTOR2(vpSize.x/2, vpSize.y/2));

	AdjustGrid(vpSize);

	_upArrow->SetPos(vpSize.x/2, vpSize.y/2 - 108.0f);
	_downArrow->SetPos(vpSize.x/2, vpSize.y/2 + 120.0f);
}

bool LobbyFrame::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
{
	if (sender == _mainMenu->GetItem(miBack))
	{
		steamService()->lobby()->LeaveLobby();
		_mainMenu->BackState();
		return true;
	}

	if (sender == _mainMenu->GetItem(miStart))
	{
		if (_grid->GetChildren().size() > 0)
			_mainMenu->StartMatch(_mainMenu->steamHostMode(), _mainMenu->steamHostDifficulty(), _mainMenu->steamHostProfile(), true);
		else
			menu()->ShowMessage(GetString(svWarning), GetString("svNeedAtLeastOneMember"), GetString(svOk), uiRoot()->GetVPSize()/2, gui::Widget::waCenter, 0.0f);
		return true;
	}

	if (sender == _downArrow)
	{
		ScrollGrid(1);
		return true;
	}

	if (sender == _upArrow)
	{
		ScrollGrid(-1);
		return true;
	}

	unsigned i = 0;
	for (gui::Widget::Children::const_iterator iter = _grid->GetChildren().begin(); iter != _grid->GetChildren().end(); ++iter, ++i)	
	{
		gui::Button* item = static_cast<gui::Button*>(*iter);

		if (sender == item)
		{
			return true;
		}
	}

	return false;
}

void LobbyFrame::OnProcessEvent(unsigned id, EventData* data)
{
	switch (id)
	{
	case cSteamUpdateLobby:
	case cSteamUpdateLobbyMember:
	case cSteamUpdateLobbyMembers:
		Invalidate();
		break;
	}
}




MatchmakingFrame::MatchmakingFrame(Menu* menu, MainMenu* mainMenu, gui::Widget* parent): MenuFrame(menu, parent), _mainMenu(mainMenu), _gridScroll(0)
{
	_info = menu->CreateLabel(svNull, root(), "Small");
	_info->SetText("");

	_grid = menu->CreateGrid(root(), NULL, gui::Grid::gsVertical);
	_grid->SetAlign(gui::Widget::waTop);

	_downArrow = menu->CreateArrow(root(), this);
	_downArrow->SetRot(-D3DX_PI/2);
	D3DXVECTOR2 size = _downArrow->GetSize();
	_downArrow->SetSize(menu->StretchImage(*_downArrow->GetFon(), D3DXVECTOR2(30.0f, 30.0f), true, true, true, false));
	_downArrow->SetSelSize(menu->GetImageSize(*_downArrow->GetSel()) * _downArrow->GetSize().x / size.x);

	_upArrow = menu->CreateArrow(root(), this);
	_upArrow->SetRot(D3DX_PI/2);
	size = _upArrow->GetSize();
	_upArrow->SetSize(menu->StretchImage(*_upArrow->GetFon(), D3DXVECTOR2(30.0f, 30.0f), true, true, true, false));
	_upArrow->SetSelSize(menu->GetImageSize(*_upArrow->GetSel()) * _upArrow->GetSize().x / size.x);
}

MatchmakingFrame::~MatchmakingFrame()
{
	menu()->ReleaseWidget(_upArrow);
	menu()->ReleaseWidget(_downArrow);
	menu()->ReleaseWidget(_grid);

	Show(false);

	menu()->ReleaseWidget(_info);
}

void MatchmakingFrame::UpdateGrid()
{
	_gridScroll = 0;
	D3DXVECTOR2 cellSize = NullVec2;
	unsigned count = steamService()->lobby()->GetLobbyList().size();
	gui::Widget::Children::const_iterator iter = _grid->GetChildren().begin();
	lsl::Vector<Menu::NavElement> navElements;
	lsl::Vector<gui::Widget*> gridItems;
	gui::Widget* focusedItem = NULL;

	for (unsigned i = 0; i < count; ++i)
	{
		const SteamLobby::Lobby& lobby = steamService()->lobby()->GetLobbyList()[i];

		gui::Button* item = iter != _grid->GetChildren().end() ? static_cast<gui::Button*>(*iter) : NULL;
		if (item == NULL)
		{
			item = _mainMenu->CreateMenuButton(svNull, _grid, this);			
			iter = _grid->GetChildren().end();
		}
		else
			++iter;

		item->SetText(!lobby.name.empty() ? lobby.name : "user's lobby");
		cellSize = item->GetSize();

		gridItems.push_back(item);
		int itemCount = gridItems.size();
		int index = itemCount - 1;

		if (index >= 2)
		{
			Menu::NavElement navElement = {gridItems[index - 1], {NULL, NULL, gridItems[index - 2], gridItems[index]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
			navElements.push_back(navElement);
		}	

		if (i + 1 == count)
		{
			if (itemCount >= 2)
			{
				Menu::NavElement navElement = {gridItems[0], {NULL, NULL, _upArrow, gridItems[1]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
				navElements.push_back(navElement);
			}

			Menu::NavElement navElement2 = {gridItems[itemCount - 1], {NULL, NULL, itemCount >= 2 ? gridItems[itemCount - 2] : _upArrow, _downArrow}, {cVirtualKeyEnd, cVirtualKeyEnd}};
			navElements.push_back(navElement2);
		}

		if (focusedItem == NULL && item->IsFocused())
			focusedItem = item;
	}

	gui::Widget::Children children = _grid->GetChildren();	
	while (children.size() > count)
	{		
		menu()->ReleaseWidget(children.back());
		children.erase(--children.end());
	}

	_grid->cellSize(cellSize);	

	int itemCount = gridItems.size();
	{
		Menu::NavElement navElement = {_mainMenu->GetItem(miBack), {NULL, NULL, _downArrow, _upArrow}, {vkBack, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}
	{
		Menu::NavElement navElement = {_upArrow, {NULL, NULL, _mainMenu->GetItem(miBack), itemCount > 0 ? gridItems[0] : NULL}, {cVirtualKeyEnd, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}
	{
		Menu::NavElement navElement = {_downArrow, {NULL, NULL, itemCount > 0 ? gridItems[itemCount - 1] : NULL, _mainMenu->GetItem(miBack)}, {cVirtualKeyEnd, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}

	menu()->SetNavElements(_mainMenu->GetItem(miBack), true, &navElements[0], navElements.size());

	if (focusedItem)
		focusedItem->SetFocused(true, true);
}

void MatchmakingFrame::AdjustGrid(const D3DXVECTOR2& vpSize)
{
	_grid->SetPos(vpSize.x/2, vpSize.y/2 - 90.0f);

	int i = 0;
	int cellsOnLine = 1;
	int cellsGrid = 4;

	for (gui::Widget::Children::const_iterator iter = _grid->GetChildren().begin(); iter != _grid->GetChildren().end(); ++iter)
	{
		gui::Widget* child = *iter;
		//is disposed
		if (child->disposed())
			continue;

		if (i >= _gridScroll * cellsOnLine && i < _gridScroll * cellsOnLine + cellsGrid)
			child->SetVisible(true);
		else
			child->SetVisible(false);
		++i;
	}

	_grid->Reposition();

	menu()->SetButtonEnabled(_upArrow, _gridScroll > 0);
	menu()->SetButtonEnabled(_downArrow, _gridScroll < std::max((int)ceil(i / (float)cellsOnLine) - cellsGrid/cellsOnLine, 0));

	//_upArrow->SetVisible(_gridScroll > 0);
	//_downArrow->SetVisible(_gridScroll < std::max((int)ceil(i / (float)cellsOnLine) - cellsGrid/cellsOnLine, 0));
}

void MatchmakingFrame::ScrollGrid(int step)
{
	_gridScroll += step;
	AdjustGrid(menu()->GetGUI()->GetVPSize());
}

void MatchmakingFrame::StartWaiting(bool start, StringValue hint)
{
	for (int i = 0; i < cMenuItemEnd; ++i)
	{
		if (i == miBack)
			continue;

		menu()->SetButtonEnabled(_mainMenu->GetItem(i), !start);
	}

	_grid->SetVisible(!start);

	_info->SetVisible(hint != svNull);
	_info->SetText(GetString(hint));	
}

void MatchmakingFrame::Refresh()
{
	StartWaiting(true, svHintRefreshing);
	steamService()->lobby()->RequestLobbyList();
}

void MatchmakingFrame::OnShow(bool value)
{
	const lsl::string menuItemsStr[cMenuItemEnd] = {_SC(svBack)};

	if (value)
	{
		_mainMenu->SetItems(menuItemsStr, cMenuItemEnd, false, this);		

		StartWaiting(false);
		menu()->RegUser(this);

		Refresh();
	}
	else
		menu()->UnregUser(this);
}

void MatchmakingFrame::OnInvalidate()
{
	UpdateGrid();
}

void MatchmakingFrame::OnAdjustLayout(const D3DXVECTOR2& vpSize)
{
	_info->SetPos(D3DXVECTOR2(vpSize.x/2, vpSize.y/2));

	AdjustGrid(vpSize);

	_upArrow->SetPos(vpSize.x/2, vpSize.y/2 - 108.0f);
	_downArrow->SetPos(vpSize.x/2, vpSize.y/2 + 120.0f);
}

bool MatchmakingFrame::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
{
	if (sender == _mainMenu->GetItem(miBack))
	{
		_mainMenu->BackState();
		return true;
	}

	if (sender == _downArrow)
	{
		ScrollGrid(1);
		return true;
	}

	if (sender == _upArrow)
	{
		ScrollGrid(-1);
		return true;
	}

	unsigned i = 0;
	for (gui::Widget::Children::const_iterator iter = _grid->GetChildren().begin(); iter != _grid->GetChildren().end(); ++iter, ++i)	
	{
		gui::Button* item = static_cast<gui::Button*>(*iter);

		if (sender == item)
		{
			menu()->ShowMessageLoading();
			steamService()->lobby()->JoinLobby(steamService()->lobby()->GetLobbyList()[i].id);
			return true;
		}
	}

	return false;
}

void MatchmakingFrame::OnProcessEvent(unsigned id, EventData* data)
{
	switch (id)
	{
	case cSteamGetLobbyList:
		Invalidate();
		StartWaiting(false, _grid->GetChildren().size() == 0 ? svHintHostListEmpty : svNull);
		break;

	case cSteamUpdateLobby:
		UpdateGrid();
		break;

	case cSteamEnteredLobby:
		menu()->HideMessage();

		if (data->success)
		{
			_mainMenu->SetState(MainMenu::msLobby);
		}
		else
			menu()->ShowMessage(GetString(svWarning), GetString("svSteamConnectFailed"), GetString(svOk), uiRoot()->GetVPSize()/2, gui::Widget::waCenter, 0.0f);
		break;
	}
}




SteamBrowserFrame::SteamBrowserFrame(Menu* menu, MainMenu* mainMenu, gui::Widget* parent): MenuFrame(menu, parent), _mainMenu(mainMenu), _gridScroll(0)
{
	_info = menu->CreateLabel(svNull, root(), "Small");
	_info->SetText("");

	_grid = menu->CreateGrid(root(), NULL, gui::Grid::gsVertical);
	_grid->SetAlign(gui::Widget::waTop);

	_downArrow = menu->CreateArrow(root(), this);
	_downArrow->SetRot(-D3DX_PI/2);
	D3DXVECTOR2 size = _downArrow->GetSize();
	_downArrow->SetSize(menu->StretchImage(*_downArrow->GetFon(), D3DXVECTOR2(30.0f, 30.0f), true, true, true, false));
	_downArrow->SetSelSize(menu->GetImageSize(*_downArrow->GetSel()) * _downArrow->GetSize().x / size.x);

	_upArrow = menu->CreateArrow(root(), this);
	_upArrow->SetRot(D3DX_PI/2);
	size = _upArrow->GetSize();
	_upArrow->SetSize(menu->StretchImage(*_upArrow->GetFon(), D3DXVECTOR2(30.0f, 30.0f), true, true, true, false));
	_upArrow->SetSelSize(menu->GetImageSize(*_upArrow->GetSel()) * _upArrow->GetSize().x / size.x);
}

SteamBrowserFrame::~SteamBrowserFrame()
{
	menu()->ReleaseWidget(_upArrow);
	menu()->ReleaseWidget(_downArrow);
	menu()->ReleaseWidget(_grid);

	Show(false);

	menu()->ReleaseWidget(_info);
}

void SteamBrowserFrame::UpdateGrid()
{
	_gridScroll = 0;
	D3DXVECTOR2 cellSize = NullVec2;
	unsigned count = steamService()->server()->hostList().size();
	gui::Widget::Children::const_iterator iter = _grid->GetChildren().begin();
	lsl::Vector<Menu::NavElement> navElements;
	lsl::Vector<gui::Widget*> gridItems;
	gui::Widget* focusedItem = NULL;

	for (unsigned i = 0; i < count; ++i)
	{
		const SteamServer::Host& host = steamService()->server()->hostList()[i];

		gui::Button* item = iter != _grid->GetChildren().end() ? static_cast<gui::Button*>(*iter) : NULL;
		if (item == NULL)
		{
			item = _mainMenu->CreateMenuButton(svNull, _grid, this);			
			iter = _grid->GetChildren().end();
		}
		else
			++iter;

		item->SetText(host.inst->GetName());
		cellSize = item->GetSize();

		gridItems.push_back(item);
		int itemCount = gridItems.size();
		int index = itemCount - 1;

		if (index >= 2)
		{
			Menu::NavElement navElement = {gridItems[index - 1], {NULL, NULL, gridItems[index - 2], gridItems[index]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
			navElements.push_back(navElement);
		}	

		if (i + 1 == count)
		{
			if (itemCount >= 2)
			{
				Menu::NavElement navElement = {gridItems[0], {NULL, NULL, _upArrow, gridItems[1]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
				navElements.push_back(navElement);
			}

			Menu::NavElement navElement2 = {gridItems[itemCount - 1], {NULL, NULL, itemCount >= 2 ? gridItems[itemCount - 2] : _upArrow, _downArrow}, {cVirtualKeyEnd, cVirtualKeyEnd}};
			navElements.push_back(navElement2);
		}

		if (focusedItem == NULL && item->IsFocused())
			focusedItem = item;
	}

	gui::Widget::Children children = _grid->GetChildren();	
	while (children.size() > count)
	{		
		menu()->ReleaseWidget(children.back());
		children.erase(--children.end());
	}

	_grid->cellSize(cellSize);	

	int itemCount = gridItems.size();
	{
		Menu::NavElement navElement = {_mainMenu->GetItem(miBack), {NULL, NULL, _downArrow, _upArrow}, {vkBack, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}
	{
		Menu::NavElement navElement = {_upArrow, {NULL, NULL, _mainMenu->GetItem(miBack), itemCount > 0 ? gridItems[0] : NULL}, {cVirtualKeyEnd, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}
	{
		Menu::NavElement navElement = {_downArrow, {NULL, NULL, itemCount > 0 ? gridItems[itemCount - 1] : NULL, _mainMenu->GetItem(miBack)}, {cVirtualKeyEnd, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}

	menu()->SetNavElements(_mainMenu->GetItem(miBack), true, &navElements[0], navElements.size());

	if (focusedItem)
		focusedItem->SetFocused(true, true);
}

void SteamBrowserFrame::AdjustGrid(const D3DXVECTOR2& vpSize)
{
	_grid->SetPos(vpSize.x/2, vpSize.y/2 - 90.0f);

	int i = 0;
	int cellsOnLine = 1;
	int cellsGrid = 4;

	for (gui::Widget::Children::const_iterator iter = _grid->GetChildren().begin(); iter != _grid->GetChildren().end(); ++iter)
	{
		gui::Widget* child = *iter;
		//is disposed
		if (child->disposed())
			continue;

		if (i >= _gridScroll * cellsOnLine && i < _gridScroll * cellsOnLine + cellsGrid)
			child->SetVisible(true);
		else
			child->SetVisible(false);
		++i;
	}

	_grid->Reposition();

	menu()->SetButtonEnabled(_upArrow, _gridScroll > 0);
	menu()->SetButtonEnabled(_downArrow, _gridScroll < std::max((int)ceil(i / (float)cellsOnLine) - cellsGrid/cellsOnLine, 0));

	//_upArrow->SetVisible(_gridScroll > 0);
	//_downArrow->SetVisible(_gridScroll < std::max((int)ceil(i / (float)cellsOnLine) - cellsGrid/cellsOnLine, 0));
}

void SteamBrowserFrame::ScrollGrid(int step)
{
	_gridScroll += step;
	AdjustGrid(menu()->GetGUI()->GetVPSize());
}

void SteamBrowserFrame::StartWaiting(bool start, StringValue hint)
{
	for (int i = 0; i < cMenuItemEnd; ++i)
	{
		if (i == miBack)
			continue;

		menu()->SetButtonEnabled(_mainMenu->GetItem(i), !start);
	}

	_grid->SetVisible(!start);

	_info->SetVisible(hint != svNull);
	_info->SetText(GetString(hint));	
}

void SteamBrowserFrame::Refresh()
{
	StartWaiting(true, svHintRefreshing);

	if (_mainMenu->GetServerType() == MainMenu::stLocal)
		steamService()->server()->FindLanServers();
	else
		steamService()->server()->FindInternetList();
}

void SteamBrowserFrame::OnShow(bool value)
{
	const lsl::string menuItemsStr[cMenuItemEnd] = {_SC(svBack)};

	if (value)
	{
		_mainMenu->SetItems(menuItemsStr, cMenuItemEnd, false, this);		

		StartWaiting(false);
		menu()->RegUser(this);
		net()->RegUser(this);

		Refresh();
	}
	else
	{
		menu()->UnregUser(this);
		net()->UnregUser(this);
	}
}

void SteamBrowserFrame::OnInvalidate()
{
	UpdateGrid();
}

void SteamBrowserFrame::OnAdjustLayout(const D3DXVECTOR2& vpSize)
{
	_info->SetPos(D3DXVECTOR2(vpSize.x/2, vpSize.y/2));

	AdjustGrid(vpSize);

	_upArrow->SetPos(vpSize.x/2, vpSize.y/2 - 108.0f);
	_downArrow->SetPos(vpSize.x/2, vpSize.y/2 + 120.0f);
}

bool SteamBrowserFrame::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
{
	if (sender == _mainMenu->GetItem(miBack))
	{
		_mainMenu->BackState();
		return true;
	}

	if (sender == _downArrow)
	{
		ScrollGrid(1);
		return true;
	}

	if (sender == _upArrow)
	{
		ScrollGrid(-1);
		return true;
	}

	unsigned i = 0;
	for (gui::Widget::Children::const_iterator iter = _grid->GetChildren().begin(); iter != _grid->GetChildren().end(); ++iter, ++i)	
	{
		gui::Button* item = static_cast<gui::Button*>(*iter);

		if (sender == item)
		{
			//const SteamServer::Host& host = steamService()->server()->hostList()[i];
			//net::Endpoint endpoint(host.inst->m_NetAdr.GetIP(), host.inst->m_NetAdr.GetConnectionPort());
			//if (menu()->ConnectMatch(endpoint, false))
			//	StartWaiting(true, svHintConnecting);

			_mainMenu->ConnectMatch(net::Endpoint(), steamService()->server()->hostList()[i].inst->m_steamID);
			return true;
		}
	}

	return false;
}

void SteamBrowserFrame::OnProcessEvent(unsigned id, EventData* data)
{
	switch (id)
	{
	case cSteamHostList:
		Invalidate();
		StartWaiting(false, _grid->GetChildren().size() == 0 ? svHintHostListEmpty : svNull);
		break;
	}
}

void SteamBrowserFrame::OnDisconnected(net::INetConnection* sender)
{
	StartWaiting(false);
}

void SteamBrowserFrame::OnConnectionFailed(net::INetConnection* sender, unsigned error)
{
	StartWaiting(false);
}

void SteamBrowserFrame::OnConnectedPlayer(NetPlayer* sender)
{
	if (sender->owner())
	{
		StartWaiting(false);
	}
}

void SteamBrowserFrame::OnDisconnectedPlayer(NetPlayer* sender)
{
	StartWaiting(false);
}

#endif




ServerTypeFrame::ServerTypeFrame(Menu* menu, MainMenu* mainMenu, gui::Widget* parent): MenuFrame(menu, parent), _mainMenu(mainMenu)
{	
}

void ServerTypeFrame::OnShow(bool value)
{
	const lsl::string menuItemsStr[cMenuItemEnd] = {"svLocalServer", 
#ifdef STEAM_SERVICE
	#ifndef _RETAIL
		"svSteamServer", 
	#endif
		"svSteamLobby",
#endif
		_SC(svBack)};

	if (value)
	{
		_mainMenu->SetItems(menuItemsStr, cMenuItemEnd, false, this);
	}
}

bool ServerTypeFrame::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
{
	if (sender == _mainMenu->GetItem(miLocal))
	{
		_mainMenu->SetServerType(MainMenu::stLocal);
		_mainMenu->PushState(MainMenu::msGameMode);
		return true;
	}	

#ifdef STEAM_SERVICE
	#ifndef _RETAIL
		if (sender == _mainMenu->GetItem(miSteam))
		{
			_mainMenu->SetServerType(MainMenu::stSteam);
			_mainMenu->PushState(MainMenu::msGameMode);
			return true;
		}
	#endif

	if (sender == _mainMenu->GetItem(miLobby))
	{
		_mainMenu->SetServerType(MainMenu::stLobby);
		_mainMenu->PushState(MainMenu::msGameMode);		
		return true;
	}
#endif

	if (sender == _mainMenu->GetItem(miBack))
	{
		_mainMenu->BackState();
		return true;
	}

	return false;
}




ClientTypeFrame::ClientTypeFrame(Menu* menu, MainMenu* mainMenu, gui::Widget* parent): MenuFrame(menu, parent), _mainMenu(mainMenu)
{	
}

void ClientTypeFrame::OnShow(bool value)
{
	const lsl::string menuItemsStr[cMenuItemEnd] = {"svConnectLan", "svConnectIP", 
#ifdef STEAM_SERVICE
	#ifndef _RETAIL
		"svConnectSteam", "svConnectSteamLan",
	#endif
		"svSteamLobby",
#endif
		_SC(svBack)};

	if (value)
	{
		_mainMenu->SetItems(menuItemsStr, cMenuItemEnd, false, this);
	}
}

bool ClientTypeFrame::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
{
	if (sender == _mainMenu->GetItem(miConnectLan))
	{
		_mainMenu->SetServerType(MainMenu::cServerTypeEnd);
		_mainMenu->PushState(MainMenu::msNetBrowser);
		return true;
	}	

	if (sender == _mainMenu->GetItem(miConnectIP))
	{
		_mainMenu->SetServerType(MainMenu::cServerTypeEnd);
		_mainMenu->PushState(MainMenu::msNetIPAdress);
		return true;
	}	

#ifdef STEAM_SERVICE
	#ifndef _RETAIL
		if (sender == _mainMenu->GetItem(miConnectSteam))
		{
			_mainMenu->SetServerType(MainMenu::stSteam);
			_mainMenu->PushState(MainMenu::msSteamBrowser);
			return true;
		}

		if (sender == _mainMenu->GetItem(miConnectSteamLan))
		{
			_mainMenu->SetServerType(MainMenu::stLocal);
			_mainMenu->PushState(MainMenu::msSteamBrowser);
			return true;
		}
	#endif

	if (sender == _mainMenu->GetItem(miConnectLobby))
	{
		_mainMenu->SetServerType(MainMenu::stLobby);
		_mainMenu->PushState(MainMenu::msMatchmaking);
		return true;
	}
#endif


	if (sender == _mainMenu->GetItem(miBack))
	{
		_mainMenu->BackState();
		return true;
	}

	return false;
}




NetworkFrame::NetworkFrame(Menu* menu, MainMenu* mainMenu, gui::Widget* parent): MenuFrame(menu, parent), _mainMenu(mainMenu)
{
	D3DXCOLOR color1 = Menu::cTextColor;

	StringValue strLabels[cLabelEnd] = {svNull};
	std::string fontLabels[cLabelEnd] = {"Small"};
	gui::Text::HorAlign horLabels[cLabelEnd] = {gui::Text::haCenter};
	gui::Text::VertAlign vertLabels[cLabelEnd] = {gui::Text::vaCenter};
	D3DXCOLOR colorLabels[cLabelEnd] = {color1};

	gui::Widget* labelsParent[cLabelEnd] = {root()};

	for (int i = 0; i < cLabelEnd; ++i)	
		_labels[i] = menu->CreateLabel(strLabels[i], labelsParent[i], fontLabels[i], NullVec2, horLabels[i], vertLabels[i], colorLabels[i]);
}

NetworkFrame::~NetworkFrame()
{
	for (int i = 0; i < cLabelEnd; ++i)
		menu()->ReleaseWidget(_labels[i]);
}

void NetworkFrame::OnShow(bool value)
{
	const lsl::string menuItemsStr[cMenuItemEnd] = {_SC(svNetCreate), _SC(svConnect), _SC(svBack)};

	if (value)
	{
		menu()->InitializeNet();

		_mainMenu->SetItems(menuItemsStr, cMenuItemEnd, false, this);

		lsl::StringVec addrVec;

		if (net()->GetAdapterAddresses(addrVec))
		{
			lsl::string text = "";

			for (unsigned i = 0; i < std::min(addrVec.size(), 6U); ++i)
				text += addrVec[i] + "\n";

			_labels[mlIPAdress]->SetText(!text.empty() ? "My IP:\n" + text : "");
		}
		else
			_labels[mlIPAdress]->SetText("");
	}
}

bool NetworkFrame::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
{
	if (sender == _mainMenu->GetItem(miCreate))
	{
		_mainMenu->PushState(MainMenu::msServerType);
		return true;
	}

	if (sender == _mainMenu->GetItem(miConnect))
	{
		_mainMenu->PushState(MainMenu::msClientType);
		return true;
	}

	if (sender == _mainMenu->GetItem(miBack))
	{
		menu()->FinalizateNet();
		_mainMenu->BackState();
		return true;
	}	

	return false;
}

void NetworkFrame::OnAdjustLayout(const D3DXVECTOR2& vpSize)
{	
	_labels[mlIPAdress]->SetPos(D3DXVECTOR2(vpSize.x/2, vpSize.y/2 + 75.0f));
}




TournamentFrame::TournamentFrame(Menu* menu, MainMenu* mainMenu, gui::Widget* parent): MenuFrame(menu, parent), _mainMenu(mainMenu)
{
}

void TournamentFrame::OnShow(bool value)
{
	const lsl::string menuItemsStr[cMenuItemEnd] = {_SC(svContinue), _SC(svNewGame), _SC(svLoad), _SC(svBack)};

	if (value)
	{
		bool hasProfiles = false;
		bool netGame = _mainMenu->ContainsState(MainMenu::msNetwork);

		_mainMenu->SetItems(menuItemsStr, cMenuItemEnd, false, this);
		menu()->SetButtonEnabled(_mainMenu->GetItem(miContinue), menu()->GetLastProfile(netGame) != NULL);

		for (Race::Profiles::const_iterator iter = menu()->GetRace()->GetProfiles().begin(); iter != menu()->GetRace()->GetProfiles().end(); ++iter)
			if ((*iter)->netGame() == netGame)
			{
				hasProfiles = true;
				break;
			}

		menu()->SetButtonEnabled(_mainMenu->GetItem(miLoad), hasProfiles);

		_mainMenu->InvalidateItems();
	}
}

bool TournamentFrame::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
{
	if (sender == _mainMenu->GetItem(miNewGame))
	{
		_mainMenu->PushState(MainMenu::msDifficulty);
		return true;
	}

	if (sender == _mainMenu->GetItem(miContinue))
	{
		bool netGame = _mainMenu->ContainsState(MainMenu::msNetwork);
		_mainMenu->StartMatch(Race::rmChampionship, cDifficultyEnd, menu()->GetLastProfile(netGame));
		return true;
	}

	if (sender == _mainMenu->GetItem(miLoad))
	{
		_mainMenu->PushState(MainMenu::msLoad);
		return true;
	}

	if (sender == _mainMenu->GetItem(miBack))
	{
		_mainMenu->BackState();
		return true;
	}

	return false;
}




CreditsFrame::CreditsFrame(Menu* menu, MainMenu* mainMenu, gui::Widget* parent): MenuFrame(menu, parent), _mainMenu(mainMenu), _scrollTime(0)
{
	_label = menu->CreateLabel(svCredits, root(), "Small", NullVec2, gui::Text::haCenter, gui::Text::vaTop, D3DXCOLOR(0xffff8a70));
	_label->SetAlign(gui::Widget::waTop);
	_label->SetFlag(gui::Widget::wfClientClip, true);	
	_label->SetSize(350.0f, 265.0f);
}

CreditsFrame::~CreditsFrame()
{
	menu()->ReleaseWidget(_label);
}

void CreditsFrame::OnShow(bool value)
{
	const lsl::string menuItemsStr[cMenuItemEnd] = {_SC(svBack)};

	if (value)
	{
		_mainMenu->SetItems(menuItemsStr, cMenuItemEnd, false, this);
		_label->SetVScroll(_label->GetSize().y);
		_scrollTime = 0.0f;
	}
}

void CreditsFrame::OnAdjustLayout(const D3DXVECTOR2& vpSize)
{
	_label->SetPos(D3DXVECTOR2(vpSize.x/2, vpSize.y/2 - 115.0f));
}

bool CreditsFrame::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
{
	if (sender == _mainMenu->GetItem(miBack))
	{
		_mainMenu->BackState();
		return true;
	}

	return false;
}

void CreditsFrame::OnProgress(float deltaTime)
{
	if (_scrollTime > 0 && (_scrollTime -= deltaTime) <= 0)
	{
		_scrollTime = 0;
	}

	if (_scrollTime == 0)
	{
		_label->SetVScroll(_label->GetVScroll() - 20.0f * deltaTime);

		if (abs(_label->GetVScroll()) > _label->GetTextAABB().GetSize().y)
			_label->SetVScroll(_label->GetSize().y);
	}
}




ProfileFrame::ProfileFrame(Menu* menu, MainMenu* mainMenu, gui::Widget* parent): MenuFrame(menu, parent), _mainMenu(mainMenu), _gridScroll(0)
{
	_grid = menu->CreateGrid(root(), NULL, gui::Grid::gsVertical);
	_grid->SetAlign(gui::Widget::waTop);

	_downArrow = menu->CreateArrow(root(), this);
	_downArrow->SetRot(-D3DX_PI/2);
	D3DXVECTOR2 size = _downArrow->GetSize();
	_downArrow->SetSize(menu->StretchImage(*_downArrow->GetFon(), D3DXVECTOR2(30.0f, 30.0f), true, true, true, false));
	_downArrow->SetSelSize(menu->GetImageSize(*_downArrow->GetSel()) * _downArrow->GetSize().x / size.x);

	_upArrow = menu->CreateArrow(root(), this);
	_upArrow->SetRot(D3DX_PI/2);
	size = _upArrow->GetSize();
	_upArrow->SetSize(menu->StretchImage(*_upArrow->GetFon(), D3DXVECTOR2(30.0f, 30.0f), true, true, true, false));
	_upArrow->SetSelSize(menu->GetImageSize(*_upArrow->GetSel()) * _upArrow->GetSize().x / size.x);
}

ProfileFrame::~ProfileFrame()
{
	menu()->ReleaseWidget(_upArrow);
	menu()->ReleaseWidget(_downArrow);
	menu()->ReleaseWidget(_grid);
}

void ProfileFrame::AdjustGrid(const D3DXVECTOR2& vpSize)
{
	_grid->SetPos(vpSize.x/2, vpSize.y/2 - 90.0f);

	int i = 0;
	int cellsOnLine = 1;
	int cellsGrid = 4;

	for (gui::Widget::Children::const_iterator iter = _grid->GetChildren().begin(); iter != _grid->GetChildren().end(); ++iter)
	{
		gui::Widget* child = *iter;
		//is disposed
		if (child->disposed())
			continue;

		bool visible = i >= _gridScroll * cellsOnLine && i < _gridScroll * cellsOnLine + cellsGrid;

		child->SetVisible(visible);
		child->GetChildren().front()->SetEnabled(visible);

		++i;
	}

	_grid->Reposition();

	menu()->SetButtonEnabled(_upArrow, _gridScroll > 0);
	menu()->SetButtonEnabled(_downArrow, _gridScroll < std::max((int)ceil(i / (float)cellsOnLine) - cellsGrid/cellsOnLine, 0));
}

void ProfileFrame::ScrollGrid(int step)
{
	_gridScroll += step;
	AdjustGrid(menu()->GetGUI()->GetVPSize());
}

void ProfileFrame::UpdateGrid()
{
	D3DXVECTOR2 cellSize = NullVec2;
	unsigned count = menu()->GetRace()->GetProfiles().size();
	unsigned itemCount = 0;
	gui::Widget::Children::const_iterator iter = _grid->GetChildren().begin();
	bool netGame = _mainMenu->ContainsState(MainMenu::msNetwork);
	lsl::Vector<Menu::NavElement> navElements;
	lsl::Vector<std::pair<gui::Widget*, gui::Widget*>> gridItems;	

	for (unsigned i = 0; i < count; ++i)
	{
		Race::Profile* profile = menu()->GetRace()->GetProfiles()[i];

		if (profile->netGame() == netGame)
		{
			gui::Button* item = iter != _grid->GetChildren().end() ? static_cast<gui::Button*>(*iter) : NULL;
			if (item == NULL)
			{
				item = _mainMenu->CreateMenuButton(svNull, _grid, this);			
				iter = _grid->GetChildren().end();
			}
			else
				++iter;

			item->SetText(profile->GetName());
			cellSize = item->GetSize();

			gui::Button* closeBut = menu()->CreateMenuButton(svNull, "", "GUI\\buttonBg6.png", "GUI\\buttonBgSel6.png", item, this, D3DXVECTOR2(1.8f, 1.8f), gui::Button::bsSelAnim, clrWhite);
			closeBut->SetAlign(gui::Widget::waLeft);
			closeBut->SetPos(D3DXVECTOR2(cellSize.x/2 - 40.0f, 0.0f));

			gridItems.push_back(std::make_pair(item, closeBut));
			int index = itemCount;
			++itemCount;

			if (index >= 2)
			{
				Menu::NavElement navElement = {gridItems[index - 1].first, {gridItems[index - 1].second, gridItems[index - 1].second, gridItems[index - 2].first, gridItems[index].first}, {cVirtualKeyEnd, cVirtualKeyEnd}};
				navElements.push_back(navElement);

				Menu::NavElement navElement2 = {gridItems[index - 1].second, {gridItems[index - 1].first, gridItems[index - 1].first, gridItems[index - 2].second, gridItems[index].second}, {cVirtualKeyEnd, cVirtualKeyEnd}};
				navElements.push_back(navElement2);
			}
		}	

		if (i + 1 == count)
		{
			if (itemCount >= 2)
			{
				Menu::NavElement navElement = {gridItems[0].first, {gridItems[0].second, gridItems[0].second, _upArrow, gridItems[1].first}, {cVirtualKeyEnd, cVirtualKeyEnd}};
				navElements.push_back(navElement);

				Menu::NavElement navElement2 = {gridItems[0].second, {gridItems[0].first, gridItems[0].first, _upArrow, gridItems[1].second}, {cVirtualKeyEnd, cVirtualKeyEnd}};
				navElements.push_back(navElement2);
			}

			{
				Menu::NavElement navElement = {gridItems[itemCount - 1].first, {gridItems[itemCount - 1].second, gridItems[itemCount - 1].second, itemCount >= 2 ? gridItems[itemCount - 2].first : _upArrow, _downArrow}, {cVirtualKeyEnd, cVirtualKeyEnd}};
				navElements.push_back(navElement);

				Menu::NavElement navElement2 = {gridItems[itemCount - 1].second, {gridItems[itemCount - 1].first, gridItems[itemCount - 1].first, itemCount >= 2 ? gridItems[itemCount - 2].second : _upArrow, _downArrow}, {cVirtualKeyEnd, cVirtualKeyEnd}};
				navElements.push_back(navElement2);
			}
		}
	}

	gui::Widget::Children children = _grid->GetChildren();	
	while (children.size() > itemCount)
	{		
		menu()->ReleaseWidget(children.back());
		children.erase(--children.end());
	}

	_grid->cellSize(cellSize);

	{
		Menu::NavElement navElement = {_mainMenu->GetItem(miBack), {NULL, NULL, _downArrow, _upArrow}, {vkBack, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}
	{
		Menu::NavElement navElement = {_upArrow, {NULL, NULL, _mainMenu->GetItem(miBack), itemCount > 0 ? gridItems[0].first : NULL}, {cVirtualKeyEnd, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}
	{
		Menu::NavElement navElement = {_downArrow, {NULL, NULL, itemCount > 0 ? gridItems[itemCount - 1].first : NULL, _mainMenu->GetItem(miBack)}, {cVirtualKeyEnd, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}

	menu()->SetNavElements(_mainMenu->GetItem(miBack), true, &navElements[0], navElements.size());
}

void ProfileFrame::OnShow(bool value)
{
	const lsl::string menuItemsStr[cMenuItemEnd] = {_SC(svBack)};

	if (value)
	{
		_gridScroll = 0;
		_mainMenu->SetItems(menuItemsStr, cMenuItemEnd, false, this);
		UpdateGrid();
	}
}

void ProfileFrame::OnInvalidate()
{	
}

void ProfileFrame::OnAdjustLayout(const D3DXVECTOR2& vpSize)
{
	AdjustGrid(vpSize);

	_upArrow->SetPos(vpSize.x/2, vpSize.y/2 - 108.0f);
	_downArrow->SetPos(vpSize.x/2, vpSize.y/2 + 120.0f);
}

bool ProfileFrame::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
{
	if (sender == menu()->GetAcceptSender())
	{
		if (menu()->GetAcceptResultYes())
		{
			for (unsigned i = 0; i < menu()->GetRace()->GetProfiles().size(); ++i)
			{
				if (menu()->GetAcceptData() == menu()->GetRace()->GetProfiles()[i])
				{
					menu()->DelProfile(i);
					UpdateGrid();
					break;
				}
			}
		}
		return true;
	}

	if (sender == _mainMenu->GetItem(miBack))
	{
		_mainMenu->BackState();
		return true;
	}

	if (sender == _downArrow)
	{
		ScrollGrid(1);
		return true;
	}

	if (sender == _upArrow)
	{
		ScrollGrid(-1);
		return true;
	}

	unsigned count = menu()->GetRace()->GetProfiles().size();
	unsigned i = 0;
	gui::Widget::Children::const_iterator iter = _grid->GetChildren().begin();
	bool netGame = _mainMenu->ContainsState(MainMenu::msNetwork);

	for (unsigned i = 0; i < count; ++i)
	{
		Race::Profile* profile = menu()->GetRace()->GetProfiles()[i];
		if (profile->netGame() != netGame)
			continue;

		if (iter == _grid->GetChildren().end())
			break;
		gui::Button* item = static_cast<gui::Button*>(*iter);
		++iter;

		if (sender == item)
		{
			_mainMenu->StartMatch(Race::rmChampionship, cDifficultyEnd, profile);
			return true;
		}

		if (sender->GetParent() == item)
		{
			menu()->ShowAccept(GetString("svHintDeleteProfile"), GetString(svYes), GetString(svNo), uiRoot()->GetVPSize()/2, gui::Widget::waCenter, this, profile);
			return true;
		}
	}

	return false;
}




MainFrame::MainFrame(Menu* menu, MainMenu* mainMenu, gui::Widget* parent): MenuFrame(menu, parent), _mainMenu(mainMenu)
{
}

void MainFrame::OnShow(bool value)
{
	const lsl::string menuItemsStr[cMenuItemEnd] = {_SC(svSingleGame), _SC(svNetGame), _SC(svOptions), _SC(svAuthors), _SC(svExit)};

	if (value)
	{
		_mainMenu->SetItems(menuItemsStr, cMenuItemEnd, true, this);
	}
}

bool MainFrame::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
{
	if (sender == _mainMenu->GetItem(miSingle))
	{
		_mainMenu->PushState(MainMenu::msGameMode);
		return true;
	}	

	if (sender == _mainMenu->GetItem(miNetwork))
	{
		_mainMenu->PushState(MainMenu::msNetwork);
		return true;
	}	

	if (sender == _mainMenu->GetItem(miOptions))
	{
		menu()->ShowOptions(true);
		return true;
	}

	if (sender == _mainMenu->GetItem(miAuthors))
	{
		//_mainMenu->PushState(MainMenu::msCredits);
		menu()->SetState(Menu::msFinal);
		return true;
	}

	if (sender == _mainMenu->GetItem(miExit))
	{
#ifdef STEAM_SERVICE
		if (menu()->IsSteamSavingInProcess())
			menu()->ShowSteamSavingMessage();
		else
#endif
			menu()->Terminate();
		return true;
	}

	return false;
}




MainMenu::MainMenu(Menu* menu, gui::Widget* parent): _menu(menu), _state(msMain), _serverType(cServerTypeEnd), _steamHostMode(Race::rmChampionship), _steamHostDifficulty(gdNormal), _steamHostProfile(NULL), _steamHostStarting(false), _steamConnecting(false)
{
	_root = _menu->GetGUI()->CreateDummy();
	_root->SetParent(parent);

	_winFrame = _menu->CreatePlane(_root, NULL, "GUI\\mainFrame.dds", true);
	_winFrame->SetAnchor(gui::Widget::waCenter);
	_winFrame->GetMaterial().GetSampler().SetFiltering(graph::Sampler2d::sfLinear);

	_topPanel = menu->CreatePlane(_root, NULL, "GUI\\topPanel5.png", true, IdentityVec2, gui::Material::bmTransparency);
	_topPanel->SetAlign(gui::Widget::waCenter);

	_bottomPanel = menu->CreatePlane(_root, NULL, "GUI\\bottomPanel5.png", true, IdentityVec2, gui::Material::bmTransparency);
	_bottomPanel->SetAnchor(gui::Widget::waBottom);
	_bottomPanel->SetAlign(gui::Widget::waBottom);
	_bottomPanel->SetVisible(false);

	_version = menu->CreateLabel("v. 1.2.0", _root, "VerySmall", NullVec2, gui::Text::haRight, gui::Text::vaBottom, clrWhite);

	_mainFrame = new MainFrame(menu, this, _root);
	_gameMode = new GameModeFrame(menu, this, _root);
	_network = new NetworkFrame(menu, this, _root);
	_netBrowser = new NetBrowserFrame(menu, this, _root);		
	_profile = new ProfileFrame(menu, this, _root);	
	_tournament = new TournamentFrame(menu, this, _root);
	_credits = new CreditsFrame(menu, this, _root);	
	_difficultyFrame = new DifficultyFrame(menu, this, _root);	
	_netIPAddressFrame = new NetIPAddressFrame(menu, this, _root);	
	_serverTypeFrame = new ServerTypeFrame(menu, this, _root);
	_clientTypeFrame = new ClientTypeFrame(menu, this, _root);

#ifdef STEAM_SERVICE
	_matchmakingFrame = new MatchmakingFrame(menu, this, _root);
	_lobbyFrame = new LobbyFrame(menu, this, _root);
	_steamBrowser = new SteamBrowserFrame(menu, this, _root);
#endif

	ApplyState(_state);

	_menu->RegUser(this);
	_menu->GetNet()->RegUser(this);
}

MainMenu::~MainMenu()
{
	_menu->UnregUser(this);
	_menu->GetNet()->UnregUser(this);

	if (_menuItems.size() > 0)
		_menu->UnregNavElements(_menuItems[0]);

	for (unsigned i = 0; i < _menuItems.size(); ++i)
		_menu->ReleaseWidget(_menuItems[i]);

#ifdef STEAM_SERVICE
	delete _steamBrowser;
	delete _lobbyFrame;
	delete _matchmakingFrame;
#endif
	
	delete _clientTypeFrame;
	delete _serverTypeFrame;	
	delete _netIPAddressFrame;
	delete _difficultyFrame;
	delete _credits;
	delete _tournament;	
	delete _profile;
	delete _netBrowser;
	delete _network;
	delete _gameMode;
	delete _mainFrame;

	_menu->ReleaseWidget(_version);
	_menu->ReleaseWidget(_bottomPanel);
	_menu->ReleaseWidget(_topPanel);
	_menu->GetGUI()->ReleaseWidget(_winFrame);

	_menu->GetGUI()->ReleaseWidget(_root);
}

void MainMenu::ApplyState(State state)
{
	if (state == msMain || state == msNetwork)
		_serverType = cServerTypeEnd;

	_mainFrame->Show(state == msMain);
	_gameMode->Show(state == msGameMode);
	_network->Show(state == msNetwork);	
	_netBrowser->Show(state == msNetBrowser);	
	_profile->Show(state == msLoad);	
	_tournament->Show(state == msTournament);
	_credits->Show(state == msCredits);	
	_difficultyFrame->Show(state == msDifficulty);	
	_netIPAddressFrame->Show(state == msNetIPAdress);	
	_serverTypeFrame->Show(state == msServerType);
	_clientTypeFrame->Show(state == msClientType);	

#ifdef STEAM_SERVICE
	_matchmakingFrame->Show(state == msMatchmaking);
	_lobbyFrame->Show(state == msLobby);
	_steamBrowser->Show(state == msSteamBrowser);
#endif
}

void MainMenu::AdjustMenuItems(const D3DXVECTOR2& vpSize)
{
	if (_menuItems.size() > 0)
	{
		int index = 0;

		for (unsigned i = 0; i < _menuItems.size(); ++i)
		{
			if (!_menuItems[i]->GetVisible())
				continue;

			_menuItems[i]->SetPos(vpSize.x/2 + 5, vpSize.y/2 - 100.0f + index * (_menuItems[i]->GetSize().y + 5));
			++index;
		}

		if (_state != msMain)
			_menuItems[_menuItems.size() - 1]->SetPos(vpSize.x/2 + 5, vpSize.y/2 + 150.0f);
	}
}

void MainMenu::OnProcessEvent(unsigned id, EventData* data)
{
#ifdef STEAM_SERVICE
	switch (id)
	{
	case cSteamCreateLobby:
		_menu->HideMessage();

		if (data->success)
		{
			SetState(MainMenu::msLobby);
		}
		else
			_menu->ShowMessage(_menu->GetString(svWarning), _menu->GetString("svSteamConnectFailed"), _menu->GetString(svOk), _menu->GetGUI()->GetVPSize()/2, gui::Widget::waCenter, 0.0f);
		break;

	case cSteamHostCreated:
		if (_steamHostStarting)
		{
			if (data->success)
				_menu->GetSteamService()->InitiateConnection(_menu->GetSteamService()->server()->steamId());
			else
				_menu->ShowMessage(_menu->GetString(svWarning), _menu->GetString("svSteamConnectFailed"), _menu->GetString(svOk), _menu->GetGUI()->GetVPSize()/2, gui::Widget::waCenter, 0.0f);
		}
		break;

	case cSteamLobbyGameCreated:
		if (data->success)
		{
			ConnectMatch(net::Endpoint(), ((SteamLobby::MyEventData*)data)->steamId);
		}
		else
		{
			_menu->ShowMessage(_menu->GetString(svWarning), _menu->GetString("svSteamConnectFailed"), _menu->GetString(svOk), _menu->GetGUI()->GetVPSize()/2, gui::Widget::waCenter, 0.0f);
			BackState();
		}
		break;

	case cSteamAuthEnded:
		if (_steamHostStarting)
		{
			_steamHostStarting = false;
			_menu->HideMessage();

			if (data->success)
				_menu->StartMatch(_steamHostMode, _steamHostDifficulty, _steamHostProfile, true, _menu->GetSteamService()->acceptor());
			else
				_menu->ShowMessage(_menu->GetString(svWarning), _menu->GetString("svSteamConnectFailed"), _menu->GetString(svOk), _menu->GetGUI()->GetVPSize()/2, gui::Widget::waCenter, 0.0f);
		}
		else if (_steamConnecting)
		{
			if (data->success)
				_menu->ConnectMatch(net::Endpoint(), true, _menu->GetSteamService()->acceptor());
			else
				_menu->ShowMessage(_menu->GetString(svWarning), _menu->GetString("svSteamConnectFailed"), _menu->GetString(svOk), _menu->GetGUI()->GetVPSize()/2, gui::Widget::waCenter, 0.0f);

		}
		break;
	}
#endif
}

void MainMenu::OnDisconnected(net::INetConnection* sender)
{
	_steamConnecting = false;
	_menu->HideMessage();	

	_menu->ShowMessage(_menu->GetString(svWarning), _menu->GetString(svHintHostConnectionFailed), _menu->GetString(svOk), _menu->GetGUI()->GetVPSize()/2, gui::Widget::waCenter, 0.0f);
}

void MainMenu::OnConnectionFailed(net::INetConnection* sender, unsigned error)
{
	_steamConnecting = false;
	_menu->HideMessage();

	_menu->ShowMessage(_menu->GetString(svWarning), _menu->GetString(svHintHostConnectionFailed), _menu->GetString(svOk), _menu->GetGUI()->GetVPSize()/2, gui::Widget::waCenter, 0.0f);
}

void MainMenu::OnConnectedPlayer(NetPlayer* sender)
{
	if (sender->owner())
	{
		_steamConnecting = false;
		_menu->HideMessage();

		_menu->MatchConnected();
	}
}

void MainMenu::OnDisconnectedPlayer(NetPlayer* sender)
{
	_steamConnecting = false;
	_menu->HideMessage();
}

void MainMenu::AdjustLayout(const D3DXVECTOR2& vpSize)
{
	AdjustMenuItems(vpSize);

	_topPanel->SetPos(D3DXVECTOR2(vpSize.x/2, (0 + vpSize.y/2 - 150.0f)/2.0f));

	_version->SetPos(D3DXVECTOR2(vpSize.x - 25.0f, vpSize.y - 25.0f));

	_mainFrame->AdjustLayout(vpSize);
	_gameMode->AdjustLayout(vpSize);
	_network->AdjustLayout(vpSize);
	_netBrowser->AdjustLayout(vpSize);
	_profile->AdjustLayout(vpSize);	
	_tournament->AdjustLayout(vpSize);
	_credits->AdjustLayout(vpSize);	
	_difficultyFrame->AdjustLayout(vpSize);	
	_netIPAddressFrame->AdjustLayout(vpSize);		
	_serverTypeFrame->AdjustLayout(vpSize);
	_clientTypeFrame->AdjustLayout(vpSize);

#ifdef STEAM_SERVICE
	_matchmakingFrame->AdjustLayout(vpSize);
	_lobbyFrame->AdjustLayout(vpSize);
	_steamBrowser->AdjustLayout(vpSize);
#endif
}

void MainMenu::Show(bool value)
{
	GetRoot()->SetVisible(value);
	_stateStack.clear();
}

void MainMenu::OnProgress(float deltaTime)
{
	if (_credits->visible())
		_credits->OnProgress(deltaTime);
}

gui::Widget* MainMenu::GetRoot()
{
	return _root;
}

MainMenu::State MainMenu::GetState() const
{
	return _state;
}

void MainMenu::SetState(State value)
{
	if (_state != value)
	{		
		_state = value;
		ApplyState(_state);
	}
}

void MainMenu::PushState(State value)
{
	_stateStack.push_back(value);
	SetState(value);
}

void MainMenu::BackState()
{
	if (_stateStack.size() > 0)
		_stateStack.pop_back();

	if (_stateStack.size() > 0)
		SetState(_stateStack.back());
	else
		SetState(msMain);
}

void MainMenu::ClearStates()
{
	_stateStack.clear();
}

bool MainMenu::ContainsState(State value) const
{
	for (StateStack::const_iterator iter = _stateStack.begin(); iter != _stateStack.end(); ++iter)
		if (*iter == value)
			return true;
	return false;
}

MainMenu::ServerType MainMenu::GetServerType() const
{
	return _serverType;
}

void MainMenu::SetServerType(ServerType value)
{
	_serverType = value;
}

gui::Button* MainMenu::GetItem(int index)
{
	return _menuItems[index];
}

void MainMenu::SetItems(const lsl::string menuItemsStr[], unsigned count, bool mainMenu, gui::Widget::Event* guiEvent)
{
	if (_menuItems.size() > 0)
		_menu->UnregNavElements(_menuItems[0]);

	lsl::Vector<Menu::NavElement> navElements;

	for (unsigned i = 0; i < count; ++i)
	{
		gui::Button* item = i < _menuItems.size() ? _menuItems[i] : NULL;

		if (item == NULL)
		{
			item = CreateMenuButton(_menu->GetString(menuItemsStr[i]), _root, guiEvent);
			item->SetAlign(gui::Widget::waTop);
			_menuItems.push_back(item);
		}
		else
		{
			item->SetText(_menu->GetString(menuItemsStr[i]));

			item->ClearEvents();
			item->RegEvent(_menu->GetSoundSheme(Menu::ssButton1));
			if (guiEvent)
				item->RegEvent(guiEvent);

			item->SetVisible(true);
			_menu->SetButtonEnabled(item, true);
		}

		if (i >= 2)
		{
			Menu::NavElement navElement = {_menuItems[i - 1], {NULL, NULL, _menuItems[i - 2], _menuItems[i]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
			navElements.push_back(navElement);
		}

		if (i + 1 == count)
		{
			if (count >= 2)
			{
				Menu::NavElement navElement = {_menuItems[0], {NULL, NULL, _menuItems[i], _menuItems[1]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
				navElements.push_back(navElement);
			}

			Menu::NavElement navElement2 = {_menuItems[i], {NULL, NULL, count >= 2 ? _menuItems[i - 1] : NULL, count >= 2 ? _menuItems[0] : NULL}, {!mainMenu ? vkBack : cVirtualKeyEnd, cVirtualKeyEnd}};
			navElements.push_back(navElement2);
		}
	}

	if (_menuItems.size() > count)
	{
		for (unsigned i = count; i < _menuItems.size(); ++i)
			_menu->ReleaseWidget(_menuItems[i]);
		_menuItems.erase(_menuItems.begin() + count, _menuItems.end());
	}

	InvalidateItems();

	if (count > 0)
		_menu->SetNavElements(_menuItems[0], true, &navElements[0], navElements.size());
}

void MainMenu::InvalidateItems()
{
	AdjustMenuItems(_menu->GetGUI()->GetVPSize());
}

void MainMenu::StartMatch(Race::Mode mode, Difficulty difficulty, Race::Profile* profile, bool startLobby)
{
	_steamHostMode = mode;
	_steamHostDifficulty = difficulty;
	_steamHostProfile = profile;

#ifdef STEAM_SERVICE
	if (startLobby)
	{
		if (_menu->GetSteamService()->isInit() && _menu->GetSteamService()->lobby()->StartMath())
		{
			_steamHostStarting = true;
			_menu->ShowMessageLoading();
		}
	}
	else if (_serverType == stLobby)
	{
		if (_menu->GetSteamService()->isInit())
		{
			_menu->GetSteamService()->lobby()->CreateLobby(k_ELobbyTypePublic, Race::cMaxPlayers);
			_menu->ShowMessageLoading();
		}
	}
	else if (_serverType == stSteam)
	{
		if (_menu->GetSteamService()->isInit() && _menu->GetSteamService()->server()->CreateHost())
		{
			_steamHostStarting = true;
			_menu->ShowMessageLoading();
		}
	}
	else
#endif
	{
		_menu->StartMatch(mode, difficulty, profile, ContainsState(msNetwork));
	}
}

void MainMenu::ConnectMatch(const net::Endpoint& endpoint
#ifdef STEAM_SERVICE
							, const CSteamID& steamId
#endif
							)
{
#ifdef STEAM_SERVICE
	if (steamId.IsValid() && _menu->GetSteamService()->isInit() && _menu->GetSteamService()->InitiateConnection(steamId))
	{
		_steamConnecting = true;
		_menu->ShowMessageLoading();
	}
	else 
#endif
		if (_menu->ConnectMatch(endpoint, true))
		{
			_steamConnecting = true;
			_menu->ShowMessageLoading();
		}
}

Race::Mode MainMenu::steamHostMode() const
{
	return _steamHostMode;
}

Difficulty MainMenu::steamHostDifficulty() const
{
	return _steamHostDifficulty;
}

Race::Profile* MainMenu::steamHostProfile() const
{
	return _steamHostProfile;
}

gui::Button* MainMenu::CreateMenuButton(const lsl::string& name, gui::Widget* parent, gui::Widget::Event* guiEvent)
{
	const D3DXCOLOR color1 = D3DXCOLOR(0xffff8a70);

	gui::Button* button = _menu->CreateMenuButton(name, "Header", "", "GUI\\mainItemSel5.png", parent, guiEvent, IdentityVec2, gui::Button::bsSimple, color1, Menu::ssButton1);
	button->SetSize(button->GetSelSize());
	button->GetOrCreateTextSelMaterial()->SetColor(clrWhite);

	return button;
}

gui::Button* MainMenu::CreateMenuButton(StringValue name, gui::Widget* parent, gui::Widget::Event* guiEvent)
{
	return CreateMenuButton(_menu->GetString(name), parent, guiEvent);
}

}

}

}