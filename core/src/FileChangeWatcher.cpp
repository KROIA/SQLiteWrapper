#include "FileChangeWatcher.h"
#include "Utilities.h"
#include <fstream>
#include <QFile>
#include <QDir>
#include <QTimer>
#include <QCryptographicHash>
#include <QByteArray>
#include <QApplication>
#include <functional>


namespace
{
	class PollingTimerThread : public QThread
	{
	public:
		PollingTimerThread(int intervalMs, std::function<void()> callback)
			: m_timerIntervalMs(intervalMs)
			, m_callback(std::move(callback))
		{

		}
		~PollingTimerThread()
		{
			if (m_timer)
			{
				m_timer->stop();
				delete m_timer;
				m_timer = nullptr;
			}
			quit();
			wait();
		}

		void setTimerInterval(int intervalMs)
		{
			if (intervalMs <= 0)
				intervalMs = 1;
			m_timerIntervalMs = intervalMs;
			if (m_timer)
				m_timer->setInterval(m_timerIntervalMs);
		}
		int getTimerInterval() const
		{
			return m_timerIntervalMs;
		}
		void stopTimer()
		{
			if (m_timer)
				m_timer->stop();
		}
		void startTimer()
		{
			if (m_timer)
				m_timer->start(m_timerIntervalMs);
		}


	protected:
		void run() override
		{
			SQLW_FILE_WATCHER_PROFILING_THREAD("FileChangeWatcher");
			m_timer = new QTimer();
			QObject::connect(m_timer, &QTimer::timeout, m_timer, [this]() {
				if (m_callback)
					m_callback();
				});

			if (m_callback)
				m_callback();

			m_timer->start(m_timerIntervalMs);
			exec();
			m_timer->stop();
		}

	private:
		int m_timerIntervalMs;
		QTimer* m_timer = nullptr;
		std::function<void()> m_callback;
	};
}


namespace SQLiteWrapper
{
	FileChangeWatcher::FileChangeWatcher()
		: m_path("")
		, m_mode(Mode::polling)
		, m_logger("FileChangeWatcher")
	{
		m_stopFlag.store(false);
		m_paused.store(false);
		m_fileChanged.store(false);
		m_eventHandle.store(nullptr);
		//connect(&m_timer, &QTimer::timeout, this, &FileChangeWatcher::onPollingTimerTimeout);
		connect(this, &FileChangeWatcher::onFileChangedInternal, this, &FileChangeWatcher::onFileChangedInternalSlot, Qt::QueuedConnection);
		setPollingTimerInterval(1000); // check for changes every 1000 ms
	}
	FileChangeWatcher::FileChangeWatcher(const std::string& path)
		: m_path(path)
		, m_mode(Mode::polling)
		, m_logger("FileChangeWatcher")
	{
		m_stopFlag.store(false);
		m_paused.store(false);
		m_fileChanged.store(false);
		m_eventHandle.store(nullptr);
		//connect(&m_timer, &QTimer::timeout, this, &FileChangeWatcher::onPollingTimerTimeout);
		connect(this, &FileChangeWatcher::onFileChangedInternal, this, &FileChangeWatcher::onFileChangedInternalSlot, Qt::QueuedConnection);
		setPollingTimerInterval(1000); // check for changes every 1000 ms
		startWatching();
	}
	FileChangeWatcher::FileChangeWatcher(const std::string& path, Mode mode)
		: m_path(path)
		, m_mode(mode)
		, m_logger("FileChangeWatcher")
	{
		m_stopFlag.store(false);
		m_paused.store(false);
		m_fileChanged.store(false);
		m_eventHandle.store(nullptr);
		//connect(&m_timer, &QTimer::timeout, this, &FileChangeWatcher::onPollingTimerTimeout);
		connect(this, &FileChangeWatcher::onFileChangedInternal, this, &FileChangeWatcher::onFileChangedInternalSlot, Qt::QueuedConnection);
		setPollingTimerInterval(1000); // check for changes every 1000 ms
		startWatching();
	}

	FileChangeWatcher::~FileChangeWatcher()
	{
		stopWatching();
	}

	void FileChangeWatcher::setPollingTimerInterval(int intervalMs)
	{
		if (intervalMs <= 0)
			intervalMs = 1;

		if (m_pollingThread)
			m_pollingThread->setTimerInterval(intervalMs);
		/*if (m_timer.interval() == intervalMs)
			return;

		const bool wasWatching = (m_mode == Mode::polling) && (m_pollingThread != nullptr);
		if (wasWatching)
			stopWatching();

		m_timer.setInterval(intervalMs);

		if (wasWatching)
			startWatching();*/
	}

	int FileChangeWatcher::getPollingTimerInterval() const
	{
		if (!m_pollingThread)
			return 0;
		return m_pollingThread->getTimerInterval();
	}

