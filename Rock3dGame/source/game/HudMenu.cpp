#include "stdafx.h"

#include "game/Menu.h"
#include "game/HudMenu.h"

namespace r3d
{
	namespace game
	{
		PlayerStateFrame::PlayerStateFrame(Menu* menu, HudMenu* hudMenu, gui::Widget* parent): MenuFrame(menu, parent),	_hudMenu(hudMenu)
		{
			const std::string imgTime[5] = {"GUI\\HUD\\tablo0.png", "GUI\\HUD\\tablo1.png", "GUI\\HUD\\tablo2.png", "GUI\\HUD\\tablo3.png",	"GUI\\HUD\\tablo4.png"};
			const std::string imgLapz[9] = {"GUI\\HUD\\lapz0.png", "GUI\\HUD\\lapz1.png", "GUI\\HUD\\lapz2.png", "GUI\\HUD\\lapz3.png", "GUI\\HUD\\lapz4.png", "GUI\\HUD\\lapz5.png", "GUI\\HUD\\lapz6.png", "GUI\\HUD\\lapz7.png"};
			Race* race = menu->GetRace();			

			for (int i = 0; i < 5; ++i)
			{
				_guiTimer[i] = menu->CreatePlane(root(), nullptr, imgTime[i], true, IdentityVec2 / 2.0f, gui::Material::bmTransparency);
				_guiTimer[i]->SetFlag(gui::Widget::wfTopmost, true);
				_guiTimer[i]->SetVisible(false);

			}

			for (int i = 0; i < 8; ++i)
			{
				_raceState[i] = menu->CreatePlane(root(), nullptr, imgLapz[i], true, IdentityVec2, gui::Material::bmTransparency);
				_raceState[i]->SetVisible(false);
			}

			_lifeBar = menu->CreateBar(_raceState[0], nullptr, "GUI\\HUD\\lifeBar.png", "");
			_lifeBar2 = menu->CreateBar(_raceState[0], nullptr, "GUI\\HUD\\lifeBar2.png", "");

			menu->ShowChat(true);
			menu->RegUser(this);
		}

		PlayerStateFrame::~PlayerStateFrame()
		{
			menu()->UnregUser(this);

			for (int i = 0; i < 5; ++i)
			{
				menu()->GetGUI()->ReleaseWidget(_guiTimer[i]);
			}

			menu()->GetGUI()->ReleaseWidget(_lifeBar);
			menu()->GetGUI()->ReleaseWidget(_lifeBar2);
		}

		void PlayerStateFrame::OnAdjustLayout(const D3DXVECTOR2& vpSize)
		{
			X_VPSIZE = vpSize.x;
			Y_VPSIZE = vpSize.y;
			
			D3DXVECTOR2 hudPos;
			hudPos.x = 180.0f;
			hudPos.y = vpSize.y - 150.0f;

			if (menu()->StyleHUD() == 2)
				hudPos.x = vpSize.x - 180.0f;

			for (int i = 0; i < 8; ++i)
			{
				_raceState[i]->SetPos(hudPos);
			}

			_lifeBar2->SetPos(0, 50);


			//300 200
			if (menu()->GetRace()->IsFriendship() && FIXED_ASPECT == true)
			{
				//Fix HUD Size and Position in Splitscreen:
				if (SPLIT_TYPE == 1)
				{
					for (int i = 0; i < 8; ++i)
					{
						_raceState[i]->SetSize(150, 200);
						_lifeBar->SetSize(150, 200);
						_lifeBar2->SetSize(150, 200);
					}
				}

				if (SPLIT_TYPE == 2)
				{
					for (int i = 0; i < 8; ++i)
					{
						_raceState[i]->SetSize(300, 100);
						_lifeBar->SetSize(300, 100);
						_lifeBar2->SetSize(300, 100);
						_raceState[i]->SetPos(hudPos.x, hudPos.y + 30.0f);
						_lifeBar2->SetPos(0, 30);
					}

					for (int i = 0; i < 5; ++i)
					{
						_guiTimer[i]->SetSize(_guiTimer[i]->GetSize().x, _guiTimer[i]->GetSize().y / 2);
					}
				}
			}
		}

