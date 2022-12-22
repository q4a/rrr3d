#pragma once

#include "MenuSystem.h"

namespace r3d
{
	namespace game
	{
		namespace n
		{
			class OptionsMenu;

			class GameFrame : public MenuFrame
			{
			private:
				enum Label
				{
					mlLapsCount,
					mlSelectedLevel,
					mlMinimapStyle,
					mlStyleHUD,
					mlMaxComputers,
					mlUpgradeMaxLevel,
					mlWeaponUpgrades,
					mlSurvivalMode,
					mlEnableMineBug,
					mlOilDestroyer,
					cLabelEnd
				}; //mlMaxPlayers 
				enum Stepper
				{
					dbLapsCount,
					dbSelectedLevel,
					dbMinimapStyle,
					dbStyleHUD,
					dbMaxComputers,
					dbUpgradeMaxLevel,
					dbWeaponUpgrades,
					dbSurvivalMode,
					dbEnableMineBug,
					dbOilDestroyer,
					cStepperEnd
				}; //dbMaxPlayers
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
				void OnShow(bool value) override;
				void OnAdjustLayout(const D3DXVECTOR2& vpSize) override;
				void OnInvalidate() override;

				bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick) override;
			public:
				GameFrame(Menu* menu, OptionsMenu* optionsMenu, gui::Widget* parent);
				~GameFrame() override;

				bool ApplyChanges();
				void CancelChanges();
			};

			class CameraFrame : public MenuFrame
			{
			private:
				enum Label
				{
					mlCamera,
					mlSplitType,
					mlCameraDist,
					mlCamLock,
					mlStaticCam,
					mlCamFov,
					mlCamProection,
					mlAutoCamera,
					mlSubjectView,
					cLabelEnd
				};

				enum Stepper
				{
					dbCamera,
					dbSplitType,
					dbCameraDist,
					dbCamLock,
					dbStaticCam,
					dbCamFov,
					dbCamProection,
					dbAutoCamera,
					dbSubjectView,
					cStepperEnd
				};

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
				void OnShow(bool value) override;
				void OnAdjustLayout(const D3DXVECTOR2& vpSize) override;
				void OnInvalidate() override;

				bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick) override;
			public:
				CameraFrame(Menu* menu, OptionsMenu* optionsMenu, gui::Widget* parent);
				~CameraFrame() override;

				bool ApplyChanges();
				void CancelChanges();
			};

			class MediaFrame : public MenuFrame
			{
			private:
				enum Label
				{
					mlResolution = 0,
					mlFiltering,
					mlMultisampling,
					mlShadow,
					mlEnv,
					mlLight,
					mlPostProcess,
					mlDisableVideo,
					cLabelEnd
				}; //mlWindowMode
				enum Stepper
				{
					dbResolution = 0,
					dbFiltering,
					dbMultisampling,
					dbShadow,
					dbEnv,
					dbLight,
					dbPostProcess,
					dbDisableVideo,
					cStepperEnd
				}; //dbWindowMode
			private:
				OptionsMenu* _optionsMenu;

				gui::Label* _labels[cLabelEnd];
				gui::StepperBox* _steppers[cStepperEnd];
				gui::PlaneFon* _itemsBg[cLabelEnd];

				void LoadCfg();
			protected:
				void OnShow(bool value) override;
				void OnAdjustLayout(const D3DXVECTOR2& vpSize) override;
				void OnInvalidate() override;
			public:
				MediaFrame(Menu* menu, OptionsMenu* optionsMenu, gui::Widget* parent);
				~MediaFrame() override;

				bool ApplyChanges();
				void CancelChanges();
			};

			class NetworkTab : public MenuFrame
			{
			private:
				enum Label { mlLanguage, mlCommentator, mlMusic, msSound, msVoice, cLabelEnd };

				enum Stepper { dbLanguage, dbCommentator, cStepperEnd };

				enum VolumeBar { tbMusic = 0, tbSound, tbDicter, cVolumeBarEnd };

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
				void OnShow(bool value) override;
				void OnAdjustLayout(const D3DXVECTOR2& vpSize) override;
				void OnInvalidate() override;

				bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick) override;
			public:
				NetworkTab(Menu* menu, OptionsMenu* optionsMenu, gui::Widget* parent);
				~NetworkTab() override;

