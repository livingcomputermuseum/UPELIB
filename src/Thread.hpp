//++
// Thread.hpp -> Simple, lightweight, platform independent, thread object ...
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
// REVISION HISTORY:
// 31-MAY-17  RLA   New file.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
//--
#pragma once
#include <string>               // C++ std::string class, et al ...
using std::string;              // ...
#ifdef __linux__
#include <semaphore.h>          // we need a definition for sem_t
#include <pthread.h>            // we also need pthread_t ...
#endif


//++
//   The THREAD_ATTRIBUTES macro declares any special attributes required for
// the main thread routine.  Unfortunately the calling sequence for thread
// routines is slightly different between Windows and Unix, so we have to do
// it this way...
//--
#ifdef _WIN32
#define THREAD_ATTRIBUTES __cdecl
#elif __linux__
#define THREAD_ATTRIBUTES
#endif

//++
//   A THREAD_ROUTINE is the prototype for a thread's main routine.  Strictly
// speaking this isn't the same between Windows and Linux - Linux requires 
// that it return a void* value and Windows doesn't want any return value.
// It turns out that Windows doesn't really care what we return (the return
// value is ignored anyway), so we can cheat and use the same definition on
// both.  This cheating saves more than just another typedef here because every
// thread routine needs to end with a "return NULL" and we'd have to #ifdef
// that out on Windows.
//--
typedef void* (THREAD_ATTRIBUTES *THREAD_ROUTINE) (void *pParam);

//++
//   And lastly, a THREAD_ID is an indentification for any thread.  Exactly
// what this value means is operating system specific, but it's guaranteed
// to be unique.  The code outside this module can use this ID to uniquely
// identify a thread, and that's all we need.
//
//   Unfortunately the code outside this module makes a couple of assumptions
// about the THREAD_ID type - that it's compatible with an integer, and that
// zero is never used as a thread ID.  Unused ID variables are initialized
// to zero and the code tests for that value.  Sorry - it's not the best, but
// it isn't really a problem.
//--
#ifdef _WIN32
//   A thread ID is really a DWORD under Windows but using that type would
// require that we include Windows.h, and we'd really rather not.  This is
// cheating a bit, but it works fine.
typedef uint32_t THREAD_ID;
typedef uint32_t PROCESS_ID;
#elif __linux__
// On Linux a thread ID is a pthread_t type ...
typedef pthread_t THREAD_ID;
typedef uint32_t PROCESS_ID;
#endif


class CThread {
  //++
  //   Super simple, lightweight, portable thread implementation.  These
  // declarations are platform independent, but the implementation is not!
  //--

  // Constructors and destructor ...
public:
  CThread(THREAD_ROUTINE pThread, const char *pszName=NULL, uint32_t nParameters=1, uint32_t nFlags=0);
  virtual ~CThread();
private:
  // Disallow copy and assignment operations with CThread objects...
  CThread(const CThread &t) = delete;
  CThread& operator= (const CThread &t) = delete;

  // Thread attributes ...
public:
  // Return TRUE if this thread is still running ...
  bool IsRunning() const;
  // Set or test the ExitRequested flag ...
  void RequestExit() {m_fExitRequested = true;}
  bool IsExitRequested() const {return m_fExitRequested;}
  // Get or set the thread name (used for error and status messages!) ...
  void SetName (const char *psz) {m_sName = psz;}
  string GetName() const {return m_sName;}
  // Get or set the thread's parameter ...
  void SetParameter (void *pParam, uint32_t nParam=0)
    {assert(nParam==0);  m_pParameter = pParam;}
  void *GetParameter (uint32_t nParam=0) const
    {assert(nParam==0);  return m_pParameter;}
  // Get the thread's identification ...
  THREAD_ID GetID() const {assert(m_idThread != 0);  return m_idThread;}
  // Get the identification of the current thread or process ...
  static THREAD_ID GetCurrentThreadID();
  static PROCESS_ID GetCurrentProcessID();

  // Thread methods ...
public:
  // Begin and end thread execution ...
  bool Begin();
  void *End (void *pResult=NULL);
  // Wait for thread completion ...
  void Wait();
  // Do a RequestExit() followed by a Wait() ...
  void WaitExit();
  // Force a thread to terminate, regardless of what it's doing ...
  void ForceExit();
  // Set this thread's priority to below normal (background) ...
  void SetBackgroundPriority();
  // Raise or wait for an event (aka semaphore) ...
  void RaiseFlag (uint32_t nFlag=0);
  bool WaitForFlag (uint32_t nTimeout, uint32_t nFlag=0);

  // Local members ...
protected:
  THREAD_ROUTINE m_pRoutine;        // address of the thread's main routine
  string         m_sName;           // thread name (for messages!)
  uint32_t       m_nFlags;          // number of flags (currently 0 or 1!)
  void          *m_pParameter;      // the one and only thread parameter
  bool           m_fExitRequested;  // TRUE to request that the thread exit
  THREAD_ID      m_idThread;        // THREAD_ID assigned to this thread
#ifdef _WIN32
  intptr_t       m_hThread;         // handle of this thread
  intptr_t       m_hFlag;           // event for RaiseFlag()/WaitForFlag()
#elif __linux__
  sem_t         *m_pFlag;           // semaphore for RaiseFlag()/WaitForFlag()
#endif
};
