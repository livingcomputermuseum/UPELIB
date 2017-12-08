//++
// MessageQueue.hpp -> UPE Library log file message queue methods
//
//       COPYRIGHT (C) 2017 BY THE LIVING COMPUTER MUSEUM, SEATTLE WA.
//
// LICENSE:
//    This file is part of the UPE LIBRARY project.  UPELIB is free software;
// you may redistribute it and/or modify it under the terms of the GNU Affero
// General Public License as published by the Free Software Foundation, either
// version 3 of the License, or (at your option) any later version.
//
//    UPELIB is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License for
// more details.  You should have received a copy of the GNU Affero General
// Public License along with MBS.  If not, see http://www.gnu.org/licenses/.
//
// DESCRIPTION:
//   This module implements a simple FIFO queue for buffering console and log
// file messages, and also a background thread for taking messages from this
// queue and logging them later. The problem is that directly logging trace and
// debug messages slows down the CDC channel threads to the point where they
// can't meet the PP timeout requirements.  Buffering the messages and then 
// logging them in the background helps, although it doesn't completely fix
// the problem.
//
//    We could use one of the nifty STL classes to implement our queue, but
// there's really no need.  We use a simple structure, QENTRY, to hold each
// message and singly linked list  to implement the FIFO. The member variable
// m_pQueueHead points to the first message to remove (i.e. the oldest entry
// in the queue) and m_pQueueTail points to the most recent queue entry. Adding
// and removing entries takes just a tiny bit of pointer prestidigitation.
//
//   We keep a free list of unused QENTRY nodes so that we don't have to call
// new and delete for every message.  When we need a new QENTRY we first check
// the free list for one and, if none are free, only then do we allocate one.
// We NEVER (well, almost never) call delete - free entries are simply put on
// the free list.  That's a bit wasteful, but there's plenty of memory to go
// around.  The only time delete is called is by the class destructor, so that
// all memory is eventually returned.
//
// Bob Armstrong <bob@jfcl.com>   [14-DEC-2015]
//
// REVISION HISTORY:
// 14-DEC-15  RLA   New file.
// 28-FEB-17  RLA   Make 64 bit clean.
//  1-JUN-17  RLA   Linux port.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include <string.h>             // strcpy(), memset(), strerror(), etc ...
#include <sys/timeb.h>          // struct __timeb, ftime(), etc ...
#ifdef _WIN32
#include <wtypes.h>             // Windows types for WaitForSingleObject() ... 
#elif __linux__
#endif
#include "Mutex.hpp"            // CMutex critical section lock
#include "Thread.hpp"           // CThread portable thread library
#include "SafeCRT.h"		// replacements for Microsoft "safe" CRT functions
#include "UPELIB.hpp"           // global declarations for this library
#include "LogFile.hpp"          // file and console logging methods
#include "MessageQueue.hpp"     // declarations for this class


CMessageQueue::CMessageQueue()
  : m_LoggingThread(&CMessageQueue::LoggingThread, "message logging", 0, 1)
{
  //++
  // Initialize all member variables ...
  //--
  m_pQueueHead = m_pQueueTail = m_pFreeList = NULL;  m_nQueueEntries = 0;
  m_LoggingThread.SetParameter(this);
}


CMessageQueue::~CMessageQueue()
{
  //++
  //   Dispose of this object, being sure to delete any entries on either the
  // message queue or the free list.
  //--
  EndLoggingThread();
  while (m_pQueueHead != NULL) {
    QENTRY *p = m_pQueueHead;  m_pQueueHead = p->pNext;  delete p;
  }
  while (m_pFreeList != NULL) {
    QENTRY *p = m_pFreeList;  m_pFreeList = p->pNext;  delete p;
  }
}


CMessageQueue::QENTRY *CMessageQueue::NewEntry()
{
  //++
  //   This method creates a new queue entry and zeros it out.  If there's
  // already an entry on the free list, then we can save ourselves a lot
  // of time by just using that one.  If the free list is empty, though,
  // then we'll allocate space for a brand new entry.
  //--
  QENTRY *p;
  m_FreeLock.Enter();
  if (m_pFreeList != NULL) {
    p = m_pFreeList;  m_pFreeList = p->pNext;
    m_FreeLock.Leave();
  } else {
    m_FreeLock.Leave();
    p = DBGNEW QENTRY;  ++m_nQueueEntries;
  }
  memset(p, 0, sizeof(QENTRY));
  return p;
}


CMessageQueue::QENTRY *CMessageQueue::NewEntry (CLog::SEVERITY nLevel, const char *pszText, bool fToConsole, bool fToLog, const CLog::TIMESTAMP *ptm)
{
  //++
  //   This method creates a new queue entry and initializes all the fields
  // from the parameter values specified.  Note that we have to save the ToLog
  // and ToConsole destination flags here rather than computing them when the
  // message is actually logged because message levels are thread specific.
  // We need the message level for the current thread to determine whether this
  // message should eventually be sent to the console, log file, or both.
  //
  //   The ptm structure specifies the timestamp for this message.  If it is
  // NULL, then the current time will be used instead.  If it is not NULL, then
  // the TIMESTAMP structure will be copied (it's only a dozen bytes or so)
  // because we can't depend on the caller's structure still being valid when
  // this message is recalled.
  //
  //   Likewise note that the actual text of the message is copied into the
  // QENTRY for the same reason.
  //--
  assert(pszText != NULL);
  QENTRY *p = NewEntry();
  p->nLevel = nLevel;  p->fToConsole = fToConsole;  p->fToLog = fToLog;
  strcpy_s(p->szText, CLog::MAXMSG, pszText);
  if (ptm != NULL)
    memcpy(&(p->tmNow), ptm, sizeof(CLog::TIMESTAMP));
  else
    CLog::GetTimeStamp(&(p->tmNow));
  return p;
}


