#ifndef D3D_EXCEPTIONS
#define D3D_EXCEPTIONS

#include "r3dMessages.h"
#include <stdexcept>

namespace r3d
{

class EInitD3D9Failed: public std::runtime_error
{
public:
	EInitD3D9Failed(const char* message = sInitD3D9Failed): runtime_error(message){}
};

class EInvalidParent: public std::runtime_error
{
public:
	EInvalidParent(const char* message = sInvalidParent): runtime_error(message){}
};

class ERenderObjectError: public std::runtime_error
{
public:
	ERenderObjectError(const char* message = sRenderObjectError): runtime_error(message){}
};

class EInvalidData: public std::runtime_error
{
public:
	EInvalidData(const char* message = sInvalidData): runtime_error(message){}
};

class D3DException: public std::runtime_error
{
private:
	HRESULT _eCode;
public:
	D3DException(HRESULT eCode, const char* message): runtime_error(message), _eCode(eCode){}
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