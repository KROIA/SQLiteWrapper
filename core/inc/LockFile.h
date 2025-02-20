#pragma once

#include "SQLiteWrapper_base.h"
#include <string>

namespace SQLiteWrapper
{
	/**
	 * @brief This class creates a lock file. 
	 * A locked lockfile can not be deleted by any other process.
	 */
	class SQLITE_WRAPPER_EXPORT LockFile
	{
	public:
		enum LockStatus
		{
			locked,
			alreadyLocked,
			unableToCreateLockFile,
			unableToLock,
		};
		enum UnlockStatus
		{
			unlocked,
			alreadyUnlocked,
			unableToUnlock
		};
		LockFile(const std::string& path);
		~LockFile();

		const std::string& getPath() const { return m_path; }

		LockStatus tryGetLock();
		UnlockStatus releaseLock();
		bool isLocked() const { return m_locked; }

	private:
		const std::string m_path;
		HANDLE m_handle;
		bool m_locked;
	};
}