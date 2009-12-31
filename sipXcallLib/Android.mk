#
# Copyright (C) 2009 SIPfoundry Inc.
# Licensed by SIPfoundry under the LGPL license.
#
# Copyright (C) 2009 SIPez LLC.
# Licensed to SIPfoundry under a Contributor Agreement.
#
#
#//////////////////////////////////////////////////////////////////////////
#
# Author: Dan Petrie (dpetrie AT SIPez DOT com)
#
#
# This Makefile is for building sipXcallLib as a part of Android NDK.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

# Set up the target identity.
# LOCAL_MODULE/_CLASS are required for local-intermediates-dir to work.
LOCAL_MODULE := libsipXcall
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
#intermediates := $(call local-intermediates-dir)

fail_to_compile := \


LOCAL_SRC_FILES := \
    src/cp/CallManager.cpp \
    src/cp/CSeqManager.cpp \
    src/cp/Connection.cpp \
    src/cp/CpCall.cpp \
    src/cp/CpCallManager.cpp \
    src/cp/CpGatewayManager.cpp \
    src/cp/CpGhostConnection.cpp \
    src/cp/CpIntMessage.cpp \
    src/cp/CpMultiStringMessage.cpp \
    src/cp/CpPeerCall.cpp \
    src/cp/CpStringMessage.cpp \
    src/cp/SipConnection.cpp \
    src/tao/TaoEvent.cpp \
    src/tao/TaoEventListener.cpp \
    src/tao/TaoListenerEventMessage.cpp \
    src/tao/TaoMessage.cpp \
    src/tao/TaoObjectMap.cpp \
    src/tao/TaoReference.cpp \
    src/tao/TaoString.cpp \
    src/tapi/sipXtapi.cpp \
    src/tapi/SipXEventDispatcher.cpp \
    src/tapi/sipXtapiEvents.cpp \
    src/tapi/sipXtapiInternal.cpp \
    src/tapi/SipXHandleMap.cpp \
    src/tapi/SipXMessageObserver.cpp

# Not immediately needed on Android
FOO_DONT_BUILD := \


LOCAL_CXXFLAGS += -D__pingtel_on_posix__ \
                  -DANDROID \
                  -DDEFINE_S_IREAD_IWRITE \
                  -DSIPX_TMPDIR=\"/usr/var/tmp\" -DSIPX_CONFDIR=\"/etc/sipxpbx\"

#ifeq ($(TARGET_ARCH),arm)
#	LOCAL_CFLAGS += -DARMv5_ASM
#endif

#ifeq ($(TARGET_BUILD_TYPE),debug)
#	LOCAL_CFLAGS += -DDEBUG
#endif

LOCAL_C_INCLUDES += \
    $(SIPX_HOME)/libpcre \
    $(SIPX_HOME)/sipXportLib/include \
    $(SIPX_HOME)/sipXsdpLib/include \
    $(SIPX_HOME)/sipXtackLib/include \
    $(SIPX_HOME)/sipXmediaLib/include \
    $(SIPX_HOME)/sipXmediaAdapterLib/sipXmediaMediaProcessing/include \
    $(SIPX_HOME)/sipXmediaAdapterLib/interface \
    $(SIPX_HOME)/sipXcallLib/include

LOCAL_SHARED_LIBRARIES := libpcre libsipXport libsipXsdp libsipXtack libsipXmedia libsipXmediaAdapter

#LOCAL_STATIC_LIBRARIES := 

LOCAL_LDLIBS += -lstdc++ -ldl

include $(BUILD_SHARED_LIBRARY)
