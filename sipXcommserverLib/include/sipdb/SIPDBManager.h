// 
// 
// Copyright (C) 2004 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
// 
// Copyright (C) 2004 Pingtel Corp.
// Licensed to SIPfoundry under a Contributor Agreement.
// 
// $$
//////////////////////////////////////////////////////////////////////////////
#ifndef SIPDBMANAGER_H
#define SIPDBMANAGER_H

// SYSTEM INCLUDES
//#include <...>

// APPLICATION INCLUDES
#include "os/OsDefs.h"
#include "os/OsMutex.h"
#include "os/OsFS.h"
#include "fastdb/fastdb.h"

// DEFINES
#define SPECIAL_IMDB_NULL_VALUE "%"

// MACROS
// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
#define SIPX_DB_CFG_PATH         "SIPX_DB_CFG_PATH"  
#define SIPX_DB_VAR_PATH         "SIPX_DB_VAR_PATH"  

// STRUCTS
// TYPEDEFS
// FORWARD DECLARATIONS
class TiXmlNode;
class ResultSet;

// TableInfo is used to keep a track of
// attached processes to the DB, first one must
// provision the DB from the XML if it exists
// and the last one out must serialize the DB
// The unload is either done cleanly by the
// application or through the signal handler
class TableInfo
{
public:
    const char* tablename;      // the name of the database
    int4        pid;            // the process ID
    int4        loadchecksum;   // a checksum only used during load (to detect change)
    bool        changed;        // flag used by other tables to determine if changed
    TYPE_DESCRIPTOR(
      (KEY(tablename, INDEXED),
       FIELD(pid),
       FIELD(loadchecksum),
       FIELD(changed)));
};

/**
 * Wrapper to the fast DB singleton.  This singleton ensures
 * that there is only one named instance of fastDB running per
 * process.  This instance will have all the tables stored in it
 *
 * @author John P. Coffey
 * @version 1.0
 */
class SIPDBManager
{
public:
    /**
     * Singleton Accessor
     *
     * @return
     */
    static SIPDBManager* getInstance();

    /**
     * Virtual Destructor
     */
    virtual ~SIPDBManager();

    /** dumps the IMDB meta table info */
    void getAllTableProcesses ( ResultSet& rResultSet ) const;

    /** Number of Processes attached to tables (Processes * Tables) */
    OsStatus getProcessCount ( int& rProcessCount ) const;

    /**
     * Registerd the database and pid, opens the DB if necessary
     * and returns it
     */
    dbDatabase* getDatabase ( const UtlString& tablename ) const;

    /**
     * unregisters the database from the IMDB and calls close to
     * decrement the user count etc.
     */
    void removeDatabase ( const UtlString& tablename ) const;

    /** counts the number of processes attached to the DB */
    int getNumDatabaseProcesses ( const UtlString& tablename ) const;

    OsStatus getDatabaseInfo (UtlString& rDatabaseInfo ) const;

    /** 
     * Gets the location of the working files directory (var).
     * If the SIPX_DB_VAR_PATH environment variable is defined and exists,
     * that directory is used, otherwise, the current working directory is
     * used. 
     */
    const UtlString& getWorkingDirectory () const;
    
    /** 
     * Gets the location of the config files directory.
     * If the SIPX_DB_CFG_PATH environment variable is defined and exists,
     * that directory is used, otherwise, the current working directory is
     * used. 
     */
    const UtlString& getConfigDirectory () const;

    /** After loading an IMDB update its checksum information */
    void updateDatabaseInfo ( const UtlString& tablename, const int& checksum ) const;

    /** Sets a change flag for the table indicating that there was an insert/update to it */
    void setDatabaseChangedFlag ( const UtlString& tablename, bool changed ) const;

    /** determines whether the database has changed or not */
    bool getDatabaseChangedFlag ( const UtlString& tablename ) const;

    /** method to ping the data store and sleeps for timeoutSecs in the transaction */
    OsStatus pingDatabase ( 
        const int& rTransactionLockSecs = 0, 
        const UtlBoolean& rTestWriteLock = FALSE ) const;

    /** Helper Method for all IMDB tables */
    static void getFieldValue ( 
        const unsigned char* base,
        const dbFieldDescriptor* fd, 
        UtlString& textValue);

    /** Helper Method for all IMDB tables */
    static OsStatus getAttributeValue ( 
        const TiXmlNode& node,
        const UtlString& key,
        UtlString& value );

    /** preload all database tables **/
    OsStatus preloadAllDatabase() const;

    /** release all database tables **/
    OsStatus releaseAllDatabase() const;

    /** Gets the absolute or relative path prefix for dynamic data files */
    static OsPath getVarPath() ;

    /** Gets the absolute or relative path prefix for static data files */
    static OsPath getCfgPath() ;

protected:
    /**
     * Registerd the database and pid, opens the DB if necessary
     * and returns it
     */
    dbDatabase* openDatabase () const;

    // Protected Constructor
    SIPDBManager();

    /// Override the default fastdb tmp dir if the env var SIPX_DB_VAR_PATH is set
    void setFastdbTempDir();

    // Singleton instance
    static SIPDBManager* spInstance;

    // Exclusive binary lock
    static OsMutex sLockMutex;

    // Fast DB instance
    static dbDatabase* spFastDB;

    // the working directory for all database instances
    // the XML files are located here (var)
    UtlString m_absWorkingDirectory;

    // The configuration file directory (etc)
    UtlString m_absConfigDirectory;

    // Fastdb tmp dir path
    UtlString m_FastDbTmpDirPath;
};

#endif //SIPDBMANAGER_H