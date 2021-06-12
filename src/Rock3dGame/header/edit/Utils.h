#pragma once

#include "ISceneControl.h"

namespace r3d
{

namespace edit
{

class ScNodeCont: public IScNodeCont
{
private:
	graph::BaseSceneNode* _node;
protected:
	void Reset(graph::BaseSceneNode* node);
public:
	ScNodeCont(graph::BaseSceneNode* node);
	virtual ~ScNodeCont();

	virtual bool RayCastInters(const D3DXVECTOR3& rayPos, const D3DXVECTOR3& rayVec) const;
	virtual bool Compare(const IMapObjRef& node) const;

	virtual D3DXVECTOR3 GetPos() const;
	virtual void SetPos(const D3DXVECTOR3& value);
	//
	virtual D3DXQUATERNION GetRot() const;
	virtual void SetRot(const D3DXQUATERNION& value);
	//
	virtual D3DXVECTOR3 GetScale() const;
	virtual void SetScale(const D3DXVECTOR3& value);
	//
	virtual D3DXVECTOR3 GetDir() const;
	virtual D3DXVECTOR3 GetRight() const;
	virtual D3DXVECTOR3 GetUp() const;

	virtual D3DXMATRIX GetMat() const;
	//В локальных координатах, включая дочерние узлы
	virtual AABB GetAABB() const;

	graph::BaseSceneNode* GetNode();
};

}

}