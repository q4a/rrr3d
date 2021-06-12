#include "stdafx.h"
#include "game\Menu.h"

#include "game\FinishMenu.h"

namespace r3d
{

namespace game
{

namespace n
{
	
FinishMenu::FinishMenu(Menu* menu, gui::Widget* parent): MenuFrame(menu, parent), _time(-1.0f), _playerCount(0)
{
	D3DXCOLOR color1(0xffe9a73f);
	D3DXCOLOR color2(0xffe1e1e1);
	D3DXCOLOR color3(0xff84bc43);

	StringValue strLabels[cLabelEnd] = {svNull, svPrice, svNull, svNull};
	std::string fontLabels[cLabelEnd] = {"Header", "Header", "Header", "Header"};
	gui::Text::HorAlign horLabels[cLabelEnd] = {gui::Text::haCenter, gui::Text::haCenter, gui::Text::haCenter, gui::Text::haCenter};
	gui::Text::VertAlign vertLabels[cLabelEnd] = {gui::Text::vaCenter, gui::Text::vaCenter, gui::Text::vaCenter, gui::Text::vaCenter};
	D3DXCOLOR colorLabels[cLabelEnd] = {color1, color1, color2, color3};

	for (int i = 0; i < cBoxCount; ++i)
	{
		_boxes[i].leftFrame = menu->CreatePlane(root(), this, "GUI\\playerLeftFrame.png", true);
		_boxes[i].leftFrame->SetAlign(gui::Widget::waLeftTop);
		
		_boxes[i].rightFrame = menu->CreatePlane(_boxes[i].leftFrame, this, "GUI\\playerRightFrame.png", true);		
		_boxes[i].rightFrame->SetAlign(gui::Widget::waLeftTop);

		_boxes[i].lineFrame = menu->CreatePlane(_boxes[i].leftFrame, this, "GUI\\playerLineFrame.png", true);
		_boxes[i].lineFrame->SetAlign(gui::Widget::waLeftTop);

		_boxes[i].cup = menu->CreatePlane(_boxes[i].rightFrame, this, lsl::StrFmt("GUI\\cup%d.dds", i + 1), true, IdentityVec2, gui::Material::bmTransparency);
		_boxes[i].cup->SetSize(menu->StretchImage(_boxes[i].cup->GetMaterial(), D3DXVECTOR2(190.0f, 160.0f), true, false, true, false));

		_boxes[i].photo = menu->CreatePlane(_boxes[i].leftFrame, this, "", true, IdentityVec2, gui::Material::bmTransparency);

		for (int j = 0; j < cLabelEnd; ++j)	
			_boxes[i].labels[j] = menu->CreateLabel(strLabels[j], _boxes[i].leftFrame, fontLabels[j], NullVec2, horLabels[j], vertLabels[j], colorLabels[j]);
		_boxes[i].labels[mlPriceInfo]->SetText(menu->GetString(svMoney) + "\n" + menu->GetString(svPoints));

		_boxes[i].duration = 0.0f;
	}
}

FinishMenu::~FinishMenu()
{
	menu()->GetControl()->RemoveEvent(this);

	for (int i = 0; i < cBoxCount; ++i)
		menu()->ReleaseWidget(_boxes[i].leftFrame);
}

void FinishMenu::OnShow(bool value)
{
	_time = 0.0f;
	for (int i = 0; i < cBoxCount; ++i)
		_boxes[i].leftFrame->SetVisible(false);

	if (value)
		menu()->GetControl()->InsertEvent(this);
	else
		menu()->GetControl()->RemoveEvent(this);
}

void FinishMenu::OnAdjustLayout(const D3DXVECTOR2& vpSize)
{
	D3DXVECTOR2 leftSize = _boxes[0].leftFrame->GetSize();
	float offsetY = (vpSize.y - (cBoxCount * leftSize.y))/2.0f;
	float leftOffsetX = (leftSize.x + vpSize.x/2)/2;
	float rightOffsetX = (vpSize.x/2 + vpSize.x - leftSize.x)/2;

	for (int i = 0; i < cBoxCount; ++i)
	{
		_boxes[i].leftFrame->SetPos(0.0f, offsetY + i * leftSize.y);
		_boxes[i].rightFrame->SetPos(vpSize.x - leftSize.x, 0.0f);

		_boxes[i].lineFrame->SetPos(leftSize.x, 0.0f);
		_boxes[i].lineFrame->SetSize(vpSize.x - 2.0f * leftSize.x, leftSize.y);

		_boxes[i].photo->SetPos(D3DXVECTOR2(128.0f, 116.0f));
		_boxes[i].cup->SetPos(D3DXVECTOR2(160.0f, 115.0f));

		_boxes[i].labels[mlName]->SetPos(leftOffsetX, 63.0f);
		_boxes[i].labels[mlPriceInfo]->SetPos(leftOffsetX, 154.0f);
		_boxes[i].labels[mlPrice]->SetPos(rightOffsetX, 63.0f);
		_boxes[i].labels[mlPriceVal]->SetPos(rightOffsetX, 154.0f);
	}
}

void FinishMenu::OnInvalidate()
{
	const Race::Results& results = menu()->GetRace()->GetResults();
	_playerCount = std::min(results.size(), (unsigned)cBoxCount);

	for (unsigned i = 0; i < cBoxCount; ++i)
	{
		Box& box = _boxes[i];

		if (i < results.size())
		{
			const Race::Result& result = results[i];

			Player* player = menu()->GetRace()->GetPlayerById(result.playerId);
			if (player == NULL)
			{
				box.leftFrame->SetVisible(false);
				continue;
			}

			box.leftFrame->SetVisible(_time == -1.0f);

			box.photo->GetMaterial().GetSampler().SetTex(player->GetPhoto());
			box.photo->SetSize(menu()->StretchImage(box.photo->GetMaterial(), D3DXVECTOR2(198.0f, 193.0f), true, false, true, false));

			box.labels[mlName]->SetText(GetString(player->GetName()));

			if (result.pickMoney > 0)
				box.labels[mlPriceVal]->SetText(lsl::StrFmt("%d + %d\n%d", result.money, result.pickMoney, result.points));
			else
				box.labels[mlPriceVal]->SetText(lsl::StrFmt("%d\n%d", result.money, result.points));

			box.duration = result.voiceNameDur;
			box.plrId = result.playerId;
		}
		else
			box.leftFrame->SetVisible(false);
	}
}

bool FinishMenu::OnMouseClickEvent(const MouseClick& mClick)
{
	if (mClick.state == ksDown && mClick.key == mkLeft)
	{
		menu()->OnFinishClose();
		return true;
	}

	return false;
}

bool FinishMenu::OnHandleInput(const InputMessage& msg)
{
	if (msg.state != ksDown)
		return false;

	if (msg.action == gaEscape || msg.action == gaAction)
	{
		menu()->OnFinishClose();
		return true;
	}

	return false;
}

void FinishMenu::OnProgress(float deltaTime)
{
	const float cDelay = 0.15f;
	unsigned playerFinish[4] = {cPlayerFinishFirst, cPlayerFinishSecond, cPlayerFinishThird, cPlayerFinishLast};

	if (_time >= 0.0f)
	{
		float summDuration = 0.0f;
		D3DXVECTOR2 vpSize = menu()->GetGUI()->GetVPSize();

		for (unsigned i = 0; i < _playerCount; ++i)
		{
			const Box& box = _boxes[i];

			float alpha = lsl::ClampValue((_time - summDuration - cDelay)/0.5f, 0.0f, 1.0f);
			summDuration += _boxes[i].duration;

			bool setVisible = alpha > 0.0f;
			if (!box.leftFrame->GetVisible() && setVisible)
				menu()->SendEvent(playerFinish[i], &EventData(box.plrId));
			box.leftFrame->SetVisible(setVisible);

			D3DXVECTOR2 vec = box.leftFrame->GetPos();
			vec.x = (1.0f - alpha) * (vpSize.x + 25.0f) * (i % 2 == 1 ? 1 : -1);
			box.leftFrame->SetPos(vec);
		}

		if (_time >= summDuration + cDelay)
		{
			_time = -1.0f;
			const Race::Results& results = menu()->GetRace()->GetResults();
			if (results.size() >= 4)
				menu()->SendEvent(playerFinish[3], &EventData(results[results.size() - 1].playerId));
		}
		else
			_time += deltaTime;
	}
}

}

}

}