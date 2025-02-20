#pragma once

#include "SQLiteWrapper_base.h"
#include <string>
#include <QObject>
#include "sqlite3.h"
#include "FileChangeWatcher.h"
#include "LockFile.h"

namespace SQLiteWrapper
{
    /**
     * @class SQLite
     * @brief A wrapper for SQLite database interactions.
     *
     * This class provides an abstraction over SQLite to simplify working with databases,
     * including operations like executing queries, managing transactions, and checking table existence.
     * It also handles logError logging and provides basic database connectivity functionalities.
     *
     * @note The SQLite library must be linked in your project to use this class.
     */
    class SQLITE_WRAPPER_EXPORT SQLite : public QObject
    {
		Q_OBJECT
    public:
        /**
         * @brief Constructor. It does not open the database.
         *
         * @param dbPath The path to the SQLite database file.
         */
        SQLite(const std::string& dbPath);

        /**
         * @brief Destructor that ensures the database is closed.
         */
        ~SQLite();

        /**
         * @brief Opens the SQLite database connection.
         *
         * @return True if the connection was successfully opened, false otherwise.
         */
        bool open();

        /**
         * @brief Closes the SQLite database connection.
         *
         * @return True if the connection was successfully closed, false otherwise.
         */
        bool close();

        /**
         * @brief Checks if the database connection is currently open.
         *
         * @return True if the database is open, false otherwise.
         */
        bool isOpen() const;

        /**
         * @brief Executes a simple SQL query without parameters.
         *
         * @param query The SQL query to execute.
         *
         * @return True if the query was successfully executed, false otherwise.
         */
        bool execute(const std::string& query);

        /**
         * @brief Executes an SQL query with parameters.
         *
         * @param query The SQL query to execute.
         * @param params A vector of parameters to bind to the query.
         *
         * @return True if the query was successfully executed, false otherwise.
         */
        bool executeWithParams(const std::string& query, const std::vector<std::string>& params);

        /**
		 * @brief Inserts a row into a table.
         * 
		 * @param tableName The name of the table to insert into.
		 * @param params A vector of pairs where each pair is a column name and its value.
         * 
		 * @return True if the row was successfully inserted, false otherwise.
         * 
		 * @example
		 * db.insertRow("Users", {{"Name", "'David'"}, {"Age","60"}}); 
         */
        bool insertRow(const std::string& tableName, const std::vector<std::pair<std::string, std::string>>& params);


        /**
		 * @brief Changes a row in a table.
         * 
		 * @param tableName The name of the table to change.
		 * @param params A vector of pairs where each pair is a column name and its value.
         * 
		 * @return True if the row was successfully changed, false otherwise.
         * 
		 * @example
		 * db.changeRow("Users", {{"Age","15"}},"ID = 2");
         */
		bool changeRow(const std::string& tableName, const std::vector<std::pair<std::string, std::string>>& params, const std::string& condition);

        /**
		 * @brief Removes a table
         * 
		 * @param tableName The name of the table to remove.
         * 
		 * @return True if the table was successfully removed, false otherwise.
         */
		bool dropTable(const std::string& tableName);

        /**
         * @brief Fetches all results from a SELECT query.
         *
         * @param query The SQL SELECT query.
         *
         * @return A 2D vector where each row is a vector of column values from the query result.
         */
        std::vector<std::vector<std::string>> fetchAll(const std::string& query);

        /**
         * @brief Begins a database transaction.
         *
         * @return True if the transaction was successfully started, false otherwise.
         */
        bool beginTransaction();

        /**
         * @brief Commits the current transaction.
         *
         * @return True if the transaction was successfully committed, false otherwise.
         */
        bool commitTransaction();

        /**
         * @brief Rolls back the current transaction.
         *
         * @return True if the transaction was successfully rolled back, false otherwise.
         */
        bool rollbackTransaction();

        /**
         * @brief Checks if a table exists in the database.
         *
         * @param tableName The name of the table to check.
         *
         * @return True if the table exists, false otherwise.
         */
        bool tableExists(const std::string& tableName);

        /**
         * @brief Gets the last insert row ID after an insert operation.
         *
         * @return The last inserted row ID.
         */
        sqlite3_int64 getLastInsertRowId();

        /**
         * @brief Gets the underlying SQLite database pointer.
         *
         * @return The SQLite database pointer.
         */
        sqlite3* getDB() const { return m_db; }

        /**
         * @brief Gets the path of the SQLite database.
         *
         * @return The path to the database.
         */
        const std::string& getDBPath() const { return m_dbPath; }

    signals:
        void onDBChanged();

    private slots:
        /**
         * @brief Slot for handling database file changes.
         *
         * @param path The path to the changed file.
         */
        void onDBFileChanged(const std::string& path);

    private:
        /**
         * @brief Handles SQLite logError codes and logs any issues.
         *
         * @param rc The return code from the SQLite function.
         *
         * @return The logError code.
         */
        int handleSQLiteError(int rc);

	


        const std::string m_dbPath; ///< Path to the SQLite database file.
        sqlite3* m_db; ///< SQLite database connection.
        Log::LogObject m_logger; ///< Logger for logError handling.
		FileChangeWatcher m_watcher; ///< File change watcher for database file changes.
    };

}