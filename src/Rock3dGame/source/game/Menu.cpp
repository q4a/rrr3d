#include "stdafx.h"
#include "game\World.h"

#include "game\Menu.h"

namespace r3d
{

namespace game
{

const D3DXCOLOR Menu::cTextColor = D3DXCOLOR(176.0f, 205.0f, 249.0f, 255.0f)/255.0f;
const D3DXVECTOR2 Menu::cWinSize(1280.0f, 1024.0f);
const D3DXVECTOR2 Menu::cMinWinSize(800.0f, 600.0f);
const D3DXVECTOR2 Menu::cMaxWinSize(3072.0f, 1536.0f);




Menu::Menu(GameMode* game): _game(game), _state(msMain2), _loadingVisible(false), _optionsVisible(false), _startOptionsVisible(false), _screenFon(0), _mainMenu2(0), _raceMenu2(0), _hudMenu(0), _finishMenu2(NULL), _finalMenu(NULL), _infoMenu(0), _optionsMenu(NULL), _startOptionsMenu(NULL), _weaponTime(-1), _messageTime(-1), _musicTime(-1)
{
	_controlEvent = new MyControlEvent(this);
	_game->GetWorld()->GetControl()->InsertEvent(_controlEvent);

	_disconnectEvent = new MyDisconnectEvent(this);
	_syncModeEvent = new MySyncModeEvent(this);
	_steamErrorEvent = new MySteamErrorEvent(this);
	_steamSavingEvent = new MySteamSavingEvent(this);

	LSL_LOG("menu create cursor");

	_cursor = CreatePlane(GetGUI()->GetRoot(), 0, "GUI\\cursor.png", true, IdentityVec2, gui::Material::bmTransparency);
	_cursor->SetVisible(false);	
	_cursor->SetFlag(gui::Widget::wfTopmost, true);
	_cursor->SetTopmostLevel(MenuFrame::cTopmostCursor);	

	_audioSource = GetWorld()->GetLogic()->CreateSndSource(Logic::scEffects);	

	for (int i = 0; i < cSoundShemeTypeEnd; ++i)
		_soundShemes[i] = new SoundSheme(this);

	_soundShemes[ssButton1]->clickDown(GetSound("Sounds\\UI\\click.ogg"));

	_soundShemes[ssButton2]->clickDown(GetSound("Sounds\\UI\\click.ogg"));
	_soundShemes[ssButton2]->mouseEnter(GetSound("Sounds\\UI\\navedenie.ogg"));

	_soundShemes[ssButton3]->clickDown(GetSound("Sounds\\UI\\pickup_down.ogg"));

	_soundShemes[ssButton4]->clickDown(GetSound("Sounds\\UI\\repaint.ogg"));

	_soundShemes[ssButton5]->clickDown(GetSound("Sounds\\UI\\showPlanet.ogg"));
	_soundShemes[ssButton5]->focused(GetSound("Sounds\\UI\\showPlanet.ogg"));

	_soundShemes[ssStepper]->selectItem(GetSound("Sounds\\UI\\changeOption.ogg"));	

	LSL_LOG("menu create dialogs");

	_acceptDlg = new n::AcceptDialog(this, GetGUI()->GetRoot());
	_weaponDlg = new n::WeaponDialog(this, GetGUI()->GetRoot());
	_messageDlg = new n::InfoDialog(this, GetGUI()->GetRoot());
	_musicDlg = new n::MusicDialog(this, GetGUI()->GetRoot());
	_userChat = new n::UserChat(this, GetGUI()->GetRoot());

	ApplyState(_state);

	_game->RegUser(this);
	GetNet()->RegUser(this);
}
	
Menu::~Menu()
{
	_game->GetWorld()->GetControl()->RemoveEvent(_controlEvent);
	GetNet()->UnregUser(this);
	_game->UnregUser(this);

	HideAccept();
	HideMessage();
	
	SetFinalMenu(false);
	SetFinishMenu2(false);
	SetOptionsMenu(false);
	SetStartOptionsMenu(false);
	SetScreenFon(false);
	SetMainMenu2(false);
	SetRaceMenu2(false);
	SetHudMenu(false);
	SetInfoMenu(false);

	delete _userChat;
	delete _musicDlg;
	delete _messageDlg;
	delete _weaponDlg;
	delete _acceptDlg;

	for (int i = 0; i < cSoundShemeTypeEnd; ++i)
		delete _soundShemes[i];

	GetWorld()->GetLogic()->ReleaseSndSource(_audioSource);
	
	GetGUI()->ReleaseWidget(_cursor);

	delete _controlEvent;
	delete _disconnectEvent;
	delete _syncModeEvent;
	delete _steamErrorEvent;
	delete _steamSavingEvent;
}

Menu::MyControlEvent::MyControlEvent(Menu* menu): _menu(menu)
{
}

bool Menu::MyControlEvent::OnMouseMoveEvent(const MouseMove& mMove)
{
	return _menu->OnMouseMoveEvent(mMove);
}

bool Menu::MyControlEvent::OnHandleInput(const InputMessage& msg)
{
	return _menu->OnHandleInput(msg);
}

Menu::MyDisconnectEvent::MyDisconnectEvent(Menu* menu): _menu(menu)
{
}

bool Menu::MyDisconnectEvent::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
{
	if (sender == _menu->GetMessageSender())
	{
		//_menu->ShowCursor(false);
		_menu->Pause(false);

		_menu->ExitRace();
		_menu->ExitMatch(true);
		return true;
	}

	return false;
}

Menu::MySyncModeEvent::MySyncModeEvent(Menu* menu): _menu(menu)
{
}

bool Menu::MySyncModeEvent::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
{
	if (sender == _menu->GetAcceptSender())
	{
		if (_menu->GetAcceptResultYes())
			_menu->GetEnv()->SetSyncFrameRate(Environment::sfrNone);
		return true;
	}

	return false;
}

Menu::MySteamErrorEvent::MySteamErrorEvent(Menu* menu): _menu(menu)
{
}

bool Menu::MySteamErrorEvent::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
{
	if (sender == _menu->GetMessageSender())
	{
#ifdef _RETAIL
		_menu->GetGame()->GetWorld()->Terminate(EXIT_FAILURE);
#else
		_menu->_game->CheckStartupMenu();
#endif
		return true;
	}

	return false;
}

Menu::MySteamSavingEvent::MySteamSavingEvent(Menu* menu): _menu(menu)
{
}

bool Menu::MySteamSavingEvent::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
{
	if (sender == _menu->GetAcceptSender())
	{
		if (_menu->GetAcceptResultYes())
			_menu->Terminate();
#ifdef STEAM_SERVICE
		else if (_menu->IsSteamSavingInProcess())
			_menu->GetSteamService()->Sync(0.0f, true);
#endif
		return true;
	}

	return false;
}

Menu::SoundSheme::SoundSheme(Menu* menu): _menu(menu), _clickDown(NULL), _mouseEnter(NULL), _selectItem(NULL), _focused(NULL)
{
}

Menu::SoundSheme::~SoundSheme()
{
	clickDown(NULL);
	mouseEnter(NULL);
	selectItem(NULL);
	focused(NULL);
}

bool Menu::SoundSheme::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
{
	if (mClick.state == ksDown)
	{
		_menu->PlaySound(_clickDown);
		return false;
	}

	return false;
}

bool Menu::SoundSheme::OnMouseEnter(gui::Widget* sender, const gui::MouseMove& mMove)
{
	_menu->PlaySound(_mouseEnter);

	return false;
}

void Menu::SoundSheme::OnMouseLeave(gui::Widget* sender, bool wasReset)
{
}

bool Menu::SoundSheme::OnMouseOver(gui::Widget* sender, const gui::MouseMove& mMove)
{
	return false;
}

bool Menu::SoundSheme::OnSelect(gui::Widget* sender, Object* item)
{
	_menu->PlaySound(_selectItem);

	return false;
}

void Menu::SoundSheme::OnFocusChanged(gui::Widget* sender)
{
	if (sender->IsFocused())
		_menu->PlaySound(_focused);
}

snd::Sound* Menu::SoundSheme::clickDown() const
{
	return _clickDown;
}

void Menu::SoundSheme::clickDown(snd::Sound* value)
{
	if (ReplaceRef(_clickDown, value))
		_clickDown = value;
}

snd::Sound* Menu::SoundSheme::mouseEnter() const
{
	return _mouseEnter;
}

void Menu::SoundSheme::mouseEnter(snd::Sound* value)
{
	if (ReplaceRef(_mouseEnter, value))
		_mouseEnter = value;
}

snd::Sound* Menu::SoundSheme::selectItem() const
{
	return _selectItem;
}

void Menu::SoundSheme::selectItem(snd::Sound* value)
{
	if (ReplaceRef(_selectItem, value))
		_selectItem = value;
}

snd::Sound* Menu::SoundSheme::focused() const
{
	return _focused;
}

void Menu::SoundSheme::focused(snd::Sound* value)
{
	if (ReplaceRef(_focused, value))
		_focused = value;
}

D3DXVECTOR2 Menu::GetImageSize(gui::Material& material)
{
	return material.GetSampler().GetSize();
}

D3DXVECTOR2 Menu::GetAspectSize(const D3DXVECTOR2& curSize, const D3DXVECTOR2& newSize)
{
	float wScale = newSize.x / curSize.x;
	float hScale = newSize.y / curSize.y;
	float minScale = std::min(wScale, hScale);

	return curSize * minScale;
}

D3DXVECTOR2 Menu::GetImageAspectSize(gui::Material& material, const D3DXVECTOR2& newSize)
{
	return GetAspectSize(GetImageSize(material), newSize);
}

D3DXVECTOR2 Menu::StretchImage(D3DXVECTOR2 imageSize, const D3DXVECTOR2& size, bool keepAspect, bool fillRect, bool scaleDown, bool scaleUp)
{
	imageSize.x = std::max(imageSize.x, 1.0f);
	imageSize.y = std::max(imageSize.y, 1.0f);

	D3DXVECTOR2 scale = IdentityVec2;

	if (keepAspect)
	{
		if (fillRect)
			scale.x = scale.y = std::max(size.x / imageSize.x, size.y / imageSize.y);
		else
			scale.x = scale.y = std::min(size.x / imageSize.x, size.y / imageSize.y);
	}
	else
	{
		scale.x = size.x / imageSize.x;
		scale.y = size.y / imageSize.y;
	}

	if (!scaleDown)
	{
		scale.x = std::max(scale.x, 1.0f);
		scale.y = std::max(scale.y, 1.0f);
	}
	if (!scaleUp)
	{
		scale.x = std::min(scale.x, 1.0f);
		scale.y = std::min(scale.y, 1.0f);
	}

	return scale * imageSize;
}

D3DXVECTOR2 Menu::StretchImage(gui::Material& material, const D3DXVECTOR2& size, bool keepAspect, bool fillRect, bool scaleDown, bool scaleUp)
{
	return StretchImage(GetImageSize(material), size, keepAspect, fillRect, scaleDown, scaleUp);
}

D3DXQUATERNION Menu::GetIsoRot()
{
	D3DXQUATERNION rotZ;
	D3DXQuaternionRotationAxis(&rotZ, &ZVector, -2.0f * D3DX_PI/3.0f);
	D3DXQUATERNION rotY;
	D3DXQuaternionRotationAxis(&rotY, &YVector, 0);
	D3DXQUATERNION rotX;
	D3DXQuaternionRotationAxis(&rotX, &XVector, -D3DX_PI/3.0f);

	return rotZ * rotY * rotX;
}

void Menu::SetScreenFon(bool init)
{
	if (init && !_screenFon)
	{
		_screenFon = GetGUI()->CreatePlaneFon();
		_screenFon->GetMaterial().SetColor(clrBlack);
	} 
	else if (!init && _screenFon)
	{
		GetGUI()->ReleaseWidget(_screenFon);
		_screenFon = 0;
	}
}

void Menu::SetMainMenu2(bool init)
{
	if (init && !_mainMenu2)
	{
		_mainMenu2 = new n::MainMenu(this, GetGUI()->GetRoot());
		_mainMenu2->Show(true);
		_mainMenu2->SetState(n::MainMenu::msMain);
	} 
	else if (!init && _mainMenu2)
	{
		lsl::SafeDelete(_mainMenu2);
	}
}

void Menu::SetRaceMenu2(bool init)
{
	if (init && !_raceMenu2)
	{
		LSL_ASSERT(GetPlayer());

		_raceMenu2 = new n::RaceMenu(this, GetGUI()->GetRoot(), GetPlayer());
		_raceMenu2->Show(true);
		_raceMenu2->SetState(n::RaceMenu::msMain);

	}
	else if (!init && _raceMenu2)
	{
		lsl::SafeDelete(_raceMenu2);
	}
}

void Menu::SetHudMenu(bool init)
{
	if (init && !_hudMenu)
	{
		LSL_ASSERT(GetPlayer());

		_hudMenu = new HudMenu(this, GetGUI()->GetRoot(), GetPlayer());
		_hudMenu->Show(true);
		_hudMenu->SetState(HudMenu::msMain);
	}
	else if (!init && _hudMenu)
	{
		lsl::SafeDelete(_hudMenu);
	}
}

void Menu::SetFinishMenu2(bool init)
{
	if (init && !_finishMenu2)
	{
		_finishMenu2 = new n::FinishMenu(this, GetGUI()->GetRoot());
		_finishMenu2->Show(true);
	}
	else if (!init && _finishMenu2)
	{
		lsl::SafeDelete(_finishMenu2);
	}
}

void Menu::SetFinalMenu(bool init)
{
	if (init && !_finalMenu)
	{
		_finalMenu = new FinalMenu(this, GetGUI()->GetRoot());
		_finalMenu->Show(true);
	}
	else if (!init && _finalMenu)
	{
		lsl::SafeDelete(_finalMenu);
	}
}

void Menu::SetInfoMenu(bool init)
{
	if (init && !_infoMenu)
	{
		_infoMenu = new n::InfoMenu(this, GetGUI()->GetRoot());
		_infoMenu->SetState(n::InfoMenu::msLoading);
		_infoMenu->Show(true);
		_infoMenu->GetRoot()->ShowModal(true);
	} 
	else if (!init && _infoMenu)
	{
		lsl::SafeDelete(_infoMenu);
	}
}

void Menu::SetOptionsMenu(bool init)
{
	if (init && !_optionsMenu)
	{
		_optionsMenu = new n::OptionsMenu(this, GetGUI()->GetRoot());
		_optionsMenu->Show(true);
		_optionsMenu->SetState(n::OptionsMenu::msGame);

		_optionsMenu->GetRoot()->ShowModal(true);
		_optionsMenu->GetRoot()->SetFlag(gui::Widget::wfTopmost, true);
		_optionsMenu->GetRoot()->SetTopmostLevel(MenuFrame::cTopmostDef);
	}
	else if (!init && _optionsMenu)
	{
		_optionsMenu->GetRoot()->ShowModal(false);
		lsl::SafeDelete(_optionsMenu);
	}
}

void Menu::SetStartOptionsMenu(bool init)
{
	if (init && !_startOptionsMenu)
	{
		_startOptionsMenu = new n::StartOptionsMenu(this, GetGUI()->GetRoot());		
		_startOptionsMenu->ShowModal(true);
	}
	else if (!init && _startOptionsMenu)
	{
		_startOptionsMenu->ShowModal(false);
		lsl::SafeDelete(_startOptionsMenu);
	}
}

void Menu::ApplyState(State state)
{
	bool guiMode = state != msHud && state != msRace2;

	HideAccept();
	HideMusicInfo();
	//ShowChat(false);

	GetWorld()->GetEnv()->SetGUIMode(guiMode);
	GetGUI()->SetInvertY(state == msInfo);
	ShowCursor(state != msHud && state != msInfo);

	SetScreenFon(guiMode);	
	SetMainMenu2(state == msMain2);	
	SetRaceMenu2(state == msRace2);
	SetHudMenu(state == msHud);	
	SetFinishMenu2(state == msFinish2);
	SetFinalMenu(state == msFinal);
	SetInfoMenu(state == msInfo || _loadingVisible);
	SetOptionsMenu(_optionsVisible);
	SetStartOptionsMenu(_startOptionsVisible);

	AdjustLayout(GetGUI()->GetVPSize());
}

Menu::NavElement Menu::NavElementFind(const NavElements& elements, const NavElement& element, lsl::List<gui::Widget*> ignoreList, NavDir navDir)
{
	for (int j = 0; j < cNavDirEnd; ++j)
	{
		int i = j;
		switch (j)
		{
		case 0:
			i = navDir;
			break;
		case 1:
			i = navDir == ndLeft ? ndRight : (navDir == ndRight ? ndLeft : (navDir == ndUp ? ndDown : ndUp));
			break;
		case 2:
			i = navDir == ndLeft ? ndUp : (navDir == ndRight ? ndDown : (navDir == ndUp ? ndLeft : ndRight));
			break;
		case 3:
			i = navDir == ndLeft ? ndDown : (navDir == ndRight ? ndUp : (navDir == ndUp ? ndRight : ndLeft));
			break;
		}

		if (element.nextWidget[i] == NULL)
			continue;

		if (ignoreList.size() > 0 && element.nextWidget[i] == ignoreList.front() && navDir != cNavDirEnd &&
			(((i == ndLeft || i == ndRight) && (navDir == ndLeft || navDir == ndRight)) ||
			((i == ndUp || i == ndDown) && (navDir == ndUp || navDir == ndDown))))
		{
			NavElements::const_iterator iter = elements.find(element.widget);
			if (iter != elements.end() && element.widget->GetVisible() && element.widget->GetEnabled())
				return iter->second;
		}

		if (ignoreList.IsFind(element.nextWidget[i]))
			continue;

		NavElements::const_iterator iter = elements.find(element.nextWidget[i]);
		if (iter == elements.end())
			continue;

		if (navDir == cNavDirEnd ||
			((i == ndLeft || i == ndRight) && (navDir == ndLeft || navDir == ndRight)) ||
			((i == ndUp || i == ndDown) && (navDir == ndUp || navDir == ndDown)))
		{
			if (element.nextWidget[i]->GetVisible() && element.nextWidget[i]->GetEnabled())
				return iter->second;
		}

		ignoreList.push_back(element.widget);
		NavElement navElement = NavElementFind(elements, iter->second, ignoreList, navDir);

		if (navElement.widget)
			return navElement;
	}

	NavElement navElement;
	ZeroMemory(&navElement, sizeof(navElement));

	return navElement;
}

void Menu::DoPlayFinal()
{
	Planet* nextPlanet = GetGame()->GetRace()->GetTournament().GetNextPlanet();

	if (GetRace()->GetPlanetChampion() && (nextPlanet == NULL || nextPlanet->GetIndex() > Race::cTournamentPlanetCount - 1))
	{
#ifdef STEAM_SERVICE
		if (GetSteamService()->isInit())
		{
			if (!_game->GetRace()->GetCarChanged())
				GetSteamService()->steamStats()->UnlockAchievment(SteamStats::atOldSchool);

			Difficulty diff = _game->GetRace()->GetMinDifficulty();
			switch (diff)
			{
			case gdEasy:
				GetSteamService()->steamStats()->UnlockAchievment(SteamStats::atEasyChampion);
				break;
			case gdNormal:
				GetSteamService()->steamStats()->UnlockAchievment(SteamStats::atNormalChampion);
				break;
			case gdHard:
				GetSteamService()->steamStats()->UnlockAchievment(SteamStats::atHardChampion);
				break;
			}
		}
#endif

		ExitMatch();
		SetState(msFinal);
	}
}

void Menu::OnDisconnectedPlayer(NetPlayer* sender)
{
	if (sender->owner() && GetNet()->isClient() && _state != msMain2)
	{		
		ShowMessage(GetString(svWarning), GetString(svHintDisconnect), GetString(svOk), GetGUI()->GetVPSize()/2.0f, gui::Widget::waCenter, 0.0f, _disconnectEvent);

		ShowCursor(true);
		Pause(true);
	}
}

void Menu::OnFailed(unsigned error)
{
	ShowMessage(GetString(svWarning), GetString(svCriticalNetError), GetString(svOk), GetGUI()->GetVPSize()/2.0f, gui::Widget::waCenter, 0.0f, _disconnectEvent);

	ShowCursor(true);
	Pause(true);
}

void Menu::OnProcessNetEvent(unsigned id, NetEventData* data)
{
	if (id == cNetRacePushLine)
	{
		NetRace::MyEventData* myData = (NetRace::MyEventData*)data;
		NetPlayer* plr = GetNet()->GetPlayerByOwnerId(myData->senderPlayer);
		lsl::string name = "<" + (plr ? GetString(plr->model()->GetName()) : "");

		_userChat->PushLine(lsl::ConvertStrAToW(name, CP_THREAD_ACP), myData->text, plr ? plr->model()->GetColor() : clrWhite);
	}
}

bool Menu::OnMouseMoveEvent(const MouseMove& mMove)
{
	D3DXVECTOR2 pos = GetGUI()->ScreenToView(mMove.coord);

	pos.x += _cursor->GetSize().x/2;

	if (GetGUI()->GetInvertY())
		pos.y -= _cursor->GetSize().y/2;		
	else
		pos.y += _cursor->GetSize().y/2;		

	_cursor->SetPos(pos);

	return false;
}

bool Menu::OnHandleInput(const InputMessage& msg)
{
	if (msg.state != ksDown || GetWorld()->GetVideoMode())
		return false;

	if (msg.controller == ctKeyboard && msg.key == vkStart && IsChatVisible())
	{
		if (IsChatInputVisible())
		{
			lsl::string name = "<" + (GetPlayer() ? GetString(GetPlayer()->GetName()) : "");
			lsl::stringW inputText = _userChat->inputText();

			if (inputText.size() > 0)
			{
				_userChat->PushLine(lsl::ConvertStrAToW(name, CP_THREAD_ACP), inputText, GetPlayer() ? GetPlayer()->GetColor() : clrWhite);

				if (IsNetGame())
					GetNet()->race()->PushLine(inputText);

				/*if (!IsNetGame())
				{	
					GameObject* car = GetPlayer() ? GetPlayer()->GetCar().gameObj : NULL;

					if (car && inputText == L"invulnerable")
					{
						car->Immortal(60 * 10);
					}

					if (inputText == L"millionaire")
					{
						GetPlayer()->SetMoney(999999);

						SendEvent(cRaceMenuInvalidate, NULL);
					}

					if (inputText == L"champion")
					{
						GetPlayer()->SetPoints(999999);

						SendEvent(cRaceMenuInvalidate, NULL);
					}
				}*/
			}
		}

		ShowChatInput(!IsChatInputVisible());
		return true;
	}

	if (msg.key == vkChar && IsChatInputVisible())
	{
		_userChat->CharInput(msg.unicode);
	}

	if (_navElementsList.size() > 0)
	{
		VirtualKey dirKeys[cNavDirEnd] = {vkLeft, vkRight, vkUp, vkDown};
		NavElement curNavElement;
		ZeroMemory(&curNavElement, sizeof(curNavElement));	

		NavElementsList::const_iterator navElementsIter = _navElementsList.end();
		--navElementsIter;

		for (NavElements::const_iterator navIter = navElementsIter->second.begin(); navIter != navElementsIter->second.end(); ++navIter)
		{
			gui::Widget* widget = navIter->first;
			const NavElement& navElement = navIter->second;

			if (!widget->GetVisible() || !widget->GetEnabled())
			{
				widget->SetFocused(false, true);
				continue;
			}

			for (int i = 0; i < 2; ++i)
			{
				if (navElement.actionKeys[i] == msg.key)
				{
					navElement.widget->Press();
					return true;
				}
			}

			if (widget->IsFocused() && curNavElement.widget == NULL)
				curNavElement = navElement;
			else if (widget->IsFocused())
				widget->SetFocused(false, true);
		}

		if (msg.action == gaAction && curNavElement.widget != NULL && curNavElement.widget->GetVisible() && curNavElement.widget->GetEnabled())
		{
			curNavElement.widget->Press();
			return true;
		}

		for (int i = 0; i < cNavDirEnd; ++i)
		{
			if (dirKeys[i] == msg.key)
			{	
				if (curNavElement.widget)
				{
					if (curNavElement.nextWidget[i] != NULL)
					{
						if (curNavElement.nextWidget[i]->GetVisible() && curNavElement.nextWidget[i]->GetEnabled())
						{
							curNavElement.widget->SetFocused(false);
							curNavElement.nextWidget[i]->SetFocused(true);
						}
						else
						{
							NavElements::const_iterator iter = navElementsIter->second.find(curNavElement.nextWidget[i]);
							if (iter != navElementsIter->second.end())
							{
								List<gui::Widget*> ignoreList;
								ignoreList.push_back(curNavElement.widget);
								NavElement element = NavElementFind(navElementsIter->second, iter->second, ignoreList, (NavDir)i);

								if (element.widget)
								{
									curNavElement.widget->SetFocused(false);
									element.widget->SetFocused(true);
									return true;
								}
							}
						}
					}
				}
				else if (_navElementsList.size() > 0)
				{
					gui::Widget* widget = navElementsIter->first;

					if (widget->GetVisible() && widget->GetEnabled())
					{
						widget->SetFocused(true);
						return true;
					}
					else
					{
						NavElements::const_iterator iter = navElementsIter->second.find(widget);

						if (iter != navElementsIter->second.end())
						{
							NavElement element = NavElementFind(navElementsIter->second, iter->second, List<gui::Widget*>(), (NavDir)i);

							if (element.widget)
							{
								element.widget->SetFocused(true);
								return true;
							}
						}
					}
				}

				break;
			}
		}
	}

	return false;
}

World* Menu::GetWorld()
{
	return _game->GetWorld();
}

D3DXVECTOR2 Menu::WinToLocal(const D3DXVECTOR2& vec, bool centUnscacle)
{
	if (centUnscacle)
	{
		D3DXVECTOR2 off = 0.5f * (cWinSize - GetGUI()->GetVPSize());
		off.y = -off.y;
		return off + vec;
	}
	else
	{
		return GetAspectSize(cWinSize, GetGUI()->GetVPSize()) / cWinSize * vec;
	}
}

void Menu::AdjustLayout(const D3DXVECTOR2& vpSize)
{
	if (_screenFon)
	{
		_screenFon->SetPos(vpSize / 2.0f);
		_screenFon->SetSize(vpSize);
	}

	if (_mainMenu2)
	{
		_mainMenu2->AdjustLayout(vpSize);
	}

	if (_raceMenu2)
	{
		_raceMenu2->AdjustLayout(vpSize);
	}

	if (_hudMenu)
	{
		_hudMenu->AdjustLayout(vpSize);
	}

	if (_finishMenu2)
	{
		_finishMenu2->AdjustLayout(vpSize);
	}

	if (_finalMenu)
	{
		_finalMenu->AdjustLayout(vpSize);
	}

	if (_infoMenu)
	{
		_infoMenu->GetRoot()->SetPos(vpSize / 2.0f);
		_infoMenu->AdjustLayout(vpSize);
	}

	if (_optionsMenu)
	{
		_optionsMenu->AdjustLayout(vpSize);
	}

	if (_startOptionsMenu)
	{
		_startOptionsMenu->AdjustLayout(vpSize);
	}

	if (_acceptDlg)
	{
		_acceptDlg->AdjustLayout(vpSize);
	}

	if (_weaponDlg)
	{
		_weaponDlg->AdjustLayout(vpSize);
	}

	if (_messageDlg)
	{
		_messageDlg->AdjustLayout(vpSize);
	}

	if (_musicDlg)
	{
		_musicDlg->AdjustLayout(vpSize);
	}

	if (_userChat)
	{
		_userChat->AdjustLayout(vpSize);
	}
}

void Menu::OnResetView()
{
	_game->GetMenu()->AdjustLayout(GetGUI()->GetVPSize());
}

void Menu::OnFinishClose()
{
	D3DXVECTOR2 pos = GetGUI()->GetVPSize()/2;
	SetState(msRace2);
	_game->OnFinishFrameClose();

	if (GetRace()->GetPlanetChampion())
	{
		Planet* nextPlanet = GetGame()->GetRace()->GetTournament().GetNextPlanet();

		if (nextPlanet == NULL || nextPlanet->GetIndex() > Race::cTournamentPlanetCount - 1)
		{
			if (!GetDisableVideo())
			{
				const Language* lang = GetLanguageParam();
				if (lang && lang->charset == lcRussian)
					PlayMovie("Data\\Video\\final_eng.avi");
				else
					PlayMovie("Data\\Video\\final.avi");
			}
			else
			{
				DoPlayFinal();
			}
		}
		else if (IsNetGame() && _game->netGame()->isClient())
		{
			ShowMessage(GetString(svWarning), GetString(svHintYouCanFlyPlanetClient), GetString(svOk), pos, gui::Widget::waCenter, 0.0f, NULL);
		}
		else
		{
			_raceMenu2->SetState(n::RaceMenu::msAngar);
			ShowMessage(GetString(svWarning), GetString(svHintYouCanFlyPlanet), GetString(svOk), pos, gui::Widget::waCenter, 0.0f, NULL);
		}
	}
	else if (GetRace()->GetPassChampion())
	{
		ShowMessage(GetString(svWarning), GetString(svHintYouCompletePass), GetString(svOk), pos, gui::Widget::waCenter, 0.0f, NULL);
	}
	else if (IsCampaign() && GetRace()->GetTournament().GetCurTrackIndex() == 0)
	{
		ShowMessage(GetString(svWarning), GetString(svHintYouNotCompletePass), GetString(svOk), pos, gui::Widget::waCenter, 0.0f, NULL);
	}
}

void Menu::ShowCursor(bool value)
{
	_cursor->SetVisible(value);
}

bool Menu::IsCursorVisible() const
{
	return _cursor->GetVisible();
}

void Menu::ShowAccept(const std::string& message, const std::string& yesText, const std::string& noText, const D3DXVECTOR2& pos, gui::Widget::Anchor align, gui::Widget::Event* guiEvent, Object* data, bool maxButtonsSize, bool maxMode, bool disableFocus)
{
	_acceptDlg->Show(message, yesText, noText, pos, align, guiEvent, data, maxButtonsSize, maxMode, disableFocus);

	PlaySound("Sounds\\UI\\acception.ogg");
}

void Menu::HideAccept()
{
	_acceptDlg->Hide();
}

gui::Widget* Menu::GetAcceptSender()
{
	return _acceptDlg->root();
}

bool Menu::GetAcceptResultYes()
{
	return _acceptDlg->resultYes();
}

Object* Menu::GetAcceptData()
{
	return _acceptDlg->data();
}

void Menu::ShowWeaponDialog(const std::string& title, const std::string& message, const std::string& moneyText, const std::string& damageText, const D3DXVECTOR2& pos, gui::Widget::Anchor align, float timeDelay)
{
	_weaponDlg->Show(title, message, moneyText, damageText, pos, align);

	if (_weaponTime != -2.0f)
	{
		_weaponDlg->root()->SetVisible(false);
		_weaponTime = timeDelay;
	}
}

void Menu::HideWeaponDialog()
{
	_weaponDlg->Hide();
	_weaponTime = -1.0f;
}

void Menu::ShowMessage(const std::string& title, const std::string& message, const std::string& okText, const D3DXVECTOR2& pos, gui::Widget::Anchor align, const float timeDelay, gui::Widget::Event* guiEvent, bool okButton)
{
	_messageDlg->Show(title, message, okText, pos, align, guiEvent, NULL, okButton);

	if (timeDelay == 0.0f)
	{
		_messageTime = -1.0f;
		_messageDlg->root()->SetVisible(true);
	}
	else
	{
		_messageDlg->root()->SetVisible(false);
		_messageTime = timeDelay;
	}

	PlaySound("Sounds\\UI\\warning.ogg");
}

void Menu::ShowMessageLoading()
{
	ShowMessage(GetString(svWarning), GetString(svHintPleaseWait), GetString(svOk), GetGUI()->GetVPSize()/2, gui::Widget::waCenter, 0.0f, NULL, false);
}

void Menu::HideMessage()
{
	_messageDlg->Hide();
	_messageTime = -1.0f;
}

gui::Widget* Menu::GetMessageSender()
{
	return _messageDlg->root();
}

void Menu::ShowMusicInfo(const std::string& title, const std::string& text)
{
	_musicDlg->Show(title, text);

	if (_musicTime == -1.0f)
	{
		_musicDlg->root()->SetVisible(false);
		_musicTime = -0.999f;
	}
}

void Menu::HideMusicInfo()
{
	_musicDlg->Hide();
	_musicTime = -1.0f;
}

void Menu::ShowDiscreteVideoCardMessage()
{
	ShowAccept(GetString("svDiscreteVideoDetection"), GetString("svActivate"), GetString("svIgnore"), GetGUI()->GetVPSize()/2, gui::Widget::waCenter, _syncModeEvent, NULL, true, true);
}

void Menu::ShowSteamErrorMessage()
{
	ShowMessage(GetString("svWarning"), GetString("svSteamErrorMessage"), GetString("svOk"), GetGUI()->GetVPSize()/2, gui::Widget::waCenter, 0.0f, _steamErrorEvent, true);
}

void Menu::ShowSteamSavingMessage()
{
	ShowAccept(GetString("svSteamSavingMessage"), GetString("svExit"), GetString("svCancel"), GetGUI()->GetVPSize()/2, gui::Widget::waCenter, _steamSavingEvent, NULL, true, true);
}

void Menu::ShowChat(bool show)
{
	if (!show)
		ShowChatInput(false);

	_userChat->Show(show);
}

void Menu::ClearChat()
{
	_userChat->DelLines();
}

bool Menu::IsChatVisible() const
{
	return _userChat->visible();
}

n::UserChat* Menu::GetUserChat()
{
	return _userChat;
}

void Menu::ShowChatInput(bool show)
{
	lsl::string name = (GetPlayer() ? GetString(GetPlayer()->GetName()) : "") + ": ";
	_userChat->ShowInput(show, lsl::ConvertStrAToW(name, CP_THREAD_ACP), L"", GetPlayer() ? GetPlayer()->GetColor() : clrWhite);
}

void Menu::ClearChatInput()
{
	_userChat->ClearInput();
}

bool Menu::IsChatInputVisible() const
{
	return IsChatVisible() && _userChat->IsInputVisible();
}

void Menu::Terminate()
{
	_game->Terminate();
}

void Menu::InitializeNet()
{
	GetNet()->Initializate();
}

void Menu::FinalizateNet()
{
	GetNet()->Finalizate();
}

void Menu::StartMatch(Race::Mode mode, Difficulty difficulty, Race::Profile* profile, bool netGame, net::INetAcceptorImpl* impl)
{
	if (netGame)
	{
		GetNet()->CreateHost(impl);

		GetNet()->race()->StartMatch(mode, difficulty, profile);
	}
	else
	{
		_game->StartMatch(mode, difficulty, profile, true, false, false);
	}

	if (profile == NULL)
	{
		SetState(msRace2);
		_raceMenu2->SetState(n::RaceMenu::msGamers);
	}
	else
	{
		SetState(msRace2);
		_raceMenu2->SetState(n::RaceMenu::msMain);
	}
}

void Menu::ExitMatch(bool kicked)
{	
	SetState(msMain2);

	if (IsNetGame())
	{
		if (GetNet()->race())
			GetNet()->race()->ExitMatch();

		FinalizateNet();

#ifdef STEAM_SERVICE
		if (GetSteamService()->isInit())
			GetSteamService()->server()->ShutdownHost();
#endif
	}
	else
	{
		_game->ExitMatch(true);
	}
}

bool Menu::ConnectMatch(const net::Endpoint& endpoint, bool useDefaultPort, net::INetAcceptorImpl* impl)
{
	return GetNet()->Connect(endpoint, useDefaultPort, impl);
}

void Menu::MatchConnected()
{
	SetState(msRace2);

	if (GetPlayer()->GetCar().record)
	{
		_raceMenu2->SetState(n::RaceMenu::msMain);
	}
	else
	{
		_raceMenu2->SetState(n::RaceMenu::msGamers);
	}
}

void Menu::DisconnectMatch()
{
	GetNet()->Close();
}

void Menu::CarSlotsChanged()
{
	if (IsNetGame())
		GetNet()->player()->CarSlotsChanged();
}

bool Menu::IsCampaign()
{
	return GetRace()->IsCampaign();
}

bool Menu::IsSkirmish()
{
	return GetRace()->IsSkirmish();
}

bool Menu::IsNetGame()
{
	return GetNet()->isStarted();
}

#ifdef STEAM_SERVICE
bool Menu::IsSteamSavingInProcess()
{
	return GetSteamService()->syncInProcess();
}
#endif

void Menu::DelProfile(int profileIndex)
{
	_game->GetRace()->DelProfile(profileIndex, true);
}

Race::Profile* Menu::GetLastProfile(bool netGame)
{
	Race::Profile* profile = netGame ? GetRace()->GetLastNetProfile() : GetRace()->GetLastProfile();
	
	if (profile == NULL)
	{
		for (Race::Profiles::const_iterator iter = GetRace()->GetProfiles().begin(); iter != GetRace()->GetProfiles().end(); ++iter)
			if ((*iter)->netGame() == netGame)
				return *iter;
	}

	return profile;
}

Player* Menu::GetPlayer()
{
	return GetRace()->GetHuman() ? GetRace()->GetHuman()->GetPlayer() : NULL;
}

void Menu::SetGamerId(int gamerId)
{
	if (IsNetGame())
	{
		GetNet()->player()->SetGamerId(gamerId);
	}
	else
		GetPlayer()->SetGamerId(gamerId);
}

const D3DXCOLOR& Menu::GetCarColor()
{
	if (IsNetGame())
		return GetNet()->player()->GetColor();
	else
		return GetPlayer()->GetColor();
}

void Menu::SetCarColor(const D3DXCOLOR& color)
{
	if (IsNetGame())
		GetNet()->player()->SetColor(color);
	else
		GetPlayer()->SetColor(color);
}

bool Menu::BuyCar(Garage::Car* car)
{
	if (IsNetGame())
		return GetNet()->player()->BuyCar(car);
	else
		return GetRace()->GetGarage().BuyCar(GetPlayer(), car);
}

void Menu::ChangePlanet(Planet* planet)
{
	if (IsNetGame())
		GetNet()->race()->ChangePlanet(planet);
	else
		_game->ChangePlanet(planet);
}

int Menu::GetUpgradeMaxLevel()
{
	if (IsNetGame())
		return GetNet()->race()->GetUpgradeMaxLevel();
	else
		return _game->upgradeMaxLevel();
}

void Menu::SetUpgradeMaxLevel(int value)
{
	if (IsNetGame())
		GetNet()->race()->SetUpgradeMaxLevel(value);
	else
		_game->upgradeMaxLevel(value);
}

bool Menu::EnabledOptionUpgradeMaxLevel()
{
	return !IsNetGame() || GetNet()->isHost();
}

int Menu::GetWeaponMaxLevel()
{
	if (IsNetGame())
		return GetNet()->race()->GetWeaponMaxLevel();
	else
		return _game->weaponMaxLevel();
}

void Menu::SetWeaponMaxLevel(int value)
{
	if (IsNetGame())
		GetNet()->race()->SetWeaponMaxLevel(value);
	else
		_game->weaponMaxLevel(value);
}

bool Menu::EnabledOptionWeaponMaxLevel()
{
	return !IsNetGame() || GetNet()->isHost();
}

Difficulty Menu::currentDiff()
{
	if (IsNetGame())
		return GetNet()->race()->GetCurrentDifficulty();
	else
		return _game->currentDiff();
}

void Menu::currentDiff(Difficulty value)
{
	if (IsNetGame())
		GetNet()->race()->SetCurrentDifficulty(value);
	else
		_game->currentDiff(value);
}

bool Menu::enabledOptionDiff()
{
	return _game->enabledOptionDiff() && (!IsNetGame() || GetNet()->isHost());
}

unsigned Menu::lapsCount()
{
	if (IsNetGame())
		return GetNet()->race()->GetLapsCount();
	else
		return _game->lapsCount();
}

void Menu::lapsCount(unsigned value)
{
	if (IsNetGame())
		GetNet()->race()->SetLapsCount(value);
	else
		_game->lapsCount(value);
}

bool Menu::enabledLapsCount()
{
	return !IsNetGame() || GetNet()->isHost();
}

unsigned Menu::maxPlayers()
{
	if (IsNetGame())
		return GetNet()->race()->GetMaxPlayers();
	else
		return _game->maxPlayers();
}

void Menu::maxPlayers(unsigned value)
{
	if (IsNetGame())
		GetNet()->race()->SetMaxPlayers(value);
	else
		_game->maxPlayers(value);
}

bool Menu::enabledMaxPlayers()
{
	return !IsNetGame() || GetNet()->isHost();
}

unsigned Menu::maxComputers()
{
	if (IsNetGame())
		return GetNet()->race()->GetMaxComputers();
	else
		return _game->maxComputers();
}

void Menu::maxComputers(unsigned value)
{
	if (IsNetGame())
		GetNet()->race()->SetMaxComputers(value);
	else
		_game->maxComputers(value);
}

bool Menu::enabledMaxComputers()
{
	return !IsNetGame() || GetNet()->isHost();
}

bool Menu::springBorders()
{
	if (IsNetGame())
		return GetNet()->race()->GetSpringBorders();
	else
		return _game->springBorders();
}

void Menu::springBorders(bool value)
{
	if (IsNetGame())
		GetNet()->race()->SetSpringBorders(value);
	else
		_game->springBorders(value);
}

bool Menu::enabledSpringBorders()
{
	return !IsNetGame() || GetNet()->isHost();
}

bool Menu::enableHUD()
{
	return _game->enableHUD();
}

void Menu::enableHUD(bool value)
{
	_game->enableHUD(value);
}

bool Menu::enableMineBug()
{
	if (IsNetGame())
		return GetNet()->race()->GetEnableMineBug();
	else
		return _game->enableMineBug();
}

void Menu::enableMineBug(bool value)
{
	if (IsNetGame())
		GetNet()->race()->SetEnableMineBug(value);
	else
		_game->enableMineBug(value);	
}

bool Menu::activeEnableMineBug()
{
	return !IsNetGame() || GetNet()->isHost();
}

void Menu::StartRace()
{
	if (IsNetGame())
		GetNet()->race()->StartRace();
	else
	{
		_game->StartRace();
		_game->GoRaceTimer();
	}
}

void Menu::ExitRace()
{
	if (IsNetGame())
	{
		if (GetNet()->isHost())
		{
			GetNet()->race()->ExitRace();
		}
		else
		{
			_game->ExitRace(false);
			_game->ExitRaceGoFinish();
		}
	}
	else
	{
		_game->ExitRace(true);
		_game->ExitRaceGoFinish();
	}
}

void Menu::RegUser(IGameUser* user)
{
	_game->RegUser(user);
}

void Menu::UnregUser(IGameUser* user)
{
	_game->UnregUser(user);
}

void Menu::SendEvent(unsigned id, EventData* data)
{	
	_game->SendEvent(id, data);
}

void Menu::OnProcessEvent(unsigned id, EventData* data)
{
	switch (id)
	{
	case cRaceFinish:
		if (IsNetGame())
		{
			GetNet()->player()->RaceFinish(true);
		}
		else
			_game->RunFinishTimer();
		break;

	case cRaceFinishTimeEnd:
		ExitRace();
		break;

	case cVideoStopped:
		DoPlayFinal();
		break;
	}
}

gui::Dummy* Menu::CreateDummy(gui::Widget* parent, gui::Widget::Event* guiEvent, SoundShemeType soundSheme)
{
	gui::Dummy* node = GetGUI()->CreateDummy();
	node->SetParent(parent);

	if (soundSheme != cSoundShemeTypeEnd)
		node->RegEvent(_soundShemes[soundSheme]);
	if (guiEvent)
		node->RegEvent(guiEvent);

	return node;
}

gui::PlaneFon* Menu::CreatePlane(gui::Widget* parent, gui::Widget::Event* guiEven, graph::Tex2DResource* image, bool imageSize, const D3DXVECTOR2& size, gui::Material::Blending blend, SoundShemeType soundSheme)
{
	gui::PlaneFon* plane = GetGUI()->CreatePlaneFon();
	plane->SetParent(parent);

	plane->GetMaterial().GetSampler().SetTex(image);
	plane->GetMaterial().SetBlending(blend);

	if (soundSheme != cSoundShemeTypeEnd)
		plane->RegEvent(_soundShemes[soundSheme]);
	if (guiEven)
		plane->RegEvent(guiEven);
	
	if (imageSize)
		plane->SetSize(GetImageSize(plane->GetMaterial()) * size);
	else
		plane->SetSize(size);

	return plane;
}

gui::PlaneFon* Menu::CreatePlane(gui::Widget* parent, gui::Widget::Event* guiEven, const std::string& image, bool imageSize, const D3DXVECTOR2& size, gui::Material::Blending blend, SoundShemeType soundSheme)
{
	return CreatePlane(parent, guiEven, !image.empty() ? GetTexture(image) : 0, imageSize, size, blend, soundSheme);
}

gui::Button* Menu::CreateArrowButton(gui::Widget* parent, gui::Widget::Event* guiEvent, const D3DXVECTOR2& size)
{
	gui::Button* button = GetGUI()->CreateButton();
	button->SetParent(parent);
	
	gui::Material* fonMat = button->GetOrCreateFon();
	fonMat->GetSampler().SetTex(GetTexture("GUI\\viewArrow.tga"));
	fonMat->SetBlending(gui::Material::bmTransparency);

	gui::Material* selMat = button->GetOrCreateSel();
	selMat->GetSampler().SetTex(GetTexture("GUI\\viewArrowSel.tga"));	
	selMat->SetBlending(gui::Material::bmTransparency);

	button->SetSize(GetImageSize(*fonMat) * size);
	button->SetEvent(guiEvent);

	return button;
}

gui::Button* Menu::CreateSpaceArrowButton(gui::Widget* parent, gui::Widget::Event* guiEvent, const D3DXVECTOR2& size)
{
	gui::Button* button = GetGUI()->CreateButton();
	button->SetParent(parent);
	
	gui::Material* fonMat = button->GetOrCreateFon();
	fonMat->GetSampler().SetTex(GetTexture("GUI\\spaceArrow.tga"));
	fonMat->SetBlending(gui::Material::bmTransparency);

	gui::Material* selMat = button->GetOrCreateSel();
	selMat->GetSampler().SetTex(GetTexture("GUI\\spaceArrowSel.tga"));	
	selMat->SetBlending(gui::Material::bmTransparency);

	button->SetSize(GetImageSize(*fonMat) * size);
	button->SetEvent(guiEvent);

	return button;
}

gui::Button* Menu::CreateMenuButton(const lsl::string& name, const std::string& font, const std::string& norm, const std::string& sel, gui::Widget* parent, gui::Widget::Event* guiEvent, const D3DXVECTOR2& size, gui::Button::Style style, const D3DXCOLOR& textColor, SoundShemeType soundSheme)
{
	gui::Button* button = GetGUI()->CreateButton();
	button->SetParent(parent);
	button->SetStyle(style);

	D3DXVECTOR2 normSize = size;
	if (norm != "")
	{
		gui::Material* fonMat = button->GetOrCreateFon();
		fonMat->GetSampler().SetTex(GetTexture(norm));	
		fonMat->SetBlending(gui::Material::bmTransparency);		
		normSize = normSize * GetImageSize(*fonMat);
	}

	button->SetSize(normSize);

	if (sel != "")
	{
		gui::Material* selMat = button->GetOrCreateSel();
		selMat->GetSampler().SetTex(GetTexture(sel));
		selMat->SetBlending(gui::Material::bmTransparency);
		button->SetSelSize(GetImageSize(*selMat) * size);
	}

	if (font != "")
	{
		button->GetOrCreateTextMaterial()->SetColor(textColor);
		button->SetFont(GetFont(font));
		button->SetText(name);
	}

	if (soundSheme != cSoundShemeTypeEnd)
		button->RegEvent(_soundShemes[soundSheme]);

	if (guiEvent)
		button->RegEvent(guiEvent);

	return button;
}

gui::Button* Menu::CreateMenuButton(StringValue name, const std::string& font, const std::string& norm, const std::string& sel, gui::Widget* parent, gui::Widget::Event* guiEvent, const D3DXVECTOR2& size, gui::Button::Style style, const D3DXCOLOR& textColor, SoundShemeType soundSheme)
{
	return CreateMenuButton(GetString(name), font, norm, sel, parent, guiEvent, size, style, textColor, soundSheme);	
}

gui::Button* Menu::CreateMenuButton(const std::string& name, gui::Widget* parent, gui::Widget::Event* guiEvent, const D3DXVECTOR2& size, SoundShemeType soundSheme)
{
	gui::Button* but = CreateMenuButton(svNull, "Header", "GUI\\buttonBig.tga", "GUI\\buttonBigSel.tga", parent, guiEvent, size, gui::Button::bsSimple, cTextColor, soundSheme);
	but->SetText(name);
	return but;
}

gui::Button* Menu::CreateMenuButton(StringValue name, gui::Widget* parent, gui::Widget::Event* guiEvent, const D3DXVECTOR2& size, SoundShemeType soundSheme)
{
	return CreateMenuButton(GetString(name), parent, guiEvent, size, soundSheme);
}

gui::Button* Menu::CreateMenuButton2(StringValue name, gui::Widget* parent, gui::Widget::Event* guiEvent)
{
	gui::Button* button = CreateMenuButton(name, "Item", "GUI\\buttonBg5.png", "", parent, guiEvent, IdentityVec2, gui::Button::bsSelAnim, D3DXCOLOR(0xffafafaf), Menu::ssButton1);
	button->GetOrCreateTextSelMaterial()->SetColor(D3DXCOLOR(0xffeb733e));

	return button;
}

gui::Button* Menu::CreateArrow(gui::Widget* parent, gui::Widget::Event* guiEvent)
{
	return CreateMenuButton(svNull, "", "GUI\\arrow1.png", "GUI\\arrowSel1.png", parent, guiEvent, IdentityVec2, gui::Button::bsSelAnim);
}

gui::Label* Menu::CreateLabel(const std::string& name, gui::Widget* parent, const std::string& font, const D3DXVECTOR2& size, gui::Text::HorAlign horAlign, gui::Text::VertAlign vertAlign, const D3DXCOLOR& color)
{
	gui::Label* label = GetGUI()->CreateLabel();
	label->SetParent(parent);

	label->SetFont(GetFont(font));
	label->SetSize(size);
	label->SetText(name);
	label->SetHorAlign(horAlign);
	label->SetVertAlign(vertAlign);

	label->GetMaterial().SetColor(color);

	return label;
}

gui::Label* Menu::CreateLabel(StringValue name, gui::Widget* parent, const std::string& font, const D3DXVECTOR2& size, gui::Text::HorAlign horAlign, gui::Text::VertAlign vertAlign, const D3DXCOLOR& color)
{
	return CreateLabel(GetString(name), parent, font, size, horAlign, vertAlign, color);	
}

gui::DropBox* Menu::CreateDropBox(gui::Widget* parent, gui::Widget::Event* guiEvent, const lsl::StringList& items)
{
	gui::DropBox* dropBox = GetGUI()->CreateDropBox();
	dropBox->SetParent(parent);
	dropBox->SetSize(D3DXVECTOR2(150.0f, 25.0f));
	dropBox->SetEvent(guiEvent);

	dropBox->SetFont(GetFont("Small"));
	dropBox->GetFonMaterial().SetColor(D3DXCOLOR(51.0f, 83.0f, 113.0f, 255.0f)/255.0f);
	dropBox->GetButMaterial().GetSampler().SetTex(GetTexture("GUI\\dropBoxButton.tga"));
	dropBox->GetTextMaterial().SetColor(cTextColor);
	dropBox->GetSelMaterial().SetColor(D3DXCOLOR(250.0f, 255.0f, 0.0f, 255.0f)/255.0f);
	
	dropBox->SetItems(items);

	return dropBox;
}

gui::TrackBar* Menu::CreateTrackBar(gui::Widget* parent, gui::Widget::Event* guiEvent, const D3DXVECTOR2& size)
{
	gui::TrackBar* trackBar = GetGUI()->CreateTrackBar();
	trackBar->SetParent(parent);
	trackBar->SetSize(size);
	trackBar->SetEvent(guiEvent);

	trackBar->GetBarMaterial().GetSampler().SetTex(GetTexture("GUI\\trackBar.tga"));
	trackBar->GetBarMaterial().SetBlending(gui::Material::bmTransparency);
	trackBar->GetButMaterial().GetSampler().SetTex(GetTexture("GUI\\trackButton.tga"));

	return trackBar;
}

gui::ListBox* Menu::CreateListBox(gui::Widget* parent, gui::Widget::Event* guiEvent, const D3DXVECTOR2& size, const D3DXVECTOR2& itemSize, const D3DXVECTOR2& itemSpace)
{
	gui::ListBox* listBox = GetGUI()->CreateListBox();
	listBox->SetParent(parent);
	listBox->SetSize(size);
	listBox->SetItemSize(itemSize);
	listBox->SetItemSpace(itemSpace);

	listBox->GetOrCreateFon().SetColor(D3DXCOLOR(1.0f, 1.0f, 1.0f, 0.3f));
	listBox->GetOrCreateFon().SetBlending(gui::Material::bmTransparency);
	listBox->GetOrCreateFon().GetSampler().SetTex(GetTexture("GUI\\listBoxFon.tga"));	

	listBox->GetOrCreateFrame().GetSampler().SetTex(GetTexture("GUI\\itemFrame.tga"));
	listBox->GetOrCreateFrame().SetBlending(gui::Material::bmTransparency);

	listBox->GetArrowMaterial().GetSampler().SetTex(GetTexture("GUI\\scrollArrow.tga"));
	listBox->GetArrowMaterial().SetBlending(gui::Material::bmTransparency);
	listBox->GetArrowSelMaterial().GetSampler().SetTex(GetTexture("GUI\\scrollArrowSel.tga"));
	listBox->GetArrowSelMaterial().SetBlending(gui::Material::bmTransparency);
	listBox->SetArrowSize(GetImageSize(listBox->GetArrowMaterial()));

	return listBox;
}

gui::ProgressBar* Menu::CreateBar(gui::Widget* parent, gui::Widget::Event* guiEvent, const std::string& front, const std::string& back, gui::ProgressBar::Style style)
{
	gui::ProgressBar* bar = GetGUI()->CreateProgressBar();	
	bar->SetParent(parent);
	bar->SetPos(D3DXVECTOR2(0, 0));	
	bar->GetFront().GetSampler().SetTex(GetTexture(front));
	bar->SetSize(GetImageSize(bar->GetFront()));
	bar->GetFront().SetBlending(gui::Material::bmTransparency);
	bar->SetStyle(style);

	if (back != "")
	{
		bar->SetBackEnabled(true);
		bar->GetBack().GetSampler().SetTex(GetTexture(back));
	}
	else
		bar->SetBackEnabled(false);
	//bar->GetBack().SetColor(clrBlack);

	return bar;
}

gui::ChargeBar* Menu::CreateChargeBar(gui::Widget* parent, gui::Widget::Event* guiEvent, unsigned maxCharge, unsigned curCharge)
{
	gui::ChargeBar* chargeBar = GetGUI()->CreateChargeBar();
	chargeBar->SetParent(parent);	

	chargeBar->GetFrame().GetSampler().SetTex(GetTexture("GUI\\chargeFrame.tga"));
	chargeBar->GetFrame().SetBlending(gui::Material::bmTransparency);

	chargeBar->GetStreak().GetSampler().SetTex(GetTexture("GUI\\chargeStreak.tga"));
	chargeBar->GetStreak().SetBlending(gui::Material::bmTransparency);

	chargeBar->GetUp().GetSampler().SetTex(GetTexture("GUI\\chargeUp.tga"));
	chargeBar->GetUp().SetBlending(gui::Material::bmTransparency);

	chargeBar->GetUpSel().GetSampler().SetTex(GetTexture("GUI\\chargeUpSel.tga"));
	chargeBar->GetUpSel().SetBlending(gui::Material::bmTransparency);

	chargeBar->SetSize(GetImageSize(chargeBar->GetFrame()));
	chargeBar->SetChargeMax(maxCharge);
	chargeBar->SetCharge(curCharge);

	return chargeBar;
}

gui::ColorList* Menu::CreateColorList(gui::Widget* parent, gui::Widget::Event* guiEvent, const D3DXVECTOR2& size)
{
	gui::ColorList* colorList = GetGUI()->CreateColorList();
	colorList->SetParent(parent);
	colorList->SetSize(size);
	colorList->SetSpace(D3DXVECTOR2(0.0f, 10.0f));
	colorList->SetEvent(guiEvent);

	colorList->GetFrame().GetSampler().SetTex(GetTexture("GUI\\colorFrame.tga"));
	colorList->GetFrame().SetBlending(gui::Material::bmTransparency);

	colorList->GetBox().GetSampler().SetTex(GetTexture("GUI\\colorBox.tga"));
	colorList->GetBox().SetBlending(gui::Material::bmTransparency);

	colorList->GetCheck().GetSampler().SetTex(GetTexture("GUI\\colorCheck.tga"));
	colorList->GetCheck().SetBlending(gui::Material::bmTransparency);

	colorList->InsertColor(clrBlack);
	colorList->InsertColor(clrBlue);
	colorList->InsertColor(D3DXCOLOR(48.0f, 139.0f, 231.0f, 255.0f)/255.0f);
	colorList->InsertColor(clrRed);
	colorList->InsertColor(clrGreen);
	colorList->InsertColor(clrYellow);
	colorList->InsertColor(D3DXCOLOR(255.0f, 150.0f, 0.0f, 255.0f)/255.0f);
	colorList->InsertColor(D3DXCOLOR(180.0f, 0.0f, 180.0f, 255.0f)/255.0f);
	colorList->InsertColor(D3DXCOLOR(255.0f, 100.0f, 255.0f, 255.0f)/255.0f);
	colorList->InsertColor(clrWhite);

	colorList->SelectColor(&clrRed);

	return colorList;
}

gui::ViewPort3d* Menu::CreateItemBox(gui::Widget* parent, gui::Widget::Event* guiEvent, graph::IndexedVBMesh* mesh, graph::Tex2DResource* meshTex)
{
	gui::ViewPort3d* viewPort = CreateViewPort3d(parent, guiEvent, "GUI\\itemBox.tga", gui::ViewPort3d::msStatic, true, true, IdentityVec2);

	CreateMesh3d(viewPort, mesh, meshTex);

	return viewPort;
}

gui::Button* Menu::CreateCloseButton(gui::Widget* parent, gui::Widget::Event* guiEvent, const D3DXVECTOR2& size)
{
	gui::Button* button = GetGUI()->CreateButton();
	button->SetParent(parent);

	gui::Material* fonMat = button->GetOrCreateFon();
	fonMat->GetSampler().SetTex(GetTexture("GUI\\closeButton.tga"));
	fonMat->SetColor(D3DXCOLOR(1.0f, 1.0f, 1.0f, 0.3f));
	fonMat->SetBlending(gui::Material::bmTransparency);

	gui::Material* selMat = button->GetOrCreateSel();
	selMat->GetSampler().SetTex(GetTexture("GUI\\closeButtonSel.tga"));	
	selMat->SetColor(D3DXCOLOR(1.0f, 1.0f, 1.0f, 0.75f));
	selMat->SetBlending(gui::Material::bmTransparency);

	button->SetSize(GetImageSize(*fonMat) * size);
	button->SetEvent(guiEvent);

	return button;
}

gui::ScrollBox* Menu::CreateScrollBox(gui::Widget* parent, gui::Widget::Event* guiEvent, const D3DXVECTOR2& size)
{
	gui::ScrollBox* scrollBox = GetGUI()->CreateScrollBox();
	scrollBox->SetParent(parent);
	scrollBox->SetSize(size);
	
	scrollBox->GetArrowMaterial().GetSampler().SetTex(GetTexture("GUI\\scrollArrow.tga"));
	scrollBox->GetArrowMaterial().SetBlending(gui::Material::bmTransparency);
	scrollBox->GetArrowSelMaterial().GetSampler().SetTex(GetTexture("GUI\\scrollArrowSel.tga"));
	scrollBox->GetArrowSelMaterial().SetBlending(gui::Material::bmTransparency);
	scrollBox->SetArrowSize(GetImageSize(scrollBox->GetArrowMaterial()));

	return scrollBox;
}

gui::Grid* Menu::CreateGrid(gui::Widget* parent, gui::Widget::Event* guiEvent, gui::Grid::Style style, const D3DXVECTOR2& cellSize, unsigned maxCellsOnLine)
{
	gui::Grid* grid = GetGUI()->CreateGrid();
	grid->SetParent(parent);
	grid->SetEvent(guiEvent);
	grid->style(style);
	grid->cellSize(cellSize);
	grid->maxCellsOnLine(maxCellsOnLine);

	return grid;
}

gui::StepperBox* Menu::CreateStepper(const StringList& items, gui::Widget* parent, gui::Widget::Event* guiEvent)
{
	gui::StepperBox* stepper = GetGUI()->CreateStepperBox();
	stepper->SetParent(parent);

	stepper->RegEvent(GetSoundSheme(Menu::ssStepper));
	if (guiEvent)
		stepper->RegEvent(guiEvent);

	stepper->SetItemsLoop(true);	
	stepper->SetItems(items);
	stepper->SetSelIndex(0);

	stepper->GetOrCreateArrow()->GetSampler().SetTex(GetTexture("GUI\\arrow3.png"));
	stepper->GetOrCreateArrow()->SetBlending(gui::Material::bmTransparency);

	stepper->GetOrCreateArrowSel()->GetSampler().SetTex(GetTexture("GUI\\arrowSel3.png"));
	stepper->GetOrCreateArrowSel()->SetBlending(gui::Material::bmTransparency);

	stepper->SetFont(GetFont("Small"));
	stepper->GetOrCreateText()->SetColor(D3DXCOLOR(0xffafafaf));

	stepper->SetSize(240.0f, 45.0f);

	stepper->Invalidate();

	return stepper;
}

gui::ViewPort3d* Menu::CreateViewPort3d(gui::Widget* parent, gui::Widget::Event* guiEvent, const std::string& fon, gui::ViewPort3d::Style style, bool isoRot, bool fonSize, const D3DXVECTOR2& size, SoundShemeType soundSheme)
{
	gui::ViewPort3d* viewPort = GetGUI()->CreateViewPort3d();
	viewPort->SetParent(parent);

	D3DXVECTOR2 viewSize = size;
	if (!fon.empty())
	{
		gui::Plane* plane = GetGUI()->GetContext().CreatePlane();
		plane->GetOrCreateMaterial()->GetSampler().SetTex(GetTexture(fon));
		plane->GetOrCreateMaterial()->SetBlending(gui::Material::bmTransparency);
		viewPort->InsertGraphic(plane, viewPort->GetGraphics().begin());

		if (fonSize)
			viewSize *= GetImageSize(*plane->GetMaterial());

		plane->SetSize(viewSize);
	}

	viewPort->GetBox()->SetRot(isoRot ? GetIsoRot() : NullQuaternion);
	viewPort->SetStyle(style);
	viewPort->SetSize(viewSize);

	if (soundSheme != cSoundShemeTypeEnd)
		viewPort->RegEvent(_soundShemes[soundSheme]);
	if (guiEvent)
		viewPort->RegEvent(guiEvent);

	return viewPort;
}

gui::Mesh3d* Menu::CreateMesh3d(gui::ViewPort3d* parent, graph::IndexedVBMesh* mesh, graph::Tex2DResource* meshTex, int meshId)
{
	gui::Mesh3d* mesh3d = parent->GetContext().CreateMesh3d();

	mesh3d->SetMesh(mesh);
	mesh3d->SetMeshId(meshId);
	mesh3d->GetMaterial()->GetSampler().SetTex(meshTex);

	parent->GetBox()->InsertChild(mesh3d);

	return mesh3d;
}

gui::ViewPort3d* Menu::CreateMesh3dBox(gui::Widget* parent, gui::Widget::Event* guiEvent, graph::IndexedVBMesh* mesh, graph::Tex2DResource* meshTex, gui::ViewPort3d::Style style, SoundShemeType soundSheme)
{
	gui::ViewPort3d* viewPort = CreateViewPort3d(parent, guiEvent, "", style, true, true, IdentityVec2, soundSheme);
	CreateMesh3d(viewPort, mesh, meshTex);

	return viewPort;
}

gui::Plane3d* Menu::CreatePlane3d(gui::ViewPort3d* parent, const std::string& fon, const D3DXVECTOR2& size)
{
	gui::Plane3d* plane3d = parent->GetContext().CreatePlane3d();
	plane3d->SetSize(size);
	parent->GetBox()->InsertChild(plane3d);
	
	if (!fon.empty())
	{
		plane3d->GetOrCreateMaterial()->GetSampler().SetTex(GetTexture(fon));
	}

	return plane3d;
}

void Menu::ReleaseWidget(gui::Widget* widget)
{
	widget->DeleteAllChildren();
	GetGUI()->ReleaseWidget(widget);
}

void Menu::SetButtonEnabled(gui::Button* button, bool enable)
{
	if (button->GetFon())
		button->GetFon()->SetAlpha(enable ? 1.0f : 0.25f);
	if (button->GetTextMaterial())
		button->GetTextMaterial()->SetAlpha(enable ? 1.0f : 0.25f);

	button->SetEnabled(enable);
}

void Menu::SetStepperEnabled(gui::StepperBox* stepper, bool enable)
{
	stepper->GetOrCreateArrow()->SetAlpha(enable ? 1.0f : 0.25f);

	stepper->SetEnabled(enable);
}

void Menu::OnProgress(float deltaTime)
{
	if (_hudMenu)
		_hudMenu->OnProgress(deltaTime);	
	if (_mainMenu2)
		_mainMenu2->OnProgress(deltaTime);
	if (_raceMenu2)
		_raceMenu2->OnProgress(deltaTime);
	if (_finishMenu2)
		_finishMenu2->OnProgress(deltaTime);
	if (_finalMenu)
		_finalMenu->OnProgress(deltaTime);

	if (_weaponTime >= 0 && (_weaponTime -= deltaTime) <= 0.0f)
	{
		_weaponTime = -2.0f;
		_weaponDlg->root()->SetVisible(true);
	}

	if (_messageTime >= 0 && (_messageTime -= deltaTime) <= 0.0f)
	{
		_messageTime = -1.0f;
		_messageDlg->root()->SetVisible(true);
	}
	
	if (_musicTime != -1.0f)
	{	
		const float cMusicDelay = 1.0f;
		const float cMusicLife = 3.0f;		

		float offset = ClampValue((_musicTime - 0.0f)/cMusicDelay, 0.0f, 1.0f) - ClampValue((_musicTime - cMusicDelay - cMusicLife)/cMusicDelay, 0.0f, 1.0f);
		D3DXVECTOR2 size = _musicDlg->size();
		D3DXVECTOR2 vpSize = GetGUI()->GetVPSize();
		D3DXVECTOR2 pos = D3DXVECTOR2(-5.0f + (40.0f + size.x) * offset, vpSize.y - 30.0f) - size/2;

		_musicDlg->root()->SetPos(pos);

		if (_musicTime >= cMusicLife + 2.0f * cMusicDelay)
			HideMusicInfo();
		else
		{
			_musicTime += deltaTime;
			_musicDlg->root()->SetVisible(true);
		}
	}

	if (_userChat && _userChat->visible())
		_userChat->OnProgress(deltaTime);
}

Menu::State Menu::GetState() const
{
	return _state;
}

void Menu::SetState(State value)
{
	if (_state != value)
	{
		_state = value;

		GetWorld()->ResetInput();
		ApplyState(_state);
	}
}

void Menu::ShowLoading(bool show)
{
	if (_loadingVisible != show)
	{
		_loadingVisible = show;
		ApplyState(_state);
	}
}

bool Menu::IsLoadingVisible() const
{
	return _loadingVisible;
}

void Menu::ShowOptions(bool show)
{
	if (_optionsVisible != show)
	{
		_optionsVisible = show;
		ApplyState(_state);
	}
}

bool Menu::IsOptionsVisible() const
{
	return _optionsVisible;
}

void Menu::ShowStartOptions(bool show)
{
	if (_startOptionsVisible != show)
	{
		_startOptionsVisible = show;
		ApplyState(_state);
	}
}

bool Menu::IsStartOptionsVisible() const
{
	return _startOptionsVisible;
}

ControlManager* Menu::GetControl()
{
	return GetWorld()->GetControl();
}

Environment* Menu::GetEnv()
{
	return GetWorld()->GetEnv();
}

DataBase* Menu::GetDB()
{
	return GetWorld()->GetDB();
}

gui::Manager* Menu::GetGUI()
{
	return &GetWorld()->GetGraph()->GetGUI();
}

GameMode* Menu::GetGame()
{
	return _game;
}

Race* Menu::GetRace()
{
	return _game->GetRace();
}

Trace* Menu::GetTrace()
{
	return &_game->GetWorld()->GetMap()->GetTrace();
}

NetGame* Menu::GetNet()
{
	return _game->netGame();
}

#ifdef STEAM_SERVICE

SteamService* Menu::GetSteamService()
{
	return _game->steamService();
}

#endif

float Menu::GetVolume(Logic::SndCategory cat)
{
	return _game->GetWorld()->GetLogic()->GetVolume(cat);
}

void Menu::SetVolume(Logic::SndCategory cat, float value)
{
	_game->GetWorld()->GetLogic()->SetVolume(cat, value);
}

D3DXVECTOR2 Menu::GetAspectSize()
{
	return GetAspectSize(Menu::cWinSize, GetGUI()->GetVPSize());
}

graph::Tex2DResource* Menu::GetTexture(const std::string& name, bool ifNullThrow)
{
	if (ifNullThrow)
		return GetWorld()->GetResManager()->GetImageLib().Get(name).GetOrCreateTex2d();
	else
	{
		ComplexImage* image = GetWorld()->GetResManager()->GetImageLib().Find(name);

		return image ? image->GetOrCreateTex2d() : NULL;
	}
}

graph::IndexedVBMesh* Menu::GetMesh(const std::string& name)
{
	return GetWorld()->GetResManager()->GetMeshLib().Get(name).GetOrCreateIVBMesh();
}

graph::TextFont* Menu::GetFont(const std::string& name)
{
	return &GetWorld()->GetResManager()->GetTextFontLib().Get(name);
}

snd::Sound* Menu::GetSound(const lsl::string& name)
{
	snd::Sound* snd = GetWorld()->GetResManager()->GetSoundLib().Find(name);

	LSL_ASSERT(snd);

	return snd;
}

Menu::SoundSheme* Menu::GetSoundSheme(SoundShemeType sheme)
{
	return _soundShemes[sheme];
}

const std::string& Menu::GetString(StringValue value)
{
	return GetWorld()->GetResManager()->GetStringLib().Get(value);
}

const std::string& Menu::GetString(const std::string& value)
{
	return GetWorld()->GetResManager()->GetStringLib().Get(value);
}

bool Menu::HasString(StringValue value)
{
	return GetWorld()->GetResManager()->GetStringLib().Has(value);
}

bool Menu::HasString(const std::string& value)
{
	return GetWorld()->GetResManager()->GetStringLib().Has(value);
}

struct Currency
{
	static char const sep = ',';
	static int const group_size = 3;
private:
	int val;
	std::string unit;
public:
	Currency(int val, std::string unit): val(val), unit(unit) {}