CMessageQueue::QENTRY *CMessageQueue::AddEntry (QENTRY *pEntry)
{
  //++
  //   This method adds an entry to the message queue.  Remember that the queue
  // tail pointer is where we add entries (the head is where we remove them)!
  //--
  assert(pEntry != NULL);
  m_QueueLock.Enter();
  //   Usually the old tail (the previous last queue item) will now point to
  // this one, and this one then becomes the new tail.  Be careful, though
  // because if the queue is empty then the tail will be NULL!
  if (m_pQueueTail != NULL) m_pQueueTail->pNext = pEntry;
  m_pQueueTail = pEntry;
  // If the queue is empty, then this item is now also the head ...
  if (m_pQueueHead == NULL) m_pQueueHead = pEntry;
  m_QueueLock.Leave();
  WakeLoggingThread();
  return pEntry;
}


CMessageQueue::QENTRY *CMessageQueue::RemoveEntry()
{
  //++
  //   This routine removes an entry from the head of the queue (remember - new
  // items are added at the tail and removed from the head!).  If the queue is
  // currently empty, then NULL is returned.
  //
  //  BE SURE TO CALL FreeEntry() WHEN YOU'RE DONE PROCESSING THIS ENTRY!
  //--
  m_QueueLock.Enter();
  if (m_pQueueHead == NULL) {
    // If the head is NULL, then the queue is empty ...
    m_QueueLock.Leave();  return NULL;
  }
  //   Pull the next entry off the head of the list, and then the old second
  // item on the list becomes the new head.  Note that if the list currently
  // has only one item then the queue head and tail both point to the same
  // thing.  When we remove the first item the queue head will become null and
  // it's important to make sure the tail becomes null too!
  QENTRY *p = m_pQueueHead;  m_pQueueHead = p->pNext;
  if (m_pQueueHead == NULL) m_pQueueTail = NULL;
  m_QueueLock.Leave();
  //   This item isn't part of the list anymore, so zero the next pointer
  // just so it isn't left still pointing to the item behind it.  This reallly
  // shouldn't be necessary, but just in case...
  p->pNext = NULL;
  return p;
}


void CMessageQueue::FreeEntry (QENTRY *pEntry)
{
  //++
  //   This routine will free a queue entry.  It simply adds the item to the
  // free list - note that we never, ever, delete queue entries once they've
  // been allocated!
  //
  //   One subtle point - while the message queue is a LIFO structure, the
  // free list is actually a FIFO.  That means if there are several free
  // entries then the same message entry keeps getting used over and over
  // again.  That's harmless and makes the code a little easier.
  //--
  assert(pEntry != NULL);
  m_FreeLock.Enter();
  pEntry->pNext = m_pFreeList;  m_pFreeList = pEntry;
  m_FreeLock.Leave();
}


void* THREAD_ATTRIBUTES CMessageQueue::LoggingThread (void *pParam)
{
  //++
  //   This is the background message logging thread.  It pulls as many
  // messages as it can from the message queue and writes them to the console
  // and/or log file.  When the queue is finally empty, it goes to sleep.
  //--
  assert(pParam != NULL);
  CThread *pThread = (CThread *) pParam;
  CMessageQueue *pQueue = static_cast<CMessageQueue *>(pThread->GetParameter());
//LOGS(DEBUG, "message logging thread started");
  while (true) {
    QENTRY *pEntry;
    while ((pEntry = pQueue->RemoveEntry()) != NULL) {
      if (pEntry->fToConsole)
        CLog::GetLog()->SendConsole(pEntry->nLevel, pEntry->szText);
      if (pEntry->fToLog)
        CLog::GetLog()->SendLog(pEntry->nLevel, pEntry->szText, &(pEntry->tmNow));
      pQueue->FreeEntry(pEntry);
    }
    if (pThread->IsExitRequested()) break;
    pThread->WaitForFlag(100);
  }
  LOGS(DEBUG, "message logging thread terminated");
  return pThread->End();
}


bool CMessageQueue::BeginLoggingThread()
{
  //++
  //   This method will start up the background message logging thread, and
  // it returns true if everything is OK and false otherwise.  Once the thread
  // is started, it will try to set the thread priority to background level.
  //
  //   BTW, if this method fails (i.e. it returns false) then the message
  // logging thread is NOT running and without that there's no way for messages
  // in the queue to get logged!  The only safe options in that case are to
  // either a) stop using the message queue, or b) just exit completely.
  //--
  if (!m_LoggingThread.Begin()) return false;
  m_LoggingThread.SetBackgroundPriority();
  return true;
}


void CMessageQueue::EndLoggingThread()
{
  //++
  //   This method shuts down the message logging thread.  Usually it's called
  // by the CMessageQueue destructor, but it can be called explicitly if that's
  // necessary.  Remember, though - if you shut down the logging thread then
  // any messages you put in the queue (e.g. any calls to LOGS() or LOGF())
  // won't go anywhere!
  //--
  if (!IsLoggingThreadRunning()) return;
  //LOGS(DEBUG, "waiting for message logging thread to terminate");
  m_LoggingThread.RequestExit();  WakeLoggingThread();
  m_LoggingThread.WaitExit();
}
