#pragma once

namespace net
{

class INetChannel
{
public:	
	virtual unsigned ping() const = 0;
	virtual unsigned pingTime() const = 0;
	virtual unsigned bytesSend() const = 0;
	virtual unsigned bytesReceived() const = 0;
};

}