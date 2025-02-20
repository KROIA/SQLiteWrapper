#include "FileChangeWatcher.h"
#include "Utilities.h"
#include <fstream>
#include <QFile>
#include <QDir>
#include <QCryptographicHash>
#include <QByteArray>
#include <QApplication>


namespace SQLiteWrapper
{
	FileChangeWatcher::FileChangeWatcher(const std::string& path)
		: m_path(path)
		, m_mode(Mode::polling)
	{
		m_stopFlag.store(false);
		m_paused.store(false);
		m_fileChanged.store(false);
		m_eventHandle.store(nullptr);
		connect(&m_timer, &QTimer::timeout, this, &FileChangeWatcher::onPollingTimerTimeout);
		connect(this, &FileChangeWatcher::onFileChangedInternal, this, &FileChangeWatcher::onFileChangedInternalSlot, Qt::QueuedConnection);
		startWatching();
	}
	FileChangeWatcher::FileChangeWatcher(const std::string& path, Mode mode)
		: m_path(path)
		, m_mode(mode)
	{
		m_stopFlag.store(false);
		m_paused.store(false);
		m_fileChanged.store(false);
		m_eventHandle.store(nullptr);
		connect(&m_timer, &QTimer::timeout, this, &FileChangeWatcher::onPollingTimerTimeout);
		connect(this, &FileChangeWatcher::onFileChangedInternal, this, &FileChangeWatcher::onFileChangedInternalSlot, Qt::QueuedConnection);
		startWatching();
	}

	FileChangeWatcher::~FileChangeWatcher()
	{
		stopWatching();
	}

