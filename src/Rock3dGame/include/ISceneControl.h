#pragma once

#include "IMapObj.h"

namespace r3d
{

namespace edit
{

//�� ����� ���� �������� ���� �� ����������� �����������, � ������� ���������� ���������������� ���� � ����������� � ������, � �� ������ ���������� �������� ��������� �� ����������� ������������. � ����� ���������� ������� ����������� ��� ���������� ��������� �����
class IScNodeCont: public Object
{
public:
	virtual void Select(bool active) = 0;
	//������� �������� ����
	virtual bool RayCastInters(const glm::vec3& rayPos, const glm::vec3& rayVec) const = 0;
	//�������� � �������� �����
	virtual bool Compare(const IMapObjRef& node) const = 0;

	//������������� � ������ none
	virtual void OnStartDrag(const glm::vec3& scrRayPos, const glm::vec3& scrRayVec) {};
	virtual void OnEndDrag(const glm::vec3& scrRayPos, const glm::vec3& scrRayVec) {};
	virtual void OnDrag(const glm::vec3& pos, const glm::vec3& scrRayPos, const glm::vec3& scrRayVec) {};
	//�������� ��� ������� shift
	virtual void OnShiftAction(const glm::vec3& scrRayPos, const glm::vec3& scrRayVec) {};

	//� ������� �����������
	virtual glm::vec3 GetPos() const = 0;
	virtual void SetPos(const glm::vec3& value) = 0;
	//
	virtual glm::quat GetRot() const = 0;
	virtual void SetRot(const glm::quat& value) = 0;
	//
	virtual glm::vec3 GetScale() const = 0;
	virtual void SetScale(const glm::vec3& value) = 0;
	//
	virtual glm::vec3 GetDir() const = 0;
	virtual glm::vec3 GetRight() const = 0;
	virtual glm::vec3 GetUp() const = 0;

	virtual D3DXMATRIX GetMat() const = 0;
	//������� �������� ����
	virtual AABB GetAABB() const = 0;
};

typedef lsl::AutoRef<IScNodeCont> IScNodeContRef;

class ISceneControl: public Object
{
public:
	enum SelMode {smNone, smLink, smMove, smRotate, smScale};
	enum DirMove {dmNone, dmX, dmY, dmZ, dmXY, dmXZ, dmYZ, dmView, cDirMoveEnd};

	typedef IScNodeCont INode;
	typedef IScNodeContRef INodeRef;
public:
	virtual glm::vec3 ComputePoint(const glm::vec3& curPos, const glm::vec3& rayStart, const glm::vec3& rayVec, DirMove dirMove, const glm::vec3& centerOff) = 0;
	virtual glm::vec3 ComputePos(INode* node, const glm::vec3& rayStart, const glm::vec3& rayVec, DirMove dirMove, const glm::vec3& centerOff) = 0;

	virtual INodeRef GetSelNode() = 0;
	virtual void SelectNode(const INodeRef& value) = 0;

	virtual SelMode GetSelMode() const = 0;
	virtual void SetSelMode(SelMode value) = 0;

	virtual bool GetLinkBB() const = 0;
	virtual void SetLinkBB(bool value) = 0;
};

}

}