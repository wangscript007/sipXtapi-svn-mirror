//  
// Copyright (C) 2006-2007 SIPez LLC. 
// Licensed to SIPfoundry under a Contributor Agreement. 
//
// Copyright (C) 2004-2007 SIPfoundry Inc.
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
#include "os/OsDefs.h"
#include "os/OsTask.h"
#include "os/OsReadLock.h"
#include "os/OsWriteLock.h"
#include <os/OsEvent.h>
#include "mp/MpFlowGraphBase.h"
#include "mp/MpFlowGraphMsg.h"
#include "mp/MpResourceMsg.h"
#include "mp/MpResourceSortAlg.h"
#include "mp/MpMediaTask.h"
#include <mp/MpMisc.h>

#ifdef RTL_ENABLED
#   include <rtl_macro.h>
#endif

// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STATIC VARIABLE INITIALIZATIONS
int MpFlowGraphBase::gFgMaxNumber = 0;

/* //////////////////////////// PUBLIC //////////////////////////////////// */

/* ============================ CREATORS ================================== */

// Constructor
MpFlowGraphBase::MpFlowGraphBase(int samplesPerFrame, int samplesPerSec)
: mRWMutex(OsRWMutex::Q_PRIORITY)
, mFgNumber(gFgMaxNumber++)
, mResourceDict()
, mCurState(STOPPED)
, mMessages(MAX_FLOWGRAPH_MESSAGES)
, mNotifyDispatcher(NULL)
, mPeriodCnt(0)
, mLinkCnt(0)
, mResourceCnt(0)
, mRecomputeOrder(FALSE)
, mSamplesPerFrame(samplesPerFrame)
, mSamplesPerSec(samplesPerSec)
, mpResourceInProcess(NULL)
{
   int i;

   for (i=0; i < MAX_FLOWGRAPH_RESOURCES; i++)
   {
      mUnsorted[i] = NULL;
      mExecOrder[i] = NULL;
   }
}

// Destructor
MpFlowGraphBase::~MpFlowGraphBase()
{
   int      msecsPerFrame;
   OsStatus res;

   // release the flow graph and any resources it contains
   res = destroyResources();
   assert(res == OS_SUCCESS);

   // since the destroyResources() call may not take effect until the start
   // of the next frame processing interval, we loop until this flow graph is
   // stopped and contains no resources
   msecsPerFrame = (mSamplesPerFrame * 1000) / mSamplesPerSec;
   while (mCurState != STOPPED || mResourceCnt != 0)
   {
      res = OsTask::delay(msecsPerFrame);
      assert(res == OS_SUCCESS);
   }
}

/* ============================ MANIPULATORS ============================== */

// Creates a link between the "outPortIdx" port of the "rFrom" resource
// to the "inPortIdx" port of the "rTo" resource.
// If the flow graph is not "started", this call takes effect immediately.
// Otherwise, the call takes effect at the start of the next frame processing
// interval.
// Returns OS_SUCCESS if the link was successfully added. Returns
// OS_INVALID_ARGUMENT if the caller specified an invalid port index.
// Returns OS_UNSPECIFIED if the addLink attempt failed for some other
// reason.
OsStatus MpFlowGraphBase::addLink(MpResource& rFrom, int outPortIdx,
                                  MpResource& rTo,   int inPortIdx)
{
   OsWriteLock    lock(mRWMutex);

   UtlBoolean      handled;
   MpFlowGraphMsg msg(MpFlowGraphMsg::FLOWGRAPH_ADD_LINK, NULL,
                      &rFrom, &rTo, outPortIdx, inPortIdx);

   if (outPortIdx < 0 || outPortIdx >= rFrom.maxOutputs() ||
       inPortIdx  < 0 || inPortIdx  >= rTo.maxInputs())
      return OS_INVALID_ARGUMENT;

   if (mCurState == STARTED)
      return postMessage(msg);

   handled = handleMessage(msg);
   if (handled)
      return OS_SUCCESS;
   else
      return OS_UNSPECIFIED;
}

OsMsgDispatcher* 
MpFlowGraphBase::setNotificationDispatcher(OsMsgDispatcher* notifyDispatcher)
{
   OsMsgDispatcher* oldDispatcher = mNotifyDispatcher;
   mNotifyDispatcher = notifyDispatcher;
   return oldDispatcher;
}

// Adds the indicated media processing object to the flow graph.  If 
// "makeNameUnique" is TRUE, then if a resource with the same name already
// exists in the flow graph, the name for "rResource" will be changed (by
// adding a numeric suffix) to make it unique within the flow graph.
// If the flow graph is not "started", this call takes effect immediately.
// Otherwise, the call takes effect at the start of the next frame processing
// interval.
// Returns OS_SUCCESS if the resource was successfully added.  Otherwise
// returns OS_UNSPECIFIED.
OsStatus MpFlowGraphBase::addResource(MpResource& rResource,
                                  UtlBoolean makeNameUnique)
{
   OsWriteLock    lock(mRWMutex);

   UtlBoolean      handled;
   MpFlowGraphMsg msg(MpFlowGraphMsg::FLOWGRAPH_ADD_RESOURCE, NULL,
                      &rResource, NULL, makeNameUnique);

   // Setting notification enabled/disabled status based on 
   // what was already set on existing resources.
   // Check to see if we have any resources already added to the flowgraph
   if(numResources() > 0)
   {
      // and if so, get their notification enabled/disabled state,
      UtlBoolean notfState = mUnsorted[0]->areNotificationsEnabled();

      // and set it on this new resource.
      rResource.setNotificationsEnabled(notfState);
   }

   if (mCurState == STARTED)
      return postMessage(msg);

   handled = handleMessage(msg);
   if (handled)
      return OS_SUCCESS;
   else
      return OS_UNSPECIFIED;
}

