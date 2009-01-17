//
// Copyright (C) 2008 SIPez LLC. 
// Licensed to SIPfoundry under a Contributor Agreement. 
//
// Copyright (C) 2008 SIPfoundry Inc.
// Licensed by SIPfoundry under the LGPL license.
//
// $$
///////////////////////////////////////////////////////////////////////////////

// Author: Alexander Chemeris <Alexander DOT Chemeris AT sipez DOT com>

// SYSTEM INCLUDES
#include <assert.h>

// APPLICATION INCLUDES
#include <mp/MprSpeakerSelector.h>
#include <mp/MpSpeakerSelectBase.h>
#include <mp/MpStringResourceMsg.h>

// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// DEFINES
// STATIC VARIABLE INITIALIZATIONS
// LOCAL CLASSES DECLARATION

/* //////////////////////////// PUBLIC //////////////////////////////////// */

/* ============================ CREATORS ================================== */

// Constructor
MprSpeakerSelector::MprSpeakerSelector(const UtlString& rName,
                                       int maxInputs,
                                       int maxOutputs,
                                       int maxActiveSpeakers,
                                       const UtlString &algorithmName)
:  MpAudioResource(rName, 
                   1, maxInputs, 
                   1, maxOutputs)
, mNumStreams(maxInputs)
, mpSS(NULL)
, mSsFresh(FALSE)
, mpFrameParams(new MpSpeechParams*[mNumStreams])
, mMaxActiveSpeakers(maxActiveSpeakers)
, mTopRanks(new RankIndexPair[mMaxActiveSpeakers])
, mInToOutMap(new int[maxInputs])
, mOutToInMap(new int[maxOutputs])
, mMapTime(0)
, mChangeMapTime(new int[maxInputs])
{
   assert(maxInputs >= maxOutputs);
   assert(maxOutputs >= maxActiveSpeakers);

   // Initialize Speaker Selection algorithm.
   mpSS = MpSpeakerSelectBase::createInstance(algorithmName);
   assert(mpSS != NULL);
   mpSS->init(mNumStreams, mMaxActiveSpeakers);
   mSsFresh = TRUE;

   // Initialize mappings.
   for (int i=0; i<maxInputs; i++)
   {
      mInToOutMap[i] = -1;
      mChangeMapTime[i] = 0;
   }
   for (int j=0; j<maxOutputs; j++)
   {
      mOutToInMap[j] = -1;
   }
}

// Constructor
MprSpeakerSelector::MprSpeakerSelector(const UtlString& rName,
                                       int maxInputs,
                                       int maxOutputs,
                                       int maxActiveSpeakers,
                                       MpSpeakerSelectBase *pSS)
:  MpAudioResource(rName, 
                   1, maxInputs, 
                   1, maxOutputs)
, mNumStreams(maxInputs)
, mpSS(pSS)
, mSsFresh(FALSE)
, mpFrameParams(new MpSpeechParams*[mNumStreams])
, mMaxActiveSpeakers(maxActiveSpeakers)
, mTopRanks(new RankIndexPair[mMaxActiveSpeakers])
, mInToOutMap(new int[maxInputs])
, mOutToInMap(new int[maxOutputs])
, mMapTime(0)
, mChangeMapTime(new int[maxInputs])
{
   assert(maxInputs >= maxOutputs);
   assert(maxOutputs >= maxActiveSpeakers);

   // Initialize Speaker Selection algorithm.
   assert(mpSS != NULL);
   mpSS->init(mNumStreams, mMaxActiveSpeakers);
   mSsFresh = TRUE;

   // Initialize mappings.
   for (int i=0; i<maxInputs; i++)
   {
      mInToOutMap[i] = -1;
      mChangeMapTime[i] = 0;
   }
   for (int j=0; j<maxOutputs; j++)
   {
      mOutToInMap[j] = -1;
   }
}

// Destructor
MprSpeakerSelector::~MprSpeakerSelector()
{
   delete mpSS;
   delete[] mpFrameParams;
   delete[] mTopRanks;
   delete[] mInToOutMap;
   delete[] mOutToInMap;
   delete[] mChangeMapTime;
}

/* ============================ MANIPULATORS ============================== */

OsStatus MprSpeakerSelector::chageAlgorithm(const UtlString& namedResource, 
                                            OsMsgQ& fgQ,
                                            const UtlString &algorithmName)
{
   MpStringResourceMsg msg((MpResourceMsg::MpResourceMsgType)MPRM_CHANGE_ALGORITHM,
                           namedResource,
                           algorithmName);
   return fgQ.send(msg);
}

/* ============================ ACCESSORS ================================= */

/* ============================ INQUIRY =================================== */

/* //////////////////////////// PROTECTED ///////////////////////////////// */

UtlBoolean MprSpeakerSelector::connectInput(MpResource& rFrom,
                                            int fromPortIdx,
                                            int toPortIdx)
{
   // TODO:: Move this to MprEncode and implement disconnect!
   UtlBoolean res = MpAudioResource::connectInput(rFrom, fromPortIdx, toPortIdx);
   if (res)
   {
      if (isOutputConnected(toPortIdx))
      {
         // Output is already connected, enable participant
         mpSS->enableParticipant(toPortIdx, TRUE);
      }
   }
   return res;
}

UtlBoolean MprSpeakerSelector::connectOutput(MpResource& rTo,
                                             int toPortIdx,
                                             int fromPortIdx)
{
   UtlBoolean res = MpAudioResource::connectOutput(rTo, toPortIdx, fromPortIdx);
   if (res)
   {
      if (isInputConnected(fromPortIdx))
      {
         // Input is already connected, enable participant
         mpSS->enableParticipant(fromPortIdx, TRUE);
      }
   }
   return res;
}

UtlBoolean MprSpeakerSelector::disconnectInput(int inPortIdx)
{
   UtlBoolean res = MpAudioResource::disconnectInput(inPortIdx);
   if (res)
   {
      // No need to process stream with disconnected input
      mpSS->enableParticipant(inPortIdx, FALSE);
   }
   return res;
}

UtlBoolean MprSpeakerSelector::disconnectOutput(int outPortIdx)
{
   UtlBoolean res = MpAudioResource::disconnectOutput(outPortIdx);
   if (res)
   {
      // No need to process stream with disconnected output
      mpSS->enableParticipant(outPortIdx, FALSE);
   }
   return res;
}

OsStatus MprSpeakerSelector::setFlowGraph(MpFlowGraphBase* pFlowGraph)
{
   OsStatus res =  MpAudioResource::setFlowGraph(pFlowGraph);

   if (res == OS_SUCCESS)
   {
      if (pFlowGraph != NULL)
      {
         // We're added to flowgraph. Do nothing.
      } 
      else
      {
         // We're removed from flowgraph. Reset algorithm instance.
         assert(mpSS != NULL);
         mpSS->reset();
         // Enable connected participants.
         enableConnectedStreams();
      }
   }

   return res;
}

UtlBoolean MprSpeakerSelector::handleMessage(MpResourceMsg& rMsg)
{
   UtlBoolean msgHandled = FALSE;

   switch (rMsg.getMsg()) 
   {
   case MPRM_CHANGE_ALGORITHM:
      {
         MpStringResourceMsg *pMsg = (MpStringResourceMsg *)&rMsg;
         handleChangeAlgorithm(pMsg->getData());
      }
      msgHandled = TRUE;
      break;

   default:
      // If we don't handle the message here, let our parent try.
      msgHandled = MpResource::handleMessage(rMsg); 
      break;
   }
   return msgHandled;
}

