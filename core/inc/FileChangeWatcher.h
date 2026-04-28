#pragma once

#include "SQLiteWrapper_base.h"
#include <string>
#include <filesystem>
#include <QObject>
#include <QTimer>
#include <QThread>
#include "Logger.h"

namespace 
{
	class PollingTimerThread;
}

namespace SQLiteWrapper
{
	/**
	 * @brief Watches a file for changes
	 * A signal is emitted when the file changes
	 */
	
	class SQLITE_WRAPPER_API FileChangeWatcher : public QObject
	{
		Q_OBJECT
	public:
		enum Mode
		{
			polling,  // background timer calculates hashes and sets a change flag
			winApi    // Uses the "FindFirstChangeNotification" function to monitor file changes (does not work on network drives)
		};
		FileChangeWatcher();
		FileChangeWatcher(const std::string& path);
		FileChangeWatcher(const std::string& path, Mode mode);
		~FileChangeWatcher();

		void setMode(Mode mode);
		Mode getMode() const { return m_mode; }
		void setPath(const std::string& path);
		const std::string& getPath() const { return m_path; }
		void setModeAndPath(Mode mode, const std::string& path);
		
		void setLoggerParent(const Log::LogObject& parent)
		{
			m_logger.setParentID(parent.getID());
		}
		void setPollingTimerInterval(int intervalMs);
		int getPollingTimerInterval() const;
		

		void pause();
		void unpause();
		bool isPaused() const;

	signals:
		void onFileChanged(const std::string& path);

		void onFileChangedInternal(QPrivateSignal*);
	private slots:
		void onFileChangedInternalSlot(QPrivateSignal*);
		void onPollingTimerTimeout();

	protected:
		/**
		 * @brief Helpoer function to log messages.
		 * @param msg
		 */
		void debug(const std::string& msg) const;
		void info(const std::string& msg) const;
		void warning(const std::string& msg) const;
		void error(const std::string& msg) const;

	private:
		bool hasChanged();
		void clearFileChangedFlag();

		void startWatching();
		void stopWatching();
		bool fileChanged();
		std::string calculateMD5Hash(bool& success);

		// For WinAPI mode
		void monitorFile();
		std::thread* m_watchThread = nullptr;
		std::atomic<HANDLE> m_eventHandle;
		std::mutex m_mutex;
		std::condition_variable m_cv;
		std::atomic<bool> m_stopFlag;

		// For polling mode
		void checkFile();
		std::string m_md5;
		QTimer m_timer;
		PollingTimerThread* m_pollingThread = nullptr;

		std::string m_path;
		Mode m_mode;
		std::filesystem::file_time_type m_lastModificationTime;
		std::atomic<bool> m_fileChanged;
		std::atomic<bool> m_paused;
		unsigned int m_lastCheckTimeMillis = 0;
		mutable Log::LogObject m_logger;
	};
}
