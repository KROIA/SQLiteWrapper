// @file SQLiteWrapper_base.h
// @brief Internal base header included by every library source file.
//
// Pulls in the global export/import macros, debug/profiling utilities,
// and library metadata. Include this in your own library headers instead
// of including the individual headers separately.
#pragma once

/// USER_SECTION_START 1

/// USER_SECTION_END

#include "SQLiteWrapper_global.h"
#include "SQLiteWrapper_debug.h"
#include "SQLiteWrapper_info.h"

/// USER_SECTION_START 2
//#define SQLITE_API SQLITE_WRAPPER_API
//#include "sqlite3ext.h"
#include "sqlite/sqlite3.h"

/// USER_SECTION_END