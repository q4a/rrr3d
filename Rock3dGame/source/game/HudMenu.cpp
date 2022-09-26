#include "stdafx.h"

#include "game/Menu.h"
#include "game/HudMenu.h"

namespace r3d
{
	namespace game
	{
		PlayerStateFrame::PlayerStateFrame(Menu* menu, HudMenu* hudMenu, gui::Widget* parent): MenuFrame(menu, parent),
			_hudMenu(hudMenu)
		{
			const std::string imgTime[5] = {
				"GUI\\HUD\\tablo0.png", "GUI\\HUD\\tablo1.png", "GUI\\HUD\\tablo2.png", "GUI\\HUD\\tablo3.png",
				"GUI\\HUD\\tablo4.png"
			};

			Race* race = menu->GetRace();
			//piska
			//субъектом дл€ хада всегда будет человек за которым наблюдают.
			Player* subPlr = player();
			for (auto iter = race->GetPlayerList().begin(); iter != race->GetPlayerList().end(); ++iter)
			{
				if ((*iter)->isSubject())
					subPlr = *iter;
			}

			UpdateOpponents();

			for (int i = 0; i < cCarLifeEnd; ++i)
			{
				_carLifes[i].target = nullptr;
				_carLifes[i].timer = -1;
				_carLifes[i].timeMax = 4;

				if (HUD_STYLE == 1)
				{
					_carLifes[i].back = menu->CreatePlane(root(), nullptr, "GUI\\HUD\\carLifeBack.png", true,
					                                      IdentityVec2, gui::Material::bmTransparency);
					_carLifes[i].back->SetVisible(false);

					_carLifes[i].bar = menu->CreateBar(_carLifes[i].back, nullptr, "GUI\\HUD\\carLifeBar.png", "");
					_carLifes[i].bar->SetPos(_hudMenu->GetCarLifeBarPos());
				}
				else if (HUD_STYLE == 2 || HUD_STYLE == 3)
				{
					_carLifes[i].back = menu->CreatePlane(root(), nullptr, "GUI\\HUD\\carLifeBack.png", true,
					                                      IdentityVec2, gui::Material::bmTransparency);
					_carLifes[i].back->SetVisible(false);

					_carLifes[i].bar = menu->
						CreateBar(_carLifes[i].back, nullptr, "GUI\\HUD\\carLifeBarSilver.png", "");
					_carLifes[i].bar->SetPos(_hudMenu->GetCarLifeBarPos());
				}
				else
				{
					_carLifes[i].back = menu->CreatePlane(root(), nullptr, "GUI\\HUD\\carLifeBackX.png", true,
					                                      IdentityVec2, gui::Material::bmTransparency);
					_carLifes[i].back->SetVisible(false);

					_carLifes[i].bar = menu->CreateBar(_carLifes[i].back, nullptr, "GUI\\HUD\\carLifeBarX.png", "");
					_carLifes[i].bar->SetPos(_hudMenu->GetCarLifeBarXPos());
				}
			}

			//+2 +3 +4
			if (subPlr->GetSlotInst(Player::stWeapon2) && subPlr->GetSlotInst(Player::stWeapon3) && subPlr->GetSlotInst(
				Player::stWeapon4))
			{
				GUN2_OFFSET = 391.0f;
				GUN3_OFFSET = 503.0f;
				GUN4_OFFSET = 617.0f;
			}
			//+2 +3
			else if (subPlr->GetSlotInst(Player::stWeapon2) && subPlr->GetSlotInst(Player::stWeapon3) && subPlr->
				GetSlotInst(Player::stWeapon4) == nullptr)
			{
				GUN2_OFFSET = 391.0f;
				GUN3_OFFSET = 503.0f;
				GUN4_OFFSET = 617.0f;
			}
			//+2 +4
			else if (subPlr->GetSlotInst(Player::stWeapon2) && subPlr->GetSlotInst(Player::stWeapon4) && subPlr->
				GetSlotInst(Player::stWeapon3) == nullptr)
			{
				GUN2_OFFSET = 391.0f;
				GUN3_OFFSET = 503.0f;
				GUN4_OFFSET = 503.0f;
			}
			//+3 +4
			else if (subPlr->GetSlotInst(Player::stWeapon3) && subPlr->GetSlotInst(Player::stWeapon4) && subPlr->
				GetSlotInst(Player::stWeapon2) == nullptr)
			{
				GUN2_OFFSET = 391.0f;
				GUN3_OFFSET = 391.0f;
				GUN4_OFFSET = 503.0f;
			}
			//+2
			else if (subPlr->GetSlotInst(Player::stWeapon2) && subPlr->GetSlotInst(Player::stWeapon3) == nullptr &&
				subPlr->GetSlotInst(Player::stWeapon4) == nullptr)
			{
				GUN2_OFFSET = 391.0f;
				GUN3_OFFSET = 503.0f;
				GUN4_OFFSET = 617.0f;
			}
			//+3
			else if (subPlr->GetSlotInst(Player::stWeapon3) && subPlr->GetSlotInst(Player::stWeapon2) == nullptr &&
				subPlr->GetSlotInst(Player::stWeapon4) == nullptr)
			{
				GUN2_OFFSET = 391.0f;
				GUN3_OFFSET = 391.0f;
				GUN4_OFFSET = 617.0f;
			}
			//+4
			else if (subPlr->GetSlotInst(Player::stWeapon4) && subPlr->GetSlotInst(Player::stWeapon3) == nullptr &&
				subPlr->GetSlotInst(Player::stWeapon2) == nullptr)
			{
				GUN2_OFFSET = 391.0f;
				GUN3_OFFSET = 503.0f;
				GUN4_OFFSET = 391.0f;
			}
			else
			{
				GUN2_OFFSET = 391.0f;
				GUN3_OFFSET = 503.0f;
				GUN4_OFFSET = 617.0f;
			}

			if (MM_STYLE == 1)
			{
				MMAP_OFFSET_X = 160.0f;
				MMAP_OFFSET_Y = 0.0f;
				FIX_OFFSET = 15.0f;
			}
			else if (MM_STYLE == 2)
			{
				MMAP_OFFSET_X = 192.0f;
				MMAP_OFFSET_Y = 2.0f;
				FIX_OFFSET = 15.0f;
			}
			else if (MM_STYLE == 3)
			{
				MMAP_OFFSET_X = 224.0f;
				MMAP_OFFSET_Y = 5.0f;
				FIX_OFFSET = 10.0f;
			}
			else if (MM_STYLE == 4)
			{
				MMAP_OFFSET_X = 256.0f;
				MMAP_OFFSET_Y = 10.0f;
				FIX_OFFSET = 10.0f;
			}
			else if (MM_STYLE == 5)
			{
				MMAP_OFFSET_X = 288.0f;
				MMAP_OFFSET_Y = 15.0f;
				FIX_OFFSET = 7.0f;
			}
			else if (MM_STYLE == 6)
			{
				MMAP_OFFSET_X = 320.0f;
				MMAP_OFFSET_Y = 20.0f;
				FIX_OFFSET = 7.0f;
			}
			else if (MM_STYLE == 7)
			{
				MMAP_OFFSET_X = 352.0f;
				MMAP_OFFSET_Y = 25.0f;
				FIX_OFFSET = 5.0f;
			}
			else if (MM_STYLE == 8)
			{
				MMAP_OFFSET_X = 384.0f;
				MMAP_OFFSET_Y = 30.0f;
				FIX_OFFSET = 5.0f;
			}
			else if (MM_STYLE == 9)
			{
				MMAP_OFFSET_X = 416.0f;
				MMAP_OFFSET_Y = 35.0f;
				FIX_OFFSET = 5.0f;
			}
			else if (MM_STYLE == 10)
			{
				MMAP_OFFSET_X = 448.0f;
				MMAP_OFFSET_Y = 40.0f;
				FIX_OFFSET = 5.0f;
			}
			else if (MM_STYLE == 11)
			{
				MMAP_OFFSET_X = 480.0f;
				MMAP_OFFSET_Y = 45.0f;
				FIX_OFFSET = 5.0f;
			}
			else
			{
				MMAP_OFFSET_X = 0.0f;
				MMAP_OFFSET_Y = 0.0f;
				FIX_OFFSET = 200.0f;
			}

			if (HUD_STYLE == 1)
			{
				_raceState = menu->CreatePlane(root(), nullptr, "GUI\\HUD\\placeMineHyper.png", true, IdentityVec2,
				                               gui::Material::bmTransparency);
				_bankBgr = menu->CreatePlane(root(), nullptr, "GUI\\HUD\\MnBgr.png", true, IdentityVec2,
				                             gui::Material::bmTransparency);
				_classicLifePanel = menu->CreatePlane(root(), nullptr, "GUI\\HUD\\placeMineHyper.png", true,
				                                      IdentityVec2, gui::Material::bmTransparency);
				_lifeBack = menu->CreatePlane(_raceState, nullptr, "GUI\\HUD\\lifeBarBack.png", true, IdentityVec2,
				                              gui::Material::bmTransparency);
				_hyperBack = menu->CreatePlane(_raceState, nullptr, "GUI\\HUD\\AmmoBack.png", true, IdentityVec2,
				                               gui::Material::bmTransparency);
				_gun1Back = menu->CreatePlane(_raceState, nullptr, "GUI\\HUD\\AmmoBack.png", true, IdentityVec2,
				                              gui::Material::bmTransparency);
				_gun2Back = menu->CreatePlane(_raceState, nullptr, "GUI\\HUD\\AmmoBack.png", true, IdentityVec2,
				                              gui::Material::bmTransparency);
				_gun3Back = menu->CreatePlane(_raceState, nullptr, "GUI\\HUD\\AmmoBack.png", true, IdentityVec2,
				                              gui::Material::bmTransparency);
				_gun4Back = menu->CreatePlane(_raceState, nullptr, "GUI\\HUD\\AmmoBack.png", true, IdentityVec2,
				                              gui::Material::bmTransparency);
				_mineBack = menu->CreatePlane(_raceState, nullptr, "GUI\\HUD\\AmmoBack.png", true, IdentityVec2,
				                              gui::Material::bmTransparency);
				_hyperBar = menu->CreateBar(_hyperBack, nullptr, "GUI\\HUD\\AmmoBar.png", "");
				_lifeBar = menu->CreateBar(_lifeBack, nullptr, "GUI\\HUD\\lifeBar.png", "");
				_gun1Bar = menu->CreateBar(_gun1Back, nullptr, "GUI\\HUD\\AmmoBar.png", "");
				_gun2Bar = menu->CreateBar(_gun2Back, nullptr, "GUI\\HUD\\AmmoBar.png", "");
				_gun3Bar = menu->CreateBar(_gun3Back, nullptr, "GUI\\HUD\\AmmoBar.png", "");
				_gun4Bar = menu->CreateBar(_gun4Back, nullptr, "GUI\\HUD\\AmmoBar.png", "");
				_mineBar = menu->CreateBar(_mineBack, nullptr, "GUI\\HUD\\AmmoBar.png", "");
				_place = menu->CreateLabel(svNull, _raceState, "Header", NullVec2, gui::Text::haCenter,
				                           gui::Text::vaCenter, clrWhite);
				_laps = menu->CreateLabel(svNull, _raceState, "Header", NullVec2, gui::Text::haCenter,
				                          gui::Text::vaCenter, clrWhite);
				_bank = menu->CreateLabel(svNull, _raceState, "Header", NullVec2, gui::Text::haCenter,
				                          gui::Text::vaCenter, clrWhite);
				_hypercount = menu->CreateLabel(svNull, _raceState, "Header", NullVec2, gui::Text::haCenter,
				                                gui::Text::vaCenter, clrWhite);
				_minescount = menu->CreateLabel(svNull, _raceState, "Header", NullVec2, gui::Text::haCenter,
				                                gui::Text::vaCenter, clrWhite);
				_weaponscount = menu->CreateLabel(svNull, _raceState, "Header", NullVec2, gui::Text::haCenter,
				                                  gui::Text::vaCenter, clrWhite);
				_raceState->SetAlign(gui::Widget::waLeftTop);
				_classicLifePanel->SetAlign(gui::Widget::waCenter);
				_bankBgr->SetAlign(gui::Widget::waCenter);
				_hypercount->SetVisible(false);
				_minescount->SetVisible(false);
				_weaponscount->SetVisible(false);
				_laps->SetVisible(false);
				_bank->SetVisible(false);
				_hyperBar->SetVisible(false);
				_gun1Bar->SetVisible(false);
				_gun2Bar->SetVisible(false);
				_gun3Bar->SetVisible(false);
				_gun4Bar->SetVisible(false);
				_gun1Back->SetVisible(false);
				_gun2Back->SetVisible(false);
				_gun3Back->SetVisible(false);
				_gun4Back->SetVisible(false);
				_mineBar->SetVisible(false);
				_hyperBack->SetVisible(false);
				_mineBack->SetVisible(false);
				_bankBgr->SetVisible(false);
				LL_OFFSET = 0.0f;
			}
			else if (HUD_STYLE == 2)
			{
				_bankBgr = menu->CreatePlane(root(), nullptr, "GUI\\HUD\\MnBgr.png", true, IdentityVec2,
				                             gui::Material::bmTransparency);
				_raceState = menu->CreatePlane(root(), nullptr, "GUI\\HUD\\SilverPlaceMineHyper.png", true,
				                               IdentityVec2, gui::Material::bmTransparency);
				_classicLifePanel = menu->CreatePlane(root(), nullptr, "GUI\\HUD\\placeMineHyper.png", true,
				                                      IdentityVec2, gui::Material::bmTransparency);
				_lifeBack = menu->CreatePlane(_raceState, nullptr, "GUI\\HUD\\lifeBarBack.png", true, IdentityVec2,
				                              gui::Material::bmTransparency);
				_hyperBack = menu->CreatePlane(_raceState, nullptr, "GUI\\HUD\\AmmoBack.png", true, IdentityVec2,
				                               gui::Material::bmTransparency);
				_gun1Back = menu->CreatePlane(_raceState, nullptr, "GUI\\HUD\\AmmoBack.png", true, IdentityVec2,
				                              gui::Material::bmTransparency);
				_gun2Back = menu->CreatePlane(_raceState, nullptr, "GUI\\HUD\\AmmoBack.png", true, IdentityVec2,
				                              gui::Material::bmTransparency);
				_gun3Back = menu->CreatePlane(_raceState, nullptr, "GUI\\HUD\\AmmoBack.png", true, IdentityVec2,
				                              gui::Material::bmTransparency);
				_gun4Back = menu->CreatePlane(_raceState, nullptr, "GUI\\HUD\\AmmoBack.png", true, IdentityVec2,
				                              gui::Material::bmTransparency);
				_mineBack = menu->CreatePlane(_raceState, nullptr, "GUI\\HUD\\AmmoBack.png", true, IdentityVec2,
				                              gui::Material::bmTransparency);
				_lifeBar = menu->CreateBar(_lifeBack, nullptr, "GUI\\HUD\\lifeBarSilver.png", "");
				_hyperBar = menu->CreateBar(_hyperBack, nullptr, "GUI\\HUD\\AmmoBar.png", "");
				_gun1Bar = menu->CreateBar(_gun1Back, nullptr, "GUI\\HUD\\AmmoBar.png", "");
				_gun2Bar = menu->CreateBar(_gun2Back, nullptr, "GUI\\HUD\\AmmoBar.png", "");
				_gun3Bar = menu->CreateBar(_gun3Back, nullptr, "GUI\\HUD\\AmmoBar.png", "");
				_gun4Bar = menu->CreateBar(_gun4Back, nullptr, "GUI\\HUD\\AmmoBar.png", "");
				_mineBar = menu->CreateBar(_mineBack, nullptr, "GUI\\HUD\\AmmoBar.png", "");
				_place = menu->CreateLabel(svNull, _raceState, "Header", NullVec2, gui::Text::haCenter,
				                           gui::Text::vaCenter, clrWhite);
				_laps = menu->CreateLabel(svNull, _raceState, "Header", NullVec2, gui::Text::haCenter,
				                          gui::Text::vaCenter, clrWhite);
				_bank = menu->CreateLabel(svNull, _raceState, "Header", NullVec2, gui::Text::haCenter,
				                          gui::Text::vaCenter, clrWhite);
				_hypercount = menu->CreateLabel(svNull, _raceState, "Header", NullVec2, gui::Text::haCenter,
				                                gui::Text::vaCenter, clrWhite);
				_minescount = menu->CreateLabel(svNull, _raceState, "Header", NullVec2, gui::Text::haCenter,
				                                gui::Text::vaCenter, clrBlue);
				_weaponscount = menu->CreateLabel(svNull, _raceState, "Header", NullVec2, gui::Text::haCenter,
				                                  gui::Text::vaCenter, clrRed);
				_raceState->SetAlign(gui::Widget::waLeftTop);
				_classicLifePanel->SetAlign(gui::Widget::waCenter);
				_bankBgr->SetAlign(gui::Widget::waCenter);
				_hypercount->SetVisible(false);
				_minescount->SetVisible(false);
				_weaponscount->SetVisible(false);
				_laps->SetVisible(false);
				_bank->SetVisible(false);
				_hyperBar->SetVisible(false);
				_gun1Bar->SetVisible(false);
				_gun2Bar->SetVisible(false);
				_gun3Bar->SetVisible(false);
				_gun4Bar->SetVisible(false);
				_gun1Back->SetVisible(false);
				_gun2Back->SetVisible(false);
				_gun3Back->SetVisible(false);
				_gun4Back->SetVisible(false);
				_mineBar->SetVisible(false);
				_hyperBack->SetVisible(false);
				_mineBack->SetVisible(false);
				_bankBgr->SetVisible(false);
				LL_OFFSET = 0.0f;
			}
			else if (HUD_STYLE == 3)
			{
				VPSIZE_GET();
				_bankBgr = menu->CreatePlane(root(), nullptr, "GUI\\HUD\\MnBgr.png", true, IdentityVec2,
				                             gui::Material::bmTransparency);
				//PARENT:
				if (subPlr->GetSlotInst(Player::stHyper) && subPlr->GetSlotInst(Player::stHyper)->GetItem().GetName() ==
					"scSpring")
				{
					_raceState = menu->CreatePlane(root(), nullptr, "GUI\\HUD\\classicWeaponPanel2.png", true,
					                               IdentityVec2, gui::Material::bmTransparency);
				}
				else if (subPlr->GetSlotInst(Player::stHyper) && subPlr->GetSlotInst(Player::stHyper)->GetItem().
				                                                         GetName() == "scShell")
				{
					_raceState = menu->CreatePlane(root(), nullptr, "GUI\\HUD\\classicWeaponPanel3.png", true,
					                               IdentityVec2, gui::Material::bmTransparency);
				}
				else if (subPlr->GetSlotInst(Player::stHyper) && subPlr->GetSlotInst(Player::stHyper)->GetItem().
				                                                         GetName() == "scRipper")
				{
					_raceState = menu->CreatePlane(root(), nullptr, "GUI\\HUD\\classicWeaponPanel4.png", true,
					                               IdentityVec2, gui::Material::bmTransparency);
				}
				else
				{
					_raceState = menu->CreatePlane(root(), nullptr, "GUI\\HUD\\classicWeaponPanel.png", true,
					                               IdentityVec2, gui::Material::bmTransparency);
				}
				//LABEL:

				_place = menu->CreateLabel(svNull, _raceState, "Header", NullVec2, gui::Text::haCenter,
				                           gui::Text::vaCenter, clrWhite);
				_classicLifePanel = menu->CreatePlane(root(), nullptr, "GUI\\HUD\\AlphaBack.png", true, IdentityVec2,
				                                      gui::Material::bmTransparency);
				_hypercount = menu->CreateLabel(svNull, _raceState, "Arial Black40", NullVec2, gui::Text::haCenter,
				                                gui::Text::vaCenter, clrWhite);
				_minescount = menu->CreateLabel(svNull, _raceState, "Arial Black40", NullVec2, gui::Text::haCenter,
				                                gui::Text::vaCenter, clrWhite);
				_weaponscount = menu->CreateLabel(svNull, _raceState, "Arial Black40", NullVec2, gui::Text::haCenter,
				                                  gui::Text::vaCenter, clrWhite);
				_raceState->SetPos((X_VPSIZE / 2) - 12.0f, (Y_VPSIZE - Y_VPSIZE) + 70.0f + MMAP_OFFSET_Y);
				_classicLifePanel->SetPos(X_VPSIZE - 500.0f, Y_VPSIZE - 150.0f);
				//CHILD:
				_lifeBack = menu->CreatePlane(_classicLifePanel, nullptr, "GUI\\HUD\\lifeBarClassicBack.png", true,
				                              IdentityVec2, gui::Material::bmTransparency);
				_laps = menu->CreateLabel(svNull, _classicLifePanel, "Arial Black48", NullVec2, gui::Text::haCenter,
				                          gui::Text::vaCenter, clrWhite);
				_bank = menu->CreateLabel(svNull, _raceState, "Header", NullVec2, gui::Text::haCenter,
				                          gui::Text::vaCenter, clrWhite);
				_hyperBack = menu->CreatePlane(_raceState, nullptr, "GUI\\HUD\\AmmoBack.png", true, IdentityVec2,
				                               gui::Material::bmTransparency);
				_gun1Back = menu->CreatePlane(_raceState, nullptr, "GUI\\HUD\\AmmoBack.png", true, IdentityVec2,
				                              gui::Material::bmTransparency);
				_gun2Back = menu->CreatePlane(_raceState, nullptr, "GUI\\HUD\\AmmoBack.png", true, IdentityVec2,
				                              gui::Material::bmTransparency);
				_gun3Back = menu->CreatePlane(_raceState, nullptr, "GUI\\HUD\\AmmoBack.png", true, IdentityVec2,
				                              gui::Material::bmTransparency);
				_gun4Back = menu->CreatePlane(_raceState, nullptr, "GUI\\HUD\\AmmoBack.png", true, IdentityVec2,
				                              gui::Material::bmTransparency);
				_mineBack = menu->CreatePlane(_raceState, nullptr, "GUI\\HUD\\AmmoBack.png", true, IdentityVec2,
				                              gui::Material::bmTransparency);
				_lifeBar = menu->CreateBar(_lifeBack, nullptr, "GUI\\HUD\\lifeBarClassic.png", "");
				_hyperBar = menu->CreateBar(_hyperBack, nullptr, "GUI\\HUD\\AmmoBar.png", "");
				_gun1Bar = menu->CreateBar(_gun1Back, nullptr, "GUI\\HUD\\AmmoBar.png", "");
				_gun2Bar = menu->CreateBar(_gun2Back, nullptr, "GUI\\HUD\\AmmoBar.png", "");
				_gun3Bar = menu->CreateBar(_gun3Back, nullptr, "GUI\\HUD\\AmmoBar.png", "");
				_gun4Bar = menu->CreateBar(_gun4Back, nullptr, "GUI\\HUD\\AmmoBar.png", "");
				_mineBar = menu->CreateBar(_mineBack, nullptr, "GUI\\HUD\\AmmoBar.png", "");
				//FORMAT:
				_place->SetVisible(false);
				_raceState->SetAlign(gui::Widget::waCenter);
				_classicLifePanel->SetAlign(gui::Widget::waCenter);
				_bankBgr->SetAlign(gui::Widget::waCenter);
				_hyperBar->SetVisible(false);
				_gun1Bar->SetVisible(false);
				_gun2Bar->SetVisible(false);
				_gun3Bar->SetVisible(false);
				_gun4Bar->SetVisible(false);
				_gun1Back->SetVisible(false);
				_gun2Back->SetVisible(false);
				_gun3Back->SetVisible(false);
				_gun4Back->SetVisible(false);
				_mineBar->SetVisible(false);
				_hyperBack->SetVisible(false);
				_mineBack->SetVisible(false);
				_bankBgr->SetVisible(false);
				LL_OFFSET = 0.0f;
			}
			else
			{
				VPSIZE_GET();
				_bankBgr = menu->CreatePlane(root(), nullptr, "GUI\\HUD\\MnBgr.png", true, IdentityVec2,
				                             gui::Material::bmTransparency);
				_classicLifePanel = menu->CreatePlane(root(), nullptr, "GUI\\HUD\\AlphaBack.png", true, IdentityVec2,
				                                      gui::Material::bmTransparency);
				_raceState = menu->CreatePlane(root(), nullptr, "GUI\\HUD\\Device.png", true, IdentityVec2,
				                               gui::Material::bmTransparency);
				if (subPlr->GetSlotInst(Player::stWeapon1))
				{
					_hyperBack = menu->CreatePlane(_raceState, nullptr, "GUI\\HUD\\AmmoBack.png", true, IdentityVec2,
					                               gui::Material::bmTransparency);
					_gun1Back = menu->CreatePlane(_raceState, nullptr, "GUI\\HUD\\AmmoBack.png", true, IdentityVec2,
					                              gui::Material::bmTransparency);
					_gun2Back = menu->CreatePlane(_raceState, nullptr, "GUI\\HUD\\AmmoBack.png", true, IdentityVec2,
					                              gui::Material::bmTransparency);
					_gun3Back = menu->CreatePlane(_raceState, nullptr, "GUI\\HUD\\AmmoBack.png", true, IdentityVec2,
					                              gui::Material::bmTransparency);
					_gun4Back = menu->CreatePlane(_raceState, nullptr, "GUI\\HUD\\AmmoBack.png", true, IdentityVec2,
					                              gui::Material::bmTransparency);
					_mineBack = menu->CreatePlane(_raceState, nullptr, "GUI\\HUD\\AmmoBack.png", true, IdentityVec2,
					                              gui::Material::bmTransparency);
					_lifeBack = menu->CreatePlane(_raceState, nullptr, "GUI\\HUD\\lifeBarBack.png", true, IdentityVec2,
					                              gui::Material::bmTransparency);
					_lifeBar = menu->CreateBar(_lifeBack, nullptr, "GUI\\HUD\\lifeBar.png", "");
					_hyperBar = menu->CreateBar(_hyperBack, nullptr, "GUI\\HUD\\AmmoBar.png", "");
					_gun1Bar = menu->CreateBar(_gun1Back, nullptr, "GUI\\HUD\\AmmoBar.png", "");
					_gun2Bar = menu->CreateBar(_gun2Back, nullptr, "GUI\\HUD\\AmmoBar.png", "");
					_gun3Bar = menu->CreateBar(_gun3Back, nullptr, "GUI\\HUD\\AmmoBar.png", "");
					_gun4Bar = menu->CreateBar(_gun4Back, nullptr, "GUI\\HUD\\AmmoBar.png", "");
					_mineBar = menu->CreateBar(_mineBack, nullptr, "GUI\\HUD\\AmmoBar.png", "");
				}
				else
				{
					_hyperBack = menu->CreatePlane(_raceState, nullptr, "GUI\\HUD\\AlphaBack.png", true, IdentityVec2,
					                               gui::Material::bmTransparency);
					_gun1Back = menu->CreatePlane(_raceState, nullptr, "GUI\\HUD\\AlphaBack.png", true, IdentityVec2,
					                              gui::Material::bmTransparency);
					_gun2Back = menu->CreatePlane(_raceState, nullptr, "GUI\\HUD\\AlphaBack.png", true, IdentityVec2,
					                              gui::Material::bmTransparency);
					_gun3Back = menu->CreatePlane(_raceState, nullptr, "GUI\\HUD\\AlphaBack.png", true, IdentityVec2,
					                              gui::Material::bmTransparency);
					_gun4Back = menu->CreatePlane(_raceState, nullptr, "GUI\\HUD\\AlphaBack.png", true, IdentityVec2,
					                              gui::Material::bmTransparency);
					_mineBack = menu->CreatePlane(_raceState, nullptr, "GUI\\HUD\\AlphaBack.png", true, IdentityVec2,
					                              gui::Material::bmTransparency);
					_lifeBack = menu->CreatePlane(_raceState, nullptr, "GUI\\HUD\\AlphaBack.png", true, IdentityVec2,
					                              gui::Material::bmTransparency);
					_lifeBar = menu->CreateBar(_lifeBack, nullptr, "GUI\\HUD\\AlphaBack.png", "");
					_hyperBar = menu->CreateBar(_hyperBack, nullptr, "GUI\\HUD\\AlphaBack.png", "");
					_gun1Bar = menu->CreateBar(_gun1Back, nullptr, "GUI\\HUD\\AlphaBack.png", "");
					_gun2Bar = menu->CreateBar(_gun2Back, nullptr, "GUI\\HUD\\AlphaBack.png", "");
					_gun3Bar = menu->CreateBar(_gun3Back, nullptr, "GUI\\HUD\\AlphaBack.png", "");
					_gun4Bar = menu->CreateBar(_gun4Back, nullptr, "GUI\\HUD\\AlphaBack.png", "");
					_mineBar = menu->CreateBar(_mineBack, nullptr, "GUI\\HUD\\AlphaBack.png", "");
				}

				_place = menu->CreateLabel(svNull, _raceState, "ARIAL DEVICE", NullVec2, gui::Text::haCenter,
				                           gui::Text::vaCenter, D3DXCOLOR(0xFF03C0AD));
				_laps = menu->CreateLabel(svNull, _raceState, "Header", NullVec2, gui::Text::haCenter,
				                          gui::Text::vaCenter, clrWhite);
				_bank = menu->CreateLabel(svNull, _raceState, "Arial Black48", NullVec2, gui::Text::haCenter,
				                          gui::Text::vaCenter, D3DXCOLOR(0xFF03C0AD));
				_hypercount = menu->CreateLabel(svNull, _raceState, "Header", NullVec2, gui::Text::haCenter,
				                                gui::Text::vaCenter, clrWhite);
				_minescount = menu->CreateLabel(svNull, _raceState, "Header", NullVec2, gui::Text::haCenter,
				                                gui::Text::vaCenter, clrBlue);
				_weaponscount = menu->CreateLabel(svNull, _raceState, "Header", NullVec2, gui::Text::haCenter,
				                                  gui::Text::vaCenter, clrRed);
				_raceState->SetAlign(gui::Widget::waLeftTop);
				_classicLifePanel->SetAlign(gui::Widget::waCenter);
				_bankBgr->SetPos(X_VPSIZE - 100, Y_VPSIZE - 35);
				_bankBgr->SetAlign(gui::Widget::waCenter);
				_hypercount->SetVisible(false);
				_minescount->SetVisible(false);
				_weaponscount->SetVisible(false);
				_laps->SetVisible(false);
				_bank->SetVisible(true);
				_raceState->SetVisible(true);
				_lifeBack->SetVisible(false);
				_lifeBar->SetVisible(false);
				_bankBgr->SetVisible(HUD_STYLE == 4);
				LL_OFFSET = 190.0f;
			}

			for (int i = 0; i < 5; ++i)
			{
				_guiTimer[i] = menu->CreatePlane(root(), nullptr, imgTime[i], true, IdentityVec2 / 2.0f,
				                                 gui::Material::bmTransparency);
				_guiTimer[i]->SetFlag(gui::Widget::wfTopmost, true);
				_guiTimer[i]->SetVisible(false);
			}

			menu->ShowChat(true);
			menu->RegUser(this);
		}

