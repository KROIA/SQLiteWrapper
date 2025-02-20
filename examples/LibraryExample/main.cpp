#ifdef QT_ENABLED
#include <QApplication>
#endif
#include <iostream>
#include "SQLiteWrapper.h"


#ifdef QT_WIDGETS_ENABLED
#include <QWidget>
#endif

void concurencyTest();

int main(int argc, char* argv[])
{
#ifdef QT_WIDGETS_ENABLED
	QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
	QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif
#ifdef QT_ENABLED
	QApplication app(argc, argv);
#endif
	SQLiteWrapper::Profiler::start();
	SQLiteWrapper::LibraryInfo::printInfo();
#ifdef QT_WIDGETS_ENABLED
	QWidget* widget = SQLiteWrapper::LibraryInfo::createInfoWidget();
	if (widget)
		widget->show();
#endif
	Log::UI::NativeConsoleView consoleView;
    consoleView.show();

    //concurencyTest();

	//SQLiteWrapper::SQLite db("T:/Alex Krieg/example.db");
	SQLiteWrapper::SQLite db("example.db");
    if (db.open())
    {
        db.dropTable("Users");
        db.beginTransaction();
		db.execute("CREATE TABLE IF NOT EXISTS Users (ID INTEGER PRIMARY KEY AUTOINCREMENT, Name TEXT NOT NULL, Age INTEGER, Test TEXT);");
		db.execute("INSERT INTO Users (Name, Age) VALUES ('Alice', '8b');");
		db.execute("INSERT INTO Users (Name, Age) VALUES ('Bob', 40);");
		db.execute("INSERT INTO Users (Name, Age) VALUES ('Charlie', 50);");
		//db.execute("INSERT INTO Users (Name, Test) VALUES ('David', 60);");
        db.insertRow("Users", {{"Name", "'David'"}, {"Age","60"}});
		//db.execute("UPDATE Users SET Age = 15 WHERE ID = 3");
        db.changeRow("Users", { {"Age","'15a'"} },"ID = 2");
        db.commitTransaction();

        std::vector<std::vector<std::string>> result = db.fetchAll("SELECT ID, Name, Age, Test FROM Users;");
        std::cout << "Table:\n";
		for (const auto& row : result)
		{
			for (const auto& column : row)
			{
				std::cout << column << " ";
			}
			std::cout << std::endl;
		}
        db.close();
    }



    /*
    sqlite3* db;
    char* errMsg = nullptr;

    // Open a database (it will be created if it doesn't exist)
    if (sqlite3_open("example.db", &db) != SQLITE_OK) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }
    std::cout << "Database opened successfully.\n";

    // Create a table
    const char* createTableSQL = "CREATE TABLE IF NOT EXISTS Users ("
        "ID INTEGER PRIMARY KEY AUTOINCREMENT, "
        "Name TEXT NOT NULL, "
        "Age INTEGER);";

    if (sqlite3_exec(db, createTableSQL, 0, 0, &errMsg) != SQLITE_OK) {
        std::cerr << "logError creating table: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return 1;
    }
    std::cout << "Table created successfully.\n";

    // Insert data
    const char* insertSQL = "INSERT INTO Users (Name, Age) VALUES ('Alice', 30);";
    if (sqlite3_exec(db, insertSQL, 0, 0, &errMsg) != SQLITE_OK) {
        std::cerr << "logError inserting data: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    }
    else {
        std::cout << "Data inserted successfully.\n";
    }

    // Query data
    const char* selectSQL = "SELECT ID, Name, Age FROM Users;";
    auto callback = [](void*, int argc, char** argv, char** colNames) -> int {
        for (int i = 0; i < argc; i++) {
            std::cout << colNames[i] << ": " << (argv[i] ? argv[i] : "NULL") << "  ";
        }
        std::cout << std::endl;
        return 0;
        };

    if (sqlite3_exec(db, selectSQL, callback, 0, &errMsg) != SQLITE_OK) {
        std::cerr << "logError querying data: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    }
    else {
        std::cout << "Data queried successfully.\n";
    }

    // Close the database
    sqlite3_close(db);
    std::cout << "Database closed.\n";
    */
	int ret = 0;
#ifdef QT_ENABLED
	ret = app.exec();
#endif
	SQLiteWrapper::Profiler::stop((std::string(SQLiteWrapper::LibraryInfo::name) + ".prof").c_str());
	return ret;
}

void concurencyTest()
{
    SQLiteWrapper::SQLite db1("example.db");
    SQLiteWrapper::SQLite db2("example.db");
    if (db1.open() && db2.open())
    {
        db1.beginTransaction();
        db1.execute("CREATE TABLE IF NOT EXISTS Users (ID INTEGER PRIMARY KEY AUTOINCREMENT, Name TEXT NOT NULL, Age INTEGER, Test TEXT);");
        db1.execute("INSERT INTO Users (Name, Age) VALUES ('Alice', '8');");
        db1.execute("INSERT INTO Users (Name, Age) VALUES ('Bob', 40);");

        db2.beginTransaction();
        db2.execute("CREATE TABLE IF NOT EXISTS Users (ID INTEGER PRIMARY KEY AUTOINCREMENT, Name TEXT NOT NULL, Age INTEGER, Test TEXT);");
        db2.execute("INSERT INTO Users (Name, Age) VALUES ('Alice', '80');");
        db2.execute("INSERT INTO Users (Name, Age) VALUES ('Bob', 400);");
        db2.execute("INSERT INTO Users (Name, Age) VALUES ('Charlie', 500);");
        db2.insertRow("Users", { {"Name", "'David'"}, {"Age","600"} });
        db2.changeRow("Users", { {"Age","150"} }, "ID = 2");

        db1.execute("INSERT INTO Users (Name, Age) VALUES ('Charlie', 50);");
        db1.insertRow("Users", { {"Name", "'David'"}, {"Age","60"} });
        db1.changeRow("Users", { {"Age","15"} }, "ID = 2");
        db1.commitTransaction();

        db2.commitTransaction();

        std::vector<std::vector<std::string>> result = db1.fetchAll("SELECT ID, Name, Age, Test FROM Users;");
        std::cout << "Table:\n";
        for (const auto& row : result)
        {
            for (const auto& column : row)
            {
                std::cout << column << " ";
            }
            std::cout << std::endl;
        }
        db1.close();
        db2.close();
    }
}