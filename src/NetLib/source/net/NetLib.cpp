#include "stdafx.h"

#include "net\NetLib.h"
#include "net\NetService.h"

namespace net
{

NetService netService;

INetService& GetNetService()
{
	return netService;
}

}