// Stops the flow graph, removes all of the resources in the flow graph 
// and destroys them.  If the flow graph is not "started", this call takes
// effect immediately.  Otherwise, the call takes effect at the start of
// the next frame processing interval.  For now, this method always returns
// success.
OsStatus MpFlowGraphBase::destroyResources(void)
{
   OsWriteLock    lock(mRWMutex);

   UtlBoolean      handled;
   MpFlowGraphMsg msg(MpFlowGraphMsg::FLOWGRAPH_DESTROY_RESOURCES, NULL);

   if (mCurState == STARTED)
      return postMessage(msg);

   handled = handleMessage(msg);
   if (handled)
      return OS_SUCCESS;
   else
      return OS_UNSPECIFIED;
}

// Invokes the disable() method for each resource in the flow graph.
// Resources must be enabled before they will perform any meaningful
// processing on the media stream.
// If the flow graph is not "started", this call takes effect
// immediately.  Otherwise, the call takes effect at the start of the
// next frame processing interval.  For now, this method always returns
// success.
OsStatus MpFlowGraphBase::disable(void)
{
   OsWriteLock    lock(mRWMutex);

   UtlBoolean      handled;
   MpFlowGraphMsg msg(MpFlowGraphMsg::FLOWGRAPH_DISABLE, NULL);

   if (mCurState == STARTED)
      return postMessage(msg);

   handled = handleMessage(msg);
   if (handled)
      return OS_SUCCESS;
   else
      return OS_UNSPECIFIED;
}

// Invokes the enable() method for each resource in the flow graph.
// Resources must be enabled before they will perform any meaningful
// processing on the media stream.
// If the flow graph is not "started", this call takes effect
// immediately.  Otherwise, the call takes effect at the start of the
// next frame processing interval.  For now, this method always returns
// success.
OsStatus MpFlowGraphBase::enable(void)
{
   OsWriteLock    lock(mRWMutex);

   UtlBoolean      handled;
   MpFlowGraphMsg msg(MpFlowGraphMsg::FLOWGRAPH_ENABLE, NULL);

   if (mCurState == STARTED)
      return postMessage(msg);

   handled = handleMessage(msg);
   if (handled)
      return OS_SUCCESS;
   else
      return OS_UNSPECIFIED;
}

// Notification that this flow graph has just been granted the focus.
// However, we do not want it.
OsStatus MpFlowGraphBase::gainFocus(void)
{
   Nprintf("MpFG::gainFocus(0x%X), not supported!\n", (int) this, 0,0,0,0,0);
   return OS_INVALID_ARGUMENT;
}

// Inserts "rResource" into the flow graph downstream of the
// designated "rUpstreamResource" resource.
// The new resource will be inserted on the "outPortIdx" output
// link of "rUpstreamResource".
// If the flow graph is not "started", this call takes effect
// immediately.  Otherwise, the call takes effect at the start of the
// next frame processing interval.
// Returns OS_SUCCESS if the resource was successfully inserted. Returns
// OS_INVALID_ARGUMENT if the caller specified an invalid port index.
OsStatus MpFlowGraphBase::insertResourceAfter(MpResource& rResource,
                                 MpResource& rUpstreamResource,
                                 int outPortIdx)
{
   MpResource *pDownstreamResource;
   int         downstreamInPortIdx;
   OsStatus    res;

   // Get information about the downstream end of the link
   rUpstreamResource.getOutputInfo(outPortIdx, pDownstreamResource,
                                   downstreamInPortIdx);

   // Add the new resource to the flow graph
   res = addResource(rResource);
   if (res != OS_SUCCESS)
      return res;

   if (pDownstreamResource != NULL)
   {
      // Remove the link between the upstream and downstream resources
      res = removeLink(rUpstreamResource, outPortIdx);
      if (res != OS_SUCCESS)
      {                              // recover from remove link failure
         removeResource(rResource);
         return res;
      }

      // Add the link between output port 0 the new resource and the
      // downstream resource
      res = addLink(rResource, 0, *pDownstreamResource, downstreamInPortIdx);
      if (res != OS_SUCCESS)
      {                              // recover from add link failure
         removeResource(rResource);
         addLink(rUpstreamResource, outPortIdx,
                 *pDownstreamResource, downstreamInPortIdx);
         return res;
      }
   }

   // Add the link between the upstream resource and input port 0 of
   // the new resource
   res = addLink(rUpstreamResource, outPortIdx, rResource, 0);
   if (res != OS_SUCCESS)
   {                              // recover from add link failure
      removeResource(rResource);
      if (pDownstreamResource != NULL)
      {
         addLink(rUpstreamResource, outPortIdx,
                 *pDownstreamResource, downstreamInPortIdx);
      }
   }

   return res;
}

//:Inserts "rResource" into the flow graph upstream of the
//:designated "rDownstreamResource" resource.
// The new resource will be inserted on the "inPortIdx" input
// link of "rDownstreamResource".
// If the flow graph is not "started", this call takes effect
// immediately.  Otherwise, the call takes effect at the start of the
// next frame processing interval.
// Returns OS_SUCCESS if the resource was successfully inserted. Returns
// OS_INVALID_ARGUMENT if the caller specified an invalid port index.
OsStatus MpFlowGraphBase::insertResourceBefore(MpResource& rResource,
                                 MpResource& rDownstreamResource,
                                 int inPortIdx)
{
   MpResource *pUpstreamResource;
   int         upstreamOutPortIdx;
   OsStatus    res;

   // Get information about the downstream end of the link
   rDownstreamResource.getInputInfo(inPortIdx, pUpstreamResource,
                                    upstreamOutPortIdx);

   // Add the new resource to the flow graph
   res = addResource(rResource);
   if (res != OS_SUCCESS)
      return res;

   if (pUpstreamResource != NULL)
   {
      // Remove the link between the upstream and downstream resources
      res = removeLink(*pUpstreamResource, upstreamOutPortIdx);
      if (res != OS_SUCCESS)
      {                              // recover from remove link failure
         removeResource(rResource);
         return res;
      }

      // Add the link between output port 0 the new resource and the
      // downstream resource
      res = addLink(rResource, 0, rDownstreamResource, inPortIdx);
      if (res != OS_SUCCESS)
      {                              // recover from add link failure
         removeResource(rResource);
         addLink(*pUpstreamResource, upstreamOutPortIdx,
                 rDownstreamResource, inPortIdx);
         return res;
      }
   }

   // Add the link between the upstream resource and input port 0 of
   // the new resource
   res = addLink(*pUpstreamResource, upstreamOutPortIdx, rResource, 0);
   if (res != OS_SUCCESS)
   {                              // recover from add link failure
      removeResource(rResource);
      if (pUpstreamResource != NULL)
      {
         addLink(*pUpstreamResource, upstreamOutPortIdx,
                 rDownstreamResource, inPortIdx);
      }
   }

   return res;
}

