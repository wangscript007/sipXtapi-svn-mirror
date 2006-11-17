// 
// 
// Copyright (C) 2005 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
// 
// Copyright (C) 2005 Pingtel Corp.
// Licensed to SIPfoundry under a Contributor Agreement.
// 
// $$
//////////////////////////////////////////////////////////////////////////////

#ifndef SIPREDIRECTORMAPPING_H
#define SIPREDIRECTORMAPPING_H

// SYSTEM INCLUDES
//#include <...>

// APPLICATION INCLUDES
#include "registry/RedirectPlugin.h"
#include "digitmaps/UrlMapping.h"

// DEFINES
// MACROS
// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STRUCTS
// TYPEDEFS
// FORWARD DECLARATIONS

/**
 * SipRedirectorMapping is a class whose object adds contacts that are
 * listed a mapping rules file.
 *
 * Currently, we instantiate two objects within the class, one for
 * mappingrules.xml and one for fallbackrules.xml.
 */

class SipRedirectorMapping : public RedirectPlugin
{
  public:

   SipRedirectorMapping(const UtlString& instanceName);

   ~SipRedirectorMapping();

   /**
    * Requires the following config parameters:
    *
    * MEDIA_SERVER - the URI of the Media Server.
    *
    * VOICEMAIL_SERVER - the URI of the voicemail server.
    *
    * MAPPING_RULES_FILENAME - full filename of the mapping rules file
    * to load (e.g., "mappingrules.xml")
    *
    * FALLBACK - "true" if fallback rules, that is, no contacts should
    * be added if there are already contacts in the set.
    */
   virtual void readConfig(OsConfigDb& configDb);

   virtual OsStatus initialize(OsConfigDb& configDb,
                               SipUserAgent* pSipUserAgent,
                               int redirectorNo,
                               const UtlString& localDomainHost);

   virtual void finalize();

   virtual RedirectPlugin::LookUpStatus lookUp(
      const SipMessage& message,
      const UtlString& requestString,
      const Url& requestUri,
      const UtlString& method,
      SipMessage& response,
      RequestSeqNo requestSeqNo,
      int redirectorNo,
      SipRedirectorPrivateStorage*& privateStorage);

  protected:

   // String to use in place of class name in log messages:
   // "[instance] class".
   UtlString mLogName;

   /**
    * Set to OS_SUCCESS once the file of mapping rules is loaded into memory.
    */
   OsStatus mMappingRulesLoaded;

   /**
    * The mapping rules parsed from the file.
    */
   UrlMapping mMap;

   /**
    * Name to use when reporting on this mapping.
    */
   UtlString mName;

   /**
    * True if this mapping is "fallback", that is, no contacts should
    * be added if there are already contacts in the set.
    */
   UtlBoolean mFallback;

   /**
    * SIP URI to access the Media Server.
    */
   UtlString mMediaServer;

   /**
    * HTTP URL to access the Voicemail Server.
    */
   UtlString mVoicemailServer;

   /**
    * Full name of file containing the mapping rules.
    */
   UtlString mFileName;
};

#endif // SIPREDIRECTORMAPPING_H
