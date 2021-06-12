#pragma once

#include "MenuSystem.h"

namespace r3d
{

namespace game
{

namespace n
{

class OptionsMenu;

class GameFrame: public MenuFrame
{
private:
	enum Label {mlCamera, mlCameraDist, mlEnableHUD, mlDiff, mlSpringBorders, mlUpgradeMaxLevel, mlWeaponMaxLevel, mlMaxPlayers, mlMaxComputers, mlLapsCount, mlEnableMineBug, mlDisableVideo, cLabelEnd};
	enum Stepper {dbCamera, dbCameraDist, dbEnableHUD, dbDiff, dbSpringBorders, dbUpgradeMaxLevel, dbWeaponMaxLevel, dbMaxPlayers, dbMaxComputers, dbLapsCount, dbEnableMineBug, dbDisableVideo, cStepperEnd};
private:
	OptionsMenu* _optionsMenu;
	int _gridScroll;

	gui::Grid* _grid;
	gui::Button* _downArrow;
	gui::Button* _upArrow;

	gui::Label* _labels[cLabelEnd];
	gui::StepperBox* _steppers[cStepperEnd];	
	gui::PlaneFon* _itemsBg[cLabelEnd];

	void AdjustGrid(const D3DXVECTOR2& vpSize);
	void ScrollGrid(int step);

	void LoadCfg();	
protected:
	virtual void OnShow(bool value);
	virtual void OnAdjustLayout(const D3DXVECTOR2& vpSize);
	virtual void OnInvalidate();

	virtual bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick);
public:
	GameFrame(Menu* menu, OptionsMenu* optionsMenu, gui::Widget* parent);
	virtual ~GameFrame();

	bool ApplyChanges();
	void CancelChanges();
};

class MediaFrame: public MenuFrame
{
private:	
	enum Label {mlResolution = 0, mlFiltering, mlMultisampling, mlShadow, mlEnv, mlLight, mlPostProcess, mlWindowMode, cLabelEnd};
	enum Stepper {dbResolution = 0, dbFiltering, dbMultisampling, dbShadow, dbEnv, dbLight, dbPostProcess, dbWindowMode, cStepperEnd};	
private:
	OptionsMenu* _optionsMenu;	

	gui::Label* _labels[cLabelEnd];
	gui::StepperBox* _steppers[cStepperEnd];	
	gui::PlaneFon* _itemsBg[cLabelEnd];

	void LoadCfg();
protected:	
	virtual void OnShow(bool value);
	virtual void OnAdjustLayout(const D3DXVECTOR2& vpSize);
	virtual void OnInvalidate();
public:
	MediaFrame(Menu* menu, OptionsMenu* optionsMenu, gui::Widget* parent);
	virtual ~MediaFrame();

	bool ApplyChanges();
	void CancelChanges();
};

class NetworkTab: public MenuFrame
{
private:
	enum Label {mlLanguage, mlCommentator, mlMusic, msSound, msVoice, cLabelEnd};
	enum Stepper {dbLanguage, dbCommentator, cStepperEnd};
	enum VolumeBar {tbMusic = 0, tbSound, tbDicter, cVolumeBarEnd};
private:
	OptionsMenu* _optionsMenu;
	float _defVolumes[cVolumeBarEnd];

	gui::Label* _labels[cLabelEnd];
	gui::StepperBox* _steppers[cStepperEnd];
	gui::VolumeBar* _volumeBars[cVolumeBarEnd];
	gui::PlaneFon* _itemsBg[cLabelEnd];

	void LoadCfg();	
	void ApplyVolume(bool revertChanges);
protected:
	virtual void OnShow(bool value);
	virtual void OnAdjustLayout(const D3DXVECTOR2& vpSize);
	virtual void OnInvalidate();

	virtual bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick);	
public:
	NetworkTab(Menu* menu, OptionsMenu* optionsMenu, gui::Widget* parent);
	virtual ~NetworkTab();

	bool ApplyChanges();
	void CancelChanges();
};

class ControlsFrame: public MenuFrame, ControlEvent
{
private:
	struct InputBox
	{
		gui::PlaneFon* sprite;
		gui::Label* label;
		gui::Button* keys[cControllerTypeEnd];

		VirtualKey virtualKeys[cControllerTypeEnd];
	};

	typedef lsl::Vector<InputBox> InputBoxes;
private:
	OptionsMenu* _optionsMenu;
	int _gridScroll;

	gui::Grid* _grid;
	gui::Button* _downArrow;
	gui::Button* _upArrow;
	InputBoxes _boxes;

	gui::PlaneFon* _controllerIcons[cControllerTypeEnd];

	void AdjustGrid(const D3DXVECTOR2& vpSize);
	void ScrollGrid(int step);

