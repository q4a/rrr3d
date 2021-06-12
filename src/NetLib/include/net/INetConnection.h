#pragma once

namespace net
{

class INetConnectionImplUser: public virtual lsl::Object
{
public:
	virtual void OnAccepted(bool success) = 0;
	virtual void OnConnected(bool success) = 0;
	virtual void OnDisconnected() = 0;
	virtual void OnSend(unsigned numBytes, bool success)  = 0;
};

class INetConnectionImpl
{
public:
	typedef lsl::List<INetConnectionImplUser*> Users;
private:
	Users _users;
protected:
	void NotifyAccepted(bool success)
	{
		for (Users::const_iterator iter = _users.begin(); iter != _users.end(); ++iter)
			(*iter)->OnAccepted(success);
	}

	void NotifyConnected(bool success)
	{
		for (Users::const_iterator iter = _users.begin(); iter != _users.end(); ++iter)
			(*iter)->OnConnected(success);
	}

	void NotifyDisconnected()
	{
		for (Users::const_iterator iter = _users.begin(); iter != _users.end(); ++iter)
			(*iter)->OnDisconnected();
	}

	void NotifySend(unsigned numBytes, bool success)
	{
		for (Users::const_iterator iter = _users.begin(); iter != _users.end(); ++iter)
			(*iter)->OnSend(numBytes, success);
	}
public:
	virtual bool Send(const void* data, unsigned size) = 0;
	virtual bool Receive(void* data, unsigned size, unsigned& numBytes) = 0;
	virtual bool IsAvailable(unsigned& size) = 0;

	virtual void Accept() = 0;
	virtual bool Connect(const Endpoint& endpoint) = 0;
	virtual void Close() = 0;
	virtual bool isOpen() const = 0;

	virtual Endpoint localEndpoint() = 0;
	virtual Endpoint remoteEndpoint() = 0;
	virtual lsl::string userName() const = 0;

	void RegUser(INetConnectionImplUser* user)
	{
		if (_users.IsFind(user))
			return;

		user->AddRef();
		_users.push_back(user);
	}

	void UnRegUser(INetConnectionImplUser* user)
	{
		if (!_users.IsFind(user))
			return;

		user->Release();
		_users.Remove(user);
	}
};

class INetChannelImplUser: public virtual lsl::Object
{
public:
	virtual void OnSend(unsigned numBytes, bool success)  = 0;
};

class INetChannelImpl
{
public:
	typedef lsl::List<INetChannelImplUser*> Users;
private:
	Users _users;
protected:
	void NotifySend(unsigned numBytes, bool success)
	{
		for (Users::const_iterator iter = _users.begin(); iter != _users.end(); ++iter)
			(*iter)->OnSend(numBytes, success);
	}
public:
	virtual bool Send(const Endpoint& endpoint, const void* data, unsigned size) = 0;
	virtual bool Receive(void* data, unsigned size, unsigned& numBytes, Endpoint& remoteEndpoint, bool fromOutput) = 0;
	virtual bool IsAvailable(unsigned& size, bool fromOutput) = 0;
	virtual void Flush() = 0;

	virtual void Open(bool broadcast) = 0;
	virtual void Bind(const Endpoint& endpoint) = 0;
	virtual void Unbind() = 0;
	virtual void Close() = 0;
	virtual bool IsOpen() const = 0;
	virtual bool IsBind() const = 0;
	virtual bool broadcast() const = 0;
	virtual Endpoint bindEndpoint() const = 0;

	virtual Endpoint localEndpoint() = 0;

	void RegUser(INetChannelImplUser* user)
	{
		if (_users.IsFind(user))
			return;

		user->AddRef();
		_users.push_back(user);
	}

	void UnRegUser(INetChannelImplUser* user)
	{
		if (!_users.IsFind(user))
			return;

		user->Release();
		_users.Remove(user);
	}
};

class INetAcceptorImpl
{
public:
	virtual void Listen(const Endpoint& endpoint) = 0;
	virtual void Close() = 0;

	virtual INetConnectionImpl* NewConnection() = 0;
	virtual void ReleaseConnection(INetConnectionImpl* connection) = 0;

	virtual INetChannelImpl* NewChannel() = 0;
	virtual void ReleaseChannel(INetChannelImpl* connection) = 0;
};

class INetConnection
{
public:
	virtual unsigned id() const = 0;
	virtual unsigned ping() const = 0;
	virtual unsigned pingTime() const = 0;
	virtual unsigned bytesSend() const = 0;
	virtual unsigned bytesReceived() const = 0;

	virtual lsl::string userName() const = 0;
};

}