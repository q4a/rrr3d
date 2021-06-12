#ifndef D3D_EXCEPTIONS
#define D3D_EXCEPTIONS

#include "r3dMessages.h"
#include <exception>

namespace r3d
{

class EInitD3D9Failed: public std::exception
{
public:
	EInitD3D9Failed(const char* message = sInitD3D9Failed): exception(message){}
};

class EInvalidParent: public std::exception
{
public:
	EInvalidParent(const char* message = sInvalidParent): exception(message){}
};

class ERenderObjectError: public std::exception
{
public:
	ERenderObjectError(const char* message = sRenderObjectError): exception(message){}
};

class EInvalidData: public std::exception
{
public:
	EInvalidData(const char* message = sInvalidData): exception(message){}
};

class D3DException: public std::exception
{
private:
	HRESULT _eCode;
public:
	D3DException(HRESULT eCode, const char* message): exception(message), _eCode(eCode){}
};

class EGetD3DCaps9Failed: public D3DException
{
public:
	EGetD3DCaps9Failed(const HRESULT eCode, const char* message = sGetD3DCaps9Failed): D3DException(eCode, message){}
};

class ECreateD3DDevice9Failed: public D3DException
{
public:
	ECreateD3DDevice9Failed(const HRESULT eCode, const char* message = sCreateD3DDevice9Failed): D3DException(eCode, message){}
};

}

#endif