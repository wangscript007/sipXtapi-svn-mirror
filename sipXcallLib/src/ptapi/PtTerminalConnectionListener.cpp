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
#include "ptapi/PtTerminalConnectionListener.h"

// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STATIC VARIABLE INITIALIZATIONS

int FORCE_REFERENCE_PtTerminalConnectionListener = 0 ;

/* //////////////////////////// PUBLIC //////////////////////////////////// */

/* ============================ CREATORS ================================== */

// Constructor
PtTerminalConnectionListener::PtTerminalConnectionListener(PtEventMask* mask) :
PtConnectionListener(mask)
{
}

// Copy constructor
PtTerminalConnectionListener::PtTerminalConnectionListener(const PtTerminalConnectionListener& rPtTerminalConnectionListener)
{
}

// Destructor
PtTerminalConnectionListener::~PtTerminalConnectionListener()
{
}

/* ============================ MANIPULATORS ============================== */

// Assignment operator
PtTerminalConnectionListener& 
PtTerminalConnectionListener::operator=(const PtTerminalConnectionListener& rhs)
{
   if (this == &rhs)            // handle the assignment to self case
      return *this;

   return *this;
}

void PtTerminalConnectionListener::terminalConnectionCreated(const PtTerminalConnectionEvent& rEvent)
{
}

void PtTerminalConnectionListener::terminalConnectionIdle(const PtTerminalConnectionEvent& rEvent)
{
}

void PtTerminalConnectionListener::terminalConnectionRinging(const PtTerminalConnectionEvent& rEvent)
{
}

void PtTerminalConnectionListener::terminalConnectionDropped(const PtTerminalConnectionEvent& rEvent)
{
}

void PtTerminalConnectionListener::terminalConnectionUnknown(const PtTerminalConnectionEvent& rEvent)
{
}

void PtTerminalConnectionListener::terminalConnectionHeld(const PtTerminalConnectionEvent& rEvent)
{
}

void PtTerminalConnectionListener::terminalConnectionTalking(const PtTerminalConnectionEvent& rEvent)
{
}

void PtTerminalConnectionListener::terminalConnectionInUse(const PtTerminalConnectionEvent& rEvent)
{
}


/* ============================ ACCESSORS ================================= */

/* ============================ INQUIRY =================================== */

PT_IMPLEMENT_CLASS_INFO(PtTerminalConnectionListener, PtConnectionListener)

/* //////////////////////////// PROTECTED ///////////////////////////////// */

/* //////////////////////////// PRIVATE /////////////////////////////////// */

/* ============================ FUNCTIONS ================================= */