		void PlayerStateFrame::UpdateState(float deltaTime)
		{
			Player* plr = menu()->GetPlayer();
			Player* plr2 = menu()->GetPlayer();
			Player* leader = menu()->GetPlayer();


			if (menu()->GetRace()->IsFriendship() == true && menu()->StyleHUD() > 0)
			{
				plr2 = menu()->GetSecondPlayer();
				if (plr2 != nullptr)
				{
					if (plr->GetPlace() < plr2->GetPlace())
						leader = plr;
					else
						leader = plr2;

					if (plr->GetCar().gameObj && plr2->GetCar().gameObj && plr->GetFinished() == false && plr2->GetFinished() == false)
					{
						float valueSec = 1;
						float maxValueSec = plr2->GetCar().gameObj->GetMaxLife();
						if (maxValueSec > 0)
							valueSec = plr2->GetCar().gameObj->GetLife() / maxValueSec;

						_lifeBar->SetProgress(valueSec);

						unsigned lapsCount = leader->GetRace()->GetTournament().GetCurTrack().GetLapsCount();
						unsigned numLaps = leader->GetCar().numLaps;
						unsigned RestLap = (lapsCount - numLaps) - 1;

						for (int i = 0; i < 8; ++i)
						{
							_raceState[i]->SetVisible(false);
							if (i == RestLap)
							{
								_raceState[i]->SetVisible(true);
								_lifeBar->SetParent(_raceState[i]);
								_lifeBar->SetVisible(true);
								_lifeBar2->SetParent(_raceState[i]);
							}
						}
					}
				}
			}
			else
				_lifeBar2->SetVisible(false);		

			if (plr != nullptr && plr->GetFinished() == false && plr->GetCar().gameObj && menu()->StyleHUD() > 0)
			{
				float value = 1;
				float maxValue = plr->GetCar().gameObj->GetMaxLife();
				if (maxValue > 0)
					value = plr->GetCar().gameObj->GetLife() / maxValue;

				if (menu()->GetRace()->IsFriendship())
					_lifeBar2->SetProgress(value);
				else
				{
					_lifeBar->SetProgress(value);
					_lifeBar->SetVisible(true);
					_lifeBar->GetParent()->SetVisible(true);
				}
			}

		}

		void PlayerStateFrame::OnProgress(float deltaTime)
		{		
			UpdateState(deltaTime);

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
				D3DXVECTOR2 pos = menu()->GetGUI()->GetVPSize() / 2.0f;

				for (int i = 0; i < 5; ++i)
				{
					_guiTimer[i]->SetVisible(i == raceTimerInd);
					_guiTimer[i]->SetPos(pos);
					_guiTimer[i]->GetMaterial().SetAlpha(1.0f);
				}

				return;
			}
		}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		MiniMapFrame::MiniMapFrame(Menu* menu, HudMenu* hudMenu, gui::Widget* parent): _menu(menu), _hudMenu(hudMenu)
		{
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

			_root = _menu->GetGUI()->CreateDummy();
			_root->SetParent(parent);
			_map = _menu->CreateViewPort3d(_root, nullptr, "", gui::ViewPort3d::msStatic, false);

			UpdateMap();
			CreatePlayers();
		}

