#pragma once

#include "SQLiteWrapper_base.h"
#include <string>
#include <filesystem>
#include <QObject>
#include <QTimer>


namespace SQLiteWrapper
{
	/**
	 * @brief Watches a file for changes
	 * A signal is emitted when the file changes
	 */
	class SQLITE_WRAPPER_EXPORT FileChangeWatcher : public QObject
	{
		Q_OBJECT
	public:
		enum Mode
		{
			polling,  // checks for a file change when asked
			winApi    // Uses the "FindFirstChangeNotification" function to monitor file changes (does not work on network drives)
		};
		FileChangeWatcher(const std::string& path);
		FileChangeWatcher(const std::string& path, Mode mode);
		~FileChangeWatcher();

		void setMode(Mode mode);
		Mode getMode() const { return m_mode; }
		const std::string& getPath() const { return m_path; }
		
		

		void pause();
		void unpause();
		bool isPaused() const;

	signals:
		void onFileChanged(const std::string& path);

		void onFileChangedInternal(QPrivateSignal*);
	private slots:
		void onFileChangedInternalSlot(QPrivateSignal*);
		void onPollingTimerTimeout();
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

		const std::string m_path;
		Mode m_mode;
		std::filesystem::file_time_type m_lastModificationTime;
		std::atomic<bool> m_fileChanged;
		std::atomic<bool> m_paused;
	};
}