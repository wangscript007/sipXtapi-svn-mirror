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
#include <string.h>
#include <sys/types.h>
#include <sys/types.h>
#include <regex.h>

// APPLICATION INCLUDES
#include "os/OsSysLog.h"
#include "os/OsDefs.h"
#include "SipRedirectorISN.h"
#include "net/SipSrvLookup.h"
#include "net/Url.h"

#if defined(_WIN32)
#       include "resparse/wnt/sysdep.h"
#       include <resparse/wnt/netinet/in.h>
#       include <resparse/wnt/arpa/nameser.h>
#       include <resparse/wnt/resolv/resolv.h>
#       include <winsock.h>
extern "C" {
#       include "resparse/wnt/inet_aton.h"       
}
#elif defined(_VXWORKS)
#       include <stdio.h>
#       include <netdb.h>
#       include <netinet/in.h>
#       include <vxWorks.h>
/* Use local lnameser.h for info missing from VxWorks version --GAT */
/* lnameser.h is a subset of resparse/wnt/arpa/nameser.h                */
#       include <resolv/nameser.h>
#       include <resparse/vxw/arpa/lnameser.h>
/* Use local lresolv.h for info missing from VxWorks version --GAT */
/* lresolv.h is a subset of resparse/wnt/resolv/resolv.h               */
#       include <resolv/resolv.h>
#       include <resparse/vxw/resolv/lresolv.h>
/* #include <sys/socket.h> used sockLib.h instead --GAT */
#       include <sockLib.h>
#       include <resolvLib.h>
#       include <resparse/vxw/hd_string.h>
#elif defined(__pingtel_on_posix__)
#       include <arpa/inet.h>
#       include <netinet/in.h>
#       include <sys/socket.h>
#       include <resolv.h>
#       include <netdb.h>
#else
#       error Unsupported target platform.
#endif

#ifndef __pingtel_on_posix__
extern struct __res_state _sip_res;
#endif

#include "resparse/rr.h"

// The space allocated for returns from res_query.
#define DNS_RESPONSE_SIZE 4096

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
   return new SipRedirectorISN(instanceName);
}

// Constructor
SipRedirectorISN::SipRedirectorISN(const UtlString& instanceName) :
   RedirectPlugin(instanceName)
{
}

// Destructor
SipRedirectorISN::~SipRedirectorISN()
{
}

// Read config information.
void SipRedirectorISN::readConfig(OsConfigDb& configDb)
{
   if (configDb.get("WK_DOMAIN", mWKDomain) != OS_SUCCESS ||
       mWKDomain.isNull())
   {
      OsSysLog::add(FAC_SIP, PRI_CRIT,
                    "SipRedirectorISN::readConfig "
                    "WK_DOMAIN parameter missing or empty");
   }
}

// Initializer
OsStatus
SipRedirectorISN::initialize(OsConfigDb& configDb,
                               SipUserAgent* pSipUserAgent,
                               int redirectorNo,
                               const UtlString& localDomainHost)
{
   return OS_SUCCESS;
}

// Finalizer
void
SipRedirectorISN::finalize()
{
}

