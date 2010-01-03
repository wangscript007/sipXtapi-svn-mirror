// Copyright 2008 AOL LLC.
// Licensed to SIPfoundry under a Contributor Agreement.
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
// USA. 
// 
// Copyright (C) 2007 Pingtel Corp., certain elements licensed under a 
// Contributor Agreement.  Contributors retain copyright to elements 
// licensed under a Contributor Agreement.  Licensed to the User under the 
// LGPL license.
// 
// $$
//////////////////////////////////////////////////////////////////////////////

// SYSTEM INCLUDES
#include <stdlib.h>
#include <assert.h>

// APPLICATION INCLUDES
#include "include/VoiceEngineFactoryImpl.h"
#include "include/VoiceEngineBufferInStream.h"
#include "mediaBaseImpl/CpMediaNetTask.h"
#include "mediaBaseImpl/CpMediaSocketMonitorTask.h"
#include "mediaBaseImpl/CpMediaDeviceMgr.h"
#include "include/VoiceEngineSocketFactory.h"
//#include "mi/CpMediaInterfaceFactoryFactory.h"

#include "os/OsPerfLog.h"
#include "os/OsConfigDb.h"
#include "os/OsRegistry.h"
#include "os/OsSysLog.h"
#include "os/OsTimerTask.h"
#include "os/OsProtectEventMgr.h"
#include "os/OsNameDb.h"
#include "os/OsLock.h"
#include "sdp/SdpCodec.h"
#include "sdp/SdpCodecList.h"
#include "net/NetBase64Codec.h"
#include "utl/UtlSListIterator.h"

#ifdef WIN32
#include <mmsystem.h>
#endif

#ifdef __MACH__
#include <CoreAudio/AudioHardware.h>
#endif

// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
#define CONFIG_PHONESET_SEND_INBAND_DTMF  "PHONESET_SEND_INBAND_DTMF"
#define MAX_MANAGED_FLOW_GRAPHS           16
#define MAX_AUDIO_DEVICES                 16
// MACROS
#if !defined(_WIN32)
#define __declspec(x)
#endif

#ifdef _WIN32
#  ifndef IDB_CONNECTING
#    define IDB_CONNECTING                  101
#  endif
#  ifndef IDB_MISSING_CAMERA
#    define IDB_MISSING_CAMERA              102
#  endif
#endif

// STATIC VARIABLE INITIALIZATIONS
OsMutex sGuard(OsMutex::Q_FIFO);
VoiceEngineFactoryImpl* spMediaFactory = NULL;
int iMediaFactoryCount = 0;

#ifdef _WIN32
extern HANDLE hModule_sipXtapi ;
#endif

extern "C" __declspec(dllexport) IMediaDeviceMgr* createMediaDeviceMgr()
{
    OsLock lock(sGuard);
    iMediaFactoryCount++;
    if (NULL == spMediaFactory)
    {
        spMediaFactory = new VoiceEngineFactoryImpl();
    }
    return spMediaFactory;
}

extern "C" __declspec(dllexport) void releaseMediaDeviceMgr(void* pDevice)
{
    OsLock lock(sGuard);
    if (iMediaFactoryCount <= 1)
    {
        if (pDevice == spMediaFactory)
        {
            spMediaFactory = NULL;
        }
        IMediaDeviceMgr* pMediaDeviceMgr = (IMediaDeviceMgr*)pDevice;
        pMediaDeviceMgr->destroy();
        delete pMediaDeviceMgr;
    }
    iMediaFactoryCount--;
}

extern "C" __declspec(dllexport) int getReferenceCount()
{
    OsLock lock(sGuard);
    return iMediaFactoryCount;
}


// STATIC FUNCTIONS

static void VideoEngineLogCallback(GIPSTraceCallbackObject obj, char *pData, int nLength)
{
    if ((nLength > 0) && pData != NULL)
    {
#ifdef ENCODE_GIPS_LOGS
        UtlString encodedData ;        
        NetBase64Codec::encode(nLength, pData, encodedData) ;        
        OsSysLog::add(FAC_VIDEOENGINE, PRI_DEBUG, encodedData.data()) ;
#else
        OsSysLog::add(FAC_VIDEOENGINE, PRI_DEBUG, pData) ;
#endif        
    }
#ifdef GIPS_LOG_FLUSH
    OsSysLog::flush() ;
#endif
}


/* //////////////////////////// PUBLIC //////////////////////////////////// */

/* ============================ CREATORS ================================== */