		PlayerStateFrame::~PlayerStateFrame()
		{
			menu()->ShowChat(false);
			menu()->UnregUser(this);

			for (int i = 0; i < 5; ++i)
			{
				menu()->GetGUI()->ReleaseWidget(_guiTimer[i]);
			}

			for (int i = 0; i < cCarLifeEnd; ++i)
			{
				SafeRelease(_carLifes[i].target);
				menu()->GetGUI()->ReleaseWidget(_carLifes[i].bar);
				menu()->GetGUI()->ReleaseWidget(_carLifes[i].back);
			}

			for (auto iter = _pickItemsBuffer.begin(); iter != _pickItemsBuffer.end(); ++iter)
				menu()->ReleaseWidget(iter->image);
			_pickItemsBuffer.clear();

			for (auto iter = _pickItems.begin(); iter != _pickItems.end(); ++iter)
				menu()->ReleaseWidget(iter->image);
			_pickItems.clear();

			for (auto iter = _achievmentsBuffer.begin(); iter != _achievmentsBuffer.end(); ++iter)
				menu()->ReleaseWidget(iter->image);
			_achievmentsBuffer.clear();

			for (auto iter = _achievmentItems.begin(); iter != _achievmentItems.end(); ++iter)
				menu()->ReleaseWidget(iter->image);
			_achievmentItems.clear();


			ClearOpponents();
			ClearSlots();

			menu()->GetGUI()->ReleaseWidget(_lifeBar);
			menu()->GetGUI()->ReleaseWidget(_hyperBar);
			menu()->GetGUI()->ReleaseWidget(_gun1Bar);
			menu()->GetGUI()->ReleaseWidget(_gun2Bar);
			menu()->GetGUI()->ReleaseWidget(_gun3Bar);
			menu()->GetGUI()->ReleaseWidget(_gun4Bar);
			menu()->GetGUI()->ReleaseWidget(_mineBar);
			menu()->GetGUI()->ReleaseWidget(_lifeBack);
			menu()->GetGUI()->ReleaseWidget(_hyperBack);
			menu()->GetGUI()->ReleaseWidget(_gun1Back);
			menu()->GetGUI()->ReleaseWidget(_gun2Back);
			menu()->GetGUI()->ReleaseWidget(_gun3Back);
			menu()->GetGUI()->ReleaseWidget(_gun4Back);
			menu()->GetGUI()->ReleaseWidget(_mineBack);
			menu()->GetGUI()->ReleaseWidget(_place);
			menu()->GetGUI()->ReleaseWidget(_laps);
			menu()->GetGUI()->ReleaseWidget(_bank);
			menu()->GetGUI()->ReleaseWidget(_hypercount);
			menu()->GetGUI()->ReleaseWidget(_minescount);
			menu()->GetGUI()->ReleaseWidget(_weaponscount);
			menu()->GetGUI()->ReleaseWidget(_raceState);
			menu()->GetGUI()->ReleaseWidget(_bankBgr);
			menu()->GetGUI()->ReleaseWidget(_classicLifePanel);
		}