// Notification that this flow graph has just lost the focus.
// However, we did not want it.
OsStatus MpFlowGraphBase::loseFocus(void)
{
   Nprintf("MpFG::loseFocus(0x%X), not supported!\n", (int) this, 0,0,0,0,0);
   return OS_INVALID_ARGUMENT;
}

// Post a notification message to the dispatcher.
OsStatus MpFlowGraphBase::postNotification(const MpResNotificationMsg& msg)
{
   // If there is no dispatcher, OS_NOT_FOUND is used.
   OsStatus stat = OS_NOT_FOUND;
   
   if(mNotifyDispatcher != NULL)
   {
      // If the limit is reached on the queue, OS_LIMIT_REACHED is sent.
      // otherwise success - OS_SUCCESS.
      stat = mNotifyDispatcher->post(msg);
   }
   return stat;
}

// Processes the next frame interval's worth of media for the flow graph.
// For now, this method always returns success.
OsStatus MpFlowGraphBase::processNextFrame(void)
{
#ifdef RTL_ENABLED
    RTL_BLOCK("MpFlowGraphBase.processNextFrame");
#endif
    
    UtlBoolean boolRes;
   int       i;
   OsStatus  res;

   // Call processMessages() to handle any messages that have been posted
   // to either resources in the flow graph or to the flow graph itself.
   res = processMessages();
   assert(res == OS_SUCCESS);

   // If resources or links have been added/removed from the flow graph,
   // then we need to recompute the execution order for resources in the
   // flow graph.  This is done to ensure that resources producing output
   // buffers are executed before other resources in the flow graph that
   // expect to consume those buffers.
   if (mRecomputeOrder)
   {
      res = computeOrder();
      assert(res == OS_SUCCESS);

//#define TEST_PRINT_TOPOLOGY
#ifdef TEST_PRINT_TOPOLOGY
      for(i=0; i < mResourceCnt; i++)
      {
           int outIndex;
           for(outIndex = 0; outIndex < mExecOrder[i]->mMaxOutputs; outIndex++)
           {
               printf("%s[%d]==>%s[%d]\n",
                   mExecOrder[i]->data(), outIndex, 
                   mExecOrder[i]->mpOutConns[outIndex].pResource ? mExecOrder[i]->mpOutConns[outIndex].pResource->data() : "",
                   mExecOrder[i]->mpOutConns[outIndex].portIndex);
           }
      }
#endif
   }

   // If the flow graph is "STOPPED" then there is no further processing
   // required for this frame interval.  However, if the flow graph is
   // "STARTED", we invoke the processFrame() method for each of the
   // resources in the flow graph.
   if (getState() == STARTED)
   {

      for (i=0; i < mResourceCnt; i++)
      {
         mpResourceInProcess = mExecOrder[i];
         boolRes = mExecOrder[i]->processFrame();
         if (!boolRes) {
            osPrintf("MpMedia: called %s, which indicated failure\n",
               mpResourceInProcess->data());
         }
      }
   }

   mpResourceInProcess = NULL;
   mPeriodCnt++;

   return OS_SUCCESS;
}

// Removes the link between the "outPortIdx" port of the "rFrom"
// resource and its downstream counterpart. If the flow graph is not
// "started", this call takes effect immediately.  Otherwise, the call
// takes effect at the start of the next frame processing interval.
// Returns OS_SUCCESS if the link is removed.  Returns
// OS_INVALID_ARGUMENT if the caller specified an invalid port index.
OsStatus MpFlowGraphBase::removeLink(MpResource& rFrom, int outPortIdx)
{
   OsWriteLock    lock(mRWMutex);

   UtlBoolean      handled;
   MpFlowGraphMsg msg(MpFlowGraphMsg::FLOWGRAPH_REMOVE_LINK, NULL,
                      &rFrom, NULL, outPortIdx);

   if (outPortIdx < 0 || outPortIdx >= rFrom.maxOutputs())
      return OS_INVALID_ARGUMENT;

   if (mCurState == STARTED)
      return postMessage(msg);

   handled = handleMessage(msg);
   if (handled)
      return OS_SUCCESS;
   else
      return OS_UNSPECIFIED;
}

// Removes the indicated media processing object from the flow graph.
// If the flow graph is not "started", this call takes effect
// immediately.  Otherwise, the call takes effect at the start of the
// next frame processing interval.  Returns OS_SUCCESS to indicate that
// the resource has been removed or OS_UNSPECIFIED if the removeResource
// operation failed.
OsStatus MpFlowGraphBase::removeResource(MpResource& rResource)
{
   OsWriteLock    lock(mRWMutex);

   UtlBoolean      handled;
   MpFlowGraphMsg msg(MpFlowGraphMsg::FLOWGRAPH_REMOVE_RESOURCE, NULL,
                      &rResource);

   if (mCurState == STARTED)
      return postMessage(msg);

   handled = handleMessage(msg);
   if (handled)
      return OS_SUCCESS;
   else
      return OS_UNSPECIFIED;
}

