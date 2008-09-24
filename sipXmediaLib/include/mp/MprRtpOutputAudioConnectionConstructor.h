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

#ifndef _MprRtpOutputAudioConnectionConstructor_h_
#define _MprRtpOutputAudioConnectionConstructor_h_

// SYSTEM INCLUDES
// APPLICATION INCLUDES
#include <mp/MpAudioResourceConstructor.h>
#include <mp/MpRtpOutputAudioConnection.h>
#include <mp/MprToSpkr.h>

// DEFINES
// MACROS
// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STRUCTS
// TYPEDEFS
// FORWARD DECLARATIONS

/**
*  @brief MprRtpOutputAudioConnectionConstructor is used to construct a ToOutputDevice resource
*
*/
class MprRtpOutputAudioConnectionConstructor : public MpAudioResourceConstructor
{
/* //////////////////////////// PUBLIC //////////////////////////////////// */
public:

/* ============================ CREATORS ================================== */

    /** Constructor
     */
    MprRtpOutputAudioConnectionConstructor()
    : MpAudioResourceConstructor(DEFAULT_RTP_OUTPUT_RESOURCE_TYPE,
                                 0, 1, //minInputs, maxInputs,
                                 0, 0) //minOutputs, maxOutputs
    {
    };

    /** Destructor
     */
    virtual ~MprRtpOutputAudioConnectionConstructor(){};

/* ============================ MANIPULATORS ============================== */

    /// Create a new resource
    virtual OsStatus newResource(const UtlString& resourceName,
                                 int maxResourcesToCreate,
                                 int& numResourcesCreated,
                                 MpResource* resourceArray[])
    {
        assert(maxResourcesToCreate >= 1);
        numResourcesCreated = 1;
        resourceArray[0] = new MpRtpOutputAudioConnection(resourceName, 999);
        resourceArray[0]->enable();
        return(OS_SUCCESS);
    }

/* ============================ ACCESSORS ================================= */

/* ============================ INQUIRY =================================== */

/* //////////////////////////// PROTECTED ///////////////////////////////// */
protected:

/* //////////////////////////// PRIVATE /////////////////////////////////// */
private:

    /** Disabled copy constructor
     */
    MprRtpOutputAudioConnectionConstructor(const MprRtpOutputAudioConnectionConstructor& rMprRtpOutputAudioConnectionConstructor);


    /** Disable assignment operator
     */
    MprRtpOutputAudioConnectionConstructor& operator=(const MprRtpOutputAudioConnectionConstructor& rhs);

};

/* ============================ INLINE METHODS ============================ */

#endif  // _MprRtpOutputAudioConnectionConstructor_h_