	friend std::ostream& operator<<(std::ostream& out, Currency const& v)
	{
		// currently ignores stream width and fill
		std::ostringstream ss;
		bool const neg = v.val < 0;
		int const val = (neg ? -v.val : v.val);
		if (neg) out << '-';
		ss << val;
		std::string const s = ss.str();
		int sn = s.size() % v.group_size;
		std::string::size_type n = sn;
		if (n) out << s.substr(0, n);
		for (; n < s.size(); n += v.group_size) {
			if (sn != 0 || n > 0)
				out << sep;
			out << s.substr(n, v.group_size);
		}
		out << ' ' << v.unit;
		return out;
	}
};

lsl::string Menu::FormatCurrency(int val, lsl::string unit)
{
	std::stringstream sstream;
	sstream << Currency(val, unit);
	
	return sstream.str();
}

const graph::DisplayModes& Menu::GetDisplayModes()
{
	return GetWorld()->GetGraph()->GetDisplayModes();
}

bool Menu::FindNearMode(const lsl::Point& resolution, graph::DisplayMode& mode)
{
	return GetWorld()->GetGraph()->FindNearMode(resolution, mode);
}

lsl::Point Menu::GetDisplayMode()
{
	return GetWorld()->GetView()->GetDesc().resolution;
}

void Menu::SetDisplayMode(const lsl::Point& resolution)
{
	View::Desc desc = GetWorld()->GetView()->GetDesc();

	if (desc.resolution.x != resolution.x || desc.resolution.y != resolution.y)
	{
		desc.resolution = resolution;
		GetWorld()->GetView()->Reset(desc);
	}
}

int Menu::GetDisplayModeIndex()
{
	int modeInd = 0;
	graph::DisplayMode mode;	
	mode.width = GetDisplayMode().x;
	mode.height = GetDisplayMode().y;

	FindNearMode(GetDisplayMode(), mode);

	for (unsigned i = 0; i < GetDisplayModes().size(); ++i)
		if (mode.width == GetDisplayModes()[i].width && mode.height == GetDisplayModes()[i].height)
		{
			modeInd = i;
			break;
		}

	return modeInd;
}

bool Menu::GetFullScreen()
{
	return _game->fullScreen();
}

void Menu::SetFullScreen(bool value)
{
	_game->fullScreen(value);
}

bool Menu::GetDisableVideo()
{
	return _game->disableVideo();
}

void Menu::SetDisableVideo(bool value)
{
	_game->disableVideo(value);
}

void Menu::SaveGameOpt()
{
	_game->SaveConfig();
}

const lsl::string& Menu::GetLanguage() const
{
	return _game->GetLanguage();
}

void Menu::SetLanguage(const lsl::string& value)
{
	return _game->SetLanguage(value);
}

const Language* Menu::GetLanguageParam() const
{
	return _game->GetLanguageParam();
}

int Menu::GetLanguageIndex() const
{
	return _game->GetLanguageIndex();
}

void Menu::Pause(bool pause)
{
	if (IsNetGame())
		GetNet()->race()->Pause(pause);
	else
		_game->Pause(pause);
}

bool Menu::IsPaused()
{
	return _game->IsPaused();
}

void Menu::PlaySound(snd::Sound* sound)
{
	if (sound)
	{
		StopSound();

		_audioSource->SetSound(sound);
		_audioSource->SetPos(0);
		_audioSource->Play();
	}
}

void Menu::PlaySound(const lsl::string& soundName)
{
	PlaySound(GetSound(soundName));
}

void Menu::StopSound()
{
	_audioSource->Stop();	
}

void Menu::PlayMusic(snd::Sound* sound, const lsl::string& name, const lsl::string& band, bool showInfo)
{
	GameMode::Track track;
	track.SetSound(sound);
	track.band = band;
	track.name = name;

	_game->PlayMusic(track, NULL, 0, showInfo);
}

void Menu::PlayMusic(const lsl::string& soundName, const lsl::string& name, const lsl::string& band, bool showInfo)
{
	PlayMusic(GetSound(soundName), name, band, showInfo);
}

void Menu::StopMusic()
{
	_game->StopMusic();
}

void Menu::PlayMovie(const std::string& name)
{
	return _game->PlayMovie(name);
}

bool Menu::IsMoviePlaying() const
{
	return _game->IsMoviePlaying();
}

void Menu::RegNavElements(gui::Widget* key, const NavElements& value)
{
	NavElementsList::iterator iter = GetNavElements(key);

	if (iter == _navElementsList.end())
	{
		key->AddRef();
		_navElementsList.push_back(std::make_pair(key, value));
	}
	else
		iter->second = value;
}

void Menu::UnregNavElements(gui::Widget* key)
{
	NavElementsList::iterator iter = GetNavElements(key);
	if (iter == _navElementsList.end())
		return;

	key->Release();
	_navElementsList.erase(iter);
}

void Menu::SetNavElements(gui::Widget* key, bool reg, NavElement elements[], int count)
{
	if (reg)
	{
		NavElements navElements;

		for (int i = 0; i < count; ++i)
		{
			navElements[elements[i].widget] = elements[i];
			elements[i].widget->SetFocused(false, true);
		}

		RegNavElements(key, navElements);
	}
	else
		UnregNavElements(key);
}

Menu::NavElementsList::iterator Menu::GetNavElements(gui::Widget* key)
{
	for (NavElementsList::iterator iter = _navElementsList.begin(); iter != _navElementsList.end(); ++iter)
		if (iter->first == key)
			return iter;
	return _navElementsList.end();
}

}

}