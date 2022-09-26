#include "stdafx.h"
#include "game/AICar.h"

#include "game/World.h"

namespace r3d
{
	namespace game
	{
		const float AICar::cSteerAngleBias = D3DX_PI / 128.0f;
		const float AICar::cMaxSpeedBlocking = 0.5f;
		const float AICar::cMaxTimeBlocking = 1.0f;

		const float AICar::cMaxVisibleDistShot = 100.0f;


		AICar::AICar(Player* player): _player(player), _path(_player->GetMap()->GetTrace().cTrackCnt), _enbAI(true)
		{
			_player->GetRace()->GetGame()->RegUser(this);
		}

		AICar::~AICar()
		{
			_player->GetRace()->GetGame()->UnregUser(this);
		}

		AICar::PathState::PathState(unsigned trackCnt): curTile(nullptr), nextTile(nullptr), curNode(nullptr),
		                                                dirArea(0), _break(false),
		                                                cTrackCnt(trackCnt)
		{
			freeTracks.resize(cTrackCnt);
			lockTracks.resize(cTrackCnt);
		}

		AICar::PathState::~PathState()
		{
			SetCurTile(nullptr);
			SetNextTile(nullptr);
		}

		bool AICar::PathState::FindFirstUnlockTrack(unsigned track, unsigned target, unsigned& res)
		{
			const int inc = target > track ? +1 : -1;

			for (int i = 1; i < abs(static_cast<int>(track - target)) + 1; ++i)
			{
				const unsigned testTrack = track + i * inc;
				if (!lockTracks[testTrack])
				{
					res = testTrack;
					return true;
				}
			}

			return false;
		}

		bool AICar::PathState::FindLastUnlockTrack(unsigned track, unsigned target, unsigned& res)
		{
			const int inc = target > track ? +1 : -1;

			for (int i = 1; i < abs(static_cast<int>(track - target)) + 1; ++i)
			{
				const unsigned testTrack = track + i * inc;
				if (lockTracks[testTrack])
					return i > 1;

				res = testTrack;
			}

			return track != target;
		}

		bool AICar::PathState::FindFirstSiblingUnlock(unsigned track, unsigned& res)
		{
			if (track > cTrackCnt - 1)
				return false;

			const int down = track - 1;
			if (down >= 0 && !lockTracks[down])
			{
				res = down;
				return true;
			}

			const unsigned up = track + 1;
			if (up < cTrackCnt && !lockTracks[up])
			{
				res = up;
				return true;
			}

			return false;
		}

		bool AICar::PathState::FindLastSiblingUnlock(unsigned track, unsigned& res)
		{
			if (track > cTrackCnt - 1)
				return false;

			for (unsigned i = 1; i < cTrackCnt; ++i)
			{
				const int down = track - i;
				if (down >= 0 && !lockTracks[down])
				{
					res = down;
					return true;
				}

				const unsigned up = track + i;
				if (up < cTrackCnt && !lockTracks[up])
				{
					res = up;
					return true;
				}
			}

			return false;
		}