UtlBoolean MprSpeakerSelector::doProcessFrame(MpBufPtr inBufs[],
                                     MpBufPtr outBufs[],
                                     int inBufsSize,
                                     int outBufsSize,
                                     UtlBoolean isEnabled,
                                     int samplesPerFrame,
                                     int samplesPerSecond)
{
   MpAudioBufPtr in;
   UtlBoolean ret = FALSE;

   // We're disabled or have nothing to process.
   if ( outBufsSize == 0 || inBufsSize == 0)
   {
      return TRUE;
   }

   if (!isEnabled)
   {
      // Do nothing.
      ret = TRUE;

      // Pass inputs to outputs (keep in mind we have less outputs then inputs).
      for (int i=0; i<outBufsSize; i++)
      {
         outBufs[i].swap(inBufs[i]);
      }
   }
   else
   {
      // Fill mpFrameParams array.
      for (int i=0; i<mNumStreams; i++)
      {
         UtlBoolean enabled;
         mpSS->isParticipantEnabled(i, enabled);
         if (!enabled)
         {
            // If participant is not enabled - pass input to output directly.
            mpFrameParams[i] = NULL;
            outBufs[i].swap(inBufs[i]);
         }
         else if (inBufs[i].isValid())
         {
            // Prepare for processing if participant is enabled and active.
            MpAudioBufPtr pBuf = inBufs[i];
            mpFrameParams[i] = &pBuf->getSpeechParams();
         }
         else
         {
            // Participant is enabled, but is not active.
            mpFrameParams[i] = NULL;
         }
      }

      // Process frames.
      OsStatus res = mpSS->processFrame(mpFrameParams,
                                        samplesPerFrame*1000/samplesPerSecond);
      ret = (res==OS_SUCCESS);

      // Peek top speakers.
      peekTopSpeakers(mpFrameParams, mNumStreams, mTopRanks, mMaxActiveSpeakers);

      // Update inputs to outputs mapping.
      updateMapping(mTopRanks, mMaxActiveSpeakers);

      // Send frames of top speakers to the output
      for (int j=0; j<mMaxActiveSpeakers; j++)
      {
         int index = mTopRanks[j].mIndex;
         if (index == -1)
         {
            // We've reached the end of the active participants list.
            break;
         }
         outBufs[getOutputForInput(index)].swap(inBufs[index]);
      }
   }

   return ret;
}

UtlBoolean MprSpeakerSelector::handleEnable()
{
   UtlBoolean res = MpResource::handleEnable();
   if (res)
   {
      if (!mSsFresh)
      {
         // Reset algorithm only if it haven't been just created.
         mpSS->reset();
         enableConnectedStreams();
         mSsFresh = TRUE;
      }
   }
   return res;
}

UtlBoolean MprSpeakerSelector::handleDisable()
{
   UtlBoolean res = MpResource::handleDisable();
   if (res)
   {
      // Mark that we should reset algorithm on next enable().
      mSsFresh = FALSE;
   }
   return res;
}

UtlBoolean MprSpeakerSelector::handleChangeAlgorithm(const UtlString &algorithmName)
{
   if (mpSS != NULL)
   {
      delete mpSS;
   }
   mpSS = MpSpeakerSelectBase::createInstance(algorithmName);
   // Enable connected participants.
   enableConnectedStreams();

   return TRUE;
}

void MprSpeakerSelector::enableConnectedStreams()
{
   for (int i=0; i<mNumStreams; i++)
   {
      // Here we assume that all participants are disabled initially.
      if (isInputConnected(i) && isOutputConnected(i))
      {
         mpSS->enableParticipant(i, TRUE);
      }
   }
}

