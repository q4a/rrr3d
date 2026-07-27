#ifndef D3D_EXCEPTIONS
#define D3D_EXCEPTIONS

#include "r3dMessages.h"
#include <exception>
#include <string>

namespace r3d
{

class EInitD3D9Failed: public std::exception
{
public:
	EInitD3D9Failed(const char* message = sInitD3D9Failed): _message(message ? message : "") {}
	virtual const char* what() const noexcept override { return _message.c_str(); }
private:
	std::string _message;
};

class EInvalidParent: public std::exception
{
public:
	EInvalidParent(const char* message = sInvalidParent): _message(message ? message : "") {}
	virtual const char* what() const noexcept override { return _message.c_str(); }
private:
	std::string _message;
};

class ERenderObjectError: public std::exception
{
public:
	ERenderObjectError(const char* message = sRenderObjectError): _message(message ? message : "") {}
	virtual const char* what() const noexcept override { return _message.c_str(); }
private:
	std::string _message;
};

class EInvalidData: public std::exception
{
public:
	EInvalidData(const char* message = sInvalidData): _message(message ? message : "") {}
	virtual const char* what() const noexcept override { return _message.c_str(); }
private:
	std::string _message;
};

class D3DException: public std::exception
{
private:
	HRESULT _eCode;
public:
	D3DException(HRESULT eCode, const char* message): _message(message ? message : ""), _eCode(eCode) {}
	virtual const char* what() const noexcept override { return _message.c_str(); }
private:
	std::string _message;
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