#pragma once

#include "lslObject.h"
#include "r3dMath.h"
#include "lslUtility.h"
#include "lslAutoRef.h"

namespace r3d {using namespace lsl;}

#include "IView.h"
#include "IEdit.h"
#include "ICameraManager.h"

namespace r3d
{

const UINT WM_GRAPH_EVENT = WM_APP + 1;

namespace game
{

class IWorld: public Object
{
public:
	virtual void MainProgress() = 0;	

	virtual void ResetInput(bool reset = true) = 0;
	virtual bool InputWasReset() const = 0;

	virtual void RunGame() = 0;
	virtual void ExitGame() = 0;
	
	virtual void RunWorldEdit() = 0;
	virtual void ExitWorldEdit() = 0;

	virtual void SaveLevel(const std::string& level) = 0;
	virtual void LoadLevel(const std::string& level) = 0;

	virtual IView* GetView() = 0;
	virtual edit::IEdit* GetEdit() = 0;
	virtual ICameraManager* GetICamera() = 0;

	virtual bool IsVideoPlaying() const = 0;

	virtual bool IsTerminate() const = 0;
	virtual int GetTerminateResult() const = 0;

	virtual bool OnPaint(HWND handle) = 0;
	virtual void OnDisplayChange() = 0;
	virtual void OnWMGraphEvent() = 0;
};

class World;

}

}