		void AICar::PathState::ComputeMovDir(AICar* owner, float deltaTime, const Player::CarState& car)
		{
			LSL_ASSERT(curTile && nextTile);

			unsigned newTrack = car.track;
			//������� ������
			const bool onLeft = D3DXVec2CCW(&curTile->GetTile().GetDir(), &nextTile->GetTile().GetDir()) > 0;
			//
			const WayNode* movNode = curTile;
			//
			dirArea = 5.0f + abs(car.speed) * car.kSteerControl * 10;

			//������������� ���������� �������� ������������ ��������
			if (nextTile->GetTile().GetTurnAngle() > D3DX_PI / 12)
			{
				const float edgeDist = Line2DistToPoint(nextTile->GetTile().GetEdgeLine(), car.pos);
				if (edgeDist < car.size)
				{
					const float tileWidth = nextTile->GetPoint()->GetSize() / cTrackCnt;
					const D3DXVECTOR2 targPnt = nextTile->GetPos2() + nextTile->GetTile().GetEdgeNorm() * tileWidth *
						static_cast<float>(cTrackCnt) / 2.0f - car.pos;
					const float proj = D3DXVec2Dot(&curTile->GetTile().GetDir(), &targPnt);

					if (proj < car.size)
						movNode = nextTile;
					else
					{
						dirArea = proj;

						if (onLeft)
							newTrack = 0;
						else
							newTrack = cTrackCnt - 1;
					}
				}
			}

			//������ ���������������
			//���� � �������� ���������� ���� �������� ���� ���� ��������� ����
			WayNode* curBreakTile = curNode ? curNode : nextTile;
			//
			if (curBreakTile->GetTile().GetTurnAngle() > D3DX_PI / 12)
			{
				const float kLong = car.kSteerControl * car.kSteerControl;
				const float normDist = Line2DistToPoint(curBreakTile->GetTile().GetMidNormLine(), car.pos);
				const float kBreak = -normDist + curBreakTile->GetPoint()->GetSize();

				float kRot = D3DXVec2Dot(&car.dir, &curBreakTile->GetTile().GetDir());
				kRot = 1.0f - std::max(kRot, 0.0f);

				//������� ��������� � ���������� ����� ��� ������ �� ����
				if (!_break && car.speed * car.speed * kLong * kRot > 1.5f * kBreak)
					_break = true;
				else if (_break && car.speed * car.speed * kLong * kRot < 1.0f * kBreak)
					_break = false;
			}
			else
				_break = false;

			//������������� newTrack � ������ ��������������� �����
			{
				//�� ��������� �������� �� car.track � ������ ������
				unsigned res = car.track;
				//����� ��������� ��������� ������� � ����������� ��������
				if (FindLastUnlockTrack(car.track, newTrack, res))
				{
					//Nothing
				}
				//�������, ���� ������� car.track �������������, ������� ������� �� ������
				else if (lockTracks[car.track])
				{
					//����� ������ ���������� ������� �� car.track
					FindFirstSiblingUnlock(car.track, res);
				}

				newTrack = res;
			}

			//������ ���������� ��������
			{
				const D3DXVECTOR2 dir = movNode->GetTile().GetDir();
				D3DXVECTOR2 target = car.pos + dir * dirArea;
				target += movNode->GetTile().ComputeTrackNormOff(target, newTrack);

				moveDir = target - car.pos;
				D3DXVec2Normalize(&moveDir, &moveDir);
			}
		}

		void AICar::PathState::Update(AICar* owner, float deltaTime, const Player::CarState& car)
		{
			if (car.curTile != curTile)
			{
				if (car.lastNode &&
					(car.curTile == nullptr ||
						(car.lastNode->GetNext() != car.curTile && car.lastNode != car.curTile && car.lastNode->
							GetTile().IsContains(car.pos3, true, nullptr, 5.0f)))
				)
					SetCurTile(car.lastNode);
				else
					SetCurTile(car.curTile);
				SetNextTile(nullptr);
			}

			if (curTile && nextTile == nullptr)
			{
				//�������� �������� ��������������� ����������� ���� �������������� WayPoint-�
				//���� ���������� ��������� ����, �� � ������ ������� �������� �� ����
				if (curTile->GetNext())
					SetNextTile(curTile->GetNext());
					//����� �������� ����� ����������� �� �������� ����
				else
					SetNextTile(curTile->GetPoint()->GetRandomNode(curTile, true));

				//���� �� ��������� �����. � ��������� ���������� ��������, �����  ��� ���������� ����� ������� ����� ����� �� ������� ��� ������ �� ��������� ����, ��������� � ������ ������ ������ ����� ���������� ���� ����� ���� ������ ��������� ������, � ������� ������ ����� ������ ���� �� ����������� �� ���� ���� � �� ����
				//�������� ����� ����������� �� �������� ����, ���� �� � ������ ��� ���� �������� ���� �� ��������� ����
				/*if (curTile->GetNext() == NULL || Random() < cSecretPathChance)
				{
					WayNode* nextTile = curTile->GetNext() != NULL ? curTile->GetNext()->GetPoint()->GetRandomNode(curTile->GetNext(), true) : curTile->GetPoint()->GetRandomNode(curTile, true);
		
					if (nextTile)
					{
						SetCurTile(nextTile);
						SetNextTile(nextTile);
					}
					else
						SetNextTile(curTile->GetNext());
				}
				else
					SetNextTile(curTile->GetNext());*/
			}

			curNode = curTile && curTile->IsContains2(car.pos) ? curTile : nullptr;

			moveDir = NullVec2;
			if (nextTile)
				ComputeMovDir(owner, deltaTime, car);
		}

		void AICar::PathState::SetCurTile(WayNode* value)
		{
			if (ReplaceRef(curTile, value))
				curTile = value;
		}

		void AICar::PathState::SetNextTile(WayNode* value)
		{
			if (ReplaceRef(nextTile, value))
				nextTile = value;
		}

		AICar::AttackState::AttackState(): target(nullptr), backTarget(nullptr), placeMineRandom(-1.0f)
		{
		}

		AICar::AttackState::~AttackState()
		{
			SetTarget(nullptr);
			SetBackTarget(nullptr);
		}

		//���� ����� ��������� ����������
		Player* AICar::AttackState::FindEnemy(const AICar* owner, const Player::CarState& car, int dir,
		                                      Player* currentEnemy)
		{
			//���� ����� ��� ����� ��������� ����
			Player* enemy = owner->_player->FindClosestEnemy(D3DX_PI / 4 * dir, true);

			if (currentEnemy && currentEnemy == enemy)
				return enemy;

			//��������� ������� ����
			currentEnemy = currentEnemy && car.curTile && car.curTile->GetTile().IsZLevelContains(
				               currentEnemy->GetCar().pos3)
				               ? currentEnemy
				               : nullptr;

			//������� �� �������� target ��� ������� �� target->GetCar().size
			const bool testEnemy = enemy && (!currentEnemy || D3DXVec2Length(
				&(currentEnemy->GetCar().pos - enemy->GetCar().pos)) > currentEnemy->GetCar().size);

			return testEnemy ? enemy : currentEnemy;
		}

		void AICar::AttackState::ShotByEnemy(AICar* owner, const CarState& car, const Player* enemy) const
		{
			//���������� �� ������������� � �������� enemy->GetCar().radius
			bool bShoot = enemy && abs(Line2DistToPoint(car.dirLine, enemy->GetCar().pos)) < enemy->GetCar().radius;
			//������ � �������� z ������������
			bShoot = bShoot && abs(car.pos3.z - enemy->GetCar().pos3.z) < std::min(car.radius, enemy->GetCar().radius);
			//
			if (bShoot)
			{
				struct Weapon
				{
					Player::SlotType slot;
					WeaponItem* wpn;

					Weapon(Player::SlotType mSlot, WeaponItem* mWpn): slot(mSlot), wpn(mWpn)
					{
					}
				};
				using WeaponList = Vector<Weapon>;

				WeaponList weaponList;

				const float distToTarget = D3DXVec2Length(&(enemy->GetCar().pos - car.pos));
				int wpnCount = 0;

				for (int i = Player::stWeapon1; i <= Player::stWeapon4; ++i)
				{
					Slot* slot = owner->_player->GetSlotInst(static_cast<Player::SlotType>(i));
					WeaponItem* weapon = slot ? slot->GetItem().IsWeaponItem() : nullptr;
					if (weapon == nullptr)
						continue;

					const Proj::Type projType = weapon->GetDesc().Front().type;
					if (projType == Proj::ptHyper || projType == Proj::ptMine)
						continue;

					float static_delay = 0.25f;

					//if at least one weapon is not ready to return
					//�������������� �������� ������ �� �������� � ���������.

					//�� ������� ���� �������� � ������� ���������.
					if (GAME_DIFF == 0)
					{
						if (!weapon->GetWeapon()->IsReadyShot(RandomRange(0.2f, 0.6f)))
							return;
					}
					else
					{
						if (weapon->GetAutoShot())
						{
							if (!weapon->GetWeapon()->IsReadyShot(0))
								return;
						}
						else
						{
							if (!weapon->GetWeapon()->IsReadyShot(
								std::max(weapon->GetWeapon()->GetDesc().shotDelay, 0.25f)))
								return;
						}
					}

					float dist = weapon->GetDesc().Front().maxDist;
					if (weapon->GetCurCharge() > 0)
						++wpnCount;

					bool tShoot = dist <= 0 || distToTarget < std::min(dist, cMaxVisibleDistShot);
					tShoot = tShoot && (weapon->GetCntCharge() == 0 || weapon->GetCurCharge() > 0);
					tShoot = tShoot && (weapon->GetDesc().Front().type == Proj::ptTorpeda || Line2DistToPoint(
						car.normLine, enemy->GetCar().pos) > car.radius / 2.0f);

					if (tShoot)
						weaponList.push_back(Weapon(static_cast<Player::SlotType>(i), weapon));
				}

				struct DistSort : std::binary_function<Weapon, Weapon, bool>
				{
					bool operator()(Weapon weapon1, Weapon weapon2) const
					{
						return weapon1.wpn->GetDesc().Front().maxDist < weapon2.wpn->GetDesc().Front().maxDist;
					}
				};

				if (!weaponList.empty())
				{
					//��������� �� �������� dist
					std::stable_sort(weaponList.begin(), weaponList.end(), DistSort());

					Weapon weapon = weaponList.front();
					if (Random() < 0.25f)
						weapon = weaponList[RandomRange(0, weaponList.size() - 1)];

					const unsigned cntCharge = weapon.wpn->GetCntCharge();
					const unsigned curCharge = weapon.wpn->GetCurCharge();

					constexpr float lowP = 0.0f;
					constexpr float highP = 0.7f;

					const float roadDist = car.GetDist(true) / car.GetPathLength(true);
					//�������� �� [lowP...highP], ����������� � [0...1]
					const float summPart = ClampValue((roadDist - lowP) / (highP - lowP), 0.0f, 1.0f);

					const float wpPart = wpnCount > 0 ? 1.0f / wpnCount : 1.0f;
					const float part = std::min(summPart / wpPart, 1.0f);
					const float ammo = std::max(curCharge - (1.0f - part) * cntCharge, 0.0f);

					if ((cntCharge == 0 || ammo > 0.0f) && owner->_enbAI)
						owner->GetLogic()->Shot(owner->_player, enemy->GetCar().mapObj, weapon.slot);
				}
			}
		}

		void AICar::AttackState::RunHyper(const AICar* owner, const CarState& car, const PathState& path) const
		{
			Slot* hyperSlot = owner->_player->GetSlotInst(Player::stHyper);
			WeaponItem* hyperDrive = hyperSlot ? hyperSlot->GetItem().IsWeaponItem() : nullptr;

			const float distTile = car.curTile->GetTile().ComputeLength(
				1.0f - car.curTile->GetTile().ComputeCoordX(car.pos));
			const float maxDistHyper = hyperDrive ? (hyperDrive->GetDesc().Front().speed + car.speed) : 0.0f;
			const bool hyperDist = (path.nextTile == nullptr || path.nextTile->GetTile().GetTurnAngle() < D3DX_PI / 6 ||
				distTile > maxDistHyper);

			//autoShot ��������� �� ������.
			if (hyperDrive && hyperDrive->GetAutoShot() && owner->GetCar().gameObj->GoAiJump())
			{
				//����� ��� �� ������ ���� ��������� ������� ������ ������, � �� ������ ������ � ������ �����.
				if (backTarget && (D3DXVec2Length(&(backTarget->GetCar().pos - car.pos)) < 7.0f || backTarget->
					GetRaceTime() < 8.0f))
				{
					owner->GetCar().gameObj->GoAiJump(false);
					return;
				}

				if (hyperDrive->GetCurCharge() > 0)
				{
					owner->GetLogic()->Shot(owner->_player, nullptr, Player::stHyper);
					owner->GetCar().gameObj->GoAiJump(false);
				}
			}

			if (hyperDrive && !hyperDrive->GetAutoShot() && car.speed > 1.0f && hyperDist && !path._break)
			{
				constexpr float lowP = 0.0f;
				constexpr float highP = 0.7f;

				const float roadDist = car.GetDist(true) / car.GetPathLength(true);
				//�������� �� [lowP...highP], ����������� � [0...1]
				const float summPart = ClampValue((roadDist - lowP) / (highP - lowP), 0.0f, 1.0f);

				const unsigned cntCharge = hyperDrive->GetCntCharge();
				const unsigned curCharge = hyperDrive->GetCurCharge();

				const float ammo = std::max(curCharge - (1.0f - summPart) * cntCharge, 0.0f);
				if ((cntCharge == 0 || ammo > 0.0f) && owner->_enbAI)
					owner->GetLogic()->Shot(owner->_player, nullptr, Player::stHyper);
			}
		}

