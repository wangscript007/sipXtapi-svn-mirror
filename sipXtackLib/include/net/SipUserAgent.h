//
// Copyright (C) 2004-2006 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
//
// Copyright (C) 2004-2006 Pingtel Corp.  All rights reserved.
// Licensed to SIPfoundry under a Contributor Agreement.
//
// $$
///////////////////////////////////////////////////////////////////////////////


#ifndef _SipUserAgent_h_
#define _SipUserAgent_h_

// SYSTEM INCLUDES
//#include <...>

// APPLICATION INCLUDES
#include <utl/UtlHashBag.h>
#include <os/OsServerTask.h>
#include <net/SipUserAgentBase.h>
#include <net/SipMessage.h>
#include <net/SipMessageEvent.h>
#include <net/SipTransaction.h>
#include <net/SipTransactionList.h>
#include <net/SipUdpServer.h>
#include <os/OsQueuedEvent.h>
#ifdef SIP_TLS
#include <net/SipTlsServer.h>
#include <os/OsTLS.h>
#endif
#include <os/OsNatKeepaliveListener.h>

// DEFINES
#define SIP_DEFAULT_RTT     500
#define SIP_MINIMUM_RTT     100
#define SIP_MAX_PORT_RANGE  10  // If a port is in use and the sip user agent 
                                // is created with bUseNextAvailablePort set to
                                // true, this is the number of sequential ports
                                // to try.

// proxy, registrar, etc. UDP socket buffer size
#define SIPUA_DEFAULT_SERVER_UDP_BUFFER_SIZE 1000000

// proxy, registrar, etc. OsServerTask OsMsg queue size
#define SIPUA_DEFAULT_SERVER_OSMSG_QUEUE_SIZE 10000

// MACROS
// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STRUCTS
// TYPEDEFS
// FORWARD DECLARATIONS
class OsConfigDb;
class OsQueuedEvent;
class OsTimer;
class SipSession;
class SipTcpServer;
class SipLineMgr;
class SipUserAgentBase;

