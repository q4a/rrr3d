#include "stdafx.h"
#include "game/Menu.h"

#include "game/DialogMenu2.h"

namespace r3d
{
	namespace game
	{
		namespace n
		{
			const int UserChat::cMaxLines = 50;


			AcceptDialog::AcceptDialog(Menu* menu, gui::Widget* parent) : MenuFrame(menu, parent), _resultYes(false),
				_data(nullptr), _guiEvent(nullptr)
			{
				constexpr StringValue strMenuItems[cMenuItemEnd] = { svNo, svYes };
				const D3DXCOLOR color1(0xffafafaf);

				constexpr StringValue strLabels[cLabelEnd] = { svNull };
				const std::string fontLabels[cLabelEnd] = { "Item" };
				constexpr gui::Text::HorAlign horLabels[cLabelEnd] = { gui::Text::haCenter };
				constexpr gui::Text::VertAlign vertLabels[cLabelEnd] = { gui::Text::vaCenter };
				const D3DXCOLOR colorLabels[cLabelEnd] = { color1 };

				_menuBg1 = menu->CreatePlane(root(), this, "GUI\\HUD\\dlgWnd1.png", true, IdentityVec2,
					gui::Material::bmTransparency);
				_menuBg2 = menu->CreatePlane(root(), this, "GUI\\HUD\\dlgWnd2.png", true, IdentityVec2,
					gui::Material::bmTransparency);
				_menuBg3 = menu->CreatePlane(root(), this, "GUI\\HUD\\dlgWnd3.png", true, IdentityVec2,
					gui::Material::bmTransparency);
				_menuBg4 = menu->CreatePlane(root(), this, "GUI\\HUD\\dlgWnd4.png", true, IdentityVec2,
					gui::Material::bmTransparency);

				gui::Widget* labelsParent[cLabelEnd] = { _menuBg1 };
				for (int i = 0; i < cLabelEnd; ++i)
					_labels[i] = menu->CreateLabel(strLabels[i], labelsParent[i], fontLabels[i], NullVec2, horLabels[i],
						vertLabels[i], colorLabels[i]);
				_labels[mlInfo]->SetWordWrap(true);

				for (int i = 0; i < cMenuItemEnd; ++i)
				{
					_menuItems[i] = menu->CreateMenuButton(strMenuItems[i], "Item", "GUI\\HUD\\button1.png",
						"GUI\\HUD\\button1Sel.png", _menuBg1, this, IdentityVec2,
						gui::Button::bsSelAnim, color1);
					_menuItems2[i] = menu->CreateMenuButton(strMenuItems[i], "Item", "GUI\\HUD\\button2.png",
						"GUI\\HUD\\button2Sel.png", _menuBg2, this, IdentityVec2,
						gui::Button::bsSelAnim, color1);
					_menuItems3[i] = menu->CreateMenuButton(strMenuItems[i], "Item", "GUI\\HUD\\button3.png",
						"GUI\\HUD\\button3Sel.png", _menuBg3, this, IdentityVec2,
						gui::Button::bsSelAnim, color1);
					_menuItems4[i] = menu->CreateMenuButton(strMenuItems[i], "Item", "GUI\\HUD\\button4.png",
						"GUI\\HUD\\button4Sel.png", _menuBg4, this, IdentityVec2,
						gui::Button::bsSelAnim, color1);
				}

				_menuBg1->SetEnabled(false);
				_menuBg2->SetEnabled(false);
				_menuBg3->SetEnabled(false);
				_menuBg4->SetEnabled(false);
			}

			AcceptDialog::~AcceptDialog()
			{
				menu()->UnregNavElements(_menuItems[0]);
				menu()->UnregNavElements(_menuItems2[0]);
				menu()->UnregNavElements(_menuItems3[0]);
				menu()->UnregNavElements(_menuItems4[0]);
				menu()->UnregNavElements(_menuBg1);
				menu()->UnregNavElements(_menuBg2);
				menu()->UnregNavElements(_menuBg3);
				menu()->UnregNavElements(_menuBg4);

				_data = nullptr;
				SafeRelease(_guiEvent);

				for (const auto& _label : _labels)
					menu()->ReleaseWidget(_label);
			}