				bool ApplyChanges();
				void CancelChanges();
			};

			class ControlsFrame : public MenuFrame, ControlEvent
			{
			private:
				struct InputBox
				{
					gui::PlaneFon* sprite;
					gui::Label* label;
					gui::Button* keys[cControllerTypeEnd];

					VirtualKey virtualKeys[cControllerTypeEnd];
				};

				using InputBoxes = Vector<InputBox>;
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
				void OnShow(bool value) override;
				void OnAdjustLayout(const D3DXVECTOR2& vpSize) override;
				void OnInvalidate() override;

				bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick) override;
				bool OnHandleInput(const InputMessage& msg) override;
			public:
				ControlsFrame(Menu* menu, OptionsMenu* optionsMenu, gui::Widget* parent);
				~ControlsFrame() override;

				bool ApplyChanges();
				void CancelChanges();
			};

			class OptionsMenu : public gui::Widget::Event
			{
			public:
				enum State { msGame = 0, msCamera, msMedia, msNetwork, msControls, cStateEnd };

				enum Label { mlHeader, cLabelEnd };

				enum MenuItem { miBack = 0, miApply, cMenuItemEnd };

				struct ButtonLR
				{
					gui::Widget* left;
					gui::Widget* right;

					gui::Widget* GetLeft() { return left; }
					gui::Widget* GetRight() { return right; }

					ButtonLR* operator->() { return this; }
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

				GameFrame* _gameFrame;
				CameraFrame* _cameraFrame;
				MediaFrame* _mediaFrame;
				NetworkTab* _networkFrame;
				ControlsFrame* _controlsFrame;

				void SetSelection(State state, bool instant);
				void ApplyState();

				void CancelChanges();
				//true - if need reload
				bool ApplyChanges();
			protected:
				bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick) override;
				bool OnMouseEnter(gui::Widget* sender, const gui::MouseMove& mMove) override;
				void OnMouseLeave(gui::Widget* sender, bool wasReset) override;
				void OnFocusChanged(gui::Widget* sender) override;
			public:
				OptionsMenu(Menu* menu, gui::Widget* parent);
				virtual ~OptionsMenu();

				template <class _T1, class _T2>
				void RegSteppers(Vector<_T2>& navElements, _T1 steppers[], int stepperCount, gui::Widget* upWidgetLeft,
				                 gui::Widget* upWidgetRight, gui::Widget* downWidgetLeft, gui::Widget* downWidgetRight);
				void RegNavElements(gui::StepperBox* steppers[], int stepperCount, gui::VolumeBar* volumes[],
				                    int volumeCount, ButtonLR buttons[] = nullptr, int buttonsCount = 0,
				                    gui::Widget* upArrow = nullptr, gui::Widget* downArrow = nullptr);

				gui::Button* CreateMenuButton(StringValue name, gui::Widget* parent, gui::Widget::Event* guiEvent);
				gui::Button* CreateMenuButton2(StringValue name, gui::Widget* parent, gui::Widget::Event* guiEvent);
				gui::PlaneFon* CreateItemBg(gui::Widget* parent, gui::Widget::Event* guiEvent);
				gui::StepperBox* CreateStepper(const StringList& items, gui::Widget* parent,
				                               gui::Widget::Event* guiEvent);
				gui::VolumeBar* CreateVolumeBar(gui::Widget* parent, gui::Widget::Event* guiEvent);

				void AdjustLayout(const D3DXVECTOR2& vpSize);
				void Show(bool value);

				void OnProgress(float deltaTime);

				gui::Widget* GetRoot();

				State GetState() const;
				void SetState(State value);
			};

			class StartOptionsMenu : public MenuFrame
			{
			private:
				enum Label { mlCamera, mlResolution, mlLanguage, mlCommentator, mlInfo, cLabelEnd };

				enum Stepper { dbCamera, dbResolution, dbLanguage, dbCommentator, cStepperEnd };

				enum MenuItem { miApply, cMenuItemEnd };

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
				void OnShow(bool value) override;
				void OnAdjustLayout(const D3DXVECTOR2& vpSize) override;
				void OnInvalidate() override;

				bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick) override;
				bool OnSelect(gui::Widget* sender, Object* item) override;
			public:
				StartOptionsMenu(Menu* menu, gui::Widget* parent);
				~StartOptionsMenu() override;

				bool ApplyChanges();
				void CancelChanges();
			};
		}
	}
}