	void FileChangeWatcher::setMode(Mode mode)
	{
		if (mode == m_mode)
			return;
		stopWatching();
		m_mode = mode;
		startWatching();
	}
	void FileChangeWatcher::setPath(const std::string& path)
	{
		if (path == m_path)
			return;
		stopWatching();
		m_path = path;
		startWatching();
	}
	void FileChangeWatcher::setModeAndPath(Mode mode, const std::string& path)
	{
		stopWatching();
		m_mode = mode;
		m_path = path;
		startWatching();
	}
	bool FileChangeWatcher::hasChanged()
	{
		return m_fileChanged.load();
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
		if (m_mode == Mode::polling && m_pollingThread)
			m_pollingThread->stopTimer();
	}
	void FileChangeWatcher::unpause()
	{
		m_paused.store(false);
		if (m_mode == Mode::polling && m_pollingThread)
			m_pollingThread->startTimer();
	}
	bool FileChangeWatcher::isPaused() const
	{
		return m_paused.load();
	}

	void FileChangeWatcher::startWatching()
	{
		if (m_mode == Mode::polling)
		{
			if (!m_pollingThread)
			{
				m_md5.clear();
				m_fileChanged.store(false);
				m_pollingThread = new PollingTimerThread(100, [this]() {
					checkFile();
					});
				m_pollingThread->start();
			}

			if (m_paused.load())
				m_pollingThread->stopTimer();
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
			//m_timer.stop();
			if (m_pollingThread)
			{
				m_pollingThread->quit();
				m_pollingThread->wait();
				delete m_pollingThread;
				m_pollingThread = nullptr;
			}
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
	/*void FileChangeWatcher::onPollingTimerTimeout()
	{
		if (hasChanged())
		{
			onFileChangedInternalSlot(nullptr);
		}
	}*/

	void FileChangeWatcher::debug(const std::string& msg) const { m_logger.logDebug(msg); }
	void FileChangeWatcher::info(const std::string& msg) const { m_logger.info(msg); }
	void FileChangeWatcher::warning(const std::string& msg) const { m_logger.warning(msg); }
	void FileChangeWatcher::error(const std::string& msg) const { m_logger.error(msg); }

	bool FileChangeWatcher::fileChanged()
	{
		SQLW_FILE_WATCHER_PROFILING_FUNCTION(SQLW_COLOR_STAGE_2);
		std::filesystem::path file(m_path);

		if (!std::filesystem::exists(file)) {
			error("FileChangeWatcher: File: \"" + m_path + "\" does not exist!");
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
		const std::string title = ("FileChangeWatcher \"" + m_path + "\"");
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
			error("FileChangeWatcher: Starting directory monitoring: " + Utilities::getLastErrorString(GetLastError()));
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
						DWORD lastErr = GetLastError();
						error("FileChangeWatcher: FindNextChangeNotification. GetLastError() =  " + std::to_string(lastErr) + " : " + Utilities::getLastErrorString(lastErr));
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
					debug("FileChangeWatcher: File changed: " + m_path);
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
				DWORD lastErr = GetLastError();
				error("FileChangeWatcher: Waiting for file changes. GetLastError() =  " + std::to_string(lastErr) + " : " + Utilities::getLastErrorString(lastErr));
			}
#endif
		}
	exitThread:;

		FindCloseChangeNotification(m_eventHandle.load());
		m_eventHandle.store(nullptr);
	}
	void FileChangeWatcher::checkFile()
	{
		SQLW_FILE_WATCHER_PROFILING_FUNCTION(SQLW_COLOR_STAGE_2);
		if (m_paused.load())
			return;

		auto startTime = std::chrono::steady_clock::now();

		bool success;
		std::string md5 = calculateMD5Hash(success);
		if (!success)
		{
#ifdef SQLW_DEBUG
			error("FileChangeWatcher: Could not calculate the MD5 hash of the file: " + m_path);
#endif
			return;
		}
		if (md5 != m_md5 && m_md5 != "")
		{
			m_fileChanged.store(true);
			debug("FileChangeWatcher: File changed: " + m_path);
			emit onFileChangedInternal(nullptr);
		}
		auto endTime = std::chrono::steady_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
		m_lastCheckTimeMillis = static_cast<unsigned int>(duration.count());
		if (m_pollingThread)
		{
			int currentInterval = m_pollingThread->getTimerInterval();
			if (m_lastCheckTimeMillis > (unsigned)currentInterval)
			{
				info("FileChangeWatcher: Adjusting polling interval to " + std::to_string(m_lastCheckTimeMillis * 2) + " ms due to long check duration (" + std::to_string(m_lastCheckTimeMillis) + " ms)");
				m_pollingThread->setTimerInterval(m_lastCheckTimeMillis * 2);
			}
		}
		m_md5 = md5;
	}

	std::string FileChangeWatcher::calculateMD5Hash(bool& success)
	{
		SQLW_FILE_WATCHER_PROFILING_FUNCTION(SQLW_COLOR_STAGE_3);
		QFile file(m_path.c_str());
		if (!file.open(QIODevice::ReadOnly)) {
#ifdef SQLW_DEBUG
			error("FileChangeWatcher: Could not open file: " + m_path + " to calculate the MD5 hash");
#endif
			success = false;
			return "";
		}

		QCryptographicHash hash(QCryptographicHash::Md5);
		if (!hash.addData(&file)) {
#ifdef SQLW_DEBUG
			error("FileChangeWatcher: Could not read file: " + m_path + " to calculate the MD5 hash");
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
