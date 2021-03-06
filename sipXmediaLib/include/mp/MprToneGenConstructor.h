//  
// Copyright (C) 2006-2007 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
//
// Copyright (C) 2006-2007 SIPez LLC. 
// Licensed to SIPfoundry under a Contributor Agreement. 
//
// $$
///////////////////////////////////////////////////////////////////////////////

// Author: Dan Petrie <dpetrie AT SIPez DOT com>

#ifndef _MprToneGenConstructor_h_
#define _MprToneGenConstructor_h_

// SYSTEM INCLUDES
// APPLICATION INCLUDES
#include <mp/MpAudioResourceConstructor.h>
#include <mp/MprToneGen.h>

// DEFINES
// MACROS
// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STRUCTS
// TYPEDEFS
// FORWARD DECLARATIONS

/**
*  @brief MprToneGenConstructor is used to construct a ToneGen resource
*
*/
class MprToneGenConstructor : public MpAudioResourceConstructor
{
/* //////////////////////////// PUBLIC //////////////////////////////////// */
public:

/* ============================ CREATORS ================================== */

    /** Constructor
     */
    MprToneGenConstructor(const UtlString& locale = "")
    : MpAudioResourceConstructor(DEFAULT_TONE_GEN_RESOURCE_TYPE,
                                 0, 1, //minInputs, maxInputs,
                                 1, 1) //minOutputs, maxOutputs
    , mLocale(locale)
    {
    };

    /** Destructor
     */
    virtual ~MprToneGenConstructor(){};

/* ============================ MANIPULATORS ============================== */

    /// Create a new resource
    virtual OsStatus newResource(const UtlString& resourceName,
                                 int maxResourcesToCreate,
                                 int& numResourcesCreated,
                                 MpResource* resourceArray[])
    {
        assert(maxResourcesToCreate >= 1);
        numResourcesCreated = 1;
        resourceArray[0] = new MprToneGen(resourceName,
                                          mLocale);
        resourceArray[0]->disable();
        return(OS_SUCCESS);
    }

/* ============================ ACCESSORS ================================= */

/* ============================ INQUIRY =================================== */

/* //////////////////////////// PROTECTED ///////////////////////////////// */
protected:

/* //////////////////////////// PRIVATE /////////////////////////////////// */
private:
    UtlString mLocale;

    /** Disabled copy constructor
     */
    MprToneGenConstructor(const MprToneGenConstructor& rMprToneGenConstructor);


    /** Disable assignment operator
     */
    MprToneGenConstructor& operator=(const MprToneGenConstructor& rhs);

};

/* ============================ INLINE METHODS ============================ */

#endif  // _MprToneGenConstructor_h_