			void AcceptDialog::OnShow(bool value)
			{
				if (!value)
				{
					menu()->UnregNavElements(_menuItems[0]);
					menu()->UnregNavElements(_menuItems2[0]);
					menu()->UnregNavElements(_menuItems3[0]);
					menu()->UnregNavElements(_menuItems4[0]);

					menu()->UnregNavElements(_menuBg1);

					_data = nullptr;
					SafeRelease(_guiEvent);
				}

				{
					Menu::NavElement elements[] = {
						{
							_menuItems[miNo], {_menuItems[miYes], _menuItems[miYes], nullptr, nullptr},
							{vkBack, cVirtualKeyEnd}
						},
						{
							_menuItems[miYes], {_menuItems[miNo], _menuItems[miNo], nullptr, nullptr},
							{cVirtualKeyEnd, cVirtualKeyEnd}
						}
					};

					menu()->SetNavElements(_menuItems[miYes], value, elements, ARRAY_LENGTH(elements));
				}
			}

			void AcceptDialog::OnAdjustLayout(const D3DXVECTOR2& vpSize)
			{
				if (ENABLE_SPLIT_SCREEN == true)
				{
					if (SPLIT_TYPE == 1)
						_menuBg1->SetSize(192, 156);
					else if (SPLIT_TYPE == 2)
						_menuBg1->SetSize(384, 78);
				}
			}

			void AcceptDialog::OnInvalidate()
			{
			}

			bool AcceptDialog::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
			{
				if (sender == _menuItems[miYes] || sender == _menuItems2[miYes] || sender == _menuItems3[miYes] ||
					sender == _menuItems4[miYes])
				{
					_resultYes = true;

					gui::Widget::Event* guiEvent = _guiEvent;
					SafeRelease(_guiEvent);

					Object* data = _data;
					Hide();
					_data = data;

					if (guiEvent)
						guiEvent->OnClick(root(), mClick);

					_data = nullptr;

					return true;
				}

				if (sender == _menuItems[miNo] || sender == _menuItems2[miNo] || sender == _menuItems3[miNo] || sender
					== _menuItems4[miNo])
				{
					_resultYes = false;

					gui::Widget::Event* guiEvent = _guiEvent;
					SafeRelease(_guiEvent);

					if (guiEvent)
						guiEvent->OnClick(root(), mClick);

					Hide();
					return true;
				}

				return false;
			}

			void AcceptDialog::Show(bool fixmenu, const std::string& message, const std::string& yesText,
				const std::string& noText, const D3DXVECTOR2& pos, gui::Widget::Anchor align,
				gui::Widget::Event* guiEvent, Object* data, bool maxButtonsSize, bool maxMode,
				bool disableFocus)
			{
				D3DXVECTOR2 bgSize;
				auto infoSize = D3DXVECTOR2(325.0f, 65.0f);
				const auto infoPos = D3DXVECTOR2(0.0f, -25.0f);
				auto posYes = D3DXVECTOR2(-70.0f, 32.0f);
				auto posNo = D3DXVECTOR2(70.0f, 32.0f);
				D3DXVECTOR2 sizeYesNo;


				if (fixmenu == true)
				{
					bgSize = _menuBg1->GetMaterial().GetSampler().GetSize();
					_menuBg1->SetVisible(true);
					_menuBg2->SetVisible(false);
					_menuBg3->SetVisible(false);
					_menuBg4->SetVisible(false);

					_menuItems[miNo]->SetVisible(true);
					_menuItems[miYes]->SetVisible(true);
					_menuItems[miNo]->SetEnabled(true);
					_menuItems[miYes]->SetEnabled(true);

					_menuItems2[miNo]->SetVisible(false);
					_menuItems2[miYes]->SetVisible(false);
					_menuItems2[miNo]->SetEnabled(false);
					_menuItems2[miYes]->SetEnabled(false);

					_menuItems3[miNo]->SetVisible(false);
					_menuItems3[miYes]->SetVisible(false);
					_menuItems3[miNo]->SetEnabled(false);
					_menuItems3[miYes]->SetEnabled(false);

					_menuItems4[miNo]->SetVisible(false);
					_menuItems4[miYes]->SetVisible(false);
					_menuItems4[miNo]->SetEnabled(false);
					_menuItems4[miYes]->SetEnabled(false);

					_menuItems[miNo]->SetParent(_menuBg1);
					_menuItems[miYes]->SetParent(_menuBg1);
					_labels[mlInfo]->SetParent(_menuBg1);
					sizeYesNo = _menuItems[miNo]->GetFon()->GetSampler().GetSize();
				}
				else
				{
					{
						bgSize = _menuBg1->GetMaterial().GetSampler().GetSize();
						_menuBg1->SetVisible(true);
						_menuBg2->SetVisible(false);
						_menuBg3->SetVisible(false);
						_menuBg4->SetVisible(false);

						_menuItems[miNo]->SetVisible(true);
						_menuItems[miYes]->SetVisible(true);
						_menuItems[miNo]->SetEnabled(true);
						_menuItems[miYes]->SetEnabled(true);

						_menuItems2[miNo]->SetVisible(false);
						_menuItems2[miYes]->SetVisible(false);
						_menuItems2[miNo]->SetEnabled(false);
						_menuItems2[miYes]->SetEnabled(false);

						_menuItems3[miNo]->SetVisible(false);
						_menuItems3[miYes]->SetVisible(false);
						_menuItems3[miNo]->SetEnabled(false);
						_menuItems3[miYes]->SetEnabled(false);

						_menuItems4[miNo]->SetVisible(false);
						_menuItems4[miYes]->SetVisible(false);
						_menuItems4[miNo]->SetEnabled(false);
						_menuItems4[miYes]->SetEnabled(false);

						_menuItems[miNo]->SetParent(_menuBg1);
						_menuItems[miYes]->SetParent(_menuBg1);
						_labels[mlInfo]->SetParent(_menuBg1);
						sizeYesNo = _menuItems[miNo]->GetFon()->GetSampler().GetSize();
					}
				}

				string infoFont = "Item";
				if (maxMode)
				{
					bgSize = bgSize * 1.7f;
					infoSize = infoSize * 1.7f;
					sizeYesNo.x = sizeYesNo.x * 1.5f;
					posYes = D3DXVECTOR2(-100.0f, 72.0f);
					posNo = D3DXVECTOR2(100.0f, 72.0f);
					infoFont = "Small";
				}

				if (maxButtonsSize)
				{
					sizeYesNo = D3DXVECTOR2(sizeYesNo.x * 1.5f, sizeYesNo.y);
					posYes.x = posYes.x - 10;
					posNo.x = posNo.x + 10;
				}

				_menuBg1->SetSize(bgSize);
				_menuBg2->SetSize(bgSize);
				_menuBg3->SetSize(bgSize);
				_menuBg4->SetSize(bgSize);

				_menuItems[miNo]->SetSize(sizeYesNo);
				_menuItems[miNo]->SetSelSize(sizeYesNo);
				_menuItems[miYes]->SetSize(sizeYesNo);
				_menuItems[miYes]->SetSelSize(sizeYesNo);

				_menuItems2[miNo]->SetSize(sizeYesNo);
				_menuItems2[miNo]->SetSelSize(sizeYesNo);
				_menuItems2[miYes]->SetSize(sizeYesNo);
				_menuItems2[miYes]->SetSelSize(sizeYesNo);

				_menuItems3[miNo]->SetSize(sizeYesNo);
				_menuItems3[miNo]->SetSelSize(sizeYesNo);
				_menuItems3[miYes]->SetSize(sizeYesNo);
				_menuItems3[miYes]->SetSelSize(sizeYesNo);

				_menuItems4[miNo]->SetSize(sizeYesNo);
				_menuItems4[miNo]->SetSelSize(sizeYesNo);
				_menuItems4[miYes]->SetSize(sizeYesNo);
				_menuItems4[miYes]->SetSelSize(sizeYesNo);


				_menuItems[miNo]->SetPos(posNo);
				_menuItems[miYes]->SetPos(posYes);

				_menuItems2[miNo]->SetPos(posNo);
				_menuItems2[miYes]->SetPos(posYes);

				_menuItems3[miNo]->SetPos(posNo);
				_menuItems3[miYes]->SetPos(posYes);

				_menuItems4[miNo]->SetPos(posNo);
				_menuItems4[miYes]->SetPos(posYes);

				_labels[mlInfo]->SetPos(infoPos);
				_labels[mlInfo]->SetSize(infoSize);
				_labels[mlInfo]->SetFont(menu()->GetFont(infoFont));

				_labels[mlInfo]->SetText(message);
				_menuItems[miYes]->SetText(yesText);
				_menuItems[mlInfo]->SetText(noText);
				_menuItems2[miYes]->SetText(yesText);
				_menuItems2[mlInfo]->SetText(noText);
				_menuItems3[miYes]->SetText(yesText);
				_menuItems3[mlInfo]->SetText(noText);
				_menuItems4[miYes]->SetText(yesText);
				_menuItems4[mlInfo]->SetText(noText);

				ShowModal(true, cTopmostModal);

				SetPos(pos, align, _menuBg1->GetSize());

				if (ReplaceRef(_guiEvent, guiEvent))
					_guiEvent = guiEvent;
				_data = data;

				if (disableFocus)
				{
					if (fixmenu == true)
					{
						Menu::NavElement navElements[] = {
							{_menuBg1, {nullptr, nullptr, nullptr, nullptr}, {vkBack, cVirtualKeyEnd}}
						};

						menu()->UnregNavElements(_menuItems[miYes]);
						menu()->SetNavElements(_menuBg1, true, navElements, 1);
					}
					else
					{
						{
							Menu::NavElement navElements[] = {
								{_menuBg1, {nullptr, nullptr, nullptr, nullptr}, {vkBack, cVirtualKeyEnd}}
							};

							menu()->UnregNavElements(_menuItems[miYes]);
							menu()->SetNavElements(_menuBg1, true, navElements, 1);
						}
					}
				}
			}

			void AcceptDialog::Hide()
			{
				ShowModal(false);
			}

			bool AcceptDialog::resultYes() const
			{
				return _resultYes;
			}

			Object* AcceptDialog::data() const
			{
				return _data;
			}


			WeaponDialog::WeaponDialog(Menu* menu, gui::Widget* parent) : MenuFrame(menu, parent)
			{
				const D3DXCOLOR color1(0xffafafaf);
				const D3DXCOLOR color2(0xFFFFFFFF);

				constexpr StringValue strLabels[cLabelEnd] = { svNull, svNull, svNull, svNull };
				const std::string fontLabels[cLabelEnd] = { "VerySmall", "Small", "Small", "Small" };
				constexpr gui::Text::HorAlign horLabels[cLabelEnd] = {
					gui::Text::haLeft, gui::Text::haCenter, gui::Text::haCenter, gui::Text::haCenter
				};
				constexpr gui::Text::VertAlign vertLabels[cLabelEnd] = {
					gui::Text::vaCenter, gui::Text::vaCenter, gui::Text::vaCenter, gui::Text::vaCenter
				};
				const D3DXCOLOR colorLabels[cLabelEnd] = { color1, color2, color2, color2 };

				_menuBg = menu->CreatePlane(root(), this, "GUI\\dlgFrame3.png", true, IdentityVec2,
					gui::Material::bmTransparency);

				gui::Widget* labelsParent[cLabelEnd] = { _menuBg, _menuBg, _menuBg, _menuBg };
				for (int i = 0; i < cLabelEnd; ++i)
					_labels[i] = menu->CreateLabel(strLabels[i], labelsParent[i], fontLabels[i], NullVec2, horLabels[i],
						vertLabels[i], colorLabels[i]);
				_labels[mlInfo]->SetWordWrap(true);
			}

			WeaponDialog::~WeaponDialog()
			{
				for (const auto& _label : _labels)
					menu()->ReleaseWidget(_label);

				menu()->ReleaseWidget(_menuBg);
			}

			void WeaponDialog::OnShow(bool value)
			{
			}

			void WeaponDialog::OnAdjustLayout(const D3DXVECTOR2& vpSize)
			{
				_labels[mlInfo]->SetPos(3.0f, -3.0f);
				_labels[mlInfo]->SetSize(280.0f, 75.0f);

				_labels[mlMoney]->SetPos(-60.0f, 54.0f);
				_labels[mlDamage]->SetPos(80.0f, 54.0f);
				_labels[mlName]->SetPos(0.0f, -58.0f);
			}

			void WeaponDialog::OnInvalidate()
			{
			}

			bool WeaponDialog::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
			{
				return false;
			}

			void WeaponDialog::Show(const std::string& title, const std::string& message, const std::string& moneyText,
				const std::string& damageText, const D3DXVECTOR2& pos, gui::Widget::Anchor align)
			{
				_labels[mlName]->SetText(title);
				_labels[mlInfo]->SetText(message);
				_labels[mlMoney]->SetText(moneyText);
				_labels[mlDamage]->SetText(damageText);

				MenuFrame::Show(true);
				root()->SetFlag(gui::Widget::wfTopmost, true);
				root()->SetTopmostLevel(cTopmostDef);

				SetPos(pos, align, _menuBg->GetSize());
			}

			void WeaponDialog::Hide()
			{
				ShowModal(false);
			}


			InfoDialog::InfoDialog(Menu* menu, gui::Widget* parent) : MenuFrame(menu, parent), _data(nullptr),
				_guiEvent(nullptr)
			{
				const D3DXCOLOR color1(0xffafafaf);
				const D3DXCOLOR color2(0xffffffff);

				constexpr StringValue strMenuItems[cMenuItemEnd] = { svOk };

				constexpr StringValue strLabels[cLabelEnd] = { svNull, svNull };
				const std::string fontLabels[cLabelEnd] = { "Small", "Header" };
				constexpr gui::Text::HorAlign horLabels[cLabelEnd] = { gui::Text::haLeft, gui::Text::haCenter };
				constexpr gui::Text::VertAlign vertLabels[cLabelEnd] = { gui::Text::vaCenter, gui::Text::vaCenter };
				const D3DXCOLOR colorLabels[cLabelEnd] = { color2, color1 };

				_menuBg = menu->CreatePlane(root(), this, "GUI\\dlgFrame4.png", true, IdentityVec2,
					gui::Material::bmTransparency);

				gui::Widget* labelsParent[cLabelEnd] = { _menuBg, _menuBg };
				for (int i = 0; i < cLabelEnd; ++i)
					_labels[i] = menu->CreateLabel(strLabels[i], labelsParent[i], fontLabels[i], NullVec2, horLabels[i],
						vertLabels[i], colorLabels[i]);
				_labels[mlInfo]->SetWordWrap(true);

				for (int i = 0; i < cMenuItemEnd; ++i)
				{
					_menuItems[i] = menu->CreateMenuButton(strMenuItems[i], "Item", "GUI\\HUD\\ibutton1.png",
						"GUI\\HUD\\ibutton1Sel.png", _menuBg, this, IdentityVec2,
						gui::Button::bsSelAnim, color2);
				}
			}

			InfoDialog::~InfoDialog()
			{
				menu()->UnregNavElements(_menuItems[miOk]);

				_data = nullptr;
				SafeRelease(_guiEvent);

				for (const auto& _label : _labels)
					menu()->ReleaseWidget(_label);

				menu()->ReleaseWidget(_menuBg);
			}

			void InfoDialog::OnShow(bool value)
			{
				if (!value)
				{
					menu()->UnregNavElements(_menuItems[miOk]);

					_data = nullptr;
					SafeRelease(_guiEvent);
				}

				Menu::NavElement elements[] = {
					{_menuItems[miOk], {nullptr, nullptr, nullptr, nullptr}, {vkStart, cVirtualKeyEnd}}
				};

				menu()->SetNavElements(_menuItems[miOk], value, elements, ARRAY_LENGTH(elements));
			}

			void InfoDialog::OnAdjustLayout(const D3DXVECTOR2& vpSize)
			{
				_labels[mlTitle]->SetPos(-27.f, -105.0f);

				_labels[mlInfo]->SetPos(0.0f, 5.0f);
				_labels[mlInfo]->SetSize(245.0f, 135.0f);

				_menuItems[miOk]->SetPos(0.0f, 105.0f);
			}

			void InfoDialog::OnInvalidate()
			{
			}

			bool InfoDialog::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
			{
				if (sender == _menuItems[miOk])
				{
					gui::Widget::Event* guiEvent = _guiEvent;
					SafeRelease(_guiEvent);

					if (guiEvent)
						guiEvent->OnClick(root(), mClick);

					if (DIVISION_END == true)
					{
						if (DLG_ITTER == 0)
							menu()->SponsorMessage();
						DLG_ITTER += 1;
					}

					Hide();
					return true;
				}

				return false;
			}

			void InfoDialog::Show(const std::string& titleText, const std::string& message, const std::string& okText,
				const D3DXVECTOR2& pos, gui::Widget::Anchor align, gui::Widget::Event* guiEvent,
				Object* data, bool okButton)
			{
				_labels[mlTitle]->SetText(titleText);
				_labels[mlInfo]->SetText(message);
				_menuItems[miOk]->SetText(okText);
				_menuItems[miOk]->SetVisible(okButton);

				ShowModal(true, cTopmostModal);

				SetPos(pos, align, _menuBg->GetSize());

				if (ReplaceRef(_guiEvent, guiEvent))
					_guiEvent = guiEvent;
				_data = data;
			}

			void InfoDialog::Hide()
			{
				ShowModal(false);

				if (DIVISION_END == true && DLG_ITTER == 2 || DIVISION_END == false)
				{
					DIVISION_END = false;
					DLG_ONSHOW = false;
					DLG_ITTER = 0;
				}
			}

			Object* InfoDialog::data() const
			{
				return _data;
			}


			MusicDialog::MusicDialog(Menu* menu, gui::Widget* parent) : MenuFrame(menu, parent)
			{
				const D3DXCOLOR color1(0xffafafaf);
				const D3DXCOLOR color2(0xFFFFFFFF);

				constexpr StringValue strLabels[cLabelEnd] = { svNull, svNull };
				const std::string fontLabels[cLabelEnd] = { "Small", "Item" };
				constexpr gui::Text::HorAlign horLabels[cLabelEnd] = { gui::Text::haLeft, gui::Text::haLeft };
				constexpr gui::Text::VertAlign vertLabels[cLabelEnd] = { gui::Text::vaCenter, gui::Text::vaCenter };
				const D3DXCOLOR colorLabels[cLabelEnd] = { color1, color2 };

				_menuBg = menu->CreatePlane(root(), this, "GUI\\HUD\\mWnd1.png", true, IdentityVec2,
					gui::Material::bmTransparency);
				_menuBg2 = menu->CreatePlane(root(), this, "GUI\\HUD\\mWnd2.png", true, IdentityVec2,
					gui::Material::bmTransparency);
				_menuBg3 = menu->CreatePlane(root(), this, "GUI\\HUD\\mWnd3.png", true, IdentityVec2,
					gui::Material::bmTransparency);
				_menuBg4 = menu->CreatePlane(root(), this, "GUI\\HUD\\mWnd4.png", true, IdentityVec2,
					gui::Material::bmTransparency);

				gui::Widget* labelsParent[cLabelEnd] = { _menuBg, _menuBg };
				for (int i = 0; i < cLabelEnd; ++i)
				{
					_labels[i] = menu->CreateLabel(strLabels[i], labelsParent[i], fontLabels[i], NullVec2, horLabels[i],
						vertLabels[i], colorLabels[i]);
					_labels[i]->SetAlign(gui::Widget::waLeft);
				}

				_labels[mlInfo]->SetWordWrap(true);
			}

			MusicDialog::~MusicDialog()
			{
				for (const auto& _label : _labels)
					menu()->ReleaseWidget(_label);

				menu()->ReleaseWidget(_menuBg);
				menu()->ReleaseWidget(_menuBg2);
				menu()->ReleaseWidget(_menuBg3);
				menu()->ReleaseWidget(_menuBg4);
			}

			void MusicDialog::OnShow(bool value)
			{
			}

			void MusicDialog::OnAdjustLayout(const D3DXVECTOR2& vpSize)
			{
				{
					_menuBg->SetVisible(false);
					_menuBg2->SetVisible(false);
					_menuBg3->SetVisible(false);
					_menuBg4->SetVisible(false);
				}

				_labels[mlInfo]->SetPos(-130.0f, 17.0f);
				_labels[mlInfo]->SetSize(290.0f, 45.0f);

				_labels[mlTitle]->SetPos(-130.0f, -21.0f);
			}

			void MusicDialog::OnInvalidate()
			{
			}

			bool MusicDialog::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
			{
				return false;
			}

			void MusicDialog::Show(const std::string& title, const std::string& message)
			{
				_labels[mlTitle]->SetText(title);
				_labels[mlInfo]->SetText(message);

				MenuFrame::Show(true);
				root()->SetFlag(gui::Widget::wfTopmost, true);
				root()->SetTopmostLevel(cTopmostPopup);
			}

			void MusicDialog::Hide()
			{
				ShowModal(false);
			}

			const D3DXVECTOR2& MusicDialog::size() const
			{
				return _menuBg->GetSize();
			}


			InfoMenu::InfoMenu(Menu* menu, gui::Widget* parent) : _menu(menu), _state(msLoading)
			{
				_root = _menu->GetGUI()->CreateDummy();
				_root->SetParent(parent);

				_loadingFrame = _menu->CreatePlane(_root, nullptr, "GUI\\loadingFrame.dds", true);
				_loadingFrame->GetMaterial().GetSampler().SetFiltering(graph::Sampler2d::sfLinear);
				_loadingFrame->SetFlag(gui::Widget::wfTopmost, true);
				_loadingFrame->SetTopmostLevel(MenuFrame::cTopmostLoading);

				ApplyState(_state);
			}

			InfoMenu::~InfoMenu()
			{
				_menu->GetGUI()->ReleaseWidget(_loadingFrame);
				_menu->GetGUI()->ReleaseWidget(_root);
			}

			void InfoMenu::ApplyState(State state) const
			{
				_loadingFrame->SetVisible(_state == msLoading);
			}

			void InfoMenu::AdjustLayout(const D3DXVECTOR2& vpSize) const
			{
				_loadingFrame->SetSize(_menu->GetImageAspectSize(_loadingFrame->GetMaterial(), vpSize));
			}

			void InfoMenu::Show(bool value) const
			{
				GetRoot()->SetVisible(value);
			}

			gui::Widget* InfoMenu::GetRoot() const
			{
				return _root;
			}

			InfoMenu::State InfoMenu::GetState() const
			{
				return _state;
			}

			void InfoMenu::SetState(State value)
			{
				if (_state != value)
				{
					_state = value;
					ApplyState(_state);
				}
			}


			UserChat::UserChat(Menu* menu, gui::Widget* parent) : MenuFrame(menu, parent), _linesPos(NullVector),
				_inputPos(NullVector), _linesSize(300.0f, 300.0f),
				_inputSize(300.0f, 300.0f)
			{
				_input = AddLine(L"", L"", clrWhite, false);
				_input.name->SetVisible(false);
			}

			UserChat::~UserChat()
			{
				menu()->ReleaseWidget(_input.name);

				DelLines();
			}

			UserChat::Line UserChat::AddLine(const stringW& name, const stringW& text, const D3DXCOLOR& nameColor,
				bool right) const
			{
				// ReSharper disable once CppInitializedValueIsAlwaysRewritten
				Line line{};
				line.time = 0.0f;

				line.name = menu()->CreateLabel("", root(), "Small", NullVec2,
					right ? gui::Text::haRight : gui::Text::haLeft,
					right ? gui::Text::vaTop : gui::Text::vaBottom, nameColor);
				line.name->SetTextW(name);
				line.name->SetAlign(right ? gui::Widget::waRightTop : gui::Widget::waLeftBottom);

				line.text = menu()->CreateLabel("", root(), "Small", NullVec2,
					right ? gui::Text::haRight : gui::Text::haLeft,
					right ? gui::Text::vaTop : gui::Text::vaBottom, clrWhite);
				line.text->SetTextW(text);
				line.text->SetAlign(right ? gui::Widget::waRightTop : gui::Widget::waLeftBottom);
				line.text->SetParent(line.name);
				line.text->SetWordWrap(true);

				return line;
			}

