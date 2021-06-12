#include "stdafx.h"
#include "game\Menu.h"

#include "game\OptionsMenu.h"
#include "game\World.h"

namespace r3d
{

namespace game
{

namespace n
{

GameFrame::GameFrame(Menu* menu, OptionsMenu* optionsMenu, gui::Widget* parent): MenuFrame(menu, parent), _optionsMenu(optionsMenu), _gridScroll(0)
{
	D3DXCOLOR color1(0xffafafaf);

	StringList diffStr;
	for (int i = 0; i < cDifficultyEnd; ++i)
		diffStr.push_back(GetString(cDifficultyStr[i]));

	const StringValue cPrefCameraStr[GameMode::cPrefCameraEnd] = {svCameraSecView, svCameraOrtho};

	StringList cameraStr;
	for (int i = 0; i < GameMode::cPrefCameraEnd; ++i)
		cameraStr.push_back(GetString(cPrefCameraStr[i]));

	StringList cameraDistStr;
	for (int i = 0; i < 5; ++i)
		cameraDistStr.push_back(lsl::StrFmt("%0.2f x", 1.0f + i * 0.25f));

	StringList upgradeMaxLevel;
	for (int i = 0; i < Garage::cUpgCntLevel; ++i)
		upgradeMaxLevel.push_back(lsl::StrFmt("%d", i + 1));

	StringList weaponMaxLevel;
	for (int i = 1; i <= Garage::cWeaponMaxLevel; ++i)
		weaponMaxLevel.push_back(lsl::StrFmt("%d", i));

	StringList numPlayersStr;
	for (int i = 2; i <= Race::cMaxPlayers; ++i)
		numPlayersStr.push_back(lsl::StrFmt("%d", i));

	StringList numComputersStr;
	for (int i = 0; i <= Race::cMaxPlayers - 1; ++i)
		numComputersStr.push_back(lsl::StrFmt("%d", i));

	StringList lapsCountStr;
	for (int i = 1; i <= 8; ++i)
		lapsCountStr.push_back(lsl::StrFmt("%d", i));

	StringList onOffLevel;
	onOffLevel.push_back(GetString(svOff));
	onOffLevel.push_back(GetString(svOn));

	StringList itemsStepper[cStepperEnd] = {cameraStr, cameraDistStr, onOffLevel, diffStr, onOffLevel, upgradeMaxLevel, weaponMaxLevel, numPlayersStr, numComputersStr, lapsCountStr, onOffLevel, onOffLevel};

	lsl::string strLabels[cLabelEnd] = {_SC(svCamera), "svCameraDist", _SC(svEnableHUD), _SC(svDifficulty), _SC(svSpringBorders), _SC(svUpgradeMaxLevel), _SC(svWeaponMaxLevel), _SC(svMaxPlayers), _SC(svMaxComputers), _SC(svLapsCount), _SC(svEnableMineBug), "svDisableVideo"};
	std::string fontLabels[cLabelEnd] = {"Small", "Small", "Small", "Small", "Small", "Small", "Small", "Small", "Small", "Small", "Small", "Small"};
	gui::Text::HorAlign horLabels[cLabelEnd] = {gui::Text::haLeft, gui::Text::haLeft, gui::Text::haLeft, gui::Text::haLeft, gui::Text::haLeft, gui::Text::haLeft, gui::Text::haLeft, gui::Text::haLeft, gui::Text::haLeft, gui::Text::haLeft, gui::Text::haLeft, gui::Text::haLeft};
	gui::Text::VertAlign vertLabels[cLabelEnd] = {gui::Text::vaCenter, gui::Text::vaCenter, gui::Text::vaCenter, gui::Text::vaCenter, gui::Text::vaCenter, gui::Text::vaCenter, gui::Text::vaCenter, gui::Text::vaCenter, gui::Text::vaCenter, gui::Text::vaCenter, gui::Text::vaCenter, gui::Text::vaCenter};
	D3DXCOLOR colorLabels[cLabelEnd] = {color1, color1, color1, color1, color1, color1, color1, color1, color1, color1, color1, color1};

	_grid = menu->CreateGrid(root(), NULL, gui::Grid::gsVertical);
	_grid->SetAlign(gui::Widget::waLeftTop);

	for (int i = 0; i < cLabelEnd; ++i)
	{
		_itemsBg[i] = menu->CreatePlane(_grid, NULL, "GUI\\labelBg1.png", true, IdentityVec2, gui::Material::bmTransparency);
		_itemsBg[i]->SetAlign(gui::Widget::waLeft);
	}
	
	for (int i = 0; i < cLabelEnd; ++i)	
		_labels[i] = menu->CreateLabel(GetString(strLabels[i]), _itemsBg[i], fontLabels[i], NullVec2, horLabels[i], vertLabels[i], colorLabels[i]);

	for (int i = 0; i < cStepperEnd; ++i)
	{
		_steppers[i] = _optionsMenu->CreateStepper(itemsStepper[i], _itemsBg[i], this);
		_steppers[i]->SetAlign(gui::Widget::waLeft);
		_steppers[i]->SetTag(1);
	}

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

	LoadCfg();
}

GameFrame::~GameFrame()
{
	menu()->ReleaseWidget(_upArrow);
	menu()->ReleaseWidget(_downArrow);

	for (int i = 0; i < cStepperEnd; ++i)
		menu()->ReleaseWidget(_steppers[i]);

	for (int i = 0; i < cLabelEnd; ++i)
		menu()->ReleaseWidget(_labels[i]);

	for (int i = 0; i < cLabelEnd; ++i)
		menu()->ReleaseWidget(_itemsBg[i]);

	menu()->ReleaseWidget(_grid);
}

void GameFrame::AdjustGrid(const D3DXVECTOR2& vpSize)
{
	_grid->SetPos(-235.0f, -178.0f);
	_grid->cellSize(D3DXVECTOR2(0, 50.0f));

	int i = 0;
	int cellsOnLine = 1;
	int cellsGrid = 7;
	
	for (gui::Widget::Children::const_iterator iter = _grid->GetChildren().begin(); iter != _grid->GetChildren().end(); ++iter)
	{
		gui::Widget* child = *iter;

		//is disposed
		if (child->disposed())
			continue;

		if (i >= _gridScroll * cellsOnLine && i < _gridScroll * cellsOnLine + cellsGrid)
		{
			child->SetVisible(true);

			for (gui::Widget::Children::const_iterator iter2 = (*iter)->GetChildren().begin(); iter2 != (*iter)->GetChildren().end(); ++iter2)
			{
				if ((*iter2)->GetTag() == 1)
				{
					gui::Material* fon = static_cast<gui::StepperBox*>(*iter2)->GetArrow();
					(*iter2)->SetEnabled(true && (fon == NULL || fon->GetAlpha() == 1.0f));
				}
				else
					(*iter2)->SetEnabled(true);
			}
		}
		else
		{
			child->SetVisible(false);

			for (gui::Widget::Children::const_iterator iter2 = (*iter)->GetChildren().begin(); iter2 != (*iter)->GetChildren().end(); ++iter2)
				(*iter2)->SetEnabled(false);
		}

		++i;
	}

	_grid->Reposition();

	menu()->SetButtonEnabled(_upArrow, _gridScroll > 0);
	menu()->SetButtonEnabled(_downArrow, _gridScroll < std::max((int)ceil(i / (float)cellsOnLine) - cellsGrid/cellsOnLine, 0));
}

void GameFrame::ScrollGrid(int step)
{
	_gridScroll += step;
	AdjustGrid(menu()->GetGUI()->GetVPSize());
}

void GameFrame::LoadCfg()
{	
	menu()->SetStepperEnabled(_steppers[dbDiff], menu()->enabledOptionDiff());	
	menu()->SetStepperEnabled(_steppers[dbLapsCount], menu()->enabledLapsCount());	
	menu()->SetStepperEnabled(_steppers[dbUpgradeMaxLevel], menu()->EnabledOptionUpgradeMaxLevel());
	menu()->SetStepperEnabled(_steppers[dbWeaponMaxLevel], menu()->EnabledOptionWeaponMaxLevel());
	menu()->SetStepperEnabled(_steppers[dbMaxPlayers], menu()->enabledMaxPlayers());
	menu()->SetStepperEnabled(_steppers[dbMaxComputers], menu()->enabledMaxComputers());
	menu()->SetStepperEnabled(_steppers[dbSpringBorders], menu()->enabledSpringBorders());
	menu()->SetStepperEnabled(_steppers[dbEnableMineBug], menu()->activeEnableMineBug());

	_steppers[dbEnableHUD]->SetSelIndex(menu()->enableHUD());
	_steppers[dbDiff]->SetSelIndex(menu()->currentDiff());
	_steppers[dbLapsCount]->SetSelIndex(menu()->lapsCount() - 1);
	_steppers[dbCamera]->SetSelIndex(menu()->GetGame()->GetPrefCamera());
	_steppers[dbCameraDist]->SetSelIndex(static_cast<int>(Round((menu()->GetGame()->GetCameraDistance() - 1.0f)/0.25f)));
	_steppers[dbUpgradeMaxLevel]->SetSelIndex(menu()->GetUpgradeMaxLevel());
	_steppers[dbWeaponMaxLevel]->SetSelIndex(menu()->GetWeaponMaxLevel() - 1);

	_steppers[dbMaxPlayers]->SetSelIndex((int)menu()->maxPlayers() - 2);
	_steppers[dbMaxComputers]->SetSelIndex(menu()->maxComputers());
	_steppers[dbSpringBorders]->SetSelIndex(menu()->springBorders());
	_steppers[dbEnableMineBug]->SetSelIndex(menu()->enableMineBug());
	_steppers[dbDisableVideo]->SetSelIndex(menu()->GetDisableVideo());	
}

void GameFrame::OnShow(bool value)
{
	if (value)
	{
		_gridScroll = 0;
		_optionsMenu->RegNavElements(_steppers, cStepperEnd, NULL, 0, NULL, 0, _upArrow, _downArrow);
	}
}

void GameFrame::OnAdjustLayout(const D3DXVECTOR2& vpSize)
{
	AdjustGrid(vpSize);

	int index = 0;
	for (int i = 0; i < cLabelEnd; ++i)
	{
		_labels[i]->SetPos(35.0f, 0.0f);
	}

	index = 0;
	for (int i = 0; i < cStepperEnd; ++i)
	{
		_steppers[i]->SetPos(495.0f, 0.0f);
	}

	_upArrow->SetPos(150.0f, -200.0f);
	_downArrow->SetPos(150.0f, 195.0f);
}

void GameFrame::OnInvalidate()
{	
}

bool GameFrame::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
{
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

	return false;
}

bool GameFrame::ApplyChanges()
{
	for (int i = 0; i < cStepperEnd; ++i)
	{
		int val = _steppers[i]->GetSelIndex();

		switch (i)
		{
		case dbEnableHUD:
			menu()->enableHUD(val != 0 ? true : false);
			break;

		case dbDiff:
			menu()->currentDiff(Difficulty(val));
			break;

		case dbLapsCount:
			menu()->lapsCount(val + 1);
			break;

		case dbCamera:
			menu()->GetGame()->SetPrefCamera(GameMode::PrefCamera(val));
			break;

		case dbCameraDist:
			menu()->GetGame()->SetCameraDistance(1.0f + val * 0.25f);
			break;

		case dbUpgradeMaxLevel:			
			menu()->SetUpgradeMaxLevel(val);
			break;

		case dbWeaponMaxLevel:
			menu()->SetWeaponMaxLevel(val + 1);
			break;

		case dbMaxPlayers:
			menu()->maxPlayers(val + 2);
			break;

		case dbMaxComputers:
			menu()->maxComputers(val);
			break;

		case dbSpringBorders:
			menu()->springBorders(val == 1);
			break;

		case dbEnableMineBug:
			menu()->enableMineBug(val == 1);
			break;

		case dbDisableVideo:
			menu()->SetDisableVideo(val == 1);
			break;
		}
	}

	return false;
}

void GameFrame::CancelChanges()
{	
}




MediaFrame::MediaFrame(Menu* menu, OptionsMenu* optionsMenu, gui::Widget* parent): MenuFrame(menu, parent), _optionsMenu(optionsMenu)
{
	StringValue gfxLevel[] = {svLow, svMiddle, svHigh};
	std::string msLevel[] = {"linear", "af 2x", "af 4x"};
	std::string aaLevel[] = {"none", "aa 2x", "aa 4x", "aa 8x"};

	StringList shadowLevel;
	for (unsigned i = 0; i < ARRAY_LENGTH(gfxLevel); ++i)
		if (menu->GetEnv()->IsShadowQualitySupported((Environment::Quality)i))
			shadowLevel.push_back(GetString(gfxLevel[i]));

	StringList lightLevel;
	for (unsigned i = 0; i < ARRAY_LENGTH(gfxLevel); ++i)
		if (menu->GetEnv()->IsLightQualitySupported((Environment::Quality)i))
			lightLevel.push_back(GetString(gfxLevel[i]));

	StringList postEffLevel;
	for (unsigned i = 0; i < ARRAY_LENGTH(gfxLevel); ++i)
		if (menu->GetEnv()->IsPostEffQualitySupported((Environment::Quality)i))
			postEffLevel.push_back(GetString(gfxLevel[i]));

	StringList envLevel;
	for (unsigned i = 0; i < ARRAY_LENGTH(gfxLevel); ++i)
		if (menu->GetEnv()->IsEnvQualitySupported((Environment::Quality)i))
			envLevel.push_back(GetString(gfxLevel[i]));

	StringList onOffLevel;
	onOffLevel.push_back(GetString(svOff));
	onOffLevel.push_back(GetString(svOn));

	StringList fltLevel;
	for (int i = 0; i < ARRAY_LENGTH(aaLevel); ++i)
		if (menu->GetEnv()->IsFilteringSupported((Environment::Filtering)i))
			fltLevel.push_back(aaLevel[i]);

	StringList msFltLevel;
	for (int i = 0; i < ARRAY_LENGTH(msLevel); ++i)
		if (menu->GetEnv()->IsMultisamplingSupported((Environment::Multisampling)i))
			msFltLevel.push_back(msLevel[i]);

	StringList resolutionStr;
	graph::DisplayModes modes = menu->GetDisplayModes();
	for (unsigned i = 0; i < modes.size(); ++i)
		resolutionStr.push_back(lsl::StrFmt("%d x %d", modes[i].width, modes[i].height));

	StringList itemsStepper[cStepperEnd] = {resolutionStr, fltLevel, msFltLevel, shadowLevel, envLevel, lightLevel, postEffLevel, onOffLevel};

	D3DXCOLOR color1(0xffafafaf);

	lsl::string strLabels[cLabelEnd] = {_SC(svResolution), _SC(svFiltering), _SC(svMultisampling), _SC(svShadow), _SC(svEnv), _SC(svLight), _SC(svPostProcess), "svWindowMode"};
	std::string fontLabels[cLabelEnd] = {"Small", "Small", "Small", "Small", "Small", "Small", "Small", "Small"};
	gui::Text::HorAlign horLabels[cLabelEnd] = {gui::Text::haLeft, gui::Text::haLeft, gui::Text::haLeft, gui::Text::haLeft, gui::Text::haLeft, gui::Text::haLeft, gui::Text::haLeft, gui::Text::haLeft};
	gui::Text::VertAlign vertLabels[cLabelEnd] = {gui::Text::vaCenter, gui::Text::vaCenter, gui::Text::vaCenter, gui::Text::vaCenter, gui::Text::vaCenter, gui::Text::vaCenter, gui::Text::vaCenter, gui::Text::vaCenter};
	D3DXCOLOR colorLabels[cLabelEnd] = {color1, color1, color1, color1, color1, color1, color1, color1};

	for (int i = 0; i < cLabelEnd; ++i)
	{
		_itemsBg[i] = menu->CreatePlane(root(), NULL, "GUI\\labelBg1.png", true, IdentityVec2, gui::Material::bmTransparency);
		_itemsBg[i]->SetAlign(gui::Widget::waLeft);
	}

	gui::Widget* labelsParent[cLabelEnd] = {root(), root(), root(), root(), root(), root(), root(), root()};
	for (int i = 0; i < cLabelEnd; ++i)	
		_labels[i] = menu->CreateLabel(GetString(strLabels[i]), labelsParent[i], fontLabels[i], NullVec2, horLabels[i], vertLabels[i], colorLabels[i]);

	for (int i = 0; i < cStepperEnd; ++i)
	{
		_steppers[i] = _optionsMenu->CreateStepper(itemsStepper[i], root(), this);
		_steppers[i]->SetAlign(gui::Widget::waLeft);
	}

	LoadCfg();
}

MediaFrame::~MediaFrame()
{
	for (int i = 0; i < cLabelEnd; ++i)
		menu()->ReleaseWidget(_itemsBg[i]);

	for (int i = 0; i < cLabelEnd; ++i)
		menu()->ReleaseWidget(_labels[i]);

	for (int i = 0; i < cStepperEnd; ++i)
		menu()->ReleaseWidget(_steppers[i]);
}

void MediaFrame::LoadCfg()
{
	_steppers[dbResolution]->SetSelIndex(menu()->GetDisplayModeIndex());
	_steppers[dbFiltering]->SetSelIndex(menu()->GetEnv()->GetFiltering());
	_steppers[dbMultisampling]->SetSelIndex(menu()->GetEnv()->GetMultisampling());
	_steppers[dbShadow]->SetSelIndex(menu()->GetEnv()->GetShadowQuality());
	_steppers[dbEnv]->SetSelIndex(menu()->GetEnv()->GetEnvQuality());
	_steppers[dbLight]->SetSelIndex(menu()->GetEnv()->GetLightQuality());
	_steppers[dbPostProcess]->SetSelIndex(menu()->GetEnv()->GetPostEffQuality());	
	_steppers[dbWindowMode]->SetSelIndex(menu()->GetFullScreen() == false);
}

void MediaFrame::OnShow(bool value)
{
	if (value)
	{
		_optionsMenu->RegNavElements(_steppers, cStepperEnd, NULL, 0);
	}
}

void MediaFrame::OnAdjustLayout(const D3DXVECTOR2& vpSize)
{
	for (int i = 0; i < cLabelEnd; ++i)
	{
		_labels[i]->SetPos(-200.0f, -174.0f + i * 50.0f);
	}

	for (int i = 0; i < cStepperEnd; ++i)
	{
		_steppers[i]->SetPos(260.0f, -174.0f + i * 50.0f);
	}

	for (int i = 0; i < cLabelEnd; ++i)
	{
		_itemsBg[i]->SetPos(-235.0f, -174.0f + i * 50.0f);
	}
}

void MediaFrame::OnInvalidate()
{	
}

bool MediaFrame::ApplyChanges()
{
	for (int i = 0; i < cStepperEnd; ++i)
	{
		int val = _steppers[i]->GetSelIndex();

		switch (i)
		{
		case dbResolution:
			if (val >= 0 && static_cast<unsigned>(val) < menu()->GetDisplayModes().size())
			{
				graph::DisplayMode mode = menu()->GetDisplayModes()[val];
				menu()->SetDisplayMode(lsl::Point(mode.width, mode.height));
			}
			break;

		case dbFiltering:
			menu()->GetEnv()->SetFiltering(Environment::Filtering(val));
			break;

		case dbMultisampling:
			menu()->GetEnv()->SetMultisampling(Environment::Multisampling(val));
			break;

		case dbShadow:
			menu()->GetEnv()->SetShadowQuality(Environment::Quality(val));
			break;

		case dbEnv:
			menu()->GetEnv()->SetEnvQuality(Environment::Quality(val));
			break;

		case dbLight:
			menu()->GetEnv()->SetLightQuality(Environment::Quality(val));
			break;

		case dbPostProcess:
			menu()->GetEnv()->SetPostEffQuality(Environment::Quality(val));
			break;

		case dbWindowMode:
			menu()->SetFullScreen(val == 0);
			break;
		}
	}

	return false;
}

void MediaFrame::CancelChanges()
{
}




NetworkTab::NetworkTab(Menu* menu, OptionsMenu* optionsMenu, gui::Widget* parent): MenuFrame(menu, parent), _optionsMenu(optionsMenu)
{
	D3DXCOLOR color1(0xffafafaf);

	StringList languageStr;
	for (Languages::const_iterator iter = menu->GetGame()->GetLanguages().begin(); iter != menu->GetGame()->GetLanguages().end(); ++iter)
		languageStr.push_back(iter->name);

	StringList commentatorStr;
	for (CommentatorStyles::const_iterator iter = menu->GetGame()->GetCommentatorStyles().begin(); iter != menu->GetGame()->GetCommentatorStyles().end(); ++iter)
		commentatorStr.push_back(iter->name);

	StringList itemsStepper[cStepperEnd] = {languageStr, commentatorStr};

	StringValue strLabels[cLabelEnd] = {svLanguage, svCommentator, svMusic, svSound, svSoundDicter};
	std::string fontLabels[cLabelEnd] = {"Small", "Small", "Small", "Small", "Small"};
	gui::Text::HorAlign horLabels[cLabelEnd] = {gui::Text::haLeft, gui::Text::haLeft, gui::Text::haLeft, gui::Text::haLeft, gui::Text::haLeft};
	gui::Text::VertAlign vertLabels[cLabelEnd] = {gui::Text::vaCenter, gui::Text::vaCenter, gui::Text::vaCenter, gui::Text::vaCenter, gui::Text::vaCenter};
	D3DXCOLOR colorLabels[cLabelEnd] = {color1, color1, color1, color1, color1};
	gui::Widget* labelsParent[cLabelEnd] = {root(), root(), root(), root(), root()};

	for (int i = 0; i < cLabelEnd; ++i)
	{
		_itemsBg[i] = menu->CreatePlane(root(), NULL, "GUI\\labelBg1.png", true, IdentityVec2, gui::Material::bmTransparency);
		_itemsBg[i]->SetAlign(gui::Widget::waLeft);
	}
	
	for (int i = 0; i < cLabelEnd; ++i)	
		_labels[i] = menu->CreateLabel(strLabels[i], labelsParent[i], fontLabels[i], NullVec2, horLabels[i], vertLabels[i], colorLabels[i]);

	for (int i = 0; i < cStepperEnd; ++i)
	{
		_steppers[i] = _optionsMenu->CreateStepper(itemsStepper[i], root(), this);
		_steppers[i]->SetAlign(gui::Widget::waLeft);
	}

	for (int i = 0; i < cVolumeBarEnd; ++i)
	{
		_volumeBars[i] = _optionsMenu->CreateVolumeBar(root(), this);
		_volumeBars[i]->SetAlign(gui::Widget::waLeft);
	}

	LoadCfg();
}

NetworkTab::~NetworkTab()
{
	for (int i = 0; i < cLabelEnd; ++i)
		menu()->ReleaseWidget(_itemsBg[i]);

	for (int i = 0; i < cStepperEnd; ++i)
		menu()->ReleaseWidget(_steppers[i]);

	for (int i = 0; i < cLabelEnd; ++i)
		menu()->ReleaseWidget(_labels[i]);

	for (int i = 0; i < cVolumeBarEnd; ++i)
		menu()->ReleaseWidget(_volumeBars[i]);
}

void NetworkTab::LoadCfg()
{
	_defVolumes[tbMusic] = menu()->GetVolume(Logic::scMusic);
	_defVolumes[tbSound] = menu()->GetVolume(Logic::scEffects);
	_defVolumes[tbDicter] = menu()->GetVolume(Logic::scVoice);

	for (int i = 0; i < cVolumeBarEnd; ++i)
		_volumeBars[i]->SetVolume(_defVolumes[i] / 2.0f);

	_steppers[dbLanguage]->SetSelIndex(menu()->GetGame()->GetLanguageIndex());
	_steppers[dbCommentator]->SetSelIndex(menu()->GetGame()->FindCommentatorStyleIndex(menu()->GetGame()->GetCommentatorStyle()));
}

void NetworkTab::ApplyVolume(bool revertChanges)
{
	if (revertChanges)
	{
		for (int i = 0; i < cVolumeBarEnd; ++i)
			_volumeBars[i]->SetVolume(_defVolumes[i] / 2.0f);
	}

	menu()->SetVolume(Logic::scMusic, _volumeBars[tbMusic]->GetVolume() * 2.0f);
	menu()->SetVolume(Logic::scEffects, _volumeBars[tbSound]->GetVolume() * 2.0f);
	menu()->SetVolume(Logic::scVoice, _volumeBars[tbDicter]->GetVolume() * 2.0f);
}

void NetworkTab::OnShow(bool value)
{
	if (value)
		_optionsMenu->RegNavElements(_steppers, cStepperEnd, _volumeBars, cVolumeBarEnd);
}

void NetworkTab::OnAdjustLayout(const D3DXVECTOR2& vpSize)
{
	for (int i = 0; i < cLabelEnd; ++i)
	{
		_labels[i]->SetPos(-200.0f, -174.0f + i * 50.0f);
	}

	for (int i = 0; i < cStepperEnd; ++i)
	{
		_steppers[i]->SetPos(260.0f, -174.0f + i * 50.0f);
	}

	for (int i = 0; i < cVolumeBarEnd; ++i)
	{
		_volumeBars[i]->SetPos(260.0f, -174.0f + (i + cStepperEnd) * 50.0f);
	}

	for (int i = 0; i < cLabelEnd; ++i)
	{
		_itemsBg[i]->SetPos(-235.0f, -174.0f + i * 50.0f);
	}
}

void NetworkTab::OnInvalidate()
{	
}

bool NetworkTab::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
{
	for (int i = 0; i < cVolumeBarEnd; ++i)		
		if (sender == _volumeBars[i])
		{
			ApplyVolume(false);
			return true;
		}

	return false;
}

bool NetworkTab::ApplyChanges()
{
	bool res = false;

	for (int i = 0; i < cStepperEnd; ++i)
	{
		int val = _steppers[i]->GetSelIndex();

		switch (i)
		{
		case dbLanguage:
		{
			int curVal = menu()->GetGame()->GetLanguageIndex();
			if (val != -1 && val != curVal)
			{
				menu()->GetGame()->SetLanguage(menu()->GetGame()->GetLanguages()[val].name);
				res |= true;
			}
			break;
		}

		case dbCommentator:
		{
			if (val != -1)
				menu()->GetGame()->SetCommentatorStyle(menu()->GetGame()->GetCommentatorStyles()[val].name);
			break;
		}
		}
	}

	ApplyVolume(false);

	return res;
}

void NetworkTab::CancelChanges()
{
	ApplyVolume(true);
}




ControlsFrame::ControlsFrame(Menu* menu, OptionsMenu* optionsMenu, gui::Widget* parent): MenuFrame(menu, parent), _optionsMenu(optionsMenu), _gridScroll(0)
{
	D3DXCOLOR color1(0xffafafaf);

	_grid = menu->CreateGrid(root(), NULL, gui::Grid::gsVertical);
	_grid->SetAlign(gui::Widget::waLeftTop);

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

	for (int i = 0; i < cControllerTypeEnd; ++i)
	{
		_controllerIcons[i] = menu->CreatePlane(root(), NULL, lsl::StrFmt("GUI\\%s.png", cControllerTypeStr[i].c_str()), true, IdentityVec2, gui::Material::bmTransparency);
	}

	LoadCfg();
}

ControlsFrame::~ControlsFrame()
{
	menu()->GetControl()->RemoveEvent(this);

	for (int i = 0; i < cControllerTypeEnd; ++i)
		menu()->ReleaseWidget(_controllerIcons[i]);

	menu()->ReleaseWidget(_upArrow);
	menu()->ReleaseWidget(_downArrow);
	menu()->ReleaseWidget(_grid);
}

void ControlsFrame::AdjustGrid(const D3DXVECTOR2& vpSize)
{
	_grid->SetPos(-235.0f, -130.0f);

	int i = 0;
	int cellsOnLine = 1;
	int cellsGrid = 6;
	
	for (InputBoxes::const_iterator iter = _boxes.begin(); iter != _boxes.end(); ++iter)
	{
		const InputBox& box = *iter;

		gui::Widget* child = box.sprite;
		//is disposed
		if (child->disposed())
			continue;

		if (i >= _gridScroll * cellsOnLine && i < _gridScroll * cellsOnLine + cellsGrid)
		{
			child->SetVisible(true);
			box.keys[0]->SetEnabled(true);
			box.keys[1]->SetEnabled(true);
		}
		else
		{
			child->SetVisible(false);
			box.keys[0]->SetEnabled(false);
			box.keys[1]->SetEnabled(false);
		}
		
		box.sprite->SetPos(0, 0);
		box.label->SetPos(-box.sprite->GetSize().x/2 + 35, 0);

		for (int j = 0; j < cControllerTypeEnd; ++j)
		{
			box.keys[j]->SetPos(315.0f + j * 180.0f, 0);
		}

		++i;
	}

	_grid->Reposition();

	menu()->SetButtonEnabled(_upArrow, _gridScroll > 0);
	menu()->SetButtonEnabled(_downArrow, _gridScroll < std::max((int)ceil(i / (float)cellsOnLine) - cellsGrid/cellsOnLine, 0));
}

void ControlsFrame::ScrollGrid(int step)
{
	_gridScroll += step;
	AdjustGrid(menu()->GetGUI()->GetVPSize());
}

void ControlsFrame::LoadCfg()
{		
	D3DXVECTOR2 cellSize = NullVec2;	
	unsigned itemCount = 0;
	InputBoxes::iterator iter = _boxes.begin();

	for (unsigned i = 0; i < cGameActionUserEnd; ++i)
	{
		GameAction action = (GameAction)i;

		InputBox* item = iter != _boxes.end() ? &(*iter) : NULL;
		if (item == NULL)
		{
			InputBox newItem;
			newItem.sprite = menu()->CreatePlane(_grid, NULL, "GUI\\labelBg2.png", true, IdentityVec2, gui::Material::bmTransparency);
			newItem.label = menu()->CreateLabel("", newItem.sprite, "Small", NullVec2, gui::Text::haLeft, gui::Text::vaCenter, 0xffafafaf);
			
			for (int j = 0; j < cControllerTypeEnd; ++j)			
			{
				newItem.keys[j] = menu()->CreateMenuButton(svNull, "Small", "GUI\\keyBg.png", "GUI\\keyBgSel.png", newItem.sprite, this, IdentityVec2, gui::Button::bsSimple, clrBlack, Menu::ssButton1);
				newItem.keys[j]->GetOrCreateTextSelMaterial()->SetColor(clrWhite);
			}	

			_boxes.push_back(newItem);
			item = &_boxes.back();
			iter = _boxes.end();			
		}
		else
			++iter;

		item->label->SetText(GetString(cGameActionStr[action]));

		for (int j = 0; j < cControllerTypeEnd; ++j)
		{
			ControllerType controller = (ControllerType)j;	
			VirtualKey virtualKey = menu()->GetControl()->GetGameKey(controller, action);
			VirtualKeyInfo info = menu()->GetControl()->GetVirtualKeyInfo(controller, virtualKey);

			item->keys[j]->SetText(info.name);
			item->keys[j]->Select(false, true);

			item->virtualKeys[j] = virtualKey;
		}
		
		cellSize = item->sprite->GetSize();
		++itemCount;
	}

	while (_boxes.size() > itemCount)
	{		
		menu()->ReleaseWidget(_boxes.back().sprite);
		_boxes.erase(--_boxes.end());
	}

	_grid->cellSize(cellSize + D3DXVECTOR2(0, 7.0f));
}

void ControlsFrame::OnShow(bool value)
{
	if (value)
	{
		_gridScroll = 0;
		menu()->GetControl()->InsertEvent(this);

		std::vector<OptionsMenu::ButtonLR> buttons;
		for (InputBoxes::const_iterator iter = _boxes.begin(); iter != _boxes.end(); ++iter)
		{
			OptionsMenu::ButtonLR button = {iter->keys[0], iter->keys[1]};
			buttons.push_back(button);
		}

		_optionsMenu->RegNavElements(NULL, 0, NULL, 0, &buttons.front(), buttons.size(), _upArrow, _downArrow);
	}
	else
	{
		menu()->GetControl()->RemoveEvent(this);
	}
}

void ControlsFrame::OnAdjustLayout(const D3DXVECTOR2& vpSize)
{
	for (int i = 0; i < cControllerTypeEnd; ++i)
	{
		_controllerIcons[i]->SetPos(255.0f + 180 * i, -190.0f);
	}

	AdjustGrid(vpSize);

	_upArrow->SetPos(150.0f, -150.0f);
	_downArrow->SetPos(150.0f, 195.0f);
}

void ControlsFrame::OnInvalidate()
{
}

bool ControlsFrame::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
{
	if (sender == menu()->GetAcceptSender())
	{
		for (int i = 0; i < cControllerTypeEnd; ++i)
			for (int j = 0; j < cGameActionUserEnd; ++j)
			{
				if (_boxes[j].keys[i]->IsSelected())
				{
					if (menu()->GetAcceptResultYes())
					{
						_boxes[j].virtualKeys[i] = cVirtualKeyEnd;
						_boxes[j].keys[i]->SetText(menu()->GetControl()->GetVirtualKeyInfo((ControllerType)i, cVirtualKeyEnd).name);
					}
					_boxes[j].keys[i]->Select(false, true);
				}
			}
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

	{
		static_cast<gui::Button*>(sender)->Select(true, true);

		menu()->ShowAccept(menu()->GetString("svPressKey"), menu()->GetString("svDeleteKey"), menu()->GetString(svCancel), menu()->GetGUI()->GetVPSize()/2, gui::Widget::waCenter, this, NULL, true, false, true);
	}

	return false;
}

bool ControlsFrame::OnHandleInput(const InputMessage& msg)
{
	if (msg.key != cVirtualKeyEnd && msg.state == ksDown)
	{
		for (int j = 0; j < cGameActionUserEnd; ++j)
		{
			if (_boxes[j].keys[msg.controller]->IsSelected())
			{
				_boxes[j].virtualKeys[msg.controller] = msg.key;
				_boxes[j].keys[msg.controller]->SetText(menu()->GetControl()->GetVirtualKeyInfo(msg.controller, msg.key).name);
				_boxes[j].keys[msg.controller]->Select(false, true);
				menu()->HideAccept();
				return true;
			}
		}
	}

	return false;
}

bool ControlsFrame::ApplyChanges()
{
	for (int i = 0; i < cControllerTypeEnd; ++i)
		for (int j = 0; j < cGameActionUserEnd; ++j)
			menu()->GetControl()->SetGameKey((ControllerType)i, (GameAction)j, _boxes[j].virtualKeys[i]);
	return false;
}

void ControlsFrame::CancelChanges()
{	
}




OptionsMenu::OptionsMenu(Menu* menu, gui::Widget* parent): _menu(menu), _state(msGame)
{
	D3DXCOLOR color1(0xffafafaf);

	StringValue strMenuItems[cMenuItemEnd] = {svBack, svApply};
	StringValue strStateItems[cStateEnd] = {svGame, svGraphic, svNetwork, svControls};

	StringValue strLabels[cLabelEnd] = {svOptions};
	std::string fontLabels[cLabelEnd] = {"Header"};
	gui::Text::HorAlign horLabels[cLabelEnd] = {gui::Text::haCenter};
	gui::Text::VertAlign vertLabels[cLabelEnd] = {gui::Text::vaCenter};
	D3DXCOLOR colorLabels[cLabelEnd] = {color1};

	LSL_ASSERT(menu);

	_root = _menu->GetGUI()->CreateDummy();
	_root->SetParent(parent);

	_menuBgMask = _menu->CreatePlane(_root, this, "", false, IdentityVec2, gui::Material::bmTransparency);
	_menuBgMask->GetMaterial().SetColor(D3DXCOLOR(0, 0, 0, 0.8f));
	_menuBgMask->SetAnchor(gui::Widget::waCenter);

	_menuBg = _menu->CreatePlane(_menuBgMask, this, "GUI\\optionsBg.png", true, IdentityVec2, gui::Material::bmTransparency);	

	_mediaFrame = new MediaFrame(menu, this, _menuBg);
	_gameFrame = new GameFrame(menu, this, _menuBg);
	_networkFrame = new NetworkTab(menu, this, _menuBg);
	_controlsFrame = new ControlsFrame(menu, this, _menuBg);

	gui::Widget* labelsParent[cLabelEnd] = {_menuBg};
	for (int i = 0; i < cLabelEnd; ++i)	
		_labels[i] = menu->CreateLabel(strLabels[i], labelsParent[i], fontLabels[i], NullVec2, horLabels[i], vertLabels[i], colorLabels[i]);

	for (int i = 0; i < cMenuItemEnd; ++i)
		_menuItems[i] = CreateMenuButton(strMenuItems[i], _menuBg, this);

	for (int i = 0; i < cStateEnd; ++i)
		_stateItems[i] = CreateMenuButton2(strStateItems[i], _menuBg, this);

	ApplyState();
}

OptionsMenu::~OptionsMenu()
{
	_menu->UnregNavElements(_menuItems[miBack]);

	delete _controlsFrame;
	delete _networkFrame;
	delete _gameFrame;
	delete _mediaFrame;

	_menu->ReleaseWidget(_menuBgMask);
	_menu->GetGUI()->ReleaseWidget(_root);
}

void OptionsMenu::SetSelection(State state, bool instant)
{
	for (int i = 0; i < cStateEnd; ++i)
	{
		bool selected = i == state;

		_stateItems[i]->Select(selected, instant);

		gui::Label* label = static_cast<gui::Label*>(_stateItems[i]->GetChildren().front());

		if (selected)
			label->GetMaterial().SetColor(0xffeb733e);
		else
			label->GetMaterial().SetColor(0xffafafaf);
	}
}

void OptionsMenu::ApplyState()
{
	_mediaFrame->Show(_state == msMedia);
	_gameFrame->Show(_state == msGame);
	_networkFrame->Show(_state == msNetwork);
	_controlsFrame->Show(_state == msControls);

	SetSelection(_state, true);
}

void OptionsMenu::CancelChanges()
{
	_mediaFrame->CancelChanges();
	_gameFrame->CancelChanges();
	_networkFrame->CancelChanges();
	_controlsFrame->CancelChanges();
}

bool OptionsMenu::ApplyChanges()
{
	bool res = _mediaFrame->ApplyChanges();
	res |= _gameFrame->ApplyChanges();
	res |= _networkFrame->ApplyChanges();
	res |= _controlsFrame->ApplyChanges();

	_menu->SaveGameOpt();

	return res;
}

bool OptionsMenu::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
{
	if (sender == _menu->GetMessageSender())
	{
		_menu->ShowOptions(false);
		return true;
	}

	if (sender == _menuItems[miBack])
	{
		_root->ShowModal(false);
		CancelChanges();
		_menu->ShowOptions(false);
		return true;
	}

	if (sender == _menuItems[miApply])
	{
		Environment::SyncFrameRate prevSyncFrameRate = _menu->GetEnv()->GetSyncFrameRate();
		bool needReload = ApplyChanges();

		if (prevSyncFrameRate == Environment::sfrFixed && _menu->GetEnv()->GetSyncFrameRate() != Environment::sfrFixed)
		{
			std::string message;
			if (needReload)
				message = _menu->GetString("svSyncFrameWarningNeedReload");
			else
				message = _menu->GetString("svSyncFrameWarning");
			_menu->ShowMessage(_menu->GetString(svWarning), message, _menu->GetString(svOk), _menu->GetGUI()->GetVPSize()/2, gui::Widget::waCenter, 0.0f, this);
		}
		else if (needReload)
		{
			_menu->ShowMessage(_menu->GetString(svWarning), _menu->GetString(svHintNeedReload), _menu->GetString(svOk), _menu->GetGUI()->GetVPSize()/2, gui::Widget::waCenter, 0.0f, this);
		}
		else
			_menu->ShowOptions(false);
		return true;
	}

	for (int i = 0; i < cStateEnd; ++i)
	{
		if (sender == _stateItems[i])
		{
			SetState((State)i);
			return true;
		}

		if (sender->GetData() != NULL)
		{
			gui::Button* button = lsl::StaticCast<gui::Button*>(sender->GetData());

			if (button == _stateItems[i])
			{
				SetState((State)i);
				return true;
			}
		}
	}

	return false;
}

bool OptionsMenu::OnMouseEnter(gui::Widget* sender, const gui::MouseMove& mMove)
{
	if (sender->GetData() != NULL)
	{
		gui::Button* button = lsl::StaticCast<gui::Button*>(sender->GetData());		
		button->Select(true, false);

		gui::Label* label = static_cast<gui::Label*>(button->GetChildren().front());
		label->GetMaterial().SetColor(0xffafafaf);
	}

	return false;
}

void OptionsMenu::OnMouseLeave(gui::Widget* sender, bool wasReset)
{
	if (sender->GetData() != NULL)
	{
		gui::Button* button = lsl::StaticCast<gui::Button*>(sender->GetData());		

		for (int i = 0; i < cStateEnd; ++i)
			if (button == _stateItems[i])
			{
				button->Select(_state == i, false);

				gui::Label* label = static_cast<gui::Label*>(button->GetChildren().front());
				label->GetMaterial().SetColor(_state == i ? 0xffeb733e : 0xffafafaf);
				return;
			}
	}
}

void OptionsMenu::OnFocusChanged(gui::Widget* sender)
{
	if (sender->GetChildren().size() >= 2)
	{
		if (sender->IsFocused())
			OnMouseEnter(sender->GetChildren().back(), gui::MouseMove());
		else
			OnMouseLeave(sender->GetChildren().back(), false);
	}
}

template<class _T1, class _T2> void OptionsMenu::RegSteppers(lsl::Vector<_T2>& navElements, _T1 steppers[], int stepperCount, gui::Widget* upWidgetLeft, gui::Widget* upWidgetRight, gui::Widget* downWidgetLeft, gui::Widget* downWidgetRight)
{
	for (int i = 0; i < stepperCount; ++i)
	{
		if (i >= 2)
		{
			Menu::NavElement navElement = {steppers[i - 1]->GetLeft(), {steppers[i - 1]->GetRight(), steppers[i - 1]->GetRight(), steppers[i - 2]->GetLeft(), steppers[i]->GetLeft()}, {cVirtualKeyEnd, cVirtualKeyEnd}};
			navElements.push_back(navElement);

			Menu::NavElement navElement2 = {steppers[i - 1]->GetRight(), {steppers[i - 1]->GetLeft(), steppers[i - 1]->GetLeft(), steppers[i - 2]->GetRight(), steppers[i]->GetRight()}, {cVirtualKeyEnd, cVirtualKeyEnd}};
			navElements.push_back(navElement2);
		}	

		if (i + 1 == stepperCount)
		{
			if (stepperCount >= 2)
			{
				Menu::NavElement navElement = {steppers[0]->GetLeft(), {steppers[0]->GetRight(), steppers[0]->GetRight(), upWidgetLeft, steppers[1]->GetLeft()}, {cVirtualKeyEnd, cVirtualKeyEnd}};
				navElements.push_back(navElement);

				Menu::NavElement navElement2 = {steppers[0]->GetRight(), {steppers[0]->GetLeft(), steppers[0]->GetLeft(), upWidgetRight, steppers[1]->GetRight()}, {cVirtualKeyEnd, cVirtualKeyEnd}};
				navElements.push_back(navElement2);
			}

			{
				Menu::NavElement navElement = {steppers[stepperCount - 1]->GetLeft(), {steppers[stepperCount - 1]->GetRight(), steppers[stepperCount - 1]->GetRight(), stepperCount >= 2 ? steppers[stepperCount - 2]->GetLeft() : upWidgetLeft, downWidgetLeft}, {cVirtualKeyEnd, cVirtualKeyEnd}};
				navElements.push_back(navElement);

				Menu::NavElement navElement2 = {steppers[stepperCount - 1]->GetRight(), {steppers[stepperCount - 1]->GetLeft(), steppers[stepperCount - 1]->GetLeft(), stepperCount >= 2 ? steppers[stepperCount - 2]->GetRight() : upWidgetRight, downWidgetRight}, {cVirtualKeyEnd, cVirtualKeyEnd}};
				navElements.push_back(navElement2);
			}
		}
	}
}

void OptionsMenu::RegNavElements(gui::StepperBox* steppers[], int stepperCount, gui::VolumeBar* volumes[], int volumeCount, ButtonLR buttons[], int buttonsCount, gui::Widget* upArrow, gui::Widget* downArrow)
{
	lsl::Vector<Menu::NavElement> navElements;

	gui::Widget* firstStepperLeft = NULL;
	gui::Widget* firstStepperRight = NULL;
	gui::Widget* lastStepperLeft = NULL;
	gui::Widget* lastStepperRight = NULL;

	if (buttons)
	{
		RegSteppers(navElements, buttons, buttonsCount, upArrow, upArrow, downArrow, downArrow);

		{
			Menu::NavElement navElement = {upArrow, {NULL, NULL, _menuItems[miBack], buttons[0].GetLeft()}, {cVirtualKeyEnd, cVirtualKeyEnd}};
			navElements.push_back(navElement);
		}
		{
			Menu::NavElement navElement = {downArrow, {NULL, NULL, buttons[buttonsCount - 1].GetLeft(), _menuItems[miBack]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
			navElements.push_back(navElement);
		}

		firstStepperLeft = upArrow;
		firstStepperRight = upArrow;
		lastStepperLeft = downArrow;
		lastStepperRight = downArrow;
	}
	else
	{
		gui::Widget* upElement = upArrow ? upArrow : _menuItems[miBack];
		gui::Widget* downElement = downArrow ? downArrow : _menuItems[miBack];

		if (steppers)
			RegSteppers(navElements, steppers, stepperCount, upElement, upElement, volumes ? volumes[0]->GetLeft() : downElement, volumes ? volumes[0]->GetRight() : downElement);
		if (volumes)
			RegSteppers(navElements, volumes, volumeCount, steppers ? steppers[stepperCount - 1]->GetLeft() : upElement, steppers ? steppers[stepperCount - 1]->GetRight() : upElement, downElement, downElement);

		firstStepperLeft = steppers ? steppers[0]->GetLeft() : NULL;
		firstStepperRight = steppers ? steppers[0]->GetRight() : NULL;
		lastStepperLeft = volumes ? volumes[volumeCount - 1]->GetLeft() : (steppers ? steppers[stepperCount - 1]->GetLeft() : NULL);
		lastStepperRight = volumes ? volumes[volumeCount - 1]->GetRight() : (steppers ? steppers[stepperCount - 1]->GetRight() : NULL);

		if (upArrow)
		{
			Menu::NavElement navElement = {upArrow, {NULL, NULL, _menuItems[miBack], firstStepperLeft}, {cVirtualKeyEnd, cVirtualKeyEnd}};
			navElements.push_back(navElement);
		}
		if (downArrow)
		{
			Menu::NavElement navElement = {downArrow, {NULL, NULL, lastStepperLeft, _menuItems[miBack]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
			navElements.push_back(navElement);
		}

		firstStepperLeft = upArrow ? upArrow : firstStepperLeft;
		firstStepperRight = upArrow ? upArrow : firstStepperLeft;
		lastStepperLeft = downArrow ? downArrow : lastStepperLeft;
		lastStepperRight = downArrow ? downArrow : lastStepperRight;
	}

	for (int i = 0; i < cStateEnd; ++i)
	{
		if (i >= 2)
		{
			Menu::NavElement navElement = {_stateItems[i - 1], {_menuItems[miApply], _menuItems[miBack], _stateItems[i - 2], _stateItems[i]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
			navElements.push_back(navElement);
		}	

		if (i + 1 == cStateEnd)
		{
			if (cStateEnd >= 2)
			{
				Menu::NavElement navElement = {_stateItems[0], {_menuItems[miApply], _menuItems[miBack], _stateItems[cStateEnd - 1], _stateItems[1]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
				navElements.push_back(navElement);
			}

			Menu::NavElement navElement2 = {_stateItems[cStateEnd - 1], {_menuItems[miApply], _menuItems[miBack], cStateEnd >= 2 ? _stateItems[cStateEnd - 2] : NULL, _stateItems[0]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
			navElements.push_back(navElement2);
		}
	}

	{
		Menu::NavElement navElement = {_menuItems[miBack], {_stateItems[0], _menuItems[miApply], lastStepperLeft, firstStepperLeft}, {vkBack, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}
	{
		Menu::NavElement navElement = {_menuItems[miApply], {_menuItems[miBack], _stateItems[0], lastStepperLeft, firstStepperLeft}, {cVirtualKeyEnd, cVirtualKeyEnd}};
		navElements.push_back(navElement);		
	}

	gui::Widget* focus = NULL;
	for (unsigned i = 0; i < navElements.size(); ++i)
		if (navElements[i].widget->IsFocused())
		{
			focus = navElements[i].widget;
			break;
		}
	
	_menu->SetNavElements(_menuItems[miBack], true, &navElements[0], navElements.size());
	if (focus)
		focus->SetFocused(true, true);
}

gui::Button* OptionsMenu::CreateMenuButton(StringValue name, gui::Widget* parent, gui::Widget::Event* guiEvent)
{
	return _menu->CreateMenuButton2(name, parent, guiEvent);
}

gui::Button* OptionsMenu::CreateMenuButton2(StringValue name, gui::Widget* parent, gui::Widget::Event* guiEvent)
{	
	gui::Button* button = _menu->CreateMenuButton(svNull, "", "GUI\\buttonBg4.png", "GUI\\buttonBgSel4.png", parent, guiEvent, IdentityVec2, gui::Button::bsSelAnim, clrWhite,  Menu::ssButton1);
	button->SetAlign(gui::Widget::waLeft);

	gui::Label* label = _menu->CreateLabel(name, button, "Header", NullVec2, gui::Text::haLeft, gui::Text::vaCenter, 0xffafafaf);
	label->SetAlign(gui::Widget::waLeft);
	label->SetPos(button->GetSize().x + 10.0f, 0.0f);
	label->AdjustSizeByText();

	gui::Dummy* dummy = _menu->CreateDummy(button, this, Menu::ssButton1);
	dummy->SetAlign(gui::Widget::waLeft);	
	dummy->SetSize(button->GetSize().x + label->GetSize().x + 10.0f, button->GetSize().y);
	dummy->SetData(button);

	return button;
}

gui::PlaneFon* OptionsMenu::CreateItemBg(gui::Widget* parent, gui::Widget::Event* guiEvent)
{
	return _menu->CreatePlane(parent, guiEvent, "GUI\\labelBg1.png", true, IdentityVec2, gui::Material::bmTransparency);
}

gui::StepperBox* OptionsMenu::CreateStepper(const StringList& items, gui::Widget* parent, gui::Widget::Event* guiEvent)
{
	return _menu->CreateStepper(items, parent, guiEvent);
}

gui::VolumeBar* OptionsMenu::CreateVolumeBar(gui::Widget* parent, gui::Widget::Event* guiEvent)
{
	gui::VolumeBar* volumeBar = _menu->GetGUI()->CreateVolumeBar();
	volumeBar->SetParent(parent);

	volumeBar->RegEvent(_menu->GetSoundSheme(Menu::ssStepper));
	if (guiEvent)
		volumeBar->RegEvent(guiEvent);

	volumeBar->GetOrCreateArrow()->GetSampler().SetTex(_menu->GetTexture("GUI\\arrow3.png"));
	volumeBar->GetOrCreateArrow()->SetBlending(gui::Material::bmTransparency);

	volumeBar->GetOrCreateArrowSel()->GetSampler().SetTex(_menu->GetTexture("GUI\\arrowSel3.png"));
	volumeBar->GetOrCreateArrowSel()->SetBlending(gui::Material::bmTransparency);

	volumeBar->GetBack().GetSampler().SetTex(_menu->GetTexture("GUI\\optBarBg.png"));
	volumeBar->GetBack().SetBlending(gui::Material::bmTransparency);

	volumeBar->GetFront().GetSampler().SetTex(_menu->GetTexture("GUI\\optBar.png"));	
	volumeBar->GetFront().SetBlending(gui::Material::bmTransparency);

	volumeBar->SetSize(240.0f, 45.0f);

	volumeBar->Invalidate();

	return volumeBar;
}

void OptionsMenu::AdjustLayout(const D3DXVECTOR2& vpSize)
{
	_menuBgMask->SetSize(vpSize);

	_labels[mlHeader]->SetPos(0.0f, -317.0f);

	for (int i = 0; i < cMenuItemEnd; ++i)
		_menuItems[i]->SetPos(-30.0f + i * 360.0f, 240.0f);

	for (int i = 0; i < cStateEnd; ++i)
		_stateItems[i]->SetPos(-558.0f, -125.0f + i * 100.0f);

	_mediaFrame->AdjustLayout(vpSize);
	_gameFrame->AdjustLayout(vpSize);
	_networkFrame->AdjustLayout(vpSize);
	_controlsFrame->AdjustLayout(vpSize);
}

void OptionsMenu::Show(bool value)
{
	GetRoot()->SetVisible(value);

	if (!value)
		_menu->UnregNavElements(_menuItems[miBack]);
}

void OptionsMenu::OnProgress(float deltaTime)
{	
}

gui::Widget* OptionsMenu::GetRoot()
{
	return _root;
}

OptionsMenu::State OptionsMenu::GetState() const
{
	return _state;
}

void OptionsMenu::SetState(State value)
{
	if (_state != value)
	{
		_state = value;
		ApplyState();
	}
}




StartOptionsMenu::StartOptionsMenu(Menu* menu, gui::Widget* parent): MenuFrame(menu, parent), _lastCameraIndex(-1)
{
	D3DXCOLOR color1(0xffafafaf);
	StringValue strMenuItems[cMenuItemEnd] = {svApply};

	const StringValue cPrefCameraStr[GameMode::cPrefCameraEnd] = {svCameraSecView, svCameraOrtho};

	StringList cameraStr;
	for (int i = 0; i < GameMode::cPrefCameraEnd; ++i)
		cameraStr.push_back(GetString(cPrefCameraStr[i]));
	cameraStr.push_back(GetString(svSelectItem));

	StringList languageStr;
	for (Languages::const_iterator iter = menu->GetGame()->GetLanguages().begin(); iter != menu->GetGame()->GetLanguages().end(); ++iter)
		languageStr.push_back(iter->name);

	StringList commentatorStr;
	for (CommentatorStyles::const_iterator iter = menu->GetGame()->GetCommentatorStyles().begin(); iter != menu->GetGame()->GetCommentatorStyles().end(); ++iter)
		commentatorStr.push_back(iter->name);

	StringList onOffLevel;
	onOffLevel.push_back(GetString(svOff));
	onOffLevel.push_back(GetString(svOn));

	StringList resolutionStr;
	graph::DisplayModes modes = menu->GetDisplayModes();
	for (unsigned i = 0; i < modes.size(); ++i)
		resolutionStr.push_back(lsl::StrFmt("%d x %d", modes[i].width, modes[i].height));

	StringList itemsStepper[cStepperEnd] = {cameraStr, resolutionStr, languageStr, commentatorStr};

	StringValue strLabels[cLabelEnd] = {svCamera, svResolution, svLanguage, svCommentator, svStartOptionsInfo};
	std::string fontLabels[cLabelEnd] = {"Small", "Small", "Small", "Small", "Small"};
	gui::Text::HorAlign horLabels[cLabelEnd] = {gui::Text::haLeft, gui::Text::haLeft, gui::Text::haLeft, gui::Text::haLeft, gui::Text::haCenter};
	gui::Text::VertAlign vertLabels[cLabelEnd] = {gui::Text::vaCenter, gui::Text::vaCenter, gui::Text::vaCenter, gui::Text::vaCenter, gui::Text::vaCenter};
	D3DXCOLOR colorLabels[cLabelEnd] = {color1, color1, color1, color1, color1};

	_menuBgMask = menu->CreatePlane(root(), this, "", false, IdentityVec2, gui::Material::bmTransparency);
	_menuBgMask->GetMaterial().SetColor(D3DXCOLOR(0, 0, 0, 0.8f));
	_menuBgMask->SetAnchor(gui::Widget::waCenter);

	_menuBg = menu->CreatePlane(_menuBgMask, this, "GUI\\startMenuBg.png", true, IdentityVec2, gui::Material::bmTransparency);

	gui::Widget* labelsParent[cLabelEnd] = {_menuBg, _menuBg, _menuBg, _menuBg, _menuBg};

	for (int i = 0; i < cStepperEnd; ++i)
	{
		_itemsBg[i] = menu->CreatePlane(_menuBg, NULL, "GUI\\labelBg1.png", true, IdentityVec2, gui::Material::bmTransparency);
		_itemsBg[i]->SetAlign(gui::Widget::waLeft);
	}

	for (int i = 0; i < cLabelEnd; ++i)	
		_labels[i] = menu->CreateLabel(strLabels[i], labelsParent[i], fontLabels[i], NullVec2, horLabels[i], vertLabels[i], colorLabels[i]);
	_labels[mlInfo]->SetWordWrap(true);

	for (int i = 0; i < cStepperEnd; ++i)
	{
		_steppers[i] = menu->CreateStepper(itemsStepper[i], _menuBg, this);
		_steppers[i]->SetAlign(gui::Widget::waLeft);
	}

	for (int i = 0; i < cMenuItemEnd; ++i)
		_menuItems[i] = menu->CreateMenuButton2(strMenuItems[i], _menuBg, this);
}

StartOptionsMenu::~StartOptionsMenu()
{
	menu()->UnregNavElements(_menuItems[miApply]);

	menu()->ReleaseWidget(_menuBg);
	menu()->ReleaseWidget(_menuBgMask);
}

void StartOptionsMenu::LoadCfg()
{	
	_steppers[dbCamera]->SetSelIndex(_lastCameraIndex = GameMode::cPrefCameraEnd);
	_steppers[dbResolution]->SetSelIndex(menu()->GetDisplayModeIndex());
	_steppers[dbLanguage]->SetSelIndex(menu()->GetGame()->GetLanguageIndex());
	_steppers[dbCommentator]->SetSelIndex(menu()->GetGame()->FindCommentatorStyleIndex(menu()->GetGame()->GetCommentatorStyle()));

	menu()->SetButtonEnabled(_menuItems[miApply], false);
}

void StartOptionsMenu::OnShow(bool value)
{
	if (value)
	{
		LoadCfg();

		lsl::Vector<Menu::NavElement> navElements;

		for (int i = 0; i < cStepperEnd; ++i)
		{
			if (i >= 2)
			{
				Menu::NavElement navElement = {_steppers[i - 1]->GetLeft(), {NULL, _steppers[i - 1]->GetRight(), _steppers[i - 2]->GetLeft(), _steppers[i]->GetLeft()}, {cVirtualKeyEnd, cVirtualKeyEnd}};
				navElements.push_back(navElement);

				Menu::NavElement navElement2 = {_steppers[i - 1]->GetRight(), {_steppers[i - 1]->GetLeft(), NULL, _steppers[i - 2]->GetRight(), _steppers[i]->GetRight()}, {cVirtualKeyEnd, cVirtualKeyEnd}};
				navElements.push_back(navElement2);
			}	

			if (i + 1 == cStepperEnd)
			{
				if (cStepperEnd >= 2)
				{
					Menu::NavElement navElement = {_steppers[0]->GetLeft(), {_steppers[0]->GetRight(), _steppers[0]->GetRight(), _menuItems[miApply], _steppers[1]->GetLeft()}, {cVirtualKeyEnd, cVirtualKeyEnd}};
					navElements.push_back(navElement);

					Menu::NavElement navElement2 = {_steppers[0]->GetRight(), {_steppers[0]->GetLeft(), _steppers[0]->GetLeft(), _menuItems[miApply], _steppers[1]->GetRight()}, {cVirtualKeyEnd, cVirtualKeyEnd}};
					navElements.push_back(navElement2);
				}

				{
					Menu::NavElement navElement = {_steppers[cStepperEnd - 1]->GetLeft(), {_steppers[cStepperEnd - 1]->GetRight(), _steppers[cStepperEnd - 1]->GetRight(), cStepperEnd >= 2 ? _steppers[cStepperEnd - 2]->GetLeft() : _menuItems[miApply], _menuItems[miApply]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
					navElements.push_back(navElement);

					Menu::NavElement navElement2 = {_steppers[cStepperEnd - 1]->GetRight(), {_steppers[cStepperEnd - 1]->GetLeft(), _steppers[cStepperEnd - 1]->GetLeft(), cStepperEnd >= 2 ? _steppers[cStepperEnd - 2]->GetRight() : _menuItems[miApply], _menuItems[miApply]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
					navElements.push_back(navElement2);
				}
			}
		}

		{
			Menu::NavElement navElement = {_menuItems[miApply], {NULL, NULL, _steppers[cStepperEnd - 1]->GetLeft(), _steppers[0]->GetLeft()}, {cVirtualKeyEnd, cVirtualKeyEnd}};
			navElements.push_back(navElement);		
		}

		menu()->SetNavElements(_menuItems[miApply], true, &navElements[0], navElements.size());
	}
	else
		menu()->UnregNavElements(_menuItems[miApply]);
}

void StartOptionsMenu::OnAdjustLayout(const D3DXVECTOR2& vpSize)
{
	D3DXVECTOR2 offs = D3DXVECTOR2(-160.0f, 45.0f);

	_menuBgMask->SetSize(vpSize);

	int index = 0;	
	for (int i = 0; i < cStepperEnd; ++i)
	{
		if (_labels[i]->GetVisible())
			_labels[i]->SetPos(-200.0f + offs.x, -190.0f + (index++) * 50.0f + offs.y);
	}

	_labels[mlInfo]->SetPos(0.0f, 70.0f);
	_labels[mlInfo]->SetSize(750.0f, 50.0f);

	index = 0;
	for (int i = 0; i < cStepperEnd; ++i)
	{
		if (_steppers[i]->GetVisible())
			_steppers[i]->SetPos(260.0f + offs.x, -190.0f + (index++) * 50.0f + offs.y);
	}

	index = 0;
	for (int i = 0; i < cStepperEnd; ++i)
	{
		if (_itemsBg[i]->GetVisible())
			_itemsBg[i]->SetPos(-235.0f + offs.x, -190.0f + (index++) * 50.0f + offs.y);
	}

	for (int i = 0; i < cMenuItemEnd; ++i)
		_menuItems[i]->SetPos(-30.0f + (i + 0.5f) * 360.0f + offs.x, 93.0f + offs.y);
}

void StartOptionsMenu::OnInvalidate()
{	
}

bool StartOptionsMenu::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
{
	if (sender == menu()->GetMessageSender())
	{
		GameMode* game = menu()->GetGame();

		menu()->ShowStartOptions(false);
		game->CheckStartupMenu();
		return true;
	}

	if (sender == _menuItems[miApply])
	{
		Environment::SyncFrameRate prevSyncFrameRate = menu()->GetEnv()->GetSyncFrameRate();
		bool needReload = ApplyChanges();
		menu()->SaveGameOpt();

		if (prevSyncFrameRate == Environment::sfrFixed && menu()->GetEnv()->GetSyncFrameRate() != Environment::sfrFixed)
		{
			std::string message;
			if (needReload)
				message = menu()->GetString("svSyncFrameWarningNeedReload");
			else
				message = menu()->GetString("svSyncFrameWarning");
			menu()->ShowMessage(menu()->GetString(svWarning), message, menu()->GetString(svOk), menu()->GetGUI()->GetVPSize()/2, gui::Widget::waCenter, 0.0f, this);
		}
		else if (needReload)
		{
			menu()->ShowMessage(menu()->GetString(svWarning), GetString(svHintNeedReload), GetString(svOk), menu()->GetGUI()->GetVPSize()/2, gui::Widget::waCenter, 0.0f, this);
		}
		else
		{
			GameMode* game = menu()->GetGame();

			menu()->ShowStartOptions(false);
			game->CheckStartupMenu();
		}
		
		return true;
	}

	return false;
}

bool StartOptionsMenu::OnSelect(gui::Widget* sender, Object* item)
{
	if (sender == _steppers[dbCamera])
	{
		if (_lastCameraIndex == GameMode::cPrefCameraEnd)
		{
			menu()->SetButtonEnabled(_menuItems[miApply], true);
			_steppers[dbCamera]->SetSelIndex(GameMode::pcIsometric);
		}
		else if (_steppers[dbCamera]->GetSelIndex() == GameMode::cPrefCameraEnd)
		{
			_steppers[dbCamera]->SetSelIndex(_lastCameraIndex == 0 ? 1 : 0);
		}

		_lastCameraIndex = _steppers[dbCamera]->GetSelIndex();
		return true;
	}

	return false;
}

bool StartOptionsMenu::ApplyChanges()
{
	bool res = false;

	for (int i = 0; i < cStepperEnd; ++i)
	{
		int val = _steppers[i]->GetSelIndex();

		switch (i)
		{
		case dbCamera:
			menu()->GetGame()->SetPrefCamera(GameMode::PrefCamera(val));
			break;

		case dbResolution:
			if (val >= 0 && static_cast<unsigned>(val) < menu()->GetDisplayModes().size())
			{
				graph::DisplayMode mode = menu()->GetDisplayModes()[val];
				menu()->SetDisplayMode(lsl::Point(mode.width, mode.height));
			}
			break;

		case dbLanguage:
		{
			int curVal = menu()->GetGame()->GetLanguageIndex();
			if (val != -1 && val != curVal)
			{
				menu()->GetGame()->SetLanguage(menu()->GetGame()->GetLanguages()[val].name);
				res |= true;
			}
			break;
		}

		case dbCommentator:
		{
			if (val != -1)
				menu()->GetGame()->SetCommentatorStyle(menu()->GetGame()->GetCommentatorStyles()[val].name);
			break;
		}
		}
	}	

	return res;
}

void StartOptionsMenu::CancelChanges()
{	
}

}

}

}