//! Transaction and Transport manager for SIP stack
/*! Note SipUserAgent is perhaps not the best name for this class.
 * It is really the transaction and transport layers of
 * of a SIP stack which may be used in a User Agent or
 * server context.
 *
 * \par Using SipUserAgent
 *
 * The SipUserAgent is the primary interface for incoming
 * and out going SIP messages.  It handles all of the
 * reliability, resending, canceling and IP layer protocol
 * details.  Application developers that wish to send or
 * recieve SIP messages can use SipUserAgent without having
 * to worry about the transaction and transport details.
 * \par
 * Applications send SIP messages via the send() method.  Incoming
 * messages are received on an OsMsgQ to be handled by the
 * application.  The message queue must be registered with
 * the SipUserAgent via addMessageObserver() before they
 * can receive incoming messages.  Alternatively applications
 * that are only interested in a specific transaction can
 * pass the OsMsgQ as part of the send() method invocation.
 * Messages which fail to be sent due to transport problems
 * will be communicated back in one of three different ways:
 * -# send() returns a failure indication synchronously (e.g.
 *        due to unresolvable DNS name)
 * -# the send fails asynchronously (e.g. ICMP error) and puts
 *        a message in the OsMsgQ with a transport failure indication
 * -# the send succeeds, but the transaction fails or times out
 *        due to the lack of completion or responses to a request. In
 *        this case a message is put in the OsMsgQ with a transport
 *        failure indication.
 *
 * In the asynchronous cases where a message is put in the message
 * queue to indicate the failure, the original SIP message is attached
 * so that the application can determine which SIP message send failed.
 *
 * \par Internal Implementation Overview
 *
 * All state information will be contained in transactions
 * and/or the messages contained by a transaction.  The transaction
 * will keep track of what protocols to use and when as well as
 * when to schedule timers. send() will no longer be used for
 * resending.  It will only be used for the first time send.
 *
 * The flow for outgoing messages is something like the
 * following:
 * - 1) An application calls send() to send a SIP request or response
 * - 2) send() for requests: constructs a client transaction,
 *    for: responses finds an existing server transaction
 * - 3) send() asks the transaction how (i.e. protocol) and whether to
 *    send the message
 * - 4) send() calls the appropriate transport sender (e.g. sendUdp, sendTcp)
 * - 5A) If the send succeeded: send() asks the transaction to schedule
 *     a timeout for resending the message or failing the transaction
 * - 5B) If the send failed: send() asks the transaction whether to:
 *     - a) dispatch the transport error and mark the transaction state
 *         to indicate the failure.
 *     - b) try another protocol and repeat starting at step 4 above
 *
 * Timeouts are handled by handle message in the following flow:
 * - 1) The timeout expires and posts a SipMessageEvent on the
 *    SipUserAgent's queue which invokes handleMessage
 * - 2) handleMessage() finds the transaction for the timeout
 * - 3) handleMessage() asks the transaction if it should resend
 *    the message.
 * - 4A) The message may not be resent for one of a number of reasons:
 *     - a) a response or ACK was recieved
 *     - b) the transaction was canceled
 *     - c) It is time to give up and fail the transaction
 *     In the latter case an error must be dispatched to the
 *     application.  In the other cases nothing is done.
 * - 4B) If the message is to be resent, the transaction tells
 *     which protocol sender to use (as in steps 4 & 5 above
 *     for outbound messages).
 *
 * Inbound messages are still sent via dispatch.  The flow is now a
 * little different due to the use of transaction objects
 * - 1) dispatch() finds a matching transaction for non transport error
 *    messages.
 * - 2A) If the message is a duplicate it is dropped on the floor
 * 2B) If the message is a new request, a server transaction is
 *     created
 * - 2C) If the message is a new response, for an exising client
 *     transaction, it is sent to the interested observers, with
 *     the original request attached.
 * - 2D) If the message is a response with no existing transaction,
 *     it is dropped on the floor ??I think it should be for UAC
 *     transactions anyway.  Proxy client response may need to be
 *     sent to observers??
 * - 3) If the message was not dropped on the floor by step 2A or 2D,
 *    the message is sent to the interested observers
 */

class SipUserAgent : public SipUserAgentBase 
#ifdef SIP_TLS
                     , public ITlsSink