// Constructor
VoiceEngineFactoryImpl::VoiceEngineFactoryImpl() :
    mCurrentWaveInDevice(-1),
    mCurrentWaveOutDevice(-1),
    mVideoQuality(0),
    mVideoBitRate(70),
    mVideoFrameRate(15),
    mCpu(40),
    mTrace(false),
    mbMute(false),
    mbEnableAGC(false),
    mRtpPortLock(OsMutex::Q_FIFO),
    mbDTMFInBand(TRUE),
    mIdleTimeout(30),
    mVideoCaptureDevice(""),
    mAECMode(MEDIA_AEC_CANCEL_AUTO),
    mNRMode(MEDIA_NOISE_REDUCTION_LOW),
    mpInStream(NULL),
    mpMediaNetTask(NULL),
    mpVideoEngine(NULL),
    mpVoiceEngine(NULL),
    mLocalConnectionId(-1),
    mAudioDeviceInput(MediaDeviceInfo::MDIT_AUDIO_INPUT),
    mAudioDeviceOutput(MediaDeviceInfo::MDIT_AUDIO_OUTPUT) 
{    
    OS_PERF_FUNC("VoiceEngineFactoryImpl::VoiceEngineFactoryImpl") ;


#ifdef _WIN32
    mhNoCameraBitmap = (HBITMAP) LoadImage((HINSTANCE) hModule_sipXtapi, "avNoCamera.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    if (mhNoCameraBitmap == NULL)
        mhNoCameraBitmap = LoadBitmap((HINSTANCE) hModule_sipXtapi, MAKEINTRESOURCE(IDB_MISSING_CAMERA)) ;
    mhConnectingBitmap = (HBITMAP) LoadImage((HINSTANCE) hModule_sipXtapi, "avConnecting.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    if (mhConnectingBitmap == NULL)
        mhConnectingBitmap = LoadBitmap((HINSTANCE) hModule_sipXtapi, MAKEINTRESOURCE(IDB_CONNECTING)) ;
#endif
}

void VoiceEngineFactoryImpl::initialize(OsConfigDb* pConfigDb, 
                         uint32_t fameSizeMs, 
                         uint32_t maxSamplesPerSec,
                         uint32_t defaultSamplesPerSec)
{
    UtlString strOOBBandDTMF ;
    mbCreateLocalConnection = true ;
    mVideoFormat = 0 ;
    mbDTMFOutOfBand = TRUE;
    mpPreviewWindowDisplay = NULL ;
    mLocalConnectionId = -1 ;
    mbLocalConnectionInUse = false ;
    mbSpeakerAdjust = false; 
    miVolume = 90 ;
    mbEnableRTCP = true ;
    mpVoiceEngine = NULL ;
    mpVideoEngine = NULL ;
    mpStaticVideoEngine = NULL ;

    mpLogger = new VoiceEngineLogger() ;
    mpVideoLogger = new VideoEngineLogger() ;

    mpMediaNetTask = CpMediaNetTask::createCpMediaNetTask() ;
    mpFactory = new VoiceEngineSocketFactory(this) ;

    OsRegistry reg;
    UtlString overrideSettings;
    UtlString temp;
    UtlString sValue;
    int value;

    if (reg.readInteger(SIPXTAPI_OVERRIDE_KEY, UtlString("audioTimeout"), value))
    {
        temp.format("audioTimeout=%d\n", value) ;
    	overrideSettings.append(temp);
    }
    if (reg.readInteger(SIPXTAPI_OVERRIDE_KEY, UtlString("videoFormat"), value))
    {
        temp.format("videoFormat=%d\n", value) ;
    	overrideSettings.append(temp);
    }
    if (reg.readInteger(SIPXTAPI_OVERRIDE_KEY, UtlString("videoFramerate"), value))
    {
        temp.format("videoFramerate=%d\n", value) ;
    	overrideSettings.append(temp);
    }
    if (reg.readInteger(SIPXTAPI_OVERRIDE_KEY, UtlString("videoBitrate"), value))
    {
        temp.format("videoBitrate=%d\n", value) ;
    	overrideSettings.append(temp);
    }
    if (reg.readInteger(SIPXTAPI_OVERRIDE_KEY, UtlString("videoQuality"), value))
    {
        temp.format("videoQuality=%d\n", value) ;
    	overrideSettings.append(temp);
    }
    if (reg.readInteger(SIPXTAPI_OVERRIDE_KEY, UtlString("videoCPU"), value))
    {
        temp.format("videoCPU=%d\n", value) ;
    	overrideSettings.append(temp);
    }
    if (reg.readInteger(SIPXTAPI_OVERRIDE_KEY, UtlString("videoQuality"), value))
    {
        temp.format("videoQuality=%d\n", value) ;
    	overrideSettings.append(temp);
    }
    if (reg.readInteger(SIPXTAPI_OVERRIDE_KEY, UtlString("consoleTrace"), value))
    {
        temp.format("consoleTrace=%d\n", value) ;
    	overrideSettings.append(temp);
    }
    if (reg.readInteger(SIPXTAPI_OVERRIDE_KEY, UtlString("enableRtcp"), value))
    {
        temp.format("enableRtcp=%d\n", value) ;
    	overrideSettings.append(temp);
    }
    if (reg.readInteger(SIPXTAPI_OVERRIDE_KEY, UtlString("ignoreCameraCaps"), value))
    {
        temp.format("ignoreCameraCaps=%d\n", value) ;
    	overrideSettings.append(temp);
    }
    if (reg.readInteger(SIPXTAPI_OVERRIDE_KEY, UtlString("mediaContactType"), value))
    {
        temp.format("mediaContactType=%d\n", value) ;
    	overrideSettings.append(temp);
    }
    if (reg.readString(SIPXTAPI_OVERRIDE_KEY, UtlString("videoCodecs"), sValue))
    {
        temp.format("videoCodecs=%s\n", sValue.data()) ;
    	overrideSettings.append(temp);
    }
    if (reg.readString(SIPXTAPI_OVERRIDE_KEY, UtlString("videoCodecOrder"), sValue))
    {
        temp.format("videoCodecOrder=%s\n", sValue.data()) ;
    	overrideSettings.append(temp);
    }
    if (reg.readString(SIPXTAPI_OVERRIDE_KEY, UtlString("audioCodecs"), sValue))
    {
        temp.format("audioCodecs=%s\n", sValue.data()) ;
    	overrideSettings.append(temp);
    }
    if (reg.readString(SIPXTAPI_OVERRIDE_KEY, UtlString("audioCodecOrder"), sValue))
    {
        temp.format("audioCodecOrder=%s\n", sValue.data()) ;
    	overrideSettings.append(temp);
    }
    OsSysLog::add(FAC_MP, PRI_NOTICE, "Overrides:\n%s", overrideSettings.data()) ;
}

void VoiceEngineFactoryImpl::setSysLogHandler(OsSysLogHandler sysLogHandler) 
{
    OsSysLog::initialize(sysLogHandler) ;
}

void VoiceEngineFactoryImpl::destroy()
{
}

// Destructor
VoiceEngineFactoryImpl::~VoiceEngineFactoryImpl()
{
    OS_PERF_FUNC("VoiceEngineFactoryImpl::~VoiceEngineFactoryImpl") ;
    OsLock lock(sGuard) ;
    
    stopPlay() ;
    destroyLocalAudioConnection() ;
    shutdownCpMediaNetTask() ;
    OsTask::yield() ;       

    if (mpVideoEngine)
    {
        releaseVideoEngineInstance(mpVideoEngine) ;
        mpVideoEngine = NULL;
    }

    if (mpVoiceEngine)
    {
        releaseVoiceEngineInstance(mpVoiceEngine) ;
        mpVoiceEngine = NULL;
    }

	delete mpLogger;
	mpLogger = NULL;
    delete mpVideoLogger ;
    mpLogger = NULL ;

    if (mpPreviewWindowDisplay)
    {
        delete (SIPXVE_VIDEO_DISPLAY*) mpPreviewWindowDisplay;
        mpPreviewWindowDisplay = NULL;
    }

    if (mpStaticVideoEngine)
    {
        mpStaticVideoEngine->GIPSVideo_Terminate() ;
#ifdef USE_GIPS
# ifdef _WIN32
        DeleteGipsVideoEngine(mpStaticVideoEngine) ;
# else
        delete mpStaticVideoEngine;
# endif
#else
        DeleteGipsVideoEngineStub(mpStaticVideoEngine) ;        
#endif
        mpStaticVideoEngine = NULL ;
    }

    if (mpFactory)
    {
        delete mpFactory ;
        mpFactory = NULL ;
    }

    CpMediaSocketMonitorTask::releaseInstance() ;
    OsNatAgentTask::releaseInstance();
    OsTimerTask::destroyTimerTask() ;
    OsProtectEventMgr::releaseEventMgr() ;
    OsSysLog::shutdown() ;
    OsNameDb::release() ;
}

//////////////////////////////////////////////////////////////////////////////
OsStatus VoiceEngineFactoryImpl::translateToneId(const SIPX_TONE_ID toneId,
                                             SIPX_TONE_ID&      xlateId ) const
{
    OsStatus rc = OS_SUCCESS;
    if (toneId >= '0' && toneId <= '9')
    {
        xlateId = (SIPX_TONE_ID)(toneId - '0');
    } 
    else if (toneId == ID_DTMF_STAR)
    {
        xlateId = (SIPX_TONE_ID)10;
    }
    else if (toneId == ID_DTMF_POUND)
    {
        xlateId = (SIPX_TONE_ID)11;
    }
    else if (toneId == ID_DTMF_FLASH)
    {
        xlateId = (SIPX_TONE_ID)16;
    }
    else
    {
        rc = OS_FAILED;
    }
    return rc;
}

void VoiceEngineFactoryImpl::doEnableLocalChannel(bool bEnable) const
{
    OS_PERF_FUNC("VoiceEngineFactoryImpl::doEnableLocalChannel") ;
    OsLock lock(sGuard) ;
    int rc ;

    if (bEnable)
    {
        if (mLocalConnectionId == -1)
        {
            constructGlobalInstance() ;
            assert(mbLocalConnectionInUse == false) ;
            assert(mpVoiceEngine != NULL) ;
            mLocalConnectionId = mpVoiceEngine->getBase()->GIPSVE_CreateChannel() ;    
            rc = mpVoiceEngine->getBase()->GIPSVE_StartPlayout(mLocalConnectionId) ;
            assert(rc == 0);
        }
    }
    else
    {
        if (mLocalConnectionId != -1)
        {
            assert(mpVoiceEngine != NULL) ;
            rc = mpVoiceEngine->getBase()->GIPSVE_DeleteChannel(mLocalConnectionId) ;
            assert(rc == 0);
            mLocalConnectionId = -1 ;
            mbLocalConnectionInUse = false ;
        }
    }
}



OsStatus VoiceEngineFactoryImpl::createLocalAudioConnection()
{
    OS_PERF_FUNC("VoiceEngineFactoryImpl::createLocalAudioConnection") ;
    OsLock lock(sGuard) ;
    OsStatus rc = OS_SUCCESS;

    mbCreateLocalConnection = true ;
    doEnableLocalChannel(true) ;
    return rc;
}

OsStatus VoiceEngineFactoryImpl::destroyLocalAudioConnection()
{
    OS_PERF_FUNC("VoiceEngineFactoryImpl::destroyLocalAudioConnection") ;
    OsLock lock(sGuard) ;
    OsStatus rc = OS_SUCCESS;

    mbCreateLocalConnection = false ;
    doEnableLocalChannel(false) ;

    return rc;
}

/* ============================ MANIPULATORS ============================== */
IMediaInterface* VoiceEngineFactoryImpl::createMediaInterface( const char* publicAddress,
                                                                const char* localAddress,
                                                                int numCodecs,
                                                                SdpCodec* sdpCodecArray[],
                                                                const char* locale,
                                                                int expeditedIpTos,
                                                                const ProxyDescriptor& stunServer,
                                                                const ProxyDescriptor& turnProxy,
                                                                const ProxyDescriptor& arsProxy,
                                                                const ProxyDescriptor& arsHttpProxy,
                                                                SIPX_MEDIA_PACKET_CALLBACK pMediaPacketCallback,
                                                                UtlBoolean enableIce,
                                                                uint32_t samplesPerSec) 
{
    OS_PERF_FUNC("VoiceEngineFactoryImpl::createMediaInterface") ;

    OsLock lock(sGuard) ;
    VoiceEngine* pVoiceEngine = NULL ;
    GipsVideoEnginePlatform* pVideoEngine = NULL ;
    
    if (mbLocalConnectionInUse)
    {
        pVoiceEngine = getNewVoiceEngineInstance() ;
        pVideoEngine = getNewVideoEngineInstance(pVoiceEngine) ;
    }
    else
    {
        doEnableLocalChannel(false) ;
        constructGlobalInstance(true, (mpPreviewWindowDisplay != NULL || pMediaPacketCallback)) ;
        pVoiceEngine = mpVoiceEngine ;
        mpVoiceEngine = NULL ;
        pVideoEngine = mpVideoEngine ;
        mpVideoEngine = NULL ;
    }

    UtlBoolean enableRtcp = mbEnableRTCP ;
    applyEnableRTCPOverride(enableRtcp) ;

    VoiceEngineMediaInterface* pMediaInterface = 
            new VoiceEngineMediaInterface(this, pVoiceEngine, 
            pVideoEngine,     
            publicAddress, localAddress, 
            numCodecs, sdpCodecArray, locale, expeditedIpTos, 
            stunServer, turnProxy,
            arsProxy, arsHttpProxy,                
            mbDTMFOutOfBand, mbDTMFInBand, enableRtcp, mRtcpName,
            pMediaPacketCallback) ;
    
    // store it in our internal list, as an int
    UtlInt* piMediaInterface = new UtlInt((int) pMediaInterface);
    mInterfaceList.insert(piMediaInterface);  

    return pMediaInterface; 
}


void VoiceEngineFactoryImpl::releaseInterface(VoiceEngineMediaInterface* pMediaInterface)
{
    OS_PERF_FUNC("VoiceEngineFactoryImpl::releaseInterface") ;
    OsLock lock(sGuard) ;
    bool bReleasedVideo = false ;

    assert(pMediaInterface != NULL) ;
    if (pMediaInterface)
    {        
        mInterfaceList.destroy(& UtlInt((int)pMediaInterface));
        if (pMediaInterface->getVideoEnginePtr())
        {
            GipsVideoEnginePlatform* pVideoEngine = (GipsVideoEnginePlatform*)pMediaInterface->getVideoEnginePtr() ;
            if (pVideoEngine)
            {
                releaseVideoEngineInstance(pVideoEngine) ;
                bReleasedVideo = true ;
            }
        }
        if (pMediaInterface->getAudioEnginePtr())
        {
            VoiceEngine* pVoiceEngine = (VoiceEngine*)pMediaInterface->getAudioEnginePtr() ;
            if (pVoiceEngine)
            {
                releaseVoiceEngineInstance(pVoiceEngine) ;
            }
        }
    }
}

// WARNING:: Assumes someone is externally holding a lock!!
VoiceEngine* VoiceEngineFactoryImpl::getNewVoiceEngineInstance() const
{
    OS_PERF_FUNC("VoiceEngineFactoryImpl::getNewVoiceEngineInstance") ;
    int rc ;
    VoiceEngine* pVoiceEngine = new VoiceEngine(GIPSVoiceEngine::Create()) ;

    if (pVoiceEngine)
    {
        // Initialize GIPS Debug Tracing
        if (getGipsTracing())
        {
            rc = pVoiceEngine->getBase()->GIPSVE_SetObserver(*mpLogger, false) ;
            assert(rc == 0) ;
            rc = pVoiceEngine->getBase()->GIPSVE_SetTraceFilter(0x00FF) ;   // All non-encrypted info
            assert(rc == 0) ;
        }

#ifdef USE_GIPS_DLL
        char* szKey = (char*) GIPS_KEY ;
        rc = pVoiceEngine->GIPSVE_Authenticate(szKey, strlen(szKey));
        assert(rc == 0) ;
#endif

#ifdef USE_GIPS
        rc = pVoiceEngine->getBase()->GIPSVE_Init(GIPS_EXPIRE_MONTH, GIPS_EXPIRE_DAY, GIPS_EXPIRE_YEAR, false) ;
        assert(rc == 0) ;
#endif

        rc = pVoiceEngine->getHardware()->GIPSVE_SetSoundDevices(mCurrentWaveInDevice, mCurrentWaveOutDevice);
        OsSysLog::add(FAC_MP, PRI_INFO, 
                      "VoiceEngineFactoryImpl:: getNewVoiceEngineInstance GIPSVE_SetSoundDevices  input: %d, output: %d, rc: %d", 
                      mCurrentWaveInDevice, mCurrentWaveOutDevice, rc); 

        assert(rc == 0) ;
        rc = 0 ;

        // Save the device info last used
        UtlString results ;
        inputDeviceIndexToString(results, mCurrentWaveInDevice) ;
        mAudioDeviceInput.setSelected(results) ;
        outputDeviceIndexToString(results, mCurrentWaveOutDevice) ;
        mAudioDeviceOutput.setSelected(results) ;
        
#  if defined (_WIN32)
        if (!isSpeakerAdjustSet())
        {
            rc = pVoiceEngine->getExternalMedia()->GIPSVE_SetExternalMediaProcessing(
				PLAYBACK_ALL_CHANNELS_MIXED,
				-1,
				true,
				(GIPSVEMediaProcess&) *this) ;
            assert(rc == 0) ;
        }
 #endif

        // Set Other Attributes
        doSetAudioAECMode(pVoiceEngine, mAECMode) ;
        doSetAudioNoiseReductionMode(pVoiceEngine, mNRMode) ;
        doEnableAGC(pVoiceEngine, mbEnableAGC) ;        
    }

    return pVoiceEngine ;
}

// WARNING:: Assumes someone is externally holding a lock!!
void VoiceEngineFactoryImpl::releaseVoiceEngineInstance(VoiceEngine* pVoiceEngine)
{
    OS_PERF_FUNC("VoiceEngineFactoryImpl::releaseVoiceEngineInstance") ;
    int rc ;

    assert(pVoiceEngine != NULL) ;
    if (pVoiceEngine)
    {
        pVoiceEngine->getBase()->GIPSVE_SetTraceFilter(TR_NONE) ;
        pVoiceEngine->getBase()->GIPSVE_SetObserver(*mpLogger, false) ;
        rc = pVoiceEngine->getExternalMedia()->GIPSVE_SetExternalMediaProcessing(PLAYBACK_ALL_CHANNELS_MIXED, -1, false, *this) ;            
        assert(rc == 0) ;
        rc = pVoiceEngine->getBase()->GIPSVE_Terminate();
        assert(rc == 0) ;

        delete pVoiceEngine;
    }
}


// WARNING:: Assumes someone is externally holding a lock!!
VoiceEngine* VoiceEngineFactoryImpl::getAnyVoiceEngine() const
{       
    VoiceEngine* pRC = NULL ;

    // First: Try global instance
    if (mpVoiceEngine)
        pRC = mpVoiceEngine ;

    // Next: Session
    if (pRC == NULL)                
    {
        UtlSListIterator iterator(mInterfaceList) ;
        UtlInt* pInterfaceInt ;
        while (pInterfaceInt = (UtlInt*) iterator())
        {
            VoiceEngineMediaInterface* pInterface = 
                    (VoiceEngineMediaInterface*) pInterfaceInt->getValue();            
            if (pInterface)
            {
                if (pInterface->getAudioEnginePtr())
                {
                    pRC = (VoiceEngine*)pInterface->getAudioEnginePtr();
                }
                break ;
            }
        }
    }

    // Last: Build it
    if (pRC == NULL)
    {
        constructGlobalInstance() ;
        pRC = mpVoiceEngine ;
    }

    return pRC ;
}

// WARNING:: Assumes someone is externally holding a lock!!
GipsVideoEnginePlatform* VoiceEngineFactoryImpl::getAnyVideoEngine() const 
{
    GipsVideoEnginePlatform* pRC = NULL ;

    // First: Try global instance
    if (mpVideoEngine)
        pRC = mpVideoEngine ;

    // Next: Session
    if (pRC == NULL)                
    {
        UtlSListIterator iterator(mInterfaceList) ;
        UtlInt* pInterfaceInt ;
        while (pInterfaceInt = (UtlInt*) iterator())
        {
            VoiceEngineMediaInterface* pInterface = 
                    (VoiceEngineMediaInterface*) pInterfaceInt->getValue();            
            if (pInterface && pInterface->getVideoEnginePtr())
            {
                pRC = (GipsVideoEnginePlatform*)pInterface->getVideoEnginePtr() ;
                break ;
            }
        }
    }

    // Last: Build it
    if (pRC == NULL)
    {
        constructGlobalInstance(false, true) ;
        pRC = mpVideoEngine ;
    }

    return pRC ;
}

// WARNING:: Assumes someone is externally holding a lock!!
GipsVideoEnginePlatform* VoiceEngineFactoryImpl::getNewVideoEngineInstance(VoiceEngine* pVoiceEngine) const
{    
    OS_PERF_FUNC("VoiceEngineFactoryImpl::getNewVideoEngineInstance") ;
    GipsVideoEnginePlatform* pVideoEngine = NULL ;
    int rc ;

    assert(pVoiceEngine != NULL) ;
    if (pVoiceEngine)
    {
        pVideoEngine = GetNewVideoEngine();

        if (getGipsTracing())
        {
            pVideoEngine->GIPSVideo_SetTraceFilter(0x00FF) ;   // All non-encrypted info
            rc = pVideoEngine->GIPSVideo_SetTraceCallback(mpVideoLogger) ;
            assert(rc == 0) ;
        }

#ifdef USE_GIPS_DLL
        char* szKey = (char*) GIPS_KEY ;
        rc = pVideoEngine->GIPSVideo_Authenticate(szKey, strlen(szKey));
        assert(rc == 0);
#endif

#ifdef USE_GIPS
        rc = pVideoEngine->GIPSVideo_Init(pVoiceEngine->getVE(), GIPS_EXPIRE_MONTH, GIPS_EXPIRE_DAY, GIPS_EXPIRE_YEAR) ;
        assert(rc == 0) ;
#endif 
    }

    return pVideoEngine ;
}


// WARNING:: Assumes someone is externally holding a lock!!
void VoiceEngineFactoryImpl::releaseVideoEngineInstance(GipsVideoEnginePlatform* pVideoEngine) 
{
    OS_PERF_FUNC("VoiceEngineFactoryImpl::releaseVideoEngineInstance") ;
    int rc ;

    assert(pVideoEngine != NULL) ;
    if (pVideoEngine)
    {
        rc = pVideoEngine->GIPSVideo_SetTraceFilter(TR_NONE) ;
        assert(rc == 0) ;
        OS_PERF_ADD("GIPSVideo_SetTrace") ;
        rc = pVideoEngine->GIPSVideo_Terminate() ;
        OS_PERF_ADD("GIPSVideo_Terminate") ;
        assert(rc == 0) ;

#ifdef USE_GIPS
# ifdef _WIN32
        DeleteGipsVideoEngine(pVideoEngine) ;
# else
        delete pVideoEngine ;
# endif
#else
        DeleteGipsVideoEngineStub(pVideoEngine) ;
#endif 
        // OS_PERF_ADD("(delete)") ;
    }
}


OsStatus VoiceEngineFactoryImpl::enableSpeakerVolumeAdjustment(bool bEnable) 
{
    OS_PERF_FUNC("VoiceEngineFactoryImpl::enableSpeakerVolumeAdjustment") ;
    OsLock lock(sGuard) ;

    if (mbSpeakerAdjust != bEnable)
    {
        mbSpeakerAdjust = bEnable ;

        if (mpVoiceEngine)
        {
			mpVoiceEngine->getExternalMedia()->GIPSVE_SetExternalMediaProcessing(
                     PLAYBACK_ALL_CHANNELS_MIXED, 
                     -1, 
                    !mbSpeakerAdjust, 
                     *this) ;
        }
    }
    return OS_SUCCESS ;
}


OsStatus VoiceEngineFactoryImpl::setSpeakerVolume(int iVolume) 
{
    OS_PERF_FUNC("VoiceEngineFactoryImpl::setSpeakerVolume") ;

    OsLock lock(sGuard) ;
    OsStatus rc = OS_FAILED ;    
    
    if (iVolume < 0 || iVolume > 100)
    {
        rc = OS_FAILED;
    }
    else 
    {
        if (mbSpeakerAdjust)
        {            
            VoiceEngine* pVoiceEngine = getAnyVoiceEngine() ;
            if (pVoiceEngine)
            {            
                int gipsVolume = (int) (((float)iVolume / 100.0) * 255.0 );
                if (0 == pVoiceEngine->getVolumeControl()->GIPSVE_SetSpeakerVolume(gipsVolume))
                    rc = OS_SUCCESS ;
            }
        }
        else
        {
            miVolume = iVolume ;
            rc = OS_SUCCESS ;
        }
    }

    return rc ;
}


OsStatus VoiceEngineFactoryImpl::setSpeakerDevice(const UtlString& device) 
{
    OS_PERF_FUNC("VoiceEngineFactoryImpl::setSpeakerDevice") ;
    OsLock lock(sGuard) ;
    OsStatus rc = OS_FAILED;

    UtlSListIterator iterator(mInterfaceList);
    VoiceEngineMediaInterface* pInterface = NULL;
    UtlInt* pInterfaceInt = NULL;

    mAudioDeviceOutput.reset() ;
    
    if (outputDeviceStringToIndex(mCurrentWaveOutDevice, device) == OS_SUCCESS)
    {
        mAudioDeviceOutput.setRequested(device) ;

        if (mpVoiceEngine)        
        {
            int iRC = mpVoiceEngine->getHardware()->GIPSVE_SetSoundDevices(mCurrentWaveInDevice, mCurrentWaveOutDevice) ;
            OsSysLog::add(FAC_MP, PRI_INFO,
                    "VoiceEngineFactoryImpl::setSpeakerDevice GIPSVE_SetSoundDevices [GLOBAL] device: %s, input: %d, output: %d, rc: %d", 
		            (const char*) device, mCurrentWaveInDevice, mCurrentWaveOutDevice, iRC);
            if (iRC == 0)
            {
                rc = OS_SUCCESS;
            }
        }

        while (pInterfaceInt = (UtlInt*)iterator())
        {
            pInterface = (VoiceEngineMediaInterface*)pInterfaceInt->getValue();
            VoiceEngine* pGips = NULL;
            
            if (pInterface && pInterface->getAudioEnginePtr())
            {
                pGips = (VoiceEngine*)pInterface->getAudioEnginePtr();            
                if (pGips)
                {
                    int iRC = mpVoiceEngine->getHardware()->GIPSVE_SetSoundDevices(mCurrentWaveInDevice, mCurrentWaveOutDevice) ;
                    OsSysLog::add(FAC_MP, PRI_INFO,
                            "VoiceEngineFactoryImpl::setSpeakerDevice GIPSVE_SetSoundDevices [CONN] device: %s, input: %d, output: %d, rc: %d", 
		                    (const char*) device, mCurrentWaveInDevice, mCurrentWaveOutDevice, iRC);
                    if (iRC == 0)
                    {
                        rc = OS_SUCCESS;
                    }
                }
            }
        } 
    }
    
    return rc ;    
}


OsStatus VoiceEngineFactoryImpl::setMicrophoneGain(int iGain) 
{
    OS_PERF_FUNC("VoiceEngineFactoryImpl::setMicrophoneGain") ;
    OsLock lock(sGuard) ;
    OsStatus rc = OS_FAILED ;
    
    if (0 == iGain)
    {
        rc  = muteMicrophone(true);
    }
    else
    {    
        VoiceEngine* pVoiceEngine = getAnyVoiceEngine() ;
        if (pVoiceEngine)
        {            
            int gipsGain = (int)((float)((float)iGain / 100.0) * 255.0);                
            if (iGain < 0 || iGain > 100)
            {
                rc = OS_FAILED;
            }
            else if (0 == pVoiceEngine->getVolumeControl()->GIPSVE_SetMicVolume(gipsGain))
            {
                miGain = gipsGain;
                rc = OS_SUCCESS;
            }
        }
    }
    return rc ;
}

OsStatus VoiceEngineFactoryImpl::setMicrophoneDevice(const UtlString& device) 
{
    OS_PERF_FUNC("VoiceEngineFactoryImpl::setMicrophoneDevice") ;
    OsLock lock(sGuard) ;
    OsStatus rc = OS_FAILED;
    UtlSListIterator iterator(mInterfaceList);
    VoiceEngineMediaInterface* pInterface = NULL;
    UtlInt* pInterfaceInt = NULL;

    mAudioDeviceInput.reset() ;
   
    assert(device.length() > 0);
    
    if (OS_SUCCESS == inputDeviceStringToIndex(mCurrentWaveInDevice, device))
    {
        mAudioDeviceInput.setRequested(device) ;

        if (mpVoiceEngine)
        {
            int iRC = mpVoiceEngine->getHardware()->GIPSVE_SetSoundDevices(mCurrentWaveInDevice, mCurrentWaveOutDevice) ;                      
            OsSysLog::add(FAC_MP, PRI_INFO, 
                          "VoiceEngineFactoryImpl::setMicrophoneDevice GIPSVE_SetSoundDevices [GLOBAL] device: %s, input: %d, output: %d, rc:  %d", 
		                  (const char*) device, mCurrentWaveInDevice, mCurrentWaveOutDevice, iRC); 

            if (iRC == 0)
            {
                rc = OS_SUCCESS ;
            }
            
        }

        while (pInterfaceInt = (UtlInt*)iterator())
        {
            pInterface = (VoiceEngineMediaInterface*)pInterfaceInt->getValue();
            VoiceEngine* pGips = NULL;
            
            if (pInterface && pInterface->getAudioEnginePtr())
            {
                pGips = (VoiceEngine*)pInterface->getAudioEnginePtr();            
                if (pGips)
                {
                    int iRC = pGips->getHardware()->GIPSVE_SetSoundDevices(mCurrentWaveInDevice, mCurrentWaveOutDevice) ;
                    OsSysLog::add(FAC_MP, PRI_INFO, 
		                    "VoiceEngineFactoryImpl::setMicrophoneDevice GIPSVE_SetSoundDevices [CONN] device: %s, input: %d, output: %d, rc=%d", 
		                    (const char*) device, mCurrentWaveInDevice, mCurrentWaveOutDevice, iRC); 
                    if (iRC == 0)
                    {
                        rc = OS_SUCCESS;
                    }
                }
            }
        } 
    }
        
    return rc ;    
}


OsStatus VoiceEngineFactoryImpl::muteMicrophone(UtlBoolean bMute) 
{    

    OS_PERF_FUNC("VoiceEngineFactoryImpl::muteMicrophone") ;
    // OsLock lock(sGuard) ;
    OsStatus rc = OS_SUCCESS;
    
    mbMute = (bMute==TRUE) ? true : false;
    
    UtlSListIterator iterator(mInterfaceList);
    UtlInt* iMediaInterface = NULL;
    while ((iMediaInterface = (UtlInt*)iterator()))
    {
        VoiceEngineMediaInterface* pInterface = (VoiceEngineMediaInterface*)iMediaInterface->getValue();
        rc = pInterface->muteMicrophone((bMute==TRUE) ? true : false);
        if (OS_FAILED == rc)
        {
            break;
        }
    }

    return rc ;
}


OsStatus VoiceEngineFactoryImpl::setAudioAECMode(const MEDIA_AEC_MODE mode)
{
    OS_PERF_FUNC("VoiceEngineFactoryImpl::setAudioAECMode") ;
    OsLock lock(sGuard) ;
    OsStatus rc = OS_SUCCESS ;

    if (mAECMode != mode)
    {

        if (mpVoiceEngine)
            doSetAudioAECMode(mpVoiceEngine, mode) ;

        // iterate through all of the VoiceEngineMedia interface pointers,
        // and set AEC for all of them
        UtlSListIterator iterator(mInterfaceList);
        VoiceEngineMediaInterface* pInterface = NULL;
        UtlInt* pInterfaceInt = NULL;        
        while (pInterfaceInt = (UtlInt*)iterator())
        {
            pInterface = (VoiceEngineMediaInterface*)pInterfaceInt->getValue();            
            if (pInterface && pInterface->getAudioEnginePtr())
            {
                doSetAudioAECMode((VoiceEngine*)pInterface->getAudioEnginePtr(), mode) ;
            }
        }
    }

    return rc;
}


OsStatus VoiceEngineFactoryImpl::setAudioNoiseReductionMode(const MEDIA_NOISE_REDUCTION_MODE mode)
{
    OS_PERF_FUNC("VoiceEngineFactoryImpl::setAudioNoiseReductionMode") ;
    OsLock lock(sGuard) ;
    OsStatus rc = OS_SUCCESS ;

    mNRMode = mode ;

    if (mpVoiceEngine)
        doSetAudioNoiseReductionMode(mpVoiceEngine, mode) ;

    // iterate through all of the VoiceEngineMedia interface pointers,
    // and set AEC for all of them
    UtlSListIterator iterator(mInterfaceList);
    VoiceEngineMediaInterface* pInterface = NULL;
    UtlInt* pInterfaceInt = NULL;
    
    while (pInterfaceInt = (UtlInt*)iterator())
    {
        pInterface = (VoiceEngineMediaInterface*)pInterfaceInt->getValue();            
        if (pInterface && pInterface->getAudioEnginePtr())
        {
            doSetAudioNoiseReductionMode((VoiceEngine*)pInterface->getAudioEnginePtr(), mode) ;           
        }
    }

    return rc;
}

OsStatus VoiceEngineFactoryImpl::enableAGC(UtlBoolean bEnable)
{    
    OS_PERF_FUNC("VoiceEngineFactoryImpl::enableAGC") ;
    OsLock lock(sGuard) ;
    OsStatus rc = OS_SUCCESS ;

    if (mbEnableAGC != bEnable)
    {        
        mbEnableAGC = bEnable ;

        // Set the mode on the global instance
        if (mpVoiceEngine)
            doEnableAGC(mpVoiceEngine, bEnable) ;

        // iterate through all of the VoiceEngineMedia interface pointers,
        // and set AEC for all of them
        UtlSListIterator iterator(mInterfaceList);
        VoiceEngineMediaInterface* pInterface = NULL;
        UtlInt* pInterfaceInt = NULL;
            
        while (pInterfaceInt = (UtlInt*)iterator())
        {
            pInterface = (VoiceEngineMediaInterface*)pInterfaceInt->getValue();            
            if (pInterface && pInterface->getAudioEnginePtr())
            {
                doEnableAGC((VoiceEngine*)pInterface->getAudioEnginePtr(), bEnable) ;
            }
        }
    }

    return rc ;
}

void VoiceEngineFactoryImpl::constructGlobalInstance(bool bNoLocalChannel, bool bStartVideo) const
{    
    OS_PERF_FUNC("VoiceEngineFactoryImpl::constructGlobalInstance") ;
    OsLock lock(sGuard) ;

    if (mpVoiceEngine == NULL)
    {
        char szVersion[4096];

        mpVoiceEngine = getNewVoiceEngineInstance() ;
		mpVoiceEngine->getBase()->GIPSVE_GetVersion(szVersion, 4096) ;
        OsSysLog::add(FAC_MP, PRI_INFO, szVersion) ;

        if (mbCreateLocalConnection && !bNoLocalChannel)
            doEnableLocalChannel(true) ;
    }

    if (mpVideoEngine == NULL && bStartVideo)
    {
        char szVersion[4096];

        assert(mpVideoEngine == NULL) ;
        mpVideoEngine = getNewVideoEngineInstance(mpVoiceEngine) ;
        mpVideoEngine->GIPSVideo_GetVersion(szVersion, 4096) ;
        OsSysLog::add(FAC_MP, PRI_INFO, szVersion) ;                
    }
}


OsStatus VoiceEngineFactoryImpl::enableOutOfBandDTMF(UtlBoolean bEnable)
{
    mbDTMFOutOfBand = bEnable;

    return OS_SUCCESS;
}

OsStatus VoiceEngineFactoryImpl::enableInBandDTMF(UtlBoolean bEnable)
{
    mbDTMFInBand = bEnable;

    return OS_SUCCESS;
}

OsStatus VoiceEngineFactoryImpl::enableRTCP(UtlBoolean bEnable)
{
    mbEnableRTCP = bEnable ;

    return OS_SUCCESS ;
}

OsStatus VoiceEngineFactoryImpl::setRTCPName(const char* szName)
{
    if (szName)
        mRtcpName = szName ;
    else
        mRtcpName = "user1@undefined" ;

    return OS_SUCCESS ;
}


static int sipxtapiVideoFormatToSdpFormat(SIPX_VIDEO_FORMAT formatId)
{
    int rc = 0 ;
    switch (formatId)
    {
        case VIDEO_FORMAT_CIF:
            rc = SDP_VIDEO_FORMAT_CIF ;
            break ;
        case VIDEO_FORMAT_QCIF:
            rc = SDP_VIDEO_FORMAT_QCIF ;
            break ;
        case VIDEO_FORMAT_SQCIF:
            rc = SDP_VIDEO_FORMAT_SQCIF ;
            break ;
        case VIDEO_FORMAT_QVGA:
            rc = SDP_VIDEO_FORMAT_QVGA ;
            break ;
        case VIDEO_FORMAT_VGA:
            rc = SDP_VIDEO_FORMAT_VGA ;
            break ;
        case VIDEO_FORMAT_4CIF:
            rc = SDP_VIDEO_FORMAT_4CIF ;
            break ;
        case VIDEO_FORMAT_16CIF:
            rc = SDP_VIDEO_FORMAT_16CIF ;
            break ;
        default:
            assert(false) ;
    }
    return rc ;
}

static bool shouldAllowFormat(int sdpFormat, int bitrate)
{
    bool bShouldAllow = false ;

    switch (sdpFormat)
    {
#ifdef INCLUDE_4CIF_RESOLUTION
        case SDP_VIDEO_FORMAT_4CIF:
            bShouldAllow = (bitrate >= 200) ;
            break ;
#endif
#ifdef INCLUDE_16CIF_RESOLUTION
        case SDP_VIDEO_FORMAT_16CIF:
            bShouldAllow = (bitrate >= 200) ;
            break ;
#endif
#ifdef INCLUDE_VGA_RESOLUTION
        case SDP_VIDEO_FORMAT_VGA:
            bShouldAllow = (bitrate >= 70) ;
            break ;
#endif
#ifdef INCLUDE_CIF_RESOLUTION
        case SDP_VIDEO_FORMAT_CIF:
            bShouldAllow = (bitrate >= 60) ;
            break ;
#endif
#ifdef INCLUDE_QVGA_RESOLUTION
        case SDP_VIDEO_FORMAT_QVGA:
            bShouldAllow = (bitrate >= 60) ;
            break ;
#endif
#ifdef INCLUDE_QCIF_RESOLUTION
        case SDP_VIDEO_FORMAT_QCIF:
            bShouldAllow = (bitrate >= 10) ;
            break ;
#endif
#ifdef INCLUDE_SQCIF_RESOLUTION
        case SDP_VIDEO_FORMAT_SQCIF:
            bShouldAllow = (bitrate >= 5) ;
            break ;
#endif

        default:
            break ;
    }
    return bShouldAllow ;
}



OsStatus VoiceEngineFactoryImpl::buildCodecFactory(SdpCodecList*    pFactory, 
                                                   const UtlString& sAudioPreferences, 
                                                   const UtlString& sVideoPreferences,
                                                   int videoFormat,
                                                   int* iRejected)
{
    OsLock lock(sGuard) ;

    if (videoFormat < 0)
        videoFormat = mVideoFormat ; 
    else
        mVideoFormat = videoFormat ;

    OsStatus rc = OS_FAILED;
    int iCodecs = 0;
    UtlString codec;
    UtlString codecList;
    static bool sbDumpedCodecs = false ;

    applyVideoFormatOverride(videoFormat) ;

    *iRejected = 0;
    if (pFactory)
    {
        pFactory->clearCodecs();
                 
        // add preferred codecs first
        applyAudioCodecListOverride((UtlString&)sAudioPreferences) ;
        if (sAudioPreferences.length() > 0)
        {
            UtlString audioPreferences = sAudioPreferences;
            *iRejected = pFactory->addCodecs(audioPreferences);
            rc = OS_SUCCESS;
        }
        else
        {
            codecList = DEFAULT_VOICEENGINE_CODEC_LIST ;
            *iRejected += pFactory->addCodecs(codecList);
            rc = OS_SUCCESS;
        }

        // For now include all video codecs
        rc = OS_FAILED ;
        codecList = "";
        // add preferred codecs first
        applyVideoCodecListOverride((UtlString&)sVideoPreferences) ;

        if (sVideoPreferences.length() > 0)
        {
            UtlString videoPreferences = sVideoPreferences;
            *iRejected = pFactory->addCodecs(videoPreferences);
            rc = OS_SUCCESS;

            // Filter out unwanted video codecs
            if (videoFormat != -1)
            {
                int numVideoCodecs;
                SdpCodec** videoCodecsArray = NULL;

                pFactory->getCodecs(numVideoCodecs, videoCodecsArray, "video");
                for (int codecIndex = 0; codecIndex<numVideoCodecs; codecIndex++)
                {
                    int format = videoCodecsArray[codecIndex]->getVideoFormat() ;

                    if ((sipxtapiVideoFormatToSdpFormat((SIPX_VIDEO_FORMAT) videoFormat) < format)
                        || !shouldAllowFormat(format, mVideoBitRate))
                    {
                        pFactory->removeCodecByType(videoCodecsArray[codecIndex]->getCodecType()) ;
                    }

                    delete videoCodecsArray[codecIndex];
                    videoCodecsArray[codecIndex] = NULL;
                }                
                delete[] videoCodecsArray;
            }
        }
        else
        {                
            codecList = DEFAULT_VIDEOENGINE_CODEC_LIST ;

            // Strip out disabled codecs
            switch (videoFormat)
            {                
                case 2:     // SQCIF
                    SdpCodecList::removeKeywordFromCodecList("-QCIF", codecList) ;
                case 1:     // QCIF
                    SdpCodecList::removeKeywordFromCodecList("-QVGA", codecList) ;
                case 3:     // QVGA
                    SdpCodecList::removeKeywordFromCodecList("-CIF", codecList) ;
                case 0:     // CIF
                    SdpCodecList::removeKeywordFromCodecList("-VGA", codecList) ;
                case  4:    // VGA
                case -1:    // Unspecified / ALL
                    break ;
            }

            *iRejected += pFactory->addCodecs(codecList);
            rc = OS_SUCCESS;
        }
    }

    sbDumpedCodecs = true ;

    return rc;
}



// Set the global video preview window 
OsStatus VoiceEngineFactoryImpl::setVideoPreviewDisplay(void* pDisplay)
{
    OsLock lock(sGuard) ;

    SIPXVE_VIDEO_DISPLAY* pOldDisplay = (SIPXVE_VIDEO_DISPLAY*) mpPreviewWindowDisplay ;    
    if (pDisplay)
    {
        SIPXVE_VIDEO_DISPLAY* pVideoDisplay = (SIPXVE_VIDEO_DISPLAY*) pDisplay;
        if (pVideoDisplay->handle || pVideoDisplay->filter)
        {
            mpPreviewWindowDisplay = new SIPXVE_VIDEO_DISPLAY(*pVideoDisplay) ;
        }
        else
        {
            mpPreviewWindowDisplay = NULL;
        }
    }
    else
    {
        mpPreviewWindowDisplay = NULL;
    }

    if (pOldDisplay)
        delete pOldDisplay;

    return OS_SUCCESS ;
}

void* VoiceEngineFactoryImpl::getVideoPreviewDisplay()
{
    OsLock lock(sGuard) ;

    return mpPreviewWindowDisplay;
}
        
// Update the video preview window given the specified display context.
OsStatus VoiceEngineFactoryImpl::updateVideoPreviewWindow(void* displayContext) 
{
    return OS_SUCCESS ;
}

OsStatus VoiceEngineFactoryImpl::setVideoQuality(int quality)
{
    mVideoQuality = quality;
    return OS_SUCCESS ;
}

OsStatus VoiceEngineFactoryImpl::setVideoCpuValue(int value)
{
    mCpu = value;
    return OS_SUCCESS;
}

OsStatus VoiceEngineFactoryImpl::setConnectionIdleTimeout(int idleTimeout)
{
    mIdleTimeout = idleTimeout;

    return OS_SUCCESS;
}

OsStatus VoiceEngineFactoryImpl::startPlayAudio(VoiceEngineBufferInStream* pStream, GIPS_FileFormats formatType, int downScaling) 
{    
    OsLock lock(sGuard) ;
    OsStatus rc = OS_FAILED ;

    stopPlay() ;
    
    constructGlobalInstance() ;
    mbLocalConnectionInUse = true ;
    mpInStream = pStream ;
	int iRC = mpVoiceEngine->getFile()->GIPSVE_StartPlayingFileLocally(mLocalConnectionId, mpInStream, formatType, ((float) downScaling) / 100) ;
    if (iRC == 0)
    {
        rc = OS_SUCCESS ;
    }

    return rc ;
}


OsStatus VoiceEngineFactoryImpl::stopPlay(const void* pSource) 
{
    OsLock lock(sGuard) ;
    bool bStopAudio = true ;

    if (mpVoiceEngine)
    {

        if (pSource != NULL && mpInStream != NULL)
        {
            if (mpInStream->getSource() != pSource)
            {
                bStopAudio = false ;
            }
        }

        if (bStopAudio)
        {
            if (mpVoiceEngine->getFile()->GIPSVE_IsPlayingFileLocally(mLocalConnectionId))
            {
                 mpVoiceEngine->getFile()->GIPSVE_StopPlayingFileLocally(mLocalConnectionId) ;
            }

            if (mpInStream != NULL)
            {
                mpInStream->close() ;
                delete mpInStream ;
                mpInStream = NULL ;
            }

            mbLocalConnectionInUse = false ;
        }
    }

    return OS_SUCCESS ; 
}


OsStatus VoiceEngineFactoryImpl::playTone(int toneId) 
{
    OsLock lock(sGuard) ;

    constructGlobalInstance() ;
	int check = mpVoiceEngine->getDTMF()->GIPSVE_PlayDTMFTone(toneId);
    assert(check == 0) ;
    check = mpVoiceEngine->getBase()->GIPSVE_LastError();

    return OS_SUCCESS ; 
}

OsStatus VoiceEngineFactoryImpl::setVideoParameters(int bitRate, int frameRate)
{
    mVideoBitRate = bitRate;
    mVideoFrameRate = frameRate;
    return OS_SUCCESS;
}

OsStatus VoiceEngineFactoryImpl::setVideoBitrate(int bitrate)
{
    mVideoBitRate = bitrate;
    return OS_SUCCESS;
}

OsStatus VoiceEngineFactoryImpl::setVideoFramerate(int framerate)
{
    mVideoFrameRate = framerate;
    return OS_SUCCESS;
}

OsStatus VoiceEngineFactoryImpl::getCodecNameByType(SdpCodec::SdpCodecTypes codecType, UtlString& codecName) const
{
    OsStatus rc = OS_FAILED;
    UtlBoolean matchFound = FALSE;

    codecName = "";

    switch (codecType)
    {
    case SdpCodec::SDP_CODEC_G729A:
        codecName = GIPS_CODEC_ID_G729;
        break;
    case SdpCodec::SDP_CODEC_TONES:
        codecName = GIPS_CODEC_ID_TELEPHONE;
        break;
    case SdpCodec::SDP_CODEC_PCMA:
        codecName = GIPS_CODEC_ID_PCMA;
        break;
    case SdpCodec::SDP_CODEC_PCMU:
        codecName = GIPS_CODEC_ID_PCMU;
        break;
    case SdpCodec::SDP_CODEC_GIPS_IPCMA:
        codecName = GIPS_CODEC_ID_EG711A;
        break;
    case SdpCodec::SDP_CODEC_GIPS_IPCMU:
        codecName = GIPS_CODEC_ID_EG711U;
        break;
    case SdpCodec::SDP_CODEC_GIPS_IPCMWB:
        codecName = GIPS_CODEC_ID_IPCMWB;
        break;
    case SdpCodec::SDP_CODEC_GIPS_ILBC:
        codecName = GIPS_CODEC_ID_ILBC;
        break;
    case SdpCodec::SDP_CODEC_GIPS_ISAC:
        codecName = GIPS_CODEC_ID_ISAC;
        break;
    case SdpCodec::SDP_CODEC_GIPS_ISAC_LC:
        codecName = GIPS_CODEC_ID_ISAC_LC;
        break;
    case SdpCodec::SDP_CODEC_GSM:
        codecName = GIPS_CODEC_ID_GSM;
        break;
    case SdpCodec::SDP_CODEC_G723:
        codecName = GIPS_CODEC_ID_G723;
        break;
    case SdpCodec::SDP_CODEC_VP71_CIF:
        codecName = GIPS_CODEC_ID_VP71_CIF;
        break;
    case SdpCodec::SDP_CODEC_H263_CIF:
        codecName = GIPS_CODEC_ID_H263_CIF;
        break;
    case SdpCodec::SDP_CODEC_VP71_QCIF:
        codecName = GIPS_CODEC_ID_VP71_QCIF;
        break;
    case SdpCodec::SDP_CODEC_H263_QCIF:
        codecName = GIPS_CODEC_ID_H263_QCIF;
        break;
    case SdpCodec::SDP_CODEC_VP71_SQCIF:
        codecName = GIPS_CODEC_ID_VP71_SQCIF;
        break;
    case SdpCodec::SDP_CODEC_H263_SQCIF:
        codecName = GIPS_CODEC_ID_H263_SQCIF;
        break;
    case SdpCodec::SDP_CODEC_VP71_QVGA:
        codecName = GIPS_CODEC_ID_VP71_QVGA;
        break;
    case SdpCodec::SDP_CODEC_VP71_VGA:   
        codecName = GIPS_CODEC_ID_VP71_VGA;
        break;
    case SdpCodec::SDP_CODEC_VP71_4CIF:
        codecName = GIPS_CODEC_ID_VP71_4CIF;
        break;
    case SdpCodec::SDP_CODEC_VP71_16CIF:    
        codecName = GIPS_CODEC_ID_VP71_16CIF;
        break;
    case SdpCodec::SDP_CODEC_LSVX:
        codecName = GIPS_CODEC_ID_LSVX;
        break ;
    default:
        OsSysLog::add(FAC_MP, PRI_DEBUG, 
                      "getCodecNameByType: unsupported codec %d", 
                      codecType); 
    }

    if (codecName != "")
    {
        matchFound = TRUE;
        rc = OS_SUCCESS;
    }

    return rc;
}


void VoiceEngineFactoryImpl::Process(int    channel_no, 
                                     short* audio_10ms_16kHz, 
                                     int    len, 
                                     int    sampfreq) 
{
    float scaleFactor = (miVolume / 100.0f) ;
    if (scaleFactor != 1.0f)
    {
        for (int i=0; i<len; i++)
        {
            short sample = audio_10ms_16kHz[i] ;
            sample = (int) (((float) sample) * scaleFactor) ;
            audio_10ms_16kHz[i] = sample ;
        }
    }
}


/* ============================ ACCESSORS ================================= */

OsStatus VoiceEngineFactoryImpl::getSpeakerVolume(int& iVolume) const
{
    OsLock lock(sGuard) ;
    OsStatus rc = OS_SUCCESS ;

    if (mbSpeakerAdjust)
    {
        VoiceEngine* pVoiceEngine = getAnyVoiceEngine() ;
        if (pVoiceEngine)
        {            
            unsigned int volume ;
            pVoiceEngine->getVolumeControl()->GIPSVE_GetSpeakerVolume(volume) ;
            iVolume = (int) ((double) ((((double)volume ) / 255.0) * 100.0) + 0.5) ;
        }
        else
        {
            iVolume = -1 ;
            rc = OS_FAILED ;
        }
    }
    else
    {
        iVolume = miVolume ;
    }
                    
    return rc ;
}

OsStatus VoiceEngineFactoryImpl::getSpeakerDevice(UtlString& device) const
{
    OsStatus rc = OS_SUCCESS ;
    
    rc = outputDeviceIndexToString(device, mCurrentWaveOutDevice);

    return rc ;
}


OsStatus VoiceEngineFactoryImpl::getMicrophoneGain(int& iGain) const
{
    OsLock lock(sGuard) ;
    OsStatus rc = OS_SUCCESS ;

    VoiceEngine* pVoiceEngine = getAnyVoiceEngine() ;
    if (pVoiceEngine)
    {
        unsigned int gain ;
        pVoiceEngine->getVolumeControl()->GIPSVE_GetMicVolume(gain) ;
        double gipsGain = (double) gain;
		iGain = (int) ((double)((double)((gipsGain) / 255.0) * 100.0) + 0.5);
    }
    else
    {
        rc = OS_FAILED ;
    }

    return rc ;
}


OsStatus VoiceEngineFactoryImpl::getMicrophoneDevice(UtlString& device) const
{
    OsStatus rc = OS_SUCCESS ;

    rc = this->inputDeviceIndexToString(device, this->mCurrentWaveInDevice);
    return rc ;
}




bool VoiceEngineFactoryImpl::getGipsTracing() const
{
    return mTrace;
}

void VoiceEngineFactoryImpl::setGipsTracing(bool bEnable)
{
    OsLock lock(sGuard) ;

    mTrace = bEnable ;

    if (mTrace)
    {
        if (mpVoiceEngine)
        {
            mpVoiceEngine->getBase()->GIPSVE_SetObserver(*mpLogger, true) ;
            mpVoiceEngine->getBase()->GIPSVE_SetTraceFilter(0x00FF) ;   // All non-encrypted info
        }

        if (mpVideoEngine)
        {
            mpVideoEngine->GIPSVideo_SetTraceCallback(mpVideoLogger) ;
            mpVideoEngine->GIPSVideo_SetTraceFilter(0x00FF) ;   // All non-encrypted info
        }
    }
    else
    {
        if (mpVoiceEngine)
            mpVoiceEngine->getBase()->GIPSVE_SetTraceFilter(TR_NONE) ;
        if (mpVideoEngine)
        {
            mpVideoEngine->GIPSVideo_SetTraceFilter(TR_NONE) ;
        }
    }
}

OsStatus VoiceEngineFactoryImpl::getLocalAudioConnectionId(int& connectionId) const
{
    connectionId = mLocalConnectionId ;

    return OS_SUCCESS;
}


void* const VoiceEngineFactoryImpl::getAudioEnginePtr() const
{
    OsLock lock(sGuard) ;

    constructGlobalInstance() ;
    return mpVoiceEngine ;
}

OsMutex* VoiceEngineFactoryImpl::getLock() 
{
    return &sGuard ; 
}


void* const VoiceEngineFactoryImpl::getVideoEnginePtr() const
{
    OsLock lock(sGuard) ;

    constructGlobalInstance(false, true) ;
    return mpVideoEngine;
}


MediaDeviceInfo& VoiceEngineFactoryImpl::getAudioInputDeviceInfo() 
{
    return mAudioDeviceInput ;
}


MediaDeviceInfo& VoiceEngineFactoryImpl::getAudioOutputDeviceInfo()
{
    return mAudioDeviceOutput ;
}


OsStatus VoiceEngineFactoryImpl::getVideoCaptureDevice(UtlString& videoDevice)
{
    OsLock lock(sGuard) ;

    OsStatus rc = OS_FAILED;

    if (mVideoCaptureDevice.isNull())
    {
        int gipsRc;
        char szDeviceName[256];
    
        GipsVideoEnginePlatform* pVideoEngine = getAnyVideoEngine() ;
        gipsRc = pVideoEngine->GIPSVideo_GetCaptureDevice(0, szDeviceName, sizeof(szDeviceName));
        if (0 == gipsRc)
        {
            videoDevice = szDeviceName;
            rc = OS_SUCCESS;
        }
        else
        {
            videoDevice.remove(0) ;
            rc = OS_FAILED;
        }
    }
    else
    {
        videoDevice = mVideoCaptureDevice ;
        rc = OS_SUCCESS ;
    }
    
    return rc;
}

OsStatus VoiceEngineFactoryImpl::getVideoCaptureDevices(UtlSList& videoDevices) const
{
    OsLock lock(sGuard) ;

    OsStatus rc = OS_FAILED;
    int gipsRc;
    int index = 0;
    char szDeviceName[256];

    while (1)
    {
        GipsVideoEnginePlatform* pVideoEngine = getAnyVideoEngine() ;
        gipsRc = pVideoEngine->GIPSVideo_GetCaptureDevice(index, szDeviceName, sizeof(szDeviceName));
        index++;        
        if (-1 == gipsRc)
        {            
            break;
        }
        else
        {
            videoDevices.append(new UtlString(szDeviceName));
            rc = OS_SUCCESS;
        }
    }
    return rc;
}

OsStatus VoiceEngineFactoryImpl::setVideoCaptureDevice(const UtlString& videoDevice)
{
    OsLock lock(sGuard) ;
    OsStatus rc = OS_SUCCESS;

    if (videoDevice.length() > 0)
    {
        if (videoDevice.compareTo(mVideoCaptureDevice, UtlString::ignoreCase) != 0)
        {
            mVideoCaptureDevice = videoDevice;
            UtlSListIterator iterator(mInterfaceList) ;
            UtlInt* pInterfaceInt ;
            while (pInterfaceInt = (UtlInt*) iterator())
            {
                VoiceEngineMediaInterface* pInterface = 
                        (VoiceEngineMediaInterface*) pInterfaceInt->getValue();            
                if (pInterface)
                {
                    pInterface->resetVideoCaptureDevice() ;
                }
            }
        }
    }
    return rc;
}

OsStatus VoiceEngineFactoryImpl::getVideoQuality(int& quality) const
{
    quality = mVideoQuality;
    applyVideoQualityOverride(quality) ;

    return OS_SUCCESS;
}

OsStatus VoiceEngineFactoryImpl::getVideoBitRate(int& bitRate) const
{
    bitRate = mVideoBitRate;
    applyVideoBitRateOverride(bitRate) ;

    return OS_SUCCESS;
}

OsStatus VoiceEngineFactoryImpl::getVideoFrameRate(int& frameRate) const
{
    frameRate = mVideoFrameRate;
    applyVideoFrameRateOverride(frameRate) ;

    return OS_SUCCESS;
}

OsStatus VoiceEngineFactoryImpl::getVideoCpuValue(int& cpuValue) const
{
    cpuValue = mCpu;
    applyVideoCpuValueOverride(cpuValue) ;

    return OS_SUCCESS;
}

OsStatus VoiceEngineFactoryImpl::getConnectionIdleTimeout(int& idleTimeout) const
{   
    idleTimeout = mIdleTimeout;
    applyIdleTimeoutOverride(idleTimeout) ;

    return OS_SUCCESS;
}

/* ============================ INQUIRY =================================== */

OsStatus VoiceEngineFactoryImpl::getAudioAECMode(MEDIA_AEC_MODE& mode) const 
{
    mode = mAECMode ;

    return OS_SUCCESS ;
}

OsStatus VoiceEngineFactoryImpl::getAudioNoiseReductionMode(MEDIA_NOISE_REDUCTION_MODE& mode) const 
{
    mode = mNRMode ;

    return OS_SUCCESS ;
}

OsStatus VoiceEngineFactoryImpl::isAGCEnabled(UtlBoolean& bEnable) const 
{
    bEnable = mbEnableAGC ;

    return OS_SUCCESS ;
}


OsStatus VoiceEngineFactoryImpl::isOutOfBandDTMFEnabled(UtlBoolean& bEnabled) const
{
    bEnabled = mbDTMFOutOfBand;

    return OS_SUCCESS;
}

OsStatus VoiceEngineFactoryImpl::isInBandDTMFEnabled(UtlBoolean& bEnabled) const
{
    bEnabled = mbDTMFInBand;

    return OS_SUCCESS;
}

/* //////////////////////////// PROTECTED ///////////////////////////////// */

bool VoiceEngineFactoryImpl::doSetAudioAECMode(VoiceEngine*  pVoiceEngine, 
                                               const MEDIA_AEC_MODE mode) const
{
    bool bRC = true ;

    if (pVoiceEngine)
    {
        switch (mode)
        {
            case MEDIA_AEC_DISABLED:
				pVoiceEngine->getVQE()->GIPSVE_SetECStatus(false) ;
                break ;
            case MEDIA_AEC_SUPPRESS:
				pVoiceEngine->getVQE()->GIPSVE_SetECStatus(true, EC_AES) ;
				break ;
            case MEDIA_AEC_CANCEL_AUTO:
            case MEDIA_AEC_CANCEL:
				pVoiceEngine->getVQE()->GIPSVE_SetECStatus(true, EC_AEC) ;
                break ;
            default:
                bRC = false ;
                assert(false) ;
                break ;
        }
    }
    else
    {
        bRC = false ;
    }

    return bRC ;
}


bool VoiceEngineFactoryImpl::doSetAudioNoiseReductionMode(VoiceEngine*  pVoiceEngine, 
                                                          const MEDIA_NOISE_REDUCTION_MODE mode) const
{
    bool bRC = true ;

    if (pVoiceEngine)
    {
        switch (mode)
        {
            case MEDIA_NOISE_REDUCTION_DISABLED:
				pVoiceEngine->getVQE()->GIPSVE_SetNSStatus(false) ;
                break ;
            case MEDIA_NOISE_REDUCTION_LOW:
                pVoiceEngine->getVQE()->GIPSVE_SetNSStatus(true, NS_LOW_SUPPRESSION) ;
                break ;
            case MEDIA_NOISE_REDUCTION_MEDIUM:
                pVoiceEngine->getVQE()->GIPSVE_SetNSStatus(true, NS_MODERATE_SUPPRESSION) ;
                break ;
            case MEDIA_NOISE_REDUCTION_HIGH:
                pVoiceEngine->getVQE()->GIPSVE_SetNSStatus(true, NS_HIGH_SUPPRESSION) ;
                break ;
            default:
                bRC = false ;
                assert(false) ;
                break ;
        }
    }
    else
    {
        bRC = false ;
    }

    return bRC ;
}


bool VoiceEngineFactoryImpl::doEnableAGC(VoiceEngine* pVoiceEngine, 
                                         bool bEnable) const
{
    bool bRC = true ;
    int iRC ;
	bool isEnabled = false ;

	if (pVoiceEngine)
    {
        GIPS_AGCmodes mode;
        pVoiceEngine->getVQE()->GIPSVE_GetAGCStatus(isEnabled, mode) ;
        if (isEnabled != bEnable)
        {
            iRC = pVoiceEngine->getVQE()->GIPSVE_SetAGCStatus(bEnable, mode) ;
            if (iRC != 0)
            {
                assert(false) ;
                bRC = false ;
            }
            
        }
    }

    return bRC ;
}



/* //////////////////////////// PRIVATE /////////////////////////////////// */

/* ============================ FUNCTIONS ================================= */

OsStatus VoiceEngineFactoryImpl::outputDeviceStringToIndex(int& deviceIndex, const UtlString& device) const
{
    OsStatus rc = OS_FAILED;
#if defined (_WIN32)
    WAVEOUTCAPS outcaps ;
    int numDevices ;
    int i ;

    numDevices = waveOutGetNumDevs();
    for (i=0; i<numDevices && i<MAX_AUDIO_DEVICES; i++)
    {
        waveOutGetDevCaps(i, &outcaps, sizeof(WAVEOUTCAPS)) ;
        if (device.length() == 0 || device.compareTo(outcaps.szPname, UtlString::ignoreCase) == 0)
        {
            deviceIndex = i;
            rc = OS_SUCCESS;
            break;
        }
    }
#elif defined (__MACH__)
    deviceIndex = 0 ;
    char*            outputAudioDevices[MAX_AUDIO_DEVICES] ;
    void*            deviceHandle[MAX_AUDIO_DEVICES] ;
    int numDevices = getNumAudioOutputDevices();
    if (numDevices > MAX_AUDIO_DEVICES)
    {
        numDevices = MAX_AUDIO_DEVICES ;
    }

    getAudioOutputDevices(outputAudioDevices, deviceHandle, numDevices) ;
    for (int i = 0; i < numDevices; i++)
    {
    	if (device.compareTo((const char*) outputAudioDevices[i], UtlString::ignoreCase) == 0)
    	{
    	    deviceIndex = (int) deviceHandle[i];
    	    rc = OS_SUCCESS;
    	}

        free(outputAudioDevices[i]) ;
    }
    rc = OS_SUCCESS;
#else
    deviceIndex = 0 ;
    rc = OS_SUCCESS;
#endif
    return rc;    
}

OsStatus VoiceEngineFactoryImpl::outputDeviceIndexToString(UtlString& device, const int deviceIndex) const
{
    OsStatus rc = OS_FAILED;
#if defined (_WIN32)
    WAVEOUTCAPS outcaps ;
    int numDevices ;

    numDevices = waveOutGetNumDevs();
    if (deviceIndex < numDevices)
    {
        waveOutGetDevCaps(deviceIndex, &outcaps, sizeof(WAVEOUTCAPS)) ;
        device = outcaps.szPname;
        rc = OS_SUCCESS;
    }
#elif defined (__MACH__)
    char*            outputAudioDevices[MAX_AUDIO_DEVICES] ;
    void*            deviceHandle[MAX_AUDIO_DEVICES] ;
    int numDevices = getNumAudioOutputDevices() ;
    if (numDevices > MAX_AUDIO_DEVICES)
    {
        numDevices = MAX_AUDIO_DEVICES ;
    }

    getAudioOutputDevices(outputAudioDevices, deviceHandle, numDevices) ;
    if (deviceIndex < 0)
    {
    	AudioDeviceID currentDevice;
    	UInt32 propsize = sizeof(AudioDeviceID);
    	OSStatus err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice,
    	   &propsize,
    	   &currentDevice);
 
        char szName[1024];
        UInt32 nSize = sizeof(szName) - 1;
        AudioDeviceGetProperty(
	                currentDevice,
			0,
			false,
			kAudioDevicePropertyDeviceName,
			&nSize,
			szName);
    	device = szName;
    	rc = OS_SUCCESS;
    }
    else
    {
        for (int i=0;i<numDevices; i++)
        {
            if (deviceIndex == (int) deviceHandle[i])
            {
                device = outputAudioDevices[i];
                rc = OS_SUCCESS;
            }
            free(outputAudioDevices[i]) ;
        }
    }

#else
    device = "dev/dsp" ;
    rc = OS_SUCCESS;

#endif
    return rc;
}

OsStatus VoiceEngineFactoryImpl::inputDeviceStringToIndex(int& deviceIndex, const UtlString& device) const
{
    OsStatus rc = OS_FAILED;
#if defined (_WIN32)
    WAVEINCAPS incaps ;
    int numDevices ;
    int i ;

    numDevices = waveInGetNumDevs();
    for (i=0; i<numDevices && i<MAX_AUDIO_DEVICES; i++)
    {
        waveInGetDevCaps(i, &incaps, sizeof(WAVEINCAPS)) ;
        if (device == UtlString(incaps.szPname))
        {
            deviceIndex = i;
            rc = OS_SUCCESS;
        }
    }
#elif defined (__MACH__)
    deviceIndex = 0 ;
    char*            inputAudioDevices[MAX_AUDIO_DEVICES] ;
    void*            deviceHandle[MAX_AUDIO_DEVICES] ;
    int numDevices = getNumAudioInputDevices();
    if (numDevices > MAX_AUDIO_DEVICES)
    {
        numDevices = MAX_AUDIO_DEVICES ;
    }    

    getAudioInputDevices(inputAudioDevices, deviceHandle, numDevices) ;
    for (int i = 0; i < numDevices; i++)
    {
    	if (device.compareTo((const char*) inputAudioDevices[i], UtlString::ignoreCase) == 0)
        {
            deviceIndex = (int) (deviceHandle[i]) ;
    	    rc = OS_SUCCESS;
    	}
 
        free(inputAudioDevices[i]) ;
    }

    rc = OS_SUCCESS ;
#else
    device = "dev/dsp" ;
    rc = OS_SUCCESS;
#endif
    return rc;    
}

OsStatus VoiceEngineFactoryImpl::inputDeviceIndexToString(UtlString& device, const int deviceIndex) const
{
    OsStatus rc = OS_FAILED;
#  if defined (_WIN32)
    WAVEINCAPS incaps ;
    int numDevices ;

    numDevices = waveInGetNumDevs();
    if (deviceIndex < numDevices)
    {
        waveInGetDevCaps(deviceIndex, &incaps, sizeof(WAVEINCAPS)) ;
        device = incaps.szPname;
        rc = OS_SUCCESS;
    }
#elif defined (__MACH__)
    char*            inputAudioDevices[MAX_AUDIO_DEVICES] ;
    void*            deviceHandle[MAX_AUDIO_DEVICES] ;
    int numDevices = getNumAudioInputDevices();
    if (numDevices > MAX_AUDIO_DEVICES)
    {
        numDevices = MAX_AUDIO_DEVICES ;
    }

    getAudioInputDevices(inputAudioDevices, deviceHandle, numDevices) ;
    if (deviceIndex < 0)
    {
    	AudioDeviceID currentDevice;
    	UInt32 propsize = sizeof(AudioDeviceID);
    	OSStatus err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultInputDevice,
    	        &propsize,
    	        &currentDevice);

        char szName[1024];
        UInt32 nSize = sizeof(szName) - 1;
        AudioDeviceGetProperty(
	                currentDevice,
			0,
			true,
			kAudioDevicePropertyDeviceName,
			&nSize,
			szName);

    	device = szName;
        
    	rc = OS_SUCCESS;
    }
    else 
    {
        for (int i=0;i<numDevices; i++)
        {
            if (deviceIndex == (int) deviceHandle[i])
            {
                device = inputAudioDevices[i];
                rc = OS_SUCCESS;
            }
            free(inputAudioDevices[i]) ;
        }
    }

    rc = OS_SUCCESS ;
#else
    device = "/dev/dsp" ;
    rc = OS_SUCCESS ;
#endif
    return rc;
}

const bool VoiceEngineFactoryImpl::isMuted() const
{
    return mbMute;
}

bool VoiceEngineFactoryImpl::isVideoSessionActive() const
{
    bool bInSession = false ;
    UtlSListIterator iterator(mInterfaceList);
    VoiceEngineMediaInterface* pInterface = NULL;
    UtlInt* pInterfaceInt = NULL;
        
    while (pInterfaceInt = (UtlInt*) iterator())
    {
        pInterface = (VoiceEngineMediaInterface*) pInterfaceInt->getValue();            
        if (pInterface && pInterface->getVideoEnginePtr())
        {
            GipsVideoEnginePlatform* pGips = 
                    (GipsVideoEnginePlatform*)pInterface->getVideoEnginePtr() ;
            if (pGips)
            {
                bInSession = true ;
                break ;
            }
        }
    }
    return bInSession ;
}

#ifdef USE_GIPS_DLL
GipsVideoEngine::~GipsVideoEngine() {} ;
#endif



VoiceEngineLogger::VoiceEngineLogger() 
{

}

VoiceEngineLogger::~VoiceEngineLogger() 
{

}

    
void VoiceEngineLogger::CallbackOnError(int errCode, int channel) 
{   
    OsSysLog::add(FAC_VOICEENGINE, PRI_ERR,"Channel: %d, error code: %d\n", channel, errCode) ;
}

