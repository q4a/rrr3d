#include "stdafx.h"
#include "ProgressTimer.h"

namespace r3d
{

namespace graph
{

namespace
{
	ProgressTimer progressTimer;
}




void IProgressUser::RegProgress()
{
	if (++_progressCount == 1)
		progressTimer.RegUser(this);
}

void IProgressUser::UnregProgress()
{
	LSL_ASSERT(_progressCount > 0);

	if (--_progressCount == 0)
		progressTimer.UnregUser(this);
}




ProgressTimer::~ProgressTimer()
{
	LSL_ASSERT(_users.empty());
}

void ProgressTimer::RegUser(IProgressUser* user)
{
	if (_users.IsFind(user))
		return;

	user->AddRef();
	_users.push_back(user);
}

void ProgressTimer::UnregUser(IProgressUser* user)
{
	if (!_users.IsFind(user))
		return;

	_users.Remove(user);
	user->Release();
}

void ProgressTimer::Progress(float deltaTime, bool pause)
{
	for (Users::const_iterator iter = _users.begin(); iter != _users.end(); ++iter)
		if (!pause || (*iter)->ignorePause())
		{
			(*iter)->OnProgress(deltaTime);
		}
}

ProgressTimer& ProgressTimer::I()
{
	return progressTimer;
}

}

}