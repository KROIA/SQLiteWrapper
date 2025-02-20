#pragma once

#include "SQLiteWrapper_base.h"
#include <string>


namespace SQLiteWrapper
{
	namespace Utilities
	{
		std::string getLastErrorString(DWORD error)
		{
			std::string errorString;
			LPSTR messageBuffer = nullptr;
			size_t size = FormatMessageA(
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL,
				error,
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				(LPSTR)&messageBuffer,
				0,
				NULL);
			errorString = std::string(messageBuffer, size);
			LocalFree(messageBuffer);
			return errorString;
		}
	}
}