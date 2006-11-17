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

// SYSTEM INCLUDES

// APPLICATION INCLUDES
#include <utl/UtlRegex.h>
#include "os/OsDateTime.h"
#include "os/OsSysLog.h"
#include "sipdb/SIPDBManager.h"
#include "sipdb/PermissionDB.h"
#include "sipdb/ResultSet.h"
#include "SipRedirectorMapping.h"

// DEFINES
// MACROS
// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STRUCTS
// TYPEDEFS
// FORWARD DECLARATIONS

// Static factory function.
extern "C" RedirectPlugin* getRedirectPlugin(const UtlString& instanceName)
{
   return new SipRedirectorMapping(instanceName);
}

// Constructor
SipRedirectorMapping::SipRedirectorMapping(const UtlString& instanceName) :
   RedirectPlugin(instanceName),
   mMappingRulesLoaded(OS_FAILED)
{
   mLogName.append("[");
   mLogName.append(instanceName);
   mLogName.append("] SipRedirectorMapping");
}

// Destructor
SipRedirectorMapping::~SipRedirectorMapping()
{
}

// Read config information.
void SipRedirectorMapping::readConfig(OsConfigDb& configDb)
{
   configDb.get("MEDIA_SERVER", mMediaServer);
   configDb.get("VOICEMAIL_SERVER", mVoicemailServer);
   configDb.get("MAPPING_RULES_FILENAME", mFileName);
   UtlString s;
   configDb.get("FALLBACK", s);
   mFallback = s.compareTo("true") == 0;
}

// Initialize
OsStatus
SipRedirectorMapping::initialize(OsConfigDb& configDb,
                                 SipUserAgent* pSipUserAgent,
                                 int redirectorNo,
                                 const UtlString& localDomainHost)
{
   OsSysLog::add(FAC_SIP, PRI_DEBUG,
                 "%s::SipRedirectorMapping Loading mapping rules from '%s'",
                 mLogName.data(), mFileName.data());

   mMappingRulesLoaded = mMap.loadMappings(mFileName,
                                           mMediaServer,
                                           mVoicemailServer,
                                           localDomainHost);

   return mMappingRulesLoaded ? OS_SUCCESS : OS_FAILED;
}

// Finalize
void
SipRedirectorMapping::finalize()
{
}

