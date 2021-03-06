# preserve header timestamps to preserve incremental builds across
# projects
nobase_includeHEADERS_INSTALL = $(INSTALL) -D -p -c -m 644

## If we use windows, make these conditional includes
EXTRA_DIST = \
    resparse/wnt/arpa/inet.h \
    resparse/wnt/arpa/nameser.h \
    resparse/wnt/crypt.h \
    resparse/wnt/inet_aton.h \
    resparse/wnt/netinet/in.h \
    resparse/wnt/nterrno.h \
    resparse/wnt/resolv/resolv.h \
    resparse/wnt/res_signal.h \
    resparse/wnt/sys/param.h \
    resparse/wnt/sys/uio.h \
    resparse/wnt/sysdep.h \
    resparse/wnt/utilNT.h

nobase_include_HEADERS =  \
    net/HttpBody.h \
    net/HttpMessage.h \
    net/HttpRequestContext.h \
    net/HttpServer.h \
    net/HttpService.h \
    net/MailAttachment.h \
    net/MailMessage.h \
    net/MimeBodyPart.h \
    net/NameValuePair.h \
    net/NameValuePairInsensitive.h \
    net/NetAttributeTokenizer.h \
    net/NetBase64Codec.h \
    net/NetMd5Codec.h \
    net/ProvisioningAgent.h \
    net/ProvisioningAgentXmlRpcAdapter.h \
    net/ProvisioningAttrList.h \
    net/ProvisioningClass.h \
    net/QoS.h \
    net/SdpBody.h \
    net/SdpHelper.h \
    net/SipClient.h \
    net/SipContactDb.h \
    net/SipDialog.h \
    net/SipDialogEvent.h \
    net/SipDialogMgr.h \
    net/SipDialogMonitor.h \
    net/SipConfigServerAgent.h \
    net/SipLineCredentials.h \
    net/SipLineEvent.h \
    net/SipLine.h \
    net/SipLineList.h \
    net/SipLineMgr.h \
    net/SipMessageEvent.h \
    net/SipMessage.h \
    net/SipMessageList.h \
    net/SipNonceDb.h \
    net/SipNotifyStateTask.h \
    net/SipObserverCriteria.h \
    net/SipPimClient.h \
    net/SipPresenceEvent.h \
    net/SipProtocolServerBase.h \
    net/SipPublishContentMgr.h \
    net/SipPublishServer.h \
    net/SipPublishServerEventStateCompositor.h \
    net/SipPublishServerEventStateMgr.h \
    net/SipRefreshMgr.h \
    net/SipRefreshManager.h \
    net/SipRequestContext.h \
    net/SipResourceList.h \
    net/SipServerBase.h \
    net/SipServerBroker.h \
    net/SipSession.h \
    net/SipSrvLookup.h \
    net/SipSubscribeClient.h \
    net/SipSubscribeServer.h \
    net/SipSubscribeServerEventHandler.h \
    net/SipSubscriptionMgr.h \
    net/SipTcpServer.h \
    net/SipTlsServer.h \
    net/SipTransaction.h \
    net/SipTransactionList.h \
    net/SipUdpServer.h \
    net/SipUserAgentBase.h \
    net/SipUserAgent.h \
    net/SipUserAgentStateless.h \
    net/SmimeBody.h \
    net/StateChangeNotifier.h \
    net/TapiMgr.h \
    net/Url.h \
    net/version.h \
    net/XmlRpcBody.h \
    net/XmlRpcDispatch.h \
    net/XmlRpcMethod.h \
    net/XmlRpcRequest.h \
    net/XmlRpcResponse.h \
    net/HttpConnection.h \
    net/HttpConnectionMap.h \
    net/PidfBody.h \
    resparse/bzero.h \
    resparse/ns_name.h \
    resparse/poll.h \
    resparse/res_config.h \
    resparse/res_info.h \
    resparse/rr.h \
    resparse/types.h \
    tapi/SipXHandleMap.h \
    tapi/SipXMessageObserver.h \
    tapi/SipXEventDispatcher.h \
    tapi/doxyfile \
    tapi/sipXtapi.h \
    tapi/sipXtapiEvents.h \
    tapi/sipXtapiInternal.h

net/version.h: .FORCE
	@test -d net || mkdir net
	@echo '#ifndef SIP_STACK_VERSION'             > net/version_new.h
	@echo '#define SIP_STACK_VERSION "@VERSION@"' >> net/version_new.h
	@echo '#endif'                                >> net/version_new.h
	@if test ! -r net/version.h || ! diff net/version.h net/version_new.h > /dev/null  \
	;then \
	   echo sipXtack version is @VERSION@ \
	;  mv net/version_new.h net/version.h \
	;else \
	   rm -f net/version_new.h \
	;fi

.FORCE:
