#include "stdafx.h"

#include "graph\\FxManager.h"
#include "graph\\SceneNode.h"
#include "r3dMath.h"

namespace r3d
{

namespace graph
{

const char* FxEmitter::cMaxNumActionStr[FxEmitter::cMaxNumActionEnd] = {"mnaReplaceLatest", "mnaWaitingFree"};

const char* FxEmitter::cStartTypeStr[FxEmitter::cStartTypeEnd] = {"sotTime", "sotDist", "sotCombine"};

FxParticleSystem::ClassList FxParticleSystem::classList;
const char* FxParticleSystem::cChildStyleStr[cChildStyleEnd] = {"csProxy", "csUnique"};




FxParticle::FxParticle(): _worldMatChanged(true), _aabbChanged(true), _child(0)
{
}

FxParticle::~FxParticle()
{
	SetChild(0);
}
	
void FxParticle::BuildWorldMat() const
{
	if (_worldMatChanged)
	{
		_worldMatChanged = false;

		D3DXMATRIX scaleMat, rotMat, transMat;

		D3DXMatrixScaling(&scaleMat, _scale.x, _scale.y, _scale.z);
		D3DXMatrixRotationQuaternion(&rotMat, &_rot);
		D3DXMatrixTranslation(&transMat, _pos.x, _pos.y, _pos.z);

		_worldMat = scaleMat * rotMat * transMat;
	}
}

void FxParticle::BuildAABB() const
{
	if (_aabbChanged)
	{
		_aabbChanged = false;

		_aabb = AABB(_scale);
		_aabb.Offset(_pos);
	}
}

void FxParticle::TransformChanged()
{
	_worldMatChanged = true;
	_aabbChanged = true;
}

const D3DXVECTOR3& FxParticle::GetPos()
{
	return _pos;
}

void FxParticle::SetPos(const D3DXVECTOR3& value)
{
	_pos = value;
	TransformChanged();	
}

const D3DXQUATERNION& FxParticle::GetRot()
{
	return _rot;
}

void FxParticle::SetRot(const D3DXQUATERNION& value)
{
	_rot = value;
	TransformChanged();
}

const D3DXVECTOR3& FxParticle::GetScale()
{
	return _scale;
}

void FxParticle::SetScale(const D3DXVECTOR3& value)
{
	_scale = value;
	TransformChanged();
}

const D3DXMATRIX& FxParticle::GetMatrix() const
{
	BuildWorldMat();

	return _worldMat;
}

AABB FxParticle::GetAABB() const
{
	BuildAABB();
	
	return _aabb;
}

BaseSceneNode* FxParticle::GetChild()
{
	return _child;
}

void FxParticle::SetChild(BaseSceneNode* value)
{
	if (ReplaceRef(_child, value))
		_child = value;
}




FxEmitter::FxEmitter(FxParticleSystem* owner): _owner(owner), _curTime(0), _lastTimeQGroup(0), _nextTimeCreate(0), _curDensParticle(0), _cntParticles(0), _numParticle(0), _worldCoordSys(true), _modeFading(false)
{
}

FxEmitter::~FxEmitter()
{
	Reset();
}

FxEmitter::ParticleDesc::ParticleDesc()
{
	maxNum = 100;
	maxNumAction = mnaWaitingFree;
	
	life = 1;
	startTime = 1;
	startDuration = 0;
	startType = sotTime;	
	density = 1;
	
	startPos = NullVector;
	startRot = NullQuaternion;
	startScale = IdentityVector;	

	rangeLife = 0.0f;
	rangePos = NullVector;
	rangeScale = NullVector;
	rangeRot = NullQuaternion;
}

FxParticleGroup* FxEmitter::AddGroup()
{
	_groupList.Insert(_owner->AddGroup());
	return _groupList.back();
}

void FxEmitter::DelGroup(_GroupList::iterator iter)
{
	FxParticleGroup* group = *iter;

	ClearParticles(group);
	_groupList.Remove(iter);
	_owner->DelGroup(group);
}

void FxEmitter::DelGroup(const _GroupList::Position& pos)
{
	FxParticleGroup* group = pos->GetItem();

	ClearParticles(group);
	_groupList.Remove(pos);
	_owner->DelGroup(group);
}

void FxEmitter::DelGroup(_GroupList::iterator stIter, _GroupList::iterator endIter)
{
	for (_GroupList::iterator iter = stIter; iter != endIter; ++iter)
	{
		ClearParticles(*iter);
		_owner->DelGroup(*iter);
	}
	_groupList.Remove(stIter, endIter);
}
void FxEmitter::DelGroup(FxParticleGroup* value)
{
	DelGroup(std::find(_groupList.begin(), _groupList.end(), value));
}

void FxEmitter::ClearGroupList()
{
	DelGroup(_groupList.begin(), _groupList.end());
}

FxParticle* FxEmitter::AddParticle(FxParticleGroup* group)
{
	FxParticle* res = CreateParticle();
	group->Insert(res);
	++_cntParticles;
	_owner->OnCreateParticle(res);
	return res;
}

void FxEmitter::DelParticle(FxParticleGroup* group, FxParticleGroup::iterator iter)
{
	FxParticle* item = *iter;
	--_cntParticles;
	group->Remove(iter);
	_owner->OnDestroyParticle(item);
	DestroyParticle(item);
}

void FxEmitter::DelParticle(FxParticleGroup* group, FxParticleGroup::iterator sIter, FxParticleGroup::iterator eIter)
{
	for (FxParticleGroup::iterator iter = sIter; iter != eIter; ++iter)
	{
		FxParticle* item = *iter;
		--_cntParticles;
		group->RemoveItem(item);
		_owner->OnDestroyParticle(item);
		DestroyParticle(item);
	}
	group->DeleteItem(sIter, eIter);
}

void FxEmitter::DelParticle(FxParticleGroup* group, FxParticle* value)
{
	DelParticle(group, std::find(group->begin(), group->end(), value));
}

void FxEmitter::ClearParticles(FxParticleGroup* group)
{
	DelParticle(group, group->begin(), group->end());
}

float FxEmitter::CheckTimeCreateQuery(D3DXVECTOR3& offPos)
{
	return 0.0f;
}

float FxEmitter::CheckDistCreateQuery(D3DXVECTOR3& offPos)
{
	return 0.0f;
}

unsigned FxEmitter::GetNextPartIndex() const
{
	return _particleDesc.maxNum > 0 ? _numParticle % _particleDesc.maxNum : _numParticle;
}

float FxEmitter::CompRangeFrame(unsigned index) const
{
	return _particleDesc.maxNum > 1 ? index / static_cast<float>(_particleDesc.maxNum - 1) : Random();
}

FxParticle* FxEmitter::CreateParticle()
{
	return new FxParticle();
}

void FxEmitter::DestroyParticle(FxParticle* value)
{
	delete value;
}

void FxEmitter::UpdateParticle(FxParticle* value, float dTime, bool init)
{
	if (init)
	{
		D3DXVECTOR3 pos = _particleDesc.startPos.GetValue();
		D3DXVECTOR3 scale = _particleDesc.startScale.GetValue();
		D3DXQUATERNION rot = _particleDesc.startRot.GetValue();

		pos += _particleDesc.rangePos.GetValue(CompRangeFrame(value->index));
		scale += _particleDesc.rangeScale.GetValue(CompRangeFrame(value->index));
		rot *= _particleDesc.rangeRot.GetValue(CompRangeFrame(value->index));
		
		if (_worldCoordSys)		
			_owner->LocalToWorldCoord(pos, pos);

		value->SetPos(pos);
		value->SetScale(scale);
		value->SetRot(rot);

		value->time = 0;		
	}

	value->time += dTime;
}

void FxEmitter::UpdateGroup(FxParticleGroup* group, float dTime, bool init)
{	
}

void FxEmitter::QueryCreateParticles(unsigned num, float deltaTime, const D3DXVECTOR3& offPos)
{
	LSL_ASSERT(_particleDesc.maxNum == 0 || _particleDesc.maxNum >= _cntParticles);

	if (num == 0)
		return;

	//Число частиц которые вмещаются в эмиттер
	unsigned crtNum = _particleDesc.maxNum == 0 ? num : std::min(_particleDesc.maxNum - _cntParticles, num);

	//Проверяем, если число создаваемых частиц crtNum меньше заданного num, то пытаемся освободить для них места
	if (crtNum < num)		
		switch (_particleDesc.maxNumAction)
		{
		case mnaReplaceLatest:
		{
			//Пока не работает
			LSL_ASSERT(false);
			/*_GroupList::iterator group = --_groupList.begin();
			FxParticleGroup::iterator first = (*group)->begin();

			DelParticle(*first, last);
			if ((*group)->Empty())				
				DelGroup(group);*/

			break;
		}

		case mnaWaitingFree:
			break;

		default:
			LSL_ASSERT(false);
		}

	bool checkDur = _particleDesc.startDuration <= 0 || _particleDesc.startDuration > _curTime;

	//Если имеются частицы создаем их
	if (crtNum > 0 && checkDur)
	{
		FxParticleGroup* group = AddGroup();
		group->life = group->maxLife = _particleDesc.life.GetValue() + _particleDesc.rangeLife.GetValue(CompRangeFrame(GetNextPartIndex()));

		for (unsigned i = 0; i < crtNum; ++i)
		{
			FxParticle* particle = AddParticle(group);
			particle->index = GetNextPartIndex();

			UpdateParticle(particle, deltaTime, true);
			particle->SetPos(particle->GetPos() + offPos);
			_owner->OnUpdateParticle(this, particle, 0, true);

			++_numParticle;
		}
		//
		UpdateGroup(group, deltaTime, true);
	}
}

void FxEmitter::QueryCreateGroup(float deltaTime, const D3DXVECTOR3& offPos)
{
	//Текущая плотность частиц
	_curDensParticle = _curDensParticle + _particleDesc.density.GetValue();
	//Число частиц которое нужно создать
	int newParticles = static_cast<int>(_curDensParticle);
	//Остаток от создания частиц
	_curDensParticle -= newParticles;
	//
	_lastTimeQGroup = _curTime;
	_lastPosQGroup = _worldCoordSys ? _owner->GetWorldPos() : _owner->GetPos();

	QueryCreateParticles(newParticles, deltaTime, offPos);
}

AABB FxEmitter::LocalDimensions() const
{
	D3DXVECTOR3 dimens = IdentityVector / 2 * _particleDesc.startScale.GetMax();
	AABB res(_particleDesc.startPos.GetMin() - dimens, _particleDesc.startPos.GetMax() + dimens);

	return res;	
}

void FxEmitter::OnProgress(float deltaTime)
{
	_curTime += deltaTime;
	//
	if (_groupList.Empty())
	{
		_lastPosQGroup = _worldCoordSys ? _owner->GetWorldPos() : _owner->GetPos();
		_lastTimeQGroup = _curTime;
	}
	
	//Удаление и обновление оставшихся групп и частиц
	for (_GroupList::Position pos = _groupList.First(); FxParticleGroup** iter = _groupList.Current(pos); _groupList.Next(pos))
	{
		FxParticleGroup* group = (*iter);

		LSL_ASSERT(!group->Empty());

		bool death = group->maxLife > 0 && (group->life = std::max(group->life - deltaTime, 0.0f)) <= 0;
		//Если группа мертва то её можно удалить
		if (death)
			DelGroup(pos);
		//Иначе обновляем её
		else
		{
			//Сначала обновление частиц
			for (FxParticleGroup::iterator iterPart = group->begin(); iterPart != group->end(); ++iterPart)
			{
				UpdateParticle(*iterPart, deltaTime, false);
				_owner->OnUpdateParticle(this, *iterPart, deltaTime, false);
			}

			//Затем обновление группы (так удобней с точки зрения реализации)
			UpdateGroup(group, deltaTime, false);
		}
	}

	//Создание частиц
if (!_modeFading)
	switch (_particleDesc.startType)
	{
	case sotTime:
	{
		//минимальное допустимое стартовое время 1 мс
		FloatRange startTime(std::max(_particleDesc.startTime.GetMin(), 0.001f), std::max(_particleDesc.startTime.GetMax(), 0.001f));
		//оставание более чем в 20 раз недопустимо
		float dTime = std::min(_curTime - _nextTimeCreate, startTime.GetMax() * 20.0f);

		D3DXVECTOR3 dPos = _lastPosQGroup - (_worldCoordSys ? _owner->GetWorldPos() : _owner->GetPos());
		float dPosTime = std::max(_curTime - _lastTimeQGroup, dTime);

		unsigned numPart = 0;		
		while (dTime > 0.0f)
		{
			QueryCreateGroup(dTime, dPos * dTime / dPosTime);
			dTime -= startTime.GetValue();
		}
		_nextTimeCreate = _curTime - dTime;

		break;
	}

	case sotDist:
	{
		if (!_groupList.Empty())
		{
			FxParticle* particle = _groupList.back()->back();
			D3DXVECTOR3 pos = GetLocalPos(particle);
			D3DXVECTOR3 dist = pos;
			float distLen = D3DXVec3Length(&dist);
			//оставание более чем в 20 раз недопустимо
			float dDist = std::min(distLen - _nextDistCreate, _particleDesc.startTime.GetMax() * 20.0f);

			D3DXVECTOR3 dPos = _lastPosQGroup - (_worldCoordSys ? _owner->GetWorldPos() : _owner->GetPos());
			float dTime = _curTime - _lastTimeQGroup;

			while (dDist > 0.0f)
			{
				float t = dDist / distLen;
				QueryCreateGroup(dTime * t, dPos * t);

				_nextDistCreate = _particleDesc.startTime.GetValue();
				dDist -= _nextDistCreate;
			}
		}
		else
		{
			//Начальная инициализация переменной
			_nextDistCreate = _particleDesc.startTime.GetValue();
			QueryCreateGroup(0, NullVector);
		}

		break;
	}

	case sotCombine:
	{
		//bool createGroup = CheckTimeCreateQuery() > 0.0f;
		//createGroup |=  CheckDistCreateQuery() > 0.0f;
		//if (createGroup)
		//	QueryCreateGroup(0.0f, NullVector);
	}

	}
}

void FxEmitter::Save(lsl::SWriter* writer)
{
	{
		lsl::SWriter* child = writer->NewDummyNode("partDesc");
		child->WriteValue("maxNum", _particleDesc.maxNum);
		lsl::SWriteEnum(child, "maxNumAction", _particleDesc.maxNumAction, cMaxNumActionStr, cMaxNumActionEnd);
		lsl::SWriteValue(child, "life", _particleDesc.life);
		lsl::SWriteValue(child, "startTime", _particleDesc.startTime);
		child->WriteValue("startDuration", _particleDesc.startDuration);
		lsl::SWriteEnum(child, "startType", _particleDesc.startType, cStartTypeStr, cStartTypeEnd);
		lsl::SWriteValue(child, "density", _particleDesc.density);
		lsl::SWriteValue(child, "startPos", _particleDesc.startPos);
		lsl::SWriteValue(child, "startScale", _particleDesc.startScale);
		lsl::SWriteValue(child, "startRot", _particleDesc.startRot);

		lsl::SWriteValue(child, "rangeLife", _particleDesc.rangeLife);
		lsl::SWriteValue(child, "rangePos", _particleDesc.rangePos);
		lsl::SWriteValue(child, "rangeScale", _particleDesc.rangeScale);
		lsl::SWriteValue(child, "rangeRot", _particleDesc.rangeRot);
	}

	writer->WriteValue("worldCoordSys", _worldCoordSys);
}

void FxEmitter::Load(lsl::SReader* reader)
{
	Reset();

	if (lsl::SReader* child = reader->ReadValue("partDesc"))
	{
		child->ReadValue("maxNum", _particleDesc.maxNum);
		lsl::SReadEnum(child, "maxNumAction", _particleDesc.maxNumAction, cMaxNumActionStr, cMaxNumActionEnd);
		lsl::SReadValue(child, "life", _particleDesc.life);
		lsl::SReadValue(child, "startTime", _particleDesc.startTime);
		child->ReadValue("startDuration", _particleDesc.startDuration);
		lsl::SReadEnum(child, "startType", _particleDesc.startType, cStartTypeStr, cStartTypeEnd);
		lsl::SReadValue(child, "density", _particleDesc.density);
		lsl::SReadValue(child, "startPos", _particleDesc.startPos);
		lsl::SReadValue(child, "startScale", _particleDesc.startScale);
		lsl::SReadValue(child, "startRot", _particleDesc.startRot);	
		
		lsl::SReadValue(child, "rangeLife", _particleDesc.rangeLife);
		lsl::SReadValue(child, "rangePos", _particleDesc.rangePos);
		lsl::SReadValue(child, "rangeScale", _particleDesc.rangeScale);
		lsl::SReadValue(child, "rangeRot", _particleDesc.rangeRot);
	}

	reader->ReadValue("worldCoordSys", _worldCoordSys);	
}

void FxEmitter::Reset()
{
	ClearGroupList();
}

FxParticleSystem* FxEmitter::GetSystem()
{
	return _owner;
}

const FxEmitter::ParticleDesc& FxEmitter::GetParticleDesc() const
{
	return _particleDesc;
}

void FxEmitter::SetParticleDesc(const ParticleDesc& value)
{
	_particleDesc = value;
}

unsigned FxEmitter::GetCntParticle() const
{
	return _cntParticles;
}

bool FxEmitter::GetWorldCoordSys() const
{
	return _worldCoordSys;
}

void FxEmitter::SetWorldCoordSys(bool value)
{
	if (_worldCoordSys != value)
		_worldCoordSys = value;
}

bool FxEmitter::GetModeFading() const
{
	return _modeFading;
}

void FxEmitter::SetModeFading(bool value)
{
	_modeFading = value;
}

D3DXVECTOR3 FxEmitter::GetLocalPos(FxParticle* particle) const
{
	D3DXVECTOR3 res = particle->GetPos();
	if (_worldCoordSys)
		_owner->WorldToLocalCoord(res, res);

	return res;
}

D3DXVECTOR3 FxEmitter::GetWorldPos(FxParticle* particle) const
{
	D3DXVECTOR3 res = particle->GetPos();
	if (!_worldCoordSys)
		_owner->LocalToWorldCoord(res, res);

	return res;
}

const D3DXMATRIX& FxEmitter::GetMatrix() const
{
	return _worldCoordSys ? IdentityMatrix : _owner->GetWorldMat();
}




FxParticleSystem::FxParticleSystem(): _fxManager(0), _childStyle(csProxy), _aabb(1.0f), _srcSpeed(NullVector)
{
	InitClassList();

	_emitters = new Emitters(this);
	_emitters->SetClassList(&classList);

	_child = new SceneNode();	
	_child->SetName("child");
	_child->SetOwner(this);
	_child->SetParent(this);

	SetOpt(noDynStructure, true);

	RegProgress();
}

FxParticleSystem::~FxParticleSystem()
{
	UnregProgress();

	SetFxManager(0);

	delete _child;
	delete _emitters;
}

void FxParticleSystem::InitClassList()
{
	static bool initClassList = false;

	if (!initClassList)
	{
		initClassList = true;

		classList.Add<FxEmitter>(etBase);
		classList.Add<FxFlowEmitter>(etFlow);
	}
}

void FxParticleSystem::OnCreateParticle(FxParticle* value)
{
	if (!_child->GetNodes().Empty())
	{
		switch (_childStyle)
		{
		case csProxy:
		{
			SceneNode::Proxy* child = &_child->GetProxyList().Add();
			child->SetParent(this);
			value->SetChild(child);
			break;
		}
		case csUnique:
		{
			SceneNode* child = new SceneNode();
			child->SetParent(this);
			child->AssignFromSer(_child, GetRoot());
			value->SetChild(child);
			break;					
		}

		default:
			LSL_ASSERT(false);
		}
	}
}

void FxParticleSystem::OnDestroyParticle(FxParticle* value)
{
	if (BaseSceneNode* child = value->GetChild())
	{
		value->SetChild(0);

		switch (_childStyle)
		{
		case csProxy:		
			_child->GetProxyList().Delete(lsl::StaticCast<SceneNode::Proxy*>(child));
			break;

		case csUnique:
			delete child;
			break;

		default:
			LSL_ASSERT(false);
		}
	}
}

void FxParticleSystem::OnUpdateParticle(FxEmitter* emitter, FxParticle* value, float dTime, bool init)
{
	if (BaseSceneNode* child = value->GetChild())
	{
		child->SetWorldPos(emitter->GetWorldPos(value));

		//if (_childStyle == csUnique)
		//	child->OnProgress(dTime);
	}

	AABB aabb = value->GetAABB();
	if (emitter->GetWorldCoordSys())
		aabb.Transform(GetInvWorldMat());

	_aabb.Add(aabb);
}

FxParticleGroup* FxParticleSystem::AddGroup()
{
	return &_fxManager->GetGroupList().Add();
}

void FxParticleSystem::DelGroup(FxParticleGroup* value)
{
	_fxManager->GetGroupList().Delete(value);
}

AABB FxParticleSystem::LocalDimensions() const
{
	//AABB aabb(1.0f);

	//for (Emitters::const_iterator iter = _emitters->begin(); iter != _emitters->end(); ++iter)
	//	aabb.Add((*iter)->LocalDimensions());

	return _aabb;
}

void FxParticleSystem::Save(lsl::SWriter* writer)
{
	_MyBase::Save(writer);

	writer->WriteRef("fxManager", _fxManager);
	writer->WriteValue("emitters", _emitters);
	material.Save(writer, this);
	
	writer->WriteValue("child", _child);
	lsl::SWriteEnum(writer, "childStyle", _childStyle, cChildStyleStr, cChildStyleEnd);
}

void FxParticleSystem::Load(lsl::SReader* reader)
{
	_MyBase::Load(reader);

	SetFxManager(0);

	reader->ReadRef("fxManager", false, this, 0);
	reader->ReadValue("emitters", _emitters);
	material.Load(reader, this);

	reader->ReadValue("child", _child);
	lsl::SReadEnum(reader, "childStyle", _childStyle, cChildStyleStr, cChildStyleEnd);
}

void FxParticleSystem::OnFixUp(const FixUpNames& fixUpNames)
{
	_MyBase::OnFixUp(fixUpNames);

	for (FixUpNames::const_iterator iter = fixUpNames.begin(); iter != fixUpNames.end(); ++iter)
	{
		if (iter->name == "fxManager")	
			SetFxManager(iter->GetComponent<FxManager*>());			
	}

	material.OnFixUp(fixUpNames, this);
}

void FxParticleSystem::DoRender(graph::Engine& engine)
{
	LSL_ASSERT(_fxManager);

	_fxManager->RenderSystem(engine, this);

	if (!_child->GetNodes().Empty())
		switch (_childStyle)
		{
		case csProxy:
			for (SceneNode::ProxyList::const_iterator iter = _child->GetProxyList().begin(); iter != _child->GetProxyList().end(); ++iter)
				(*iter)->Render(engine);
			break;

		case csUnique:
			for (Emitters::iterator iter = _emitters->begin(); iter != _emitters->end(); ++iter)
			{
				FxEmitter* emitter = *iter;
				for (FxEmitter::_GroupList::iterator iter = emitter->_groupList.begin(); iter != emitter->_groupList.end(); ++iter)
				{
					FxParticleGroup* group = *iter;
					for (FxParticleGroup::iterator iter = group->begin(); iter != group->end(); ++iter)
					{
						LSL_ASSERT((*iter)->GetChild());
						
						(*iter)->GetChild()->Render(engine);
					}
				}
			}
			break;
		}
}

void FxParticleSystem::OnProgress(float deltaTime)
{
	LSL_ASSERT(_fxManager);

	_aabb = AABB(1.0f);

	for (Emitters::iterator iter = _emitters->begin(); iter != _emitters->end(); ++iter)
		(*iter)->OnProgress(deltaTime);

	//if (_childStyle == csProxy)
	//	_child->OnProgress(deltaTime);
}

void FxParticleSystem::Reset()
{
	for (Emitters::iterator iter = _emitters->begin(); iter != _emitters->end(); ++iter)
		(*iter)->Reset();
}

FxManager* FxParticleSystem::GetFxManager()
{
	return _fxManager;
}

void FxParticleSystem::SetFxManager(FxManager* value)
{
	if (ReplaceRef(_fxManager, value))
	{
		Reset();
		_fxManager = value;
	}
}

FxParticleSystem::Emitters& FxParticleSystem::GetEmitters()
{
	return *_emitters;
}

SceneNode& FxParticleSystem::GetChild()
{
	return *_child;
}

FxParticleSystem::ChildStyle FxParticleSystem::GetChildStyle() const
{
	return _childStyle;
}

void FxParticleSystem::SetChildStyle(ChildStyle value)
{
	if (_childStyle != value)
	{
		Reset();
		_childStyle = value;
	}
}

unsigned FxParticleSystem::GetCntParticle() const
{
	unsigned res = 0;
	for (Emitters::const_iterator iter = _emitters->begin(); iter != _emitters->end(); ++iter)
		res += (*iter)->GetCntParticle();

	return res;
}

void FxParticleSystem::SetModeFading(bool value)
{
	for (Emitters::const_iterator iter = _emitters->begin(); iter != _emitters->end(); ++iter)
		(*iter)->SetModeFading(value);
}

const D3DXVECTOR3& FxParticleSystem::GetSrcSpeed() const
{
	return _srcSpeed;
}

void FxParticleSystem::SetSrcSpeed(const D3DXVECTOR3& value)
{
	_srcSpeed = value;
}




FxManager::FxManager()
{
	_groupList = new _GroupList(this);
}

FxManager::~FxManager()
{
	_groupList->Clear();
	delete _groupList;
}

FxParticleGroup* FxManager::CreateGroup()
{
	return new FxParticleGroup();
}

void FxManager::DestroyGroup(FxParticleGroup* value)
{
	delete value;
}

void FxManager::RenderEmitter(graph::Engine& engine, FxEmitter* emitter)
{
	FxParticleSystem* system = emitter->GetSystem();

	engine.GetContext().SetWorldMat(emitter->GetMatrix());

	for (FxEmitter::_GroupList::iterator iterGroup = emitter->_groupList.begin(); iterGroup != emitter->_groupList.end(); ++iterGroup)
	{
		FxParticleGroup* group = *iterGroup;

		float frame = group->maxLife > 0 ? 1.0f - group->life / group->maxLife : 0.0f;
		if (system->animMode() != graph::SceneNode::amNone)
			frame = system->GetFrame(frame);

		engine.GetContext().PushFrame(frame);

		emitter->GetSystem()->material.Apply(engine);
		RenderGroup(engine, emitter, group);
		emitter->GetSystem()->material.UnApply(engine);

		engine.GetContext().PopFrame();
	}	
}

void FxManager::RenderSystem(graph::Engine& engine, FxParticleSystem* system)
{
	for (FxParticleSystem::Emitters::iterator iter = system->GetEmitters().begin(); iter != system->GetEmitters().end(); ++iter)
	{
		FxEmitter* emitter = *iter;

		RenderEmitter(engine, emitter);
	}
}

FxManager::_GroupList& FxManager::GetGroupList()
{
	return *_groupList;
}

const FxManager::EmitterGroups& FxManager::GetEmitterGroups(const FxEmitter* emitter) const
{
	return emitter->_groupList;
}




inline DWORD FtoDw(float value)
{
	return *((DWORD*)&value);
}

struct VertexPSize
{
	static const DWORD fvf = D3DFVF_XYZ | D3DFVF_PSIZE;

	VertexPSize() {};
	VertexPSize(const D3DXVECTOR3& mPos, float mSize): pos(mPos), size(mSize) {}

	D3DXVECTOR3 pos;
	float size;
};

void FxPointSpritesManager::RenderGroup(graph::Engine& engine, FxEmitter* emitter, FxParticleGroup* group)
{
	VertexPSize* vertexBuf = new VertexPSize[group->Size()];

	VertexPSize* vertex = vertexBuf;
	for (FxParticleGroup::iterator iter = group->begin(); iter != group->end(); ++iter, ++vertex)
	{
		FxParticle* particle = *iter;

		vertex->pos = particle->GetPos();
		//Делим на 4 для преобразования к мировым координатам
		float camScale = 1.0f;
		switch (engine.GetContext().GetCamera().GetDesc().style)
		{
		case csPerspective:
			camScale = 0.25f;
			break;

		case csOrtho:
			camScale = 0.75f;
			break;

		default:
			LSL_ASSERT(false);
		}

		vertex->size = D3DXVec3Length(&particle->GetScale()) * camScale;
	}

	engine.GetDriver().GetDevice()->DrawPrimitiveUP(D3DPT_POINTLIST, group->Size(), vertexBuf, sizeof(VertexPSize));

	delete[] vertexBuf;
}

void FxPointSpritesManager::RenderSystem(graph::Engine& engine, FxParticleSystem* system)
{
	engine.GetContext().SetRenderState(graph::rsPointScaleEnable, true);
	engine.GetContext().SetRenderState(graph::rsPointScaleA, FtoDw(0.0f));
	engine.GetContext().SetRenderState(graph::rsPointScaleB, FtoDw(0.0f));
	engine.GetContext().SetRenderState(graph::rsPointScaleC, FtoDw(1.0f));
	engine.GetContext().SetRenderState(graph::rsPointSizeMin, FtoDw(1.0f));
	engine.GetContext().SetRenderState(graph::rsPointSizeMax, FtoDw(1000.0f));
	//
	engine.GetContext().SetRenderState(graph::rsPointSpriteEnable, true);
	engine.GetContext().SetRenderState(graph::rsZWriteEnable, false);

	engine.GetDriver().GetDevice()->SetFVF(VertexPSize::fvf);
	_MyBase::RenderSystem(engine, system);

	//
	engine.GetContext().RestoreRenderState(graph::rsPointScaleEnable);
	engine.GetContext().RestoreRenderState(graph::rsPointScaleA);
	engine.GetContext().RestoreRenderState(graph::rsPointScaleB);
	engine.GetContext().RestoreRenderState(graph::rsPointScaleC);	
	engine.GetContext().RestoreRenderState(graph::rsPointSizeMin);
	engine.GetContext().RestoreRenderState(graph::rsPointSizeMax);
	//
	engine.GetContext().RestoreRenderState(graph::rsPointSpriteEnable);
	engine.GetContext().RestoreRenderState(graph::rsZWriteEnable);
}




FxSpritesManager::FxSpritesManager(): dirSprite(false)
{
}

void FxSpritesManager::RenderGroup(graph::Engine& engine, FxEmitter* emitter, FxParticleGroup* group)
{
	engine.BeginMeshPT();

	for (FxParticleGroup::iterator iter = group->begin(); iter != group->end(); ++iter)
	{
		FxParticle* particle = *iter;

		if (dirSprite)
		{
			D3DXVECTOR3 dir;
			Vec3Rotate(XVector, particle->GetRot(), dir);

			engine.RenderSpritePT(emitter->GetWorldPos(particle), particle->GetScale(), 0, &dir, IdentityMatrix);
		}
		else
		{
			D3DXVECTOR3 axe;
			float angle;
			D3DXQuaternionToAxisAngle(&particle->GetRot(), &axe, &angle);
			
			engine.RenderSpritePT(emitter->GetWorldPos(particle), particle->GetScale(), angle, 0, IdentityMatrix);
		}
	}

	engine.EndMeshPT();
}




FxPlaneManager::FxPlaneManager()
{
}

void FxPlaneManager::RenderGroup(graph::Engine& engine, FxEmitter* emitter, FxParticleGroup* group)
{
	engine.BeginMeshPT();

	for (FxParticleGroup::iterator iter = group->begin(); iter != group->end(); ++iter)
	{
		FxParticle* particle = *iter;

		D3DXVECTOR3 axe;
		float angle;
		D3DXQuaternionToAxisAngle(&particle->GetRot(), &axe, &angle);

		D3DXMATRIX worldMat = particle->GetMatrix() * emitter->GetMatrix();
		engine.GetContext().SetWorldMat(worldMat);
		
		engine.RenderPlanePT();
	}

	engine.EndMeshPT();
}




FxNodeManager::FxNodeManager()
{
	_node = new SceneNode();
	_node->SetOwner(this);	
}

FxNodeManager::~FxNodeManager()
{
	delete _node;
}

void FxNodeManager::RenderGroup(graph::Engine& engine, FxEmitter* emitter, FxParticleGroup* group)
{
	for (FxParticleGroup::iterator iter = group->begin(); iter != group->end(); ++iter)
	{
		FxParticle* particle = *iter;

		_node->SetPos(particle->GetPos());
  		_node->SetRot(particle->GetRot());
		_node->SetScale(particle->GetScale());

		_node->Render(engine);
	}
}

SceneNode& FxNodeManager::GetNode()
{
	return *_node;
}




FxTrailManager::FxTrailManager(): _trailWidth(1.0f), fixedUpVec(ZVector), fixedUp(false), typeDraw(tdPerGroup)
{
}

FxTrailManager::~FxTrailManager()
{
}

void FxTrailManager::BuildVertexLine(res::VertexPT* vertex, const D3DXVECTOR3& pos, const D3DXVECTOR3& dir, const D3DXVECTOR3& camPos, float xTex)
{
	D3DXVECTOR3 yVec;

	if (fixedUp)
	{
		D3DXVec3Cross(&yVec, &fixedUpVec, &dir);
		D3DXVec3Normalize(&yVec, &yVec);
	}
	else
	{
		D3DXVECTOR3 viewVec = pos - camPos;
		D3DXVec3Normalize(&viewVec, &viewVec);

		D3DXVec3Cross(&yVec, &dir, &viewVec);
		D3DXVec3Normalize(&yVec, &yVec);
	}

	vertex[0].pos = yVec * _trailWidth + pos;
	vertex[0].tex = D3DXVECTOR2(xTex, 0);
	vertex[1].pos = -yVec * _trailWidth + pos;
	vertex[1].tex = D3DXVECTOR2(xTex, 1.0f);
}

void FxTrailManager::DrawPath(graph::Engine& engine, FxParticleSystem* system, FxParticleGroup* group, res::VertexPT* vBuf, unsigned primCnt, unsigned sPrim)
{
	float frame = group->maxLife > 0 ? 1.0f - group->life / group->maxLife : 0.0f;
	engine.GetContext().PushFrame(frame);
	system->material.Apply(engine);

	engine.GetDriver().GetDevice()->SetFVF(res::VertexPT::fvf);
	engine.GetDriver().GetDevice()->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, primCnt * 2, &vBuf[sPrim * 2], sizeof(res::VertexPT));
			
	system->material.UnApply(engine);
	engine.GetContext().PopFrame();
}

void FxTrailManager::RenderGroup(graph::Engine& engine, FxEmitter* emitter, FxParticleGroup* group)
{
	//Nothing
}

void FxTrailManager::RenderEmitter(graph::Engine& engine, FxEmitter* emitter)
{
	using namespace res;

	FxParticleSystem* system = emitter->GetSystem();
	const EmitterGroups& groups = GetEmitterGroups(emitter);
	//Общее число частиц плюс одна частица для следа от самого эммитера
	unsigned cntParticle = emitter->GetCntParticle() + 1;

	if (cntParticle < 2)
		return;

	engine.GetContext().SetWorldMat(emitter->GetMatrix());

	D3DXVECTOR3 camPos = engine.GetContext().GetCamera().GetDesc().pos;
	float xTex = 0;

	VertexPT* vertexBuf = new VertexPT[cntParticle * 2];
	VertexPT* vertex = vertexBuf;

	//
	D3DXVECTOR3 worldPos = emitter->GetWorldCoordSys() ? system->GetWorldPos() : NullVector;

	//Вычисление начального направления
	EmitterGroups::const_iterator iter1 = groups.begin();
	FxParticleGroup::const_iterator iterPart1 = (*iter1)->begin();
	D3DXVECTOR3 pos1 = (*iterPart1)->GetPos();
	D3DXVECTOR3 pos2;
	if (++iterPart1 != (*iter1)->end())
		pos2 = (*iterPart1)->GetPos();
	else if (++iter1 != groups.end())
		pos2 = (*iter1)->front()->GetPos();
	else
		pos2 = worldPos;
	D3DXVECTOR3 dir = pos2 - pos1;
	D3DXVec3Normalize(&dir, &dir);
	//Последняя позиция для вычисления направления
	D3DXVECTOR3 lastPos = pos1 - dir;
	
	//Последняя частица которая была отрисована
	unsigned lastPartDraw = 0;
	//Текущее число частиц
	unsigned numPartDraw = 0;

	bool bd = groups.Size() > 1;
	for (EmitterGroups::const_iterator iter = groups.begin(); iter != groups.end(); ++iter)
	{
		FxParticleGroup* group = *iter;

		for (FxParticleGroup::const_iterator iterPart = group->begin(); iterPart != group->end(); ++iterPart)
		{
			FxParticle* particle = *iterPart;
			D3DXVECTOR3 pos = particle->GetPos();

			//Строим линию из двух вершин
			BuildVertexLine(vertex, pos, dir, camPos, xTex);
			vertex += 2;

			//
			dir = pos - lastPos;
			D3DXVec3Normalize(&dir, &dir);
			lastPos = pos;
			//
			if ((xTex += 1) > 1)
				xTex = 0;

			++numPartDraw;
		}

		//
		unsigned primCnt = numPartDraw - lastPartDraw - 1;
		if (typeDraw == tdPerGroup && iter != groups.begin() && primCnt > 0)
		{
			EmitterGroups::const_iterator lastGr = iter;
			--lastGr;
			group = *lastGr;

			DrawPath(engine, system, group, vertexBuf, primCnt, lastPartDraw);

			lastPartDraw = numPartDraw - 1;
		}
	}	

	//Обращаемся к последней частице в роли которой выступает сам емиттер, и также строим линию из двух вершин
	dir = worldPos - lastPos;
	D3DXVec3Normalize(&dir, &dir);
	BuildVertexLine(vertex, worldPos, dir, camPos, xTex);
	++numPartDraw;

	//Рисуем все оставшиеся частицы. Если режим typeDraw == tdLastGroup то рисуются все частицы, иначе только последняя
	DrawPath(engine, system, groups.back(), vertexBuf, numPartDraw - lastPartDraw - 1, lastPartDraw);
	
	delete[] vertexBuf;
}

void FxTrailManager::RenderSystem(graph::Engine& engine, FxParticleSystem* system)
{
	_MyBase::RenderSystem(engine, system);
}

float FxTrailManager::GetTrailWidth() const
{
	return _trailWidth;
}

void FxTrailManager::SetTrailWidth(float value)
{
	if (_trailWidth != value)
		_trailWidth = value;
}




FxFlowEmitter::FxFlowEmitter(FxParticleSystem* owner): _MyBase(owner)
{	
}

FxFlowEmitter::FlowDesc::FlowDesc()
{
	speedPos = NullVector;
	speedRot = NullQuaternion;
	speedScale = NullVector;
	gravitation = NullVector;
	acceleration = NullVector;
	autoRot = false;
}

FxParticle* FxFlowEmitter::CreateParticle()
{
	return new FxFlowParticle();
}

void FxFlowEmitter::UpdateParticle(FxParticle* value, float dTime, bool init)
{
	_MyBase::UpdateParticle(value, dTime, init);

	FxFlowParticle* particle = static_cast<FxFlowParticle*>(value);

	if (init)
	{
		particle->speedPos = _flowDesc.speedPos.GetValue();
		particle->speedRot = _flowDesc.speedRot.GetValue();
		particle->speedScale = _flowDesc.speedScale.GetValue();
		particle->acceleration = _flowDesc.acceleration.GetValue();

		if (GetWorldCoordSys())
		{
			GetSystem()->LocalToWorldNorm(particle->speedPos, particle->speedPos);
			GetSystem()->LocalToWorldNorm(particle->acceleration, particle->acceleration);
		}

		D3DXVECTOR3 srcSpeed = GetSystem()->GetSrcSpeed();
		if (GetWorldCoordSys() && GetSystem()->GetParent())
			GetSystem()->GetParent()->LocalToWorldNorm(srcSpeed, srcSpeed);

		particle->speedPos += srcSpeed;
	}

	if (dTime != 0.0f)
	{
		//dS = (V + a * t) * dt
		D3DXVECTOR3 speed = particle->speedPos + (particle->acceleration + _flowDesc.gravitation) * particle->time;
		particle->SetPos(particle->GetPos() + speed * dTime);

		if (_flowDesc.autoRot)
		{
			D3DXVECTOR3 dir;
			D3DXVec3Normalize(&dir, &speed);
			D3DXQUATERNION rot;
			QuatShortestArc(XVector, dir, rot);
			particle->SetRot(rot * particle->speedRot);
		}
		else
		{
			D3DXVECTOR3 rotAxe;
			float rotAngle;
			D3DXQUATERNION rotDt;
			D3DXQuaternionToAxisAngle(&particle->speedRot, &rotAxe, &rotAngle);
			D3DXQuaternionRotationAxis(&rotDt, &rotAxe, rotAngle * dTime);
			particle->SetRot(particle->GetRot() * rotDt);
		}

		particle->SetScale(particle->GetScale() + particle->speedScale * dTime);
	}
}

void FxFlowEmitter::Save(lsl::SWriter* writer)
{
	_MyBase::Save(writer);

	{
		lsl::SWriter* child = writer->NewDummyNode("flowDesc");

		lsl::SWriteValue(child, "speedPos", _flowDesc.speedPos);
		lsl::SWriteValue(child, "speedRot", _flowDesc.speedRot);
		lsl::SWriteValue(child, "speedScale", _flowDesc.speedScale);
		lsl::SWriteValue(child, "acceleration", _flowDesc.acceleration);	
		lsl::SWriteValue(child, "gravitation", _flowDesc.gravitation);
		
		writer->WriteValue("autoRot", _flowDesc.autoRot);
	}
}

void FxFlowEmitter::Load(lsl::SReader* reader)
{
	_MyBase::Load(reader);

	if (lsl::SReader* child = reader->ReadValue("flowDesc"))
	{
		lsl::SReadValue(child, "speedPos", _flowDesc.speedPos);
		lsl::SReadValue(child, "speedRot", _flowDesc.speedRot);
		lsl::SReadValue(child, "speedScale", _flowDesc.speedScale);
		lsl::SReadValue(child, "acceleration", _flowDesc.acceleration);		
		lsl::SReadValue(child, "gravitation", _flowDesc.gravitation);

		reader->ReadValue("autoRot", _flowDesc.autoRot);
	}
}

const FxFlowEmitter::FlowDesc& FxFlowEmitter::GetFlowDesc() const
{
	return _flowDesc;
}

void FxFlowEmitter::SetFlowDesc(const FlowDesc& value)
{
	_flowDesc = value;
}




FxPhysicsEmitter::FxPhysicsEmitter(FxParticleSystem* owner): _MyBase(owner), _pxScene(0)
{
}

FxPhysicsEmitter::~FxPhysicsEmitter()
{
	SetPxScene(0);
}

FxParticle* FxPhysicsEmitter::CreateParticle()
{
	return new FxPhysicsParticle();
}

void FxPhysicsEmitter::UpdateParticle(FxParticle* value, float dTime, bool init)
{
	_MyBase::UpdateParticle(value, dTime, init);

	FxPhysicsParticle* particle = static_cast<FxPhysicsParticle*>(value);

	if (init)
	{
		NxBoxShapeDesc boxShape;
		boxShape.dimensions.set(IdentityVector * value->GetScale());
		px::BoxShape& bbShape = particle->pxActor.GetShapes().Add<px::BoxShape>();
		bbShape.AssignFromDesc(boxShape);

		//px::TriangleMeshShape& triShape = particle->pxActor.GetShapes().Add<px::TriangleMeshShape>();
		//triShape.SetMesh(_mesh, particle->index);

		NxBodyDesc body;
		body.mass = 20.0f;
		body.sleepEnergyThreshold = 0.05f;
		particle->pxActor.SetBody(&body);

		particle->pxActor.SetScene(_pxScene);
		particle->pxActor.SetPos(particle->GetPos());

		//particle->pxActor.GetNxActor()->addForceAtPos(NxVec3(D3DXVECTOR3(1.0f, 0, 1.0f) * 4000), NxVec3(((_mesh->GetMeshData()->vb.GetMinPos() + _mesh->GetMeshData()->vb.GetMaxPos()) / 2) + GetPos()));
	}
	else
	{
		particle->SetPos(particle->pxActor.GetPos());
		particle->SetRot(particle->pxActor.GetRot());
	}
}

px::Scene* FxPhysicsEmitter::GetPxScene()
{
	return _pxScene;
}

void FxPhysicsEmitter::SetPxScene(px::Scene* value)
{
	if (ReplaceRef(_pxScene, value))
		_pxScene = value;
}

}

}