		void PlayerStateFrame::NewPickItem(Slot::Type slotType, GameObject::BonusType bonusType, int targetPlayerId,
		                                   bool kill)
		{
			const auto photoSize = D3DXVECTOR2(50.0f, 50.0f);
			const D3DXCOLOR color2 = D3DXCOLOR(214.0f, 214.0f, 214.0f, 255.0f) / 255.0f;

			string image;
			graph::Tex2DResource* photo = nullptr;
			string name;

			if (kill)
			{
				Player* target = menu()->GetRace()->GetPlayerById(targetPlayerId);
				if (target == nullptr) // || target == menu()->GetPlayer())
					return;

				if (HUD_STYLE != 3 && HUD_STYLE != 4)
				{
					photo = target->GetPhoto();

					image = "GUI\\HUD\\playerKill.png";
					name = GetString(target->GetName());
				}
				else if (HUD_STYLE == 4)
				{
					photo = target->GetPhoto();

					image = "GUI\\HUD\\Skull.png";
					name = GetString(target->GetName());
				}
				else
				{
					photo = target->GetPhoto();

					image = "GUI\\HUD\\playerKillClassic.png";
					name = GetString(target->GetName());
				}
			}
			else
				switch (bonusType)
				{
				case Player::btMedpack:
					if (HUD_STYLE != 4)
					{
						if (HUD_STYLE == 3)
						{
							image = "GUI\\HUD\\pickArmorClassic.png";
						}
						else
						{
							image = "GUI\\HUD\\pickArmor.png";
						}
						break;
					}

				case Player::btCharge:
					{
						if (HUD_STYLE != 4)
						{
							switch (slotType)
							{
							case Slot::stMine:
								if (HUD_STYLE == 3)
								{
									image = "GUI\\HUD\\pickMineClassic.png";
								}
								else
								{
									image = "GUI\\HUD\\pickMine.png";
								}
								break;
							case Slot::stHyper:
								if (HUD_STYLE == 3)
								{
									image = "GUI\\HUD\\pickNitroClassic.png";
								}
								else
								{
									image = "GUI\\HUD\\pickNitro.png";
								}
								break;
							default:
								if (HUD_STYLE == 3)
								{
									image = "GUI\\HUD\\pickWeaponClassic.png";
								}
								else
								{
									image = "GUI\\HUD\\pickWeapon.png";
								}
								break;
							}
							break;
						}
					}

				case Player::btMoney:
					if (HUD_STYLE != 4)
					{
						if (HUD_STYLE == 3)
						{
							image = "GUI\\HUD\\pickMoneyClassic.png";
						}
						else
						{
							image = "GUI\\HUD\\pickMoney.png";
						}
						break;
					}

				case Player::btImmortal:

					if (HUD_STYLE != 4)
					{
						if (HUD_STYLE == 3)
						{
							image = "GUI\\HUD\\pickImmortalClassic.png";
						}
						else
						{
							image = "GUI\\HUD\\pickImmortal.png";
						}
						break;
					}

				case Player::btRage:
					if (HUD_STYLE != 4)
					{
						if (HUD_STYLE == 3)
						{
							image = "GUI\\HUD\\inRage.png";
						}
						else
						{
							image = "GUI\\HUD\\inRage.png";
						}
						break;
					}

				case Player::btLucky:
					if (HUD_STYLE != 4)
					{
						image = "GUI\\HUD\\Lucky.png";
						break;
					}

				case Player::btUnLucky:
					if (HUD_STYLE != 4)
					{
						image = "GUI\\HUD\\UnLucky.png";
						break;
					}

				case Player::btWrong:
					if (HUD_STYLE == 3)
					{
						image = "GUI\\HUD\\WrongWay.png";
						break;
					}

				case Player::btLastLap:
					if (HUD_STYLE == 3)
					{
						image = "GUI\\HUD\\LastLap.png";
						break;
					}

				case Player::btEmpty:
					if (HUD_STYLE == 3)
					{
						image = "GUI\\HUD\\Empty.png";
						break;
					}

				default:
					return;
				}

			PickItem item;

			if (_pickItemsBuffer.size() > 0)
			{
				item = _pickItemsBuffer.back();
				item.image->GetMaterial().GetSampler().SetTex(menu()->GetTexture(image));
				item.image->SetSize(menu()->GetImageSize(item.image->GetMaterial()));

				_pickItemsBuffer.pop_back();
			}
			else
			{
				item.image = menu()->CreatePlane(root(), nullptr, image, true, IdentityVec2,
				                                 gui::Material::bmTransparency);

				item.photo = menu()->CreatePlane(item.image, nullptr, "", true, IdentityVec2,
				                                 gui::Material::bmTransparency);
				item.photo->GetMaterial().GetSampler().SetFiltering(graph::Sampler2d::sfLinear);
				item.photo->SetPos(D3DXVECTOR2(-40.0f, 0.0f));

				item.label = menu()->CreateLabel(svNull, item.photo, "Small", NullVec2, gui::Text::haLeft,
				                                 gui::Text::vaCenter, color2);
				item.label->SetAlign(gui::Widget::waLeft);
				item.label->SetPos(photoSize.x / 2 + 0.0f, 0.0f);
			}

			item.time = 0;

			item.image->GetMaterial().SetColor(D3DXCOLOR(1, 1, 1, 0));
			if (HUD_STYLE == 3)
			{
				item.pos = _hudMenu->GetClassicItemsPos();
			}
			else if (HUD_STYLE == 4)
			{
				item.pos = _hudMenu->GetAltItemsPos();
			}
			else
			{
				item.pos = _hudMenu->GetPickItemsPos() + D3DXVECTOR2(item.image->GetSize().x / 2, 0);
			}
			item.image->SetPos(item.pos);

			item.photo->SetVisible(photo ? true : false);
			item.photo->GetMaterial().GetSampler().SetTex(photo);
			item.photo->SetSize(menu()->StretchImage(item.photo->GetMaterial(), photoSize, true, false, true, false));

			item.label->SetText(name);

			_pickItems.push_front(item);

			if (HUD_STYLE == 0)
			{
				item.image->SetVisible(false);
				item.photo->SetVisible(false);
			}
			else if (HUD_STYLE == 3)
			{
				item.image->SetVisible(true);
				item.photo->SetVisible(false);
			}
			else if (HUD_STYLE == 4)
			{
				item.image->SetVisible(true);
				item.photo->SetVisible(false);
			}
			else
			{
				item.image->SetVisible(true);
				item.photo->SetVisible(true);
			}
		}

		void PlayerStateFrame::ProccessPickItems(float deltaTime)
		{
			if (HUD_STYLE == 3 && _pickItems.size() > 1)
			{
				for (auto iter = _pickItems.begin(); iter != _pickItems.end(); ++iter)
				{
					if (iter->image == _pickItems.front().image)
					{
						iter->image->SetVisible(true);
					}
					else
					{
						iter->image->SetVisible(false);
					}
				}
			}

			struct Pred
			{
				PlayerStateFrame* myThis;
				float deltaTime;
				int index;

				Pred(PlayerStateFrame* owner, float dt): myThis(owner), deltaTime(dt), index(0)
				{
				}

				bool operator()(PickItem& item)
				{
					if ((item.time += deltaTime) >= 5)
					{
						item.image->SetVisible(false);
						myThis->_pickItemsBuffer.push_back(item);
						return true;
					}

					float alpha = 1.0f;
					float dPosY = 0;

					float showPickItemDuration = 4.7f;
					D3DXVECTOR2 pos = item.pos + D3DXVECTOR2(30, index * 86.0f + dPosY);

					if (HUD_STYLE == 4)
					{
						showPickItemDuration = 7.0f;
						pos = item.pos + D3DXVECTOR2(index * 80.0f, 0.0f);
					}
					else if (HUD_STYLE == 3)
					{
						showPickItemDuration = 2.0f;
						pos = item.pos;
					}

					if (item.time < 0.3f)
					{
						float lerp = ClampValue(item.time / 0.3f, 0.0f, 1.0f);
						alpha = lerp;
					}
					else if (item.time > showPickItemDuration)
					{
						float lerp = ClampValue((item.time - showPickItemDuration) / 0.3f, 0.0f, 1.0f);
						dPosY = 30.0f;
						alpha = 1.0f - lerp;
					}

					if (HUD_STYLE != 3)
					{
						//анимаци€ pickItems дл€ ретро стил€ не нужна.
						D3DXVECTOR2 curPos = item.image->GetPos();
						if (abs(pos.x - curPos.x) > 0.001f || abs(pos.y - curPos.y) > 0.001f)
						{
							curPos.x = std::min(curPos.x + 90.0f * deltaTime, pos.x);
							curPos.y = std::min(curPos.y + 120.0f * deltaTime, pos.y);
							item.image->SetPos(curPos);
						}
					}

					item.image->GetMaterial().SetColor(D3DXCOLOR(1, 1, 1, alpha));
					item.photo->GetMaterial().SetColor(D3DXCOLOR(1, 1, 1, alpha));

					D3DXCOLOR color = item.label->GetMaterial().GetColor();
					color.a = alpha;
					item.label->GetMaterial().SetColor(color);

					++index;

					return false;
				}
			};
			_pickItems.RemoveIf(Pred(this, deltaTime));
		}

		void PlayerStateFrame::NewAchievment(AchievmentCondition::MyEventData* data)
		{
			bool createAch = true;
			if (data->condition->name() == "doubleKill")
			{
				//doubleKill не должен показыватьс€ дважды.
				if (_achievmentItems.size() > 0)
				{
					for (auto iter = _achievmentItems.begin(); iter != _achievmentItems.end(); ++iter)
					{
						if (iter->image->GetMaterial().GetSampler().GetTex() == menu()->
							GetTexture("GUI\\Achievments\\doubleKill.png", false) || iter->image->GetMaterial().
							GetSampler().GetTex() == menu()->GetTexture("GUI\\Achievments\\tripleKill.png", false) ||
							iter->image->GetMaterial().GetSampler().GetTex() == menu()->
							GetTexture("GUI\\Achievments\\megaKill.png", false) || iter->image->GetMaterial().
							GetSampler().GetTex() == menu()->GetTexture("GUI\\Achievments\\monsterKill.png", false))
						{
							createAch = false;
						}
					}
				}
			}

			if (data->condition->name() == "tripleKill")
			{
				//tripleKill ищет doubleKill чтобы зайн€ть его место.
				if (_achievmentItems.size() > 0)
				{
					for (auto iter = _achievmentItems.begin(); iter != _achievmentItems.end(); ++iter)
					{
						bool megakill = false;
						bool monsterkill = false;
						//фикс триплкилла после мегакила.
						if (iter->image->GetMaterial().GetSampler().GetTex() == menu()->GetTexture(
							"GUI\\Achievments\\megaKill.png", false))
						{
							createAch = false;
							megakill = true;
						}

						//фикс триплкилла после монстеркилла.
						if (iter->image->GetMaterial().GetSampler().GetTex() == menu()->GetTexture(
							"GUI\\Achievments\\monsterKill.png", false))
						{
							createAch = false;
							monsterkill = true;
						}

						if (iter->image->GetMaterial().GetSampler().GetTex() == menu()->
							GetTexture("GUI\\Achievments\\doubleKill.png", false) || iter->image->GetMaterial().
							GetSampler().GetTex() == menu()->GetTexture("GUI\\Achievments\\tripleKill.png", false))
						{
							if (megakill == false && monsterkill == false)
							{
								iter->image->GetMaterial().GetSampler().SetTex(
									menu()->GetTexture("GUI\\Achievments\\tripleKill.png", false));
								iter->time = 0;
								iter->indexTime = -1;
							}
							createAch = false;
						}
					}
				}
			}

			if (data->condition->name() == "megaKill")
			{
				//tripleKill ищет doubleKill чтобы зайн€ть его место.
				if (_achievmentItems.size() > 0)
				{
					for (auto iter = _achievmentItems.begin(); iter != _achievmentItems.end(); ++iter)
					{
						if (iter->image->GetMaterial().GetSampler().GetTex() == menu()->GetTexture(
							"GUI\\Achievments\\tripleKill.png", false))
						{
							iter->image->GetMaterial().GetSampler().SetTex(
								menu()->GetTexture("GUI\\Achievments\\megaKill.png", false));
							iter->time = 0;
							iter->indexTime = -1;
							createAch = false;
						}

						if (iter->image->GetMaterial().GetSampler().GetTex() == menu()->
							GetTexture("GUI\\Achievments\\doubleKill.png", false) || iter->image->GetMaterial().
							GetSampler().GetTex() == menu()->GetTexture("GUI\\Achievments\\megaKill.png", false) || iter
							->image->GetMaterial().GetSampler().GetTex() == menu()->GetTexture(
								"GUI\\Achievments\\monsterKill.png", false))
						{
							createAch = false;
						}
					}
				}
			}

			if (data->condition->name() == "monsterKill")
			{
				//tripleKill ищет doubleKill чтобы зайн€ть его место.
				if (_achievmentItems.size() > 0)
				{
					for (auto iter = _achievmentItems.begin(); iter != _achievmentItems.end(); ++iter)
					{
						if (iter->image->GetMaterial().GetSampler().GetTex() == menu()->GetTexture(
							"GUI\\Achievments\\megaKill.png", false))
						{
							iter->image->GetMaterial().GetSampler().SetTex(
								menu()->GetTexture("GUI\\Achievments\\monsterKill.png", false));
							iter->time = 0;
							iter->indexTime = -1;
							createAch = false;
						}

						if (iter->image->GetMaterial().GetSampler().GetTex() == menu()->
							GetTexture("GUI\\Achievments\\doubleKill.png", false) || iter->image->GetMaterial().
							GetSampler().GetTex() == menu()->GetTexture("GUI\\Achievments\\tripleKill.png", false) ||
							iter->image->GetMaterial().GetSampler().GetTex() == menu()->
							GetTexture("GUI\\Achievments\\megaKill.png", false) || iter->image->GetMaterial().
							GetSampler().GetTex() == menu()->GetTexture("GUI\\Achievments\\monsterKill.png", false))
						{
							createAch = false;
						}
					}
				}
			}

			if (createAch == true)
			{
				Difficulty diff = menu()->GetRace()->GetProfile()->difficulty();
				std::string image = "GUI\\Achievments\\" + data->condition->name() + ".png";
				std::string pointsImage = StrFmt("GUI\\Achievments\\points%d.png", data->condition->reward());
				D3DXVECTOR2 vpSize = menu()->GetGUI()->GetVPSize();

				AchievmentItem item;

				if (_achievmentsBuffer.size() > 0)
				{
					item = _achievmentsBuffer.back();

					item.image->GetMaterial().GetSampler().SetTex(menu()->GetTexture(image, false));
					item.image->SetSize(menu()->GetImageSize(item.image->GetMaterial()));

					item.points->GetMaterial().GetSampler().SetTex(menu()->GetTexture(pointsImage, false));
					item.points->SetSize(menu()->GetImageSize(item.points->GetMaterial()));

					_achievmentsBuffer.pop_back();
				}
				else
				{
					item.image = menu()->CreatePlane(root(), nullptr, image, true, IdentityVec2,
					                                 gui::Material::bmTransparency);

					item.points = menu()->CreatePlane(item.image, nullptr, pointsImage, true, IdentityVec2,
					                                  gui::Material::bmTransparency);
					item.points->SetPos(D3DXVECTOR2(0.0f, item.image->GetSize().y / 2 + 15.0f));
					item.slotSize = D3DXVECTOR2(std::max(item.image->GetSize().x, item.points->GetSize().x),
					                            item.image->GetSize().y + item.points->GetSize().y + 15.0f);
				}

				const D3DXVECTOR2 startPos[8] = {
					D3DXVECTOR2(0 - item.slotSize.x * 2, 1 * vpSize.y / 4),
					D3DXVECTOR2(0 - item.slotSize.x, 2 * vpSize.y / 4),
					D3DXVECTOR2(0 - item.slotSize.x, 3 * vpSize.y / 4),
					D3DXVECTOR2(0, vpSize.y + item.slotSize.y),

					D3DXVECTOR2(vpSize.x + item.slotSize.y * 2, 1 * vpSize.y / 4),
					D3DXVECTOR2(vpSize.x + item.slotSize.y, 2 * vpSize.y / 4),
					D3DXVECTOR2(vpSize.x + item.slotSize.y, 3 * vpSize.y / 4),
					D3DXVECTOR2(vpSize.x, vpSize.y + item.slotSize.y)
				};

				item.time = 0;
				item.lastIndex = _achievmentItems.size();
				item.indexTime = -1.0f;


				if (HUD_STYLE != 0)
				{
					item.image->SetVisible(true);
				}
				else
				{
					item.image->SetVisible(false);
				}

				item.image->GetMaterial().SetColor(D3DXCOLOR(1, 1, 1, 1));
				item.points->GetMaterial().SetColor(D3DXCOLOR(1, 1, 1, 0));
				item.points->SetVisible(menu()->IsCampaign() && HUD_STYLE >= 1 && HUD_STYLE < 3);

				item.image->SetPos(startPos[RandomRange(0, 7)]);
				_achievmentItems.push_front(item);
			}
		}

		void PlayerStateFrame::VPSIZE_GET()
		{
			D3DXVECTOR2 vpSize = menu()->GetGUI()->GetVPSize();
			X_VPSIZE = vpSize.x;
			Y_VPSIZE = vpSize.y;
		}