		void AICar::AttackState::PlaceMine(const AICar* owner, const CarState& car, const PathState&)
		{
			GameCar* iicar = owner->GetCar().gameObj;
			const Player* aiplayer = iicar->GetMapObj()->GetPlayer();
			Slot* minesSlot = owner->_player->GetSlotInst(Player::stMine);
			WeaponItem* mines = minesSlot ? minesSlot->GetItem().IsWeaponItem() : nullptr;

			if (mines && mines->GetWeapon() && car.speed > 5.0f && aiplayer->GetMineFreeze() == false)
			{
				constexpr float lowP = 0.05f;
				constexpr float highP = 0.95f;

				if (placeMineRandom == -1.0f)
					placeMineRandom = RandomRange(-0.5f, 0.0f);

				const float roadDist = car.GetDist(true) / car.GetPathLength(true);
				//�������� �� [lowP...highP], ����������� � [0...1]
				float summPart = ClampValue((roadDist - lowP) / (highP - lowP), 0.0f, 1.0f);
				if (summPart > 0.0f && summPart < 1.0f)
				{
					//���� �����, +30% ���
					if (backTarget && D3DXVec2Length(&(backTarget->GetCar().pos - car.pos)) < 30.0f)
						summPart += 0.3f;
					summPart = ClampValue(summPart + placeMineRandom, 0.0f, 1.0f);
				}
				//���� ��������� � ������ ������:
				if (mines->GetId() == 52 || mines->GetId() == 206)
				{
					if (backTarget && !backTarget->IsComputer() && backTarget->GetCar().gameObj)
					{
						//������������� ��� ���� ���������� ������ ������ ��������, ���� �� ������ ������ ���, � ����� 7 ������ �� ������ �����.
						if (backTarget->GetCar().gameObj->GetSpeed() > 5 && backTarget->GetRaceTime() > 7.0f &&
							D3DXVec2Length(&(backTarget->GetCar().pos - car.pos)) < 8.0f)
						{
							//�� ������ ����, ���� ������� �� �����, ���� �� � �������.
							if (!backTarget->GetCar().gameObj->IsImmortal() && backTarget->GetCar().gameObj->
								IsWheelsContact())
								owner->GetLogic()->Shot(owner->_player, nullptr, Player::stMine);
						}
					}
				}
				else
				{
					//��������� ����:
					unsigned maxCharge = 3;
					if (mines->GetWeapon()->IsAutoMine())
						maxCharge = 2;

					const unsigned cntCharge = std::min(mines->GetCntCharge(), maxCharge);
					const unsigned curCharge = cntCharge - std::min(mines->GetCntCharge() - mines->GetCurCharge(),
					                                                cntCharge);
					const float ammo = std::max(curCharge - (1.0f - summPart) * cntCharge, 0.0f);

					if ((cntCharge == 0 || ammo > 0.0f) && owner->_enbAI)
					{
						owner->GetLogic()->Shot(owner->_player, nullptr, Player::stMine);
						placeMineRandom = RandomRange(-0.5f, 0.0f);
					}
				}
			}
		}