void VoiceEngineLogger::CallbackOnTrace(char* message, int length) 
{
    if (message != NULL)
    {
#ifdef ENCODE_GIPS_LOGS
        UtlString encodedData ;        
        NetBase64Codec::encode(nLength, pData, encodedData) ;
        OsSysLog::add(FAC_VOICEENGINE, PRI_DEBUG, encodedData.data()) ;
#else
        UtlString data(message, length) ;
        OsSysLog::add(FAC_VOICEENGINE, PRI_DEBUG, data.data()) ;
#endif                      

#ifdef GIPS_LOG_FLUSH
        OsSysLog::flush() ;
#endif
    }
}


VideoEngineLogger::VideoEngineLogger() 
{

}

VideoEngineLogger::~VideoEngineLogger() 
{

}

void VideoEngineLogger::Print(char *message, int length) 
{
    if (message != NULL)
    {
#ifdef ENCODE_GIPS_LOGS
        UtlString encodedData ;        
        NetBase64Codec::encode(nLength, pData, encodedData) ;
        OsSysLog::add(FAC_VIDEOENGINE, PRI_DEBUG, encodedData.data()) ;
#else
        UtlString data(message, length) ;
        OsSysLog::add(FAC_VIDEOENGINE, PRI_DEBUG, data.data()) ;
#endif                      

#ifdef GIPS_LOG_FLUSH
        OsSysLog::flush() ;
#endif
    }
}