		void PlayerStateFrame::ProccessAchievments(float deltaTime)
		{
			struct Pred
			{
				PlayerStateFrame* myThis;
				float deltaTime;
				int index;

				Pred(PlayerStateFrame* owner, float dt): myThis(owner), deltaTime(dt), index(0)
				{
				}

				bool operator()(AchievmentItem& item)
				{
					item.time += deltaTime;

					float fIndex = static_cast<float>(index);
					if (item.indexTime == -1.0f && index != item.lastIndex)
						item.indexTime = 0.0f;

					if (item.indexTime != -1.0f)
					{
						item.indexTime += deltaTime;
						float alpha = ClampValue(item.indexTime / 0.15f, 0.0f, 1.0f);
						fIndex = item.lastIndex + (index - item.lastIndex) * alpha;
						if (alpha == 1.0f)
						{
							fIndex = static_cast<float>(item.lastIndex = index);
							item.indexTime = -1.0f;
						}
					}
					D3DXVECTOR2 pos = myThis->_hudMenu->GetAchievmentItemsPos();

					pos.y += fIndex * item.slotSize.y + item.image->GetSize().y / 2;

					float flyAlpha = ClampValue(item.time / 0.3f, 0.0f, 1.0f);
					float outAlpha = ClampValue((item.time - 4.5f) / 0.3f, 0.0f, 1.0f);
					float pointsAlpha = ClampValue((item.time - 0.8f) / 0.15f, 0.0f, 1.0f);
					float pingAlpha = ClampValue((item.time - 0.2f) / 0.1f, 0.0f, 1.0f) - ClampValue(
						(item.time - 0.3f) / 0.1f, 0.0f, 1.0f);
					//outAlpha = ClampValue((item.time - 5.0f)/0.3f, 0.0f, 1.0f);

					D3DXVec2Lerp(&pos, &item.image->GetPos(), &pos, flyAlpha);
					item.image->SetPos(pos);

					D3DXVECTOR2 imgSize = myThis->menu()->GetImageSize(item.image->GetMaterial());
					D3DXVec2Lerp(&imgSize, &imgSize, &(2.0f * imgSize), pingAlpha);
					item.image->SetSize(imgSize);

					item.image->GetMaterial().SetAlpha(1.0f - outAlpha);
					item.points->GetMaterial().SetAlpha(pointsAlpha - outAlpha);

					if (outAlpha == 1.0f)
					{
						item.image->SetVisible(false);
						myThis->_achievmentsBuffer.push_back(item);
						return true;
					}

					++index;

					return false;
				}
			};

			_achievmentItems.RemoveIf(Pred(this, deltaTime));
		}

		void PlayerStateFrame::ShowCarLifeBar(CarLifeE type, int targetPlayerId, float carLifeTimeMax)
		{
			Player* targetPlayer = menu()->GetRace()->GetPlayerById(targetPlayerId);

			if (targetPlayer == nullptr)
				return;

			float barAlpha = _carLifes[type].bar->GetFront().GetColor().a;
			if (barAlpha > 0.99f)
				_carLifes[type].bar->GetFront().SetColor(D3DXCOLOR(1, 1, 1, 0));
			else
				_carLifes[type].bar->GetFront().SetColor(D3DXCOLOR(1, 1, 1, std::min(barAlpha, 0.5f)));

			if (ReplaceRef(_carLifes[type].target, targetPlayer))
			{
				_carLifes[type].target = targetPlayer;
				_carLifes[type].back->GetMaterial().SetColor(D3DXCOLOR(1, 1, 1, 0));
			}

			_carLifes[type].timer = 0;
			_carLifes[type].timeMax = carLifeTimeMax;
			if (HUD_STYLE != 0 && menu()->GetPlayer() != nullptr && menu()->GetPlayer()->GetFinished() == false)
			{
				_carLifes[type].back->SetVisible(true);
			}
			else
			{
				_carLifes[type].back->SetVisible(false);
			}
		}

		inline float StepLerp(float v1, float v2, float step)
		{
			if (v2 > v1)
				return std::min(v1 + step, v2);
			return std::max(v1 - step, v2);
		}

		void PlayerStateFrame::ProccessCarLifeBar(float deltaTime)
		{
			for (int i = 0; i < cCarLifeEnd; ++i)
			{
				if (_carLifes[i].timer < 0 || _carLifes[i].target == nullptr)
					continue;

				float targetAlpha = 1.0f;

				if ((_carLifes[i].timer += deltaTime) > _carLifes[i].timeMax || _carLifes[i].target->GetCar().gameObj ==
					nullptr)
				{
					targetAlpha = _carLifes[i].back->GetMaterial().GetColor().a;
					if (_carLifes[i].target->GetCar().gameObj && targetAlpha > 0)
						targetAlpha = 0;
					else
					{
						_carLifes[i].timer = -1;
						SafeRelease(_carLifes[i].target);
						_carLifes[i].back->SetVisible(false);
						return;
					}
				}

				float value = 1.0f;
				float maxValue = _carLifes[i].target->GetCar().gameObj->GetMaxLife();
				if (maxValue > 0)
					value = _carLifes[i].target->GetCar().gameObj->GetLife() / maxValue;
				_carLifes[i].bar->SetProgress(value);
				//фикс, чтобы бар не показывалс€ скраю экрана
				if (HUD_STYLE > 0)
				{
					if (_carLifes[i].back->GetPos().x < 75 || _carLifes[i].back->GetPos().x > (menu()->GetGUI()->
						GetVPSize().x - 110) || _carLifes[i].back->GetPos().y < 30 || _carLifes[i].back->GetPos().y > (
						menu()->GetGUI()->GetVPSize().y - 30))
					{
						_carLifes[i].bar->SetVisible(false);
						_carLifes[i].back->SetVisible(false);
					}
					else
					{
						_carLifes[i].bar->SetVisible(true);
						_carLifes[i].back->SetVisible(true);
					}
				}
				else
				{
					_carLifes[i].bar->SetVisible(false);
					_carLifes[i].back->SetVisible(false);
				}

				D3DXVECTOR3 pos = _carLifes[i].target->GetCar().gameObj->GetPos() + D3DXVECTOR3(0.0f, 0.0f, 0.0f);
				D3DXVECTOR4 projVec;
				D3DXVec3Transform(&projVec, &pos, &menu()->GetGUI()->GetCamera3d()->GetContextInfo().GetViewProj());
				D3DXVECTOR2 vec = projVec / projVec.w;

				if (projVec.z < 0)
				{
					D3DXVec2Normalize(&vec, &vec);
					vec = vec * sqrt(2.0f);
				}
				vec.x = ClampValue(vec.x, -1.0f, 1.0f);
				vec.y = ClampValue(vec.y, -1.0f, 1.0f);

				if ((abs(vec.x) == 1.0f || abs(vec.y) == 1.0f))
					targetAlpha = 0;

				D3DXVECTOR2 vpSize = menu()->GetGUI()->GetVPSize();
				vec = graph::CameraCI::ProjToView(vec, vpSize);
				vec.x = ClampValue(vec.x, 0.0f, vpSize.x - _carLifes[i].back->GetSize().x);
				vec.y = ClampValue(vec.y, _carLifes[i].back->GetSize().y, vpSize.y);

				if (HUD_STYLE != 4)
				{
					_carLifes[i].back->SetPos(
						vec + D3DXVECTOR2(_carLifes[i].back->GetSize().x / 2, -_carLifes[i].back->GetSize().y / 2));
				}
				else
				{
					_carLifes[i].back->SetPos(vec + D3DXVECTOR2(
						_carLifes[i].back->GetSize().x - _carLifes[i].back->GetSize().x,
						-_carLifes[i].back->GetSize().y * 2));
				}

				float alpha = _carLifes[i].back->GetMaterial().GetColor().a;
				_carLifes[i].back->GetMaterial().SetColor(
					D3DXCOLOR(1, 1, 1, StepLerp(alpha, targetAlpha, deltaTime / 0.3f)));
				alpha = _carLifes[i].bar->GetFront().GetColor().a;

				if (HUD_STYLE == 4)
				{
					if (value >= 0.4f)
						_carLifes[i].bar->GetFront().SetColor(clrGreen);
					else
					{
						if (value < 0.1f)
							_carLifes[i].bar->GetFront().SetColor(clrRed);
						else
							_carLifes[i].bar->GetFront().SetColor(clrYellow);
					}
				}
				else if (HUD_STYLE == 3)
				{
					if (value >= 0.8f)
						_carLifes[i].bar->GetFront().SetColor(clrGreen);
					else
					{
						if (value > 0.6f)
							_carLifes[i].bar->GetFront().SetColor(clrAcid);
						else
						{
							if (value > 0.25)
								_carLifes[i].bar->GetFront().SetColor(clrOrange);
							else
								_carLifes[i].bar->GetFront().SetColor(clrRed);
						}
					}
				}
				else
				{
					_carLifes[i].bar->GetFront().SetColor(
						D3DXCOLOR(1, 1, 1, StepLerp(alpha, targetAlpha, deltaTime / 0.3f)));
				}
			}
		}

		const PlayerStateFrame::CarLife* PlayerStateFrame::GetCarLife(Player* target)
		{
			for (int i = 0; i < cCarLifeEnd; ++i)
				if (_carLifes[i].target == target)
					return &_carLifes[i];

			return nullptr;
		}

		void PlayerStateFrame::InsertSlot(WeaponType type, Slot* slot)
		{
			if (HUD_STYLE != 3)
			{
				ClearSlot(type);

				LSL_ASSERT(slot);

				_weaponBox[type].slot = slot;
				_weaponBox[type].slot->AddRef();

				_weaponBox[type].box = nullptr;

				if (HUD_STYLE == 4)
				{
					_weaponBox[type].box = menu()->CreatePlane(_raceState, nullptr, "GUI\\HUD\\zlot.png", true,
					                                           IdentityVec2, gui::Material::bmTransparency);
				}
				else
				{
					if (type != wtMine && type != wtHyper)
					{
						_weaponBox[type].box = menu()->CreatePlane(_raceState, nullptr, "GUI\\HUD\\slot.png", true,
						                                           IdentityVec2, gui::Material::bmTransparency);
					}
				}

				gui::Widget* slotParent = _weaponBox[type].box
					                          ? static_cast<gui::Widget*>(_weaponBox[type].box)
					                          : _raceState;

				_weaponBox[type].view = menu()->CreateViewPort3d(slotParent, nullptr, "");
				if (HUD_STYLE == 4)
				{
					_weaponBox[type].view = menu()->CreateViewPort3d(slotParent, nullptr, "",
					                                                 gui::ViewPort3d::msSlowAnim);
				}
				else
				{
					_weaponBox[type].view = menu()->CreateViewPort3d(slotParent, nullptr, "");
				}

				float sizeX = 60;
				if (HUD_STYLE == 4)
				{
					sizeX = 75;
				}
				else
				{
					sizeX = 60;
				}
				if (type != wtMine && type != wtHyper)
				{
					_weaponBox[type].view->SetSize(D3DXVECTOR2(sizeX, sizeX));
				}
				else
				{
					if (HUD_STYLE == 4)
					{
						_weaponBox[type].view->SetSize(D3DXVECTOR2(sizeX - 15.0f, sizeX - 15.0f));
					}
					else
					{
						_weaponBox[type].view->SetSize(D3DXVECTOR2(sizeX, sizeX));
					}
				}
				_weaponBox[type].mesh = menu()->CreateMesh3d(_weaponBox[type].view, slot->GetItem().GetMesh(),
				                                             slot->GetItem().GetTexture());
				_weaponBox[type].mesh->AddRef();
				_weaponBox[type].mesh->GetMaterial()->GetSampler().SetFiltering(graph::Sampler2d::sfAnisotropic);

				if (HUD_STYLE != 4)
				{
					_weaponBox[type].label = menu()->CreateLabel(svNull, slotParent, "Small", NullVec2,
					                                             gui::Text::haCenter, gui::Text::vaCenter, clrWhite);
				}
				else
				{
					_weaponBox[type].label = menu()->CreateLabel(svNull, slotParent, "SmallPlus", NullVec2,
					                                             gui::Text::haCenter, gui::Text::vaCenter, clrWhite);
				}
				_weaponBox[type].label->SetText("0/0");
				_weaponBox[type].label->SetSize(D3DXVECTOR2(30.0f, 30.0f));
			}
		}

		void PlayerStateFrame::ClearSlot(WeaponType type)
		{
			if (_weaponBox[type].slot && HUD_STYLE != 3)
			{
				menu()->GetGUI()->ReleaseWidget(_weaponBox[type].label);
				_weaponBox[type].label = nullptr;

				_weaponBox[type].mesh->Release();
				_weaponBox[type].mesh = nullptr;

				menu()->GetGUI()->ReleaseWidget(_weaponBox[type].view);
				_weaponBox[type].view = nullptr;

				if (_weaponBox[type].box)
				{
					menu()->GetGUI()->ReleaseWidget(_weaponBox[type].box);
					_weaponBox[type].box = nullptr;
				}

				_weaponBox[type].slot->Release();
				_weaponBox[type].slot = nullptr;
			}
		}

		void PlayerStateFrame::ClearSlots()
		{
			if (HUD_STYLE != 3)
			{
				for (int i = 0; i < cWeaponTypeEnd; ++i)
					ClearSlot(static_cast<WeaponType>(i));
			}
		}

		void PlayerStateFrame::UpdateSlots()
		{
			Player* sPlr = player();
			for (auto iter = player()->GetRace()->GetPlayerList().begin(); iter != player()->GetRace()->GetPlayerList().
			     end(); ++iter)
			{
				if ((*iter)->isSubject())
					sPlr = *iter;
			}

			if (HUD_STYLE != 3)
			{
				ClearSlots();
				//—крываем слот с кратером из хада:
				if (sPlr->GetSlotInst(Player::stWeapon4) && sPlr->GetSlotInst(Player::stWeapon4)->GetItem().GetName() ==
					"scCrater")
				{
					for (int i = Player::stHyper; i <= Player::stWeapon3; ++i)
					{
						Slot* slot = sPlr->GetSlotInst(static_cast<Player::SlotType>(i));
						if (slot && slot->GetItem().IsMobilityItem() == nullptr)
							InsertSlot(static_cast<WeaponType>(i - Player::stHyper), slot);
					}
				}
				else
				{
					for (int i = Player::stHyper; i <= Player::stWeapon4; ++i)
					{
						Slot* slot = sPlr->GetSlotInst(static_cast<Player::SlotType>(i));
						if (slot && slot->GetItem().IsMobilityItem() == nullptr)
							InsertSlot(static_cast<WeaponType>(i - Player::stHyper), slot);
					}
				}
			}
		}

		void PlayerStateFrame::UpdateOpponents()
		{
			Race* race = menu()->GetRace();

			ClearOpponents();

			for (auto iter = race->GetPlayerList().begin(); iter != race->GetPlayerList().end(); ++iter)
			{
				if (*iter == player())
					continue;

				Opponent opponent;
				opponent.player = *iter;
				opponent.player->AddRef();

				opponent.dummy = menu()->GetGUI()->CreateDummy();
				opponent.dummy->SetParent(root());

				opponent.point = menu()->CreatePlane(opponent.dummy, nullptr, "GUI\\HUD\\playerPoint.png", true,
				                                     IdentityVec2, gui::Material::bmTransparency);
				opponent.point->SetPos(opponent.point->GetSize().x / 2, -opponent.point->GetSize().y / 2);

				opponent.label = menu()->CreateLabel(svNull, opponent.dummy, "VerySmall", NullVec2, gui::Text::haCenter,
				                                     gui::Text::vaCenter, clrWhite);
				opponent.label->SetPos(opponent.point->GetSize().x / 2, -opponent.point->GetSize().y);


				_opponents.push_back(opponent);
			}
		}

		void PlayerStateFrame::RemoveOpponent(Opponents::const_iterator iter)
		{
			menu()->GetGUI()->ReleaseWidget(iter->label);
			menu()->GetGUI()->ReleaseWidget(iter->point);
			menu()->GetGUI()->ReleaseWidget(iter->dummy);

			iter->player->Release();
			_opponents.erase(iter);
		}

		void PlayerStateFrame::RemoveOpponent(Player* player)
		{
			for (Opponents::const_iterator iter = _opponents.begin(); iter != _opponents.end(); ++iter)
				if (iter->player == player)
				{
					RemoveOpponent(iter);
					break;
				}
		}

		void PlayerStateFrame::ClearOpponents()
		{
			while (_opponents.size() > 0)
				RemoveOpponent(_opponents.begin());
		}

