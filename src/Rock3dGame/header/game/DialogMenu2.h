#pragma once

#include "MenuSystem.h"

namespace r3d
{

namespace game
{

namespace n
{

class AcceptDialog: public MenuFrame
{
private:
	enum Label {mlInfo, cLabelEnd};
	enum MenuItem {miNo, miYes, cMenuItemEnd};
private:
	bool _resultYes;
	Object* _data;
	gui::Widget::Event* _guiEvent;

	gui::PlaneFon* _menuBg;
	gui::Label* _labels[cLabelEnd];
	gui::Button* _menuItems[cMenuItemEnd];
protected:
	virtual void OnShow(bool value);
	virtual void OnAdjustLayout(const glm::vec2& vpSize);
	virtual void OnInvalidate();

	virtual bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick);
public:
	AcceptDialog(Menu* menu, gui::Widget* parent);
	virtual ~AcceptDialog();

	void Show(const std::string& message, const std::string& yesText, const std::string& noText, const glm::vec2& pos, gui::Widget::Anchor align, gui::Widget::Event* guiEvent, Object* data = NULL, bool maxButtonsSize = false, bool maxMode = false, bool disableFocus = false);
	void Hide();

	bool resultYes() const;
	Object* data() const;

	using MenuFrame::root;
};

class WeaponDialog: public MenuFrame
{
private:
	enum Label {mlInfo, mlMoney, mlDamage, mlName, cLabelEnd};
private:
	gui::PlaneFon* _menuBg;
	gui::Label* _labels[cLabelEnd];
protected:
	virtual void OnShow(bool value);
	virtual void OnAdjustLayout(const glm::vec2& vpSize);
	virtual void OnInvalidate();

	virtual bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick);
public:
	WeaponDialog(Menu* menu, gui::Widget* parent);
	virtual ~WeaponDialog();

	void Show(const std::string& title, const std::string& message, const std::string& moneyText, const std::string& damageText, const glm::vec2& pos, gui::Widget::Anchor align);
	void Hide();

	using MenuFrame::root;
};

class InfoDialog: public MenuFrame
{
private:
	enum Label {mlInfo, mlTitle, cLabelEnd};
	enum MenuItem {miOk, cMenuItemEnd};
private:
	Object* _data;
	gui::Widget::Event* _guiEvent;

	gui::PlaneFon* _menuBg;
	gui::Label* _labels[cLabelEnd];
	gui::Button* _menuItems[cMenuItemEnd];
protected:
	virtual void OnShow(bool value);
	virtual void OnAdjustLayout(const glm::vec2& vpSize);
	virtual void OnInvalidate();

	virtual bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick);
public:
	InfoDialog(Menu* menu, gui::Widget* parent);
	virtual ~InfoDialog();

	void Show(const std::string& titleText, const std::string& message, const std::string& okText, const glm::vec2& pos, gui::Widget::Anchor align, gui::Widget::Event* guiEvent, Object* data = NULL, bool okButton = true);
	void Hide();

	Object* data() const;

	using MenuFrame::root;
};

class MusicDialog: public MenuFrame
{
private:
	enum Label {mlInfo, mlTitle, cLabelEnd};
private:
	gui::PlaneFon* _menuBg;
	gui::Label* _labels[cLabelEnd];
protected:
	virtual void OnShow(bool value);
	virtual void OnAdjustLayout(const glm::vec2& vpSize);
	virtual void OnInvalidate();

	virtual bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick);
public:
	MusicDialog(Menu* menu, gui::Widget* parent);
	virtual ~MusicDialog();

	void Show(const std::string& title, const std::string& message);
	void Hide();

	const glm::vec2& size() const;

	using MenuFrame::root;
};

class InfoMenu
{
public:
	enum State {msLoading = 0, cStateEnd};
private:
	Menu* _menu;
	State _state;

	gui::Dummy* _root;
	gui::PlaneFon* _loadingFrame;

	void ApplyState(State state);
public:
	InfoMenu(Menu* menu, gui::Widget* parent);
	virtual ~InfoMenu();

	void AdjustLayout(const glm::vec2& vpSize);
	void Show(bool value);
	void Start();
	void Stop();
	void Finished();

	void OnFrame(float deltaTime);

	gui::Widget* GetRoot();

	State GetState() const;
	void SetState(State value);
};

class UserChat: public MenuFrame
{
	struct Line
	{
		gui::Label* text;
		gui::Label* name;
		float time;
	};
	typedef lsl::List<Line> Lines;

	static const int cMaxLines;
private:
	Lines _lines;
	Line _input;

	glm::vec2 _linesPos;
	glm::vec2 _inputPos;
	glm::vec2 _linesSize;
	glm::vec2 _inputSize;

	Line AddLine(const lsl::stringW& name, const lsl::stringW& text, const D3DXCOLOR& nameColor, bool right);
protected:
	virtual void OnShow(bool value);
	virtual void OnAdjustLayout(const glm::vec2& vpSize);
	virtual void OnInvalidate();
public:
	UserChat(Menu* menu, gui::Widget* parent);
	virtual ~UserChat();

	void ShowInput(bool show, const lsl::stringW& name, const lsl::stringW& text, const D3DXCOLOR& nameColor);
	void ClearInput();
	void CharInput(wchar_t value);
	bool IsInputVisible() const;
	lsl::stringW inputText() const;

	void PushLine(const lsl::stringW& name, const lsl::stringW& text, const D3DXCOLOR& nameColor);
	Lines::iterator DelLine(const Lines::const_iterator iter);
	void DelLines();
	const Lines& lines() const;

	virtual void OnProgress(float deltaTime);

	const glm::vec2& linesPos() const;
	void linesPos(const glm::vec2& value);

	const glm::vec2& inputPos() const;
	void inputPos(const glm::vec2& value);

	const glm::vec2& linesSize() const;
	void linesSize(const glm::vec2& value);

	const glm::vec2& inputSize() const;
	void inputSize(const glm::vec2& value);
};

}

}

}