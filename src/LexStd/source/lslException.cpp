#include "stdafx.h"

#include "lslException.h"
#include "lslResource.h"

/*
 * _CrtDbgBreak, for the IDRETRY branch of the assert box below.
 *
 * lslCommon.h includes <crtdbg.h> only under _DEBUG, because what it wants from
 * it there is the debug heap. MSVC gets the macro anyway through its own header
 * chain, so this file has always compiled on Windows in both configurations --
 * but off Windows it is XPlatform's crtdbg.h or nothing, and a Release build
 * stopped here with "use of undeclared identifier '_CrtDbgBreak'".
 *
 * Only on non-MSVC, so the Windows build is left exactly as it was. XPlatform's
 * header is safe to include outside a debug build: the allocator entry points
 * it declares are macros onto plain malloc and free.
 */
#ifndef _MSC_VER
#include <crtdbg.h>
#endif

namespace lsl
{

AppLog appLog("appLog.txt");




AppLog::AppLog(const std::string& mFileName): fileName(mFileName), _destroy(false)
{
}

AppLog::~AppLog()
{
	_destroy = true;
}

std::ostream* AppLog::CreateStream()
{
	return FileSystem::GetInstance()->NewOutStream(fileName, FileSystem::omText, FileSystem::cAppend);
}

void AppLog::ReleaseStream(std::ostream* stream)
{
	FileSystem::GetInstance()->FreeStream(stream);
}

void AppLog::Clear()
{
	std::ostream* stream = FileSystem::GetInstance()->NewOutStream(fileName, FileSystem::omText, FileSystem::cTruncate);
	ReleaseStream(stream);
}




Error::Error(const char* message): _MyBase(message)
{
	PrintToLog();

	LSL_ASSERT(false);
}

Error::Error(const std::string& message): _MyBase(message.c_str())
{
	PrintToLog();

	LSL_ASSERT(false);
}

void Error::PrintToLog()
{
	appLog << "Error: " << this->what() << '\n';
}




void Assert(const char* expression, const char* filePath, int line)
{
	static char sText[1024] = "";
	sprintf_s(sText, sizeof(sText),
			"Error: ( %s )\r\n"
			"File '%s', Line %d\r\n"
			"Abort execution, allow assert Retry, or Ignore in future?",
			expression, filePath, line);
	
	//appLog << "assError: " << expression << " File: " << filePath << " Line: " << line << '\n';
	
	switch (::MessageBox(0, sText, "ASSERT ERROR", MB_ABORTRETRYIGNORE | MB_TASKMODAL))
	{
	case IDIGNORE:
		break;

	case IDABORT:
		exit(1);
		break;

	case IDRETRY:
		 _CrtDbgBreak();
	}
}

}