OsStatus MpFlowGraphBase::setNotificationsEnabled(bool enabled, 
                                                  const UtlString& resourceName)
{
   OsWriteLock lock(mRWMutex);
   OsStatus res = OS_SUCCESS;
   MpResource* pResource = NULL;

   // Check to see if the resource name is null -- if it is, this means
   // send it to all resources, so just leave the null name in when sending
   // the message.  No need to validate that name.

   // Lookup the resource just to validate that the resource with that name exists
   // in the flowgraph (if it isn't null).
   if(!resourceName.isNull())
   {
      res = lookupResourcePrivate(resourceName, pResource);
   }

   if(res == OS_SUCCESS)
   {
      // If the resource exists or all resources are selected, 
      // then call the static method to send a notification enable/disable 
      // message to the actual named resource (or all resources).
      res = MpResource::setNotificationsEnabled(enabled, resourceName, *getMsgQ());
   }

   return res;
}

// Start this flow graph.
// A flow graph must be "started" before it will process media streams.
// This call takes effect immediately.  For now, this method always
// returns success.
OsStatus MpFlowGraphBase::start(void)
{
   OsWriteLock lock(mRWMutex);
   UtlBoolean   handled;

   MpFlowGraphMsg msg(MpFlowGraphMsg::FLOWGRAPH_START);

   handled = handleMessage(msg);
   if (handled)
      return OS_SUCCESS;
   else
      return OS_UNSPECIFIED;
}

// Stop this flow graph.
// Stop processing media streams with this flow graph.  This call takes
// effect at the start of the next frame processing interval.  For now,
// this method always returns success.
OsStatus MpFlowGraphBase::stop(void)
{
   OsWriteLock    lock(mRWMutex);

   MpFlowGraphMsg msg(MpFlowGraphMsg::FLOWGRAPH_STOP);

   return postMessage(msg);
}

void MpFlowGraphBase::synchronize(const char* tag, int val1)
{
   OsTask* val2 = OsTask::getCurrentTask();
   if (val2 != MpMediaTask::getMediaTask(0)) {
      OsEvent event;
      MpFlowGraphMsg msg(MpFlowGraphMsg::FLOWGRAPH_SYNCHRONIZE,
         NULL, NULL, (void*) tag, val1, (int) val2);
      OsStatus  res;

      msg.setPtr1(&event);
      res = postMessage(msg);
      // if (NULL == tag) osPrintf("MpFlowGraphBase::synchronize()\n");
      event.wait();
   } else {
      osPrintf("Note: synchronize called from within Media Task\n");
   }
}

/* ============================ ACCESSORS ================================= */

// (static) Displays information on the console about the specified flow
// graph.
void MpFlowGraphBase::flowGraphInfo(MpFlowGraphBase* pFlowGraph)
{
   int         i;
   MpResource* pResource;

   if (NULL == pFlowGraph) {
      MpMediaTask* pMediaTask = MpMediaTask::getMediaTask(0);
      pFlowGraph = pMediaTask->getFocus();
      if (NULL == pFlowGraph) {
         pMediaTask->getManagedFlowGraphs(&pFlowGraph, 1, i);
         if (0 == i) pFlowGraph = NULL;
      }
   }
   if (NULL == pFlowGraph) {
      osPrintf("No flowGraph to display!\n");
      return;
   }
   osPrintf("\nFlow graph information for %p\n", pFlowGraph);
   osPrintf("  State:                    %s\n",
             pFlowGraph->isStarted() ? "STARTED" : "STOPPED");

   osPrintf("  Processed Frame Count:    %d\n",
             pFlowGraph->numFramesProcessed());

   osPrintf("  Samples Per Frame:        %d\n",
             pFlowGraph->getSamplesPerFrame());

   osPrintf("  Samples Per Second:       %d\n",
             pFlowGraph->getSamplesPerSec());

   pResource = pFlowGraph->mpResourceInProcess;
   if (pResource == NULL)
      osPrintf("  Resource Being Processed: NULL\n");
   else
      osPrintf("  Resource Being Processed: %p\n", pResource);

   osPrintf("\n  Resource Information\n");
   osPrintf("    Resources:   %d\n", pFlowGraph->numResources());
   osPrintf("    Links: %d\n", pFlowGraph->numLinks());
   for (i=0; i < pFlowGraph->mResourceCnt; i++)
   {
      pResource = pFlowGraph->mUnsorted[i];
      pResource->resourceInfo(pResource, i);
   }
}

int flowGI(int x) {
   MpFlowGraphBase::flowGraphInfo((MpFlowGraphBase*) x);
   return 0;
}

// Returns the number of samples expected per frame.
int MpFlowGraphBase::getSamplesPerFrame(void) const
{
   return mSamplesPerFrame;
}

// Returns the number of samples expected per second.
int MpFlowGraphBase::getSamplesPerSec(void) const
{
   return mSamplesPerSec;
}

// Returns the current state of flow graph.
int MpFlowGraphBase::getState(void) const
{
   return mCurState;
}

OsMsgDispatcher* MpFlowGraphBase::getNotificationDispatcher(void) const
{
   return mNotifyDispatcher;
}

// Sets rpResource to point to the resource that corresponds to 
// name  or to NULL if no matching resource is found.
// Returns OS_SUCCESS if there is a match, otherwise returns OS_NOT_FOUND.
OsStatus MpFlowGraphBase::lookupResource(const UtlString& name,
                                         MpResource*& rpResource)
{
   OsReadLock lock(mRWMutex);
   return lookupResourcePrivate(name, rpResource);
}