void MprSpeakerSelector::peekTopSpeakers(MpSpeechParams **frameParams, int frameParamsNum,
                                         RankIndexPair *topRanks, int topRanksNum)
{
   int lowRankIdx = 0;
   int i, j;

   // NB! Keep in mind that HIGHER ranked participant have LOWER by value rank!
   //     So the top ranked participant have rank equal to 0 and lowest ranked
   //     participant have rank of UINT_MAX.

   // Initialize topRanks array.
   for (i=0; i<topRanksNum; i++)
   {
      topRanks[i].mRank = UINT_MAX;
      topRanks[i].mIndex = -1;
   }

   // Search for first topRanksNum non-NULL participants.
   for (i=0, j=0; i<frameParamsNum && j<topRanksNum; i++)
   {
      if (frameParams[i] != NULL && frameParams[i]->mSpeakerRank < UINT_MAX)
      {
         topRanks[j].mRank = frameParams[i]->mSpeakerRank;
         topRanks[j].mIndex = i;
         if (topRanks[j].mRank > topRanks[lowRankIdx].mRank)
         {
            // This participant have lower rank.
            lowRankIdx = j;
         }
         j++;
      }
   }

   if (topRanks[topRanksNum-1].mIndex == -1)
   {
      // No more non-NULL participants. Just return.
      return;
   }

   // Continue and search for top ranked participants.
   for (; i<frameParamsNum; i++)
   {
      if (frameParams[i] == NULL)
      {
         // Participant is not active. Skip it.
         continue;
      }

      unsigned rank = frameParams[i]->mSpeakerRank;
      if (rank >= topRanks[lowRankIdx].mRank)
      {
         // Nothing interesting... Continue.
         continue;
      }

      // Higher rank found. Save it and update lowRankIdx.
      topRanks[lowRankIdx].mRank = rank;
      topRanks[lowRankIdx].mIndex = i;
      lowRankIdx = 0;
      for (j=1; j<topRanksNum; j++)
      {
         if (topRanks[j].mRank > topRanks[lowRankIdx].mRank)
         {
            lowRankIdx = j;
         }
      }
   }
}

void MprSpeakerSelector::updateMapping(MprSpeakerSelector::RankIndexPair *topRanks,
                                       int topRanksNum)
{
   int i;
   UtlBoolean isChanged = FALSE;

   // Update map time.
   mMapTime++;

   // Update change time and search for conflicting active speakers.
   for (i=0; i<topRanksNum && topRanks[i].mIndex > -1; i++)
   {
      int speaker = topRanks[i].mIndex;
      mChangeMapTime[speaker] = mMapTime;
      if (  mInToOutMap[speaker] == -1
         || mOutToInMap[mInToOutMap[speaker]] != speaker)
      {
         isChanged = TRUE;
      }
   }
   // Resolve conflicts
   if (isChanged)
   {
      for (i=0; i<topRanksNum && topRanks[i].mIndex > -1; i++)
      {
         int speaker = topRanks[i].mIndex;
         if (mInToOutMap[speaker] == -1)
         {
            // This input have never been active before, look for an empty
            // slot for it.
            mInToOutMap[speaker] = getOldestOutput();
            mOutToInMap[mInToOutMap[speaker]] = speaker;
         }
         else if (mOutToInMap[mInToOutMap[speaker]] != speaker)
         {
            if (mChangeMapTime[mInToOutMap[speaker]] != mMapTime)
            {
               // This input may know its place and may reside on it.
               // Its competitor has not been active for a while.
               mOutToInMap[mInToOutMap[speaker]] = speaker;
            }
            else
            {
               // Output which this input wants is already occupied by
               // an active speaker. Seek for an other one.
               mInToOutMap[speaker] = getOldestOutput();
               mOutToInMap[mInToOutMap[speaker]] = speaker;
            }
         }
      }
   }
}

int MprSpeakerSelector::getOldestOutput()
{
   uint32_t minTime = mChangeMapTime[mOutToInMap[0]];
   int oldestOutput = 0;
   for (int i=0; i<maxOutputs(); i++)
   {
      if (mOutToInMap[i] == -1)
      {
         // If this output hasn't been used yet, just return it.
         return i;
      }

      // Is this output older then previous one?
      uint32_t time = mChangeMapTime[mOutToInMap[i]];
      if (minTime > time)
      {
         minTime = time;
         oldestOutput = i;
      }
   }

   return oldestOutput;
}

int MprSpeakerSelector::getOutputForInput(int inputNum)
{
   return mInToOutMap[inputNum];
}

/* //////////////////////////// PRIVATE /////////////////////////////////// */

/* ============================ FUNCTIONS ================================= */