RedirectPlugin::LookUpStatus
SipRedirectorISN::lookUp(
   const SipMessage& message,
   const UtlString& requestString,
   const Url& requestUri,
   const UtlString& method,
   SipMessage& response,
   RequestSeqNo requestSeqNo,
   int redirectorNo,
   SipRedirectorPrivateStorage*& privateStorage)
{
   bool status = false;

   // Get the user part.
   UtlString userId;
   requestUri.getUserId(userId);

   // Test of the user part is in ISN format -- digits*digits
   const char* user = userId.data();
   size_t i = strspn(user, "0123456789");
   size_t j = 0;
   if (i > 0)
   {
      if (user[i] == '*')
      {
         j = strspn(user + i + 1, "0123456789");
         if (user[i + 1 + j] == '\0')
         {
            status = true;
         }
      }
   }

   if (status)
   {
      // Format of user part is correct.  Look for NAPTR records.

      // Create the domain to look up.
      char domain[2 * strlen(user) + mWKDomain.length()];
      {
         char* p = &domain[0];
         // Copy the extension, reversing it and following each digit with a period.
         for (int k = i; --k >= 0; )
         {
            *p++ = user[k];
            *p++ = '.';
         }
         // Append the ITAD and a period.
         strcpy(p, user + i + 1);
         strcat(p, ".");
         // Append the well-known ITAD root domain.
         strcat(p, mWKDomain.data());
      }
      OsSysLog::add(FAC_SIP, PRI_DEBUG,
                    "SipRedirectorISN::lookUp user '%s' has ISN format, domain is '%s'",
                    user, domain);

      // To hold the return of res_query_and_parse.
      res_response* dns_response;
      const char* canonical_name;
      
      // Make the query and parse the response.
      SipSrvLookup::res_query_and_parse(domain, T_NAPTR, NULL, canonical_name, dns_response);

      if (dns_response != NULL)
      {
         // Search the list of RRs for the 'best' answer.
         // Initialize to values at least 2**16.
         int lowest_order_seen = 1 << 16, lowest_preference_seen = 1 << 16;
         int best_response = -1; // No response found..
         // Search the answer list.
         for (unsigned int i = 0; i < dns_response->header.ancount; i++)
         {
            if (dns_response->answer[i]->rclass == C_IN &&
                dns_response->answer[i]->type == T_NAPTR &&
                // Note we look for the canonical name now.
                strcasecmp(canonical_name, dns_response->answer[i]->name) == 0)
            {
               // A NAPTR record has been found.
               OsSysLog::add(FAC_SIP, PRI_DEBUG,
                             "SipRedirectorISN::LookUp "
                             "NAPTR record found '%s' %d %d %d %d '%s' '%s' '%s' '%s'",
                             dns_response->answer[i]->name,
                             dns_response->answer[i]->rclass,
                             dns_response->answer[i]->type,
                             dns_response->answer[i]->rdata.naptr.order,
                             dns_response->answer[i]->rdata.naptr.preference,
                             dns_response->answer[i]->rdata.naptr.flags,
                             dns_response->answer[i]->rdata.naptr.services,
                             dns_response->answer[i]->rdata.naptr.regexp,
                             dns_response->answer[i]->rdata.naptr.replacement);
               // Accept the record if flags are 'u' and services are 'E2U+sip'.
               // I believe that the value 'E2U+sip' defined by the
               // ISN Cookbook is wrong, as it does not follow the
               // pattern in all other NAPTR uses, but instead should be
               // 'sip+E2U'.  So here we accept either form, in case ISN corrects
               // the services value.
               if (strcasecmp(dns_response->answer[i]->rdata.naptr.flags, "u") == 0 &&
                   (strcasecmp(dns_response->answer[i]->rdata.naptr.services, "E2U+sip") == 0 ||
                    strcasecmp(dns_response->answer[i]->rdata.naptr.services, "E2U+sip") == 0))
               {
                  // Check that it has the lowest order and preference values seen so far.
                  if (dns_response->answer[i]->rdata.naptr.order < lowest_order_seen ||
                      (dns_response->answer[i]->rdata.naptr.order == lowest_order_seen &&
                       dns_response->answer[i]->rdata.naptr.preference < lowest_preference_seen))
                  {
                     best_response = i;
                     lowest_order_seen = dns_response->answer[i]->rdata.naptr.order;
                     lowest_preference_seen = dns_response->answer[i]->rdata.naptr.preference;
                  }
               }
            }
         }

         // At this point, best_response (if any) is the response we chose.
         if (best_response != -1)
         {
            char* p = dns_response->answer[best_response]->rdata.naptr.regexp;
            OsSysLog::add(FAC_SIP, PRI_DEBUG,
                          "SipRedirectorISN::LookUp Using NAPTR rewrite '%s' for '%s'",
                          p, domain);
            // Enough space for the 'match' part of the regexp field.
            char match[strlen(p) + 1];
            // Pointer to the 'replace' part of the regexp field.
            char delim;
            const char* replace;
            int i_flag;
            if (res_naptr_split_regexp(p, &delim, match, &replace, &i_flag))
            {
               OsSysLog::add(FAC_SIP, PRI_DEBUG,
                             "SipRedirectorISN::LookUp match = '%s', replace = '%s', i_flag = %d",
                             match, replace, i_flag);
               // Split operation was successful.  Try to match.
               regex_t reg;
               int ret = regcomp(&reg, match, REG_EXTENDED | (i_flag ? REG_ICASE : 0));
               if (ret == 0)
               {
                  // NAPTR matches can have only 9 substitutions.
                  regmatch_t pmatch[9];
                  // regexec returns 0 for success.
                  // Though RFC 2915 and the ISN Cookbook don't say, it appears
                  // that the regexp is matched against the user-part of the SIP URI.
                  if (regexec(&reg, userId, 9, pmatch, 0) == 0)
                  {
                     // Match was successful.  Perform replacement.
                     char* result = res_naptr_replace(replace, delim, pmatch, userId);
                     OsSysLog::add(FAC_SIP, PRI_DEBUG,
                                   "SipRedirectorISN::LookUp result = '%s'",
                                   result);
                     // Parse result string into URI.
                     Url contact(result, TRUE);
                     // Almost all strings are parsable as SIP URIs with a sufficient
                     // number of components missing.  Be we can check that a scheme
                     // was identified, and that a host name was found.
                     // (A string with sufficiently few punctuation characters appears to
                     // be a sip: URI with the scheme missing and only a host name, but
                     // the legal character set for host names is fairly narrow.)
                     UtlString h;
                     contact.getHostAddress(h);
                     if (contact.getScheme() != Url::UnknownUrlScheme && !h.isNull())
                     {
                        RedirectPlugin::addContact(response, requestString,
                                                   contact, "ISN");

                     }
                     else
                     {
                        OsSysLog::add(FAC_SIP, PRI_ERR,
                                      "SipRedirectorISN::LookUp Bad result string '%s' - "
                                      "could not identify URI scheme and/or host name is null - "
                                      "for ISN translation of '%s'",
                                      result, requestString.data());
                     }
                     // Free the result string.
                     free(result);
                  }
                  else
                  {
                     OsSysLog::add(FAC_SIP, PRI_WARNING,
                                   "SipRedirectorISN::LookUp NAPTR regexp '%s' does not match "
                                   "for ISN translation of '%s' - no contact generated",
                                   match, requestString.data());
                  }
                  // Free the parsed regexp structure.
                  regfree(&reg);
               }
               else
               {
                  OsSysLog::add(FAC_SIP, PRI_WARNING,
                                "SipRedirectorISN::LookUp NAPTR regexp '%s' is syntactially invalid "
                                   "for ISN translation of '%s'",
                                match, requestString.data());
               }
            }
            else
            {
               OsSysLog::add(FAC_SIP, PRI_ERR,
                             "SipRedirectorISN::LookUp cannot parse NAPTR regexp field '%s' "
                             "for ISN translation of '%s'",
                             p, requestString.data());
            }
         }
         else
         {
            OsSysLog::add(FAC_SIP, PRI_WARNING,
                          "SipRedirectorISN::LookUp No usable NAPTR found for '%s'"
                          "for ISN translation of '%s'",
                          domain, requestString.data());
         }            
      }
      else
      {
         OsSysLog::add(FAC_SIP, PRI_WARNING,
                       "SipRedirectorISN::LookUp no NAPTR record found for domain '%s' "
                       "for ISN translation of '%s'",
                       domain, requestString.data());
      }
   
      // Free the result of res_parse if necessary.
      if (dns_response != NULL)
      {
         res_free(dns_response);
      }
      if (canonical_name != NULL && canonical_name != domain)
      {
         free((void*) canonical_name);
      }
   }

   return RedirectPlugin::LOOKUP_SUCCESS;
}