// Sets rpResource to point to the resource that corresponds to 
// name  or to NULL if no matching resource is found.
// Returns OS_SUCCESS if there is a match, otherwise returns OS_NOT_FOUND.
OsStatus MpFlowGraphBase::lookupResourcePrivate(
   const UtlString& name,
   MpResource*& rpResource)
{
   UtlString key(name);

   rpResource = (MpResource*) mResourceDict.findValue(&key);

   if (rpResource != NULL)
      return OS_SUCCESS;
   else
      return OS_NOT_FOUND;
}

// Returns the number of links in the flow graph.
int MpFlowGraphBase::numLinks(void) const
{
   return mLinkCnt;
}

// Returns the number of frames this flow graph has processed.
int MpFlowGraphBase::numFramesProcessed(void) const
{
   return mPeriodCnt;
}

// Returns the number of resources in the flow graph.
int MpFlowGraphBase::numResources(void) const
{
   return mResourceCnt;
}

// Returns the message queue used by the flow graph. 
OsMsgQ* MpFlowGraphBase::getMsgQ(void) 
{
   return &mMessages ;
}

/* ============================ INQUIRY =================================== */

// Returns TRUE if the flow graph has been started, otherwise FALSE.
UtlBoolean MpFlowGraphBase::isStarted(void) const
{
   return (mCurState == STARTED);
}

/* //////////////////////////// PROTECTED ///////////////////////////////// */

/* //////////////////////////// PRIVATE /////////////////////////////////// */

// Computes the execution order for the flow graph by performing a 
// topological sort on the resource graph.
// Returns OS_SUCCESS if an execution order was successfully computed.
// Returns OS_LOOP_DETECTED is a loop was detected in the flow graph.
OsStatus MpFlowGraphBase::computeOrder(void)
{
   OsWriteLock       lock(mRWMutex);

   OsStatus          res;
   MpResourceSortAlg topoSort;

   res = topoSort.doSort(mUnsorted, mExecOrder, mResourceCnt);
   if (res == OS_SUCCESS)
      mRecomputeOrder = FALSE;

   return res;
}

// Disconnects all inputs (and the corresponding upstream outputs) for 
// the indicated resource.  Returns TRUE if successful, FALSE otherwise.
UtlBoolean MpFlowGraphBase::disconnectAllInputs(MpResource* pResource)
{
   int         i;
   MpResource* pUpstreamResource;
   int         upstreamPortIdx;

   if (pResource->numInputs() == 0)
      return TRUE;

   for (i = 0; i < pResource->maxInputs(); i++)
   {
      pResource->getInputInfo(i, pUpstreamResource, upstreamPortIdx);
      if (pUpstreamResource != NULL)
      {
         if (!handleRemoveLink(pUpstreamResource, upstreamPortIdx))
         {
             assert(FALSE);
             return FALSE;
         }
      }
   }

   return TRUE;
}

// Disconnects all outputs (and the corresponding downstream inputs) for 
// the indicated resource.  Returns TRUE if successful, FALSE otherwise.
UtlBoolean MpFlowGraphBase::disconnectAllOutputs(MpResource* pResource)
{
   int i;

   if (pResource->numOutputs() == 0)
      return TRUE;

   for (i = 0; i < pResource->maxOutputs(); i++)
   {
      if (pResource->isOutputConnected(i))
      {
         if (!handleRemoveLink(pResource, i))
         {
            assert(FALSE);
            return FALSE;
         }
      }
   }

   return TRUE;
}

// Handles an incoming message for the flow graph.
// Returns TRUE if the message was handled, otherwise FALSE.
UtlBoolean MpFlowGraphBase::handleMessage(OsMsg& rMsg)
{
   // Make sure that we have either a flowgraph message,
   // or a resource message.
   assert(rMsg.getMsgType() == OsMsg::MP_FLOWGRAPH_MSG);
   if (!(rMsg.getMsgType() == OsMsg::MP_FLOWGRAPH_MSG))
   {
      // If we don't have a resource message, return 
      // indicating the message wasn't handled.
      // TODO: Should also probably log to syslog
      return FALSE;
   }

   MpFlowGraphMsg* pMsg = (MpFlowGraphMsg*) &rMsg ;
   UtlBoolean retCode;
   MpResource* ptr1;
   MpResource* ptr2;
   int         int1;
   int         int2;

   retCode = FALSE;

   ptr1 = (MpResource*) pMsg->getPtr1();    // get the parameters out of
   ptr2 = (MpResource*) pMsg->getPtr2();    // the message
   int1 = pMsg->getInt1();
   int2 = pMsg->getInt2();

   switch (pMsg->getMsg())
   {
   case MpFlowGraphMsg::FLOWGRAPH_ADD_LINK:
      retCode = handleAddLink(ptr1, int1, ptr2, int2);
      break;
   case MpFlowGraphMsg::FLOWGRAPH_ADD_RESOURCE:
      retCode = handleAddResource(ptr1, int1);
      break;
   case MpFlowGraphMsg::FLOWGRAPH_DESTROY_RESOURCES:
      retCode = handleDestroyResources();
      break;
   case MpFlowGraphMsg::FLOWGRAPH_DISABLE:
      retCode = handleDisable();
      break;
   case MpFlowGraphMsg::FLOWGRAPH_ENABLE:
      retCode = handleEnable();
      break;
   case MpFlowGraphMsg::FLOWGRAPH_REMOVE_LINK:
      retCode = handleRemoveLink(ptr1, int1);
      break;
   case MpFlowGraphMsg::FLOWGRAPH_REMOVE_RESOURCE:
      retCode = handleRemoveResource(ptr1);
      break;
   case MpFlowGraphMsg::FLOWGRAPH_SYNCHRONIZE:
      retCode = handleSynchronize(*pMsg);
      break;
   case MpFlowGraphMsg::FLOWGRAPH_START:
      retCode = handleStart();
      break;
   case MpFlowGraphMsg::FLOWGRAPH_STOP:
      retCode = handleStop();
      break;
   default:
      break;
   }

   return retCode;
}

