//  
// Copyright (C) 2007 SIPez LLC. 
// Licensed to SIPfoundry under a Contributor Agreement. 
//
// Copyright (C) 2007 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
//
// $$
///////////////////////////////////////////////////////////////////////////////

// Author: Keith Kyzivat <kkyzivat AT SIPez DOT com>


// SYSTEM INCLUDES
#include <Windows.h>

// APPLICATION INCLUDES
#include "mp/MpInputDeviceDriver.h"

// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STATIC VARIABLE INITIALIZATIONS


/* //////////////////////////// PUBLIC //////////////////////////////////// */

/* ============================ CREATORS ================================== */
// Default constructor
MpInputDeviceDriver::MpInputDeviceDriver(const UtlString& name, 
                                         MpInputDeviceManager& deviceManager)
    : UtlString(name), mpInputDeviceManager(&deviceManager),
      mIsEnabled(false), mSamplesPerFrame(0),
      mSamplesPerSec(0), mFrameTime(0), mDeviceId(-1)
{
}

/* ============================ MANIPULATORS ============================== */
OsStatus MpInputDeviceDriver::setDeviceId(int deviceId) 
{ 
    OsStatus status = OS_BUSY;
    if(!isEnabled())
    {
        mDeviceId = deviceId; 
        status = OS_SUCCESS;
    }
    return status;
}

OsStatus MpInputDeviceDriver::clearDeviceId()
{
    OsStatus status = OS_BUSY;
    if(!isEnabled())
    {
        mDeviceId = 0;
        status = OS_SUCCESS;
    }
    return status;
}

OsStatus MpInputDeviceDriver::enableDevice(int samplesPerFrame, 
                                           int samplesPerSec, 
                                           int currentFrameTime)
{
    mSamplesPerFrame = samplesPerFrame;
    mSamplesPerSec = samplesPerSec;
    mFrameTime = currentFrameTime;
    return OS_SUCCESS;
}

OsStatus MpInputDeviceDriver::disableDevice()
{
    mSamplesPerFrame = 0;
    mSamplesPerSec = 0;
    mFrameTime = 0;
    return OS_SUCCESS;
}

/* ============================ ACCESSORS ================================= */
/* ============================ INQUIRY =================================== */
/* //////////////////////////// PROTECTED ///////////////////////////////// */
/* //////////////////////////// PRIVATE /////////////////////////////////// */
