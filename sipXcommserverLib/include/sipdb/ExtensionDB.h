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
#ifndef EXTENSIONDB_H
#define EXTENSIONDB_H

// SYSTEM INCLUDES


// APPLICATION INCLUDES
#include "os/OsMutex.h"

// DEFINES
// MACROS
// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STRUCTS
// TYPEDEFS
// FORWARD DECLARATIONS
class Url;
class ExtensionCursor;
class dbDatabase;
class dbFieldDescriptor;
class UtlHashMap;
class TiXmlNode;
class ResultSet;

/**
 * This class implements the Extension database abstract class
 *
 * @author John P. Coffey
 * @version 1.0
 */
class ExtensionDB
{
public:
    /**
     * Singleton Accessor
     *
     * @return
     */
    static ExtensionDB* getInstance( const UtlString& name = "extension" );

    /// releaseInstance - cleans up the singleton (for use at exit)
    static void releaseInstance();

    // Domain Serialization/Deserialization
    OsStatus store();

    // insert or update a row in the Extensiones database.
    UtlBoolean insertRow (
        const Url& uri,
        const UtlString& extension );

    // Delete methods
    UtlBoolean removeRow (const Url& uri );

    // Flushes the entire DB
    void removeAllRows ();

    // utility method for dumping all rows
    void getAllRows(ResultSet& rResultSet) const;

    // Query interface to return single extension associated with uri
    UtlBoolean getExtension (
        const Url& uri,
        UtlString& rExtension ) const;

    // Query interface to return Extensiones associated with a sipIdentity
    UtlBoolean getUri (
        const UtlString& extension,
        Url& rUri ) const;

protected:

    // implicit loader
    OsStatus load();

    // Singleton Constructor is private
    ExtensionDB ( const UtlString& name );

    // One step closer to common load/store code
    UtlBoolean insertRow ( const UtlHashMap& nvPairs );

    // There is only one singleton in this design
    static ExtensionDB* spInstance;

    // Singleton and Serialization mutex
    static OsMutex sLockMutex;

    // ResultSet column Keys
    static UtlString gUriKey;
    static UtlString gExtensionKey;

    // Fast DB instance
    dbDatabase* m_pFastDB;

    // the persistent filename for loading/saving
    UtlString mDatabaseName;

private:
    /**
     * Virtual Destructor
     */
    virtual ~ExtensionDB();

};

#endif //EXTENSIONDB_H