static void complainAdd(const char *n1, int p1, const char *n2,
   int p2, const char *n3, int p3)
{
   Zprintf("MpFlowGraphBase::handleAddLink(%s:%d, %s:%d)\n"
           " %s:%d is already connected!\n",
           (int) n1, p1, (int) n2, p2, (int) n3, p3);
}

UtlBoolean MpFlowGraphBase::handleSynchronize(MpFlowGraphMsg& rMsg)
{
   OsNotification* pSync = (OsNotification*) rMsg.getPtr1();
   char* tag = (char*) rMsg.getPtr2();
   int val1  = rMsg.getInt1();
   int val2  = rMsg.getInt2();

   if (0 != pSync) {
      pSync->signal(val1);
      // if (NULL != tag) osPrintf(tag, val1, val2);
#ifdef DEBUG_POSTPONE /* [ */
   } else {
      // just delay (postPone()), for debugging race conditions...
      OsTask::delay(rMsg.getInt1());
#endif /* DEBUG_POSTPONE ] */
   }
   return TRUE;
}

// Handle the FLOWGRAPH_ADD_LINK message.
// Returns TRUE if the message was handled, otherwise FALSE.
UtlBoolean MpFlowGraphBase::handleAddLink(MpResource* pFrom, int outPortIdx,
                                     MpResource* pTo,   int inPortIdx)
{
   // make sure that both resources are part of this flow graph
   if ((pFrom->getFlowGraph() != this) || (pTo->getFlowGraph() != this))
   {
      assert(FALSE);
      return FALSE;
   }

   // make sure both ports are free
   if (pFrom->isOutputConnected(outPortIdx))
   {
      complainAdd(pFrom->getName(), outPortIdx,
                  pTo->getName(), inPortIdx,
                  pFrom->getName(), outPortIdx);
      // assert(FALSE);
      return FALSE;
   }

   if (pTo->isInputConnected(inPortIdx))
   {
      complainAdd(pFrom->getName(), outPortIdx,
                  pTo->getName(), inPortIdx,
                  pTo->getName(), inPortIdx);
      // assert(FALSE);
      return FALSE;
   }

   // build the downstream end of the link
   if (pTo->connectInput(*pFrom, outPortIdx, inPortIdx) == FALSE)
   {
      assert(FALSE);
      return FALSE;
   }

   // build the upstream end of the link
   if (pFrom->connectOutput(*pTo, inPortIdx, outPortIdx) == FALSE)
   {                  // should not happen, but if it does we remove the 
      assert(FALSE);  //  downstream end of the link
      pTo->disconnectInput(inPortIdx);
      return FALSE;
   }

   mLinkCnt++;
   mRecomputeOrder = TRUE;

   return TRUE;
}

// Handle the FLOWGRAPH_ADD_RESOURCE message.
// Returns TRUE if the message was handled, otherwise FALSE.
UtlBoolean MpFlowGraphBase::handleAddResource(MpResource* pResource,
                                         UtlBoolean makeNameUnique)
{
   UtlString* pInsertedKey;
   UtlString* pKey;

   // make sure that we won't exceed the MAX_FLOWGRAPH_RESOURCES limit
   if (mResourceCnt >= MAX_FLOWGRAPH_RESOURCES)
   {
      assert(FALSE);
      return FALSE;
   }

   // make sure this resource isn't part of another flow graph
   if (pResource->getFlowGraph() != NULL)
   {
      assert(FALSE);
      return FALSE;
   }

   // add the resource to the dictionary
   // $$$ For now we aren't handling the makeNameUnique option, if the name
   // $$$ is not unique, the current code will trigger an assert failure
   pKey = new UtlString(pResource->getName());
   pInsertedKey = (UtlString*)
                  mResourceDict.insertKeyAndValue(pKey, pResource);
                  
   if (pInsertedKey == NULL)
   {                             // insert failed because of non-unique name
      delete pKey;               // clean up the key object
      assert(FALSE);             // $$$ for now, trigger an assert failure
      return FALSE;
   }

   // add the resource to the unsorted array of resources for this flow graph
   mUnsorted[mResourceCnt] = pResource;

   pResource->setFlowGraph(this);

   mResourceCnt++;
   mRecomputeOrder = TRUE;

   return TRUE;
}

// Handle the FLOWGRAPH_DESTROY_RESOURCES message.
// Returns TRUE if the message was handled, otherwise FALSE.
UtlBoolean MpFlowGraphBase::handleDestroyResources(void)
{
   int         i;
   int         numResources;
   MpResource* pResource;

   // if not already stopped, then stop the flow graph
   if (mCurState == STARTED)
      if (handleStop() == FALSE)
      {
         assert(FALSE);
         return FALSE;
      }

   // iterate over all resources

   // BE VERY CAREFUL HERE.  The handleRemoveResource() operation
   // SHUFFLES the array we are using to tell us what resources need
   // to be removed.

   // You have been warned.

   numResources = mResourceCnt;
   for (i=numResources-1; i >= 0; i--)
   {
      pResource = mUnsorted[i];

      // disconnect all inputs and outputs
      if ((disconnectAllInputs(pResource) == FALSE) ||
          (disconnectAllOutputs(pResource) == FALSE))
      {
         assert(FALSE);
         return FALSE;
      }

      // remove the resource from the flow graph
      if (handleRemoveResource(pResource) == FALSE)
      {
         assert(FALSE);
         return FALSE;
      }

      // destroy the resource
      delete pResource;
   }

   return TRUE;
}

