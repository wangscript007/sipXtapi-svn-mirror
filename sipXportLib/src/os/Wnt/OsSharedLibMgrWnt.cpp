//
// Copyright (C) 2004-2006 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
//
// Copyright (C) 2004-2006 Pingtel Corp.  All rights reserved.
// Licensed to SIPfoundry under a Contributor Agreement.
//
// $$
///////////////////////////////////////////////////////////////////////////////


// SYSTEM INCLUDES
#include <assert.h>

// APPLICATION INCLUDES
#include <os/Wnt/OsSharedLibMgrWnt.h>
#include <os/OsSysLog.h>
#include <utl/UtlString.h>

// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS

// Private container class for collectable handles to shared libs
class OsSharedLibHandleWnt : public UtlString
{
public:

    OsSharedLibHandleWnt(const char* libName, HMODULE libHandle);
    HMODULE mLibHandle;
};

OsSharedLibHandleWnt::OsSharedLibHandleWnt(const char* libName, HMODULE libHandle) :
UtlString(libName ? libName : "")
{
    mLibHandle = libHandle;
}

// STATIC VARIABLE INITIALIZATIONS

    
/* //////////////////////////// PUBLIC //////////////////////////////////// */


/* ============================ CREATORS ================================== */

// Constructor
OsSharedLibMgrWnt::OsSharedLibMgrWnt()
{
}

// Copy constructor
OsSharedLibMgrWnt::OsSharedLibMgrWnt(const OsSharedLibMgrWnt& rOsSharedLibMgrWnt)
{
}

// Destructor
OsSharedLibMgrWnt::~OsSharedLibMgrWnt()
{
}

/* ============================ MANIPULATORS ============================== */

//: Loads the given shared library
//!param: libName - name of library, may include absolute or relative path
OsStatus OsSharedLibMgrWnt::loadSharedLib(const char* libName)
{
    OsStatus status = OS_INVALID;

    // Check if we aready have a handle for this lib
    UtlString collectableName(libName ? libName : "");
    sLock.acquire();
    OsSharedLibHandleWnt* collectableLibHandle = 
        (OsSharedLibHandleWnt*) mLibraryHandles.find(&collectableName);
    sLock.release();

    // We do not already have a handle for this lib
    if(!collectableLibHandle)
    {
        // Prevent dialog from freezing the process
        unsigned int previousErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS);

        // Load the shared library
        HMODULE libHandle = NULL;
        
        if(libName)
        {
            libHandle = LoadLibraryEx(libName, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
        }
        else
        {
            libHandle = GetModuleHandle(NULL);
        }
        if (!libHandle)
        {
            osPrintf("failed to shared lib load specified path\nTrying wider search path\n");
            libHandle = LoadLibraryEx(libName, NULL, 0);
        }
        if (!libHandle)
        {
            int errorCode = GetLastError();
            osPrintf("Failed to load shared library: %s error: %d\n",
                libName ? libName : "(null)", errorCode);
            status = OS_NOT_FOUND;
        }
        else
        {
            osPrintf("Loaded shared lib %s\n", libName ? libName : "(null)");
            OsSharedLibHandleWnt* collectableHandle = 
                new OsSharedLibHandleWnt(libName, libHandle);

            sLock.acquire();
            mLibraryHandles.insert(collectableHandle);
            sLock.release();

            status = OS_SUCCESS;
        }

        // Reset the error mode
        SetErrorMode(previousErrorMode);
    }
    else
    {
        status = OS_SUCCESS;
    }

    // Get a handle to the main program
    //libHandle = GetModuleHandle(NULL);
    //if(libHandle)
    //{
    //    osPrintf("got a handle to this\n");
    //}

    return(status);
}

//: Gets the address of a symbol in the shared lib
//!param: (in) libName - name of library, may include absolute or relative path
//!param: (in) symbolName - name of the variable or function exported in the shared lib
//!param: (out) symbolAddress - the address of the function or variable
OsStatus OsSharedLibMgrWnt::getSharedLibSymbol(const char* libName, 
                              const char* symbolName,
                              void*& symbolAddress)
{
    OsStatus status = OS_INVALID;
    UtlString collectableName(libName ? libName : "");
    sLock.acquire();
    OsSharedLibHandleWnt* collectableLibHandle = 
        (OsSharedLibHandleWnt*) mLibraryHandles.find(&collectableName);

    if(!collectableLibHandle)
    {
        sLock.release();
        loadSharedLib(libName);
        sLock.acquire();
        collectableLibHandle = 
            (OsSharedLibHandleWnt*) mLibraryHandles.find(&collectableName);
    }

    if(collectableLibHandle)
    {
        // Get a named symbol from the shared library
        symbolAddress = GetProcAddress(collectableLibHandle->mLibHandle, 
                                       symbolName);

        if (!symbolAddress) 
        {
            int errorCode = GetLastError();
            OsSysLog::add(FAC_KERNEL, PRI_DEBUG,
                "Failed to find symbol: %s in shared lib: %s error: %d",
                symbolName, libName ? libName : "(null)", errorCode);

            LPVOID lpMsgBuf;
            FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | 
                FORMAT_MESSAGE_FROM_SYSTEM | 
                FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,
                errorCode,
                0, // Default language
                (LPTSTR) &lpMsgBuf,
                0,
                NULL 
            );
            osPrintf("%s\n", lpMsgBuf);
            LocalFree(lpMsgBuf);

            if(errorCode = 127) status = OS_NOT_FOUND;
            else status = OS_UNSPECIFIED;
        }
        else
        {
            osPrintf("Found symbol: %s in shared lib: %s\n",
                symbolName, libName ? libName : "(null)");
            status = OS_SUCCESS;
        }

    }

    sLock.release();

    return(status);
}

OsStatus OsSharedLibMgrWnt::unloadSharedLib(const char* libName)
{
   OsStatus status = OS_INVALID;
   BOOL res;
   // Check if we already have a handle for this lib
   UtlString collectableName(libName ? libName : "");
   sLock.acquire();
   OsSharedLibHandleWnt* collectableLibHandle = 
      (OsSharedLibHandleWnt*) mLibraryHandles.find(&collectableName);

   // We do not already have a handle for this lib
   if(!collectableLibHandle)
   {
      sLock.release();
      return status;
   }

   res = FreeLibrary(collectableLibHandle->mLibHandle);
   if (res) {
      mLibraryHandles.remove(&collectableName);
      status = OS_SUCCESS;
   } else {
      status = OS_FAILED;
   }

   sLock.release();
   return(status);
}

/* ============================ ACCESSORS ================================= */

/* ============================ INQUIRY =================================== */

/* //////////////////////////// PROTECTED ///////////////////////////////// */

/* //////////////////////////// PRIVATE /////////////////////////////////// */

/* ============================ FUNCTIONS ================================= */