			void UserChat::OnShow(bool value)
			{
			}

			void UserChat::OnAdjustLayout(const D3DXVECTOR2& vpSize)
			{
				const AABB2 nameAABB = _input.name->GetTextAABB();

				_input.name->SetPos(_inputPos);
				_input.text->SetPos(D3DXVECTOR2(nameAABB.GetSize().x, 0.0f));
				_input.text->SetSize(_inputSize - D3DXVECTOR2(_input.text->GetPos().x, 0.0f));
			}

			void UserChat::OnInvalidate()
			{
			}

			void UserChat::ShowInput(bool show, const stringW& name, const stringW& text, const D3DXCOLOR& nameColor)
			{
				_input.name->SetVisible(show);
				_input.name->SetTextW(name);
				_input.name->GetMaterial().SetColor(nameColor);

				_input.text->SetTextW(text);

				Invalidate();
			}

			void UserChat::ClearInput() const
			{
				_input.text->SetTextW(L"");
			}

			void UserChat::CharInput(wchar_t value) const
			{
				const stringW text = _input.text->GetTextW();

				if (value == VK_BACK)
				{
					if (!text.empty())
						_input.text->SetTextW(text.substr(0, text.size() - 1));
				}
				else
					_input.text->SetTextW(text + value);
			}

			bool UserChat::IsInputVisible() const
			{
				return _input.name->GetVisible();
			}

			stringW UserChat::inputText() const
			{
				return _input.text->GetTextW();
			}

			void UserChat::PushLine(const stringW& name, const stringW& text, const D3DXCOLOR& nameColor)
			{
				_lines.push_front(AddLine(name, text, nameColor, true));
			}

			UserChat::Lines::iterator UserChat::DelLine(const Lines::const_iterator iter)
			{
				menu()->ReleaseWidget(iter->name);

				return _lines.erase(iter);
			}

			void UserChat::DelLines()
			{
				while (!_lines.empty())
					DelLine(_lines.begin());
			}

			const UserChat::Lines& UserChat::lines() const
			{
				return _lines;
			}

			void UserChat::OnProgress(float deltaTime)
			{
				float posY = 0.0f;

				for (auto iter = _lines.begin(); iter != _lines.end();)
				{
					if (iter->time >= 0.0f)
					{
						iter->time += deltaTime;
						const float alpha = ClampValue((iter->time - 10.0f) / 1.0f, 0.0f, 1.0f);

						iter->text->GetMaterial().SetAlpha(1.0f - alpha);
						iter->name->GetMaterial().SetAlpha(1.0f - alpha);

						if (alpha == 1.0f)
							iter->time = -1.0f;
						else
						{
							AABB2 nameAABB = iter->name->GetTextAABB();

							iter->name->SetPos(_linesPos + D3DXVECTOR2(0.0f, posY));
							iter->text->SetPos(-nameAABB.GetSize().x, 0.0f);
							iter->text->SetSize(_linesSize - D3DXVECTOR2(nameAABB.GetSize().x, 0.0f));

							AABB2 textAABB = iter->text->GetTextAABB();
							posY += textAABB.GetSize().y;
						}
					}

					if (iter->time == -1)
						iter = DelLine(iter);
					else
						++iter;
				}
			}

			const D3DXVECTOR2& UserChat::linesPos() const
			{
				return _linesPos;
			}

			void UserChat::linesPos(const D3DXVECTOR2& value)
			{
				_linesPos = value;
			}

			const D3DXVECTOR2& UserChat::inputPos() const
			{
				return _inputPos;
			}

			void UserChat::inputPos(const D3DXVECTOR2& value)
			{
				_inputPos = value;
			}

			const D3DXVECTOR2& UserChat::linesSize() const
			{
				return _linesSize;
			}

			void UserChat::linesSize(const D3DXVECTOR2& value)
			{
				_linesSize = value;
			}

			const D3DXVECTOR2& UserChat::inputSize() const
			{
				return _inputSize;
			}

			void UserChat::inputSize(const D3DXVECTOR2& value)
			{
				_inputSize = value;
			}
		}
	}
}