		void AICar::AttackState::Update(AICar* owner, float, const Player::CarState& car, const PathState& path)
		{
			if (car.curTile == nullptr)
				return;

			//���� ���� �������
			SetTarget(FindEnemy(owner, car, 1, target));
			//���� ���� �����
			SetBackTarget(FindEnemy(owner, car, -1, backTarget));

			if (target && target->GetCar().mapObj != nullptr)
			{
				if (owner->GetCar().mapObj != nullptr)
				{
					//��������� �������.
					if (GAME_DIFF < 1)
					{
						if (!owner->GetCar().mapObj->GetPlayer()->GetShotFreeze())
						{
							//4 � 5 ���� ���������� �������������.
							if (owner->GetCar().mapObj->GetPlayer() == car.mapObj->GetPlayer()->GetRace()->
							                                               GetPlayerById(Race::cComputer4) || owner->
								GetCar().mapObj->GetPlayer() == car.mapObj->GetPlayer()->GetRace()->GetPlayerById(
									Race::cComputer5))
							{
								//������������� ��� �������� ������������� �� �����.
								if (target && target->IsComputer())
									ShotByEnemy(owner, car, target);
							}
							else
							{
								if (target)
									ShotByEnemy(owner, car, target);
							}
						}
					}
					else if (GAME_DIFF > 2)
					{
						if (!owner->GetCar().mapObj->GetPlayer()->GetShotFreeze())
						{
							//��������� ������. ���� �������� �� ������ �����. ���� �� ��� � ���� ������ 30��.
							if (!target->GetCar().gameObj->IsShell() && !target->GetCar().gameObj->IsImmortal())
							{
								//���� �������� �� ������ �� ������� ��.
								if (owner->GetCar().mapObj->GetPlayer() == car.mapObj->GetPlayer()->GetRace()->
								                                               GetPlayerById(Race::cComputer1))
								{
									if (target)
										ShotByEnemy(owner, car, target);
								}
								else
								{
									if (target)
									{
										const float LifeStatus = target->GetCar().gameObj->GetLife() * 100 / target->
											GetCar().gameObj->GetMaxLife();

										if (target->GetPlace() == 1 || LifeStatus < 30)
											ShotByEnemy(owner, car, target);
									}
								}
							}
						}
					}
					else
					{
						if (!owner->GetCar().mapObj->GetPlayer()->GetShotFreeze())
						{
							//��������� ������� � ����. ���� ��� �� �������� �� �����.
							if (!target->GetCar().gameObj->IsShell() && !target->GetCar().gameObj->IsImmortal())
							{
								if (target)
									ShotByEnemy(owner, car, target);
							}
						}
					}
				}
			}

			if (backTarget && backTarget->GetCar().mapObj != nullptr)
			{
				if (owner->GetCar().mapObj != nullptr)
				{
					//��������� �������.
					if (GAME_DIFF < 1)
					{
						if (!owner->GetCar().mapObj->GetPlayer()->GetShotFreeze())
						{
							//4 � 5 ���� ���������� �������������.
							if (owner->GetCar().mapObj->GetPlayer() == car.mapObj->GetPlayer()->GetRace()->
							                                               GetPlayerById(Race::cComputer4) || owner->
								GetCar().mapObj->GetPlayer() == car.mapObj->GetPlayer()->GetRace()->GetPlayerById(
									Race::cComputer5))
							{
								//������������� ��� �������� ������������� �� �����.
								if (backTarget && backTarget->IsComputer())
									ShotByEnemy(owner, car, backTarget);
							}
							else
							{
								if (backTarget)
									ShotByEnemy(owner, car, backTarget);
							}
						}
					}
					else if (GAME_DIFF > 2)
					{
						if (!owner->GetCar().mapObj->GetPlayer()->GetShotFreeze())
						{
							//��������� ������. ���� �������� �� ������ �����. ���� �� ��� � ���� ������ 30��.
							if (!backTarget->GetCar().gameObj->IsShell() && !backTarget->GetCar().gameObj->IsImmortal())
							{
								//���� �������� �� ������ �� ������� ��.
								if (owner->GetCar().mapObj->GetPlayer() == car.mapObj->GetPlayer()->GetRace()->
								                                               GetPlayerById(Race::cComputer1))
								{
									if (backTarget)
										ShotByEnemy(owner, car, backTarget);
								}
								else
								{
									if (backTarget)
									{
										const float LifeStatus = backTarget->GetCar().gameObj->GetLife() * 100 /
											backTarget->GetCar().gameObj->GetMaxLife();

										if (backTarget->GetPlace() == 1 || LifeStatus < 40)
											ShotByEnemy(owner, car, backTarget);
									}
								}
							}
						}
					}
					else
					{
						if (!backTarget->GetCar().mapObj->GetPlayer()->GetShotFreeze())
						{
							//��������� ������� � ����. ���� ��� �� �������� �� �����.
							if (!backTarget->GetCar().gameObj->IsShell() && !backTarget->GetCar().gameObj->IsImmortal())
							{
								if (backTarget)
									ShotByEnemy(owner, car, backTarget);
							}
						}
					}
				}
			}

			RunHyper(owner, car, path);
			PlaceMine(owner, car, path);
		}

		void AICar::AttackState::SetTarget(Player* value)
		{
			if (ReplaceRef(target, value))
				target = value;
		}

		void AICar::AttackState::SetBackTarget(Player* value)
		{
			if (ReplaceRef(backTarget, value))
				backTarget = value;
		}

