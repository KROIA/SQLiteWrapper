#include "SQLite.h"

namespace SQLiteWrapper
{

	SQLite::SQLite(const std::string& dbPath)
		: m_dbPath(dbPath)
		, m_db(nullptr)
		, m_logger("SQLite:" + dbPath)
	{
	}

	SQLite::~SQLite()
	{
		if (m_db)
			close();
	}

	bool SQLite::open()
	{
		if (m_db)
		{
			m_logger.logWarning("Database is already open");
			return true;
		}
		if (handleSQLiteError(sqlite3_open(m_dbPath.c_str(), &m_db)) != SQLITE_OK)
		{
			m_logger.logError("Failed to open database: " + m_dbPath);
			return false;
		}
		m_logger.logInfo("Database opened successfully");
		return true;
	}

	bool SQLite::close()
	{
		if (m_db)
		{
			if (handleSQLiteError(sqlite3_close(m_db)) != SQLITE_OK)
			{
				m_logger.logError("Failed to close database");
				return false;
			}
			m_db = nullptr;
			m_logger.logInfo("Database closed");
			return true;
		}
		m_logger.logWarning("Database is already closed");
		return true;
	}

	bool SQLite::execute(const std::string& query)
	{
		char* errMsg = nullptr;
		int rc = sqlite3_exec(m_db, query.c_str(), nullptr, nullptr, &errMsg);
		if (rc != SQLITE_OK)
		{
			m_logger.logError("Failed to execute query: " + query + " logError: " + errMsg);
			sqlite3_free(errMsg);
			return false;
		}
		return true;
	}

	bool SQLite::executeWithParams(const std::string& query, const std::vector<std::string>& params)
	{
		sqlite3_stmt* stmt = nullptr;
		if (handleSQLiteError(sqlite3_prepare_v2(m_db, query.c_str(), -1, &stmt, nullptr)) != SQLITE_OK)
		{
			return false;
		}

		for (size_t i = 0; i < params.size(); ++i)
		{
			if (handleSQLiteError(sqlite3_bind_text(stmt, static_cast<int>(i) + 1, params[i].c_str(), -1, SQLITE_TRANSIENT)) != SQLITE_OK)
			{
				sqlite3_finalize(stmt);
				return false;
			}
		}

		int rc = sqlite3_step(stmt);
		sqlite3_finalize(stmt);
		return (rc == SQLITE_DONE);
	}

	bool SQLite::insertRow(const std::string& tableName, const std::vector<std::pair<std::string, std::string>>& params)
	{
		std::string query = "INSERT INTO " + tableName + " (";
		std::string values = "VALUES (";
		for (size_t i = 0; i < params.size(); ++i)
		{
			query += params[i].first;
			values += params[i].second;
			if (i < params.size() - 1)
			{
				query += ", ";
				values += ", ";
			}
		}
		query += ") " + values + ");";
		return execute(query);
	}

	bool SQLite::changeRow(const std::string& tableName, const std::vector<std::pair<std::string, std::string>>& params, const std::string& condition)
	{
		std::string query = "UPDATE " + tableName + " SET ";
		for (size_t i = 0; i < params.size(); ++i)
		{
			query += params[i].first + " = " + params[i].second;
			if (i < params.size() - 1)
			{
				query += ", ";
			}
		}
		query += " WHERE " + condition + ";";
		return execute(query);
	}

	bool SQLite::dropTable(const std::string& tableName)
	{
		return execute("DROP TABLE " + tableName + ";");
	}

	std::vector<std::vector<std::string>> SQLite::fetchAll(const std::string& query)
	{
		std::vector<std::vector<std::string>> results;
		sqlite3_stmt* stmt = nullptr;
		if (handleSQLiteError(sqlite3_prepare_v2(m_db, query.c_str(), -1, &stmt, nullptr)) != SQLITE_OK)
		{
			return results;
		}

		while (sqlite3_step(stmt) == SQLITE_ROW)
		{
			int columnCount = sqlite3_column_count(stmt);
			std::vector<std::string> row;
			for (int i = 0; i < columnCount; ++i)
			{
				const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
				row.push_back(text ? text : "");
			}
			results.push_back(row);
		}
		sqlite3_finalize(stmt);
		return results;
	}

	bool SQLite::beginTransaction()
	{
		return execute("BEGIN TRANSACTION;");
	}

	bool SQLite::commitTransaction()
	{
		return execute("COMMIT;");
	}

	bool SQLite::rollbackTransaction()
	{
		return execute("ROLLBACK;");
	}

	bool SQLite::tableExists(const std::string& tableName)
	{
		std::string query = "SELECT name FROM sqlite_master WHERE type='table' AND name='" + tableName + "';";
		auto result = fetchAll(query);
		return !result.empty();
	}

	sqlite3_int64 SQLite::getLastInsertRowId()
	{
		return sqlite3_last_insert_rowid(m_db);
	}

	bool SQLite::isOpen() const
	{
		return m_db != nullptr;
	}

	int SQLite::handleSQLiteError(int rc)
	{
		if (rc != SQLITE_OK && m_db)
		{
			m_logger.logError("SQLite logError: " + std::string(sqlite3_errstr(rc)));
		}
		return rc;
	}


}