#pragma once

#include "MenuSystem.h"

namespace r3d
{
	namespace game
	{
		namespace n
		{
			class AcceptDialog : public MenuFrame
			{
			private:
				enum Label { mlInfo, cLabelEnd };

				enum MenuItem { miNo, miYes, cMenuItemEnd };

			private:
				bool _resultYes;
				Object* _data;
				gui::Widget::Event* _guiEvent;

				gui::PlaneFon* _menuBg1;
				gui::PlaneFon* _menuBg2;
				gui::PlaneFon* _menuBg3;
				gui::PlaneFon* _menuBg4;
				gui::Label* _labels[cLabelEnd]{};
				gui::Button* _menuItems[cMenuItemEnd]{};
				gui::Button* _menuItems2[cMenuItemEnd]{};
				gui::Button* _menuItems3[cMenuItemEnd]{};
				gui::Button* _menuItems4[cMenuItemEnd]{};
			protected:
				void OnShow(bool value) override;
				void OnAdjustLayout(const D3DXVECTOR2& vpSize) override;
				void OnInvalidate() override;

				bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick) override;
			public:
				AcceptDialog(Menu* menu, gui::Widget* parent);
				~AcceptDialog() override;

				void Show(bool fixmenu, const std::string& message, const std::string& yesText,
				          const std::string& noText, const D3DXVECTOR2& pos, gui::Widget::Anchor align,
				          gui::Widget::Event* guiEvent, Object* data = nullptr, bool maxButtonsSize = false,
				          bool maxMode = false, bool disableFocus = false);
				void Hide();

				bool resultYes() const;
				Object* data() const;

				using MenuFrame::root;
			};

			class WeaponDialog : public MenuFrame
			{
			private:
				enum Label { mlInfo, mlMoney, mlDamage, mlName, cLabelEnd };

			private:
				gui::PlaneFon* _menuBg;
				gui::Label* _labels[cLabelEnd]{};
			protected:
				void OnShow(bool value) override;
				void OnAdjustLayout(const D3DXVECTOR2& vpSize) override;
				void OnInvalidate() override;

				bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick) override;
			public:
				WeaponDialog(Menu* menu, gui::Widget* parent);
				~WeaponDialog() override;

				void Show(const std::string& title, const std::string& message, const std::string& moneyText,
				          const std::string& damageText, const D3DXVECTOR2& pos, gui::Widget::Anchor align);
				void Hide();

				using MenuFrame::root;
			};

			class InfoDialog : public MenuFrame
			{
			private:
				enum Label { mlInfo, mlTitle, cLabelEnd };

				enum MenuItem { miOk, cMenuItemEnd };

			private:
				Object* _data;
				gui::Widget::Event* _guiEvent;

				gui::PlaneFon* _menuBg;
				gui::Label* _labels[cLabelEnd]{};
				gui::Button* _menuItems[cMenuItemEnd]{};
			protected:
				void OnShow(bool value) override;
				void OnAdjustLayout(const D3DXVECTOR2& vpSize) override;
				void OnInvalidate() override;

				bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick) override;
			public:
				InfoDialog(Menu* menu, gui::Widget* parent);
				~InfoDialog() override;

				void Show(const std::string& titleText, const std::string& message, const std::string& okText,
				          const D3DXVECTOR2& pos, gui::Widget::Anchor align, gui::Widget::Event* guiEvent,
				          Object* data = nullptr, bool okButton = true);
				void Hide();

				Object* data() const;

				using MenuFrame::root;
			};

			class MusicDialog : public MenuFrame
			{
			private:
				enum Label { mlInfo, mlTitle, cLabelEnd };

			private:
				gui::PlaneFon* _menuBg;
				gui::PlaneFon* _menuBg2;
				gui::PlaneFon* _menuBg3;
				gui::PlaneFon* _menuBg4;
				gui::Label* _labels[cLabelEnd]{};
			protected:
				void OnShow(bool value) override;
				void OnAdjustLayout(const D3DXVECTOR2& vpSize) override;
				void OnInvalidate() override;

				bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick) override;
			public:
				MusicDialog(Menu* menu, gui::Widget* parent);
				~MusicDialog() override;

				void Show(const std::string& title, const std::string& message);
				void Hide();

				const D3DXVECTOR2& size() const;

				using MenuFrame::root;
			};

			class InfoMenu
			{
			public:
				enum State { msLoading = 0, cStateEnd };

			private:
				Menu* _menu;
				State _state;

				gui::Dummy* _root;
				gui::PlaneFon* _loadingFrame;

				void ApplyState(State state) const;
			public:
				InfoMenu(Menu* menu, gui::Widget* parent);
				virtual ~InfoMenu();

				void AdjustLayout(const D3DXVECTOR2& vpSize) const;
				void Show(bool value) const;
				void Start();
				void Stop();
				void Finished();

				void OnFrame(float deltaTime);

				gui::Widget* GetRoot() const;

				State GetState() const;
				void SetState(State value);
			};

			class UserChat : public MenuFrame
			{
				struct Line
				{
					gui::Label* text;
					gui::Label* name;
					float time;
				};

				using Lines = List<Line>;

				static const int cMaxLines;
			private:
				Lines _lines;
				Line _input{};

				D3DXVECTOR2 _linesPos;
				D3DXVECTOR2 _inputPos;
				D3DXVECTOR2 _linesSize;
				D3DXVECTOR2 _inputSize;

				Line AddLine(const stringW& name, const stringW& text, const D3DXCOLOR& nameColor, bool right) const;
			protected:
				void OnShow(bool value) override;
				void OnAdjustLayout(const D3DXVECTOR2& vpSize) override;
				void OnInvalidate() override;
			public:
				UserChat(Menu* menu, gui::Widget* parent);
				~UserChat() override;

				void ShowInput(bool show, const stringW& name, const stringW& text, const D3DXCOLOR& nameColor);
				void ClearInput() const;
				void CharInput(wchar_t value) const;
				bool IsInputVisible() const;
				stringW inputText() const;

				void PushLine(const stringW& name, const stringW& text, const D3DXCOLOR& nameColor);
				Lines::iterator DelLine(Lines::const_iterator iter);
				void DelLines();
				const Lines& lines() const;

				virtual void OnProgress(float deltaTime);

				const D3DXVECTOR2& linesPos() const;
				void linesPos(const D3DXVECTOR2& value);

				const D3DXVECTOR2& inputPos() const;
				void inputPos(const D3DXVECTOR2& value);

				const D3DXVECTOR2& linesSize() const;
				void linesSize(const D3DXVECTOR2& value);

				const D3DXVECTOR2& inputSize() const;
				void inputSize(const D3DXVECTOR2& value);
			};
		}
	}
}