		void PlayerStateFrame::UpdateState(float deltaTime)
		{
			Player* sPlr = player();
			for (auto iter = player()->GetRace()->GetPlayerList().begin(); iter != player()->GetRace()->GetPlayerList().
			     end(); ++iter)
			{
				if ((*iter)->isSubject())
					sPlr = *iter;
			}
			if (HUD_STYLE != 3)
			{
				for (int i = Player::stHyper; i <= Player::stWeapon4; ++i)
				{
					Slot* slot = sPlr->GetSlotInst(static_cast<Player::SlotType>(i));
					auto type = static_cast<WeaponType>(i - Player::stHyper);
					if (slot && _weaponBox[type].slot)
					{
						WeaponItem& weapon = slot->GetItem<WeaponItem>();
						std::stringstream sstream;
						sstream << weapon.GetCurCharge() << "/" << weapon.GetCntCharge();
						_weaponBox[type].label->SetText(sstream.str());

						//HUD STYLE ALTERNATIVE:

						Slot* slotHyper = sPlr->GetSlotInst(Player::stHyper);
						WeaponItem& hyperjump = slotHyper->GetItem<WeaponItem>();
						float hyperstatus = 1.0f;
						int maxAmmoHyper = hyperjump.GetCntCharge();
						int curAmmoHyper = hyperjump.GetCurCharge();

						if (curAmmoHyper != 0)
						{
							hyperstatus = (curAmmoHyper * 10 / maxAmmoHyper) * 0.1f;
						}
						else
						{
							hyperstatus = 0.0f;
						}

						Slot* slotMine = sPlr->GetSlotInst(Player::stMine);
						WeaponItem& Mine = slotMine->GetItem<WeaponItem>();
						float minestatus = 1.0f;
						int maxAmmoMine = Mine.GetCntCharge();
						int curAmmoMine = Mine.GetCurCharge();

						if (curAmmoMine != 0)
						{
							minestatus = (curAmmoMine * 10 / maxAmmoMine) * 0.1f;
						}
						else
						{
							minestatus = 0.0f;
						}

						float gun1status = 1.0f;
						if (sPlr->GetSlotInst(Player::stWeapon1))
						{
							Slot* slotGun1 = sPlr->GetSlotInst(Player::stWeapon1);
							WeaponItem& Gun1 = slotGun1->GetItem<WeaponItem>();

							int maxAmmoGun1 = Gun1.GetCntCharge();
							int curAmmoGun1 = Gun1.GetCurCharge();

							if (curAmmoGun1 != 0)
							{
								gun1status = (curAmmoGun1 * 10 / maxAmmoGun1) * 0.1f;
							}
							else
							{
								gun1status = 0.0f;
							}
							_gun1Back->SetVisible(true);
							_gun1Bar->SetVisible(true);
							_gun1Bar->SetProgress(gun1status);

							if (gun1status >= 0.5f)
								_gun1Bar->GetFront().SetColor(clrGreen);
							else
							{
								if (gun1status < 0.2f)
									_gun1Bar->GetFront().SetColor(clrRed);
								else
									_gun1Bar->GetFront().SetColor(clrYellow);
							}
						}
						else
						{
							_gun1Back->SetVisible(false);
							_gun1Bar->SetVisible(false);
							_gun1Bar->SetProgress(gun1status);
						}

						if (sPlr->GetSlotInst(Player::stWeapon2))
						{
							Slot* slotGun2 = sPlr->GetSlotInst(Player::stWeapon2);
							WeaponItem& Gun2 = slotGun2->GetItem<WeaponItem>();
							float gun2status = 1.0f;
							int maxAmmoGun2 = Gun2.GetCntCharge();
							int curAmmoGun2 = Gun2.GetCurCharge();

							if (curAmmoGun2 != 0)
							{
								gun2status = (curAmmoGun2 * 10 / maxAmmoGun2) * 0.1f;
							}
							else
							{
								gun2status = 0.0f;
							}
							_gun2Back->SetVisible(true);
							_gun2Bar->SetVisible(true);
							_gun2Bar->SetProgress(gun2status);

							if (gun2status >= 0.5f)
								_gun2Bar->GetFront().SetColor(clrGreen);
							else
							{
								if (gun2status < 0.2f)
									_gun2Bar->GetFront().SetColor(clrRed);
								else
									_gun2Bar->GetFront().SetColor(clrYellow);
							}
						}
						else
						{
							_gun2Back->SetVisible(false);
							_gun2Bar->SetVisible(false);
							_gun2Bar->SetProgress(gun1status);
						}

						if (sPlr->GetSlotInst(Player::stWeapon3))
						{
							Slot* slotGun3 = sPlr->GetSlotInst(Player::stWeapon3);
							WeaponItem& Gun3 = slotGun3->GetItem<WeaponItem>();
							float gun3status = 1.0f;
							int maxAmmoGun3 = Gun3.GetCntCharge();
							int curAmmoGun3 = Gun3.GetCurCharge();

							if (curAmmoGun3 != 0)
							{
								gun3status = (curAmmoGun3 * 10 / maxAmmoGun3) * 0.1f;
							}
							else
							{
								gun3status = 0.0f;
							}
							_gun3Back->SetVisible(true);
							_gun3Bar->SetVisible(true);
							_gun3Bar->SetProgress(gun3status);

							if (gun3status >= 0.5f)
								_gun3Bar->GetFront().SetColor(clrGreen);
							else
							{
								if (gun3status < 0.2f)
									_gun3Bar->GetFront().SetColor(clrRed);
								else
									_gun3Bar->GetFront().SetColor(clrYellow);
							}
						}
						else
						{
							_gun3Bar->SetProgress(gun1status);
							_gun3Back->SetVisible(false);
							_gun3Bar->SetVisible(false);
						}

						if (sPlr->GetSlotInst(Player::stWeapon4) && sPlr->GetSlotInst(Player::stWeapon4)->GetItem().
						                                                  GetName() != "scCrater")
						{
							Slot* slotGun4 = sPlr->GetSlotInst(Player::stWeapon4);
							WeaponItem& Gun4 = slotGun4->GetItem<WeaponItem>();
							float gun4status = 1.0f;
							int maxAmmoGun4 = Gun4.GetCntCharge();
							int curAmmoGun4 = Gun4.GetCurCharge();

							if (curAmmoGun4 != 0)
							{
								gun4status = (curAmmoGun4 * 10 / maxAmmoGun4) * 0.1f;
							}
							else
							{
								gun4status = 0.0f;
							}
							_gun4Back->SetVisible(true);
							_gun4Bar->SetVisible(true);
							_gun4Bar->SetProgress(gun4status);

							if (gun4status >= 0.5f)
								_gun4Bar->GetFront().SetColor(clrGreen);
							else
							{
								if (gun4status < 0.2f)
									_gun4Bar->GetFront().SetColor(clrRed);
								else
									_gun4Bar->GetFront().SetColor(clrYellow);
							}
						}
						else
						{
							_gun4Bar->SetProgress(gun1status);
							_gun4Back->SetVisible(false);
							_gun4Bar->SetVisible(false);
						}

						_hyperBar->SetProgress(hyperstatus);
						_mineBar->SetProgress(minestatus);

						if (minestatus >= 0.5f)
							_mineBar->GetFront().SetColor(clrGreen);
						else
						{
							if (minestatus < 0.2f)
								_mineBar->GetFront().SetColor(clrRed);
							else
								_mineBar->GetFront().SetColor(clrYellow);
						}

						if (hyperstatus >= 0.5f)
							_hyperBar->GetFront().SetColor(clrGreen);
						else
						{
							if (hyperstatus < 0.2f)
								_hyperBar->GetFront().SetColor(clrRed);
							else
								_hyperBar->GetFront().SetColor(clrYellow);
						}

						if (HUD_STYLE != 4)
						{
							_hyperBar->SetVisible(false);
							_mineBar->SetVisible(false);
							_gun1Bar->SetVisible(false);
							_gun1Back->SetVisible(false);
							_hyperBack->SetVisible(false);
							_mineBack->SetVisible(false);
							_gun2Back->SetVisible(false);
							_gun3Back->SetVisible(false);
							_gun4Back->SetVisible(false);
						}
						else
						{
							_hyperBar->SetVisible(true);
							_mineBar->SetVisible(true);
							_gun1Bar->SetVisible(true);
							_gun1Back->SetVisible(true);
							_hyperBack->SetVisible(true);
							_mineBack->SetVisible(true);
						}

						if (_weaponBox[type].box && HUD_STYLE == 1)
						{
							bool sel = (i - Player::stWeapon1) == menu()->GetRace()->GetHuman()->GetCurWeapon();
							_weaponBox[type].box->GetMaterial().GetSampler().SetTex(
								menu()->GetTexture(sel ? "GUI\\HUD\\slotSel.png" : "GUI\\HUD\\slot.png"));
						}
						else if (_weaponBox[type].box && HUD_STYLE == 2)
						{
							bool sel = (i - Player::stWeapon1) == menu()->GetRace()->GetHuman()->GetCurWeapon();
							_weaponBox[type].box->GetMaterial().GetSampler().SetTex(
								menu()->GetTexture(sel ? "GUI\\HUD\\SilverSlotSel.png" : "GUI\\HUD\\SilverSlot.png"));
						}
						else if (_weaponBox[type].box && HUD_STYLE == 4)
						{
							bool sel = (i - Player::stWeapon1) == menu()->GetRace()->GetHuman()->GetCurWeapon();
							_weaponBox[type].box->GetMaterial().GetSampler().SetTex(
								menu()->GetTexture(sel ? "GUI\\HUD\\zlotSel.png" : "GUI\\HUD\\zlot.png"));
						}
					}
				}
			}
			else
			{
				//Ёлементы HUD дл€ классического стил€.		
				if (sPlr->GetSlotInst(Player::stHyper))
				{
					//Ќитро и прыжки:
					Slot* slotHyper = sPlr->GetSlotInst(Player::stHyper);
					WeaponItem& hyperjump = slotHyper->GetItem<WeaponItem>();
					std::stringstream sstreamHyper;
					sstreamHyper << hyperjump.GetCurCharge();
					_hypercount->SetText(sstreamHyper.str());
				}
				else
				{
					_hypercount->SetVisible(false);
				}

				if (sPlr->GetSlotInst(Player::stMine))
				{
					//ћины и масло:
					Slot* slotMiner = sPlr->GetSlotInst(Player::stMine);
					WeaponItem& mines = slotMiner->GetItem<WeaponItem>();
					std::stringstream sstreamMiner;
					sstreamMiner << mines.GetCurCharge();
					_minescount->SetText(sstreamMiner.str());
				}
				else
				{
					_minescount->SetVisible(false);
				}

				//¬се слоты оружи€ вместе:
				/*for (int i = Player::stWeapon1; i <= Player::stWeapon4; ++i)
				{
					Slot* stGuns = player()->GetSlotInst(Player::SlotType(i));
					WeaponType type = WeaponType(i);
					if (stGuns)
					{
						WeaponItem& Guns = stGuns->GetItem<WeaponItem>();
						std::stringstream sstreamGuns;
						sstreamGuns << Guns.GetCurCharge();
						_weaponscount->SetText(sstreamGuns.str());
					}
				}	*/
				//—уммируем патроны из всех слотов оружи€, если слоты не пустые:

				if (sPlr->GetSlotInst(Player::stWeapon1))
				{
					int AmmoFix = 0;
					Slot* slotGun1 = sPlr->GetSlotInst(Player::stWeapon1);
					WeaponItem& weapon1 = slotGun1->GetItem<WeaponItem>();
					//+2 +3 +4
					if (sPlr->GetSlotInst(Player::stWeapon2) && sPlr->GetSlotInst(Player::stWeapon3) && sPlr->
						GetSlotInst(Player::stWeapon4))
					{
						Slot* slotGun2 = sPlr->GetSlotInst(Player::stWeapon2);
						Slot* slotGun3 = sPlr->GetSlotInst(Player::stWeapon3);
						Slot* slotGun4 = sPlr->GetSlotInst(Player::stWeapon4);

						WeaponItem& weapon2 = slotGun2->GetItem<WeaponItem>();
						WeaponItem& weapon3 = slotGun3->GetItem<WeaponItem>();
						WeaponItem& weapon4 = slotGun4->GetItem<WeaponItem>();

						if (weapon2.GetName() == "scDroid" || weapon2.GetName() == "scReflector")
							AmmoFix += 1;

						if (weapon3.GetName() == "scDroid" || weapon3.GetName() == "scReflector")
							AmmoFix += 1;

						if (weapon4.GetName() == "scDroid" || weapon4.GetName() == "scReflector")
							AmmoFix += 1;

						std::stringstream sstreamGuns;
						sstreamGuns << (weapon1.GetCurCharge() + weapon2.GetCurCharge() + weapon3.GetCurCharge() +
							weapon4.GetCurCharge() - AmmoFix);
						_weaponscount->SetText(sstreamGuns.str());
					}
					//+2 +3
					else if (sPlr->GetSlotInst(Player::stWeapon2) && sPlr->GetSlotInst(Player::stWeapon3) && sPlr->
						GetSlotInst(Player::stWeapon4) == nullptr)
					{
						Slot* slotGun2 = sPlr->GetSlotInst(Player::stWeapon2);
						Slot* slotGun3 = sPlr->GetSlotInst(Player::stWeapon3);

						WeaponItem& weapon2 = slotGun2->GetItem<WeaponItem>();
						WeaponItem& weapon3 = slotGun3->GetItem<WeaponItem>();

						if (weapon2.GetName() == "scDroid" || weapon2.GetName() == "scReflector")
							AmmoFix += 1;

						if (weapon3.GetName() == "scDroid" || weapon3.GetName() == "scReflector")
							AmmoFix += 1;

						std::stringstream sstreamGuns;
						sstreamGuns << (weapon1.GetCurCharge() + weapon2.GetCurCharge() + weapon3.GetCurCharge() -
							AmmoFix);
						_weaponscount->SetText(sstreamGuns.str());
					}
					//+2 +4
					else if (sPlr->GetSlotInst(Player::stWeapon2) && sPlr->GetSlotInst(Player::stWeapon4) && sPlr->
						GetSlotInst(Player::stWeapon3) == nullptr)
					{
						Slot* slotGun2 = sPlr->GetSlotInst(Player::stWeapon2);
						Slot* slotGun4 = sPlr->GetSlotInst(Player::stWeapon4);

						WeaponItem& weapon2 = slotGun2->GetItem<WeaponItem>();
						WeaponItem& weapon4 = slotGun4->GetItem<WeaponItem>();

						if (weapon2.GetName() == "scDroid" || weapon2.GetName() == "scReflector")
							AmmoFix += 1;

						if (weapon4.GetName() == "scDroid" || weapon4.GetName() == "scReflector")
							AmmoFix += 1;

						std::stringstream sstreamGuns;
						sstreamGuns << (weapon1.GetCurCharge() + weapon2.GetCurCharge() + weapon4.GetCurCharge() -
							AmmoFix);
						_weaponscount->SetText(sstreamGuns.str());
					}
					//+3 +4
					else if (sPlr->GetSlotInst(Player::stWeapon3) && sPlr->GetSlotInst(Player::stWeapon4) && sPlr->
						GetSlotInst(Player::stWeapon2) == nullptr)
					{
						Slot* slotGun3 = sPlr->GetSlotInst(Player::stWeapon3);
						Slot* slotGun4 = sPlr->GetSlotInst(Player::stWeapon4);

						WeaponItem& weapon3 = slotGun3->GetItem<WeaponItem>();
						WeaponItem& weapon4 = slotGun4->GetItem<WeaponItem>();

						if (weapon3.GetName() == "scDroid" || weapon3.GetName() == "scReflector")
							AmmoFix += 1;

						if (weapon4.GetName() == "scDroid" || weapon4.GetName() == "scReflector")
							AmmoFix += 1;

						std::stringstream sstreamGuns;
						sstreamGuns << (weapon1.GetCurCharge() + weapon3.GetCurCharge() + weapon4.GetCurCharge() -
							AmmoFix);
						_weaponscount->SetText(sstreamGuns.str());
					}
					//+2
					else if (sPlr->GetSlotInst(Player::stWeapon2) && sPlr->GetSlotInst(Player::stWeapon3) == nullptr &&
						sPlr->GetSlotInst(Player::stWeapon4) == nullptr)
					{
						Slot* slotGun2 = sPlr->GetSlotInst(Player::stWeapon2);

						WeaponItem& weapon2 = slotGun2->GetItem<WeaponItem>();

						if (weapon2.GetName() == "scDroid" || weapon2.GetName() == "scReflector")
							AmmoFix += 1;

						std::stringstream sstreamGuns;
						sstreamGuns << (weapon1.GetCurCharge() + weapon2.GetCurCharge() - AmmoFix);
						_weaponscount->SetText(sstreamGuns.str());
					}
					//+3
					else if (sPlr->GetSlotInst(Player::stWeapon3) && sPlr->GetSlotInst(Player::stWeapon2) == nullptr &&
						sPlr->GetSlotInst(Player::stWeapon4) == nullptr)
					{
						Slot* slotGun3 = sPlr->GetSlotInst(Player::stWeapon3);

						WeaponItem& weapon3 = slotGun3->GetItem<WeaponItem>();

						if (weapon3.GetName() == "scDroid" || weapon3.GetName() == "scReflector")
							AmmoFix += 1;

						std::stringstream sstreamGuns;
						sstreamGuns << (weapon1.GetCurCharge() + weapon3.GetCurCharge() - AmmoFix);
						_weaponscount->SetText(sstreamGuns.str());
					}
					//+4
					else if (sPlr->GetSlotInst(Player::stWeapon4) && sPlr->GetSlotInst(Player::stWeapon3) == nullptr &&
						sPlr->GetSlotInst(Player::stWeapon2) == nullptr)
					{
						Slot* slotGun4 = sPlr->GetSlotInst(Player::stWeapon4);

						WeaponItem& weapon4 = slotGun4->GetItem<WeaponItem>();

						if (weapon4.GetName() == "scDroid" || weapon4.GetName() == "scReflector")
							AmmoFix += 1;

						std::stringstream sstreamGuns;
						sstreamGuns << (weapon1.GetCurCharge() + weapon4.GetCurCharge() - AmmoFix);
						_weaponscount->SetText(sstreamGuns.str());
					}
					else if (sPlr->GetSlotInst(Player::stWeapon4) == nullptr && sPlr->GetSlotInst(Player::stWeapon3) ==
						nullptr && sPlr->GetSlotInst(Player::stWeapon2) == nullptr)
					{
						std::stringstream sstreamGuns;
						sstreamGuns << weapon1.GetCurCharge();
						_weaponscount->SetText(sstreamGuns.str());
					}
					else
					{
						_weaponscount->SetVisible(false);
					}
				}
				else
				{
					_weaponscount->SetVisible(false);
				}
			}

			int place = sPlr->GetPlace();
			int cntPlayers = menu()->GetRace()->GetPlayerList().size();

			int maxHP = static_cast<int>(sPlr->GetCar().gameObj->GetMaxLife());
			int curHP = static_cast<int>(sPlr->GetCar().gameObj->GetLife() * 100);
			int LifeStatus = curHP / maxHP;

			if (sPlr->GetCar().gameObj)
			{
				maxHP = static_cast<int>(sPlr->GetCar().gameObj->GetMaxLife());
				curHP = static_cast<int>(sPlr->GetCar().gameObj->GetLife() * 100);
				LifeStatus = curHP / maxHP;

				_lifeBar->SetVisible(true);
				if (HUD_STYLE == 3)
				{
					if (LifeStatus > 80)
					{
						_lifeBar->GetFront().SetColor(clrGreen);
					}
					else
					{
						if (LifeStatus > 60)
							_lifeBar->GetFront().SetColor(clrAcid);
						else
						{
							if (LifeStatus > 25)
								_lifeBar->GetFront().SetColor(clrOrange);
							else
								_lifeBar->GetFront().SetColor(clrRed);
						}
					}
				}
			}
			else
			{
				LifeStatus = 0;
				_lifeBar->SetVisible(false);
			}

			if (HUD_STYLE != 4)
			{
				_place->SetText(GetString(StrFmt("svPlace%d", place)));
				_laps->SetText(GetString(StrFmt("%d", lapsRest)));
				_bank->SetText(GetString(StrFmt("%d$", lapsRest)));
			}
			else
			{
				//попеременный показ Ќ–/позиции в гонке:
				if (sPlr->GetFinished())
					_place->SetText(GetString(StrFmt("svPlace%d", place)));
				else
				{
					if (sPlr->GetRaceSeconds() > 30)
					{
						if (sPlr->GetRaceSeconds() > 45)
						{
							if (sPlr->GetRaceSeconds() > 52)
								_place->SetText(GetString(StrFmt("svPlace%d", place)));

							else
								_place->SetText(GetString(StrFmt("%d HP", LifeStatus)));
						}
						else
						{
							if (sPlr->GetRaceSeconds() > 37)
								_place->SetText(GetString(StrFmt("svPlace%d", place)));

							else
								_place->SetText(GetString(StrFmt("%d HP", LifeStatus)));
						}
					}
					else
					{
						if (sPlr->GetRaceSeconds() > 15)
						{
							if (sPlr->GetRaceSeconds() > 22)
								_place->SetText(GetString(StrFmt("svPlace%d", place)));

							else
								_place->SetText(GetString(StrFmt("%d HP", LifeStatus)));
						}
						else
						{
							if (sPlr->GetRaceSeconds() > 7)
								_place->SetText(GetString(StrFmt("svPlace%d", place)));

							else
								_place->SetText(GetString(StrFmt("%d HP", LifeStatus)));
						}
					}
				}
				_laps->SetText(GetString(StrFmt("%d", lapsRest)));
				if (TOTALPLAYERS_COUNT > 1)
				{
					if (lapsRest > 1)
					{
						_bank->SetText(GetString(StrFmt("LAPS: %d", lapsRest)));
					}
					else if (lapsRest == 1)
					{
						_bank->SetText(GetString(StrFmt("LAST LAP")));
					}
					else if (lapsRest == 0)
					{
						_bank->SetText(GetString(StrFmt("FINISHED")));
					}
				}
				else
				{
					if (sPlr->GetRaceMinutes() < 10)
					{
						if (sPlr->GetRaceSeconds() < 10)
							_bank->SetText(
								"0" + menu()->FormatCurrency(sPlr->GetRaceMinutes()) + ": 0" + menu()->
								FormatCurrency(static_cast<int>(sPlr->GetRaceSeconds())) + "." + menu()->FormatCurrency(
									static_cast<int>(sPlr->GetRaceMSeconds())));
						else
							_bank->SetText(
								"0" + menu()->FormatCurrency(sPlr->GetRaceMinutes()) + ": " + menu()->
								FormatCurrency(static_cast<int>(sPlr->GetRaceSeconds())) + "." + menu()->FormatCurrency(
									static_cast<int>(sPlr->GetRaceMSeconds())));
					}
					else
					{
						if (sPlr->GetRaceSeconds() < 10)
							_bank->SetText(
								menu()->FormatCurrency(sPlr->GetRaceMinutes()) + ": 0" + menu()->
								FormatCurrency(static_cast<int>(sPlr->GetRaceSeconds())) + "." + menu()->FormatCurrency(
									static_cast<int>(sPlr->GetRaceMSeconds())));
						else
							_bank->SetText(
								menu()->FormatCurrency(sPlr->GetRaceMinutes()) + ": " + menu()->
								FormatCurrency(static_cast<int>(sPlr->GetRaceSeconds())) + "." + menu()->FormatCurrency(
									static_cast<int>(sPlr->GetRaceMSeconds())));
					}
				}
			}

			if (sPlr->GetCar().gameObj)
			{
				float value = 1;
				float maxValue = sPlr->GetCar().gameObj->GetMaxLife();
				if (maxValue > 0)
					value = sPlr->GetCar().gameObj->GetLife() / maxValue;

				_lifeBar->SetProgress(value);
			}

			struct Pred
			{
				PlayerStateFrame* myThis;
				Player* player;

				Pred(PlayerStateFrame* owner, Player* player)
				{
					myThis = owner;
					this->player = player;
				}

				bool operator()(const Opponent& op1, const Opponent& op2)
				{
					return op1.player->GetPlace() > op2.player->GetPlace();
				}
			};

			_opponents.sort(Pred(this, player()));

			for (int i = 0; i < cCarLifeEnd; ++i)
			{
				if (_carLifes[i].target)
					for (Opponents::const_iterator iter = _opponents.begin(); iter != _opponents.end(); ++iter)
					{
						if (_carLifes[i].target == iter->player)
						{
							Opponent opponent = *iter;
							_opponents.erase(iter);
							_opponents.insert(_opponents.begin(), opponent);
							break;
						}
					}
			}

			for (auto iter = _opponents.begin(); iter != _opponents.end(); ++iter)
			{
				Opponent& opponent = *iter;
				opponent.dummy->SetVisible(opponent.player->GetCar().gameObj != nullptr);

				if (opponent.player->GetCar().gameObj)
				{
					const CarLife* carLife = GetCarLife(opponent.player);

					AABB2 aabb = opponent.label->GetTextAABB();

					D3DXVECTOR3 pos = opponent.player->GetCar().gameObj->GetWorldPos() + D3DXVECTOR3(1.0f, -0.5f, 0);
					D3DXVECTOR4 projVec;
					D3DXVec3Transform(&projVec, &pos, &menu()->GetGUI()->GetCamera3d()->GetContextInfo().GetViewProj());
					D3DXVECTOR2 vec = projVec / projVec.w;

					if (projVec.z < 0)
					{
						D3DXVec2Normalize(&vec, &vec);
						vec = vec * sqrt(2.0f);
					}
					vec.x = ClampValue(vec.x, -1.0f, 1.0f);
					vec.y = ClampValue(vec.y, -1.0f, 1.0f);

					float targetAlpha = 1.0f;
					if ((abs(vec.x) == 1.0f || abs(vec.y) == 1.0f) || carLife != nullptr)
						targetAlpha = std::max(opponent.point->GetMaterial().GetColor().a - 4.0f * deltaTime, 0.0f);

					D3DXVECTOR2 vpSize = menu()->GetGUI()->GetVPSize();
					vec = graph::CameraCI::ProjToView(vec, vpSize);
					vec.x = ClampValue(vec.x, 0.0f, vpSize.x - opponent.point->GetSize().x);
					vec.y = ClampValue(vec.y, -opponent.label->GetPos().y - aabb.min.y, vpSize.y);

					opponent.dummy->SetPos(vec);
					int oponentPlace = opponent.player->GetPlace();


					if (HUD_STYLE == 3)
					{
						//opponent.label->SetText(GetString(lsl::StrFmt("%d", oponentPlace)));
						opponent.label->SetVisible(false);
						opponent.point->SetVisible(false);
					}
					else if (HUD_STYLE == 0)
					{
						opponent.label->SetVisible(false);
						opponent.point->SetVisible(false);
					}
					else
					{
						opponent.label->SetText(StrFmt(GetString("svNamePlaceMarker").c_str(),
						                               opponent.player->GetPlace(),
						                               menu()->GetString(opponent.player->GetName()).c_str()));
						opponent.label->SetVisible(true);
						opponent.point->SetVisible(true);
					}
					opponent.center = opponent.label->GetWorldPos();
					if (carLife != nullptr)
						opponent.radius = std::max(carLife->back->GetSize().x, carLife->back->GetSize().y);
					else
						opponent.radius = std::max(aabb.GetSize().x, aabb.GetSize().y);

					float alpha = 1;
					for (auto iter2 = _opponents.begin(); iter2 != iter; ++iter2)
					{
						float rad = opponent.radius + iter2->radius;
						float dist = D3DXVec2Length(&(iter2->center - opponent.center));
						alpha = std::min(rad != 0 ? dist / rad : 0, alpha);
					}

					alpha = std::min(alpha, targetAlpha);

					opponent.point->GetMaterial().SetColor(D3DXCOLOR(1, 1, 1, alpha));
					opponent.label->GetMaterial().SetColor(D3DXCOLOR(1, 1, 1, alpha));
				}
			}
		}