	void FileChangeWatcher::setMode(Mode mode)
	{
		if (mode == m_mode)
			return;
		stopWatching();
		m_mode = mode;
		startWatching();
	}
	bool FileChangeWatcher::hasChanged()
	{
		if (m_mode == Mode::polling)
			checkFile();
		std::unique_lock<std::mutex> lock(m_mutex);
		return m_fileChanged;
	}
	void FileChangeWatcher::clearFileChangedFlag()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_fileChanged.store(false);
		if (m_mode == Mode::winApi)
			m_cv.notify_all();
	}


	void FileChangeWatcher::pause()
	{
		m_paused.store(true);
		if (m_mode == Mode::polling)
			m_timer.stop();
	}
	void FileChangeWatcher::unpause()
	{
		m_paused.store(false);
		if(m_mode == Mode::polling)
			m_timer.start();
	}
	bool FileChangeWatcher::isPaused() const
	{
		return m_paused.load();
	}

	void FileChangeWatcher::startWatching()
	{
		if (m_mode == Mode::polling)
		{
			// start polling
			checkFile();
			m_timer.start(10);
		}
		else
		{
			if (!m_watchThread)
			{
				fileChanged();
				m_stopFlag.store(false);
				m_watchThread = new std::thread(&FileChangeWatcher::monitorFile, this);
			}
		}
	}
	void FileChangeWatcher::stopWatching()
	{
		if (m_mode == Mode::polling)
		{
			// stop polling
			m_timer.stop();
		}
		else
		{
			if (m_watchThread)
			{
				{
					std::unique_lock<std::mutex> lock(m_mutex);
					m_stopFlag.store(true);
					m_cv.notify_all();
				}
				m_watchThread->join();
				delete m_watchThread;
				m_watchThread = nullptr;
			}
		}
	}


	void FileChangeWatcher::onFileChangedInternalSlot(QPrivateSignal*)
	{
		// This function is called when the file has changed
		// Emit the onFileChanged signal
		clearFileChangedFlag();
		emit onFileChanged(m_path);
	}
	void FileChangeWatcher::onPollingTimerTimeout()
	{
		if (hasChanged())
		{
			emit onFileChanged(m_path);
		}
	}

	bool FileChangeWatcher::fileChanged()
	{
		std::filesystem::path file(m_path);

		if (!std::filesystem::exists(file)) {
			Logger::logError("FileChangeWatcher: File: \"" + m_path + "\" does not exist!");
			return false;
		}
		// Wait for a short duration to ensure any ongoing file operation is completed
		// std::this_thread::sleep_for(std::chrono::milliseconds(100));

		// Get the last modification time of the file
		std::filesystem::file_time_type change = std::filesystem::last_write_time(file);


		// Check if the file has been modified since lastWriteTime
		if (change > m_lastModificationTime) {
			HANDLE fileHandle = CreateFile(
#ifdef UNICODE
				Utilities::strToWstr(m_path).c_str(),
#else
				m_path.c_str(),
#endif                     
				GENERIC_READ,
				FILE_SHARE_READ,
				nullptr,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				nullptr
			);

			if (fileHandle == INVALID_HANDLE_VALUE)
			{
				// pause for 1 ms
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				return false;
			}
			m_lastModificationTime = change;
			// Close the file handle
			CloseHandle(fileHandle);
			return true;
		}

		return false; // File has not changed
	}

	void FileChangeWatcher::monitorFile()
	{
		SQLW_FILE_WATCHER_PROFILING_THREAD("FileChangeWatcher");
#ifdef SQLW_PROFILING
		std::string title = ("FileChangeWatcher \"" + m_filePath + "\"");
		SQLW_FILE_WATCHER_PROFILING_BLOCK(title.c_str(), SQLW_COLOR_STAGE_7);
#endif
		DWORD bytesReturned;
		BYTE buffer[4096];

		std::string directory = m_path.substr(0, m_path.find_last_of("\\") + 1);
		if (directory.empty())
		{
			// Get absolute path
			QDir dir = QDir::current();
			directory = dir.absolutePath().toStdString();
		}
#ifdef UNICODE
		m_eventHandle.store(FindFirstChangeNotification(Utilities::strToWstr(directory).c_str(), FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE));
#else
		m_eventHandle.store(FindFirstChangeNotification(directory.c_str(), FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE));
#endif

		if (m_eventHandle.load() == INVALID_HANDLE_VALUE) {
			Logger::logError("FileChangeWatcher: Starting directory monitoring: " + Utilities::getLastErrorString(GetLastError()) + "\n");
			return;
		}

		//fileChanged(); // Set initial file modification time
		while (!m_stopFlag.load()) {
			SQLW_FILE_WATCHER_PROFILING_BLOCK("while", SQLW_COLOR_STAGE_8);
			DWORD waitResult = WAIT_FAILED;
			{
				SQLW_FILE_WATCHER_PROFILING_BLOCK("waitForChange", SQLW_COLOR_STAGE_9);
				while (waitResult != WAIT_OBJECT_0)
				{
					waitResult = WaitForSingleObject(m_eventHandle.load(), 1000);
					if (m_stopFlag.load())
					{
						waitResult = WAIT_FAILED;
						goto exitThread;
					}
				}
			}
			if (waitResult == WAIT_OBJECT_0) {
				SQLW_FILE_WATCHER_PROFILING_BLOCK("readFileChange", SQLW_COLOR_STAGE_9);

				HANDLE cHandle = m_eventHandle.load();
				if (cHandle)
				{
					if (m_stopFlag.load())
					{
						goto exitThread;
					}
					bool res = FindNextChangeNotification(cHandle);
#ifdef SQLW_DEBUG
					if (!res)
					{
						DWORD error = GetLastError();
						Logger::logError("FileChangeWatcher: FindNextChangeNotification. GetLastError() =  " + std::to_string(error) + " : " + Utilities::getLastErrorString(error));
					}
#else 
					SQLW_UNUSED(res);
#endif
				}

				if (fileChanged() && !m_paused.load())
				{
					SQLW_FILE_WATCHER_PROFILING_BLOCK("Change detectd, waitForLockRelease", SQLW_COLOR_STAGE_9);
					std::unique_lock<std::mutex> lock(m_mutex);

					m_fileChanged.store(true);
					Logger::logInfo("FileChangeWatcher: File changed: " + m_path);
					emit onFileChangedInternal(nullptr);
					while (m_fileChanged && !m_stopFlag.load()) {
						QApplication::processEvents();
						m_cv.wait(lock);
					}

					if (m_stopFlag.load()) {
						break;
					}
				}
			}
#ifdef SQLW_DEBUG
			else {
				DWORD error = GetLastError();
				Logger::logError("FileChangeWatcher: Waiting for file changes. GetLastError() =  " + std::to_string(error) + " : " + Utilities::getLastErrorString(error));
			}
#endif
		}
	exitThread:;

		FindCloseChangeNotification(m_eventHandle.load());
		m_eventHandle.store(nullptr);
	}
	void FileChangeWatcher::checkFile()
	{
		bool success;
		std::string md5 = calculateMD5Hash(success);
		if (!success)
		{
#ifdef SQLW_DEBUG
			Logger::logError("FileChangeWatcher: Could not calculate the MD5 hash of the file: " + m_path);
#endif
			return;
		}
		if (md5 != m_md5 && m_md5 != "")
		{
			m_fileChanged.store(true);
			Logger::logInfo("FileChangeWatcher: File changed: " + m_path);
		}
		m_md5 = md5;
	}

	std::string FileChangeWatcher::calculateMD5Hash(bool& success)
	{
		QFile file(m_path.c_str());
		if (!file.open(QIODevice::ReadOnly)) {
#ifdef SQLW_DEBUG
			Logger::logError("FileChangeWatcher: Could not open file: " + m_path +" to calculate the MD5 hash");
#endif
			success = false;
			return "";
		}

		QCryptographicHash hash(QCryptographicHash::Md5);
		if (!hash.addData(&file)) {
#ifdef SQLW_DEBUG
			Logger::logError("FileChangeWatcher: Could not read file: " + m_path + " to calculate the MD5 hash");
#endif
			success = false;
			return "";
		}

		QByteArray result = hash.result();
		file.close();
		success = true;
		return result.toHex().constData();
	}
}