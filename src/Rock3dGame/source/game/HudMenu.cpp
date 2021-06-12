#include "stdafx.h"

#include "game\Menu.h"
#include "game\HudMenu.h"

namespace r3d
{

namespace game
{

PlayerStateFrame::PlayerStateFrame(Menu* menu, HudMenu* hudMenu, gui::Widget* parent): MenuFrame(menu, parent), _hudMenu(hudMenu)
{
	const std::string imgTime[5] = {"GUI\\tablo0.png", "GUI\\tablo1.png", "GUI\\tablo2.png", "GUI\\tablo3.png", "GUI\\tablo4.png"};

	Race* race = menu->GetRace();

	UpdateOpponents();

	for (int i = 0; i < cCarLifeEnd; ++i)
	{	
		_carLifes[i].target = NULL;
		_carLifes[i].timer = -1;
		_carLifes[i].timeMax = 4;

		_carLifes[i].back = menu->CreatePlane(root(), 0, "GUI\\carLifeBack.png", true, IdentityVec2, gui::Material::bmTransparency);
		_carLifes[i].back->SetVisible(false);

		_carLifes[i].bar = menu->CreateBar(_carLifes[i].back, NULL, "GUI\\carLifeBar.png", "");
		_carLifes[i].bar->SetPos(_hudMenu->GetCarLifeBarPos());
	}

	_raceState = menu->CreatePlane(root(), 0, "GUI\\placeMineHyper.png", true, IdentityVec2, gui::Material::bmTransparency);
	_raceState->SetAlign(gui::Widget::waLeftTop);

	_place = menu->CreateLabel(svNull, _raceState, "Header", NullVec2, gui::Text::haCenter, gui::Text::vaCenter, clrWhite);	

	_lifeBack = menu->CreatePlane(_raceState, 0, "GUI\\lifeBarBack.png", true, IdentityVec2, gui::Material::bmTransparency);
	_lifeBar = menu->CreateBar(_lifeBack, NULL, "GUI\\lifeBar.png", "");

	for (int i = 0; i < 5; ++i)
	{
		_guiTimer[i] = menu->CreatePlane(root(), 0, imgTime[i], true, IdentityVec2/2.0f, gui::Material::bmTransparency);
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
		lsl::SafeRelease(_carLifes[i].target);
		menu()->GetGUI()->ReleaseWidget(_carLifes[i].bar);
		menu()->GetGUI()->ReleaseWidget(_carLifes[i].back);
	}

	for (PickItems::iterator iter = _pickItemsBuffer.begin(); iter != _pickItemsBuffer.end(); ++iter)
		menu()->ReleaseWidget(iter->image);
	_pickItemsBuffer.clear();

	for (PickItems::iterator iter = _pickItems.begin(); iter != _pickItems.end(); ++iter)
		menu()->ReleaseWidget(iter->image);
	_pickItems.clear();

	for (AchievmentItems::iterator iter = _achievmentsBuffer.begin(); iter != _achievmentsBuffer.end(); ++iter)
		menu()->ReleaseWidget(iter->image);
	_achievmentsBuffer.clear();

	for (AchievmentItems::iterator iter = _achievmentItems.begin(); iter != _achievmentItems.end(); ++iter)
		menu()->ReleaseWidget(iter->image);
	_achievmentItems.clear();

	ClearOpponents();
	ClearSlots();
	
	menu()->GetGUI()->ReleaseWidget(_lifeBar);
	menu()->GetGUI()->ReleaseWidget(_lifeBack);
	menu()->GetGUI()->ReleaseWidget(_place);
	menu()->GetGUI()->ReleaseWidget(_raceState);
}

void PlayerStateFrame::NewPickItem(Slot::Type slotType, GameObject::BonusType bonusType, int targetPlayerId, bool kill)
{
	const D3DXVECTOR2 photoSize = D3DXVECTOR2(50.0f, 50.0f);
	const D3DXCOLOR color2 = D3DXCOLOR(214.0f, 214.0f, 214.0f, 255.0f)/255.0f;	

	string image;	
	graph::Tex2DResource* photo = NULL;
	string name;

	if (kill)
	{
		Player* target = menu()->GetRace()->GetPlayerById(targetPlayerId);
		if (target == NULL)
			return;

		photo = target->GetPhoto();
		image = "GUI\\playerKill.png";
		name = GetString(target->GetName());
	}
	else
		switch (bonusType)
		{
		case game::Player::btMedpack:
			image = "GUI\\pickArmor.png";
			break;

		case game::Player::btCharge:
		{
			switch (slotType)
			{
			case game::Slot::stMine:
				image = "GUI\\pickMine.png";
				break;
			case game::Slot::stHyper:
				image = "GUI\\pickIntro.png";
				break;
			default:
				image = "GUI\\pickWeapon.png";
				break;
			}		
			break;
		}

		case game::Player::btMoney:
			image = "GUI\\pickMoney.png";
			break;

		case game::Player::btImmortal:
			image = "GUI\\pickImmortal.png";
			break;

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
		item.image = menu()->CreatePlane(root(), NULL, image, true, IdentityVec2, gui::Material::bmTransparency);

		item.photo = menu()->CreatePlane(item.image, NULL, "", true, IdentityVec2, gui::Material::bmTransparency);
		item.photo->GetMaterial().GetSampler().SetFiltering(graph::Sampler2d::sfLinear);
		item.photo->SetPos(D3DXVECTOR2(-40.0f, 0.0f));
		
		item.label = menu()->CreateLabel(svNull, item.photo, "Small", NullVec2, gui::Text::haLeft, gui::Text::vaCenter, color2);
		item.label->SetAlign(gui::Widget::waLeft);
		item.label->SetPos(photoSize.x/2 + 0.0f, 0.0f);
	}

	item.time = 0;
	item.image->SetVisible(true);
	item.image->GetMaterial().SetColor(D3DXCOLOR(1, 1, 1, 0));
	item.pos = _hudMenu->GetPickItemsPos() + D3DXVECTOR2(item.image->GetSize().x/2, 0);
	item.image->SetPos(item.pos);

	item.photo->SetVisible(photo ? true : false);	
	item.photo->GetMaterial().GetSampler().SetTex(photo);	
	item.photo->SetSize(menu()->StretchImage(item.photo->GetMaterial(), photoSize, true, false, true, false));

	item.label->SetText(name);

	_pickItems.push_front(item);
}

void PlayerStateFrame::ProccessPickItems(float deltaTime)
{
	struct Pred
	{
		PlayerStateFrame* myThis;
		float deltaTime;
		int index;

		Pred(PlayerStateFrame* owner, float dt): myThis(owner), deltaTime(dt), index(0) {}

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
			if (item.time < 0.3f)
			{
				float lerp = ClampValue(item.time / 0.3f, 0.0f, 1.0f);
				alpha = lerp;				
			}
			else if (item.time > 4.7f)
			{
				float lerp = ClampValue((item.time - 4.7f) / 0.3f, 0.0f, 1.0f);
				dPosY = 30.0f;
				alpha = 1.0f - lerp;
			}

			D3DXVECTOR2 pos = item.pos + D3DXVECTOR2(30, index * 85.0f + dPosY);
			D3DXVECTOR2 curPos = item.image->GetPos();
			if (abs(pos.x - curPos.x) > 0.001f || abs(pos.y - curPos.y) > 0.001f)
			{	
				curPos.x = std::min(curPos.x + 90.0f * deltaTime, pos.x);
				curPos.y = std::min(curPos.y + 120.0f * deltaTime, pos.y);
				item.image->SetPos(curPos);
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
	Difficulty diff = menu()->GetRace()->GetProfile()->difficulty();
	std::string image = "GUI\\Achievments\\" + data->condition->name() + ".png";	
	std::string pointsImage = lsl::StrFmt("GUI\\Achievments\\points%d.png", data->condition->reward());
	std::string pointsImageK = diff == gdHard ? "GUI\\Achievments\\points1_5.png" : "GUI\\Achievments\\points1_2.png";
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
		item.image = menu()->CreatePlane(root(), NULL, image, true, IdentityVec2, gui::Material::bmTransparency);

		item.points = menu()->CreatePlane(item.image, NULL, pointsImage, true, IdentityVec2, gui::Material::bmTransparency);
		item.points->SetPos(D3DXVECTOR2(0.0f, item.image->GetSize().y/2 + 15.0f));
		item.slotSize = D3DXVECTOR2(std::max(item.image->GetSize().x, item.points->GetSize().x), item.image->GetSize().y + item.points->GetSize().y + 15.0f);

		if (diff != gdEasy)
		{
			item.pointsK = menu()->CreatePlane(item.points, NULL, pointsImageK, true, IdentityVec2, gui::Material::bmTransparency);
			item.pointsK->SetPos(D3DXVECTOR2(95.0f, -3.0f));
		}
		else
			item.pointsK = NULL;
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

	item.image->SetVisible(true);
	item.image->GetMaterial().SetColor(D3DXCOLOR(1, 1, 1, 1));
	item.points->GetMaterial().SetColor(D3DXCOLOR(1, 1, 1, 0));	
	if (item.pointsK)
		item.pointsK->GetMaterial().SetColor(D3DXCOLOR(1, 1, 1, 0));
	item.points->SetVisible(menu()->IsCampaign());

	item.image->SetPos(startPos[RandomRange(0, 7)]);
	_achievmentItems.push_front(item);
}

void PlayerStateFrame::ProccessAchievments(float deltaTime)
{
	struct Pred
	{
		PlayerStateFrame* myThis;
		float deltaTime;
		int index;

		Pred(PlayerStateFrame* owner, float dt): myThis(owner), deltaTime(dt), index(0) {}

		bool operator()(AchievmentItem& item)
		{
			item.time += deltaTime;

			float fIndex = (float)index;
			if (item.indexTime == -1.0f && index != item.lastIndex)
				item.indexTime = 0.0f;

			if (item.indexTime != -1.0f)
			{
				item.indexTime += deltaTime;
				float alpha = lsl::ClampValue(item.indexTime/0.15f, 0.0f, 1.0f);
				fIndex = item.lastIndex + (index - item.lastIndex) * alpha;
				if (alpha == 1.0f)
				{
					fIndex = (float)(item.lastIndex = index);
					item.indexTime = -1.0f;
				}
			}

			D3DXVECTOR2 pos = myThis->_hudMenu->GetAchievmentItemsPos();
			pos.y += fIndex * item.slotSize.y + item.image->GetSize().y/2;

			float flyAlpha = ClampValue(item.time/0.3f, 0.0f, 1.0f);
			float outAlpha = ClampValue((item.time - 4.7f)/0.3f, 0.0f, 1.0f);
			float pointsAlpha = ClampValue((item.time - 0.8f)/0.15f, 0.0f, 1.0f);
			float pingAlpha = ClampValue((item.time - 0.2f)/0.1f, 0.0f, 1.0f) - ClampValue((item.time - 0.3f)/0.1f, 0.0f, 1.0f);

			D3DXVec2Lerp(&pos, &item.image->GetPos(), &pos, flyAlpha);
			item.image->SetPos(pos);

			D3DXVECTOR2 imgSize = myThis->menu()->GetImageSize(item.image->GetMaterial());
			D3DXVec2Lerp(&imgSize, &imgSize, &(2.0f * imgSize), pingAlpha);
			item.image->SetSize(imgSize);

			item.image->GetMaterial().SetAlpha(1.0f - outAlpha);
			item.points->GetMaterial().SetAlpha(pointsAlpha - outAlpha);
			if (item.pointsK)
				item.pointsK->GetMaterial().SetAlpha(pointsAlpha - outAlpha);

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

	if (targetPlayer == NULL)
		return;

	float barAlpha = _carLifes[type].bar->GetFront().GetColor().a;
	if (barAlpha > 0.99f)
		_carLifes[type].bar->GetFront().SetColor(D3DXCOLOR(1, 1, 1, 0));
	else
		_carLifes[type].bar->GetFront().SetColor(D3DXCOLOR(1, 1, 1, std::min(barAlpha, 0.5f)));

	if (Object::ReplaceRef(_carLifes[type].target, targetPlayer))
	{
		_carLifes[type].target = targetPlayer;
		_carLifes[type].back->GetMaterial().SetColor(D3DXCOLOR(1, 1, 1, 0));
	}

	_carLifes[type].timer = 0;
	_carLifes[type].timeMax = carLifeTimeMax;
	_carLifes[type].back->SetVisible(true);
}

inline float StepLerp(float v1, float v2, float step)
{
	if (v2 > v1)
		return std::min(v1 + step, v2);
	else
		return std::max(v1 - step, v2);
}

void PlayerStateFrame::ProccessCarLifeBar(float deltaTime)
{
	for (int i = 0; i < cCarLifeEnd; ++i)
	{
		if (_carLifes[i].timer < 0 || _carLifes[i].target == NULL)
			continue;

		float targetAlpha = 1.0f;	

		if ((_carLifes[i].timer += deltaTime) > _carLifes[i].timeMax || _carLifes[i].target->GetCar().gameObj == NULL)
		{
			targetAlpha = _carLifes[i].back->GetMaterial().GetColor().a;
			if (_carLifes[i].target->GetCar().gameObj && targetAlpha > 0)
				targetAlpha = 0;
			else
			{
				_carLifes[i].timer = -1;
				lsl::SafeRelease(_carLifes[i].target);
				_carLifes[i].back->SetVisible(false);
				return;
			}
		}

		float value = 1.0f;
		float maxValue = _carLifes[i].target->GetCar().gameObj->GetMaxLife();
		if (maxValue > 0)
			value = _carLifes[i].target->GetCar().gameObj->GetLife() / maxValue;
		_carLifes[i].bar->SetProgress(value);

		D3DXVECTOR3 pos = _carLifes[i].target->GetCar().gameObj->GetPos() + D3DXVECTOR3(0.0f, 0.0f, 0.0f);
		D3DXVECTOR4 projVec;
		D3DXVec3Transform(&projVec, &pos, &menu()->GetGUI()->GetCamera3d()->GetContextInfo().GetViewProj());
		D3DXVECTOR2 vec = projVec / projVec.w;

		if (projVec.z < 0)
		{		
			D3DXVec2Normalize(&vec, &vec);
			vec = vec * sqrt(2.0f);
		}
		vec.x = lsl::ClampValue(vec.x, -1.0f, 1.0f);
		vec.y = lsl::ClampValue(vec.y, -1.0f, 1.0f);

		if ((abs(vec.x) == 1.0f || abs(vec.y) == 1.0f))
			targetAlpha = 0;

		D3DXVECTOR2 vpSize = menu()->GetGUI()->GetVPSize();
		vec = graph::CameraCI::ProjToView(vec, vpSize);
		vec.x = lsl::ClampValue(vec.x, 0.0f, vpSize.x - _carLifes[i].back->GetSize().x);
		vec.y = lsl::ClampValue(vec.y, _carLifes[i].back->GetSize().y, vpSize.y);

		_carLifes[i].back->SetPos(vec + D3DXVECTOR2(_carLifes[i].back->GetSize().x/2, -_carLifes[i].back->GetSize().y/2));

		float alpha = _carLifes[i].back->GetMaterial().GetColor().a;
		_carLifes[i].back->GetMaterial().SetColor(D3DXCOLOR(1, 1, 1, StepLerp(alpha, targetAlpha, deltaTime/0.3f)));

		alpha = _carLifes[i].bar->GetFront().GetColor().a;
		_carLifes[i].bar->GetFront().SetColor(D3DXCOLOR(1, 1, 1, StepLerp(alpha, targetAlpha, deltaTime/0.3f)));
	}
}

const PlayerStateFrame::CarLife* PlayerStateFrame::GetCarLife(Player* target)
{
	for (int i = 0; i < cCarLifeEnd; ++i)
		if (_carLifes[i].target == target)
			return &_carLifes[i];

	return NULL;
}

void PlayerStateFrame::InsertSlot(WeaponType type, Slot* slot)
{
	ClearSlot(type);

	LSL_ASSERT(slot);

	_weaponBox[type].slot = slot;
	_weaponBox[type].slot->AddRef();

	_weaponBox[type].box = NULL;
	if (type != wtMine && type != wtHyper)
	{
		_weaponBox[type].box = menu()->CreatePlane(_raceState, 0, "GUI\\slot.png", true, IdentityVec2, gui::Material::bmTransparency);
	}

	gui::Widget* slotParent = _weaponBox[type].box ? (gui::Widget*)_weaponBox[type].box : _raceState;
			
	_weaponBox[type].view = menu()->CreateViewPort3d(slotParent, 0, "");
	float sizeX = 60;
	_weaponBox[type].view->SetSize(D3DXVECTOR2(sizeX, sizeX));	

	_weaponBox[type].mesh = menu()->CreateMesh3d(_weaponBox[type].view, slot->GetItem().GetMesh(), slot->GetItem().GetTexture());	
	_weaponBox[type].mesh->AddRef();
	_weaponBox[type].mesh->GetMaterial()->GetSampler().SetFiltering(graph::Sampler2d::sfAnisotropic);

	_weaponBox[type].label = menu()->CreateLabel(svNull, slotParent, "Small", NullVec2, gui::Text::haCenter, gui::Text::vaCenter, clrWhite);
	_weaponBox[type].label->SetText("0/0");
	_weaponBox[type].label->SetSize(D3DXVECTOR2(30.0f, 30.0f));
}

void PlayerStateFrame::ClearSlot(WeaponType type)
{
	if (_weaponBox[type].slot)
	{
		menu()->GetGUI()->ReleaseWidget(_weaponBox[type].label);
		_weaponBox[type].label = 0;
		
		_weaponBox[type].mesh->Release();
		_weaponBox[type].mesh = 0;

		menu()->GetGUI()->ReleaseWidget(_weaponBox[type].view);
		_weaponBox[type].view = 0;

		if (_weaponBox[type].box)
		{
			menu()->GetGUI()->ReleaseWidget(_weaponBox[type].box);
			_weaponBox[type].box = 0;
		}

		_weaponBox[type].slot->Release();
		_weaponBox[type].slot = 0;
	}
}

void PlayerStateFrame::ClearSlots()
{
	for (int i = 0; i < cWeaponTypeEnd; ++i)
		ClearSlot(WeaponType(i));
}

void PlayerStateFrame::UpdateSlots()
{
	ClearSlots();

	for (int i = Player::stHyper; i <= Player::stWeapon4; ++i)
	{
		Slot* slot = player()->GetSlotInst(Player::SlotType(i));
		if (slot && slot->GetItem().IsWeaponItem() && slot->GetItem().IsWeaponItem()->GetMapObj())
			InsertSlot(WeaponType(i - Player::stHyper), slot);
	}
}	

void PlayerStateFrame::UpdateOpponents()
{
	Race* race = menu()->GetRace();

	ClearOpponents();

	for (Race::PlayerList::const_iterator iter = race->GetPlayerList().begin(); iter != race->GetPlayerList().end(); ++iter)
	{
		if (*iter == player())
			continue;

		Opponent opponent;
		opponent.player = *iter;
		opponent.player->AddRef();

		opponent.dummy = menu()->GetGUI()->CreateDummy();
		opponent.dummy->SetParent(root());		

		opponent.point = menu()->CreatePlane(opponent.dummy, NULL, "GUI\\playerPoint.png", true, IdentityVec2, gui::Material::bmTransparency);
		opponent.point->SetPos(opponent.point->GetSize().x/2, -opponent.point->GetSize().y/2);

		opponent.label = menu()->CreateLabel(svNull, opponent.dummy, "VerySmall", NullVec2, gui::Text::haCenter, gui::Text::vaCenter, clrWhite);
		opponent.label->SetPos(opponent.point->GetSize().x/2, -opponent.point->GetSize().y);		

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
	for (int i = Player::stHyper; i <= Player::stWeapon4; ++i)
	{
		Slot* slot = player()->GetSlotInst(Player::SlotType(i));
		WeaponType type = WeaponType(i - Player::stHyper);
		if (slot && _weaponBox[type].slot)
		{
			WeaponItem& weapon = slot->GetItem<WeaponItem>();
			std::stringstream sstream;
			sstream << weapon.GetCurCharge() << "/" << weapon.GetCntCharge();

			_weaponBox[type].label->SetText(sstream.str());

			if (_weaponBox[type].box)
			{
				bool sel = (i - Player::stWeapon1) == menu()->GetRace()->GetHuman()->GetCurWeapon();
				_weaponBox[type].box->GetMaterial().GetSampler().SetTex(menu()->GetTexture(sel ? "GUI\\slotSel.png" : "GUI\\slot.png"));
			}
		}
	}

	int place = player()->GetPlace();
	int cntPlayers = menu()->GetRace()->GetPlayerList().size();

	//sstream << place << '/' << cntPlayers;
	_place->SetText(GetString(lsl::StrFmt("svPlace%d", place)));

	if (player()->GetCar().gameObj)
	{
		float value = 1;
		float maxValue = player()->GetCar().gameObj->GetMaxLife();
		if (maxValue > 0)
			value = player()->GetCar().gameObj->GetLife() / maxValue;

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

	for (Opponents::iterator iter = _opponents.begin(); iter != _opponents.end(); ++iter)
	{
		Opponent& opponent = *iter;
		opponent.dummy->SetVisible(opponent.player->GetCar().gameObj != NULL);

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
			vec.x = lsl::ClampValue(vec.x, -1.0f, 1.0f);
			vec.y = lsl::ClampValue(vec.y, -1.0f, 1.0f);

			float targetAlpha = 1.0f;
			if ((abs(vec.x) == 1.0f || abs(vec.y) == 1.0f) || carLife != NULL)
				targetAlpha = std::max(opponent.point->GetMaterial().GetColor().a - 4.0f * deltaTime, 0.0f);

			D3DXVECTOR2 vpSize = menu()->GetGUI()->GetVPSize();
			vec = graph::CameraCI::ProjToView(vec, vpSize);
			vec.x = lsl::ClampValue(vec.x, 0.0f, vpSize.x - opponent.point->GetSize().x);
			vec.y = lsl::ClampValue(vec.y, -opponent.label->GetPos().y - aabb.min.y, vpSize.y);

			opponent.dummy->SetPos(vec);

			opponent.label->SetText(lsl::StrFmt(GetString("svNamePlaceMarker").c_str(), opponent.player->GetPlace(), menu()->GetString(opponent.player->GetName()).c_str()));
			opponent.center = opponent.label->GetWorldPos();
			if (carLife != NULL)
				opponent.radius = std::max(carLife->back->GetSize().x, carLife->back->GetSize().y);
			else
				opponent.radius = std::max(aabb.GetSize().x, aabb.GetSize().y);

			float alpha = 1;
			for (Opponents::iterator iter2 = _opponents.begin(); iter2 != iter; ++iter2)
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

	int ind = 0;
	for (int i = wtHyper; i <= wtMine; ++i)
	{
		if (_weaponBox[i].slot)
		{
			_weaponBox[i].view->SetPos(subWeaponPos[ind]);
			_weaponBox[i].label->SetPos(subWeaponLabelPos[ind]);
			++ind;
		}
	}

	ind = 0;
	for (int i = wtWeapon1; i <= wtWeapon4; ++i)
	{
		if (_weaponBox[i].slot)
		{
			D3DXVECTOR2 size = _weaponBox[i].box->GetSize();
			_weaponBox[i].box->SetPos(_hudMenu->GetWeaponPos() + D3DXVECTOR2(size.x/2 + ind * (size.x - 25), size.y/2));
			_weaponBox[i].view->SetPos(_hudMenu->GetWeaponBoxPos());
			_weaponBox[i].label->SetPos(_hudMenu->GetWeaponLabelPos());
			++ind;
		}
	}

	_lifeBack->SetPos(_hudMenu->GetLifeBarPos() + _lifeBack->GetSize()/2);	

	_place->SetPos(_hudMenu->GetPlacePos());

	menu()->GetUserChat()->inputPos(D3DXVECTOR2(300.0f, vpSize.y - 10.0f));
	menu()->GetUserChat()->inputSize(D3DXVECTOR2(vpSize.x - 600.0f, 300.0f));

	menu()->GetUserChat()->linesPos(D3DXVECTOR2(vpSize.x - 10.0f, _hudMenu->GetMiniMapRect().GetSize().y));
	menu()->GetUserChat()->linesSize(D3DXVECTOR2(vpSize.x/3, vpSize.y - _hudMenu->GetMiniMapRect().GetSize().y));
}

void PlayerStateFrame::OnInvalidate()
{
	UpdateSlots();

	_raceState->SetVisible(menu()->enableHUD());
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
		float alpha = _guiTimer[4]->GetMaterial().GetAlpha() - deltaTime/1.5f;

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
		Player::MyEventData* myData = static_cast<Player::MyEventData*>(data);
		NewPickItem(myData->slotType, myData->bonusType, cUndefPlayerId, false);
		return;
	}

	if (id == cAchievmentConditionComplete)
	{
		NewAchievment((AchievmentCondition::MyEventData*)data);
		return;
	}

	if (id == cPlayerDamage && data)
	{
		GameObject::MyEventData* myData = static_cast<GameObject::MyEventData*>(data);

		if (myData->damage > 0.0f && myData->targetPlayerId == player()->GetId())
			ShowCarLifeBar(clHuman, myData->targetPlayerId, 1.5f);
		else if (myData->damage > 0.0f && data->playerId == player()->GetId())
			ShowCarLifeBar(clOpponent, myData->targetPlayerId, 4.0f);

		return;
	}

	if (id == cPlayerKill && data && Race::IsHumanId(data->playerId))
	{
		GameObject::MyEventData* myData = static_cast<GameObject::MyEventData*>(data);
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

	if (raceTimerInd != -1)
	{
		D3DXVECTOR2 pos = menu()->GetGUI()->GetVPSize()/2.0f;

		for (int i = 0; i < 5; ++i)
		{
			_guiTimer[i]->SetVisible(i == raceTimerInd);
			_guiTimer[i]->SetPos(pos);
			_guiTimer[i]->GetMaterial().SetAlpha(1.0f);
		}

		return;
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

	_map = _menu->CreateViewPort3d(_root, 0, "", gui::ViewPort3d::msStatic, false);

	_lapBack = _menu->CreatePlane(_root, 0, "GUI\\lap.png", true, IdentityVec2, gui::Material::bmTransparency);	
	_lap = _menu->CreateLabel(svNull, _root, "Small", NullVec2, gui::Text::haCenter, gui::Text::vaCenter, clrWhite);

	UpdateMap();
	CreatePlayers();

	_lapBack->SetVisible(menu->enableHUD());
	_lap->SetVisible(menu->enableHUD());
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
	Nodes::iterator nextIter = iter;
	++nextIter;
	//prevIter
	Nodes::iterator prevIter = iter;
	if (iter != sIter)
		--prevIter;
	else
		prevIter = eIter;

	//вычисляем dir
	if (nextIter != eIter)
		iter->dir = nextIter->pos - iter->pos;
	else
		iter->dir = iter->pos - prevIter->pos;
	D3DXVec2Normalize(&iter->dir, &iter->dir);
	//вычисляем prevDir
	if (prevIter != eIter)
		iter->prevDir = iter->pos - prevIter->pos;
	else
		iter->prevDir = iter->dir;
	D3DXVec2Normalize(&iter->prevDir, &iter->prevDir);
	//вычисляем midDir
	iter->midDir = (iter->prevDir + iter->dir);
	D3DXVec2Normalize(&iter->midDir, &iter->midDir);
	//вычисляем midNorm
	Vec2NormCCW(iter->midDir, iter->midNorm);

	//Вычисляем _nodeRadius
	iter->cosDelta = abs(D3DXVec2Dot(&iter->dir, &iter->prevDir));
	//sinA/2 = sin(180 - D/2) = cos(D/2) = №(1 + cosD)/2
	iter->sinAlpha2 = sqrt((1.0f + iter->cosDelta) / 2.0f);
	iter->nodeRadius = 0.5f*iter->size / iter->sinAlpha2;

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
		node1.pos.y = (node1.pos.y + node2.pos.y)/2.0f;
		node2.pos.y = node1.pos.y;
	}
	if (abs(dir.y) > cosErr)
	{
		node1.pos.x = (node1.pos.x + node2.pos.x)/2.0f;
		node2.pos.x = node1.pos.x;
	}
	if (abs(node2.size - node1.size) < sizeErr)
	{
		node1.size = (node1.size + node2.size)/2.0f;
		node2.size = node1.size;
	}
}

void MiniMapFrame::BuildPath(WayPath& path, res::VertexData& data)
{
	const float cosErr = cos(20.0f * D3DX_PI/180);
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




	for (Nodes::iterator iter = nodes.begin(); iter != nodes.end();)
	{
		Node& node = *iter;
		Nodes::iterator nextIter = iter;
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
				float dAlpha = i/smSlice * alpha2;
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
	for (Nodes::iterator iter = nodes.begin(); iter != nodes.end(); ++iter, ++i)
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

	for (Race::PlayerList::const_iterator iter = _menu->GetRace()->GetPlayerList().begin(); iter != _menu->GetRace()->GetPlayerList().end(); ++iter)
	{
		gui::Plane3d* plane = _menu->GetGUI()->GetContext().CreatePlane3d();		
		plane->SetSize(D3DXVECTOR2(10.0f, 10.0f));
		_map->GetBox()->InsertChild(plane);

		plane->GetMaterial()->SetColor(color[_players.size() % 4]);
		plane->GetMaterial()->GetSampler().SetTex(_menu->GetTexture("GUI\\playerPoint2.png"));
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
	for (Players::iterator iter = _players.begin(); iter != _players.end(); ++iter)
	{
		D3DXVECTOR3 pos3 = iter->player->GetCar().GetMapPos();
		pos3.z = 0.0f;
		iter->plane->SetPos(pos3);
		iter->plane->GetMaterial()->SetColor(iter->player->GetCar().color);
	}
}

void MiniMapFrame::UpdateMap()
{
	const float cosErr = cos(20.0f * D3DX_PI/180);
	const float sizeErr = 2.0f;

	for (Trace::Pathes::const_iterator iter = GetTrace()->GetPathes().begin(); iter != GetTrace()->GetPathes().end(); ++iter)
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
			buf3d->GetMaterial()->GetSampler().SetTex(_menu->GetTexture("GUI\\mapFrame.png"));

			_map->GetBox()->InsertChild(buf3d);
		}
	}
	
	_map->SetAlign(true);

	WayNode* node = GetTrace()->GetPathes().front()->GetFirst();

	gui::Plane3d* start = _menu->CreatePlane3d(_map, "GUI\\start.png", IdentityVec2);
	start->SetPos(node->GetPos());

	D3DXQUATERNION rot;
	QuatShortestArc(XVector, D3DXVECTOR3(node->GetTile().GetDir().x, node->GetTile().GetDir().y, 0.0f), rot);
	start->SetRot(rot);
	start->SetSize(D3DXVECTOR2(node->GetSize()/4.0f, node->GetSize()/2.0f));
}

Trace* MiniMapFrame::GetTrace()
{
	return _menu->GetTrace();
}

void MiniMapFrame::AdjustLayout(const D3DXVECTOR2& vpSize)
{
	AABB2 mapRect = _hudMenu->GetMiniMapRect();

	//_map->SetPos(mapRect.GetCenter());
	_map->SetSize(mapRect.GetSize());
	_map->SetPos(vpSize.x - mapRect.max.x, -mapRect.min.y);

	_lapBack->SetPos(_hudMenu->GetLapPos() + D3DXVECTOR2(_lapBack->GetSize().x/2, 0));
	_lap->SetPos(D3DXVECTOR2(_lapBack->GetPos().x - 10, _hudMenu->GetLapPos().y + 1));
}

void MiniMapFrame::Show(bool value)
{
	_root->SetVisible(value);
}

bool MiniMapFrame::IsVisible() const
{
	return _root->GetVisible();
}

void MiniMapFrame::OnProgress(float deltaTime)
{
	UpdatePlayers(deltaTime);

	unsigned lapsCount = _menu->GetRace()->GetTournament().GetCurTrack().GetLapsCount();
	unsigned numLaps = std::min(_hudMenu->GetPlayer()->GetCar().numLaps + 1, lapsCount);

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
	if (sender == _menu->GetAcceptSender())
	{
		_menu->ShowCursor(false);
		_menu->Pause(false);		

		if (_menu->GetAcceptResultYes())
		{
			Menu* menu = _menu;			

			if (menu->IsNetGame() && menu->GetNet()->isClient())
			{
				menu->ExitRace();
				menu->ExitMatch();
			}
			else
				menu->ExitRace();
		}
		else
			_menu->HideAccept();

		return true;
	}

	return false;
}

bool HudMenu::OnHandleInput(const InputMessage& msg)
{
	if (msg.action == gaEscape && msg.state == ksDown && !msg.repeat)
	{
		if (!_menu->IsPaused())
		{
			_menu->ShowAccept(_menu->GetString(svHintExitRace), _menu->GetString(svYes), _menu->GetString(svNo), _menu->GetGUI()->GetVPSize()/2.0f, gui::Widget::waCenter, this);
			_menu->ShowCursor(true);
			_menu->Pause(true);
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
	if (_miniMapFrame->IsVisible())
		_miniMapFrame->OnProgress(deltaTime);

	if (_playerStateFrame->visible())
		_playerStateFrame->OnProgress(deltaTime);
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

AABB2 HudMenu::GetMiniMapRect()
{
	D3DXVECTOR2 size(320.0f, 320.0f);

	return AABB2(size);
}

D3DXVECTOR2 HudMenu::GetWeaponPos()
{
	return D3DXVECTOR2(155.0f, 50.0f);
}

D3DXVECTOR2 HudMenu::GetWeaponBoxPos()
{
	return D3DXVECTOR2(5.0f, -15.0f);
}

D3DXVECTOR2 HudMenu::GetWeaponLabelPos()
{
	return D3DXVECTOR2(-10.0f, 26.0f);
}

D3DXVECTOR2 HudMenu::GetWeaponPosMine()
{
	return D3DXVECTOR2(30.0f, 140.0f);
}

D3DXVECTOR2 HudMenu::GetWeaponPosMineLabel()
{
	return D3DXVECTOR2(105.0f, 159.0f);
}

D3DXVECTOR2 HudMenu::GetWeaponPosHyper()
{
	return D3DXVECTOR2(30.0f, 32.0f);
}

D3DXVECTOR2 HudMenu::GetWeaponPosHyperLabel()
{
	return D3DXVECTOR2(105.0f, 15.0f);
}

D3DXVECTOR2 HudMenu::GetPlacePos()
{
	return D3DXVECTOR2(105, 88);
}

D3DXVECTOR2 HudMenu::GetLapPos()
{
	return D3DXVECTOR2(0, 200);
}

D3DXVECTOR2 HudMenu::GetLifeBarPos()
{
	return D3DXVECTOR2(165.0f, 0.0f);
}

D3DXVECTOR2 HudMenu::GetPickItemsPos()
{
	return D3DXVECTOR2(0.0f, 255.0f);
}

D3DXVECTOR2 HudMenu::GetAchievmentItemsPos()
{
	return D3DXVECTOR2((100.0f + _menu->GetGUI()->GetVPSize().x)/2, 15.0f);
}

D3DXVECTOR2 HudMenu::GetCarLifeBarPos()
{
	return D3DXVECTOR2(4.0f, -10.0f);
}

}

}