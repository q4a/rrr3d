#pragma once

namespace r3d
{

namespace graph
{

class IProgressUser: public virtual lsl::ObjReference
{
private:
	int _progressCount;
	bool _ignorePause;
public:
	IProgressUser(): _progressCount(0), _ignorePause(false) {}

	virtual void OnProgress(float deltaTime) = 0;

	void RegProgress();
	void UnregProgress();

	bool ignorePause() const {return _ignorePause;}
	void ignorePause(bool value) {_ignorePause = value;}
};

class ProgressTimer
{
private:
	typedef lsl::List<IProgressUser*> Users;
private:
	Users _users;
public:
	~ProgressTimer();

	void RegUser(IProgressUser* user);
	void UnregUser(IProgressUser* user);

	void Progress(float deltaTime, bool pause);

	static ProgressTimer& I();
};

}

}