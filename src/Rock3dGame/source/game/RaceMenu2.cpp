#include "stdafx.h"
#include "game\Menu.h"

#include "game\RaceMenu2.h"
#include "game\World.h"

namespace r3d
{

namespace game
{

namespace n
{

CarFrame::CarFrame(Menu* menu, RaceMenu* raceMenu, gui::Widget* parent): MenuFrame(menu, parent), _raceMenu(raceMenu), _car(NULL), _carMapObj(NULL), _camStyle(cCamStyleEnd)
{
	MapObjRec* record =  menu->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\garage");
	_garageMapObj = &menu->GetRace()->GetMap()->AddMapObj(record);	
	_garageMapObj->GetGameObj().GetGrActor().SetVisible(false);

	record = menu->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\question");
	_secretMapObj = &menu->GetRace()->GetMap()->AddMapObj(record);
	_secretMapObj->GetGameObj().SetWorldPos(D3DXVECTOR3(0, 0, 0.39f));
	_secretMapObj->GetGameObj().GetGrActor().SetVisible(false);
}

CarFrame::~CarFrame()
{
	Show(false);

	SetCar((Garage::Car*)NULL, clrWhite);

	menu()->GetRace()->GetMap()->DelMapObj(_secretMapObj);
	menu()->GetRace()->GetMap()->DelMapObj(_garageMapObj);
	menu()->GetRace()->GetWorld()->GetCamera()->ChangeStyle(CameraManager::csFreeView);
}

void CarFrame::OnProgress(float deltaTime)
{
	if (_secretMapObj->GetGameObj().GetGrActor().GetVisible())
	{
		D3DXQUATERNION rot = _secretMapObj->GetGameObj().GetRot();
		D3DXQUATERNION dRot;
		D3DXQuaternionRotationAxis(&dRot, &ZVector, 2.0f * D3DX_PI * deltaTime * 0.1f);
		_secretMapObj->GetGameObj().SetRot(dRot * rot);
	}
}

void CarFrame::OnShow(bool value)
{
	_garageMapObj->GetGameObj().GetGrActor().SetVisible(value);
	if (_carMapObj)
		_carMapObj->GetGameObj().GetGrActor().SetVisible(value);

	if (value)
	{
		menu()->GetEnv()->SetWorldType(Environment::wtGarage);
		menu()->GetEnv()->SetWheater(Environment::ewGarage);
		menu()->GetEnv()->StartScene();

		_camStyle = cCamStyleEnd;
		SetCamStyle(csCar, true);

		menu()->GetEnv()->SetLampPos(D3DXVECTOR3(-0.94673467f, 3.0021181f, 2.9447727f), 0);
		menu()->GetEnv()->SetLampRot(D3DXQUATERNION(0.19748747f, 0.34095559f, -0.46066701f, 0.79532242f), 0);
		menu()->GetEnv()->SetLampColor(clrWhite, 0);
		menu()->GetEnv()->SwitchOnLamp(0, true);

		menu()->GetEnv()->SetLampPos(D3DXVECTOR3(6.0344887f, -5.2521329f, 1.6322796f), 1);
		menu()->GetEnv()->SetLampRot(D3DXQUATERNION(-0.17059785f, 0.045529708f, 0.95100683f, 0.25379914f), 1);
		menu()->GetEnv()->SetLampColor(clrWhite, 1);
		menu()->GetEnv()->SwitchOnLamp(1, true);
	}
	else
		menu()->GetEnv()->ReleaseScene();
}

Garage::Car* CarFrame::GetCar()
{
	return _car;
}

void CarFrame::SetCar(Garage::Car* value, const D3DXCOLOR& color, bool secret)
{
	if (Object::ReplaceRef(_car, value) || (value && !secret && _carMapObj == NULL))
	{		
		if (_carMapObj)
			menu()->GetRace()->GetMap()->DelMapObj(_carMapObj);
		_carMapObj = NULL;

		_car = value;

		if (_car && !secret)
		{
			_carMapObj = &menu()->GetRace()->GetMap()->AddMapObj(_car->GetRecord());
			RockCar& gameCar = _carMapObj->GetGameObj<RockCar>();
			float wheelOffs = 0;

			for (GameCar::Wheels::const_iterator iter = gameCar.GetWheels().begin(); iter != gameCar.GetWheels().end(); ++iter) 
			{
				(*iter)->SetPos((*iter)->GetPos() + D3DXVECTOR3(0, 0, -0.5f) * (*iter)->GetShape()->GetSuspensionTravel() + (*iter)->GetOffset());

				if ((*iter)->invertWheel)
				{
					D3DXQUATERNION rot;
					D3DXQuaternionRotationAxis(&rot, &D3DXVECTOR3(0, 0, 1), D3DX_PI);
					(*iter)->SetRot(rot);
				}

				wheelOffs = abs((*iter)->GetPos().z) + (*iter)->GetShape()->GetRadius() + (*iter)->GetOffset().z;
			}

			_carMapObj->GetGameObj().SetWorldPos(D3DXVECTOR3(0, 0, wheelOffs - 0.71f));
			_carMapObj->GetGameObj().GetGrActor().SetVisible(visible());

			SetCarColor(color);
		}

		_secretMapObj->GetGameObj().GetGrActor().SetVisible(_car && secret);
	}
}

void CarFrame::SetCar(MapObjRec* value, const D3DXCOLOR& color, bool secret)
{
	SetCar(menu()->GetRace()->GetGarage().FindCar(value), color, secret);
}

D3DXCOLOR CarFrame::GetCarColor()
{
	if (_carMapObj)
	{
		graph::IVBMeshNode& mesh = static_cast<graph::IVBMeshNode&>(_carMapObj->GetGameObj().GetGrActor().GetNodes().front());
		return mesh.material.Get()->samplers[0].GetColor();
	}
	return clrWhite;
}

void CarFrame::SetCarColor(const D3DXCOLOR& value)
{
	if (_carMapObj)
	{
		graph::IVBMeshNode& mesh = static_cast<graph::IVBMeshNode&>(_carMapObj->GetGameObj().GetGrActor().GetNodes().front());
		mesh.material.Get()->samplers[0].SetColor(value);
	}
}

void CarFrame::SetSlots(Player* player, bool includeDefSlots)
{
	if (_carMapObj == NULL)
		return;

	RockCar& gameCar = _carMapObj->GetGameObj<RockCar>();
	gameCar.GetWeapons().Clear();

	for (int i = Player::stHyper; i <= Player::stWeapon4; ++i)
	{
		Player::SlotType type = (Player::SlotType)i;
		WeaponItem* slot = NULL;

		Garage::PlaceSlot placeSlot = _car->GetSlot(type);
		if (!placeSlot.active || !placeSlot.show)
			continue;

		if (includeDefSlots)
		{
			const Garage::PlaceItem* placeItem = placeSlot.defItem != NULL ? placeSlot.FindItem(placeSlot.defItem) : NULL;
			if (placeItem)
			{
				slot = &placeSlot.defItem->GetItem<WeaponItem>();
				slot->SetPos(placeSlot.pos + placeItem->offset);
				slot->SetRot(placeItem->rot);
			}
			else if (player)
			{
				Record* playerSlot = player->GetSlot(type);
				if (playerSlot && placeSlot.FindItem(playerSlot) == NULL)
					continue;
			}
		}

		if (player)
		{
			if (slot == NULL)
				slot = player->GetSlotInst(type) ? &player->GetSlotInst(type)->GetItem<WeaponItem>() : NULL;
		}

		if (slot)
		{	
			const Garage::PlaceItem* placeItem = placeSlot.FindItem(slot->GetSlot()->GetRecord());
			if (placeItem && slot->GetMapObj())
			{
				MapObj& wpn = gameCar.GetWeapons().Add(slot->GetMapObj());
				wpn.GetGameObj().SetPos(placeSlot.pos + placeItem->offset);
				wpn.GetGameObj().SetRot(placeItem->rot);
			}
		}
	}
}

void CarFrame::SetCamStyle(CamStyle value, bool instant)
{
	if (_camStyle == value)
		return;
	CamStyle lastCamStyle = _camStyle;
	_camStyle = value;

	D3DXVECTOR3 pos = NullVector;
	D3DXQUATERNION rot = NullQuaternion;
	CameraManager* camera = menu()->GetRace()->GetWorld()->GetCamera();
	graph::Camera* cameraInst = menu()->GetRace()->GetWorld()->GetGraph()->GetCamera();	

	camera->StopFly();

	switch (value)
	{
	case csCar:
	{
		if (lastCamStyle == csSlots)
		{
			pos = _camLastPos;
			rot = _camLastRot;
		}
		else
		{
			pos = D3DXVECTOR3(2.9176455f, 3.8489482f, 1.2934232f);
			rot = D3DXQUATERNION(0.11683256f, 0.058045074f, -0.88791621f, 0.44111854f);
		}

		camera->SetTarget(D3DXVECTOR4(0, 0, 0, 5.0f));
		camera->SetClampAngle(D3DXVECTOR4(0.0f, 0.0f, 25.0f * D3DX_PI/180, 80.0f * D3DX_PI/180));
		camera->SetAngleSpeed(D3DXVECTOR3(D3DX_PI/48, 0, 0));
		camera->SetStableAngle(D3DXVECTOR3(75.0f * D3DX_PI/180, 0, 0));
		break;
	}

	case csSlots:
	{
		D3DXVECTOR3 posList[8] = {
			D3DXVECTOR3(-5.6156259f, 4.3894496f, 1.3072476f),
			D3DXVECTOR3(1.0063084f, 6.9253764f, 1.7360222f),
			D3DXVECTOR3(5.6724834f, 4.9537153f, 1.3952403f),
			D3DXVECTOR3(7.0655332f, -1.0402107f, 1.2024049f),
			D3DXVECTOR3(5.4610982f, -5.3067584f, 1.2650701f),
			D3DXVECTOR3(-1.1062316f, -7.5020962f, 1.1599010f),
			D3DXVECTOR3(-5.9399834f, -4.8825927f, 1.0367264f),
			D3DXVECTOR3(-7.4102926f, 0.61909121f, 1.1492375f)
		};

		D3DXQUATERNION rotList[8] = {
			D3DXQUATERNION(0.021078700f, 0.10266567f, -0.20001189f, 0.97417259f),
			D3DXQUATERNION(0.093931124f, 0.099822313f, -0.67882264f, 0.72140080f),
			D3DXQUATERNION(0.098612130f, 0.053653944f, -0.87284911f, 0.47491166f),
			D3DXQUATERNION(0.10475823f, 0.0032271212f, -0.99402952f, 0.030623097f),
			D3DXQUATERNION(0.10137362f, -0.026610103f, -0.96191424f, -0.25249690f),
			D3DXQUATERNION(0.070100352f, -0.063905962f, -0.73568070f, -0.67066783f),
			D3DXQUATERNION(0.041057255f, -0.077143900f, -0.46803388f, -0.87939179f),
			D3DXQUATERNION(0.0077582477f, -0.092043117f, -0.083636492f, -0.99221748f)
		};

		D3DXQUATERNION camRot = cameraInst->GetRot();
		float minAngle;

		for (int i = 0; i < 8; ++i)
		{
			float angle = abs(QuatAngle(camRot, rotList[i]));
			if (i == 0 || minAngle > angle)
			{
				minAngle = angle;
				pos = posList[i];
				rot = rotList[i];
			}
		}

		camera->ChangeStyle(CameraManager::csBlock);
		break;
	}
	}

	_camLastPos = cameraInst->GetPos();
	_camLastRot = cameraInst->GetRot();

	if (instant)
	{
		cameraInst->SetPos(pos);
		cameraInst->SetRot(rot);
	}
	else
	{
		camera->FlyTo(pos, rot, 0.5f);
	}

	if (_camStyle == csCar)
		camera->ChangeStyle(CameraManager::csAutoObserver);

	if (lastCamStyle == csCar)
	{
		D3DXVECTOR4 target = camera->GetTarget();
		camera->GetObserverCoord(D3DXVECTOR3(target.x, target.y, target.z), target.w, &_camLastPos, _camLastRot, NullVec2, 0, false, false, true, NULL, NULL, NULL);
	}
}




SpaceshipFrame::SpaceshipFrame(Menu* menu, RaceMenu* raceMenu, gui::Widget* parent): MenuFrame(menu, parent), _raceMenu(raceMenu), _redLampTime(0.0f)
{
	{		
		D3DXVECTOR3 center = D3DXVECTOR3(63.0f, 0.0f, 23.0f);

		MapObjRec* record =  menu->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\space2");
		_spaceMapObj = &menu->GetRace()->GetMap()->AddMapObj(record);
		_spaceMapObj->GetGameObj().GetGrActor().SetVisible(false);		

		graph::PlaneNode* spaceSprite = &((graph::PlaneNode&)_spaceMapObj->GetGameObj().GetGrActor().GetNodes().front());		

		D3DXVECTOR2 imageSize = static_cast<graph::Sampler2d&>(spaceSprite->material.Get()->samplers.front()).GetSize();
		float aspect = imageSize.x/imageSize.y;
		spaceSprite->SetPos(center);
		spaceSprite->SetScale(D3DXVECTOR3(aspect * 70.0f, 70.0f, 1.0f));
		spaceSprite->SetPitchAngle(-D3DX_PI/2);
		spaceSprite->SetRollAngle(-D3DX_PI/2);
	}

	{
		MapObjRec* record =  menu->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\angar");
		_angarMapObj = &menu->GetRace()->GetMap()->AddMapObj(record);	
		_angarMapObj->GetGameObj().GetGrActor().SetVisible(false);
	}
}

SpaceshipFrame::~SpaceshipFrame()
{
	Show(false);

	menu()->GetRace()->GetMap()->DelMapObj(_angarMapObj);
	menu()->GetRace()->GetMap()->DelMapObj(_spaceMapObj);

	menu()->GetRace()->GetWorld()->GetCamera()->ChangeStyle(CameraManager::csFreeView);
}

void SpaceshipFrame::OnProgress(float deltaTime)
{
	_redLampTime = fmodf(_redLampTime, 3.0f);
	float redIntens = ClampValue((_redLampTime - 1.5f)/0.15f, 0.0f, 1.0f) - ClampValue((_redLampTime - 2.85f)/0.15f, 0.0f, 1.0f);
	redIntens = 0.7f * redIntens;
	_redLampTime += deltaTime;

	menu()->GetEnv()->SwitchOnLamp(1, _redLampTime >= 1.5f);
	menu()->GetEnv()->SetLampColor(clrRed * redIntens, 1);
}

void SpaceshipFrame::OnShow(bool value)
{
	_angarMapObj->GetGameObj().GetGrActor().SetVisible(value);
	_spaceMapObj->GetGameObj().GetGrActor().SetVisible(value);

	if (value)
	{
		menu()->GetEnv()->SetWorldType(Environment::wtAngar);
		menu()->GetEnv()->SetWheater(Environment::ewAngar);
		menu()->GetEnv()->StartScene();

		menu()->GetEnv()->SetLampPos(D3DXVECTOR3(22.169474f, -5.9075522f, 35.802311f), 0);
		menu()->GetEnv()->SetLampRot(D3DXQUATERNION(-0.47304764f, 0.077942163f, 0.86590993f, 0.14267041f), 0);
		menu()->GetEnv()->SetLampColor(clrWhite, 0);
		menu()->GetEnv()->SwitchOnLamp(0, true);

		menu()->GetEnv()->SetLampPos(D3DXVECTOR3(-20.881384f, -21.184746f, 26.121809f), 1);
		menu()->GetEnv()->SetLampRot(D3DXQUATERNION(-0.16464995f, 0.34524971f, 0.39772648f, 0.83397770f), 1);
		menu()->GetEnv()->SetLampColor(clrRed * 0.8f, 1);
		menu()->GetEnv()->SwitchOnLamp(1, false);

		menu()->GetEnv()->SetLampPos(D3DXVECTOR3(52.307316f, 24.327570f, 32.772705f), 2);
		menu()->GetEnv()->SetLampRot(D3DXQUATERNION(0.21948183f, 0.18329506f, -0.73549527f, 0.61423278f), 2);
		menu()->GetEnv()->SetLampColor(D3DXCOLOR(0xff88fefe), 2);
		menu()->GetEnv()->SwitchOnLamp(2, true);

		CameraManager* camera = menu()->GetRace()->GetWorld()->GetCamera();
		graph::Camera* cameraInst = menu()->GetRace()->GetWorld()->GetGraph()->GetCamera();	

		camera->StopFly();
		cameraInst->SetPos(D3DXVECTOR3(-43.756214f, -11.786510f, 21.129881f));
		cameraInst->SetRot(D3DXQUATERNION(-0.028391786f, 0.21455817f, 0.12807286f, 0.96786171f));
		camera->ChangeStyle(CameraManager::csAutoObserver);

		camera->SetTarget(D3DXVECTOR4(0, 0, 0, 50.0f));
		camera->SetClampAngle(D3DXVECTOR4(40.0f * D3DX_PI/180, 30.0f * D3DX_PI/180, 45.0f * D3DX_PI/180, 80.0f * D3DX_PI/180));
		camera->SetAngleSpeed(D3DXVECTOR3(D3DX_PI/96, 0, 0));
		camera->SetStableAngle(D3DXVECTOR3(65.0f * D3DX_PI/180, 0, 0));

		_redLampTime = 0.0f;
	}
	else
		menu()->GetEnv()->ReleaseScene();
}




GarageFrame::GarageFrame(Menu* menu, RaceMenu* raceMenu, gui::Widget* parent): MenuFrame(menu, parent), _raceMenu(raceMenu), _lastCarIndex(-1)
{
	const D3DXCOLOR color2 = D3DXCOLOR(214.0f, 214.0f, 214.0f, 255.0f)/255.0f;	

	StringValue strLabels[cLabelEnd] = {svGarage, svNull, svNull, svNull, svCarCost};
	std::string fontLabels[cLabelEnd] = {"Header", "Header", "VerySmall", "Item", "Header"};
	gui::Text::HorAlign horLabels[cLabelEnd] = {gui::Text::haCenter, gui::Text::haCenter, gui::Text::haCenter, gui::Text::haRight, gui::Text::haCenter};
	gui::Text::VertAlign vertLabels[cLabelEnd] = {gui::Text::vaCenter, gui::Text::vaCenter, gui::Text::vaTop, gui::Text::vaCenter, gui::Text::vaCenter};
	D3DXCOLOR colorLabels[cLabelEnd] = {color2, color2, color2, clrWhite, color2};

	_topPanel = menu->CreatePlane(root(), this, "GUI\\topPanel2.png", true, IdentityVec2, gui::Material::bmTransparency);
	_topPanel->SetAlign(gui::Widget::waTop);

	_bottomPanel = menu->CreatePlane(root(), this, "GUI\\bottomPanel2.png", true, IdentityVec2, gui::Material::bmTransparency);
	_bottomPanel->SetAnchor(gui::Widget::waBottom);
	_bottomPanel->SetAlign(gui::Widget::waBottom);

	_rightPanel = menu->CreatePlane(root(), this, "GUI\\rightPanel2.png", true, IdentityVec2, gui::Material::bmTransparency);
	_rightPanel->SetAlign(gui::Widget::waRight);

	_leftPanel = menu->CreatePlane(root(), this, "GUI\\rightPanel2.png", true, IdentityVec2, gui::Material::bmTransparency);
	_leftPanel->SetAlign(gui::Widget::waRight);
	_leftPanel->SetRot(D3DX_PI);

	_headerBg = menu->CreatePlane(_topPanel, this, "GUI\\header2.png", true, IdentityVec2, gui::Material::bmTransparency);
	_headerBg->SetAlign(gui::Widget::waCenter);
	_headerBg->SetVisible(false);

	_moneyBg = menu->CreatePlane(_bottomPanel, this, "GUI\\moneyBg.png", true, IdentityVec2, gui::Material::bmTransparency);
	_moneyBg->SetAlign(gui::Widget::waRightBottom);	

	_stateBg = menu->CreatePlane(_bottomPanel, this, "GUI\\statFrame2.png", true, IdentityVec2, gui::Material::bmTransparency);
	_stateBg->SetAlign(gui::Widget::waLeftTop);

	_speedBar = menu->CreateBar(_stateBg, NULL, "GUI\\statBar2.png", "");
	_armorBar = menu->CreateBar(_stateBg, NULL, "GUI\\statBar2.png", "");
	_damageBar = menu->CreateBar(_stateBg, NULL, "GUI\\statBar2.png", "");
	_speedBar->SetAlign(gui::Widget::waLeftTop);
	_armorBar->SetAlign(gui::Widget::waLeftTop);
	_damageBar->SetAlign(gui::Widget::waLeftTop);

	_armorBarValue = menu->CreateLabel("", _armorBar, "Small", NullVec2, gui::Text::haRight, gui::Text::vaTop, color2);
	_armorBarValue->SetPos(D3DXVECTOR2(_armorBar->GetSize().x - 4.0f, 1.0f));

	_damageBarValue = menu->CreateLabel("", _damageBar, "Small", NullVec2, gui::Text::haRight, gui::Text::vaTop, color2);
	_damageBarValue->SetPos(D3DXVECTOR2(_damageBar->GetSize().x - 4.0f, 1.0f));

	_speedBarValue = menu->CreateLabel("", _speedBar, "Small", NullVec2, gui::Text::haRight, gui::Text::vaTop, color2);
	_speedBarValue->SetPos(D3DXVECTOR2(_speedBar->GetSize().x - 4.0f, 1.0f));

	_menuItems[miExit] = _raceMenu->CreateMenuButton2(svBack, _bottomPanel, color2, this);
	_menuItems[miExit]->SetAlign(gui::Widget::waLeftTop);

	lsl::string buyButton = "";
	lsl::string buyButtonSel = "";
	const Language* lang = menu->GetLanguageParam();
	if (lang && lang->charset == lcRussian)
	{
		buyButton = "GUI\\buyButton_russian.png";
		buyButtonSel = "GUI\\buyButtonSel_russian.png";
	}
	else
	{
		buyButton = "GUI\\buyButton_english.png";
		buyButtonSel = "GUI\\buyButtonSel_english.png";
	}

	_menuItems[miBuy] = menu->CreateMenuButton(svNull, "Header", buyButton, buyButtonSel, _bottomPanel, this, IdentityVec2, gui::Button::bsSelAnim, color2);
	_menuItems[miBuy]->SetText("");
	_menuItems[miBuy]->SetAlign(gui::Widget::waBottom);

	_leftArrow = _raceMenu->CreateArrow(root(), this);
	_leftArrow->SetAlign(gui::Widget::waLeft);
	_rightArrow = _raceMenu->CreateArrow(root(), this);
	_rightArrow->SetAlign(gui::Widget::waLeft);
	_rightArrow->SetRot(D3DX_PI);

	gui::Widget* labelsParent[cLabelEnd] = {_headerBg, _bottomPanel, _bottomPanel, _moneyBg, _bottomPanel};
	for (int i = 0; i < cLabelEnd; ++i)	
		_labels[i] = menu->CreateLabel(strLabels[i], labelsParent[i], fontLabels[i], NullVec2, horLabels[i], vertLabels[i], colorLabels[i]);	
	_labels[mlCarInfo]->SetWordWrap(true);

	_leftColorGrid = menu->CreateGrid(_leftPanel, NULL, gui::Grid::gsVertical);
	_leftColorGrid->SetAlign(gui::Widget::waLeft);	
	_leftColorGrid->SetRot(-D3DX_PI);
	_leftColorGrid->hideInvisible(false);
	UpdateColorList(_leftColorGrid, Player::cLeftColors, Player::cColorsCount);

	_rightColorGrid = menu->CreateGrid(_rightPanel, NULL, gui::Grid::gsVertical);
	_rightColorGrid->SetAlign(gui::Widget::waRight);
	_rightColorGrid->hideInvisible(false);
	UpdateColorList(_rightColorGrid, Player::cRightColors, Player::cColorsCount);

	_carGrid = menu->CreateGrid(_topPanel, NULL, gui::Grid::gsHorizontal);
	_carGrid->SetAlign(gui::Widget::waLeftTop);	
}

GarageFrame::~GarageFrame()
{
	menu()->UnregNavElements(_menuItems[miExit]);

	for (int i = 0; i < cLabelEnd; ++i)
		menu()->ReleaseWidget(_labels[i]);

	menu()->ReleaseWidget(_leftArrow);
	menu()->ReleaseWidget(_rightArrow);

	menu()->ReleaseWidget(_leftPanel);
	menu()->ReleaseWidget(_rightPanel);
	menu()->ReleaseWidget(_bottomPanel);
	menu()->ReleaseWidget(_topPanel);
}

gui::PlaneFon* GarageFrame::AddCar(gui::Grid* grid, Garage::Car* car, bool locked)
{
	gui::PlaneFon* plane = menu()->CreatePlane(grid, this, !locked ? "GUI\\Cars\\" + car->GetRecord()->GetName() + ".png" : "GUI\\lock.png", true, IdentityVec2, gui::Material::bmTransparency);
	plane->GetMaterial().SetColor(clrWhite);
	plane->SetData(car);

	plane = menu()->CreatePlane(plane, this, "GUI\\carBox.png", true, IdentityVec2, gui::Material::bmAdditive);

	CarData data;
	data.car = car;
	data.locked = locked;
	_cars.push_back(data);

	return plane;
}

void GarageFrame::UpdateCarList(gui::Grid* grid)
{
	_cars.clear();
	grid->DeleteAllChildren();
	D3DXVECTOR2 size = NullVec2;

	Garage::Cars avCars;
	Garage::Cars secretCars;
	Garage::Cars lockCars;

	for (Garage::Cars::const_iterator iter = garage().GetCars().begin(); iter != garage().GetCars().end(); ++iter)
	{
		bool isSecret = true;
		for (Tournament::Planets::const_iterator iterPlanet = tournament().GetPlanets().begin(); iterPlanet != tournament().GetPlanets().end(); ++iterPlanet)
			if ((*iterPlanet)->FindCar(*iter) != (*iterPlanet)->GetCars().end())
			{
				isSecret = false;
				break;
			}

		if (menu()->IsCampaign() && isSecret)
			continue;

		bool hasItem = garage().FindItem(*iter) != garage().GetItems().end();
		bool checkAch = achievment().CheckMapObj((*iter)->GetRecord());

		if (isSecret && checkAch)
			secretCars.push_back(*iter);
		else if (hasItem && checkAch)
			avCars.push_back(*iter);
		else
			lockCars.push_back(*iter);
	}

	struct Pred
	{
		bool operator()(Garage::Car* car1, Garage::Car* car2)
		{
			if (car1->GetCost() < car2->GetCost())
				return true;
			else
				return false;
		}
	};
	//lockCars.sort(Pred());

	for (Garage::Cars::const_iterator iter = avCars.begin(); iter != avCars.end(); ++iter)
	{
		gui::PlaneFon* plane = AddCar(grid, *iter, false);
		size = plane->GetSize();
	}

	for (Garage::Cars::const_iterator iter = secretCars.begin(); iter != secretCars.end(); ++iter)
	{
		gui::PlaneFon* plane = AddCar(grid, *iter, false);
		size = plane->GetSize();
	}

	for (Garage::Cars::const_iterator iter = lockCars.begin(); iter != lockCars.end(); ++iter)
	{
		gui::PlaneFon* plane = AddCar(grid, *iter, true);
		size = plane->GetSize();
	}

	grid->cellSize(size + D3DXVECTOR2(0.0f, 0.0f));
	grid->Reposition();
}

GarageFrame::Cars::const_iterator GarageFrame::FindCar(Garage::Car* car)
{
	for (Cars::const_iterator iter = _cars.begin(); iter != _cars.end(); ++iter)
		if (iter->car == car)
			return iter;

	return _cars.end();
}

void GarageFrame::AdjustCarList()
{
	D3DXVECTOR2 vpSize = uiRoot()->GetVPSize();

	float carGridSpace = 10;
	int totalCars = _carGrid->GetChildren().size();
	int numCars = static_cast<int>((vpSize.x - carGridSpace) / _carGrid->cellSize().x);

	carGridSpace = (vpSize.x - numCars * _carGrid->cellSize().x)/2.0f;
	_carGrid->SetPos(D3DXVECTOR2(-vpSize.x/2 + carGridSpace, 25.0f));

	int selIndex = -1;
	int i = 0;
	bool selVisible = false;
	for (gui::Widget::Children::const_iterator iter = _carGrid->GetChildren().begin(); iter != _carGrid->GetChildren().end(); ++iter, ++i)
		if ((*iter)->GetData() == car())
		{
			selIndex = i;
			selVisible = (*iter)->GetVisible();
			break;
		}

	i = 0;
	if ((_lastCarIndex != selIndex && !selVisible) || _lastCarIndex == -1)
	{
		for (gui::Widget::Children::const_iterator iter = _carGrid->GetChildren().begin(); iter != _carGrid->GetChildren().end(); ++iter, ++i)
		{
			if (_lastCarIndex == -1)
			{
				int indexRight = lsl::ClampValue(selIndex + numCars/2, numCars - 1, totalCars - 1);
				int indexLeft = std::max(indexRight - numCars + 1, 0);
				(*iter)->SetVisible(i >= indexLeft && i <= indexRight);
			}
			else if (selIndex - _lastCarIndex >= 0 && i > selIndex - numCars && i <= std::max(selIndex, numCars - 1))
				(*iter)->SetVisible(true);
			else if (selIndex - _lastCarIndex < 0 && i >= std::min(selIndex, totalCars - numCars) && i < selIndex + numCars)
				(*iter)->SetVisible(true);
			else
				(*iter)->SetVisible(false);
		}
	}

	_lastCarIndex = selIndex;
	_carGrid->Reposition();
}

void GarageFrame::SelectCar(const CarData& carData)
{
	_carData = carData;
	_raceMenu->carFrame()->SetCar(carData.car, player()->GetColor(), carData.locked);	

	Invalidate();
}

void GarageFrame::PrevCar()
{
	Cars::const_iterator iter = FindCar(car());

	if (iter != _cars.begin())	
		SelectCar(*(--iter));
}

void GarageFrame::NextCar()
{
	Cars::const_iterator iter = FindCar(car());

	if (iter != _cars.end() && iter != --_cars.end())
		SelectCar(*(++iter));
}

gui::Widget* GarageFrame::AddColor(gui::Grid* grid, const D3DXCOLOR& color)
{
	gui::Widget* box = menu()->CreateMenuButton("", "", "GUI\\colorBoxBg.png", "GUI\\colorBoxBgSel.png", grid, this, IdentityVec2, gui::Button::bsSelAnim, clrWhite, Menu::ssButton4);

	gui::PlaneFon* plane = menu()->CreatePlane(box, NULL, "GUI\\colorBox.png", true, IdentityVec2, gui::Material::bmOpaque, Menu::ssButton4);
	plane->SetEnabled(false);
	plane->GetMaterial().SetColor(color);	

	return box;
}

void GarageFrame::UpdateColorList(gui::Grid* grid, const D3DXCOLOR colors[], unsigned count)
{
	grid->DeleteAllChildren();
	D3DXVECTOR2 size = NullVec2;

	for (unsigned i = 0; i < count; ++i)
	{
		gui::Widget* plane = AddColor(grid, colors[i]);
		size = plane->GetSize();
	}

	grid->cellSize(size + D3DXVECTOR2(12.0f, 12.0f));
	grid->Reposition();
}

void GarageFrame::RefreshColorList(gui::Grid* grid, const D3DXCOLOR colors[], unsigned count)
{
	gui::Widget::Children::const_iterator iter = grid->GetChildren().begin();

	for (unsigned i = 0; i < count; ++i)
	{
		gui::PlaneFon* plane = static_cast<gui::PlaneFon*>((*iter)->GetChildren().front());
		(*iter)->SetVisible(menu()->IsNetGame() ? net()->player()->CheckColor(plane->GetMaterial().GetColor()) : true);

		++iter;		
	}
}

void GarageFrame::RefreshColorList()
{
	RefreshColorList(_leftColorGrid, Player::cLeftColors, Player::cColorsCount);
	RefreshColorList(_rightColorGrid, Player::cRightColors, Player::cColorsCount);
}

void GarageFrame::SelectColor(const D3DXCOLOR& value)
{
	_raceMenu->carFrame()->SetCarColor(value);
	menu()->SetCarColor(value);
}

void GarageFrame::ShowMessage(StringValue message, gui::Widget* sender, const D3DXVECTOR2& slotSize)
{
	D3DXVECTOR2 cellOffs = slotSize/4;
	cellOffs.y = -cellOffs.y;

	//menu()->ShowMessage(GetString(svWarning), GetString(message), GetString(svOk), sender->GetWorldPos() + cellOffs, gui::Widget::waLeftBottom, 0.0f);
	menu()->ShowMessage(GetString(svWarning), GetString(message), GetString(svOk), uiRoot()->GetVPSize()/2, gui::Widget::waCenter, 0.0f);
}

void GarageFrame::ShowAccept(const std::string& message, gui::Widget* sender, const D3DXVECTOR2& slotSize)
{
	D3DXVECTOR2 cellOffs = slotSize/4;
	cellOffs.y = -cellOffs.y;

	//menu()->ShowAccept(message, GetString(svYes), GetString(svNo), sender->GetWorldPos() + cellOffs, gui::Widget::waLeftBottom, this);
	menu()->ShowAccept(message, GetString(svYes), GetString(svNo), uiRoot()->GetVPSize()/2, gui::Widget::waCenter, this);
}

void GarageFrame::OnShow(bool value)
{
	_lastCarIndex = -1;

	if (value)
	{
		lsl::Vector<Menu::NavElement> navElements;

		UpdateCarList(_carGrid);

		Cars::const_iterator iter = FindCar(car());
		if (iter != _cars.end())
			_carData = *iter;

		int index = 0;
		std::vector<gui::Widget*> gridItemsLeft;
		std::vector<gui::Widget*> gridItemsRight;

		for (gui::Widget::Children::const_iterator iter = _leftColorGrid->GetChildren().begin(); iter != _leftColorGrid->GetChildren().end(); ++iter, ++index)
		{
			gridItemsLeft.push_back(*iter);
			int itemCount = _leftColorGrid->GetChildren().size();

			if (index >= 2)
			{
				Menu::NavElement navElement = {gridItemsLeft[index - 1], {_rightColorGrid->GetChildren().front(), _leftArrow, gridItemsLeft[index - 2], gridItemsLeft[index]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
				navElements.push_back(navElement);
			}	

			if (index + 1 == itemCount)
			{
				if (itemCount >= 2)
				{
					Menu::NavElement navElement = {gridItemsLeft[0], {_rightColorGrid->GetChildren().front(), _leftArrow, _menuItems[miBuy], gridItemsLeft[1]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
					navElements.push_back(navElement);
				}

				Menu::NavElement navElement2 = {gridItemsLeft[itemCount - 1], {_rightColorGrid->GetChildren().front(), _leftArrow, itemCount >= 2 ? gridItemsLeft[itemCount - 2] : _menuItems[miBuy], _menuItems[miBuy]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
				navElements.push_back(navElement2);
			}
		}

		index = 0;
		for (gui::Widget::Children::const_iterator iter = _rightColorGrid->GetChildren().begin(); iter != _rightColorGrid->GetChildren().end(); ++iter, ++index)
		{
			gridItemsRight.push_back(*iter);
			int itemCount = _rightColorGrid->GetChildren().size();

			if (index >= 2)
			{
				Menu::NavElement navElement = {gridItemsRight[index - 1], {_rightArrow, _leftColorGrid->GetChildren().front(), gridItemsRight[index - 2], gridItemsRight[index]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
				navElements.push_back(navElement);
			}	

			if (index + 1 == itemCount)
			{
				if (itemCount >= 2)
				{
					Menu::NavElement navElement = {gridItemsRight[0], {_rightArrow, _leftColorGrid->GetChildren().front(), _menuItems[miBuy], gridItemsRight[1]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
					navElements.push_back(navElement);
				}

				Menu::NavElement navElement2 = {gridItemsRight[itemCount - 1], {_rightArrow, _leftColorGrid->GetChildren().front(), itemCount >= 2 ? gridItemsRight[itemCount - 2] : _menuItems[miBuy], _menuItems[miBuy]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
				navElements.push_back(navElement2);
			}
		}

		{
			Menu::NavElement navElement = {_menuItems[miExit], {_menuItems[miBuy], _menuItems[miBuy], _menuItems[miBuy], _menuItems[miBuy]}, {vkBack, cVirtualKeyEnd}};
			navElements.push_back(navElement);
		}
		{
			Menu::NavElement navElement = {_menuItems[miBuy], {gridItemsLeft[gridItemsLeft.size() - 1], gridItemsRight[gridItemsRight.size() - 1], _leftArrow, _menuItems[miExit]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
			navElements.push_back(navElement);
		}
		{
			Menu::NavElement navElement = {_leftArrow, {gridItemsLeft[0], _rightArrow, _menuItems[miBuy], _menuItems[miBuy]}, {vkShoulderLeft, cVirtualKeyEnd}};
			navElements.push_back(navElement);
		}
		{
			Menu::NavElement navElement = {_rightArrow, {_leftArrow, gridItemsRight[0], _menuItems[miBuy], _menuItems[miBuy]}, {vkShoulderRight, cVirtualKeyEnd}};
			navElements.push_back(navElement);
		}

		menu()->SetNavElements(_menuItems[miExit], true, &navElements[0], navElements.size());
	}
	else
	{
		_cars.clear();
		_carGrid->DeleteAllChildren();
		menu()->UnregNavElements(_menuItems[miExit]);
	}
}

void GarageFrame::OnAdjustLayout(const D3DXVECTOR2& vpSize)
{
	D3DXVECTOR2 size = menu()->GetImageSize(_topPanel->GetMaterial());
	//23.0f
	_topPanel->SetSize(size.x, size.y);
	_topPanel->SetPos(vpSize.x/2, 0.0f);

	_rightPanel->SetPos(D3DXVECTOR2(vpSize.x + 1, (_topPanel->GetSize().y + vpSize.y - _bottomPanel->GetSize().y + 0.0f)/2.0f));
	_leftPanel->SetPos(D3DXVECTOR2(-1, (_topPanel->GetSize().y + vpSize.y - _bottomPanel->GetSize().y + 0.0f)/2.0f));
	_headerBg->SetPos(D3DXVECTOR2(0.0f, _topPanel->GetSize().y - 5));
	_moneyBg->SetPos(D3DXVECTOR2(vpSize.x/2 + 1, -_bottomPanel->GetSize().y + 38));
	_stateBg->SetPos(D3DXVECTOR2(418.0f, -_bottomPanel->GetSize().y + 60));

	_damageBar->SetPos(D3DXVECTOR2(48.0f, 3.0f));
	_armorBar->SetPos(D3DXVECTOR2(48.0f, 41.0f));
	_speedBar->SetPos(D3DXVECTOR2(48.0f, 78.0f));

	_menuItems[miExit]->SetPos(D3DXVECTOR2(-vpSize.x/2, -_bottomPanel->GetSize().y - 8.0f));
	_menuItems[miBuy]->SetPos(D3DXVECTOR2(15.0f, -_bottomPanel->GetSize().y));

	_leftArrow->SetPos(D3DXVECTOR2(_leftPanel->GetSize().x + 45.0f, vpSize.y/2));
	_rightArrow->SetPos(D3DXVECTOR2(vpSize.x - _rightPanel->GetSize().x - 45.0f, vpSize.y/2));

	_leftColorGrid->SetPos(D3DXVECTOR2(-20.0f, 0.0f));
	_rightColorGrid->SetPos(D3DXVECTOR2(-20.0f, 0.0f));	
	
	_labels[mlCarName]->SetPos(D3DXVECTOR2(10.0f, -155.0f));
	_labels[mlMoney]->SetPos(D3DXVECTOR2(-53.0f, -29.0f));
	_labels[mlPrice]->SetPos(D3DXVECTOR2(-523.0f, -80.0f));

	_labels[mlCarInfo]->SetPos(D3DXVECTOR2(0.0f, -60.0f));
	_labels[mlCarInfo]->SetSize(D3DXVECTOR2(800.0f, 105.0f));

	AdjustCarList();
}

void GarageFrame::OnInvalidate()
{
	if (car() == NULL)
	{
		_carData.car = garage().GetCars().front();
		_carData.locked = false;
		_raceMenu->carFrame()->SetCar(_carData.car, player()->GetColor(), _carData.locked);
	}

	LSL_ASSERT(car());

	_raceMenu->carFrame()->SetSlots(NULL, true);

	_labels[mlCarName]->SetText(GetString(!_carData.locked ? car()->GetName() : _SC(svLockedCarName)));
	_labels[mlCarInfo]->SetText(GetString(!_carData.locked ? car()->GetDesc() : _SC(svLockedCarInfo)));
	_labels[mlMoney]->SetText(menu()->FormatCurrency(player()->GetMoney()));
	_labels[mlPrice]->SetText(!_carData.locked ? menu()->FormatCurrency(car()->GetCost()) : "-");

	int carIndex = -1;
	int i = 0;
	for (gui::Widget::Children::const_iterator iter = _carGrid->GetChildren().begin(); iter != _carGrid->GetChildren().end(); ++iter, ++i)
	{
		gui::PlaneFon* carPlane = static_cast<gui::PlaneFon*>(*iter);
		gui::PlaneFon* carBox = static_cast<gui::PlaneFon*>(carPlane->GetChildren().front());

		bool isSel = carPlane->GetData() == car();
		if (isSel)
			carIndex = i;
		carPlane->GetMaterial().SetColor(isSel ? D3DXCOLOR(255.0f, 150.0f, 0.0f, 255.0f)/255.0f : clrWhite);
		carBox->GetMaterial().GetSampler().SetTex(isSel ? GetTexture("GUI\\carBoxSel.png") : GetTexture("GUI\\carBox.png"));
		carBox->GetMaterial().SetBlending(isSel ? gui::Material::bmAdditive : gui::Material::bmTransparency);
	}
	AdjustCarList();

	_leftArrow->SetVisible(carIndex > 0);
	_rightArrow->SetVisible(carIndex < (int)_carGrid->GetChildren().size() - 1);

	menu()->SetButtonEnabled(_menuItems[miExit], player()->GetCar().record != NULL);
	menu()->SetButtonEnabled(_menuItems[miBuy], !_carData.locked);

	_raceMenu->UpdateStats(!_carData.locked ? car() : NULL, NULL, _armorBar, _speedBar, _damageBar, _armorBarValue, _damageBarValue, _speedBarValue);

	RefreshColorList();
}

bool GarageFrame::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
{
	if (sender == _menuItems[miExit])
	{
		_raceMenu->SetState(RaceMenu::msMain);
		return true;
	}

	if (sender == _menuItems[miBuy])
	{
		if (player()->GetCar().record != car()->GetRecord())
		{
			std::string message = GetString(svBuyCar);
			if (HasString(svBuyCar))
				message = lsl::StrFmt(message.c_str(), menu()->FormatCurrency(garage().GetCarCost(car())).c_str());

			if (menu()->IsCampaign())
				ShowAccept(message, sender, sender->GetSize());
			else
			{
				menu()->BuyCar(car());
				_raceMenu->SetState(RaceMenu::msMain);
			}
		}
		else
			_raceMenu->SetState(RaceMenu::msMain);
		return true;
	}

	if (sender == _leftArrow)
	{
		PrevCar();
		return true;
	}

	if (sender == _rightArrow)
	{
		NextCar();
		return true;
	}

	if (sender->GetParent() && (sender->GetParent() == _leftColorGrid || sender->GetParent() == _rightColorGrid))
	{
		SelectColor(D3DXCOLOR(static_cast<gui::PlaneFon*>(sender->GetChildren().front())->GetMaterial().GetColor()));
		return true;
	}

	if (sender == menu()->GetAcceptSender())
	{
		if (menu()->GetAcceptResultYes())
		{	
			if (menu()->BuyCar(car()))
				_raceMenu->SetState(RaceMenu::msMain);
			else
				ShowMessage(svHintCantMoney, _menuItems[miBuy], _menuItems[miBuy]->GetSize());
		}
		return true;
	}

	return false;
}

Garage::Car* GarageFrame::car()
{
	return _raceMenu->carFrame()->GetCar();
}

D3DXCOLOR GarageFrame::color()
{
	return _raceMenu->carFrame()->GetCarColor();
}

void GarageFrame::OnProcessNetEvent(unsigned id, NetEventData* data)
{
	if (id == cNetPlayerSetColor)
		RefreshColorList();
}




WorkshopFrame::WorkshopFrame(Menu* menu, RaceMenu* raceMenu, gui::Widget* parent): MenuFrame(menu, parent), _raceMenu(raceMenu), _goodScroll(0), _overGood(NULL), _overGood2(NULL), _infoId(-1), _numSelectedSlots(0)
{
	const D3DXCOLOR color2 = D3DXCOLOR(214.0f, 214.0f, 214.0f, 255.0f)/255.0f;
	const StringValue menuItemsStr[cMenuItemEnd] = {svBack};

	root()->SetEvent(this);

	StringValue strLabels[cLabelEnd] = {svWorkshop, svNull, svNull};
	std::string fontLabels[cLabelEnd] = {"Header", "Item", "VerySmall"};
	gui::Text::HorAlign horLabels[cLabelEnd] = {gui::Text::haCenter, gui::Text::haRight, gui::Text::haCenter};
	gui::Text::VertAlign vertLabels[cLabelEnd] = {gui::Text::vaCenter, gui::Text::vaCenter, gui::Text::vaTop};
	D3DXCOLOR colorLabels[cLabelEnd] = {color2, clrWhite, color2};

	_topPanel = menu->CreatePlane(root(), this, "GUI\\topPanel3.png", true, IdentityVec2, gui::Material::bmTransparency);
	_topPanel->SetAnchor(gui::Widget::waTop);
	_topPanel->SetAlign(gui::Widget::waTop);

	_bottomPanel = menu->CreatePlane(root(), this, "GUI\\bottomPanel3.png", true, IdentityVec2, gui::Material::bmTransparency);
	_bottomPanel->SetAnchor(gui::Widget::waBottom);
	_bottomPanel->SetAlign(gui::Widget::waBottom);

	_leftPanel = menu->CreatePlane(root(), this, "GUI\\leftPanel3.png", true, IdentityVec2, gui::Material::bmTransparency);
	_leftPanel->SetAlign(gui::Widget::waLeft);	

	_moneyBg = menu->CreatePlane(_bottomPanel, this, "GUI\\moneyBg.png", true, IdentityVec2, gui::Material::bmTransparency);
	_moneyBg->SetAlign(gui::Widget::waRightBottom);	

	_stateBg = menu->CreatePlane(_bottomPanel, this, "GUI\\statFrame2.png", true, IdentityVec2, gui::Material::bmTransparency);
	_stateBg->SetAlign(gui::Widget::waLeftTop);

	_speedBarBonus = menu->CreateBar(_stateBg, NULL, "GUI\\statBar2Plus.png", "");
	_armorBarBonus = menu->CreateBar(_stateBg, NULL, "GUI\\statBar2Plus.png", "");
	_damageBarBonus = menu->CreateBar(_stateBg, NULL, "GUI\\statBar2Plus.png", "");
	_speedBarBonus->SetAlign(gui::Widget::waLeftTop);
	_armorBarBonus->SetAlign(gui::Widget::waLeftTop);
	_damageBarBonus->SetAlign(gui::Widget::waLeftTop);
	_speedBarBonus->SetProgress(0.0f);
	_armorBarBonus->SetProgress(0.0f);
	_damageBarBonus->SetProgress(0.0f);

	_speedBar = menu->CreateBar(_speedBarBonus, NULL, "GUI\\statBar2.png", "");
	_armorBar = menu->CreateBar(_armorBarBonus, NULL, "GUI\\statBar2.png", "");
	_damageBar = menu->CreateBar(_damageBarBonus, NULL, "GUI\\statBar2.png", "");
	_speedBar->SetAlign(gui::Widget::waLeftTop);
	_armorBar->SetAlign(gui::Widget::waLeftTop);
	_damageBar->SetAlign(gui::Widget::waLeftTop);

	_speedBarBonus->SetSize(_speedBar->GetSize());
	_armorBarBonus->SetSize(_armorBar->GetSize());
	_damageBarBonus->SetSize(_damageBar->GetSize());

	_armorBarValue = menu->CreateLabel("", _armorBar, "Small", NullVec2, gui::Text::haRight, gui::Text::vaTop, color2);
	_armorBarValue->SetPos(D3DXVECTOR2(_armorBar->GetSize().x - 4.0f, 1.0f));

	_damageBarValue = menu->CreateLabel("", _damageBar, "Small", NullVec2, gui::Text::haRight, gui::Text::vaTop, color2);
	_damageBarValue->SetPos(D3DXVECTOR2(_damageBar->GetSize().x - 4.0f, 1.0f));

	_speedBarValue = menu->CreateLabel("", _speedBar, "Small", NullVec2, gui::Text::haRight, gui::Text::vaTop, color2);
	_speedBarValue->SetPos(D3DXVECTOR2(_speedBar->GetSize().x - 4.0f, 1.0f));

	_goodGrid = menu->CreateGrid(_leftPanel, this, gui::Grid::gsHorizontal, IdentityVec2, 3);
	_goodGrid->SetAlign(gui::Widget::waLeft);

	_downArrow = _raceMenu->CreateArrow(_leftPanel, this);
	_downArrow->SetRot(-D3DX_PI/2);
	D3DXVECTOR2 size = _downArrow->GetSize();
	_downArrow->SetSize(menu->StretchImage(*_downArrow->GetFon(), D3DXVECTOR2(30.0f, 30.0f), true, true, true, false));
	_downArrow->SetSelSize(menu->GetImageSize(*_downArrow->GetSel()) * _downArrow->GetSize().x / size.x);

	_upArrow = _raceMenu->CreateArrow(_leftPanel, this);
	_upArrow->SetRot(D3DX_PI/2);
	size = _upArrow->GetSize();
	_upArrow->SetSize(menu->StretchImage(*_upArrow->GetFon(), D3DXVECTOR2(30.0f, 30.0f), true, true, true, false));
	_upArrow->SetSelSize(menu->GetImageSize(*_upArrow->GetSel()) * _upArrow->GetSize().x / size.x);

	const std::string slotIcons[Player::cSlotTypeEnd] = {"", "", "", "", "GUI\\hyperSlot.png", "GUI\\mineSlot.png", "GUI\\wpnSlot.png", "GUI\\wpnSlot.png", "GUI\\wpnSlot.png", "GUI\\wpnSlot.png"};

	for (int i = 0; i < Player::cSlotTypeEnd; ++i)
	{
		SlotBox& slot = _slots[i];
		slot.upLevel = 0;
		slot.selected = false;

		slot.plane = menu->CreatePlane(root(), this, "GUI\\slot2.png", true, IdentityVec2, gui::Material::bmTransparency);

		if (slotIcons[i] != "")
			slot.slotIcon = menu->CreatePlane(slot.plane, NULL, slotIcons[i], true, IdentityVec2, gui::Material::bmTransparency);
		else
			slot.slotIcon = NULL;

		slot.viewport = menu->CreateViewPort3d(slot.plane, NULL, "", gui::ViewPort3d::msMouseOverAnim);
		float size = std::min(slot.plane->GetSize().x, slot.plane->GetSize().y);
		slot.viewport->SetSize(size, size);

		D3DXQUATERNION quat;
		D3DXQuaternionRotationAxis(&quat, &ZVector, D3DX_PI/2.0f);
		slot.viewport->SetRot3dSpeed(quat);

		slot.mesh3d = menu->CreateMesh3d(slot.viewport, NULL, NULL);
		slot.mesh3d->GetMaterial()->GetSampler().SetFiltering(graph::BaseSampler::sfAnisotropic);

		slot.level = _raceMenu->CreatePlusButton(slot.plane, this);
		slot.level->SetAlign(gui::Widget::waRightBottom);
		slot.level->SetPos(51.0f, 38.0f);

		slot.chargeBox = menu->CreatePlane(slot.plane, NULL, "GUI\\chargeBox.png", true);
		slot.chargeBox->SetAlign(gui::Widget::waLeftTop);
		slot.chargeBox->SetPos(68.0f, -12.0f);

		slot.chargeBar = menu->CreateBar(slot.chargeBox, NULL, "GUI\\chargeBar.png", "", gui::ProgressBar::psVertical);
		slot.chargeBar->SetAlign(gui::Widget::waLeftBottom);
		slot.chargeBar->SetPos(4.0f, slot.chargeBox->GetSize().y - 5.0f);

		slot.chargeButton = _raceMenu->CreatePlusButton(slot.chargeBox, this);
		slot.chargeButton->SetAlign(gui::Widget::waLeftBottom);
		slot.chargeButton->SetPos(0.0f, -5.0f);
	}

	for (int i = 0; i < cMenuItemEnd; ++i)
	{
		_menuItems[i] = _raceMenu->CreateMenuButton2(menuItemsStr[i], _bottomPanel, color2, this);
		_menuItems[i]->SetAlign(gui::Widget::waLeftTop);
	}

	gui::Widget* labelsParent[cLabelEnd] = {_topPanel, _moneyBg, _bottomPanel};
	for (int i = 0; i < cLabelEnd; ++i)	
		_labels[i] = menu->CreateLabel(strLabels[i], labelsParent[i], fontLabels[i], NullVec2, horLabels[i], vertLabels[i], colorLabels[i]);	
	_labels[mlHint]->SetWordWrap(true);

	_dragItem.viewport = menu->CreateViewPort3d(root(), NULL, "", gui::ViewPort3d::msStatic);
	_dragItem.mesh3d = menu->CreateMesh3d(_dragItem.viewport, NULL, NULL);
	_dragItem.mesh3d->GetMaterial()->GetSampler().SetFiltering(graph::BaseSampler::sfAnisotropic);
	_dragItem.slot = NULL;
}

WorkshopFrame::~WorkshopFrame()
{
	menu()->UnregNavElements(_menuItems[miExit]);

	HideInfo(cUndefInfo);
	SetOverGood(NULL);
	SetOverGood2(NULL);
	StopDrag(true, false);
	menu()->ReleaseWidget(_dragItem.viewport);

	for (int i = 0; i < Player::cSlotTypeEnd; ++i)
		menu()->ReleaseWidget(_slots[i].plane);

	for (int i = 0; i < cLabelEnd; ++i)
		menu()->ReleaseWidget(_labels[i]);

	menu()->ReleaseWidget(_leftPanel);
	menu()->ReleaseWidget(_bottomPanel);
	menu()->ReleaseWidget(_topPanel);
}

gui::ViewPort3d* WorkshopFrame::AddGood(Slot* slot, int index, int count)
{
	gui::ViewPort3d* viewPort = menu()->CreateMesh3dBox(_goodGrid, this, slot != NULL ? slot->GetItem().GetMesh() : NULL, slot != NULL ? slot->GetItem().GetTexture() : NULL, gui::ViewPort3d::msMouseOverAnim);
	viewPort->SetData(slot);

	if (slot)
	{
		gui::Mesh3d* mesh3d = static_cast<gui::Mesh3d*>(viewPort->GetBox()->GetChildren().front());
		mesh3d->GetOrCreateMaterial()->GetSampler().SetFiltering(graph::BaseSampler::sfAnisotropic);

		D3DXQUATERNION quat;
		D3DXQuaternionRotationAxis(&quat, &ZVector, D3DX_PI/2.0f);
		viewPort->SetRot3dSpeed(quat);
		D3DXQuaternionRotationAxis(&quat, &ZVector, 2.0f * D3DX_PI * index / count);
		viewPort->GetBox()->SetRot(quat * viewPort->GetBox()->GetRot());
	}

	gui::PlaneFon* box = menu()->CreatePlane(viewPort, NULL, "GUI\\slot2Frame.png", true, IdentityVec2, gui::Material::bmTransparency);
	float size = std::max(box->GetSize().x, box->GetSize().y);
	viewPort->SetSize(size, size);

	return viewPort;
}

void WorkshopFrame::UpdateGoods()
{
	const int cellsCount = 12;

	SetOverGood(NULL);
	SetOverGood2(NULL);
	_goodGrid->DeleteAllChildren();
	D3DXVECTOR2 size = IdentityVec2;

	struct Pred
	{
		bool operator()(Slot* slot1, Slot* slot2)
		{
			if (slot1->GetItem().GetCost() < slot2->GetItem().GetCost())
				return true;
			else
				return false;
		}
	};
	
	Workshop::Items items = workshop().GetItems();
	items.sort(Pred());

	int i = 0;
	for (Workshop::Items::const_iterator iter = items.begin(); iter != items.end(); ++iter)
	{
		Slot* slot = *iter;

		if (achievment().CheckMapObj(slot->GetRecord()) == false)
			continue;

		gui::ViewPort3d* viewPort = AddGood(slot, i, cellsCount);
		size = viewPort->GetSize();
		++i;
	}

	for (; i < 12; ++i)
	{
		gui::ViewPort3d* viewPort = AddGood(NULL, i, cellsCount);
		size = viewPort->GetSize();
	}

	_goodGrid->cellSize(size + D3DXVECTOR2(22.0f, 22.0f));
}

void WorkshopFrame::AdjustGood()
{
	_goodGrid->SetPos(D3DXVECTOR2(22.0f, 11.0f));

	int i = 0;
	for (gui::Widget::Children::const_iterator iter = _goodGrid->GetChildren().begin(); iter != _goodGrid->GetChildren().end(); ++iter)
	{
		gui::Widget* child = *iter;
		//is disposed
		if (child->disposed())
			continue;

		if (i >= _goodScroll * _goodGrid->maxCellsOnLine() && i < _goodScroll * _goodGrid->maxCellsOnLine() + 12)
			child->SetVisible(true);
		else
			child->SetVisible(false);
		++i;
	}

	_goodGrid->Reposition();

	_upArrow->SetVisible(_goodScroll > 0);
	_downArrow->SetVisible(_goodScroll < std::max((int)ceil(i / (float)_goodGrid->maxCellsOnLine()) - 4, 0));
}

void WorkshopFrame::ScrollGood(int step)
{
	_goodScroll += step;
	AdjustGood();
}

void WorkshopFrame::StartDrag(Slot* slot, Player::SlotType slotType, int chargeCount)
{
	LSL_ASSERT(_dragItem.slot == NULL && slot);

	if (Object::ReplaceRef(_dragItem.slot, slot))
	{
		_dragItem.slot = slot;
		_dragItem.mesh3d->SetMesh(slot->GetItem().GetMesh());
		_dragItem.mesh3d->GetMaterial()->GetSampler().SetTex(slot->GetItem().GetTexture());
		_dragItem.viewport->SetSize(_slots[0].viewport->GetSize());
	}

	_dragItem.slotType = slotType;
	_dragItem.chargeCount = chargeCount;

	UpdateDragPos(menu()->GetControl()->GetMouseVec());
	UpdateSelection();

	HideInfo(cUndefInfo);

	menu()->PlaySound("Sounds\\UI\\pickup_down.ogg");
}

bool WorkshopFrame::StopDrag(bool dropOut, bool intoGood)
{
	if (_dragItem.slot == NULL)
		return true;

	if (dropOut)
	{
		if (!intoGood && _dragItem.slotType != Player::cSlotTypeEnd && player()->GetSlot(_dragItem.slotType) == NULL)
		{
			InstalSlot(_dragItem.slotType, _dragItem.slot, _dragItem.chargeCount);			
		}
		else
		{
			if (menu()->IsCampaign() && _dragItem.slotType != Player::cSlotTypeEnd)
			{
				std::string message = GetString(svSellWeapon);
				if (HasString(svSellWeapon))
					message = lsl::StrFmt(message.c_str(), menu()->FormatCurrency(workshop().GetCostItem(_dragItem.slot, true, _dragItem.chargeCount)).c_str());

				ShowAccept(message, _dragItem.viewport, _dragItem.viewport->GetSize(), _dragItem.slot);
				return false;
			}
			else				
				SellSlot(_dragItem.slot, _dragItem.slotType != Player::cSlotTypeEnd, _dragItem.chargeCount);
		}
	}

	ResetDrag();
	return true;
}

void WorkshopFrame::ResetDrag()
{
	LSL_ASSERT(_dragItem.slot);

	lsl::SafeRelease(_dragItem.slot);
	_dragItem.slot = NULL;
	_dragItem.mesh3d->SetMesh(NULL);

	UpdateSelection();

	menu()->PlaySound("Sounds\\UI\\pickup_up.ogg");
}

bool WorkshopFrame::IsDragItem()
{
	return _dragItem.slot != NULL;
}

void WorkshopFrame::UpdateDragPos(const D3DXVECTOR2& pos)
{
	_dragItem.viewport->SetPos(pos);
}

void WorkshopFrame::SetSlotActive(Player::SlotType type, bool active, bool enabled)
{
	SlotBox& box = _slots[type];

	box.plane->SetVisible(active);

	if (!enabled)
	{
		box.mesh3d->SetActive(false);
		if (box.slotIcon)
			box.slotIcon->SetVisible(true);
		box.chargeBox->SetVisible(false);		
		box.chargeButton->SetVisible(false);
		box.level->SetVisible(false);
	}
}

void WorkshopFrame::UpdateSlot(Player::SlotType type, Slot* slot)
{
	LSL_ASSERT(car());

	SlotBox& box = _slots[type];	
	int wpnSlotIndex = type - Player::stWeapon1;

	if (!car()->GetSlot(type).active || 
		(menu()->IsSkirmish() && wpnSlotIndex >= 0 && wpnSlotIndex < Garage::cWeaponMaxLevel && wpnSlotIndex >= garage().GetWeaponMaxLevel()))
	{
		SetSlotActive(type, false, false);
		return;
	}

	if (slot != NULL)
	{
		SetSlotActive(type, true, true);

		box.mesh3d->SetActive(true);
		box.mesh3d->SetMesh(slot->GetItem().GetMesh());
		box.mesh3d->GetMaterial()->GetSampler().SetTex(slot->GetItem().GetTexture());

		if (box.slotIcon)
			box.slotIcon->SetVisible(box.selected);

		WeaponItem* wpn = slot->GetItem().IsWeaponItem();

		box.chargeBox->SetVisible(wpn != NULL);
		box.chargeButton->SetVisible(wpn != NULL);

		if (wpn)
		{
			bool maxCharge = wpn->GetCntCharge() >= wpn->GetMaxCharge();
			box.chargeButton->SetEnabled(!maxCharge);
			box.chargeButton->Select(maxCharge, true);
		}

		box.level->SetVisible(wpn == NULL);
		
		if (wpn)
		{	
			box.upLevel = wpn->GetCntCharge();
			box.chargeBar->SetProgress(box.upLevel / (float)wpn->GetMaxCharge());			
		}
		else
		{
			box.upLevel = garage().GetUpgradeCarLevel(car(), type, slot->GetRecord());
			int level = lsl::ClampValue(box.upLevel, 0, 2);
			const std::string imgs[3] = {"GUI\\upLevel1.png", "GUI\\upLevel2.png", "GUI\\upLevel3.png"};

			box.level->GetFon()->GetSampler().SetTex(GetTexture(imgs[level]));
			box.level->SetSize(menu()->GetImageSize(*box.level->GetFon()));

			box.level->GetSel()->GetSampler().SetTex(GetTexture(imgs[std::min(level + 1, 2)]));
			box.level->SetSelSize(menu()->GetImageSize(*box.level->GetSel()));

			box.level->SetEnabled(box.upLevel < Garage::cUpgMaxLevel && 
				(menu()->IsCampaign() || box.upLevel < garage().GetUpgradeMaxLevel()));
		}
	}
	else
	{
		SetSlotActive(type, true, false);
	}
}

void WorkshopFrame::UpdateSlots()
{
	for (int i = 0; i < Player::cSlotTypeEnd; ++i)
	{
		Player::SlotType type = (Player::SlotType)i;
		UpdateSlot(type, player()->GetSlotInst(type));
	}
}

bool WorkshopFrame::UpgradeSlot(gui::Widget* sender, Player::SlotType type, int level)
{
	Slot* slot = player()->GetSlotInst(type);
	bool isOk = false;
	D3DXVECTOR2 slotSize = sender->GetSize();

	WeaponItem* weapon = slot ? slot->GetItem().IsWeaponItem() : 0;
	if (weapon)
	{
		if (weapon->GetCntCharge() < weapon->GetMaxCharge())
		{
			isOk = workshop().BuyChargeFor(_raceMenu->GetPlayer(), weapon);
			UpdateSlot(type, slot);
			UpdateMoney();
		}
		else
			isOk = true;

		slotSize *= 4;
	}
	else
	{
		Record* slotRec = garage().GetUpgradeCar(car(), type, level);
		Slot* newSlot = workshop().BuyUpgrade(player(), slotRec);
		if (newSlot)
		{
			garage().InstalSlot(player(), type, car(), newSlot);
			UpdateSlot(type, newSlot);
			UpdateStats();
			UpdateMoney();
			slot = newSlot;
			isOk = true;
		}
	}

	if (!isOk)
	{
		ShowMessage(svHintCantMoney, sender, slotSize);
		return false;
	}

	UpdateSlotInfo(sender, _slots[type], slot, type);

	return true;
}

void WorkshopFrame::SelectSlots(Slot* slot, bool select)
{
	_numSelectedSlots = 0;
	float minDamage = -1.0f;
	Player::SlotType slotType = Player::cSlotTypeEnd;
	
	for (int i = 0; i < Player::cSlotTypeEnd; ++i)
	{
		Player::SlotType type = (Player::SlotType)i;
		bool checkSlot = slot != NULL ? car()->GetSlot(type).FindItem(slot) != NULL : false;
		bool selectSlot = select && checkSlot && !car()->GetSlot(type).lock;
		Slot* plrSlot = player()->GetSlotInst(type);

		if (selectSlot)
			++_numSelectedSlots;

		switch (type)
		{
		case Player::stWeapon1:
		case Player::stWeapon2:
		case Player::stWeapon3:
		case Player::stWeapon4:
			_slots[i].selected = selectSlot;
			_slots[i].slotIcon->GetMaterial().GetSampler().SetTex(GetTexture(selectSlot ? "GUI\\wpnSlotSel.png" : "GUI\\wpnSlot.png"));
			_slots[i].slotIcon->SetSize(menu()->GetImageSize(_slots[i].slotIcon->GetMaterial()));
			_slots[i].slotIcon->SetVisible(selectSlot || plrSlot == NULL);
			break;
		}

		if (checkSlot)
		{			
			WeaponItem* wpn = plrSlot ? plrSlot->GetItem().IsWeaponItem() : NULL;

			if (wpn)
			{
				float dmg = wpn->GetDamage(true);
				if (minDamage > dmg || minDamage == -1.0f)
				{
					minDamage = dmg;
					slotType = type;
				}
			}
			else if (slotType == Player::cSlotTypeEnd || plrSlot == NULL)
			{
				slotType = type;

				if (plrSlot == NULL)
					minDamage = 0.0f;
			}
		}
	}

	UpdateBonusStats(slotType, slot);
}

void WorkshopFrame::UpdateSelection()
{
	if (_dragItem.slot)
		SelectSlots(_dragItem.slot, true);
	else if (_overGood2 && _overGood2->GetData())
		SelectSlots(static_cast<Slot*>(_overGood2->GetData()), true);
	else if (_overGood && _overGood->GetData())
		SelectSlots(static_cast<Slot*>(_overGood->GetData()), true);
	else
		SelectSlots(NULL, false);
}

void WorkshopFrame::SetOverGood(gui::Widget* value)
{
	if (Object::ReplaceRef(_overGood, value))
	{
		_overGood = value;

		if (_overGood && _overGood->GetData())
		{
			Slot* slot = static_cast<Slot*>(_overGood->GetData());
			ShowInfo(slot, value, _goodGrid->cellSize(), cGoodInfo);
		}
		else
		{
			HideInfo(cGoodInfo);
		}

		UpdateSelection();
	}
}

void WorkshopFrame::SetOverGood2(gui::Widget* value)
{
	if (Object::ReplaceRef(_overGood2, value))
	{		
		if (_overGood2)
			lsl::StaticCast<gui::ViewPort3d*>(_overGood2)->SetStyle(_overGood2->GetParent() == _goodGrid ? gui::ViewPort3d::msMouseOverAnim : gui::ViewPort3d::msStatic);

		_overGood2 = value;

		if (_overGood2)
			lsl::StaticCast<gui::ViewPort3d*>(_overGood2)->SetStyle(gui::ViewPort3d::msAnim);

		UpdateSelection();
	}
}

void WorkshopFrame::BuySlot(gui::Widget* sender, Slot* slot)
{
	if (workshop().BuyItem(player(), slot))
	{
		StartDrag(slot, Player::cSlotTypeEnd, -1);
		UpdateGoods();
		AdjustGood();
		UpdateMoney();
	}
	else
	{
		ShowMessage(svHintCantMoney, sender, _goodGrid->cellSize());
	}
}

void WorkshopFrame::SellSlot(Slot* slot, bool sellDiscount, int chargeCount)
{
	workshop().SellItem(player(), slot, sellDiscount, chargeCount);

	UpdateGoods();
	AdjustGood();
	UpdateMoney();
}

void WorkshopFrame::InstalSlot(Player::SlotType type, Slot* slot, int chargeCount)
{
	garage().InstalSlot(player(), type, car(), slot, chargeCount);
	UpdateSlot(type, player()->GetSlotInst(type));
	UpdateStats();
	_raceMenu->carFrame()->SetSlots(player(), false);

}

void WorkshopFrame::UpdateMoney()
{
	_labels[mlMoney]->SetText(menu()->FormatCurrency(player()->GetMoney()));
}

void WorkshopFrame::ShowInfo(Slot* slot, int cost, gui::Widget* sender, const D3DXVECTOR2& slotSize, int infoId)
{
	_infoId = infoId;

	std::string damage = "-";
	WeaponItem* wpn = slot->GetItem().IsWeaponItem();
	if (wpn)
		damage = lsl::StrFmt("%0.0f", wpn->GetDamage());

	D3DXVECTOR2 cellOffs = slotSize/4;
	cellOffs.y = -cellOffs.y;

	menu()->ShowWeaponDialog(GetString(slot->GetItem().GetName()), GetString(slot->GetItem().GetInfo()), menu()->FormatCurrency(cost), damage, sender->GetWorldPos() + cellOffs, gui::Widget::waLeftBottom, 0.0f);
}

void WorkshopFrame::ShowInfo(Slot* slot, gui::Widget* sender, const D3DXVECTOR2& slotSize, int infoId)
{
	ShowInfo(slot, slot->GetItem().GetCost(), sender, slotSize, infoId);
}

void WorkshopFrame::HideInfo(int infoId)
{
	if (infoId != cUndefInfo && _infoId != infoId)
		return;
	_infoId = -1;

	menu()->HideWeaponDialog();
}

bool WorkshopFrame::UpdateSlotInfo(gui::Widget* sender, const SlotBox& slotBox, Slot* slot, Player::SlotType type)
{
	bool isSender = slotBox.level == sender || slotBox.plane == sender || slotBox.chargeButton == sender;
	if (!isSender)
		return false;

	int index = (int)type;	
	int infoId = cSlotInfo + index * 10;

	if (slotBox.chargeButton->IsMouseEnter())
	{
		ShowInfo(slot, slot->GetItem<WeaponItem>().GetChargeCost() * slot->GetItem<WeaponItem>().GetChargeStep(), sender, sender->GetSize() * 4.0f, infoId);
	}
	else if (slotBox.level->IsMouseEnter())
	{
		Record* slotRec = garage().GetUpgradeCar(car(), type, slotBox.upLevel + 1);
		if (slotRec != NULL)
			slot = workshop().FindSlot(slotRec);

		ShowInfo(slot, sender, sender->GetSize(), infoId);

		if (!IsDragItem())
			UpdateBonusStats(type, slot);
	}
	else if (slotBox.plane->IsMouseEnter())
	{
		ShowInfo(slot, sender, sender->GetSize(), infoId);
	}
	else
	{
		HideInfo(infoId);
	}

	if (!IsDragItem() && sender == slotBox.level && !sender->IsMouseEnter())
		UpdateBonusStats(type, NULL);

	return true;
}

void WorkshopFrame::UpdateStats()
{
	_raceMenu->UpdateStats(car(), player(), _armorBar, _speedBar, _damageBar, _armorBarValue, _damageBarValue, _speedBarValue);
}

void WorkshopFrame::UpdateBonusStats(Player::SlotType type, Slot* slot)
{
	if (slot && type != Player::cSlotTypeEnd)
	{
		Slot* slotInst = player()->GetSlotInst(type);
		Slot* curClot = slotInst ? workshop().FindSlot(slotInst->GetRecord()) : NULL;
		int chargeCount = slotInst && slotInst->GetItem().IsWeaponItem() ? slotInst->GetItem().IsWeaponItem()->GetCntCharge() : -1;

		garage().InstalSlot(player(), type, car(), slot);
		_raceMenu->UpdateStats(car(), player(), _armorBarBonus, _speedBarBonus, _damageBarBonus, _armorBarValue, _damageBarValue, _speedBarValue);
		garage().InstalSlot(player(), type, car(), curClot, chargeCount);
	}
	else
	{
		_raceMenu->UpdateStats(NULL, NULL, _armorBarBonus, _speedBarBonus, _damageBarBonus, NULL, NULL, NULL);
		UpdateStats();
	}
}

void WorkshopFrame::ShowMessage(StringValue message, gui::Widget* sender, const D3DXVECTOR2& slotSize)
{
	HideInfo(cUndefInfo);

	D3DXVECTOR2 cellOffs = slotSize/4;
	cellOffs.y = -cellOffs.y;

	menu()->ShowMessage(GetString(svWarning), GetString(message), GetString(svOk), sender->GetWorldPos() + cellOffs, gui::Widget::waLeftBottom, 0.0f);
}

void WorkshopFrame::ShowAccept(const std::string& message, gui::Widget* sender, const D3DXVECTOR2& slotSize, Slot* slot)
{
	HideInfo(cUndefInfo);

	D3DXVECTOR2 cellOffs = slotSize/4;
	cellOffs.y = -cellOffs.y;
	SetOverGood2(sender);

	menu()->ShowAccept(message, GetString(svYes), GetString(svNo), sender->GetWorldPos() + cellOffs, gui::Widget::waLeftBottom, this, slot);
}

void WorkshopFrame::OnShow(bool value)
{
	_goodScroll = 0;

	if (value)
	{
		_raceMenu->carFrame()->SetCamStyle(CarFrame::csSlots, false);
	}
	else
	{
		menu()->UnregNavElements(_menuItems[miExit]);

		HideInfo(cUndefInfo);
		SetOverGood(NULL);
		SetOverGood2(NULL);
		menu()->CarSlotsChanged();
		StopDrag(true, false);

		UpdateSelection();
	}
}

void WorkshopFrame::OnAdjustLayout(const D3DXVECTOR2& vpSize)
{
	_leftPanel->SetPos(D3DXVECTOR2(30, (_topPanel->GetSize().y - 30.0f + vpSize.y - _bottomPanel->GetSize().y)/2));
	_moneyBg->SetPos(D3DXVECTOR2(vpSize.x/2 + 1, -_bottomPanel->GetSize().y + 5));
	//_stateBg->SetPos(D3DXVECTOR2(418.0f, -_bottomPanel->GetSize().y + 25.0f));	
	_stateBg->SetPos(D3DXVECTOR2(vpSize.x/2 - _stateBg->GetSize().x - 15.0f, -_bottomPanel->GetSize().y + 25.0f));

	_damageBarBonus->SetPos(D3DXVECTOR2(48.0f, 3.0f));
	_armorBarBonus->SetPos(D3DXVECTOR2(48.0f, 41.0f));
	_speedBarBonus->SetPos(D3DXVECTOR2(48.0f, 78.0f));

	_upArrow->SetPos(D3DXVECTOR2(_leftPanel->GetSize().x/2, -_leftPanel->GetSize().y/2 + 65.0f));
	_downArrow->SetPos(D3DXVECTOR2(_leftPanel->GetSize().x/2, _leftPanel->GetSize().y/2 - 42.0f));

	AdjustGood();

	for (int i = 0; i < cMenuItemEnd; ++i)
		_menuItems[i]->SetPos(D3DXVECTOR2(-vpSize.x/2, -_bottomPanel->GetSize().y + 40.0f + i * (_menuItems[i]->GetSize().y + 10.0f)));

	_labels[mlHeader]->SetPos(D3DXVECTOR2(20.0f, 40.0f));
	_labels[mlMoney]->SetPos(D3DXVECTOR2(-53.0f, -29.0f));

	_labels[mlHint]->SetPos(D3DXVECTOR2((_menuItems[miExit]->GetPos().x + _menuItems[miExit]->GetSize().x + _stateBg->GetPos().x)/2, -72.0f));
	_labels[mlHint]->SetSize(D3DXVECTOR2(800.0f, 105.0f));
	
	float scale = vpSize.y/720.0f;
	float centerX = vpSize.x * 0.63f;
	float centerY = vpSize.y * 0.451f;
	float leftOffset = std::max(centerX - 330.0f * scale, _leftPanel->GetPos().x + _leftPanel->GetSize().x + 80.0f);
	float rightOffset = std::min(centerX + 330.0f * scale, vpSize.x - 120.0f);
	float topOffset = std::max(centerY - 180.0f * scale, _topPanel->GetSize().y + 65.0f);
	float bottomOffset = std::min(centerY + 180.0f * scale, vpSize.y - _bottomPanel->GetSize().y - 65.0f);

	D3DXVECTOR2 slotSize = _slots[0].plane->GetSize();
	float slotSpace = 15.0f;

	D3DXVECTOR2 slotOffset[Player::cSlotTypeEnd] = {
		D3DXVECTOR2(leftOffset, centerY + slotSize.y/2 + slotSpace), 
		D3DXVECTOR2(leftOffset, centerY - slotSize.y/2 - slotSpace), 
		D3DXVECTOR2(centerX - slotSize.x/2 - 6.0f * slotSpace, bottomOffset), 
		D3DXVECTOR2(centerX + slotSize.x/2 + 6.0f * slotSpace, bottomOffset), 
		D3DXVECTOR2(rightOffset, centerY - slotSize.y/2 - slotSpace), 
		D3DXVECTOR2(rightOffset, centerY + slotSize.y/2 + slotSpace),
		D3DXVECTOR2(centerX - (slotSize.x + 4.5f * slotSpace) * 1.5f, topOffset),
		D3DXVECTOR2(centerX - (slotSize.x + 4.5f * slotSpace) * 0.5f, topOffset),
		D3DXVECTOR2(centerX + (slotSize.x + 4.5f * slotSpace) * 0.5f, topOffset),
		D3DXVECTOR2(centerX + (slotSize.x + 4.5f * slotSpace) * 1.5f, topOffset),
	};

	for (int i = 0; i < Player::cSlotTypeEnd; ++i)
		_slots[i].plane->SetPos(slotOffset[i]);
}

void WorkshopFrame::OnInvalidate()
{
	LSL_ASSERT(car());

	UpdateMoney();
	UpdateGoods();
	UpdateSlots();
	UpdateStats();

	_labels[mlHint]->SetText(GetString(car()->GetDesc()));

	lsl::Vector<Menu::NavElement> navElements;

	gui::Widget* slots[Player::cSlotTypeEnd];
	Player::SlotType lastWeapon = Player::stWeapon1;

	for (int i = 0; i < Player::cSlotTypeEnd; ++i)
	{
		Player::SlotType type = (Player::SlotType)i;

		gui::Button* level = _slots[type].level && _slots[type].level->GetVisible() ? _slots[type].level : NULL;
		gui::Button* chargeButton = _slots[type].chargeButton && _slots[type].chargeButton->GetVisible() ? _slots[type].chargeButton : NULL;
		slots[i] = level ? level : (chargeButton ? chargeButton : _slots[type].level);

		if (chargeButton && type > lastWeapon)
			lastWeapon = type;
	}

	{
		Menu::NavElement navElement = {slots[Player::stArmor], {slots[Player::stWheel], slots[Player::stMotor], slots[Player::stWheel], _menuItems[miExit]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}
	{
		Menu::NavElement navElement = {slots[Player::stMotor], {slots[Player::stArmor], slots[Player::stMine], slots[Player::stMine], _menuItems[miExit]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}
	{
		Menu::NavElement navElement = {slots[Player::stMine], {slots[Player::stMotor], slots[Player::stHyper], slots[Player::stHyper], slots[Player::stMotor]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}
	{
		Menu::NavElement navElement = {slots[Player::stHyper], {slots[lastWeapon], slots[Player::stMine], slots[lastWeapon], slots[Player::stMine]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}
	{
		Menu::NavElement navElement = {slots[Player::stWeapon4], {slots[Player::stWeapon3], slots[Player::stHyper], slots[Player::stWeapon3], slots[Player::stHyper]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}
	{
		Menu::NavElement navElement = {slots[Player::stWeapon3], {slots[Player::stWeapon2], slots[Player::stWeapon4], slots[Player::stWeapon2], slots[Player::stWeapon4]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}
	{
		Menu::NavElement navElement = {slots[Player::stWeapon2], {slots[Player::stWeapon1], slots[Player::stWeapon3], slots[Player::stWeapon1], slots[Player::stWeapon3]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}
	{
		Menu::NavElement navElement = {slots[Player::stWeapon1], {slots[Player::stTruba], slots[Player::stWeapon2], slots[Player::stTruba], slots[Player::stTruba]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}
	{
		Menu::NavElement navElement = {slots[Player::stTruba], {slots[Player::stWheel], slots[Player::stWeapon1], slots[Player::stWeapon1], slots[Player::stWheel]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}
	{
		Menu::NavElement navElement = {slots[Player::stWheel], {slots[Player::stTruba], slots[Player::stArmor], slots[Player::stTruba], slots[Player::stArmor]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}

	{
		Menu::NavElement navElement = {_menuItems[miExit], {slots[Player::stArmor], slots[Player::stArmor], slots[Player::stArmor], slots[Player::stArmor]}, {vkBack, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}

	gui::Widget* focus = NULL;

	for (unsigned i = 0; i < navElements.size(); ++i)
		if (navElements[i].widget->IsFocused())
		{
			focus = navElements[i].widget;
			break;
		}

	menu()->SetNavElements(_menuItems[miExit], true, &navElements[0], navElements.size());

	if (focus)
		focus->SetFocused(true, true);
}

bool WorkshopFrame::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
{
	if (sender == _menuItems[miExit] && !IsDragItem())
	{
		_raceMenu->SetState(RaceMenu::msMain);
		return true;
	}

	if (sender == _downArrow && !IsDragItem())
	{
		ScrollGood(1);
		return true;
	}

	if (sender == _upArrow && !IsDragItem())
	{
		ScrollGood(-1);
		return true;
	}

	if (sender == menu()->GetAcceptSender())
	{
		if (menu()->GetAcceptResultYes())
		{
			if (IsDragItem())
			{
				SellSlot(_dragItem.slot, _dragItem.slotType != Player::cSlotTypeEnd, _dragItem.chargeCount);
				ResetDrag();
			}
			else
			{
				Slot* slot = static_cast<Slot*>(menu()->GetAcceptData());			
				BuySlot(_overGood2, slot);
			}
		}

		SetOverGood2(NULL);
		return true;
	}

	if (sender->GetParent() == _goodGrid)
	{
		Slot* slot = static_cast<Slot*>(sender->GetData());

		if (IsDragItem())
		{
			StopDrag(true, true);
		}
		else if (slot)
		{	
			if (_numSelectedSlots == 0)
			{
				ShowMessage(svHintWeaponNotSupport, sender, _goodGrid->cellSize());
			}
			else if (menu()->IsCampaign())			
			{
				std::string message = GetString(svBuyWeapon);
				if (HasString(svBuyWeapon))
					message = lsl::StrFmt(message.c_str(), menu()->FormatCurrency(slot->GetItem().GetCost()).c_str());

				ShowAccept(message, sender, _goodGrid->cellSize(), slot);
			}
			else
				BuySlot(sender, slot);
		}
		return true;
	}

	for (int i = 0; i < Player::cSlotTypeEnd; ++i)
	{
		Player::SlotType type = (Player::SlotType)i;
		Slot* slot = player()->GetSlotInst(type);
		const Garage::PlaceSlot& place = car()->GetSlot(type);

		Slot* slotShop = slot ? workshop().FindSlot(slot->GetRecord()) : NULL;
		int chargeCount = -1;
		if (slot && slot->GetItem().IsWeaponItem())
			chargeCount = slot->GetItem().IsWeaponItem()->GetCntCharge();

		if (_slots[i].chargeButton == sender && !IsDragItem())
		{
			if (slot)
				UpgradeSlot(sender, type, _slots[i].upLevel + 1);
			return true;
		}

		if (_slots[i].plane == sender)
		{
			if (!place.lock && !(IsDragItem() && place.FindItem(_dragItem.slot) == NULL))
			{
				InstalSlot(type, IsDragItem() ? _dragItem.slot : NULL, _dragItem.chargeCount);
				if (IsDragItem())
					UpdateSlotInfo(sender, _slots[i], _dragItem.slot, type);
				StopDrag(false, false);

				if (slotShop)
					StartDrag(slotShop, type, chargeCount);
			}

			return true;
		}

		if (_slots[i].level == sender && slot && !IsDragItem())
		{		
			UpgradeSlot(sender, type, _slots[i].upLevel + 1);
			return true;
		}
	}

	StopDrag(true, sender == _leftPanel || sender == _goodGrid);

	return false;
}

bool WorkshopFrame::OnMouseDown(gui::Widget* sender, const gui::MouseClick& mClick)
{
	if (sender == root() && mClick.state == ksDown && IsDragItem())
	{
		StopDrag(true, _leftPanel->GetWorldAABB(false).ContainsPoint(mClick.worldCoord));
		return true;
	}

	return false;
}

bool WorkshopFrame::OnMouseMove(gui::Widget* sender, const gui::MouseMove& mMove)
{
	if (sender == root())
	{
		if (_dragItem.slot)
			UpdateDragPos(D3DXVECTOR2(static_cast<float>(mMove.worldCoord.x), static_cast<float>(mMove.worldCoord.y)));
	}

	return false;
}

bool WorkshopFrame::OnMouseEnter(gui::Widget* sender, const gui::MouseMove& mMove)
{
	if (sender->GetParent() == _goodGrid)
	{
		SetOverGood(sender);
		return false;
	}

	for (int i = 0; i < Player::cSlotTypeEnd; ++i)
	{
		Player::SlotType type = (Player::SlotType)i;
		Slot* slot = player()->GetSlot(type) ? workshop().FindSlot(player()->GetSlot(type)) : NULL;
		if (slot == NULL)
			continue;

		if (UpdateSlotInfo(sender, _slots[i], slot, type))
			return false;
	}

	return false;
}

void WorkshopFrame::OnMouseLeave(gui::Widget* sender, bool wasReset)
{
	if (sender == _goodGrid)
	{
		SetOverGood(NULL);
		return;
	}

	for (int i = 0; i < Player::cSlotTypeEnd; ++i)
	{
		Player::SlotType type = (Player::SlotType)i;
		Slot* slot = player()->GetSlotInst(type);
		if (slot == NULL)
			continue;

		if (UpdateSlotInfo(sender, _slots[i], slot, type))
			return;
	}
}

Garage::Car* WorkshopFrame::car()
{
	return _raceMenu->carFrame()->GetCar();
}

void WorkshopFrame::OnProcessEvent(unsigned id, EventData* data)
{
	if (id == cUpgradeMaxLevelChanged ||
		id == cWeaponMaxLevelChanged)
	{
		UpdateSlots();
	}
}




GamersFrame::GamersFrame(Menu* menu, RaceMenu* raceMenu, gui::Widget* parent): MenuFrame(menu, parent), _raceMenu(raceMenu), _planetIndex(-1)
{
	const D3DXCOLOR color2 = D3DXCOLOR(214.0f, 214.0f, 214.0f, 255.0f)/255.0f;	

	StringValue strLabels[cLabelEnd] = {svNull, svNull, svNull};
	std::string fontLabels[cLabelEnd] = {"VerySmall", "Header", "Item"};
	gui::Text::HorAlign horLabels[cLabelEnd] = {gui::Text::haLeft, gui::Text::haCenter, gui::Text::haCenter};
	gui::Text::VertAlign vertLabels[cLabelEnd] = {gui::Text::vaTop, gui::Text::vaCenter, gui::Text::vaCenter};
	D3DXCOLOR colorLabels[cLabelEnd] = {color2, clrWhite, color2};

	_space = menu->CreatePlane(root(), this, "GUI\\space1.dds", true);
	_space->SetAnchor(gui::Widget::waCenter);

	_viewport = menu->CreateViewPort3d(root(), NULL, "", gui::ViewPort3d::msAnim, false);	
	D3DXQUATERNION quat;
	D3DXQuaternionRotationAxis(&quat, &ZVector, D3DX_PI/24.0f);
	_viewport->SetRot3dSpeed(quat);
	D3DXQuaternionRotationAxis(&quat, &XVector, D3DX_PI/2.0f);
	_viewport->GetBox()->SetRot(quat);

	_mesh3d = menu->CreateMesh3d(_viewport, NULL, NULL);
	_mesh3d->GetMaterial()->GetSampler().SetFiltering(graph::BaseSampler::sfAnisotropic);

	_bottomPanel = menu->CreatePlane(root(), this, "GUI\\bottomPanel4.png", true, IdentityVec2, gui::Material::bmTransparency);
	_bottomPanel->SetAnchor(gui::Widget::waBottom);
	_bottomPanel->SetAlign(gui::Widget::waBottom);	

	_photo = menu->CreatePlane(_bottomPanel, this, "", true, IdentityVec2, gui::Material::bmTransparency);
	_photo->SetAlign(gui::Widget::waBottom);
	_photo->GetMaterial().GetSampler().SetFiltering(graph::Sampler2d::sfLinear);

	_photoLight = menu->CreatePlane(_bottomPanel, this, "GUI\\wndLight4.png", true, IdentityVec2, gui::Material::bmTransparency);	

	_leftArrow = _raceMenu->CreateArrow(root(), this);
	_leftArrow->SetAlign(gui::Widget::waRight);
	_leftArrow->SetRot(0);

	_rightArrow = _raceMenu->CreateArrow(root(), this);
	_rightArrow->SetAlign(gui::Widget::waRight);
	_rightArrow->SetRot(D3DX_PI);		

	_nextArrow = _raceMenu->CreateArrow2(_bottomPanel, this);
	_nextArrow->SetAlign(gui::Widget::waLeft);

	gui::Widget* labelsParent[cLabelEnd] = {_bottomPanel, _bottomPanel, _bottomPanel};
	for (int i = 0; i < cLabelEnd; ++i)	
		_labels[i] = menu->CreateLabel(strLabels[i], labelsParent[i], fontLabels[i], NullVec2, horLabels[i], vertLabels[i], colorLabels[i]);	
	_labels[mlInfo]->SetWordWrap(true);
}

GamersFrame::~GamersFrame()
{
	menu()->UnregUser(this);
	menu()->UnregNavElements(_nextArrow);

	menu()->ReleaseWidget(_viewport);
	menu()->ReleaseWidget(_leftArrow);
	menu()->ReleaseWidget(_rightArrow);

	for (int i = 0; i < cLabelEnd; ++i)
		menu()->ReleaseWidget(_labels[i]);

	menu()->ReleaseWidget(_bottomPanel);
	menu()->ReleaseWidget(_space);
}

void GamersFrame::SelectPlanet(int value)
{
	if (_planetIndex != value)
	{
		_planetIndex = value;
		Invalidate();
	}
}

void GamersFrame::PrevPlanet()
{
	SelectPlanet(GetPrevPlanetIndex(_planetIndex));
}

void GamersFrame::NextPlanet()
{
	SelectPlanet(GetNextPlanetIndex(_planetIndex));
}

int GamersFrame::GetNextPlanetIndex(int sIndex)
{
	for (int i = sIndex + 1; i < (int)tournament().GetGamers().size(); ++i)
	{
		int gamerId = tournament().GetGamers()[i]->GetId();

		if (achievment().CheckGamerId(gamerId) && !(menu()->IsNetGame() && !net()->player()->CheckGamerId(gamerId)))
			return i;
	}

	return -1;
}

int GamersFrame::GetPrevPlanetIndex(int sIndex)
{
	for (int i = sIndex - 1; i >= 0; --i)
	{
		int gamerId = tournament().GetGamers()[i]->GetId();

		if (achievment().CheckGamerId(gamerId) && !(menu()->IsNetGame() && !net()->player()->CheckGamerId(gamerId)))
			return i;
	}

	return -1;
}

void GamersFrame::OnAdjustLayout(const D3DXVECTOR2& vpSize)
{
	float planetRadius = (vpSize.y - _bottomPanel->GetSize().y)/2;
	D3DXVECTOR2 planetPos = D3DXVECTOR2(vpSize.x/2 - 25.0f, planetRadius);

	menu()->StretchImage(_space->GetMaterial(), vpSize, true, true);

	_photo->SetPos(D3DXVECTOR2(-30.0f, -18.0f));
	_photoLight->SetPos(D3DXVECTOR2(-30.0f, -100.0f));

	_leftArrow->SetPos(D3DXVECTOR2(planetPos.x - planetRadius - 40.0f + 3.0f, planetPos.y));
	_rightArrow->SetPos(D3DXVECTOR2(planetPos.x + planetRadius + 40.0f + 3.0f, planetPos.y));
	_nextArrow->SetPos(D3DXVECTOR2(400.0f, -100.0f));

	_viewport->SetPos(planetPos);
	planetRadius = std::min(planetRadius, 300.0f);
	_viewport->SetSize(planetRadius * 3.1f, planetRadius * 3.1f);

	_labels[mlName]->SetPos(D3DXVECTOR2(-25.0f, -_bottomPanel->GetSize().y + 23.0f));
	_labels[mlBonus]->SetPos(D3DXVECTOR2(245.0f, -105.0f));

	_labels[mlInfo]->SetPos(D3DXVECTOR2(-390.0f, -100.0f));
	_labels[mlInfo]->SetSize(D3DXVECTOR2(475.0f, 160.0f));
}

void GamersFrame::OnShow(bool value)
{
	if (value)
	{
		menu()->RegUser(this);

		Planet* planet = tournament().GetGamer(player()->GetGamerId());
		_planetIndex = planet ? planet->GetIndex() : GetNextPlanetIndex(-1);
	}
	else
	{
		menu()->UnregUser(this);
	}

	Menu::NavElement elements[] = {
		{_nextArrow, {NULL, NULL, _rightArrow, _rightArrow}, {cVirtualKeyEnd, cVirtualKeyEnd}},
		{_rightArrow, {_leftArrow, _leftArrow, _nextArrow, _nextArrow}, {vkShoulderRight, cVirtualKeyEnd}},
		{_leftArrow, {_rightArrow, _rightArrow, _nextArrow, _nextArrow}, {vkShoulderLeft, cVirtualKeyEnd}}
	};

	menu()->SetNavElements(_nextArrow, value, elements, ARRAY_LENGTH(elements));
}

void GamersFrame::OnInvalidate()
{
	Planet* planet = tournament().GetGamers()[_planetIndex];

	_mesh3d->SetMesh(planet->GetMesh());
	_mesh3d->GetMaterial()->GetSampler().SetTex(planet->GetTexture());

	_photo->GetMaterial().GetSampler().SetTex(planet->GetBoss().photo);
	_photo->SetSize(menu()->StretchImage(_photo->GetMaterial(), D3DXVECTOR2(190.0f, 190.0f), true, false, true, false));

	_leftArrow->SetVisible(GetPrevPlanetIndex(_planetIndex) != -1);
	_rightArrow->SetVisible(GetNextPlanetIndex(_planetIndex) != -1);

	_labels[mlName]->SetText(GetString(planet->GetName()));
	_labels[mlInfo]->SetText(GetString(planet->GetInfo()));
	_labels[mlBonus]->SetText(GetString(planet->GetBoss().bonus));
}

bool GamersFrame::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
{
	if (sender == _nextArrow)
	{
		Planet* planet = tournament().GetGamers()[_planetIndex];		

		if (menu()->IsNetGame())
		{
			menu()->ShowMessageLoading();
			menu()->SetGamerId(planet->GetId());
		}
		else
		{
			menu()->SetGamerId(planet->GetId());

#ifndef _DEBUG
			if (menu()->IsCampaign() && !menu()->GetDisableVideo())
			{
				const Language* lang = menu()->GetLanguageParam();
				if (lang && lang->charset == lcRussian)
					menu()->PlayMovie("Data\\Video\\intaria.avi");
				else
					menu()->PlayMovie("Data\\Video\\intaria_eng.avi");
			}
			else
#endif			
				_raceMenu->SetState(RaceMenu::msGarage);
		}

		return true;
	}

	if (sender == _leftArrow)
	{
		PrevPlanet();
		return true;
	}

	if (sender == _rightArrow)
	{
		NextPlanet();
		return true;
	}

	return false;
}

void GamersFrame::OnProcessEvent(unsigned id, EventData* data)
{
	if (id == cVideoStopped)
	{
		_raceMenu->SetState(RaceMenu::msGarage);
	}
}

void GamersFrame::OnProcessNetEvent(unsigned id, NetEventData* data)
{
	switch (id)
	{
	case cNetPlayerSetGamerId:
		if (data->sender == netPlayer()->id())
		{
			menu()->HideMessage();

			if (data->failed)
			{
				menu()->ShowMessage(GetString(svWarning), GetString(svHintSetGamerFailed), GetString(svOk), uiRoot()->GetVPSize()/2, gui::Widget::waCenter, 0.0f);
			}
			else
				_raceMenu->SetState(RaceMenu::msGarage);
		}
		break;
	}
}




AngarFrame::AngarFrame(Menu* menu, RaceMenu* raceMenu, gui::Widget* parent): MenuFrame(menu, parent), _raceMenu(raceMenu), _planetIndex(-1), _planetPrevIndex(-1), _doorTime(-1.0f)
{
	const D3DXCOLOR color = D3DXCOLOR(0xff76cef2);
	const D3DXCOLOR color2 = D3DXCOLOR(214.0f, 214.0f, 214.0f, 255.0f)/255.0f;
	const StringValue menuItemsStr[cMenuItemEnd] = {svBack};

	StringValue strLabels[cLabelEnd] = {svNull, svNull, svNull};
	std::string fontLabels[cLabelEnd] = {"Item", "Small", "VerySmallThink"};
	gui::Text::HorAlign horLabels[cLabelEnd] = {gui::Text::haCenter, gui::Text::haCenter, gui::Text::haLeft};
	gui::Text::VertAlign vertLabels[cLabelEnd] = {gui::Text::vaCenter, gui::Text::vaCenter, gui::Text::vaTop};
	D3DXCOLOR colorLabels[cLabelEnd] = {clrWhite, color, color};

	_bottomPanelBg = menu->CreateDummy(root(), this);
	_bottomPanelBg->SetAlign(gui::Widget::waBottom);

	_planetGridBg = menu->CreateGrid(_bottomPanelBg, NULL, gui::Grid::gsHorizontal);
	_planetGridBg->SetAlign(gui::Widget::waLeft);

	_bottomPanel = menu->CreatePlane(_bottomPanelBg, this, "GUI\\bottomPanel6.png", true, IdentityVec2, gui::Material::bmTransparency);	
	_bottomPanel->SetAlign(gui::Widget::waBottom);

	_planetGrid = menu->CreateGrid(_bottomPanel, NULL, gui::Grid::gsHorizontal);
	_planetGrid->SetAlign(gui::Widget::waLeft);

	_planetInfo = menu->CreatePlane(_bottomPanel, this, "GUI\\planetInfo.png", true, IdentityVec2, gui::Material::bmTransparency);	
	_planetInfo->SetAlign(gui::Widget::waBottom);

	_planetInfoClose = menu->CreateMenuButton(svNull, "", "GUI\\buttonBg6.png", "GUI\\buttonBgSel6.png", _planetInfo, this, IdentityVec2, gui::Button::bsSelAnim, clrWhite);

	_planetBoss = menu->CreatePlane(_planetInfo, this, "", true, IdentityVec2, gui::Material::bmTransparency);	

	_planetBossCar = menu->CreateViewPort3d(_planetInfo, NULL, "", gui::ViewPort3d::msAnim);	
	D3DXQUATERNION quat;
	D3DXQuaternionRotationAxis(&quat, &ZVector, D3DX_PI/2.0f);
	_planetBossCar->SetRot3dSpeed(quat);

	for (int i = 0; i < cMenuItemEnd; ++i)
	{
		_menuItems[i] = _raceMenu->CreateMenuButton2(menuItemsStr[i], root(), color2, this);
		_menuItems[i]->SetAlign(gui::Widget::waLeftTop);
	}

	gui::Widget* labelsParent[cLabelEnd] = {_planetInfo, _planetInfo, _planetInfo};
	for (int i = 0; i < cLabelEnd; ++i)	
		_labels[i] = menu->CreateLabel(strLabels[i], labelsParent[i], fontLabels[i], NullVec2, horLabels[i], vertLabels[i], colorLabels[i]);
	_labels[mlBossInfo]->SetAlign(gui::Widget::waLeftTop);
	_labels[mlBossInfo]->SetWordWrap(true);	
	_labels[mlBossInfo]->SetFlag(gui::Widget::wfClientClip, true);
}

AngarFrame::~AngarFrame()
{
	menu()->UnregUser(this);
	menu()->UnregNavElements(_menuItems[miExit]);

	for (int i = 0; i < cMenuItemEnd; ++i)
		menu()->ReleaseWidget(_menuItems[i]);

	for (int i = 0; i < cLabelEnd; ++i)
		menu()->ReleaseWidget(_labels[i]);

	menu()->ReleaseWidget(_bottomPanelBg);
}

AngarFrame::PlanetBox& AngarFrame::AddPlanet(Planet* planet, int index)
{
	Race* race = menu()->GetRace();
	PlanetBox* box = (unsigned)index < _planets.size() ? &(_planets[index]) : NULL;

	if (box == NULL)
	{
		_planets.push_back(PlanetBox());
		box = &_planets.back();

		box->viewport = menu()->CreateMesh3dBox(_planetGrid, this, planet->GetMesh(), planet->GetTexture(), gui::ViewPort3d::msStatic, Menu::ssButton5);
		D3DXQUATERNION quat;
		D3DXQuaternionRotationAxis(&quat, &ZVector, D3DX_PI/24.0f);
		box->viewport->SetRot3dSpeed(quat);
		D3DXQuaternionRotationAxis(&quat, &XVector, D3DX_PI/2.0f);
		box->viewport->GetBox()->SetRot(quat);
		box->viewport->SetSize(D3DXVECTOR2(180.0f, 180.0f));
		gui::Mesh3d* mesh3d = static_cast<gui::Mesh3d*>(box->viewport->GetBox()->GetChildren().front());
		mesh3d->GetMaterial()->GetSampler().SetFiltering(graph::BaseSampler::sfAnisotropic);

		box->slot = menu()->CreateMenuButton(svNull, "Item", "GUI\\doorSlot.png", "GUI\\doorSlotSel.png", _planetGridBg, this, IdentityVec2, gui::Button::bsSelAnim, clrWhite);

		box->slotClip = menu()->CreateDummy(box->slot, NULL);
		box->slotClip->SetFlag(gui::Widget::wfClientClip, true);
		box->slotClip->SetSize(box->slot->GetSize() - D3DXVECTOR2(8.0f, 8.0f));

		box->doorDown = menu()->CreatePlane(box->slotClip, NULL, "GUI\\doorDown.png", true, IdentityVec2, gui::Material::bmTransparency);
		box->doorDown->SetAlign(gui::Widget::waTop);

		box->doorUp = menu()->CreatePlane(box->slotClip, NULL, "GUI\\doorUp.png", true, IdentityVec2, gui::Material::bmTransparency);			
		box->doorUp->SetAlign(gui::Widget::waBottom);
	}

	if ((planet->GetState() == Planet::psOpen) || (race->GetPlanetChampion() && tournament().GetNextPlanet() == planet))
	{
		box->slot->SetText(GetString(svOpen));
	}	
	else if (planet->GetState() == Planet::psClosed)
	{
		box->slot->SetText(lsl::StrFmt(GetString(svRequestPoints).c_str(), planet->GetRequestPoints(planet->GetPass())));
	}
	else if (planet->GetState() == Planet::psUnavailable)
	{
		if (race->IsCampaign() && tournament().GetNextPlanet() == planet)
			box->slot->SetText(GetString(svUnavailableTitulA));
		else // if (race->IsCampaign())
			box->slot->SetText(GetString(svClosed));
		//else
		//	box->slot->SetText(GetString(svUnavailable));
	}
	else if (planet->GetState() == Planet::psCompleted)
	{
		box->slot->SetText(GetString(svCompleted));
	}		

	return *box;
}

void AngarFrame::UpdatePlanets()
{
	unsigned index = 0;

	for (Tournament::Planets::const_iterator iter = tournament().GetPlanets().begin(); iter != tournament().GetPlanets().end(); ++iter, ++index)
	{
		AddPlanet(*iter, index);
	}

	if (index < _planets.size())
	{
		for (unsigned i = index; i < _planets.size(); ++i)
		{	
			menu()->ReleaseWidget(_planets[i].slot);
			menu()->ReleaseWidget(_planets[i].viewport);
		}

		_planets.erase(_planets.begin() + index, _planets.end());
	}

	_planetGrid->cellSize(D3DXVECTOR2(224.0f, 130.0f));
	_planetGrid->Reposition();

	_planetGridBg->cellSize(_planetGrid->cellSize());
	_planetGridBg->Reposition();
}

void AngarFrame::SelectPlanet(int index)
{
	if (_planetIndex != index)
	{
		if (_planetPrevIndex != -1)
			SetDoorPos(_planets[_planetPrevIndex], 0.0f);

		_planetPrevIndex = _planetIndex;
		_planetIndex = index;
		_doorTime = 0.0f;

		Invalidate();
	}
}

void AngarFrame::SetDoorPos(PlanetBox& planet, float alpha)
{
	if (menu()->IsNetGame() && net()->isClient())
		alpha = 0.0f;

	float doorOffset = 16.0f * alpha;
	planet.doorDown->SetPos(D3DXVECTOR2(-1.0f, -4.0f + doorOffset));
	planet.doorUp->SetPos(D3DXVECTOR2(-1.0f, 4.0f - doorOffset));
}

void AngarFrame::ShowMessage(StringValue message, gui::Widget* sender, const D3DXVECTOR2& slotSize)
{
	D3DXVECTOR2 cellOffs = slotSize/2;
	cellOffs.y = -cellOffs.y;
	cellOffs.x = 0;

	menu()->ShowMessage(GetString(svWarning), GetString(message), GetString(svOk), sender->GetWorldPos() + cellOffs, gui::Widget::waBottom, 0.0f);
}

void AngarFrame::ShowAccept(const std::string& message, gui::Widget* sender, const D3DXVECTOR2& slotSize, lsl::Object* data)
{
	D3DXVECTOR2 cellOffs = slotSize/2;
	cellOffs.y = -cellOffs.y;
	cellOffs.x = 0;

	menu()->ShowAccept(message, GetString(svYes), GetString(svNo), (sender ? sender->GetWorldPos() : uiRoot()->GetVPSize()/2) + cellOffs, gui::Widget::waBottom, this, data);
}

void AngarFrame::OnShow(bool value)
{
	if (value)
	{
		bool selectCurPlanet = menu()->GetRace()->GetPlanetChampion() && tournament().GetNextPlanet() != NULL;

		if (selectCurPlanet)
			_planetIndex = tournament().GetCurPlanetIndex() + 1;
		else
			_planetIndex = -1;

		_planetPrevIndex = -1;
		_doorTime = -1.0f;

		UpdatePlanets();

		lsl::Vector<Menu::NavElement> navElements;
		unsigned count = _planets.size();

		for (unsigned i = 0; i < count; ++i)
		{
			PlanetBox planetBox = _planets[i];

			if (i + 1 == count)
			{
				if (count >= 2)
				{
					Menu::NavElement navElement = {_planets[0].viewport, {_planets[i].viewport, _planets[1].viewport, _menuItems[miExit], _menuItems[miExit]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
					navElements.push_back(navElement);

					Menu::NavElement navElement2 = {_planets[0].slot, {_planets[i].viewport, _planets[1].viewport, _menuItems[miExit], _menuItems[miExit]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
					navElements.push_back(navElement2);
				}

				{
					Menu::NavElement navElement = {_planets[i].viewport, {count >= 2 ? _planets[i - 1].viewport : NULL, count >= 2 ? _planets[0].viewport : NULL, _menuItems[miExit], _menuItems[miExit]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
					navElements.push_back(navElement);

					Menu::NavElement navElement2 = {_planets[i].slot, {count >= 2 ? _planets[i - 1].viewport : NULL, count >= 2 ? _planets[0].viewport : NULL, _menuItems[miExit], _menuItems[miExit]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
					navElements.push_back(navElement2);
				}
			}

			if (i >= 2)
			{
				Menu::NavElement navElement = {_planets[i - 1].viewport, {_planets[i - 2].viewport, _planets[i].viewport, _menuItems[miExit], _menuItems[miExit]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
				navElements.push_back(navElement);

				Menu::NavElement navElement2 = {_planets[i - 1].slot, {_planets[i - 2].viewport, _planets[i].viewport, _menuItems[miExit], _menuItems[miExit]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
				navElements.push_back(navElement2);
			}
		}

		{
			Menu::NavElement navElement = {_menuItems[miExit], {NULL, NULL, _planets[0].viewport, _planets[0].viewport}, {vkBack, cVirtualKeyEnd}};
			navElements.push_back(navElement);
		}

		menu()->SetNavElements(_menuItems[miExit], true, &navElements[0], navElements.size());

		if (selectCurPlanet)
			_planets[_planetIndex].slot->SetFocused(true, true);

		menu()->RegUser(this);
	}
	else
	{
		menu()->UnregUser(this);

		menu()->UnregNavElements(_menuItems[miExit]);
	}
}

void AngarFrame::OnAdjustLayout(const D3DXVECTOR2& vpSize)
{
	_bottomPanelBg->SetPos(vpSize.x/2, vpSize.y - 20.0f);	
	_planetGrid->SetPos(D3DXVECTOR2(-_bottomPanel->GetSize().x/2 + 35.0f, -_bottomPanel->GetSize().y/2));
	_planetGridBg->SetPos(_planetGrid->GetPos() + D3DXVECTOR2(0.0f, -124.0f));

	if (_planetIndex != -1)
	{
		D3DXVECTOR2 pos = _planets[_planetIndex].slot->GetPos() + D3DXVECTOR2(0.0f, -37.0f) + _planetGridBg->GetPos();
		pos.x = lsl::ClampValue(pos.x, -vpSize.x/2 + _planetInfo->GetSize().x/2, -vpSize.x/2 + vpSize.x - _planetInfo->GetSize().x/2);
		_planetInfo->SetPos(pos);
	}
	_planetInfoClose->SetPos(D3DXVECTOR2(180.0f, -265.0f));
	_planetBoss->SetPos(D3DXVECTOR2(107.0f, -191.0f));

	_planetBossCar->SetPos(D3DXVECTOR2(107.0f, -78.0f));
	_planetBossCar->SetSize(D3DXVECTOR2(110.0f, 110.0f));

	_labels[mlPlanetName]->SetPos(D3DXVECTOR2(0, -269.0f));
	_labels[mlBossName]->SetPos(D3DXVECTOR2(-60.0f, -238.0f));
	_labels[mlBossInfo]->SetPos(D3DXVECTOR2(-_planetInfo->GetSize().x/2 + 15, -222.0f));
	_labels[mlBossInfo]->SetSize(225.0f, 200.0f);

	for (int i = 0; i < cMenuItemEnd; ++i)
		_menuItems[i]->SetPos(D3DXVECTOR2(0.0f, 30.0f + i * (_menuItems[i]->GetSize().y + 10.0f)));
}

void AngarFrame::OnInvalidate()
{
	if (_planetIndex != -1)
	{
		Planet* planet = tournament().GetPlanet(_planetIndex);	

		_planetInfo->SetVisible(true);

		_planetBoss->GetMaterial().GetSampler().SetTex(planet->GetBoss().photo);
		_planetBoss->SetSize(menu()->StretchImage(_planetBoss->GetMaterial(), D3DXVECTOR2(110.0f, 85.0f), true, false, true, false));

		if (planet->GetBoss().cars.size() > 0)
			_raceMenu->CreateCar(_planetBossCar, garage().FindCar(planet->GetBoss().cars.front().record), clrWhite, NULL);
		else
			_raceMenu->CreateCar(_planetBossCar, NULL);

		_labels[mlPlanetName]->SetText(GetString(planet->GetName()));
		_labels[mlBossName]->SetText(GetString(planet->GetBoss().name));
		_labels[mlBossInfo]->SetText(GetString(planet->GetInfo()));
	}
	else
		_planetInfo->SetVisible(false);

	for (unsigned index = 0; index < _planets.size(); ++index)
	{
		PlanetBox& box = _planets[index];

		box.viewport->SetStyle(index == _planetIndex ? gui::ViewPort3d::msAnim : gui::ViewPort3d::msMouseOverAnim);
		box.slot->SetEnabled(index == _planetIndex && !(menu()->IsNetGame() && net()->isClient()));
		if (!box.slot->GetEnabled())
			box.slot->SetFocused(false, true);
		SetDoorPos(box, index == _planetIndex ? 1.0f : 0.0f);
	}
}

bool AngarFrame::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
{
	if (sender == _menuItems[miExit])
	{
		if (menu()->GetRace()->GetPlanetChampion())
			ShowAccept(GetString(svYouReadyStayPlanet), NULL, NullVec2, &tournament().GetCurPlanet());
		else
			_raceMenu->SetState(RaceMenu::msMain);
		return true;
	}

	if (sender == _planetInfoClose)
	{
		SelectPlanet(-1);
		return true;
	}

	if (sender == menu()->GetAcceptSender())
	{
		if (menu()->GetAcceptResultYes())
		{
			Planet* planet = lsl::StaticCast<Planet*>(menu()->GetAcceptData());
			bool newPlanet = planet->GetState() == Planet::psClosed || planet->GetState() == Planet::psUnavailable;
			menu()->ChangePlanet(planet);

			if (newPlanet)
			{	
#ifndef _DEBUG
				if (!menu()->IsNetGame() && menu()->IsCampaign() && !menu()->GetDisableVideo())
				{
					const Language* lang = menu()->GetLanguageParam();
					if (lang && lang->charset == lcRussian)
						menu()->PlayMovie("Data\\Video\\" + planet->GetName() + ".avi");
					else
						menu()->PlayMovie("Data\\Video\\" + planet->GetName() + "_eng.avi");
				}
				else
#endif
				{
#ifdef STEAM_SERVICE
					if (steamService()->isInit() && menu()->IsCampaign())
						steamService()->steamStats()->UnlockAchievment(SteamStats::atWhereNoManHasGoneBefore);
#endif

					_raceMenu->SetState(RaceMenu::msMain);
				}
			}
			else
				_raceMenu->SetState(RaceMenu::msMain);
		}

		return true;
	}

	for (unsigned i = 0; i < _planets.size(); ++i)
	{
		PlanetBox planetBox = _planets[i]; 

		if (sender == planetBox.viewport)
		{
			if (_planetIndex == i)
				SelectPlanet(-1);
			else
				SelectPlanet(i);
			return true;
		}

		if (sender == planetBox.slot && i == _planetIndex)
		{
			Planet* planet = tournament().GetPlanet(i);
			Race* race = menu()->GetRace();	
			bool isCurrent = tournament().GetCurPlanetIndex() == i;

			if (race->GetPlanetChampion())
			{
				if (isCurrent)
				{
					ShowAccept(GetString(svYouReadyStayPlanet), sender, sender->GetSize(), planet);
					return true;
				}
				else if (planet == tournament().GetNextPlanet())
				{
					ShowAccept(GetString(svYouReadyFlyPlanet), sender, sender->GetSize(), planet);
					return true;
				}
			}
			
			if (!menu()->IsCampaign() && planet->GetState() == Planet::psOpen)
			{
				menu()->ChangePlanet(planet);
				_raceMenu->SetState(RaceMenu::msMain);
			}
			else if (isCurrent)
				_raceMenu->SetState(RaceMenu::msMain);
			else
				ShowMessage(svHintCantFlyPlanet, sender, sender->GetSize());

			return true;
		}
	}

	return false;
}

void AngarFrame::OnFocusChanged(gui::Widget* sender)
{
	if (sender->IsFocused())
	{
		for (unsigned i = 0; i < _planets.size(); ++i)
		{
			PlanetBox planetBox = _planets[i]; 		

			if (sender == planetBox.viewport)
			{
				SelectPlanet(i);
				planetBox.viewport->SetFocused(false);
				planetBox.slot->SetFocused(true);
				return;
			}
		}

		if (_menuItems[miExit] == sender)
			SelectPlanet(-1);
	}
}

void AngarFrame::OnProcessEvent(unsigned id, EventData* data)
{
	if (id == cVideoStopped)
	{
#ifdef STEAM_SERVICE
		if (steamService()->isInit() && menu()->IsCampaign())
			steamService()->steamStats()->UnlockAchievment(SteamStats::atWhereNoManHasGoneBefore);
#endif

		_raceMenu->SetState(RaceMenu::msMain);
	}
}

void AngarFrame::OnProgress(float deltaTime)
{
	if (_doorTime >= 0.0f)
	{
		float alpha = lsl::ClampValue(_doorTime / 0.25f, 0.0f, 1.0f);
		_doorTime += deltaTime;

		if (_planetIndex != -1)
			SetDoorPos(_planets[_planetIndex], alpha);
		if (_planetPrevIndex != -1)
			SetDoorPos(_planets[_planetPrevIndex], 1.0f - alpha);

		if (alpha == 1.0f)
			_doorTime = -1.0f;
	}
}




AchievmentFrame::AchievmentFrame(Menu* menu, RaceMenu* raceMenu, gui::Widget* parent): MenuFrame(menu, parent), _raceMenu(raceMenu)
{
	const D3DXCOLOR color = D3DXCOLOR(0xFFFA5800);
	const D3DXCOLOR color2 = clrWhite;
	const StringValue menuItemsStr[cMenuItemEnd] = {svBack};

	StringValue strLabels[cLabelEnd] = {svNull, svRewards};
	std::string fontLabels[cLabelEnd] = {"Header", "Header"};
	gui::Text::HorAlign horLabels[cLabelEnd] = {gui::Text::haCenter, gui::Text::haCenter};
	gui::Text::VertAlign vertLabels[cLabelEnd] = {gui::Text::vaCenter, gui::Text::vaCenter};
	D3DXCOLOR colorLabels[cLabelEnd] = {color, color2};

	_bottomPanel = menu->CreatePlane(root(), this, "GUI\\achievmentBottomPanel.png", true, IdentityVec2, gui::Material::bmTransparency);	
	_bottomPanel->SetAlign(gui::Widget::waBottom);	
	_bottomPanel->GetMaterial().GetSampler().SetFiltering(graph::Sampler2d::sfLinear);

	_bg = menu->CreatePlane(root(), this, "GUI\\achievmentBg.dds", true, IdentityVec2, gui::Material::bmTransparency);
	_bg->SetAlign(gui::Widget::waCenter);
	_bg->SetAnchor(gui::Widget::waCenter);
	_bg->GetMaterial().GetSampler().SetFiltering(graph::Sampler2d::sfLinear);

	_panel = menu->CreatePlane(root(), this, "GUI\\achievmentPanel.png", true, IdentityVec2, gui::Material::bmTransparency);	
	_panel->GetMaterial().GetSampler().SetFiltering(graph::Sampler2d::sfLinear);
	_panel->SetAlign(gui::Widget::waTop);
	_panel->SetAnchor(gui::Widget::waTop);	

	for (int i = 0; i < cMenuItemEnd; ++i)
	{
		_menuItems[i] = _raceMenu->CreateMenuButton2(menuItemsStr[i], root(), color2, this);
		_menuItems[i]->SetAlign(gui::Widget::waLeftTop);
	}

	gui::Widget* labelsParent[cLabelEnd] = {_bottomPanel, _panel};
	for (int i = 0; i < cLabelEnd; ++i)	
		_labels[i] = menu->CreateLabel(strLabels[i], labelsParent[i], fontLabels[i], NullVec2, horLabels[i], vertLabels[i], colorLabels[i]);
}

AchievmentFrame::~AchievmentFrame()
{
	menu()->UnregNavElements(_menuItems[miExit]);

	for (int i = 0; i < cMenuItemEnd; ++i)
		menu()->ReleaseWidget(_menuItems[i]);

	for (int i = 0; i < cLabelEnd; ++i)
		menu()->ReleaseWidget(_labels[i]);

	menu()->ReleaseWidget(_bottomPanel);
	menu()->ReleaseWidget(_panel);
	menu()->ReleaseWidget(_bg);
}

const AchievmentFrame::AchievmentBox* AchievmentFrame::AddAchievment(unsigned index, const std::string& lockImg, const std::string& img, const D3DXVECTOR2& pos, Achievment* model)
{
	LSL_ASSERT(model);

	AchievmentBox* box = index < _achievments.size() ? &_achievments[index] : NULL;;
	if (box == NULL)
	{
		AchievmentBox newBox;		
		newBox.image = menu()->CreatePlane(_panel, this, "", false, IdentityVec2, gui::Material::bmTransparency);
		newBox.image->GetMaterial().GetSampler().SetFiltering(graph::Sampler2d::sfLinear);

		newBox.price = menu()->CreateLabel(svNull, newBox.image, "Item", NullVec2, gui::Text::haCenter, gui::Text::vaCenter, D3DXCOLOR(0xFFC3C2C0));
		newBox.button = menu()->CreateMenuButton(svNull, "", "", "", newBox.image, this, IdentityVec2, gui::Button::bsSelAnim);		

		_achievments.push_back(newBox);
		box = &_achievments.back();
	}

	box->model = model;	

	box->image->SetData(model);	
	box->image->GetMaterial().GetSampler().SetTex(GetTexture(model->state() == Achievment::asOpened ? img : lockImg));
	box->image->SetSize(menu()->GetImageSize(box->image->GetMaterial()));
	box->image->SetPos(pos);

	box->button->SetData(model);
	box->button->SetEnabled(model->state() == Achievment::asUnlocked);

	box->price->SetVisible(model->state() == Achievment::asUnlocked);
	box->price->SetText(lsl::StrFmt("%d", model->price()));
	box->price->SetPos(D3DXVECTOR2(0.0f, box->image->GetSize().y/2 - 25.0f));

	if (model->state() == Achievment::asLocked)
		box->button->GetOrCreateFon()->GetSampler().SetTex(GetTexture("GUI\\closeBut.png"));
	else if (model->state() == Achievment::asUnlocked)
		box->button->GetOrCreateFon()->GetSampler().SetTex(GetTexture("GUI\\okBut.png"));
	else
		box->button->GetOrCreateFon()->GetSampler().SetTex(GetTexture("GUI\\okButSel.png"));
	
	box->button->GetFon()->GetSampler().SetFiltering(graph::Sampler2d::sfLinear);
	box->button->GetFon()->SetBlending(gui::Material::bmTransparency);
	box->button->SetSize(menu()->GetImageSize(*box->button->GetFon()));

	if (model->state() == Achievment::asUnlocked)
	{
		box->button->GetOrCreateSel()->GetSampler().SetTex(GetTexture("GUI\\okButSel.png"));
		box->button->GetOrCreateSel()->SetBlending(gui::Material::bmTransparency);
		box->button->GetSel()->GetSampler().SetFiltering(graph::Sampler2d::sfLinear);
		box->button->SetSelSize(menu()->GetImageSize(*box->button->GetSel()));
	}
	else
		box->button->SetSel(NULL);

	box->button->SetPos(box->image->GetSize()/2 - D3DXVECTOR2(15.0f, 25.0f));

	return box;
}

const AchievmentFrame::AchievmentBox* AchievmentFrame::GetAchievment(Achievment* model)
{
	for (Achievments::const_iterator iter = _achievments.begin(); iter != _achievments.end(); ++iter)
		if (iter->model == model)
			return &(*iter);
	return NULL;
}

void AchievmentFrame::UpdateAchievments()
{
	const std::string nameList[cMenuBoxEnd] = {AchievmentModel::cViper, AchievmentModel::cBuggi, AchievmentModel::cAirblade, AchievmentModel::cReflector, AchievmentModel::cDroid, AchievmentModel::cTankchetti, AchievmentModel::cPhaser, AchievmentModel::cMustang, AchievmentModel::cArmor4};
	const std::string lockImgList[cMenuBoxEnd] = {"GUI\\Rewards\\viperLock.png", "GUI\\Rewards\\buggiLock.png", "GUI\\Rewards\\airbladeLock.png", "GUI\\Rewards\\reflectorLock.png", "GUI\\Rewards\\droidLock.png", "GUI\\Rewards\\tankchettiLock.png", "GUI\\Rewards\\phaserLock.png", "GUI\\Rewards\\mustangLock.png", "GUI\\Rewards\\musicTrackLock.png"};
	const std::string imgList[cMenuBoxEnd] = {"GUI\\Rewards\\viper.png", "GUI\\Rewards\\buggi.png", "GUI\\Rewards\\airblade.png", "GUI\\Rewards\\reflector.png", "GUI\\Rewards\\droid.png", "GUI\\Rewards\\tankchetti.png", "GUI\\Rewards\\phaser.png", "GUI\\Rewards\\mustang.png", "GUI\\Rewards\\musicTrack.png"};
	const D3DXVECTOR2 posList[cMenuBoxEnd] = {D3DXVECTOR2(200.0f, 105.0f), D3DXVECTOR2(405.0f, 155.0f), D3DXVECTOR2(0.0f, 245.0f), D3DXVECTOR2(-190.0f, 125.0f), D3DXVECTOR2(-380.0f, 95.0f), D3DXVECTOR2(-325.0f, 265.0f), D3DXVECTOR2(-190.0f, 400.0f), D3DXVECTOR2(205.0f, 375.0f), D3DXVECTOR2(445.0f, 315.0f)};

	const Box sortBox[cMenuBoxEnd] = {mbMusicTrack, mbBuggi, mbViper, mbPhaser, mbMustang, mbAirblade, mbReflector, mbDroid, mbTankchetti};

	lsl::Vector<Menu::NavElement> navElements;
	unsigned count = 0;
	AchievmentBox achievments[cMenuBoxEnd];

	for (int i = 0; i < cMenuBoxEnd; ++i)
	{
		Box box = sortBox[i];
		Achievment* model = achievment().Get(nameList[box]);
		if (model)
		{
			achievments[box] = *AddAchievment(count++, lockImgList[box], imgList[box], posList[box] + D3DXVECTOR2(-30.0f, 0.0f), model);
		}
	}

	_labels[mlPoints]->SetText(GetString(svPoints) + " " + menu()->FormatCurrency(achievment().points()));

	{
		Menu::NavElement navElement = {achievments[mbPhaser].button, {achievments[mbTankchetti].button, achievments[mbMustang].button, achievments[mbAirblade].button, _menuItems[miExit]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}
	{
		Menu::NavElement navElement = {achievments[mbMustang].button, {achievments[mbPhaser].button, achievments[mbMusicTrack].button, achievments[mbAirblade].button, _menuItems[miExit]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}
	{
		Menu::NavElement navElement = {achievments[mbMusicTrack].button, {achievments[mbMustang].button, achievments[mbTankchetti].button, achievments[mbBuggi].button, _menuItems[miExit]}, {cVirtualKeyEnd, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}
	{
		Menu::NavElement navElement = {achievments[mbTankchetti].button, {achievments[mbMusicTrack].button, achievments[mbAirblade].button, achievments[mbDroid].button, achievments[mbPhaser].button}, {cVirtualKeyEnd, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}
	{
		Menu::NavElement navElement = {achievments[mbAirblade].button, {achievments[mbTankchetti].button, achievments[mbMusicTrack].button, achievments[mbReflector].button, achievments[mbPhaser].button}, {cVirtualKeyEnd, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}
	{
		Menu::NavElement navElement = {achievments[mbDroid].button, {achievments[mbBuggi].button, achievments[mbReflector].button, _menuItems[miExit], achievments[mbTankchetti].button}, {cVirtualKeyEnd, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}
	{
		Menu::NavElement navElement = {achievments[mbReflector].button, {achievments[mbDroid].button, achievments[mbViper].button, _menuItems[miExit], achievments[mbTankchetti].button}, {cVirtualKeyEnd, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}
	{
		Menu::NavElement navElement = {achievments[mbViper].button, {achievments[mbReflector].button, achievments[mbBuggi].button, _menuItems[miExit], achievments[mbAirblade].button}, {cVirtualKeyEnd, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}
	{
		Menu::NavElement navElement = {achievments[mbBuggi].button, {achievments[mbViper].button, achievments[mbDroid].button, _menuItems[miExit], achievments[mbMusicTrack].button}, {cVirtualKeyEnd, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}

	{
		Menu::NavElement navElement = {_menuItems[miExit], {NULL, NULL, achievments[mbPhaser].button, achievments[mbPhaser].button}, {vkBack, cVirtualKeyEnd}};
		navElements.push_back(navElement);
	}	

	menu()->SetNavElements(_menuItems[miExit], true, &navElements[0], navElements.size());
}

void AchievmentFrame::UpdateSelection(gui::Widget* sender, bool select)
{
	bool setSelection = false;

	for (Achievments::const_reverse_iterator iter = _achievments.rbegin(); iter != _achievments.rend(); ++iter)
	{
		if (iter->image->IsMouseEnter() && !setSelection)
		{
			iter->button->Select(true, false);
			setSelection = true;
		}
		else
		{
			iter->button->Select(false, false);
		}
	}
}

void AchievmentFrame::ShowMessage(StringValue message, gui::Widget* sender)
{
	menu()->ShowMessage(GetString(svWarning), GetString(message), GetString(svOk), uiRoot()->GetVPSize()/2, gui::Widget::waCenter, 0.0f);
}

void AchievmentFrame::ShowAccept(const std::string& message, gui::Widget* sender, Achievment* achievment)
{
	menu()->ShowAccept(message, GetString(svYes), GetString(svNo), uiRoot()->GetVPSize()/2, gui::Widget::waCenter, this, achievment);
}

void AchievmentFrame::OnShow(bool value)
{
	menu()->GetRace()->GetWorld()->GetGraph()->SetGUIMode(value);

	if (value)
	{
		UpdateAchievments();
	}
	else
	{
		menu()->CarSlotsChanged();
		menu()->UnregNavElements(_menuItems[miExit]);
	}
}

void AchievmentFrame::OnAdjustLayout(const D3DXVECTOR2& vpSize)
{
	_bottomPanel->SetPos(vpSize.x/2, vpSize.y - 80.0f);
	_bottomPanel->SetSize(vpSize.x, _bottomPanel->GetSize().y);

	_bg->SetSize(menu()->StretchImage(_bg->GetMaterial(), vpSize, true, true));

	float scale = std::min(vpSize.x/1090.0f, vpSize.y/720.0f);
	_panel->SetScale(scale, scale);

	_labels[mlPoints]->SetPos(D3DXVECTOR2(0, 40.0f));
	_labels[mlRewards]->SetPos(D3DXVECTOR2(0.0f, 555.0f));
	
	for (int i = 0; i < cMenuItemEnd; ++i)
		_menuItems[i]->SetPos(D3DXVECTOR2(0.0f, _bottomPanel->GetPos().y + 17.0f + i * (_menuItems[i]->GetSize().y + 10.0f)));
}

void AchievmentFrame::OnInvalidate()
{	
}

bool AchievmentFrame::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
{
	if (sender == _menuItems[miExit])
	{
		_raceMenu->SetState(RaceMenu::msMain);
		return true;
	}

	if (sender == menu()->GetAcceptSender() && menu()->GetAcceptResultYes())
	{
		Achievment* achievment = lsl::StaticCast<Achievment*>(menu()->GetAcceptData());
		if (achievment->Buy(player()))
			UpdateAchievments();
		else
			ShowMessage(svHintCantPoints, sender);
		return true;
	}
	
	const AchievmentBox* box = (sender->GetData() || sender->GetParent() && sender->GetParent()->GetData()) ? GetAchievment((Achievment*)sender->GetData()) : NULL;
	if (box)
	{
		if (box->model->state() == Achievment::asUnlocked)
			ShowAccept(GetString(svBuyReward), sender, box->model);
		return true;
	}	
	
	return false;
}

bool AchievmentFrame::OnMouseEnter(gui::Widget* sender, const gui::MouseMove& mMove)
{
	if (sender->GetData())
	{
		UpdateSelection(sender, true);

		/*const AchievmentBox* box = GetAchievment((Achievment*)sender->GetData());

		for (Achievments::const_iterator iter = _achievments.begin(); iter != _achievments.end(); ++iter)
			if (&(*iter) != box)
				iter->button->Select(false, false);

		if (box)
			box->button->Select(true, false);*/

		return false;
	}

	return false;
}

void AchievmentFrame::OnMouseLeave(gui::Widget* sender, bool wasReset)
{
	if (sender->GetData())
	{
		UpdateSelection(sender, false);

		/*const AchievmentBox* box = GetAchievment((Achievment*)sender->GetData());
		if (box)
			box->button->Select(false, false);
		return;*/
	}
}




RaceMainFrame::RaceMainFrame(Menu* menu, RaceMenu* raceMenu, gui::Widget* parent): MenuFrame(menu, parent), _raceMenu(raceMenu)
{
	const D3DXCOLOR color1 = D3DXCOLOR(214.0f, 214.0f, 214.0f, 255.0f)/255.0f;
	const D3DXCOLOR color2 = D3DXCOLOR(214.0f, 184.0f, 164.0f, 255.0f)/255.0f;	
	const std::string menuItemsIcon[cMenuItemEnd] = {"GUI\\icoStart.png", "GUI\\icoWorkshop.png", "GUI\\icoGarage.png", "GUI\\icoSpace.png", "GUI\\icoAchivment.png", "GUI\\icoOptions.png", "GUI\\icoExit.png"};

	StringValue strLabels[cLabelEnd] = {svPlayer, svPassing, svTournament, svWeapons, svBossName, svNull, svPassInfo, svTournamentInfo};
	std::string fontLabels[cLabelEnd] = {"Small", "Small", "Small", "Small", "Small", "Item", "Small", "Small"};
	gui::Text::HorAlign horLabels[cLabelEnd] = {gui::Text::haCenter, gui::Text::haCenter, gui::Text::haCenter, gui::Text::haCenter, gui::Text::haCenter, gui::Text::haRight, gui::Text::haLeft, gui::Text::haLeft};
	gui::Text::VertAlign vertLabels[cLabelEnd] = {gui::Text::vaCenter, gui::Text::vaCenter, gui::Text::vaCenter, gui::Text::vaCenter, gui::Text::vaCenter, gui::Text::vaCenter, gui::Text::vaCenter, gui::Text::vaCenter};
	D3DXCOLOR colorLabels[cLabelEnd] = {color1, color1, color1, color1, color1, clrWhite, color2, color2};

	_topPanel = menu->CreatePlane(root(), this, "GUI\\topPanel.png", true);
	_topPanel->SetAnchor(gui::Widget::waTop);
	_topPanel->SetAlign(gui::Widget::waTop);

	_bottomPanel = menu->CreatePlane(root(), this, "GUI\\bottomPanel.png", true, IdentityVec2, gui::Material::bmTransparency);
	_bottomPanel->SetAnchor(gui::Widget::waBottom);
	_bottomPanel->SetAlign(gui::Widget::waBottom);

	_moneyBg = menu->CreatePlane(_bottomPanel, this, "GUI\\moneyBg.png", true, IdentityVec2, gui::Material::bmTransparency);
	_moneyBg->SetAlign(gui::Widget::waRightBottom);	

	_stateBg = menu->CreatePlane(_bottomPanel, this, "GUI\\statFrame.png", true, IdentityVec2, gui::Material::bmTransparency);
	_stateBg->SetAlign(gui::Widget::waLeftBottom);

	_wheater = menu->CreatePlane(_topPanel, this, "", true, IdentityVec2, gui::Material::bmTransparency);

	_speedBar = menu->CreateBar(_stateBg, NULL, "GUI\\statBar.png", "");
	_armorBar = menu->CreateBar(_stateBg, NULL, "GUI\\statBar.png", "");
	_damageBar = menu->CreateBar(_stateBg, NULL, "GUI\\statBar.png", "");

	_armorBarValue = menu->CreateLabel("", _armorBar, "Small", NullVec2, gui::Text::haRight, gui::Text::vaCenter, color1);
	_armorBarValue->SetPos(D3DXVECTOR2(_armorBar->GetSize().x/2 - 15.0f, 0.0f));

	_damageBarValue = menu->CreateLabel("", _damageBar, "Small", NullVec2, gui::Text::haRight, gui::Text::vaCenter, color1);
	_damageBarValue->SetPos(D3DXVECTOR2(_damageBar->GetSize().x/2 - 15.0f, 0.0f));

	_speedBarValue = menu->CreateLabel("", _speedBar, "Small", NullVec2, gui::Text::haRight, gui::Text::vaCenter, color1);
	_speedBarValue->SetPos(D3DXVECTOR2(_speedBar->GetSize().x/2 - 15.0f, 0.0f));

	_viewportCar = menu->CreateViewPort3d(_topPanel, NULL, "", gui::ViewPort3d::msAnim);	
	D3DXQUATERNION quat;
	D3DXQuaternionRotationAxis(&quat, &ZVector, D3DX_PI/2.0f);
	_viewportCar->SetRot3dSpeed(quat);

	_playerGrid = menu->CreateDummy(root(), NULL);

	for (int i = 0; i < 2; ++i)
	{
		_photoImages[i] = menu->CreatePlane(_topPanel, NULL, "", true, IdentityVec2, gui::Material::bmTransparency);
		_photoImages[i]->GetMaterial().GetSampler().SetFiltering(graph::BaseSampler::sfLinear);
	}

	for (int i = 0; i < 3; ++i)
	{
		_frameImages[i] = menu->CreatePlane(i == 2 ? (gui::Widget*)_viewportCar : (gui::Widget*)_photoImages[i], NULL, "GUI\\imageFrame1.png", true, IdentityVec2, gui::Material::bmTransparency);
		_frameImages[i]->GetMaterial().GetSampler().SetFiltering(graph::BaseSampler::sfLinear);
	}

	for (int i = 0; i < 6; ++i)
	{
		_chargeBars[i] = menu->CreateBar(_topPanel, NULL, "GUI\\chargeBar1.png", "", gui::ProgressBar::psVertical);
		_chargeBars[i]->SetAlign(gui::Widget::waBottom);
	}

	for (int i = 0; i < 6; ++i)
	{
		_slots[i] = menu->CreateMesh3dBox(_topPanel, NULL, NULL, NULL, gui::ViewPort3d::msAnim);
		_slots[i]->SetSize(D3DXVECTOR2(50.0f, 50.0f));

		D3DXQUATERNION quat;
		D3DXQuaternionRotationAxis(&quat, &ZVector, D3DX_PI/2.0f);
		_slots[i]->SetRot3dSpeed(quat);

		D3DXQuaternionRotationAxis(&quat, &ZVector, 2.0f * D3DX_PI * i / 6.0f);
		_slots[i]->GetBox()->SetRot(quat * _slots[i]->GetBox()->GetRot());
	}

	for (int i = 0; i < cMenuItemEnd; ++i)
		_menuItems[i] = _raceMenu->CreateMenuButton(menuItemsIcon[i], _bottomPanel, this);

	gui::Widget* labelsParent[cLabelEnd] = {_topPanel, _topPanel, _topPanel, _topPanel, _topPanel, _moneyBg, _topPanel, _topPanel};
	for (int i = 0; i < cLabelEnd; ++i)	
		_labels[i] = menu->CreateLabel(strLabels[i], labelsParent[i], fontLabels[i], NullVec2, horLabels[i], vertLabels[i], colorLabels[i]);

	_netInfo = menu->CreateLabel(svNull, _topPanel, "Small", NullVec2, gui::Text::haLeft, gui::Text::vaTop);
	_netInfo->SetAlign(gui::Widget::waLeftTop);
	_netInfo->SetText("");
}

RaceMainFrame::~RaceMainFrame()
{
	if (visible())
		menu()->ShowChat(false);

	menu()->UnregNavElements(_menuItems[miStartRace]);
	menu()->ReleaseWidget(_playerGrid);

	for (int i = 0; i < cLabelEnd; ++i)
		menu()->ReleaseWidget(_labels[i]);

	menu()->ReleaseWidget(_bottomPanel);
	menu()->ReleaseWidget(_topPanel);
}

void RaceMainFrame::RaceRady(bool ready)
{
	net()->player()->RaceReady(ready);

	Invalidate();
}

RaceMainFrame::PlayerBox* RaceMainFrame::AddPlayer(NetPlayer* netPlayer, unsigned index)
{
	const D3DXCOLOR color1 = clrWhite;

	PlayerBox* box = index < _players.size() ? &_players[index] : NULL;
	if (box == NULL)
	{
		PlayerBox newBox;

		newBox.bgRoot = menu()->CreateDummy(_playerGrid, NULL);

		newBox.bg = menu()->CreatePlane(newBox.bgRoot, NULL, "GUI\\netPlayerFrame.png", true, IdentityVec2, gui::Material::bmTransparency);		

		newBox.photo = menu()->CreatePlane(newBox.bgRoot, NULL, "", false, IdentityVec2, gui::Material::bmTransparency);

		newBox.name = menu()->CreateLabel(svNull, newBox.bgRoot, "VerySmall", NullVec2, gui::Text::haLeft, gui::Text::vaCenter, color1);
		newBox.name->SetAlign(gui::Widget::waLeft);

		newBox.readyLabel = menu()->CreateLabel(svNull, newBox.bgRoot, "VerySmall", NullVec2, gui::Text::haLeft, gui::Text::vaCenter, color1);
		newBox.readyLabel->SetAlign(gui::Widget::waLeft);

		newBox.kick = menu()->CreateMenuButton(svNull, "", "GUI\\netPlayerKick.png", "GUI\\netPlayerKickSel.png", newBox.bgRoot, this, IdentityVec2, gui::Button::bsSelAnim);		

		newBox.readyState = menu()->CreatePlane(newBox.bgRoot, NULL, "", false, IdentityVec2, gui::Material::bmTransparency);

		newBox.viewportCar = menu()->CreateViewPort3d(newBox.bgRoot, NULL, "", gui::ViewPort3d::msAnim);
		D3DXQUATERNION quat;
		D3DXQuaternionRotationAxis(&quat, &ZVector, D3DX_PI/2.0f);
		newBox.viewportCar->SetRot3dSpeed(quat);

		_players.push_back(newBox);
		box = &_players.back();
	}

	box->netPlayer = netPlayer;

	box->name->SetText(GetString(netPlayer->model()->GetName()));
	box->photo->GetMaterial().GetSampler().SetTex(netPlayer->model()->GetPhoto());
	box->kick->SetVisible(net()->isHost());

	if (box->netPlayer->ownerId() == net::cServerPlayer)
		box->readyLabel->SetText(GetString(svHostLabel));		
	else
		box->readyLabel->SetText(GetString(netPlayer->IsRaceReady() ? svReadyRace : svCancelReadyRace));		

	box->readyState->SetVisible(box->netPlayer->ownerId() != net::cServerPlayer);
	box->readyState->GetMaterial().GetSampler().SetTex(GetTexture(netPlayer->IsRaceReady() ? "GUI\\netPlayerReadyStateSel.png" : "GUI\\netPlayerReadyState.png"));

	_raceMenu->CreateCar(box->viewportCar, netPlayer->model());	

	return box;
}

void RaceMainFrame::AdjustPlayer(PlayerBox* box, bool invert)
{
	float dir = invert ? -1.0f : 1.0f;

	box->bg->SetRot(invert ? D3DX_PI : 0.0f);

	box->photo->SetSize(menu()->StretchImage(box->photo->GetMaterial(), D3DXVECTOR2(100.0f, 97.0f), true, false, true, false));
	box->photo->SetPos(D3DXVECTOR2(-60.0f * dir, 0.0f));

	box->name->SetPos(D3DXVECTOR2(0.0f * dir, -70.0f));
	box->name->SetAlign(gui::Widget::waCenter);
	box->name->SetHorAlign(gui::Text::haCenter);
	
	box->readyLabel->SetPos(D3DXVECTOR2(0.0f * dir, 68.0f));
	box->readyLabel->SetAlign(gui::Widget::waCenter);
	box->readyLabel->SetHorAlign(gui::Text::haCenter);

	box->readyState->SetSize(menu()->GetImageSize(box->readyState->GetMaterial()));
	box->readyState->SetPos(D3DXVECTOR2(128.0f * dir, 68.0f));	

	box->kick->SetPos(D3DXVECTOR2(128.0f * dir, -68.0f));

	box->viewportCar->SetSize(D3DXVECTOR2(130.0f, 130.0f));
	box->viewportCar->SetPos(D3DXVECTOR2(72.0f * dir, 0.0f));
}

void RaceMainFrame::UpdatePlayers()
{
	unsigned index = 0;

	for (NetGame::NetPlayers::const_iterator iter = net()->netOpponents().begin(); iter != net()->netOpponents().end(); ++iter, ++index)
	{
		AddPlayer(*iter, index);
	}

	if (index < _players.size())
	{
		for (unsigned i = index; i < _players.size(); ++i)
		{	
			menu()->ReleaseWidget(_players[i].bgRoot);
		}

		_players.erase(_players.begin() + index, _players.end());
	}	
}

void RaceMainFrame::OnShow(bool value)
{
	if (value)
		_raceMenu->carFrame()->SetCamStyle(CarFrame::csCar, false);

	menu()->ShowChat(value);

	//menu()->ActivateCtrlButtons(_menuItems, value);

	Menu::NavElement elements[] = {
		{_menuItems[miStartRace], {_menuItems[miExit], _menuItems[miWorkshop], NULL, NULL}, {cVirtualKeyEnd, cVirtualKeyEnd}},
		{_menuItems[miWorkshop], {_menuItems[miStartRace], _menuItems[miGarage], NULL, NULL}, {cVirtualKeyEnd, cVirtualKeyEnd}},
		{_menuItems[miGarage], {_menuItems[miWorkshop], _menuItems[miSpace], NULL, NULL}, {cVirtualKeyEnd, cVirtualKeyEnd}},
		{_menuItems[miSpace], {_menuItems[miGarage], _menuItems[miAchivments], NULL, NULL}, {cVirtualKeyEnd, cVirtualKeyEnd}},
		{_menuItems[miAchivments], {_menuItems[miSpace], _menuItems[miOptions], NULL, NULL}, {cVirtualKeyEnd, cVirtualKeyEnd}},
		{_menuItems[miOptions], {_menuItems[miAchivments], _menuItems[miExit], NULL, NULL}, {cVirtualKeyEnd, cVirtualKeyEnd}},
		{_menuItems[miExit], {_menuItems[miOptions], _menuItems[miStartRace], NULL, NULL}, {cVirtualKeyEnd, cVirtualKeyEnd}}
	};

	menu()->SetNavElements(_menuItems[miStartRace], value, elements, ARRAY_LENGTH(elements));
}

void RaceMainFrame::OnAdjustLayout(const D3DXVECTOR2& vpSize)
{
	const float menuItemSpaceX = 50.0f;

	_moneyBg->SetPos(D3DXVECTOR2(vpSize.x/2 + 1, -_bottomPanel->GetSize().y + 5));
	_stateBg->SetPos(D3DXVECTOR2(-vpSize.x/2 - 1, -_bottomPanel->GetSize().y + 5));

	_damageBar->SetPos(D3DXVECTOR2(128.0f, -98.0f));
	_armorBar->SetPos(D3DXVECTOR2(150.0f, -61.0f));
	_speedBar->SetPos(D3DXVECTOR2(173.0f, -23.0f));

	_labels[mlMoney]->SetPos(D3DXVECTOR2(-53.0f, -29.0f));
	_labels[mlPassInfo]->SetPos(D3DXVECTOR2(-377.0f, 86.0f));
	_labels[mlTournamentInfo]->SetPos(D3DXVECTOR2(-102, 86.0f));	

	for (int i = 0; i < cMenuItemEnd; ++i)
	{
		float offsX = -(cMenuItemEnd * _menuItems[i]->GetSize().x + (cMenuItemEnd - 1) * menuItemSpaceX)/2 + _menuItems[i]->GetSize().x/2;
		_menuItems[i]->SetPos(offsX + i * (_menuItems[i]->GetSize().x + menuItemSpaceX), -_menuItems[i]->GetSize().y/2 - 25);
	}

	for (int i = mlPlayer; i <= mlBoss; ++i)
	{
		_labels[i]->SetPos(-548.0f + i * 275, 18);
	}

	const float frameImagesPos[3] = {-550.0f, 475.0f, 590.0f};
	for (int i = 0; i < 3; ++i)
		_photoImages[i]->SetPos(D3DXVECTOR2(frameImagesPos[i], 87));

	_wheater->SetPos(D3DXVECTOR2(-282.0f, 112.0f));

	int chargeIndex = 0;
	float width = 0;	
	for (int i = 0; i < 6; ++i)
		if (_chargeBars[i]->GetVisible())
		{
			width = chargeIndex * (_chargeBars[i]->GetSize().x + 40.0f);
			_chargeBars[i]->SetPos(D3DXVECTOR2(257.0f + width, 120));
			++chargeIndex;
		}
	for (int i = 0; i < 6; ++i)
		if (_chargeBars[i]->GetVisible())
		{
			D3DXVECTOR2 pos = _chargeBars[i]->GetPos();
			pos.x = pos.x + _chargeBars[i]->GetSize().x/2 + (vpSize.x - width)/2 - vpSize.x/2;
			_chargeBars[i]->SetPos(pos);
			_slots[i]->SetPos(pos.x, 65.0f);
		}

	_netInfo->SetPos(-vpSize.x/2 + 20.0f, _topPanel->GetSize().y + 20.0f);

	_viewportCar->SetPos(D3DXVECTOR2(605.0f, 87.0f));	
	_viewportCar->SetSize(menu()->StretchImage(IdentityVec2, _frameImages[2]->GetSize(), true, true));

	float leftSpace = vpSize.y - _topPanel->GetSize().y - _bottomPanel->GetSize().y - _stateBg->GetSize().y;	
	for (int i = 0; i < (int)_players.size(); ++i)
	{
		D3DXVECTOR2 size = _players[i].bg->GetSize();
		float space = 25.0f;

		int leftCount = Floor<int>(leftSpace / (size.y + space * 0.5f));
		int col = std::min(i / leftCount, 1);
		int row = i - leftCount * col;

		AdjustPlayer(&_players[i], col > 0);
		_players[i].bgRoot->SetPos(D3DXVECTOR2(size.x/2 + 8.0f + col * (vpSize.x - size.x - 16.0f), size.y/2 + (size.y + space) * row));
	}
	_playerGrid->SetPos(D3DXVECTOR2(-1, _topPanel->GetSize().y + 30.0f));

	menu()->GetUserChat()->inputPos(D3DXVECTOR2(_stateBg->GetSize().x, vpSize.y - _bottomPanel->GetSize().y - 10.0f));
	menu()->GetUserChat()->inputSize(D3DXVECTOR2(vpSize.x - _stateBg->GetSize().x - _moneyBg->GetSize().x, 300.0f));

	menu()->GetUserChat()->linesPos(D3DXVECTOR2(vpSize.x - 10.0f, _topPanel->GetSize().y));
	menu()->GetUserChat()->linesSize(D3DXVECTOR2(vpSize.x/3, vpSize.y - _topPanel->GetSize().y - _bottomPanel->GetSize().y - _moneyBg->GetSize().y));
}

void RaceMainFrame::OnInvalidate()
{
	if (net()->isStarted())
	{
		std::string text = "waiting for players...\n\n";

		for (NetGame::NetPlayers::const_iterator iter = net()->players().begin(); iter != net()->players().end(); ++iter)
		{
			NetPlayer* netPlayer = (*iter);
			text += lsl::StrFmt("player(%d) connected ready=%d car=%s\n", netPlayer->id(), (DWORD)netPlayer->IsRaceReady(), netPlayer->GetCar() ? GetString(netPlayer->GetCar()->GetName()).c_str() : "");
		}

		_netInfo->SetVisible(false);
		_netInfo->SetText(text);
	}
	else
	{
		_netInfo->SetVisible(false);		
	}

	for (int i = 0; i < cMenuItemEnd; ++i)
	{
		bool raceReady = net()->isStarted() && net()->isClient() && netPlayer()->IsRaceReady();

		if (i == miStartRace)
		{
			_menuItems[miStartRace]->Select(raceReady, true);
		}
		else
		{
			menu()->SetButtonEnabled(_menuItems[i], !raceReady);
			gui::PlaneFon* plane = static_cast<gui::PlaneFon*>(_menuItems[i]->GetChildren().front());
			plane->GetMaterial().SetAlpha(!raceReady ? 1.0f : 0.25f);
		}
	}

	if (HasString(svPassInfo))
	{
		_labels[mlPassInfo]->SetText(lsl::StrFmt(GetString(svPassInfo).c_str(), 
			GetString(tournament().GetCurPlanet().GetName()).c_str(),
			tournament().GetCurTrackIndex() + 1,
			menu()->IsCampaign() ? tournament().GetCurPlanet().GetTracks().size() : tournament().GetCurPlanet().GetTrackList().size()));
	}

	if (HasString(svTournamentInfo))
	{
		int points = menu()->GetRace()->GetTotalPoints();
		int reqPoints = tournament().GetCurPlanet().GetRequestPoints(tournament().GetCurPlanet().GetPass());
		int pass = tournament().GetCurPlanet().GetPass();
		std::string passStr = "A";
		if (pass <= 1)
			passStr = "B";

		_labels[mlTournamentInfo]->SetText(lsl::StrFmt(GetString(svTournamentInfo).c_str(),
			passStr.c_str(),
			reqPoints,
			points));
	}

	_photoImages[0]->GetMaterial().GetSampler().SetTex(player()->GetPhoto());
	_photoImages[0]->SetSize(menu()->StretchImage(_photoImages[0]->GetMaterial(), _frameImages[0]->GetSize()));

	_photoImages[1]->GetMaterial().GetSampler().SetTex(tournament().GetCurPlanet().GetBoss().photo);
	_photoImages[1]->SetSize(menu()->StretchImage(_photoImages[1]->GetMaterial(), _frameImages[1]->GetSize()));

	const std::string wheaterImg[Environment::cWheaterEnd] = {"GUI\\fair.png", "GUI\\night.png", "GUI\\cloudy.png", "GUI\\rainy.png", "GUI\\fair.png", "GUI\\cloudy.png", "GUI\\fair.png", "GUI\\fair.png", "GUI\\fair.png"};
	_wheater->GetMaterial().GetSampler().SetTex(GetTexture(wheaterImg[tournament().GetWheater()]));
	_wheater->SetSize(menu()->GetImageSize(_wheater->GetMaterial()));

	Player::SlotType playerSlots[6] = {Player::stWeapon1, Player::stWeapon2, Player::stWeapon3, Player::stWeapon4, Player::stHyper, Player::stMine};
	for (int i = 0; i < 6; ++i)
	{
		Slot* slot = player()->GetSlotInst(playerSlots[i]);
		_chargeBars[i]->SetVisible(slot != NULL);

		_slots[i]->SetVisible(slot != NULL);
		if (slot)
		{
			WeaponItem& wpn = slot->GetItem<WeaponItem>();
			_chargeBars[i]->SetProgress(lsl::ClampValue(wpn.GetCntCharge() / 7.0f, 0.0f, 1.0f));

			gui::Mesh3d* mesh3d = static_cast<gui::Mesh3d*>(_slots[i]->GetBox()->GetChildren().front());
			mesh3d->SetMesh(slot->GetItem().GetMesh());
			mesh3d->GetOrCreateMaterial()->GetSampler().SetTex(slot->GetItem().GetTexture());
			mesh3d->GetOrCreateMaterial()->GetSampler().SetFiltering(graph::BaseSampler::sfAnisotropic);
		}
	}

	_raceMenu->carFrame()->SetCar(player()->GetCar().record, player()->GetColor());
	_raceMenu->carFrame()->SetSlots(player(), false);

	_labels[mlMoney]->SetText(menu()->FormatCurrency(player()->GetMoney()));

	Planet& bossPlanet = tournament().GetCurPlanet();
	if (bossPlanet.GetBoss().cars.size() > 0)
		_raceMenu->CreateCar(_viewportCar, garage().FindCar(bossPlanet.GetBoss().cars.front().record), clrWhite, NULL);
	else
		_raceMenu->CreateCar(_viewportCar, NULL);

	_raceMenu->UpdateStats(_raceMenu->carFrame()->GetCar(), player(), _armorBar, _speedBar, _damageBar, _armorBarValue, _damageBarValue, _speedBarValue);

	UpdatePlayers();
}

bool RaceMainFrame::OnClick(gui::Widget* sender, const gui::MouseClick& mClick)
{
	if (sender == menu()->GetAcceptSender())
	{
		if (menu()->GetAcceptResultYes())
			menu()->StartRace();
		return true;
	}

	if (sender == _menuItems[miStartRace])
	{
		if (menu()->IsNetGame())
		{
			if (net()->isHost())
			{
				if (net()->AllPlayersReady() && net()->netOpponents().size() > 0)
				{
					if (net()->race()->GetLeaverList().size() > 0)
						menu()->ShowAccept(GetString(svHintLeaversWillBeRemoved), GetString(svYes), GetString(svNo), uiRoot()->GetVPSize()/2, gui::Widget::waCenter, this);
					else
						menu()->StartRace();
				}
				else
					menu()->ShowMessage(GetString(svWarning), GetString(svHintPlayersIsNotReady), GetString(svOk), uiRoot()->GetVPSize()/2, gui::Widget::waCenter, 0.0f);
			}
			else
				RaceRady(!net()->player()->IsRaceReady());
		}
		else
			menu()->StartRace();

		return true;
	}

	if (sender == _menuItems[miGarage])
	{
		_raceMenu->SetState(RaceMenu::msGarage);
		return true;
	}

	if (sender == _menuItems[miWorkshop])
	{
		_raceMenu->SetState(RaceMenu::msWorkshop);
		return true;
	}

	if (sender == _menuItems[miSpace])
	{
		_raceMenu->SetState(RaceMenu::msAngar);
		return true;
	}

	if (sender == _menuItems[miAchivments])
	{
		_raceMenu->SetState(RaceMenu::msAchievments);
		return true;
	}

	if (sender == _menuItems[miOptions])
	{
		menu()->ShowOptions(true);
		return true;
	}

	if (sender == _menuItems[miExit])
	{
		menu()->ExitMatch();
		return true;
	}

	for (Players::const_iterator iter = _players.begin(); iter != _players.end(); ++iter)
	{
		if (sender == iter->kick)
		{
			net()->DisconnectPlayer(iter->netPlayer);
			Invalidate();
			return true;
		}
	}

	return false;
}

void RaceMainFrame::OnReceiveCmd(const net::NetMessage& msg, const net::NetCmdHeader& header, const void* data, unsigned size)
{
	Invalidate();
}

void RaceMainFrame::OnConnectedPlayer(NetPlayer* sender)
{
	Invalidate();
}

void RaceMainFrame::OnDisconnectedPlayer(NetPlayer* sender)
{
	Invalidate();
}




RaceMenu::RaceMenu(Menu* menu, gui::Widget* parent, Player* player): _menu(menu), _state(msMain), _lastState(cStateEnd), _player(player)
{
	LSL_ASSERT(menu && _player);

	_player->AddRef();

	_root = _menu->GetGUI()->CreateDummy();
	_root->SetParent(parent);

	_carFrame = new CarFrame(menu, this, _root);
	_spaceshipFrame = new SpaceshipFrame(menu, this, _root);
	_mainFrame = new RaceMainFrame(menu, this, _root);
	_garageFrame = new GarageFrame(menu, this, _root);
	_workshopFrame = new WorkshopFrame(menu, this, _root);	
	_gamers = new GamersFrame(menu, this, _root);
	_angarFrame = new AngarFrame(menu, this, _root);
	_achievmentFrame = new AchievmentFrame(menu, this, _root);

	ApplyState(_state);
	menu->GetRace()->GetWorld()->GetGraph()->BuildOctree();

	_menu->GetNet()->RegUser(this);
	_menu->GetGame()->RegUser(this);
}

RaceMenu::~RaceMenu()
{
	_menu->GetGame()->UnregUser(this);
	_menu->GetNet()->UnregUser(this);

	delete _achievmentFrame;
	delete _angarFrame;
	delete _gamers;
	delete _workshopFrame;
	delete _garageFrame;
	delete _mainFrame;
	delete _spaceshipFrame;
	delete _carFrame;

	_menu->GetGUI()->ReleaseWidget(_root);

	_player->Release();
}

void RaceMenu::ApplyState(State state)
{
	bool carVisible = state == msMain || state == msGarage || state == msWorkshop;
	bool spaceshipVisible = state == msAngar;
	
	if (!carVisible)
		_carFrame->Show(false);
	if (!spaceshipVisible)
		_spaceshipFrame->Show(false);

	if (carVisible)
		_carFrame->Show(true);
	if (spaceshipVisible)
		_spaceshipFrame->Show(true);

	_mainFrame->Show(state == msMain);
	_garageFrame->Show(state == msGarage);
	_workshopFrame->Show(state == msWorkshop);
	_gamers->Show(state == msGamers);
	_angarFrame->Show(state == msAngar);
	_achievmentFrame->Show(state == msAchievments);
}

void RaceMenu::OnProcessEvent(unsigned id, EventData* data)
{
	if (_workshopFrame->visible())
		_workshopFrame->OnProcessEvent(id, data);

	if (id == cUpgradeMaxLevelChanged ||
		id == cWeaponMaxLevelChanged)
	{
		if (_mainFrame->visible())
			_mainFrame->Invalidate();
	}

	if (id == cRaceMenuInvalidate)
	{
		if (_mainFrame)
			_mainFrame->Invalidate();
		if (_garageFrame)
			_garageFrame->Invalidate();
		if (_workshopFrame)
			_workshopFrame->Invalidate();
		if (_gamers)
			_gamers->Invalidate();
		if (_angarFrame)
			_angarFrame->Invalidate();
		if (_achievmentFrame)
			_achievmentFrame->Invalidate();
	}
}

void RaceMenu::OnReceiveCmd(const net::NetMessage& msg, const net::NetCmdHeader& header, const void* data, unsigned size)
{
	if (_mainFrame->visible())
		_mainFrame->OnReceiveCmd(msg, header, data, size);
}

void RaceMenu::OnConnectedPlayer(NetPlayer* sender)
{
	if (_mainFrame->visible())
		_mainFrame->OnConnectedPlayer(sender);
}

void RaceMenu::OnDisconnectedPlayer(NetPlayer* sender)
{
	if (_mainFrame->visible())
		_mainFrame->OnDisconnectedPlayer(sender);
}

void RaceMenu::OnProcessNetEvent(unsigned id, NetEventData* data)
{
	if (_garageFrame->visible())
		_garageFrame->OnProcessNetEvent(id, data);
	if (_gamers->visible())
		_gamers->OnProcessNetEvent(id, data);

	if (id == cNetPlayerSetColor && data->failed)
	{
		_carFrame->SetCarColor(_menu->GetPlayer()->GetColor());
		_menu->ShowMessage(_menu->GetString(svWarning), _menu->GetString(svHintSetColorFailed), _menu->GetString(svOk), _menu->GetGUI()->GetVPSize()/2, gui::Widget::waCenter, 0.0f);
	}
}

gui::Button* RaceMenu::CreateMenuButton(const std::string& icon, gui::Widget* parent, gui::Widget::Event* guiEvent)
{
	gui::Button* button = _menu->CreateMenuButton(svNull, "", "GUI\\buttonBg1.png", "GUI\\buttonBgSel1.png", parent, guiEvent, IdentityVec2, gui::Button::bsSelAnim, clrWhite,  Menu::ssButton2);

	_menu->CreatePlane(button, NULL, icon, true, IdentityVec2, gui::Material::bmTransparency);

	return button;
}

gui::Button* RaceMenu::CreateMenuButton2(StringValue name, gui::Widget* parent, const D3DXCOLOR& textColor, gui::Widget::Event* guiEvent)
{	
	return _menu->CreateMenuButton(name, "Header", "GUI\\buttonBg2.png", "GUI\\buttonBgSel2.png", parent, guiEvent, IdentityVec2, gui::Button::bsSelAnim, textColor, Menu::ssButton1);
}

gui::Button* RaceMenu::CreateArrow(gui::Widget* parent, gui::Widget::Event* guiEvent)
{
	return _menu->CreateArrow(parent, guiEvent);
}

gui::Button* RaceMenu::CreateArrow2(gui::Widget* parent, gui::Widget::Event* guiEvent)
{
	return _menu->CreateMenuButton(svNull, "", "GUI\\arrow2.png", "GUI\\arrowSel2.png", parent, guiEvent, IdentityVec2, gui::Button::bsSelAnim);
}

gui::Button* RaceMenu::CreatePlusButton(gui::Widget* parent, gui::Widget::Event* guiEvent)
{
	return _menu->CreateMenuButton(svNull, "", "GUI\\chargeButton.png", "GUI\\chargeButtonSel.png", parent, guiEvent, IdentityVec2, gui::Button::bsSelAnim, clrWhite, Menu::ssButton3);
}

void RaceMenu::CreateCar(gui::ViewPort3d* viewport, Garage::Car* car, const D3DXCOLOR& color, Slot* slots[Player::cSlotTypeEnd])
{
	viewport->GetBox()->DeleteChildren();	

	if (car)
	{
		for (Garage::BodyMeshes::const_iterator iter = car->GetBodies().begin(); iter != car->GetBodies().end(); ++iter)
			if (iter->mesh)
			{
				gui::Mesh3d* body = _menu->CreateMesh3d(viewport, iter->mesh, iter->texture, iter->meshId);			
				if (iter->decal)
					body->GetMaterial()->GetSampler().SetColorMode(graph::Sampler2d::tmDecal);
				body->GetMaterial()->GetSampler().SetColor(color);
				body->GetMaterial()->GetSampler().SetFiltering(graph::BaseSampler::sfAnisotropic);			
			}			

		if (!car->GetWheels().empty() && car->GetWheel())
		{
			std::vector<D3DXVECTOR3> wheels;
			CarWheels::LoadPosTo(car->GetWheels(), wheels);
			for (unsigned i = 0; i < wheels.size(); ++i)
			{
				gui::Mesh3d* wheel = _menu->CreateMesh3d(viewport, car->GetWheel(), !car->GetBodies().empty() ? car->GetBodies().front().texture : 0);
				wheel->GetMaterial()->GetSampler().SetColorMode(graph::BaseSampler::tmReplace);
				wheel->GetMaterial()->GetSampler().SetFiltering(graph::BaseSampler::sfAnisotropic);
				wheel->SetPos(wheels[i]);
				if (wheels[i].y < 0)
				{
					wheel->SetScale(D3DXVECTOR3(1.0f, -1.0f, 1.0f));
					wheel->invertCullFace = true;
				}
			}
		}
	}

	if (car && slots)
	{
		for (int i = 0; i < Player::cSlotTypeEnd; ++i)
		{
			Player::SlotType type = Player::SlotType(i);

			Slot* slot = slots[i];
			Garage::PlaceSlot placeSlot = car->GetSlot(type);
			const Garage::PlaceItem* placeItem = placeSlot.FindItem(slot);

			if (slot && placeSlot.show && placeItem)
			{
				WeaponItem* wpn = slot->GetItem().IsWeaponItem();
				if (wpn != NULL && wpn->GetMapObj() == NULL)
					continue;

				D3DXVECTOR3 pos = placeSlot.pos + placeItem->offset;
				D3DXQUATERNION rot = placeItem ? placeItem->rot : NullQuaternion;

				gui::Mesh3d* carMesh = _menu->CreateMesh3d(viewport, slot->GetItem().GetMesh(), slot->GetItem().GetTexture());
				carMesh->GetMaterial()->GetSampler().SetFiltering(graph::BaseSampler::sfAnisotropic);
				carMesh->SetPos(pos);
				carMesh->SetRot(rot);
			}
		}
	}
}

void RaceMenu::CreateCar(gui::ViewPort3d* viewport, Player* player)
{
	Garage::Car* car = player && player->GetCar().record ? _menu->GetRace()->GetGarage().FindCar(player->GetCar().record) : NULL;

	Slot* slots[Player::cSlotTypeEnd];
	ZeroMemory(slots, sizeof(slots));

	if (player)
	{
		for (int i = 0; i < Player::cSlotTypeEnd; ++i)
		{
			Player::SlotType type = Player::SlotType(i);

			Record* record = player->GetSlot(type);
			slots[i] = _menu->GetRace()->GetWorkshop().FindSlot(record);
		}
	}

	CreateCar(viewport, car, player != NULL ? player->GetColor() : clrWhite, slots);
}

void RaceMenu::AdjustLayout(const D3DXVECTOR2& vpSize)
{
	_carFrame->AdjustLayout(vpSize);
	_spaceshipFrame->AdjustLayout(vpSize);
	_mainFrame->AdjustLayout(vpSize);
	_garageFrame->AdjustLayout(vpSize);
	_workshopFrame->AdjustLayout(vpSize);
	_gamers->AdjustLayout(vpSize);
	_angarFrame->AdjustLayout(vpSize);
	_achievmentFrame->AdjustLayout(vpSize);
}

void RaceMenu::Show(bool value)
{
	GetRoot()->SetVisible(value);
}

void RaceMenu::UpdateStats(Garage::Car* car, Player* player, gui::ProgressBar* armorBar, gui::ProgressBar* speedBar, gui::ProgressBar* damageBar, gui::Label* armorBarValue, gui::Label* damageBarValue, gui::Label* speedBarValue)
{
	float armor, maxArmor;
	float damage, maxDamage;

	armorBar->SetProgress(_menu->GetRace()->GetGarage().GetArmorSkill(car, player, armor, maxArmor));
	damageBar->SetProgress(_menu->GetRace()->GetGarage().GetDamageSkill(car, player, damage, maxDamage));
	speedBar->SetProgress(_menu->GetRace()->GetGarage().GetSpeedSkill(car, player));

	if (armorBarValue)
		armorBarValue->SetText(lsl::StrFmt("%0.0f/%0.0f", armor, maxArmor));
	if (damageBarValue)
		damageBarValue->SetText(lsl::StrFmt("%0.0f/%0.0f", damage, maxDamage));
	if (speedBarValue)
		speedBarValue->SetText(lsl::StrFmt("%0.0f/%0.0f", speedBar->GetProgress() * 300.0f, 300.0f));
}

void RaceMenu::OnProgress(float deltaTime)
{
	if (_carFrame->visible())
		_carFrame->OnProgress(deltaTime);
	if (_spaceshipFrame->visible())
		_spaceshipFrame->OnProgress(deltaTime);
	if (_angarFrame)
		_angarFrame->OnProgress(deltaTime);
}

Player* RaceMenu::GetPlayer()
{
	return _player;
}

gui::Widget* RaceMenu::GetRoot()
{
	return _root;
}

RaceMenu::State RaceMenu::GetState() const
{
	return _state;
}

void RaceMenu::SetState(State value)
{
	if (_state != value)
	{
		_lastState = _state;
		_state = value;
		ApplyState(_state);
	}
}

RaceMenu::State RaceMenu::GetLastState() const
{
	return _lastState;
}

CarFrame* RaceMenu::carFrame()
{
	return _carFrame;
}

SpaceshipFrame* RaceMenu::spaceshipFrame()
{
	return _spaceshipFrame;
}

}

}

}