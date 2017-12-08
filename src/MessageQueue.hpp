//++
// MessageQueue.hpp -> UPE Library log file message queue class
//
//
//       COPYRIGHT (C) 2015-2017 Vulcan Inc.
//       Developed by Living Computers: Museum+Labs
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
// REVISION HISTORY:
// 14-DEC-15  RLA   New file.
//--
#pragma once
#include "Mutex.hpp"            // needed for CMutex ...
#include "Thread.hpp"           // needed for THREAD_ATTRIBUTES and CThread ...


// CMessageQueue class definition ...
class CMessageQueue {
  //++
  //--

  // Constants ...
public:
  enum {
  };

  // This is the structure of a message queue entry ...
protected:
  struct _QENTRY {
    CLog::SEVERITY nLevel;      // message level - ERROR, WARNING, DEBUG, etc
    bool fToConsole;            // TRUE to send this message to the console
    bool fToLog;                // TRUE to send this message to the log file
    char szText[CLog::MAXMSG];  // the actual text of the message
    CLog::TIMESTAMP tmNow;      // time this message was originally logged
    struct _QENTRY *pNext;      // next entry in the message queue
  };
  typedef struct _QENTRY QENTRY;

  // Constructors and destructor ...
public:
  CMessageQueue();
  virtual ~CMessageQueue();
private:
  CMessageQueue (const CMessageQueue &lq);
  CMessageQueue& operator= (const CMessageQueue &lq);

  // Public CMessageQueue properties ...
public:

  // Public CMessageQueue methods ...
public:
  // Create and destroy queue entries ...
  QENTRY *NewEntry();
  QENTRY *NewEntry (CLog::SEVERITY nLevel, const char *pszText, bool fToConsole, bool fToLog, const CLog::TIMESTAMP *ptm=NULL);
  QENTRY *AddEntry (QENTRY *pEntry);
  QENTRY *AddEntry (CLog::SEVERITY nLevel, const char *pszText, bool fToConsole, bool fToLog, const CLog::TIMESTAMP *ptm=NULL)
    {return AddEntry(NewEntry(nLevel, pszText, fToConsole, fToLog, ptm));}
  QENTRY *RemoveEntry();
  void FreeEntry (QENTRY *pEntry);
  // Start or stop the background logging thread ...
  bool IsLoggingThreadRunning() const {return m_LoggingThread.IsRunning();}
  bool BeginLoggingThread();
  void EndLoggingThread();
  void WakeLoggingThread() {m_LoggingThread.RaiseFlag();}

  // Private CMessageQueue methods ...
private:
  // The background task that manages this Channel ...
  static void* THREAD_ATTRIBUTES LoggingThread (void *pParam);

  // Local members ...
private:
  QENTRY     *m_pQueueHead;     // place where messages are removed for printing
  QENTRY     *m_pQueueTail;     // place where messages are added when logged
  QENTRY     *m_pFreeList;      // free list of unused QENTRY blocks
  CMutex      m_QueueLock;      // CRITICAL_SECTION lock message queue
  CMutex      m_FreeLock;       // CRITICAL_SECTION lock free list
  uint32_t    m_nQueueEntries;  // count of QENTRY blocks allocated
  CThread     m_LoggingThread;  // background thread to do the checkpoints
};
