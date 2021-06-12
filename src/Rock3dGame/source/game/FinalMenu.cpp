#include "stdafx.h"
#include "game\Menu.h"

#include "game\FinalMenu.h"
#include "game\GameMode.h"

namespace r3d
{

namespace game
{

FinalMenu::FinalMenu(Menu* menu, gui::Widget* parent): MenuFrame(menu, parent), _time(0), _linesSizeY(0)
{
	const D3DXCOLOR color1 = D3DXCOLOR(214.0f, 214.0f, 214.0f, 255.0f)/255.0f;

	StringValue strMenuItems[cMenuItemEnd] = {svBack};

	for (int i = 0; i < cMenuItemEnd; ++i)
		_menuItems[i] = menu->CreateMenuButton(strMenuItems[i], "Header", "GUI\\buttonBg2.png", "GUI\\buttonBgSel2.png", root(), this, IdentityVec2, gui::Button::bsSelAnim, color1);

	_linesRoot = menu->CreateDummy(root(), NULL);
}

FinalMenu::~FinalMenu()
{
	Show(false);

	DeleteAll();

	menu()->ReleaseWidget(_linesRoot);

	for (int i = 0; i < cMenuItemEnd; ++i)
		menu()->ReleaseWidget(_menuItems[i]);
}

void FinalMenu::AddLineBox(const lsl::string& caption, const lsl::string& text)
{
	LineBox line;

	line.caption = menu()->CreateLabel(caption, _linesRoot, "Small", NullVec2, gui::Text::haCenter, gui::Text::vaTop, 0xFFDC0000);	
	line.caption->SetAlign(gui::Widget::waTop);
	line.caption->SetWordWrap(true);

	line.text = menu()->CreateLabel(text, line.caption, "Small", NullVec2, gui::Text::haCenter, gui::Text::vaTop, 0xFFFFD6CD);
	line.text->SetAlign(gui::Widget::waTop);
	line.text->SetWordWrap(true);

	_lineBoxes.push_back(line);
}

void FinalMenu::AddSlide(const lsl::string& image)
{
	Slide slide;

	slide.plane = menu()->CreatePlane(root(), this, image, true, IdentityVec2, gui::Material::bmTransparency);
	slide.plane->GetMaterial().GetSampler().SetFiltering(graph::Sampler2d::sfLinear);

	_slides.push_back(slide);
}

void FinalMenu::DeleteAll()
{
	for (unsigned i = 0; i < _lineBoxes.size(); ++i)
		menu()->ReleaseWidget(_lineBoxes[i].caption);
	_lineBoxes.clear();

	for (unsigned i = 0; i < _slides.size(); ++i)
		menu()->ReleaseWidget(_slides[i].plane);
	_slides.clear();
}

void FinalMenu::OnShow(bool value)
{	
	if (value)
	{
		//menu()->ShowMessage("", GetString("svFinalMessage"), GetString(svOk), uiRoot()->GetVPSize()/2, gui::Widget::waCenter, 1.0f);
		menu()->GetGame()->menuMusic()->Pause(true);
		menu()->PlayMusic("Music\\TrackFinal.ogg", "", "", false);

		_time = 0.0f;
	}
	else
		menu()->GetGame()->menuMusic()->Pause(false);

	Menu::NavElement elements[] = {
		{_menuItems[miOk], {NULL, NULL, NULL, NULL}, {vkBack, cVirtualKeyEnd}}
	};

	menu()->SetNavElements(_menuItems[miOk], value, elements, ARRAY_LENGTH(elements));
}

void FinalMenu::OnAdjustLayout(const D3DXVECTOR2& vpSize)
{
	for (int i = 0; i < cMenuItemEnd; ++i)
		_menuItems[i]->SetPos(D3DXVECTOR2(_menuItems[i]->GetSize().x/2, vpSize.y - 60.0f + i * (_menuItems[i]->GetSize().y + 10.0f)));

	for (unsigned i = 0; i < _slides.size(); ++i)
	{
		_slides[i].plane->SetPos(D3DXVECTOR2((vpSize.x - 400.0f)/2.0f, vpSize.y/2));
		_slides[i].plane->SetSize(menu()->StretchImage(_slides[i].plane->GetMaterial(), D3DXVECTOR2(vpSize.x - 500.0f, vpSize.y - 300.0f), true, false));
	}

	_linesSizeY = 0.0f;

	for (unsigned i = 0; i < _lineBoxes.size(); ++i)
	{
		_lineBoxes[i].caption->SetPos(D3DXVECTOR2(0.0f, _linesSizeY));
		_lineBoxes[i].caption->SetSize(D3DXVECTOR2(480.0f, 0.0f));

		_lineBoxes[i].text->SetPos(D3DXVECTOR2(0.0f, _lineBoxes[i].caption->GetTextAABB().GetSize().y + 10.0f));
		_lineBoxes[i].text->SetSize(D3DXVECTOR2(480.0f, 0.0f));

		_linesSizeY += _lineBoxes[i].caption->GetTextAABB().GetSize().y + _lineBoxes[i].text->GetTextAABB().GetSize().y + 40.0f + 10.0f;
	}
}

void FinalMenu::OnInvalidate()
{
	DeleteAll();

	lsl::string credits = GetString(svCredits);
	lsl::StringVec creditsVec;
	lsl::StrExtractValues(credits, creditsVec, "\n\n");

	for (unsigned i = 0; i < creditsVec.size(); ++i)
	{
		lsl::string credits = creditsVec[i];
		int pos = credits.find('\n');

		AddLineBox(pos >= 0 ? credits.substr(0, pos) : credits, pos >= 0 ? credits.substr(pos + 1, credits.size() - pos - 1) : "");
	}

	AddSlide("GUI\\Slides\\slide1.dds");
	AddSlide("GUI\\Slides\\slide2.dds");
	AddSlide("GUI\\Slides\\slide3.dds");
	AddSlide("GUI\\Slides\\slide4.dds");
	AddSlide("GUI\\Slides\\slide5.dds");
	AddSlide("GUI\\Slides\\slide6.dds");
	AddSlide("GUI\\Slides\\slide7.dds");
	AddSlide("GUI\\Slides\\slide8.dds");
	AddSlide("GUI\\Slides\\slide9.dds");
}

bool FinalMenu::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
{
	if (sender == _menuItems[miOk])
	{
		menu()->SetState(Menu::msMain2);
		return true;
	}

	return false;
}

void FinalMenu::OnProgress(float deltaTime)
{
	const float cDuration = 107.0f;

	D3DXVECTOR2 vpSize = uiRoot()->GetVPSize();

	_time += deltaTime;
	float alpha = lsl::ClampValue(_time / cDuration, 0.0f, 1.0f);

	_linesRoot->SetPos(D3DXVECTOR2(vpSize.x - 250.0f, vpSize.y - alpha * (_linesSizeY + vpSize.y)));

	for (unsigned i = 0; i < _slides.size(); ++i)
	{
		float alpha1 = i / static_cast<float>(_slides.size());
		float alpha2 = (i + 1) / static_cast<float>(_slides.size());
		float duration = (alpha2 - alpha1) * cDuration;
		float time = lsl::ClampValue((alpha - alpha1) * cDuration, 0.0f, duration);
		float mAlpha = lsl::ClampValue(time / 1.0f, 0.0f, 1.0f) - lsl::ClampValue((time + 0.0f - duration) / 1.0f, 0.0f, 1.0f);

		_slides[i].plane->GetMaterial().SetAlpha(mAlpha);
	}

	if (alpha == 1.0f)
	{
#ifdef STEAM_SERVICE
		steamService()->steamStats()->UnlockAchievment(SteamStats::atSeeYouSoon);
#endif

		menu()->SetState(Menu::msMain2);
	}
}




/*FinalMenu::FinalMenu(Menu* menu, gui::Widget* parent): MenuFrame(menu, parent), _scrollTime(0)
{
	D3DXCOLOR color1(0xffafafaf);

	StringValue strMenuItems[cMenuItemEnd] = {svOk};

	StringValue strLabels[cLabelEnd] = {svCredits};
	std::string fontLabels[cLabelEnd] = {"Small"};
	gui::Text::HorAlign horLabels[cLabelEnd] = {gui::Text::haCenter};
	gui::Text::VertAlign vertLabels[cLabelEnd] = {gui::Text::vaTop};
	D3DXCOLOR colorLabels[cLabelEnd] = {D3DXCOLOR(0xffff8a70)};

	_bg = menu->CreatePlane(root(), this, "GUI\\final.dds", true, IdentityVec2, gui::Material::bmTransparency);
	_bg->SetAlign(gui::Widget::waCenter);
	_bg->SetAnchor(gui::Widget::waCenter);
	_bg->GetMaterial().GetSampler().SetFiltering(graph::Sampler2d::sfLinear);

	gui::Widget* labelsParent[cLabelEnd] = {root()};
	for (int i = 0; i < cLabelEnd; ++i)	
		_labels[i] = menu->CreateLabel(strLabels[i], labelsParent[i], fontLabels[i], NullVec2, horLabels[i], vertLabels[i], colorLabels[i]);
	
	_labels[mlCredits]->SetAlign(gui::Widget::waBottom);
	_labels[mlCredits]->SetAnchor(gui::Widget::waBottom);
	_labels[mlCredits]->SetFlag(gui::Widget::wfClientClip, true);

	for (int i = 0; i < cMenuItemEnd; ++i)
		_menuItems[i] = menu->CreateMenuButton(strMenuItems[i], "Header", "GUI\\buttonBg2.png", "GUI\\buttonBgSel2.png", root(), this, IdentityVec2, gui::Button::bsSelAnim, color1);
}

FinalMenu::~FinalMenu()
{
	for (int i = 0; i < cLabelEnd; ++i)
		menu()->ReleaseWidget(_labels[i]);

	for (int i = 0; i < cMenuItemEnd; ++i)
		menu()->ReleaseWidget(_menuItems[i]);

	menu()->ReleaseWidget(_bg);
}

void FinalMenu::OnShow(bool value)
{	
	if (value)
	{
		menu()->ShowMessage("", GetString("svFinalMessage"), GetString(svOk), uiRoot()->GetVPSize()/2, gui::Widget::waCenter, 1.0f);
		menu()->GetGame()->menuMusic()->Play(0, true, false);

		_labels[mlCredits]->SetVScroll(uiRoot()->GetVPSize().y);
		_scrollTime = 2.0f;
	}
}

void FinalMenu::OnAdjustLayout(const D3DXVECTOR2& vpSize)
{
	_bg->SetSize(menu()->StretchImage(_bg->GetMaterial(), vpSize, true, true));

	_labels[mlCredits]->SetPos(D3DXVECTOR2(vpSize.x/2, vpSize.y/2 - 115.0f));
	_labels[mlCredits]->SetSize(500.0f, vpSize.y);
	
	for (int i = 0; i < cMenuItemEnd; ++i)
		_menuItems[i]->SetPos(D3DXVECTOR2(_menuItems[i]->GetSize().x/2, vpSize.y - 60.0f + i * (_menuItems[i]->GetSize().y + 10.0f)));
}

void FinalMenu::OnInvalidate()
{
	
}

bool FinalMenu::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
{
	if (sender == _menuItems[miOk])
	{
		menu()->SetState(Menu::msMain2);
		return true;
	}

	return false;
}

void FinalMenu::OnProgress(float deltaTime)
{
	if (_scrollTime > 0 && (_scrollTime -= deltaTime) <= 0)
	{
		_scrollTime = 0;
	}

	if (_scrollTime == 0)
	{
		_labels[mlCredits]->SetVScroll(_labels[mlCredits]->GetVScroll() - 20.0f * deltaTime);

		if (abs(_labels[mlCredits]->GetVScroll()) > _labels[mlCredits]->GetTextAABB().GetSize().y)
			_labels[mlCredits]->SetVScroll(_labels[mlCredits]->GetSize().y);
	}
}*/

}

}