RedirectPlugin::LookUpStatus
SipRedirectorMapping::lookUp(
   const SipMessage& message,
   const UtlString& requestString,
   const Url& requestUri,
   const UtlString& method,
   SipMessage& response,
   RequestSeqNo requestSeqNo,
   int redirectorNo,
   SipRedirectorPrivateStorage*& privateStorage)
{
   UtlBoolean isPSTNNumber = false;
   UtlString permissionName;
   ResultSet urlMappingRegistrations;
   ResultSet urlMappingPermissions;

   // Return immediately if this is a "fallback" mapping and there are
   // already contacts.
   if (mFallback &&
       response.getCountHeaderFields(SIP_CONTACT_FIELD) > 0)
   {
      return RedirectPlugin::LOOKUP_SUCCESS;
   }

   // @JC This variable is strangely overloaded
   // If we have no permissions then add any encountered
   // contacts. If we do have permissions then the
   // permission must match
   UtlBoolean permissionFound = TRUE;

   if (mMappingRulesLoaded == OS_SUCCESS)
   {
      mMap.getContactList(
         requestUri,
         urlMappingRegistrations,
         isPSTNNumber,
         urlMappingPermissions);
   }

   int numUrlMappingPermissions = urlMappingPermissions.getSize();

   OsSysLog::add(FAC_SIP, PRI_DEBUG, "%s::lookUp "
                 "got %d UrlMapping Permission requirements for %d contacts",
                 mLogName.data(), numUrlMappingPermissions,
                 urlMappingRegistrations.getSize());

   if (numUrlMappingPermissions > 0)
   {
      // check if we have field parameter that indicates that some permissions should be ignored
      UtlString ignorePermissionStr;
      // :KLUDGE: remove const_cast and uri declaration after XSL-88 gets fixed
      Url& uri = const_cast<Url&>(requestUri);
      uri.getUrlParameter("sipx-noroute", ignorePermissionStr);  
      
      for (int i = 0; i<numUrlMappingPermissions; i++)
      {
         UtlHashMap record;
         urlMappingPermissions.getIndex(i, record);
         UtlString permissionKey("permission");
         UtlString urlMappingPermissionStr =
            *((UtlString*) record.findValue(&permissionKey));

         // Try to match the permission
         // so assume it cannot be found unless we
         // see a match in the IMDB
         permissionFound = FALSE;
         
         // if the permission we are looking for is the one the we are supposed to ignore,
         // than assume that permission is not found
         if (urlMappingPermissionStr.compareTo(ignorePermissionStr, UtlString::ignoreCase) == 0) 
         {            
             OsSysLog::add(FAC_SIP, PRI_DEBUG,
                           "%s::lookUp ignoring permission %s",
                           mLogName.data(), ignorePermissionStr.data());
             continue;
         }

         // See if we can find a matching permission in the IMDB
         ResultSet dbPermissions;

         // check in permission database is user has permisssion for voicemail
         PermissionDB::getInstance()->
            getPermissions(requestUri, dbPermissions);

         int numDBPermissions = dbPermissions.getSize();

         OsSysLog::add(FAC_SIP, PRI_DEBUG,
                       "%s::lookUp %d permissions configured for %s",
                       mLogName.data(), numDBPermissions,
                       requestUri.toString().data());
                
         if (numDBPermissions > 0)
         {
            for (int j=0; j<numDBPermissions; j++)
            {
               UtlHashMap record;
               dbPermissions.getIndex(j, record);
               UtlString dbPermissionStr =
                  *((UtlString*)record.
                    findValue(&permissionKey));

               OsSysLog::add(FAC_SIP, PRI_DEBUG, "%s::lookUp checking '%s'",
                             mLogName.data(), dbPermissionStr.data());
                
               if (dbPermissionStr.compareTo(urlMappingPermissionStr, UtlString::ignoreCase) == 0)
               {
                  // matching permission found in IMDB
                  permissionFound = TRUE;
                  break;
               }
               dbPermissionStr.remove(0);
            }
         }

         if (permissionFound)
         {
            break;               
         }
         urlMappingPermissionStr.remove(0);
      }
   }

   // either there were no requirements to match against voicemail
   // or there were and we found a match in the IMDB for the URI
   // so now add the contacts to the SIP message
   if (permissionFound)
   {
      int numUrlMappingRegistrations = urlMappingRegistrations.getSize();

      OsSysLog::add(FAC_SIP, PRI_DEBUG,
                    "%s::lookUp got %d UrlMapping Contacts",
                    mLogName.data(), numUrlMappingRegistrations);

      if (numUrlMappingRegistrations > 0)
      {
         UtlString requestIdentity;
         requestUri.getIdentity(requestIdentity);

         for (int i = 0; i < numUrlMappingRegistrations; i++)
         {
            UtlHashMap record;
            urlMappingRegistrations.getIndex(i, record);
            UtlString contactKey("contact");
            UtlString contact= *((UtlString*)record.findValue(&contactKey));
            OsSysLog::add(FAC_SIP, PRI_DEBUG,
                          "%s::lookUp contact = '%s'",
                          mLogName.data(), contact.data());
            Url contactUri(contact);
            OsSysLog::add(FAC_SIP, PRI_DEBUG,
                          "%s::lookUp contactUri = '%s'",
                          mLogName.data(), contactUri.toString().data());

            // prevent recursive loops
            UtlString routeHeaderParam;
            if (!contactUri.isUserHostPortEqual(requestUri) ||
                contactUri.getHeaderParameter("route", routeHeaderParam)
               )
            {
               // Add the contact.
               addContact(response, requestString, contactUri, mLogName.data());
            }
         }
      }
   }

   return RedirectPlugin::LOOKUP_SUCCESS;
}