		void PlayerStateFrame::OnAdjustLayout(const D3DXVECTOR2& vpSize)
		{
			//_raceState->SetPos(_raceState->GetSize().x/2, _raceState->GetSize().y/2);

			D3DXVECTOR2 subWeaponPos[2] = {_hudMenu->GetWeaponPosHyper(), _hudMenu->GetWeaponPosMine()};
			D3DXVECTOR2 subWeaponLabelPos[2] = {_hudMenu->GetWeaponPosHyperLabel(), _hudMenu->GetWeaponPosMineLabel()};
			if (HUD_STYLE == 4)
			{
				VPSIZE_GET();
				LOCAL_YPOS_MINEITEM = 100.0f;
				LOCAL_YPOS_HYPERITEM = 100.0f;
				LOCAL_YPOS_WEAPONITEM = 60.0f;
				LOCAL_YPOS_WPN_BOX_ITEM = 0.0f;

				LOCAL_XPOS_MINEITEM = X_VPSIZE - 100.0f;
				LOCAL_XPOS_HYPERITEM = X_VPSIZE - 164.0f;
				LOCAL_XPOS_WEAPONITEM = 220.0f;
				LOCAL_XPOS_WPN_BOX_ITEM = 0.0f;
			}
			else
			{
				VPSIZE_GET();
				LOCAL_YPOS_MINEITEM = 140.0f;
				LOCAL_YPOS_HYPERITEM = 32.0f;
				LOCAL_YPOS_WEAPONITEM = 50.0f;
				LOCAL_YPOS_WPN_BOX_ITEM = -15.0f;

				LOCAL_XPOS_MINEITEM = 30.0f;
				LOCAL_XPOS_HYPERITEM = 30.0f;
				LOCAL_XPOS_WEAPONITEM = 155.0f;
				LOCAL_XPOS_WPN_BOX_ITEM = 5.0f;
			}

			int ind = 0;
			if (HUD_STYLE != 3 && HUD_STYLE != 4)
			{
				for (int i = wtHyper; i <= wtMine; ++i)
				{
					if (_weaponBox[i].slot)
					{
						_weaponBox[i].view->SetPos(subWeaponPos[ind]);
						_weaponBox[i].label->SetPos(subWeaponLabelPos[ind]);
						++ind;
					}
				}
			}

			ind = 0;
			if (HUD_STYLE != 3)
			{
				if (HUD_STYLE == 4)
				{
					for (int i = wtHyper; i <= wtWeapon4; ++i)
					{
						if (_weaponBox[i].slot)
						{
							D3DXVECTOR2 size = _weaponBox[i].box->GetSize();
							_weaponBox[i].box->SetPos(
								_hudMenu->GetWeaponPos() + D3DXVECTOR2(size.x / 2 + ind * (size.x + 20), size.y / 2));
							_weaponBox[i].view->SetPos(_hudMenu->GetWeaponBoxPos());
							_weaponBox[i].label->SetPos(_hudMenu->GetWeaponLabelAltPos());
							++ind;
						}
					}
				}
				else
				{
					for (int i = wtWeapon1; i <= wtWeapon4; ++i)
					{
						if (_weaponBox[i].slot)
						{
							D3DXVECTOR2 size = _weaponBox[i].box->GetSize();
							_weaponBox[i].box->SetPos(
								_hudMenu->GetWeaponPos() + D3DXVECTOR2(size.x / 2 + ind * (size.x - 25), size.y / 2));
							_weaponBox[i].view->SetPos(_hudMenu->GetWeaponBoxPos());
							_weaponBox[i].label->SetPos(_hudMenu->GetWeaponLabelPos());
							++ind;
						}
					}
				}
			}

			_lifeBack->SetPos(_hudMenu->GetLifeBarPos() + _lifeBack->GetSize() / 2);
			_hyperBack->SetPos(LOCAL_XPOS_WEAPONITEM + 47.0f, LOCAL_YPOS_WEAPONITEM + 104.0f);
			_mineBack->SetPos(LOCAL_XPOS_WEAPONITEM + 161.0f, LOCAL_YPOS_WEAPONITEM + 104.0f);
			_gun1Back->SetPos(LOCAL_XPOS_WEAPONITEM + 275.0f, LOCAL_YPOS_WEAPONITEM + 104.0f);
			_gun2Back->SetPos(LOCAL_XPOS_WEAPONITEM + GUN2_OFFSET, LOCAL_YPOS_WEAPONITEM + 104.0f);
			_gun3Back->SetPos(LOCAL_XPOS_WEAPONITEM + GUN3_OFFSET, LOCAL_YPOS_WEAPONITEM + 104.0f);
			_gun4Back->SetPos(LOCAL_XPOS_WEAPONITEM + GUN4_OFFSET, LOCAL_YPOS_WEAPONITEM + 104.0f);

			if (HUD_STYLE != 4)
			{
				_place->SetPos(_hudMenu->GetPlacePos());
			}
			else
			{
				_place->SetPos(115.0f, 112.0f);
			}
			_laps->SetPos(_hudMenu->GetClassicLapsPos());
			_bank->SetPos(vpSize.x - 95, vpSize.y - 27);
			_hypercount->SetPos(_hudMenu->GetHyperCountLabelClassic());
			_minescount->SetPos(_hudMenu->GetMinesCountLabelClassic());
			_weaponscount->SetPos(_hudMenu->GetAllWeaponCountLabelClassic());

			menu()->GetUserChat()->inputPos(D3DXVECTOR2(300.0f, vpSize.y - 10.0f));
			menu()->GetUserChat()->inputSize(D3DXVECTOR2(vpSize.x - 600.0f, 300.0f));

			menu()->GetUserChat()->linesPos(D3DXVECTOR2(vpSize.x - 10.0f, _hudMenu->GetMiniMapRectI().GetSize().y));
			menu()->GetUserChat()->linesSize(D3DXVECTOR2(vpSize.x / 3,
			                                             vpSize.y - _hudMenu->GetMiniMapRectE().GetSize().y));
		}

		void PlayerStateFrame::OnInvalidate()
		{
			UpdateSlots();

			_raceState->SetVisible(HUD_STYLE != 0);
			_classicLifePanel->SetVisible(HUD_STYLE == 3);
		}

		void PlayerStateFrame::OnProgress(float deltaTime)
		{
			UpdateState(deltaTime);
			ProccessPickItems(deltaTime);
			ProccessCarLifeBar(deltaTime);
			ProccessAchievments(deltaTime);

			if (_guiTimer[4]->GetVisible())
			{
				const D3DXVECTOR2 speedSize(200.0f, 200.0f);
				float alpha = _guiTimer[4]->GetMaterial().GetAlpha() - deltaTime / 1.5f;

				if (alpha > 0)
				{
					_guiTimer[4]->GetMaterial().SetAlpha(alpha);
					_guiTimer[4]->SetSize(_guiTimer[4]->GetSize() + speedSize * deltaTime);
				}
				else
					_guiTimer[4]->SetVisible(false);
			}
		}

		void PlayerStateFrame::OnProcessEvent(unsigned id, EventData* data)
		{
			if (id == cPlayerPickItem && data && data->playerId == player()->GetId())
			{
				auto myData = static_cast<Player::MyEventData*>(data);
				NewPickItem(myData->slotType, myData->bonusType, cUndefPlayerId, false);
				return;
			}

			if (id == cAchievmentConditionComplete)
			{
				NewAchievment(static_cast<AchievmentCondition::MyEventData*>(data));
				return;
			}

			if (id == cPlayerDamage && data)
			{
				auto myData = static_cast<GameObject::MyEventData*>(data);

				if (myData->damage > 0.0f && myData->targetPlayerId == player()->GetId())
					ShowCarLifeBar(clHuman, myData->targetPlayerId, 0.26f);
				if (myData->damage > 0.0f && data->playerId == player()->GetId())
					ShowCarLifeBar(clOpponent, myData->targetPlayerId, 3.2f);

				return;
			}

			if (id == cPlayerKill && data && Race::IsHumanId(data->playerId) || id == cMineKill && data &&
				Race::IsHumanId(data->playerId))
			{
				auto myData = static_cast<GameObject::MyEventData*>(data);
				NewPickItem(Slot::cTypeEnd, GameObject::cBonusTypeEnd, myData->targetPlayerId, true);
				return;
			}

			int raceTimerInd = -1;

			if (id == cRaceStartWait)
				raceTimerInd = 0;
			if (id == cRaceStartTime1)
				raceTimerInd = 1;
			else if (id == cRaceStartTime2)
				raceTimerInd = 2;
			else if (id == cRaceStartTime3)
				raceTimerInd = 3;
			else if (id == cRaceStart)
				raceTimerInd = 4;

			if (raceTimerInd != -1 && HUD_STYLE != 0 && HUD_STYLE != 3)
			{
				D3DXVECTOR2 pos = menu()->GetGUI()->GetVPSize() / 2.0f;

				for (int i = 0; i < 5; ++i)
				{
					_guiTimer[i]->SetVisible(i == raceTimerInd);
					_guiTimer[i]->SetPos(pos);
					_guiTimer[i]->GetMaterial().SetAlpha(1.0f);
				}
			}
		}