#endif 
{
/* //////////////////////////// PUBLIC //////////////////////////////////// */
public:

    friend class SipTransaction;
    friend class SipUdpServer;
    friend int SipUdpServer::run(void* runArg);

    enum EventSubTypes
    {
        UNSPECIFIED = 0,
        SHUTDOWN_MESSAGE = 10,
        KEEPALIVE_MESSAGE
    };

/* ============================ CREATORS ================================== */

    //! Constructor
    /*! Sets up listeners on the defined ports and IP layer
     * protocols for incoming SIP messages.
     * \param sipTcpPort - port to listen on for SIP TCP messages.
     *        Specify PORT_DEFAULT to automatically select a port, or
     *        PORT_NONE to disable.
     * \param sipUdpPort - port to listen on for SIP UDP messages.
     *        Specify PORT_DEFAULT to automatically select a port, or
     *        PORT_NONE to disable.
     * \param sipTlsPort - port to listen on for SIP TLS messages.
     *        Specify PORT_DEFAULT to automatically select a port, or
     *        PORT_NONE to disable.
     * \param publicAddress - use this address in Via and Contact headers
     *        instead of the actual adress.  This is useful for address
     *        spoofing in a UA when behind a NAT
     * \param defaultUser - default user ID to use in Contacts which get
     *        inserted when missing on a UA.
     * \param defaultSipAddress - deprecated
     * \param sipProxyServers - server to which non-routed requests should
     *        be sent for next hop before going to the final destination
     * \param sipDirectoryServers - deprecated
     * \param sipRegistryServers - deprecated
     * \param authenticationScheme - authentication scheme to use when
     *        challenging on behalf of the UA (i.e. 401).  Valid values
     *        are NONE and DIGEST.
     * \param authenicateRealm - The authentication realm to use when
     *        sending 401 challenges.
     * \param authenticateDb - the authentication DB to use when
     *        authenticating incoming requests on behalf of the UA
     *        application (as a result of locally generated 401 challenges
     * \param authorizeUserIds - depricated by the SipLineMgr
     * \param authorizePasswords - depricated by the SipLineMgr
     * \param lineMgr - SipLinMgr object which is container for user
     *        definitions and their credentials.  This is used to
     *        authenticate incoming requests to the UA application.
     * \param sipFirstResendTimeout - T1 in RFC 3261
     * \param defaultToUaTransactions - default transactions to be
     *        UA or PROXY.  TRUE means that this is a UA and associated
     *        validation should occur.  FALSE means that this is a
     *        PROXY and that minimal validation should occur.
     * \param readBufferSize - the default IP socket buffer size
     *        to use for the listener sockets.
     * \param queueSize - Size of the OsMsgQ to use for the queues
     *        internal to the SipUserAgent and subsystems.
     * \param bUseNextAvailablePort - When setting up the sip user 
     *        agent using the designated sipTcp, sipUdp, and sipTls
     *        ports, select the next available port if the supplied
     *        port is busy.  If enable, this will attempt at most
     *        10 sequential ports.
     * \param doUaMessageChecks - check the acceptability of method,
     *        extensions, and encoding.  The default is TRUE; it may 
     *        be set to false in applications such as a redirect server
     *        that will never actually send a 2xx response, so the
     *        checks might cause errors that the application should
     *        never generate.
     */
    SipUserAgent(int sipTcpPort = SIP_PORT,
                int sipUdpPort = SIP_PORT,
                int sipTlsPort = SIP_PORT+1,
                const char* publicAddress = NULL,
                const char* defaultUser = NULL,
                const char* defaultSipAddress = NULL,
                const char* sipProxyServers = NULL,
                const char* sipDirectoryServers = NULL,
                const char* sipRegistryServers = NULL,
                const char* authenticationScheme = NULL,
                const char* authenicateRealm = NULL,
                OsConfigDb* authenticateDb = NULL,
                OsConfigDb* authorizeUserIds = NULL,
                OsConfigDb* authorizePasswords = NULL,
                SipLineMgr* lineMgr = NULL,
                int sipFirstResendTimeout = SIP_DEFAULT_RTT,
                UtlBoolean defaultToUaTransactions = TRUE,
                int readBufferSize = -1,
                int queueSize = OsServerTask::DEF_MAX_MSGS,
                UtlBoolean bUseNextAvailablePort = FALSE,
                UtlString certNickname = "",
                UtlString certPassword = "",
                UtlString dbLocation = ".",
                UtlBoolean doUaMessageChecks = TRUE);

    //! Destructor
    virtual
    ~SipUserAgent();

/* ============================ MANIPULATORS ============================== */

    //! Cleanly shuts down SipUserAgent.
    /*! This method can block until the shutdown is complete, or it can be
     * non-blocking.  When complete, the SipUserAgent can be deleted.
     * \sa isShutdownDone
     *
     * \param blockingShutdown - TRUE if this method should block until the
     * shutdown is complete, FALSE if this method should be non-blocking.
     */
    void shutdown(UtlBoolean blockingShutdown = TRUE);

    //! Enable stun lookups for UDP signaling.  Use a NULL szStunServer to 
    //! disable
    virtual void enableStun(const char* szStunServer, 
                            int iStunPort,
                            int refreshPeriodInSecs, 
                            OsNotification* pNotification = NULL,
                            const char* szIp = NULL) ;

    //! For internal use only
    virtual UtlBoolean handleMessage(OsMsg& eventMessage);

    //! Deprecated (Add a SIP message recipient)
    virtual void addMessageConsumer(OsServerTask* messageConsumer);

    //! Add a SIP message observer for receiving SIP messages meeting the
    //! given filter criteria
    /*! SIP messages will be added to the \a messageQueue if they meet
     * the given filter criteria.
     *
     * \param messageQueue - the queue on which an SipMessageEvent is
     *        dispatched
     * \param sipMethod - the specific method type of the requests or
     *        responses to be observed.  NULL or a null string indicates
     *        all methods.
     * \param wantRequests - want to observe SIP requests
     * \param wantResponses - want to observe SIP responses
     * \param wantIncoming - want to observe SIP messages originating
     *        from the network.
     * \param wantOutGoing - (not implemented) want to observe SIP
     *        messages originating from locally.
     * \param eventName - want to observer SUBSCRIBE or NOTIFY requests
     *        having the given event type
     * \param pSession - want to observe SIP message with the
     *        specified session (call-id, to url, from url)
     * \param observerData - data to be attached to SIP messages queued
     *        on the observer
     */
    void addMessageObserver(OsMsgQ& messageQueue,
                              const char* sipMethod = NULL,
                              UtlBoolean wantRequests = TRUE,
                              UtlBoolean wantResponses = TRUE,
                              UtlBoolean wantIncoming = TRUE,
                              UtlBoolean wantOutGoing = FALSE,
                              const char* eventName = NULL,
                              SipSession* pSession = NULL,
                              void* observerData = NULL);


    //! Removes all SIP message observers for the given message/queue
    //! observer
    /*! This undoes what addMessageObserver() does.
     * \param messageQueue - All observers dispatching to this message queue
     *         will be removed if the pObserverData is NULL or matches.
     * \param pObserverData - If null, all observers that match the message
     *        queue will be removed.  Otherwise, only observers that match
     *        both the message queue and observer data will be removed.
     * \return TRUE if one or more observers are removed otherwise FALSE.
     */
    UtlBoolean removeMessageObserver(OsMsgQ& messageQueue,
                                    void* pObserverData = NULL);

    //! Send a SIP message over the net
    /*! This method sends the SIP message via
     * a SIP UDP or TCP client as dictated by policy and the address
     * specified in the message.  Most applications will register a
     * OsMsgQ via addMessageObserver() prior to calling send and so
     * should call send with only one argument.
     * \note If the application does register the message queue via
     * addMessageObserver() it should not pass the message queue as
     * an argument to send or it will receive multiple copies of the
     * incoming responses.
     * \param message - the sip message to be sent
     * \param responseListener - the optional queue on which to place
     *        SipMessageEvents containing SIP responses from the same
     *        transaction as the request sent in message
     * \param responseListenerData - optional data to be passed back
     *        with responses
     */
    virtual UtlBoolean send(SipMessage& message,
                            OsMsgQ* responseListener = NULL,
                            void* responseListenerData = NULL,
                            SIPX_TRANSPORT_DATA* pTransport = NULL);

    //! Dispatch the SIP message to the message consumer(s)
    /*! This is typically only used by the SipUserAgent and its sub-system.
     * So unless you know what you are doing you should not be using this
     * method. All incoming SIP message need to be dispatched via the
     * user agent server so that it can provide the reliablity for UDP
     * (i.e. resend requests when no response is received)
     * \param messageType - is as define by SipMessageEvent::MessageStatusTypes
     *        APPLICATION type are normal incoming messages
     *        TRANSPORT_ERROR type are notification of failures to
     *        send messages
     */
    virtual void dispatch(SipMessage* message,
                          int messageType = SipMessageEvent::APPLICATION,
                          SIPX_TRANSPORT_DATA* pData = NULL);

    void allowMethod(const char* methodName, const bool bAllow = true);

    void allowExtension(const char* extension);

    void getSupportedExtensions(UtlString& extensionsString);

    //! Set the SIP proxy servers for the user agent.
    /*! This method will clear any existing proxy servers before
     *  resetting this list.  NOTE: As for 12/2004, only the first
     *  proxy server is used.  Please consider using DNS SRV in
     *  until fully implemented.
     */
    void setProxyServers(const char* sipProxyServers);


    UtlBoolean addCrLfKeepAlive(const char* szLocalIp,
                                const char* szRemoteIp,
                                const int   remotePort,
                                const int   keepAliveSecs,
                                OsNatKeepaliveListener* pListener) ;

    UtlBoolean removeCrLfKeepAlive(const char* szLocalIp,
                                   const char* szRemoteIp,
                                   const int   remotePort) ;
 
    UtlBoolean addStunKeepAlive(const char* szLocalIp,
                                const char* szRemoteIp,
                                const int   remotePort,
                                const int   keepAliveSecs,
                                OsNatKeepaliveListener* pListener) ;

    UtlBoolean removeStunKeepAlive(const char* szLocalIp,
                                   const char* szRemoteIp,
                                   const int   remotePort) ;

    OsTimer* getTimer() { return mpTimer; }

    UtlBoolean addSipKeepAlive(const char* szLocalIp,
                               const char* szRemoteIp,
                               const int   remotePort,
                               const char* szMethod,
                               const int   keepAliveSecs,
                               OsNatKeepaliveListener* pListener) ;

    UtlBoolean removeSipKeepAlive(const char* szLocalIp,
                                  const char* szRemoteIp,
                                  const int   remotePort,
                                  const char* szMethod) ;

/* ============================ ACCESSORS ================================= */

    //! Enable or disable the outbound use of rport (send packet to actual
    //! port -- not advertised port).
    UtlBoolean setUseRport(UtlBoolean bEnable) ;

    //! Is use report set?
    UtlBoolean getUseRport() const ;

    //! Enable or disable use of rport IP and port in response via for contact mapping database
    void setUseRportMapping(UtlBoolean bEnable);

    //! Get if use of rport IP and port in response via for contact mapping database is enabled
    UtlBoolean getUseRportMapping() const;

    //! Get the manually configured public address
    UtlBoolean getConfiguredPublicAddress(UtlString* pIpAddress, int* pPort) ;

    //! Get the local address and port
    UtlBoolean getLocalAddress(UtlString* pIpAddress,
                               int* pPort,
                               SIPX_TRANSPORT_TYPE protocol = TRANSPORT_UDP) ;

    //! Get the NAT mapped address and port
    UtlBoolean getNatMappedAddress(UtlString* pIpAddress, int* pPort) ;

    void setIsUserAgent(UtlBoolean isUserAgent);

    /// Provides a string to be appended to the standard User-Agent header.
    void setUserAgentHeaderProperty( const char* property );
    /**<
     * The property is added between "<product>/<version>" and the platform (eg "(VxWorks)")
     * The value should be formated either as "token/token", "token", or "(string)"
     * with no leading or trailing space.
     */

    //! Set the limit of allowed hops a message can make
    void setMaxForwards(int maxForwards);

    //! Get the limit of allowed hops a message can make
    int getMaxForwards();

    //! Allow or disallow recursion and forking of 3xx class requests
    void setForking(UtlBoolean enabled);

    void getFromAddress(UtlString* address, int* port, UtlString* protocol);

    void getViaInfo(int         protocol,
                    UtlString&  address,
                    int&        port,
                    const char* pszTargetAddress,
                    const int*  piTargetPort);

    void getDirectoryServer(int index, UtlString* address,
                            int* port, UtlString* protocol);

    void getProxyServer(int index, UtlString* address,
                        int* port, UtlString* protocol);

    //! Print diagnostics
    void printStatus();

    void startMessageLog(int newMaximumLogSize = 0);

    void stopMessageLog();

    void clearMessageLog();

    virtual void logMessage(const char* message, int messageLength);

    void getMessageLog(UtlString& logData);

    int getSipStateTransactionTimeout();

    int getDefaultExpiresSeconds() const;
    
    const int getRegisterResponseTimeout() const { return mRegisterTimeoutSeconds; }
    void setRegisterResponseTimeout(const int seconds) { mRegisterTimeoutSeconds = seconds; }

    void setDefaultExpiresSeconds(int expiresSeconds);

    int getDefaultSerialExpiresSeconds() const;

    void setLocationHeader(const char* szHeader);

    //! Tells the User Agent whether or not to append
    //! the platform name onto the User Agent string
    void setIncludePlatformInUserAgentName(const bool bInclude);
    
    void setDefaultSerialExpiresSeconds(int expiresSeconds);

    //! Period of time a TCP socket can remain idle before it is removed
    void setMaxTcpSocketIdleTime(int idleTimeSeconds);

    //! Get the maximum number of DNS SRV records to pursue in the
    //! case of failover
    int getMaxSrvRecords() const;

    //! Set the maximum number of DNS SRV records to pursue in the
    //! case of failover
    void setMaxSrvRecords(int numRecords);

    //! Get the number of seconds to wait before trying the next DNS SRV record
    int getDnsSrvTimeout();

    //! Set the number of seconds to wait before trying the next DNS SRV record
    void setDnsSrvTimeout(int timeout);

    //! Set other DNS names or IP addresses which are considered to
    //! refer to this SipUserAgent.
    /*! Used with routing decisions to determine whether routes
     * are targeted to this SIP server or not.
     * \param aliases - space or comma separated of the format:
     *        "sip:host:port" or "host:port"
     */
    void setHostAliases(UtlString& aliases);

    //! Flag to recurse only one contact in a 300 response
    void setRecurseOnlyOne300Contact(UtlBoolean recurseOnlyOne);
    /***< @note this is a 300 not 3xx class response.@endnote */

    //! Flag to return Vias in too many hops response to request with max-forwards == 0
    void setReturnViasForMaxForwards(UtlBoolean returnVias);

    //! Get a copy of the original request that was sent corresponding
    //! to this incoming response
    /*! \returns NULL if not found.  Caller MUST free the copy of the
     * request when done
     */
    SipMessage* getRequest(const SipMessage& response);

    int getUdpPort() const ;
      //! Get the local UDP port number (or PORT_NONE if disabled) 

    int getTcpPort() const ;
     //! Get the local TCP port number (or PORT_NONE if disabled) 

    int getTlsPort() const ;
      //! Get the local Tls port number (or PORT_NONE if disabled) 
      
    void setUserAgentName(const UtlString& name);
      //! Sets the User Agent name sent with outgoing sip messages.

    const UtlString& getUserAgentName() const;
      //! Sets the User Agent name sent with outgoing sip messages.

    
    void setHeaderOptions(const bool bAllowHeader,
                          const bool bDateHeader,
                          const bool bShortNames,
                          const UtlString& acceptLanguage);                                   
     //! Sets header options - send or not send

    bool getEnabledShortNames()
        {return mbShortNames;}
     // Return enabled state of short names

    void setEnableLocationHeader(const bool bLocationHeader)
        {mbUseLocationHeader=bLocationHeader;}
     // Set if location header is enabled or not


    void stopTransactionTimers() { mSipTransactions.stopTransactionTimers(); }
    void startTransactionTimers() { mSipTransactions.startTransactionTimers(); }                                       

/* ============================ INQUIRY =================================== */

    virtual UtlBoolean isMessageLoggingEnabled();

    virtual UtlBoolean isReady();
    //: Return boolean if the UA is started and initialized

    virtual UtlBoolean waitUntilReady();
    //: Block and wait until the UA is started and initialized

    UtlBoolean isMethodAllowed(const char* method);

    UtlBoolean isExtensionAllowed(const char* extension) const;

    UtlBoolean isForkingEnabled();

    UtlBoolean isMyHostAlias(Url& route) const;

    UtlBoolean recurseOnlyOne300Contact();

    UtlBoolean isOk(OsSocket::IpProtocolSocketType socketType);

    //! Find out if SipUserAgent has finished shutting down.
    /*! Useful when using the non-blocking form of \ref shutdown.
     *
     * \returns TRUE if SipUserAgent has finished shutting down, FALSE otherwise.
     */
    UtlBoolean isShutdownDone();

    void setUserAgentHeader(SipMessage& message);

    void setServerHeader(SipMessage& message);

    /// Add either Server or User-Agent header, as appropriate based on isUserAgent
    void setSelfHeader(SipMessage& message);

    SipContactDb& getContactDb() { return mContactDb; }

    //! Adds a contact record to the contact db
    const bool addContactAddress(SIPX_CONTACT_ADDRESS& contactAddress);
    
    //! Gets all contact addresses for this user agent
    void getContactAddresses(SIPX_CONTACT_ADDRESS* pContacts[], int &numContacts);

    void prepareVia(SipMessage&          message,
                    UtlString&           branchId, 
                    OsSocket::IpProtocolSocketType& toProtocol,
                    const char*          szTargetAddress, 
                    const int*           piTargetPort,
                    SIPX_TRANSPORT_DATA* pTransport = NULL) ;

#ifdef SIP_TLS    
    SipTlsServer* getTlsServer() { return mSipTlsServer; }
    // ITlsSink implementation
    bool onServerCertificate(void* pCert, char* serverHostName);
    bool onTlsEvent(int cause);
#endif

    void addExternalTransport(const UtlString tranportName, const SIPX_TRANSPORT_DATA* const pTransport);
    void removeExternalTransport(const UtlString transportName, const SIPX_TRANSPORT_DATA* const pTransport);
    const SIPX_TRANSPORT_DATA* const lookupExternalTransport(const UtlString transportName, const UtlString ipAddress) const;
    
/* //////////////////////////// PROTECTED ///////////////////////////////// */
protected:
    void prepareContact(SipMessage& message, 
                        const char* szTargetAddress, 
                        const int*  piTargetPort) ;

    /// constuct the value to be used in either user-agent or server header.
    void selfHeaderValue(UtlString& self);

    void getAllowedMethods(UtlString* allowedMethods);

    void whichExtensionsNotAllowed(const SipMessage* message,
                                   UtlString* disallowedExtensions) const;

    UtlBoolean checkMethods(SipMessage* message);

    UtlBoolean checkExtensions(SipMessage* message);

    UtlBoolean sendStatelessResponse(SipMessage& response);

    UtlBoolean sendStatelessRequest(SipMessage& request,
                                    UtlString& address,
                                    int port,
                                    OsSocket::IpProtocolSocketType protocol,
                                    UtlString& branchId);

    UtlBoolean sendTls(SipMessage* message,
                       const char* serverAddress,
                       int port);

    UtlBoolean sendTcp(SipMessage* message,
                       const char* serverAddress,
                       int port);

    UtlBoolean sendUdp(SipMessage* message,
                       const char* serverAddress,
                       int port);

    UtlBoolean sendCustom(SIPX_TRANSPORT_DATA* pTransport, 
                          SipMessage* message, 
                          const char* sendAddress, 
                          const int sendPort);                       

    UtlBoolean sendSymmetricUdp(SipMessage& message,
                                const char* serverAddress,
                                int         port);

    //! DNS SRV lookup for to address
    void lookupSRVSipAddress(UtlString protocol,
                             UtlString& sipAddress,
                             int& port,
                             UtlString& srcIp);

    int getReliableTransportTimeout();

    int getFirstResendTimeout();

    int getLastResendTimeout();

    UtlBoolean shouldAuthenticate(SipMessage* message) const;

    UtlBoolean authorized(SipMessage* request,
                          const char* uri = NULL) const;

    void addAuthentication(SipMessage* message) const;

    UtlBoolean resendWithAuthorization(SipMessage* response,
                                       SipMessage* request,
                                       int* messageType,
                                       int authorizationEntity);
   
/* //////////////////////////// PRIVATE /////////////////////////////////// */
private:

    SipTcpServer* mSipTcpServer;
    SipUdpServer* mSipUdpServer;
#ifdef SIP_TLS
    SipTlsServer* mSipTlsServer;
#endif
    SipTransactionList mSipTransactions;
    UtlString defaultSipUser;
    UtlString defaultSipAddress;
    UtlString proxyServers;
    UtlString directoryServers;
    UtlString registryServers;
    int registryPeriod;
    int lastRegisterSeqNum;
    UtlString registerCallId;
    UtlString sipIpAddress;
    UtlString mConfigPublicAddress ;
    int mSipPort;
    UtlDList allowedSipMethods;
    UtlDList allowedSipExtensions;
    UtlString mUserAgentHeaderProperties;
    UtlHashBag mMyHostAliases;
    UtlHashBag mMessageObservers;
    UtlHashMap mExternalTransports;
    OsRWMutex mMessageLogRMutex;
    OsRWMutex mMessageLogWMutex;
    
    //times
    int mFirstResendTimeoutMs; //intialtimeout
    int mLastResendTimeoutMs; //timeout between last 2 resends
    int mReliableTransportTimeoutMs;//TCP timeout
    int mTransactionStateTimeoutMs;//transaction timeout
    int mDefaultExpiresSeconds; // Seconds
    int mDefaultSerialExpiresSeconds;
    int mMinInviteTransactionTimeout; // INVITE tx must live longer so the phone can ring
    int mMaxTcpSocketIdleTime; // time after which unused TCP sockets are removed
    int mMaxSrvRecords; // Max num of DNS SRV records to use before giving up
    int mDnsSrvTimeout; // second to give up & try the next DNS SRV record

    SipMessage* mpLastSipMessage;
    UtlString defaultUserAgentName;
    long mLastCleanUpTime;
    UtlString mAuthenticationScheme;
    UtlString mAuthenticationRealm;
    OsConfigDb* mpAuthenticationDb;
    OsConfigDb* mpAuthorizationUserIds;
    OsConfigDb* mpAuthorizationPasswords;
    SipLineMgr* mpLineMgr;
    int mMaxMessageLogSize;
    UtlString mMessageLog;
    UtlString mLocationHeader;
    UtlBoolean mIsUaTransactionByDefault;
    UtlBoolean mForkingEnabled;
    int mMaxForwards;
    UtlBoolean mRecurseOnlyOne300Contact;
    UtlBoolean mReturnViasForMaxForwards;
    UtlBoolean mbUseRport;
    UtlBoolean mUseRportMapping; // use Rport IP and port for contact database
    UtlBoolean mbUseLocationHeader;
    bool mbIncludePlatformInUserAgentName;  // whether or not the platform name should
                                            // be appended to the user agent name

    /** check the acceptability of method, extensions, and encoding.
     * The default is TRUE; it may be set to false in applications such as a redirect server
     * that will never actually send a 2xx response, so the checks might cause errors that
     * the application should never generate.
     */
    UtlBoolean mDoUaMessageChecks;

    void garbageCollection();

    void queueMessageToInterestedObservers(SipMessageEvent& event,
                                           const UtlString& method);
    void queueMessageToObservers(SipMessage* message,
                                 int messageType);

    //! timer that sends events to the queue periodically
    OsTimer* mpTimer;

    //! flags used during shutdown
    UtlBoolean mbShuttingDown;
    UtlBoolean mbShutdownDone;
    UtlBoolean mbBlockingShutdown;
    
    bool mbAllowHeader;
    bool mbDateHeader;
    bool mbShortNames;
    UtlString mAcceptLanguage;

    int mRegisterTimeoutSeconds;    

    //! Disabled copy constructor
    SipUserAgent(const SipUserAgent& rSipUserAgent);

    //! Disabled assignment operator
    SipUserAgent& operator=(const SipUserAgent& rhs);

};

/* ============================ INLINE METHODS ============================ */

#endif  // _SipUserAgent_h_
