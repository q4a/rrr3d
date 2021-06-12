#pragma once

#include "MenuSystem.h"

namespace r3d
{

namespace game
{

class FinalMenu: public MenuFrame
{
private:
	enum Label {mlCredits, cLabelEnd};
	enum MenuItem {miOk, cMenuItemEnd};

	struct LineBox
	{
		gui::Label* caption;
		gui::Label* text;
	};
	typedef lsl::Vector<LineBox> LineBoxes;

	struct Slide
	{
		gui::PlaneFon* plane;		
	};
	typedef lsl::Vector<Slide> Slides;
private:
	gui::Button* _menuItems[cMenuItemEnd];
	gui::Widget* _linesRoot;
	LineBoxes _lineBoxes;
	Slides _slides;

	float _time;
	float _linesSizeY;

	void AddLineBox(const lsl::string& caption, const lsl::string& text);
	void AddSlide(const lsl::string& image);
	void DeleteAll();
protected:
	virtual void OnShow(bool value);
	virtual void OnAdjustLayout(const D3DXVECTOR2& vpSize);
	virtual void OnInvalidate();

	bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick);
public:
	FinalMenu(Menu* menu, gui::Widget* parent);
	virtual ~FinalMenu();

	void OnProgress(float deltaTime);
};

/*class FinalMenu: public MenuFrame
{
private:
	enum Label {mlCredits, cLabelEnd};
	enum MenuItem {miOk, cMenuItemEnd};
private:
	gui::Label* _labels[cLabelEnd];
	gui::Button* _menuItems[cMenuItemEnd];
	gui::PlaneFon* _bg;

	float _scrollTime;
protected:
	virtual void OnShow(bool value);
	virtual void OnAdjustLayout(const D3DXVECTOR2& vpSize);
	virtual void OnInvalidate();

	bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick);
public:
	FinalMenu(Menu* menu, gui::Widget* parent);
	virtual ~FinalMenu();

	void OnProgress(float deltaTime);
};*/

}

}