		AICar::ControlState::ControlState(): timeBlocking(0.0f), blocking(false), backMovingMode(false),
		                                     backMoving(false), timeBackMoving(0.0f), timeResetBlockCar(0.0f)
		{
		}

		void AICar::ControlState::UpdateResetCar(AICar* owner, float deltaTime, const Player::CarState& car)
		{
			//Reset ��������������� ������
			if (car.mapObj && (blocking || car.curTile == nullptr || abs(car.speed) < cMaxSpeedBlocking))
			{
				timeResetBlockCar += deltaTime;
				if (timeResetBlockCar > 3.0f * cMaxTimeBlocking)
				{
					owner->_player->ResetCar();
					timeResetBlockCar = 0.0f;
				}
			}
			else
				timeResetBlockCar = 0.0f;
		}

		void AICar::ControlState::Update(AICar* owner, float deltaTime, const Player::CarState& car,
		                                 const PathState& path)
		{
			//��������� ���� ����� ������������ ������ � ������������ ��������
			steerAngle = abs(acos(D3DXVec2Dot(&car.dir, &path.moveDir)));
			//���� ������������� �������� ����������
			//steerAngle = std::max(0.0f, steerAngle - D3DX_PI * deltaTime * 2.0f);
			//float errorSteer = 

			//���� �������� �����
			if (steerAngle > cSteerAngleBias)
				steerAngle = D3DXVec2CCW(&car.dir, &path.moveDir) > 0 ? steerAngle : -steerAngle;
			else
				steerAngle = 0;

			//��������� �����������������
			if (abs(car.speed) < cMaxSpeedBlocking)
			{
				timeBlocking += deltaTime;
				if (timeBlocking > cMaxTimeBlocking)
				{
					timeBlocking = 0.0f;
					blocking = true;
				}
			}
			else
			{
				timeBlocking = 0.0f;
				blocking = false;
			}

			//�������� �����
			if (!backMovingMode)
			{
				backMovingMode = blocking;
				backMoving = blocking;
			}
			if (backMovingMode)
			{
				timeBackMoving += deltaTime;
				if (timeBackMoving > cMaxTimeBlocking || (backMoving && abs(steerAngle) < cSteerAngleBias &&
					timeBackMoving > 0.5f * cMaxTimeBlocking))
				{
					backMoving = !backMoving;
					timeBackMoving = 0.0f;
					backMovingMode = blocking;
				}

				if (backMoving)
					steerAngle = -steerAngle;
			}

			if (owner->_enbAI)
			{
				GameCar::MoveCarState moveState = backMoving ? GameCar::mcBack : GameCar::mcAccel;
				if (car.cheatSlower)
					moveState = GameCar::mcNone;

				car.gameObj->SetMoveCar(path._break ? GameCar::mcBrake : moveState);

				//�������� �� ���������
				if (car.gameObj->GetWastedControl() || car.gameObj->GetTurnFreeze())
					car.gameObj->SetSteerWheel(GameCar::swNone);
				else
					car.gameObj->SetSteerWheel(GameCar::smManual);

				//������� �����
				car.gameObj->SetSteerWheelAngle(steerAngle);
			}
		}

		void AICar::UpdateAI(float deltaTime, const Player::CarState& car)
		{
			_path.Update(this, deltaTime, car);
			_attack.Update(this, deltaTime, car, _path);
			_control.Update(this, deltaTime, car, _path);
		}

		void AICar::OnProcessEvent(unsigned id, EventData* data)
		{
			if (id == cPlayerDispose)
			{
				if (_attack.target && _attack.target->GetId() == data->playerId)
				{
					_attack.SetTarget(nullptr);
				}

				if (_attack.backTarget && _attack.backTarget->GetId() == data->playerId)
				{
					_attack.SetBackTarget(nullptr);
				}
			}
		}

		void AICar::OnProgress(float deltaTime)
		{
			if (GetCar().mapObj)
			{
				UpdateAI(deltaTime, GetCar());
			}

			//� ����� GetCar
			if (_enbAI)
				_control.UpdateResetCar(this, deltaTime, GetCar());
		}

		const AICar::CarState& AICar::GetCar() const
		{
			return _player->GetCar();
		}

		Logic* AICar::GetLogic() const
		{
			return _player->GetRace()->GetWorld()->GetLogic();
		}
	}
}