	void LoadCfg();	
protected:
	virtual void OnShow(bool value);
	virtual void OnAdjustLayout(const D3DXVECTOR2& vpSize);
	virtual void OnInvalidate();

	virtual bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick);
	virtual bool OnHandleInput(const InputMessage& msg);
public:
	ControlsFrame(Menu* menu, OptionsMenu* optionsMenu, gui::Widget* parent);
	virtual ~ControlsFrame();

	bool ApplyChanges();
	void CancelChanges();
};

class OptionsMenu: public gui::Widget::Event
{
public:
	enum State {msGame = 0, msMedia, msNetwork, msControls, cStateEnd};
	enum Label {mlHeader, cLabelEnd};
	enum MenuItem {miBack = 0, miApply, cMenuItemEnd};

	struct ButtonLR
	{
		gui::Widget* left;
		gui::Widget* right;

		gui::Widget* GetLeft() {return left;}
		gui::Widget* GetRight() {return right;}

		ButtonLR* operator->() {return this;}
	};
private:
	Menu* _menu;	
	State _state;

	gui::Dummy* _root;
	gui::Label* _labels[cLabelEnd];
	gui::Button* _menuItems[cMenuItemEnd];
	gui::Button* _stateItems[cStateEnd];
	gui::PlaneFon* _menuBgMask;
	gui::PlaneFon* _menuBg;	

	MediaFrame* _mediaFrame;
	GameFrame* _gameFrame;
	NetworkTab* _networkFrame;
	ControlsFrame* _controlsFrame;

	void SetSelection(State state, bool instant);
	void ApplyState();

	void CancelChanges();
	//true - if need reload
	bool ApplyChanges();
protected:
	virtual bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick);	
	virtual bool OnMouseEnter(gui::Widget* sender, const gui::MouseMove& mMove);
	virtual void OnMouseLeave(gui::Widget* sender, bool wasReset);
	virtual void OnFocusChanged(gui::Widget* sender);
public:
	OptionsMenu(Menu* menu, gui::Widget* parent);
	virtual ~OptionsMenu();

	template<class _T1, class _T2> void RegSteppers(lsl::Vector<_T2>& navElements, _T1 steppers[], int stepperCount, gui::Widget* upWidgetLeft, gui::Widget* upWidgetRight, gui::Widget* downWidgetLeft, gui::Widget* downWidgetRight);
	void RegNavElements(gui::StepperBox* steppers[], int stepperCount, gui::VolumeBar* volumes[], int volumeCount, ButtonLR buttons[] = NULL, int buttonsCount = 0, gui::Widget* upArrow = NULL, gui::Widget* downArrow = NULL);

	gui::Button* CreateMenuButton(StringValue name, gui::Widget* parent, gui::Widget::Event* guiEvent);
	gui::Button* CreateMenuButton2(StringValue name, gui::Widget* parent, gui::Widget::Event* guiEvent);
	gui::PlaneFon* CreateItemBg(gui::Widget* parent, gui::Widget::Event* guiEvent);
	gui::StepperBox* CreateStepper(const StringList& items, gui::Widget* parent, gui::Widget::Event* guiEvent);
	gui::VolumeBar* CreateVolumeBar(gui::Widget* parent, gui::Widget::Event* guiEvent);

	void AdjustLayout(const D3DXVECTOR2& vpSize);
	void Show(bool value);

	void OnProgress(float deltaTime);

	gui::Widget* GetRoot();

	State GetState() const;
	void SetState(State value);
};

class StartOptionsMenu: public MenuFrame
{
private:
	enum Label {mlCamera, mlResolution, mlLanguage, mlCommentator, mlInfo, cLabelEnd};
	enum Stepper {dbCamera, dbResolution, dbLanguage, dbCommentator, cStepperEnd};
	enum MenuItem {miApply, cMenuItemEnd};
private:
	gui::PlaneFon* _menuBgMask;
	gui::PlaneFon* _menuBg;

	gui::Label* _labels[cLabelEnd];
	gui::StepperBox* _steppers[cStepperEnd];	
	gui::PlaneFon* _itemsBg[cLabelEnd];
	gui::Button* _menuItems[cMenuItemEnd];

	int _lastCameraIndex;

	void LoadCfg();
protected:
	virtual void OnShow(bool value);
	virtual void OnAdjustLayout(const D3DXVECTOR2& vpSize);
	virtual void OnInvalidate();

	virtual bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick);	
	virtual bool OnSelect(gui::Widget* sender, Object* item);
public:
	StartOptionsMenu(Menu* menu, gui::Widget* parent);
	virtual ~StartOptionsMenu();

	bool ApplyChanges();
	void CancelChanges();
};

}

}

}