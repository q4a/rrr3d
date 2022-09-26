#include "stdafx.h"

#include "lslException.h"
#include "lslResource.h"

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