		MiniMapFrame::~MiniMapFrame()
		{
			ClearPlayers();

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

					buf3d->GetMaterial()->SetBlending(gui::Material::bmTransparency);
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
			AABB2 mapRect = _hudMenu->GetMiniMapRectA();
			if (MM_STYLE == 1)		
				mapRect = _hudMenu->GetMiniMapRectA();			
			else if (MM_STYLE == 2)			
				mapRect = _hudMenu->GetMiniMapRectB();			
			else if (MM_STYLE == 3)			
				mapRect = _hudMenu->GetMiniMapRectC();			
			else if (MM_STYLE == 4)			
				 mapRect = _hudMenu->GetMiniMapRectD();			
			else if (MM_STYLE == 5)			
				 mapRect = _hudMenu->GetMiniMapRectE();			
			else if (MM_STYLE == 6)			
				mapRect = _hudMenu->GetMiniMapRectF();			
			else if (MM_STYLE == 7)			
				mapRect = _hudMenu->GetMiniMapRectG();			
			else if (MM_STYLE == 8)			
				mapRect = _hudMenu->GetMiniMapRectH();			
			else if (MM_STYLE == 9)			
				mapRect = _hudMenu->GetMiniMapRectI();	
			else if (MM_STYLE == 10)			
				mapRect = _hudMenu->GetMiniMapRectJ();			
			else if (MM_STYLE == 11)			
				mapRect = _hudMenu->GetMiniMapRectK();			
			else			
				mapRect = _hudMenu->GetMiniMapRectF();


			_map->SetSize(mapRect.GetSize());
			_map->SetPos(vpSize.x - mapRect.max.x, -mapRect.min.y);

			if (FIXED_ASPECT == true && _menu->GetRace()->IsFriendship())
			{
				//Fix MiniMap Size and Position in Splitscreen:
				if (SPLIT_TYPE == 1)
				{
					_map->SetSize(_map->GetSize().x / 2, _map->GetSize().y);
					_map->SetPos(_map->GetPos().x + (_map->GetSize().x / 2), _map->GetPos().y);
				}

				if (SPLIT_TYPE == 2)
				{
					_map->SetSize(_map->GetSize().x, _map->GetSize().y / 2);
					_map->SetPos(_map->GetPos().x, _map->GetPos().y - (_map->GetSize().y / 2));
				}
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
					_menu->HideAccept();
				

				return true;
			}

			return false;
		}

		bool HudMenu::OnHandleInput(const InputMessage& msg)
		{
			if (msg.action == gaScreenMode && msg.state == ksDown && !msg.repeat)
			{
				Player* plr = _menu->GetRace()->GetPlayerById(Race::cHuman);

				_menu->GetRace()->QuickFinish(plr);
				_menu->GetRace()->RaceClear();
				_menu->SetState(Menu::msRace2);
				SPLIT_TYPE = 0;
				FIXED_ASPECT = false;
				_menu->SetState(Menu::msInfo);

				return true;
			}


			if (msg.action == gaEscape && msg.state == ksDown && !msg.repeat)
			{
				Player* plr = _menu->GetRace()->GetPlayerById(Race::cHuman);

				if (plr && plr->GetCar().gameObj != nullptr)
					plr->GetCar().gameObj->GetGrActor().SetVisible(true);

				if (!_menu->IsPaused())
				{
					if (ENABLE_SPLIT_SCREEN == false)
					{
						_menu->ShowAccept(false, _menu->GetString(svHintExitRace), _menu->GetString(svYes), _menu->GetString(svNo), _menu->GetGUI()->GetVPSize() / 2.0f, gui::Widget::waCenter, this);
						_menu->ShowCursor(true);
					}					
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
			Menu* menu = _menu;

			if (_miniMapFrame->IsVisible())
			{
				_miniMapFrame->OnProgress(deltaTime);
			}

			if (_playerStateFrame->visible())
			{
				_playerStateFrame->OnProgress(deltaTime);
			}

			if (_menu->GetRace() && _menu->GetRace()->GetPlayerById(Race::cHuman) != nullptr)
			{
				Player* plr = _menu->GetRace()->GetPlayerById(Race::cHuman);
				if (plr && plr->GetSuicide() && plr->GetCar().mapObj)
				{
					if (menu->IsNetGame() && menu->GetNet()->isClient())
					{
						menu->ExitRace();
						menu->ExitMatch();
					}
					else
					{
						menu->GetRace()->QuickFinish(plr);
						menu->GetRace()->RaceClear();
						menu->SetState(Menu::msRace2);
						SPLIT_TYPE = 0;
						menu->SetState(Menu::msInfo);
					}
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
	}
}