		void PlayerStateFrame::OnDisconnectedPlayer(NetPlayer* sender)
		{
			RemoveOpponent(sender->model());
		}


		MiniMapFrame::MiniMapFrame(Menu* menu, HudMenu* hudMenu, gui::Widget* parent): _menu(menu), _hudMenu(hudMenu)
		{
			_root = _menu->GetGUI()->CreateDummy();
			_root->SetParent(parent);

			_map = _menu->CreateViewPort3d(_root, nullptr, "", gui::ViewPort3d::msStatic, false);

			_lapBack = _menu->CreatePlane(_root, nullptr, "GUI\\HUD\\lap.png", true, IdentityVec2,
			                              gui::Material::bmTransparency);
			_lap = _menu->CreateLabel(svNull, _root, "Small", NullVec2, gui::Text::haCenter, gui::Text::vaCenter,
			                          clrWhite);

			UpdateMap();
			CreatePlayers();

			_lapBack->SetVisible(HUD_STYLE != 3 && HUD_STYLE != 0 && HUD_STYLE != 4);
			_lap->SetVisible(HUD_STYLE != 3 && HUD_STYLE != 0 && HUD_STYLE != 4);
			_map->SetVisible(MM_STYLE != 0);
		}

		MiniMapFrame::~MiniMapFrame()
		{
			ClearPlayers();

			_menu->GetGUI()->ReleaseWidget(_lapBack);
			_menu->GetGUI()->ReleaseWidget(_lap);
			_menu->GetGUI()->ReleaseWidget(_map);
			_menu->GetGUI()->ReleaseWidget(_root);
		}

		void MiniMapFrame::ComputeNode(Nodes::iterator sIter, Nodes::iterator eIter, Nodes::iterator iter)
		{
			//nextIter
			auto nextIter = iter;
			++nextIter;
			//prevIter
			auto prevIter = iter;
			if (iter != sIter)
				--prevIter;
			else
				prevIter = eIter;

			//вычисл€ем dir
			if (nextIter != eIter)
				iter->dir = nextIter->pos - iter->pos;
			else
				iter->dir = iter->pos - prevIter->pos;
			D3DXVec2Normalize(&iter->dir, &iter->dir);
			//вычисл€ем prevDir
			if (prevIter != eIter)
				iter->prevDir = iter->pos - prevIter->pos;
			else
				iter->prevDir = iter->dir;
			D3DXVec2Normalize(&iter->prevDir, &iter->prevDir);
			//вычисл€ем midDir
			iter->midDir = (iter->prevDir + iter->dir);
			D3DXVec2Normalize(&iter->midDir, &iter->midDir);
			//вычисл€ем midNorm
			Vec2NormCCW(iter->midDir, iter->midNorm);

			//¬ычисл€ем _nodeRadius
			iter->cosDelta = abs(D3DXVec2Dot(&iter->dir, &iter->prevDir));
			//sinA/2 = sin(180 - D/2) = cos(D/2) = є(1 + cosD)/2
			iter->sinAlpha2 = sqrt((1.0f + iter->cosDelta) / 2.0f);
			iter->nodeRadius = 0.5f * iter->size / iter->sinAlpha2;

			iter->ccw = D3DXVec2CCW(&iter->prevDir, &iter->dir) > 0;
			if (iter->ccw)
				Vec2NormCCW(iter->midDir, iter->edgeNorm);
			else
				Vec2NormCW(iter->midDir, iter->edgeNorm);
		}

		void MiniMapFrame::AlignNode(const Node& src, Node& dest, float cosErr, float sizeErr)
		{
			D3DXVECTOR2 dir = dest.pos - src.pos;
			D3DXVec2Normalize(&dir, &dir);

			if (abs(dir.x) > cosErr)
			{
				dest.pos.y = src.pos.y;
			}
			if (abs(dir.y) > cosErr)
			{
				dest.pos.x = src.pos.x;
			}
			if (abs(dest.size - src.size) < sizeErr)
			{
				dest.size = src.size;
			}
		}

		void MiniMapFrame::AlignMidNodes(Node& node1, Node& node2, float cosErr, float sizeErr)
		{
			D3DXVECTOR2 dir = node2.pos - node1.pos;
			D3DXVec2Normalize(&dir, &dir);

			if (abs(dir.x) > cosErr)
			{
				node1.pos.y = (node1.pos.y + node2.pos.y) / 2.0f;
				node2.pos.y = node1.pos.y;
			}
			if (abs(dir.y) > cosErr)
			{
				node1.pos.x = (node1.pos.x + node2.pos.x) / 2.0f;
				node2.pos.x = node1.pos.x;
			}
			if (abs(node2.size - node1.size) < sizeErr)
			{
				node1.size = (node1.size + node2.size) / 2.0f;
				node2.size = node1.size;
			}
		}

		void MiniMapFrame::BuildPath(WayPath& path, res::VertexData& data)
		{
			const float cosErr = cos(20.0f * D3DX_PI / 180);
			const float sizeErr = 2.0f;
			const float smRadius = 10.0f;
			const int smSlice = 2;

			Nodes nodes;

			if (path.GetCount() < 2)
				return;

			WayNode* node = path.GetFirst();
			while (node)
			{
				nodes.push_back(Node(node->GetPos(), node->GetSize()));
				node = node->GetNext();
			}


			for (auto iter = nodes.begin(); iter != nodes.end();)
			{
				Node& node = *iter;
				auto nextIter = iter;
				++nextIter;
				if (nextIter == nodes.end()) break;
				Node& nextNode = *nextIter;

				D3DXVECTOR2 dir = nextNode.pos - node.pos;
				D3DXVec2Normalize(&dir, &dir);

				if (nextIter != --nodes.end())
				{
					AlignNode(node, nextNode, cosErr, sizeErr);
				}
				else
				{
					AlignMidNodes(nextNode, nodes.front(), cosErr, sizeErr);
					AlignNode(nextNode, node, cosErr, sizeErr);
					AlignNode(nodes.front(), *(++nodes.begin()), cosErr, sizeErr);
				}

				ComputeNode(nodes.begin(), nodes.end(), iter);
				if (smSlice > 0 && node.cosDelta < cosErr)
				{
					float cosAlpha2 = sqrt(1 - node.sinAlpha2 * node.sinAlpha2);
					float size = iter->size;

					D3DXVECTOR2 smPos = node.pos + smRadius / cosAlpha2 * node.edgeNorm;
					D3DXVECTOR2 smVec = -node.edgeNorm;
					float alpha2 = asin(node.sinAlpha2);
					bool ccw = node.ccw;

					iter = nodes.erase(iter);
					for (float i = -smSlice; i <= smSlice; ++i)
					{
						float dAlpha = i / smSlice * alpha2;
						D3DXQUATERNION rot;
						D3DXQuaternionRotationAxis(&rot, &ZVector, ccw ? dAlpha : -dAlpha);
						D3DXMATRIX rotMat;
						D3DXMatrixRotationQuaternion(&rotMat, &rot);
						D3DXVECTOR2 vec;
						D3DXVec2TransformNormal(&vec, &smVec, &rotMat);

						Node newNode;
						newNode.pos = smPos + vec * smRadius;
						newNode.size = size;
						iter = nodes.insert(iter, newNode);
						++iter;
					}
				}
				else
					iter = nextIter;
			}


			data.SetFormat(res::VertexData::vtPos3, true);
			data.SetFormat(res::VertexData::vtTex0, true);
			data.SetVertexCount(nodes.size() * 2);
			data.Init();
			res::VertexIter pVert = data.begin();

			int i = 0;
			for (auto iter = nodes.begin(); iter != nodes.end(); ++iter, ++i)
			{
				//curNode
				ComputeNode(nodes.begin(), nodes.end(), iter);
				Node node = *iter;

				D3DXVECTOR2 pos[2];
				pos[0] = node.pos + node.midNorm * node.nodeRadius;
				pos[1] = node.pos - node.midNorm * node.nodeRadius;

				*pVert.Pos3() = D3DXVECTOR3(pos[0].x, pos[0].y, 0.0f);
				*pVert.Tex0() = D3DXVECTOR2(static_cast<float>(i % 2), 0.0f);
				++pVert;
				*pVert.Pos3() = D3DXVECTOR3(pos[1].x, pos[1].y, 0.0f);
				*pVert.Tex0() = D3DXVECTOR2(static_cast<float>(i % 2), 1.0f);
				++pVert;
			}

			data.Update();
		}

		void MiniMapFrame::CreatePlayers()
		{
			const D3DXCOLOR color[4] = {clrRed, clrGreen, clrWhite, clrYellow};

			ClearPlayers();

			for (auto iter = _menu->GetRace()->GetPlayerList().begin(); iter != _menu->GetRace()->GetPlayerList().end();
			     ++iter)
			{
				gui::Plane3d* plane = _menu->GetGUI()->GetContext().CreatePlane3d();
				plane->SetSize(D3DXVECTOR2(10.0f, 10.0f));
				_map->GetBox()->InsertChild(plane);

				plane->GetMaterial()->SetColor(color[_players.size() % 4]);
				plane->GetMaterial()->GetSampler().SetTex(_menu->GetTexture("GUI\\HUD\\playerPoint2.png"));
				plane->GetMaterial()->SetBlending(gui::Material::bmTransparency);

				PlayerPoint pnt;
				pnt.plane = plane;
				plane->AddRef();

				pnt.player = *iter;
				pnt.player->AddRef();

				_players.push_back(pnt);
			}
		}

		void MiniMapFrame::DelPlayer(Players::const_iterator iter)
		{
			_map->GetBox()->RemoveChild(iter->plane);
			iter->plane->Release();
			_menu->GetGUI()->GetContext().ReleaseGraphic(iter->plane);

			iter->player->Release();

			_players.erase(iter);
		}

		void MiniMapFrame::DelPlayer(Player* player)
		{
			for (Players::const_iterator iter = _players.begin(); iter != _players.end(); ++iter)
				if (iter->player == player)
				{
					DelPlayer(iter);
					break;
				}
		}

		void MiniMapFrame::ClearPlayers()
		{
			while (_players.size() > 0)
				DelPlayer(_players.begin());
		}

		void MiniMapFrame::UpdatePlayers(float deltaTime)
		{
			for (auto iter = _players.begin(); iter != _players.end(); ++iter)
			{
				D3DXVECTOR3 pos3 = iter->player->GetCar().GetMapPos();
				pos3.z = 0.0f;
				iter->plane->SetPos(pos3);
				iter->plane->GetMaterial()->SetColor(iter->player->GetCar().color);
				if (iter->player->IsSpectator())
				{
					DelPlayer(iter->player);
				}
			}
		}

		void MiniMapFrame::UpdateMap()
		{
			const float cosErr = cos(20.0f * D3DX_PI / 180);
			const float sizeErr = 2.0f;

			for (auto iter = GetTrace()->GetPathes().begin(); iter != GetTrace()->GetPathes().end(); ++iter)
			{
				WayPath* path = *iter;

				if (path->GetCount() > 1)
				{
					gui::VBuf3d* buf3d = _menu->GetGUI()->GetContext().CreateVBuf3d();

					buf3d->GetOrCreateMesh();
					buf3d->GetMesh()->primitiveType = D3DPT_TRIANGLESTRIP;
					res::VertexData& data = *buf3d->GetMesh()->GetOrCreateData();

					BuildPath(*path, data);

					//buf3d->GetMaterial()->SetColor(clrBlue);
					buf3d->GetMaterial()->SetBlending(gui::Material::bmTransparency);
					if (HUD_STYLE == 4)
						buf3d->GetMaterial()->GetSampler().SetTex(_menu->GetTexture("GUI\\HUD\\mapFrameX.png"));
					else if (HUD_STYLE == 3)
						buf3d->GetMaterial()->GetSampler().SetTex(_menu->GetTexture("GUI\\HUD\\mapFrameClassic.png"));
					else if (HUD_STYLE == 2)
						buf3d->GetMaterial()->GetSampler().SetTex(_menu->GetTexture("GUI\\HUD\\mapFrameDark.png"));
					else
						buf3d->GetMaterial()->GetSampler().SetTex(_menu->GetTexture("GUI\\HUD\\mapFrame.png"));

					_map->GetBox()->InsertChild(buf3d);
				}
			}

			_map->SetAlign(true);

			WayNode* node = GetTrace()->GetPathes().front()->GetFirst();

			gui::Plane3d* start = _menu->CreatePlane3d(_map, "GUI\\HUD\\start.png", IdentityVec2);
			start->SetPos(node->GetPos());

			D3DXQUATERNION rot;
			QuatShortestArc(XVector, D3DXVECTOR3(node->GetTile().GetDir().x, node->GetTile().GetDir().y, 0.0f), rot);
			start->SetRot(rot);
			start->SetSize(D3DXVECTOR2(node->GetSize() / 4.0f, node->GetSize() / 2.0f));
		}

		Trace* MiniMapFrame::GetTrace()
		{
			return _menu->GetTrace();
		}

