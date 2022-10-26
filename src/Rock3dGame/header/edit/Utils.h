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

	virtual bool RayCastInters(const glm::vec3& rayPos, const glm::vec3& rayVec) const;
	virtual bool Compare(const IMapObjRef& node) const;

	virtual glm::vec3 GetPos() const;
	virtual void SetPos(const glm::vec3& value);
	//
	virtual glm::quat GetRot() const;
	virtual void SetRot(const glm::quat& value);
	//
	virtual glm::vec3 GetScale() const;
	virtual void SetScale(const glm::vec3& value);
	//
	virtual glm::vec3 GetDir() const;
	virtual glm::vec3 GetRight() const;
	virtual glm::vec3 GetUp() const;

	virtual D3DXMATRIX GetMat() const;
	//В локальных координатах, включая дочерние узлы
	virtual AABB GetAABB() const;

	graph::BaseSceneNode* GetNode();
};

}

}