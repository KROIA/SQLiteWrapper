#include "LockFile.h"
#include "Utilities.h"

namespace SQLiteWrapper
{
	LockFile::LockFile(const std::string& path)
		: m_path(path)
		, m_handle(INVALID_HANDLE_VALUE)
		, m_locked(false)
	{

	}
	LockFile::~LockFile()
	{
		if (m_locked)
			releaseLock();

	}


	LockFile::LockStatus LockFile::tryGetLock()
	{
		if (m_locked)
			return LockStatus::alreadyLocked;
		HANDLE fileHandle = CreateFile(
#ifdef UNICODE
			Utilities::strToWstr(m_path).c_str(),
#else
			m_path.c_str(),
#endif
			GENERIC_WRITE,
			0,
			nullptr,
			CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			nullptr
		);

		if (fileHandle == INVALID_HANDLE_VALUE) {
			// Failed to open the file, indicating it might be locked
			m_locked = false;
			Logger::logError("LockFile: Creating lock file: " + Utilities::getLastErrorString(GetLastError()) + "\n");
			return LockStatus::unableToCreateLockFile; // File is possibly locked
		}

		if (!::LockFile(fileHandle, 0, 0, MAXDWORD, MAXDWORD))
		{
			m_locked = false;
			Logger::logError("LockFile: Locking lock file: " + Utilities::getLastErrorString(GetLastError()) + "\n");
			CloseHandle(fileHandle);
			m_handle = INVALID_HANDLE_VALUE;
			return LockStatus::unableToLock;
		}
		m_handle = fileHandle;
		m_locked = true;
		return LockStatus::locked;
	}
	LockFile::UnlockStatus LockFile::releaseLock()
	{
		if (!m_locked)
			return UnlockStatus::alreadyUnlocked;
		if (!UnlockFile(m_handle, 0, 0, MAXDWORD, MAXDWORD))
		{
			Logger::logError("LockFile: Unlocking lock file: " + Utilities::getLastErrorString(GetLastError()) + "\n");
			CloseHandle(m_handle);
			m_handle = INVALID_HANDLE_VALUE;
			return UnlockStatus::unableToUnlock;
		}
		CloseHandle(m_handle);
		m_handle = INVALID_HANDLE_VALUE;
		// Delete file
		DeleteFile(
#ifdef UNICODE
			Utilities::strToWstr(m_path).c_str()
#else
			m_path.c_str()
#endif
			);
		
		
		m_locked = false;
		return LockFile::unlocked;		
	}
}