#pragma once

#include "ISceneControl.h"

namespace r3d
{

namespace edit
{

class Edit;

class SceneControl: public ISceneControl
{
private:
	class Control: public game::ControlEvent
	{
	private:
		SceneControl* _owner;

		bool _shiftAction;

		bool _clDrag;
		bool _startDrag;
		glm::vec3 _clDragOff;
		//
		graph::MovCoordSys::DirMove _clDirMove;
		glm::vec3 _clDirOff;
		//
		graph::ScaleCoordSys::DirMove _clScDirMove;
		glm::vec3 _clStScale;
		//
		bool _clRotating;
		glm::quat _clStartRot;
	public:
		Control(SceneControl* owner);
		void ResetState();

		virtual bool OnMouseClickEvent(const game::MouseClick& mClick);
		virtual bool OnMouseMoveEvent(const game::MouseMove& mMove);
	};
private:
	Edit* _edit;

	SelMode _selMode;
	INodeRef _selNode;
	bool _linkBB;

	Control* _control;
	graph::Actor* _actor;
	graph::MovCoordSys* _movCoordSys;
	graph::ScaleCoordSys* _scaleCoordSys;

	bool ComputeAxeLink(const AABB& aabb, const D3DXMATRIX& aabbToWorld, const D3DXMATRIX& worldToAABB, const glm::vec3& normOff, INode* ignore, float& outDistOff, const float distLink = 1.0f);
	void ComputeLink(INode* node, const glm::vec3& pos, glm::vec3& resPos);

	void CreateGraphActor();
	void ReleaseGraphActor();
	void CreateMovCoordSys();
	void FreeMovCoordSys();
	void CreateScaleCoordSys();
	void FreeScaleCoordSys();

	void ApplySelMode();
public:
	SceneControl(Edit* edit);
	virtual ~SceneControl();

	glm::vec3 ComputePoint(const glm::vec3& curPos, const glm::vec3& rayStart, const glm::vec3& rayVec, DirMove dirMove, const glm::vec3& centerOff);
	glm::vec3 ComputePos(INode* node, const glm::vec3& rayStart, const glm::vec3& rayVec, DirMove dirMove, const glm::vec3& centerOff);

	INodeRef GetSelNode();
	void SelectNode(const INodeRef& node);

	SelMode GetSelMode() const;
	void SetSelMode(SelMode value);

	bool GetLinkBB() const;
	void SetLinkBB(bool value);
};

}

}