// Handle the FLOWGRAPH_DISABLE message.
// Returns TRUE if the message was handled, otherwise FALSE.
UtlBoolean MpFlowGraphBase::handleDisable(void)
{
   int            i;
   MpFlowGraphMsg msg(MpFlowGraphMsg::RESOURCE_DISABLE);
   MpResource*    pResource;

   // iterate over all resources
   // invoke the disable() method for each resource in the flow graph
   for (i=0; i < mResourceCnt; i++)
   {
      // iterate through the resources
      pResource = mUnsorted[i];

      // make each resource handle a RESOURCE_DISABLE message
      msg.setMsgDest(pResource);
      if (!pResource->handleMessage(msg))
      {
         assert(FALSE);
         return FALSE;
      }
   }

   return TRUE;
}

// Handle the FLOWGRAPH_ENABLE message.
// Returns TRUE if the message was handled, otherwise FALSE.
UtlBoolean MpFlowGraphBase::handleEnable(void)
{
   int            i;
   MpFlowGraphMsg msg(MpFlowGraphMsg::RESOURCE_ENABLE);
   MpResource*    pResource;

   // iterate over all resources
   // invoke the enable() method for each resource in the flow graph
   for (i=0; i < mResourceCnt; i++)
   {
      // iterate through the resources
      pResource = mUnsorted[i];

      // make each resource handle a RESOURCE_ENABLE message
      msg.setMsgDest(pResource);
      if (!pResource->handleMessage(msg))
      {
         assert(FALSE);
         return FALSE;
      }
   }

   return TRUE;
}

// Handle the FLOWGRAPH_REMOVE_LINK message.
// Returns TRUE if the message was handled, otherwise FALSE.
UtlBoolean MpFlowGraphBase::handleRemoveLink(MpResource* pFrom, int outPortIdx)
{
   int         connectedPort;
   MpResource* pConnectedResource;

   // make sure the resource is part of this flow graph
   if (pFrom->getFlowGraph() != this)
   {
      Zprintf("handleRemoveLink: pFrom->getFlowGraph() != this: 0x%x != 0x%x\n",
         (int) (pFrom->getFlowGraph()), (int) this, 0,0,0,0);
      assert(FALSE);
      return FALSE;
   }

   // get information about the downstream end of the link
   pFrom->getOutputInfo(outPortIdx, pConnectedResource, connectedPort);

   // disconnect the upstream end of the link
   if (pFrom->disconnectOutput(outPortIdx) == FALSE)
   {
      Zprintf("handleRemoveLink: disconnectOutput(0x%x, %d) failed\n",
         (int) pFrom, outPortIdx, 0,0,0,0);
      assert(FALSE);    // couldn't disconnect
      return FALSE;
   }

   // disconnect the downstream end of the link
   if (pConnectedResource->disconnectInput(connectedPort) == FALSE)
   {
      Zprintf("handleRemoveLink: disconnectInput(0x%x, %d) failed\n",
         (int) pConnectedResource, connectedPort, 0,0,0,0);
      assert(FALSE);    // couldn't disconnect
      return FALSE;
   }

   mLinkCnt--;
   mRecomputeOrder = TRUE;

   return TRUE;
}

// Handle the FLOWGRAPH_REMOVE_RESOURCE message.
// Returns TRUE if the message was handled, otherwise FALSE.
UtlBoolean MpFlowGraphBase::handleRemoveResource(MpResource* pResource)
{
   UtlBoolean            found;
   int                  i;
   UtlString* pDictKey;
   UtlString* pKey;

   // make sure this resource is part of this flow graph
   if (pResource->getFlowGraph() != this)
   {
      Zprintf("handleRemoveResource:\n"
         "  pResource=0x%x, pResource->getFlowGraph()=0x%x, this=0x%x\n",
         (int) pResource, (int) (pResource->getFlowGraph()), (int) this, 0,0,0);
      assert(FALSE);
      return FALSE;
   }

   // remove all input links from this resource
   if (disconnectAllInputs(pResource) == FALSE)
   {
      assert(FALSE);
      return FALSE;
   }

   // remove all output links from this resource
   if (disconnectAllOutputs(pResource) == FALSE)
   {
      assert(FALSE);
      return FALSE;
   }

   // remove the entry from the dictionary for this resource
   pKey = new UtlString(pResource->getName());
   pDictKey = (UtlString*) mResourceDict.remove(pKey);
   delete pKey;

   if (pDictKey == NULL)
   {
      assert(FALSE);         // no entry found for the resource
      return FALSE;
   }
   delete pDictKey;          // get rid of the dictionary key for the entry

   // remove the resource from the unsorted array of resources for this graph
   found = FALSE;
   for (i=0; i < mResourceCnt; i++)
   {
      if (found)
      {                                   // shift entries following the
         mUnsorted[i-1] = mUnsorted[i];   //  deleted resource down by one
      }
      else if (mUnsorted[i] == pResource)
      {
         found = TRUE;
         mUnsorted[i] = NULL;      // clear the entry
      }
   }

   if (!found)
   {
      assert(FALSE);               // didn't find the entry
      return FALSE;
   }
   
   pResource->setFlowGraph(NULL);  // remove the reference to this flow graph

   mResourceCnt--;
   mUnsorted[mResourceCnt] = NULL;
   mRecomputeOrder = TRUE;

   return TRUE;
}

// Handle the FLOWGRAPH_START message.
// Returns TRUE if the message was handled, otherwise FALSE.
UtlBoolean MpFlowGraphBase::handleStart(void)
{
   mCurState  = STARTED;

   return TRUE;
}

// Handle the FLOWGRAPH_STOP message.
// Returns TRUE if the message was handled, otherwise FALSE.
UtlBoolean MpFlowGraphBase::handleStop(void)
{
   mCurState  = STOPPED;

   return TRUE;
}

// Posts a message to this flow graph.
// Returns the result of the message send operation.
OsStatus MpFlowGraphBase::postMessage(const MpFlowGraphMsg& rMsg,
                                  const OsTime& rTimeout)
{
   OsStatus res;

   res = mMessages.send(rMsg, rTimeout);
   return res;
}