		void MiniMapFrame::AdjustLayout(const D3DXVECTOR2& vpSize)
		{
			//_map->SetPos(mapRect.GetCenter());

			if (MM_STYLE == 1)
			{
				AABB2 mapRect = _hudMenu->GetMiniMapRectA();
				_map->SetSize(mapRect.GetSize());
				if (HUD_STYLE != 3)
				{
					_map->SetPos(vpSize.x - mapRect.max.x, -mapRect.min.y);
				}
				else
				{
					_map->SetPos(mapRect.max.x, -mapRect.min.y);
				}
				_lapBack->SetPos(_hudMenu->GetLapPos() + D3DXVECTOR2(_lapBack->GetSize().x / 2, 0));
				_lap->SetPos(D3DXVECTOR2(_lapBack->GetPos().x - 10, _hudMenu->GetLapPos().y + 1));
			}
			else if (MM_STYLE == 2)
			{
				AABB2 mapRect = _hudMenu->GetMiniMapRectB();
				_map->SetSize(mapRect.GetSize());
				if (HUD_STYLE != 3)
				{
					_map->SetPos(vpSize.x - mapRect.max.x, -mapRect.min.y);
				}
				else
				{
					_map->SetPos(mapRect.max.x, -mapRect.min.y);
				}
				_lapBack->SetPos(_hudMenu->GetLapPos() + D3DXVECTOR2(_lapBack->GetSize().x / 2, 0));
				_lap->SetPos(D3DXVECTOR2(_lapBack->GetPos().x - 10, _hudMenu->GetLapPos().y + 1));
			}
			else if (MM_STYLE == 3)
			{
				AABB2 mapRect = _hudMenu->GetMiniMapRectC();
				_map->SetSize(mapRect.GetSize());
				if (HUD_STYLE != 3)
				{
					_map->SetPos(vpSize.x - mapRect.max.x, -mapRect.min.y);
				}
				else
				{
					_map->SetPos(mapRect.max.x, -mapRect.min.y);
				}
				_lapBack->SetPos(_hudMenu->GetLapPos() + D3DXVECTOR2(_lapBack->GetSize().x / 2, 0));
				_lap->SetPos(D3DXVECTOR2(_lapBack->GetPos().x - 10, _hudMenu->GetLapPos().y + 1));
			}
			else if (MM_STYLE == 4)
			{
				AABB2 mapRect = _hudMenu->GetMiniMapRectD();
				_map->SetSize(mapRect.GetSize());
				if (HUD_STYLE != 3)
				{
					_map->SetPos(vpSize.x - mapRect.max.x, -mapRect.min.y);
				}
				else
				{
					_map->SetPos(mapRect.max.x, -mapRect.min.y);
				}
				_lapBack->SetPos(_hudMenu->GetLapPos() + D3DXVECTOR2(_lapBack->GetSize().x / 2, 0));
				_lap->SetPos(D3DXVECTOR2(_lapBack->GetPos().x - 10, _hudMenu->GetLapPos().y + 1));
			}
			else if (MM_STYLE == 5)
			{
				AABB2 mapRect = _hudMenu->GetMiniMapRectE();
				_map->SetSize(mapRect.GetSize());
				if (HUD_STYLE != 3)
				{
					_map->SetPos(vpSize.x - mapRect.max.x, -mapRect.min.y);
				}
				else
				{
					_map->SetPos(mapRect.max.x, -mapRect.min.y);
				}
				_lapBack->SetPos(_hudMenu->GetLapPos() + D3DXVECTOR2(_lapBack->GetSize().x / 2, 0));
				_lap->SetPos(D3DXVECTOR2(_lapBack->GetPos().x - 10, _hudMenu->GetLapPos().y + 1));
			}
			else if (MM_STYLE == 6)
			{
				AABB2 mapRect = _hudMenu->GetMiniMapRectF();
				_map->SetSize(mapRect.GetSize());
				if (HUD_STYLE != 3)
				{
					_map->SetPos(vpSize.x - mapRect.max.x, -mapRect.min.y);
				}
				else
				{
					_map->SetPos(mapRect.max.x, -mapRect.min.y);
				}
				_lapBack->SetPos(_hudMenu->GetLapPos() + D3DXVECTOR2(_lapBack->GetSize().x / 2, 0));
				_lap->SetPos(D3DXVECTOR2(_lapBack->GetPos().x - 10, _hudMenu->GetLapPos().y + 1));
			}
			else if (MM_STYLE == 7)
			{
				AABB2 mapRect = _hudMenu->GetMiniMapRectG();
				_map->SetSize(mapRect.GetSize());
				if (HUD_STYLE != 3)
				{
					_map->SetPos(vpSize.x - mapRect.max.x, -mapRect.min.y);
				}
				else
				{
					_map->SetPos(mapRect.max.x, -mapRect.min.y);
				}
				_lapBack->SetPos(_hudMenu->GetLapPos() + D3DXVECTOR2(_lapBack->GetSize().x / 2, 0));
				_lap->SetPos(D3DXVECTOR2(_lapBack->GetPos().x - 10, _hudMenu->GetLapPos().y + 1));
			}
			else if (MM_STYLE == 8)
			{
				AABB2 mapRect = _hudMenu->GetMiniMapRectH();
				_map->SetSize(mapRect.GetSize());
				if (HUD_STYLE != 3)
				{
					_map->SetPos(vpSize.x - mapRect.max.x, -mapRect.min.y);
				}
				else
				{
					_map->SetPos(mapRect.max.x, -mapRect.min.y);
				}
				_lapBack->SetPos(_hudMenu->GetLapPos() + D3DXVECTOR2(_lapBack->GetSize().x / 2, 0));
				_lap->SetPos(D3DXVECTOR2(_lapBack->GetPos().x - 10, _hudMenu->GetLapPos().y + 1));
			}
			else if (MM_STYLE == 9)
			{
				AABB2 mapRect = _hudMenu->GetMiniMapRectI();
				_map->SetSize(mapRect.GetSize());
				if (HUD_STYLE != 3)
				{
					_map->SetPos(vpSize.x - mapRect.max.x, -mapRect.min.y);
				}
				else
				{
					_map->SetPos(mapRect.max.x, -mapRect.min.y);
				}
				_lapBack->SetPos(_hudMenu->GetLapPos() + D3DXVECTOR2(_lapBack->GetSize().x / 2, 0));
				_lap->SetPos(D3DXVECTOR2(_lapBack->GetPos().x - 10, _hudMenu->GetLapPos().y + 1));
			}
			else if (MM_STYLE == 10)
			{
				AABB2 mapRect = _hudMenu->GetMiniMapRectJ();
				_map->SetSize(mapRect.GetSize());
				if (HUD_STYLE != 3)
				{
					_map->SetPos(vpSize.x - mapRect.max.x, -mapRect.min.y);
				}
				else
				{
					_map->SetPos(mapRect.max.x, -mapRect.min.y);
				}
				_lapBack->SetPos(_hudMenu->GetLapPos() + D3DXVECTOR2(_lapBack->GetSize().x / 2, 0));
				_lap->SetPos(D3DXVECTOR2(_lapBack->GetPos().x - 10, _hudMenu->GetLapPos().y + 1));
			}
			else if (MM_STYLE == 11)
			{
				AABB2 mapRect = _hudMenu->GetMiniMapRectK();
				_map->SetSize(mapRect.GetSize());
				if (HUD_STYLE != 3)
				{
					_map->SetPos(vpSize.x - mapRect.max.x, -mapRect.min.y);
				}
				else
				{
					_map->SetPos(mapRect.max.x, -mapRect.min.y);
				}
				_lapBack->SetPos(_hudMenu->GetLapPos() + D3DXVECTOR2(_lapBack->GetSize().x / 2, 0));
				_lap->SetPos(D3DXVECTOR2(_lapBack->GetPos().x - 10, _hudMenu->GetLapPos().y + 1));
			}
			else
			{
				AABB2 mapRect = _hudMenu->GetMiniMapRectF();
				_map->SetSize(mapRect.GetSize());
				if (HUD_STYLE != 3)
				{
					_map->SetPos(vpSize.x - mapRect.max.x, -mapRect.min.y);
				}
				else
				{
					_map->SetPos(mapRect.max.x, -mapRect.min.y);
				}
				_lapBack->SetPos(_hudMenu->GetLapPos() + D3DXVECTOR2(_lapBack->GetSize().x / 2, 0));
				_lap->SetPos(D3DXVECTOR2(_lapBack->GetPos().x - 10, _hudMenu->GetLapPos().y + 1));
			}
		}

		void MiniMapFrame::Show(bool value)
		{
			_root->SetVisible(value);

			float angle = _menu->GetRace()->GetTournament().GetCurTrack().GetMMAngle();

			D3DXQUATERNION rot;
			D3DXQuaternionRotationAxis(&rot, &ZVector, angle);
			_map->GetBox()->SetRot(rot);

			if (!_menu->GetRace()->GetTournament().GetCurTrack().Minimap())
				_map->SetVisible(false);
		}

		bool MiniMapFrame::IsVisible() const
		{
			return _root->GetVisible();
		}

		void MiniMapFrame::OnProgress(float deltaTime)
		{
			UpdatePlayers(deltaTime);

			Player* sPlr = _menu->GetPlayer();
			for (auto iter = _menu->GetRace()->GetPlayerList().begin(); iter != _menu->GetRace()->GetPlayerList().end();
			     ++iter)
			{
				if ((*iter)->isSubject())
					sPlr = *iter;
			}

			unsigned lapsCount = _menu->GetRace()->GetTournament().GetCurTrack().GetLapsCount();
			if (_menu->survivalMode())
			{
				if (_menu->IsNetGame() || _menu->IsSkirmish())
				{
					lapsCount = (TOTALPLAYERS_COUNT - SPECTATORS_COUNT) - 1;
				}
			}
			unsigned numLaps = std::min(sPlr->GetCar().numLaps + 1, lapsCount);
			if (_hudMenu->GetPlayer()->GetFinished() == true)
			{
				lapRestFix = 0;
			}
			lapsRest = lapRestFix + (lapsCount - numLaps);

			std::stringstream sstream;
			sstream << _menu->GetString(svLap) << " " << numLaps << '/' << lapsCount;
			_lap->SetText(sstream.str());
		}

		void MiniMapFrame::OnDisconnectedPlayer(NetPlayer* sender)
		{
			DelPlayer(sender->model());
		}

		gui::Dummy* MiniMapFrame::GetRoot()
		{
			return _root;
		}


		HudMenu::HudMenu(Menu* menu, gui::Widget* parent, Player* player): _menu(menu), _player(player), _state(msMain)
		{
			LSL_ASSERT(menu && _player);

			_player->AddRef();

			_root = _menu->GetGUI()->CreateDummy();
			_root->SetParent(parent);

			_miniMapFrame = new MiniMapFrame(menu, this, _root);
			_playerStateFrame = new PlayerStateFrame(menu, this, _root);

			ApplyState(_state);

			_menu->GetNet()->RegUser(this);
			_menu->GetControl()->InsertEvent(this);
		}

		HudMenu::~HudMenu()
		{
			_menu->GetControl()->RemoveEvent(this);
			_menu->GetNet()->UnregUser(this);

			delete _playerStateFrame;
			delete _miniMapFrame;

			_menu->GetGUI()->ReleaseWidget(_root);

			_player->Release();
		}

		void HudMenu::ApplyState(State state)
		{
			_miniMapFrame->Show(state == msMain);
			_playerStateFrame->Show(state == msMain);
		}

		void HudMenu::OnDisconnectedPlayer(NetPlayer* sender)
		{
			_playerStateFrame->OnDisconnectedPlayer(sender);
			_miniMapFrame->OnDisconnectedPlayer(sender);
		}

		bool HudMenu::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
		{
			Player* plr = _menu->GetPlayer();
			GameCar* plrcar = plr->GetCar().gameObj;
			if (sender == _menu->GetAcceptSender())
			{
				_menu->ShowCursor(false);
				_menu->Pause(false);

				if (_menu->GetAcceptResultYes())
				{
					Menu* menu = _menu;
					if (_menu->IsCampaign())
					{
						if (plrcar != nullptr && plr->GetFinished() == false)
						{
							plrcar->GetLogic()->Damage(plrcar, plr->GetId(), plrcar, 1000, plrcar->dtNone);
							plr->SetSuicide(true);
							plr->AddDeadsTotal(-1);
							_menu->StopMusic();
							_menu->StopSound();
						}
						else
						{
							plr->SetSuicide(true);
							_menu->StopMusic();
							_menu->StopSound();
						}
					}
					else
					{
						plr->SetSuicide(true);
						_menu->StopMusic();
						_menu->StopSound();
					}
				}
				else
				{
					//GetPlayer()->GetRace()->GetGame()->gameMusic()->Pause(false);
					_menu->HideAccept();
				}

				return true;
			}

			return false;
		}

		bool HudMenu::OnHandleInput(const InputMessage& msg)
		{
			Player* plr = _menu->GetPlayer();
			if (msg.action == gaEscape && msg.state == ksDown && !msg.repeat)
			{
				if (plr->GetCar().gameObj != nullptr)
					plr->GetCar().gameObj->GetGrActor().SetVisible(true);

				if (!_menu->IsPaused())
				{
					_menu->ShowAccept(false, _menu->GetString(svHintExitRace), _menu->GetString(svYes),
					                  _menu->GetString(svNo), _menu->GetGUI()->GetVPSize() / 2.0f,
					                  gui::Widget::waCenter, this);
					_menu->ShowCursor(true);
					_menu->Pause(true);
					//world->GetLogic()->GetVolume(Logic::scMusic)

					//GetPlayer()->GetRace()->GetGame()->gameMusic()->Pause(true);
				}
				else
				{
					_menu->ShowCursor(false);
					_menu->Pause(false);
					_menu->HideAccept();
				}
				return true;
			}
			return false;
		}

		void HudMenu::AdjustLayout(const D3DXVECTOR2& vpSize)
		{
			_miniMapFrame->AdjustLayout(vpSize);
			_playerStateFrame->AdjustLayout(vpSize);
		}

		void HudMenu::Show(bool value)
		{
			_root->SetVisible(value);
		}

		void HudMenu::OnProgress(float deltaTime)
		{
			Menu* menu = _menu;
			Player* plr = _menu->GetPlayer();
			if (_miniMapFrame->IsVisible())
			{
				_miniMapFrame->OnProgress(deltaTime);
			}

			if (_playerStateFrame->visible())
			{
				_playerStateFrame->OnProgress(deltaTime);
			}
			if (plr->IsHuman() && plr->GetSuicide() && (plr->GetCar().mapObj || plr->IsSpectator()))
			{
				if (menu->IsNetGame() && menu->GetNet()->isClient())
				{
					menu->ExitRace();
					menu->ExitMatch();
				}
				else
				{
					menu->ExitRace();
				}
			}
		}

		Player* HudMenu::GetPlayer()
		{
			return _player;
		}

		gui::Widget* HudMenu::GetRoot()
		{
			return _root;
		}

		HudMenu::State HudMenu::GetState() const
		{
			return _state;
		}

		void HudMenu::SetState(State value)
		{
			if (_state != value)
			{
				_state = value;
				ApplyState(_state);
			}
		}

		AABB2 HudMenu::GetMiniMapRectA() //50%
		{
			D3DXVECTOR2 size(160.0f, 160.0f);
			return AABB2(size);
		}

		AABB2 HudMenu::GetMiniMapRectB() //60%
		{
			D3DXVECTOR2 size(192.0f, 192.0f);
			return AABB2(size);
		}

		AABB2 HudMenu::GetMiniMapRectC() //70%
		{
			D3DXVECTOR2 size(224.0f, 224.0f);
			return AABB2(size);
		}

		AABB2 HudMenu::GetMiniMapRectD() //80%
		{
			D3DXVECTOR2 size(256.0f, 256.0f);
			return AABB2(size);
		}

		AABB2 HudMenu::GetMiniMapRectE() //90%
		{
			D3DXVECTOR2 size(288.0f, 288.0f);
			return AABB2(size);
		}

		AABB2 HudMenu::GetMiniMapRectF() //100%
		{
			D3DXVECTOR2 size(320.0f, 320.0f);
			return AABB2(size);
		}

		AABB2 HudMenu::GetMiniMapRectG() //110%
		{
			D3DXVECTOR2 size(352.0f, 352.0f);
			return AABB2(size);
		}

		AABB2 HudMenu::GetMiniMapRectH() //120%
		{
			D3DXVECTOR2 size(384.0f, 384.0f);
			return AABB2(size);
		}

		AABB2 HudMenu::GetMiniMapRectI() //130%
		{
			D3DXVECTOR2 size(416.0f, 416.0f);
			return AABB2(size);
		}

		AABB2 HudMenu::GetMiniMapRectJ() //140%
		{
			D3DXVECTOR2 size(448.0f, 448.0f);
			return AABB2(size);
		}

		AABB2 HudMenu::GetMiniMapRectK() //150%
		{
			D3DXVECTOR2 size(480.0f, 480.0f);
			return AABB2(size);
		}

		D3DXVECTOR2 HudMenu::GetWeaponPos()
		{
			return D3DXVECTOR2(LOCAL_XPOS_WEAPONITEM, LOCAL_YPOS_WEAPONITEM);
		}

		D3DXVECTOR2 HudMenu::GetWeaponBoxPos()
		{
			return D3DXVECTOR2(LOCAL_XPOS_WPN_BOX_ITEM, LOCAL_YPOS_WPN_BOX_ITEM);
		}

		D3DXVECTOR2 HudMenu::GetWeaponLabelPos()
		{
			return D3DXVECTOR2(-10.0f, 26.0f);
		}

		D3DXVECTOR2 HudMenu::GetWeaponLabelAltPos()
		{
			return D3DXVECTOR2(0.0f, -64.0f);
		}

		D3DXVECTOR2 HudMenu::GetWeaponPosMine()
		{
			return D3DXVECTOR2(LOCAL_XPOS_MINEITEM, LOCAL_YPOS_MINEITEM);
		}

		D3DXVECTOR2 HudMenu::GetWeaponPosMineLabel()
		{
			return D3DXVECTOR2(105.0f, 157.0f);
		}

		D3DXVECTOR2 HudMenu::GetWeaponPosHyper()
		{
			return D3DXVECTOR2(LOCAL_XPOS_HYPERITEM, LOCAL_YPOS_HYPERITEM);
		}

		D3DXVECTOR2 HudMenu::GetWeaponPosHyperLabel()
		{
			return D3DXVECTOR2(105.0f, 12.0f);
		}

		D3DXVECTOR2 HudMenu::GetHyperCountLabelClassic()
		{
			return D3DXVECTOR2(50.0f, 9.0f);
		}

		//
		D3DXVECTOR2 HudMenu::GetMinesCountLabelClassic()
		{
			return D3DXVECTOR2(200.0f, 9.0f);
		}

		//
		D3DXVECTOR2 HudMenu::GetAllWeaponCountLabelClassic()
		{
			return D3DXVECTOR2(-90.0f, 9.0f);
		}

		D3DXVECTOR2 HudMenu::GetPlacePos()
		{
			return D3DXVECTOR2(105, 88);
		}

		D3DXVECTOR2 HudMenu::GetClassicLapsPos()
		{
			return D3DXVECTOR2(323.0f, 43.0f);
		}

		D3DXVECTOR2 HudMenu::GetLapPos()
		{
			return D3DXVECTOR2(0, 200);
		}

		D3DXVECTOR2 HudMenu::GetLifeBarPos()
		{
			return D3DXVECTOR2(165.0f, 0.0f);
		}

		D3DXVECTOR2 HudMenu::GetHyperBarPos()
		{
			return D3DXVECTOR2(LOCAL_XPOS_WEAPONITEM + 12.0f, LOCAL_YPOS_WEAPONITEM + 93.0f);
		}

		D3DXVECTOR2 HudMenu::GetPickItemsPos()
		{
			return D3DXVECTOR2(0.0f, 255.0f);
		}

		D3DXVECTOR2 HudMenu::GetClassicItemsPos()
		{
			return D3DXVECTOR2(X_VPSIZE / 2, 247.0f);
		}

		D3DXVECTOR2 HudMenu::GetAltItemsPos()
		{
			return D3DXVECTOR2(45.0f, Y_VPSIZE - 45.0f);
		}

		D3DXVECTOR2 HudMenu::GetAchievmentItemsPos()
		{
			return D3DXVECTOR2((100.0f + _menu->GetGUI()->GetVPSize().x) / 2, 15.0f + LL_OFFSET);
		}

		D3DXVECTOR2 HudMenu::GetCarLifeBarPos()
		{
			return D3DXVECTOR2(4.0f, -10.0f);
		}

		D3DXVECTOR2 HudMenu::GetCarLifeBarXPos()
		{
			return D3DXVECTOR2(0.0f, 0.0f);
		}
	}
}
