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
		D3DXVECTOR3 _clDragOff;
		//
		graph::MovCoordSys::DirMove _clDirMove;
		D3DXVECTOR3 _clDirOff;
		//
		graph::ScaleCoordSys::DirMove _clScDirMove;
		D3DXVECTOR3 _clStScale;
		//
		bool _clRotating;
		D3DXQUATERNION _clStartRot;		
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

	bool ComputeAxeLink(const AABB& aabb, const D3DXMATRIX& aabbToWorld, const D3DXMATRIX& worldToAABB, const D3DXVECTOR3& normOff, INode* ignore, float& outDistOff, const float distLink = 1.0f);
	void ComputeLink(INode* node, const D3DXVECTOR3& pos, D3DXVECTOR3& resPos);

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

	D3DXVECTOR3 ComputePoint(const D3DXVECTOR3& curPos, const D3DXVECTOR3& rayStart, const D3DXVECTOR3& rayVec, DirMove dirMove, const D3DXVECTOR3& centerOff);
	D3DXVECTOR3 ComputePos(INode* node, const D3DXVECTOR3& rayStart, const D3DXVECTOR3& rayVec, DirMove dirMove, const D3DXVECTOR3& centerOff);

	INodeRef GetSelNode();
	void SelectNode(const INodeRef& node);

	SelMode GetSelMode() const;
	void SetSelMode(SelMode value);

	bool GetLinkBB() const;
	void SetLinkBB(bool value);
};

}

}