// Processes all of the messages currently queued for this flow graph.
// For now, this method always returns OS_SUCCESS.
OsStatus MpFlowGraphBase::processMessages(void)
{
#ifdef RTL_ENABLED
    RTL_BLOCK("MpFlowGraphBase.processMessages");
#endif

   OsWriteLock     lock(mRWMutex);

   UtlBoolean       handled = FALSE;
   static MpFlowGraphMsg* pStopMsg = NULL;
   MpResource*     pMsgDest = NULL;
   
   OsStatus        res;

   // First, we send ourselves a FLOWGRAPH_PROCESS_FRAME message.
   // This message serves as a "stopper" in the message queue.  When we
   // handle that message, we know that we have processed all of the messages
   // for the flowgraph (and its resources) that had arrived prior to the
   // start of this frame processing interval.

   if (NULL == pStopMsg) {
      pStopMsg = new MpFlowGraphMsg(MpFlowGraphMsg::FLOWGRAPH_PROCESS_FRAME);
      pStopMsg->setReusable(TRUE);
   }

   res = postMessage(*pStopMsg);
   assert(res == OS_SUCCESS);

   UtlBoolean done = FALSE;
   while (!done)
   {                  
      // get the next message
      OsMsg* pMsg ;

      res = mMessages.receive(pMsg, OsTime::NO_WAIT_TIME);      

      assert(res == OS_SUCCESS);
      
      if (pMsg->getMsgType() == OsMsg::STREAMING_MSG)
      {
         handleMessage(*pMsg);
      }
      else if (pMsg->getMsgType() == OsMsg::MP_FLOWGRAPH_MSG)
      {
         MpFlowGraphMsg* pFlowgraphMsg = (MpFlowGraphMsg*) pMsg ;
         // determine if this message is intended for a resource in the
         // flow graph (as opposed to a message for the flow graph itself)
         pMsgDest = pFlowgraphMsg->getMsgDest();


         if (pMsgDest != NULL)
         {
            // deliver the message if the resource is still part of this graph
            if (pMsgDest->getFlowGraph() == this)
            {
               handled = pMsgDest->handleMessage(*pFlowgraphMsg);
               assert(handled);
            }
         }
         else
         {
            // since pMsgDest is NULL, this msg is intended for the flow graph
            switch (pFlowgraphMsg->getMsg())
            {
            case MpFlowGraphMsg::FLOWGRAPH_PROCESS_FRAME:
               done = TRUE;    // "stopper" message encountered -- we are done
               break;          //  processing messages for this frame interval

            default:
               handled = handleMessage(*pFlowgraphMsg);
               assert(handled);
               break;
            }
         }
      }
      else if (pMsg->getMsgType() == OsMsg::MP_RESOURCE_MSG)
      {
         MpResourceMsg* pResourceMsg = 
            dynamic_cast<MpResourceMsg*>(pMsg);
         assert(pResourceMsg != NULL);
         if (pResourceMsg != NULL)
         {
            // If the destination resource name is null, 
            // then treat that as a desire to send this message to all
            // resources.
            if(pResourceMsg->getDestResourceName().isNull())
            {
               // Send this message to all resources in the flowgraph.
               int i;
               for(i = 0; i < mResourceCnt; i++)
               {
                  handled = mUnsorted[i]->handleMessage(*pResourceMsg);
                  assert(handled);
                  if(!handled)
                  {
                     OsSysLog::add(FAC_MP, PRI_WARNING, 
                                   "MpFlowGraphBase::processMessages: "
                                   "Resource message subtype %d directed to all "
                                   "resources failed when sending to resource %s.",
                                   pResourceMsg->getMsgSubType(),
                                   mUnsorted[i]->getName());
                  }
               }
            }
            else
            {
               // From the resource message, get the name of the resource, and look it up.
               // If we find it, we call the resource's handleMessage method.
               res = lookupResourcePrivate(pResourceMsg->getDestResourceName(), pMsgDest);
               assert(res == OS_SUCCESS);
               assert(pMsgDest != NULL);

               if (pMsgDest != NULL)
               {
                  handled = pMsgDest->handleMessage(*pResourceMsg);
                  assert(handled);
               }
               else
               {
                  OsSysLog::add(FAC_MP, PRI_DEBUG,
                     "MpFlowGraphBase::processMessages - "
                     "Failed looking up resource!: "
                     "name=\"%s\", lookupResource status=0x%X, "
                     "resource pointer returned = 0x%X",
                     pResourceMsg->getDestResourceName().data(), 
                     res, pMsgDest);
               }
            }
         }
         else // pResourceMsg == NULL
         {
            OsSysLog::add(FAC_MP, PRI_DEBUG,
               "MpFlowGraphBase::processMessages - "
               "message type field indicated it was an MP_RESOURCE_MSG "
               "but actual language type was not an MP_RESOURCE_MSG: "
               "msgType==0x%X, msgSubType=0x%X",
               pMsg->getMsgType(), pMsg->getMsgSubType());
         }
      }
      else
      {
         // We shouldn't get here.  If we do, then someone was dumb
         // and stuck some weird messages in the queue with types
         // other than MP_FLOWGRAPH_MSG and MP_RESOURCE_MSG
         assert(0);
         OsSysLog::add(FAC_MP, PRI_DEBUG, 
            "MpFlowGraphBase::processMessages - "
            "SAW WEIRD MESSAGE!: "
            "msgType==0x%X, msgSubType=0x%X",
            pMsg->getMsgType(), pMsg->getMsgSubType());
      }

      // We're done with the message, we can release it now.
      pMsg->releaseMsg();
   }

   return OS_SUCCESS;
}

/* ============================ FUNCTIONS ================================= */


