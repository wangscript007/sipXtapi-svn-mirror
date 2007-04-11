//  
// Copyright (C) 2007 SIPez LLC. 
// Licensed to SIPfoundry under a Contributor Agreement. 
//
// Copyright (C) 2007 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
//
// $$
///////////////////////////////////////////////////////////////////////////////

#ifndef _MpResourceMsg_h_
#define _MpResourceMsg_h_

// SYSTEM INCLUDES

// APPLICATION INCLUDES
#include "os/OsMsg.h"
#include "utl/UtlString.h"

// DEFINES
// MACROS
// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STRUCTS
// TYPEDEFS
// FORWARD DECLARATIONS

/// Message object used to communicate with the media processing task
class MpResourceMsg : public OsMsg
{
/* //////////////////////////// PUBLIC //////////////////////////////////// */
public:

   /// Phone set message types
   typedef enum
   {
      MPRM_RESOURCE_DISABLE,
      MPRM_RESOURCE_ENABLE,
      MPRM_TONEGEN_START,
      MPRM_TONEGEN_STOP,
      MPRM_FROMFILE_START,
      MPRM_FROMFILE_PAUSE,
      MPRM_FROMFILE_STOP
   } MpResourceMsgType;

/* ============================ CREATORS ================================== */
///@name Creators
//@{

     /// Constructor
   MpResourceMsg(int msg, const UtlString& msgDestName);

     /// Copy constructor
   MpResourceMsg(const MpResourceMsg& rMpResourceMsg);

     /// Create a copy of this msg object (which may be of a derived type)
   virtual OsMsg* createCopy(void) const;

     /// Destructor
   virtual
   ~MpResourceMsg();

//@}

/* ============================ MANIPULATORS ============================== */
///@name Manipulators
//@{

     /// Assignment operator
   MpResourceMsg& operator=(const MpResourceMsg& rhs);

     /// Set the name of the resource this message applies to.
   void setDestResourceName(const UtlString& msgDestName);
     /**<
     *  Sets the name of the intended recipient for this message.
     */
//@}

/* ============================ ACCESSORS ================================= */
///@name Accessors
//@{

     /// Returns the type of the media resource message
   int getMsg(void) const;

     /// Get the name of the resource this message applies to.
   UtlString getDestResourceName(void) const;
     /**<
     *  Returns the name of the MpResource object that is the intended 
     *  recipient for this message.
     */

//@}

/* ============================ INQUIRY =================================== */
///@name Inquiry
//@{

//@}

/* //////////////////////////// PROTECTED ///////////////////////////////// */
protected:

/* //////////////////////////// PRIVATE /////////////////////////////////// */
private:
   UtlString mMsgDestName; ///< Intended recipient for this message
};

/* ============================ INLINE METHODS ============================ */

#endif  // _MpResourceMsg_h_
