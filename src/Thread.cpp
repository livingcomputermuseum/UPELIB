//++
// Thread.cpp -> Simple, platform independent, thread object ...
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
// DESCRIPTION:
//   This class implements a simple CThread object which is platform independent
// and works on both Windows and Linux.  Both operating systems have the concept
// of threads (aka light weight processes) but the implementation and especially
// the terminology differs.  In the case of Windows we use the _beginthread()
// and _endthread() methods to create and destroy the threads, and the Windows
// WaitForSingleObject() to synchronize with thread termination.  On Linux we
// use the pthreads library and the pthread_create(), pthread_exit() and 
// pthread_join() routines to accomplish the same thing.
//
//   The threading model we use is pretty trivial - the "body" of a thread is
// a subroutine which must be declared with the THREAD_ATTRIBUTES modifier.
// On Windows this gives the routine the __cdecl calling conventions, but on
// Linux it does nothing.  Each thread routine receives a single parameter
// which is the pointer to THIS OBJECT, the one and only CThread object that
// created it.
//
// THREAD CREATING AND TERMINATION
//   Creating the CThread object DOES NOT automatically start execution of the
// thread.  You do that by calling the Begin() method on the CThread object. A
// thread can terminate itself by calling the End() method on its own CThread
// object, or the parent pprocess can forcibly terminate the thread by doing
// the same.
//
// EXIT REQUEST
//   The CThread object also contains an ExitRequested flag, which anybody may
// set by calling the RequestExit() method on the CThread object.  The thread
// code can test this flag periodically with the IsExitRequested() method and,
// if an exit is requested, the thread can exit by calling End().  This is
// strictly voluntary, however, and an exit request does not force a thread to
// terminate in any way.  Note that our use of the ExitRequested flag, et al,
// corresponds more or less to the pthreads "cancellation point" concept.
// Windows doesn't have this, so we do it ourselves.
//
// THREAD PARAMETERS
//   As already mentioned, each thread routine gets one explict parameter, the
// address of this CThread object.  This object implements a more general
// purpose parameter passing mechanism, however, and the creating thread can
// optionally call the CThread SetParameter() method before the thread is
// started.  A thread can then retrieve its parameter by calling the CThread
// GetParameter() method.  The thread routine will need to use the pointer to
// CThread object to do this.
//
//   The current implementation allows only one parameter because that's all
// we usually need, however it'd be a simple matter to add an array or some
// collection class to CThread and allow any number of parameters.  Also there
// is no rule which says that parameters flow only from the parent to the
// child threads - there's no reason why the child can't call SetParameter()
// during execution and the parent can call GetParameter() after the thread
// terminates.
//   
//   NOTE that the parameters are shared by the parent thread and its child,
// but there is ABSOLUTELY NO MUTEX OR CRITICAL SECTION USED TO INTERLOCK
// ACCESS to the parameters.  It's assumed that the parent thread will call
// SetParameter() once before starting the thread with Begin(), and then the
// thread routine will call GetParameter() later to get the parameter value.
// If a parameter is used to return a value then the reverse is true - it's
// assumed that the parent won't call GetParameter() until after the child
// has terminated.  For that exchange no mutex is needed, however if you are
// planning any parameter exchanges WHILE THE THREAD IS ACTIVE, then you 
// will need to add some kind of mutex to SetParameter()/GetParameter().
//
// EVENTS/SEMAPHORES/FLAGS
//   It's handy for one thread to have some way to "signal" (and I'm not using
// that word in the Un*x sense!) another.  This happens, for example, in the
// case of the message logging thread.  A queue is kept of the messages waiting
// to be logged, and the message logger thread takes messages from the queue
// and writes them out.  When the queue is empty the logger thread should go
// to sleep, but we need some way to wake it up when a new message is added to
// the queue.
//
//   To do this, CThread implements what we call a "flag".  Windows calls this
// an Event and Linux would call it a Semaphore, but they're all the same thing.
// When the child thread runs out of things to do, it can call the CThread
// WaitForFlag() routine and that will block the child thread until some other
// thread calls the RaiseFlag() method.
//
//   Note that at present we're limited to only one flag, the same way that
// we're limited to one parameter, but there's no real reason why that couldn't
// be extended to more than one.
//
// Bob Armstrong <bob@jfcl.com>   [31-MAY-2017]
//
// REVISION HISTORY:
// 31-MAY-17  RLA   New file.
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
//--
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <process.h>            // needed for _beginthread(), et al ...
#include <Windows.h>            // Sleep() and WaitForSingleObject() ...
#elif __linux__
#include <sys/types.h>          // pid_t and others ...
#include <unistd.h>             // getpid(), etc ...
#include <pthread.h>            // pthread_t, pthread_create(), and so on ...
#include <semaphore.h>          // sem_init(), sem_wait(), and all the rest ...
#endif
#include "UPELIB.hpp"           // UPE library definitions
#include "Thread.hpp"           // declarations for this module
#include "LogFile.hpp"          // UPE library message logging facility


CThread::CThread (THREAD_ROUTINE pThread, const char *pszName, uint32_t nParameters, uint32_t nFlags)
{
  //++
  //   Create and initialize a new thread object.  The pszName parameter, if
  // specified, can be used in place of SetName() to set the name of this thread.
  // The nParameters and nFlags parameters set the number of thread parameters
  // and flags, respectively.  Currently the maximum number for both is one,
  // and the value specified in either case must be 0 or 1.
  //
  //   Note that this routine stores the value for the number of flags/events/
  // semaphores requested, but they aren't actually created until the Begin()
  // method is called.  That's because event/semaphore creation can possibly
  // fail, and there's no easy way for a constructor to report failure.
  //--
  assert((pThread != NULL) && (nParameters <= 1) && (nFlags <= 1));
  m_pRoutine = pThread;  m_pParameter = NULL;  m_idThread = 0;
  m_fExitRequested = false;  m_nFlags = nFlags;
  m_sName = (pszName != NULL) ? pszName : "<UNKNOWN>";
  #ifdef _WIN32
    m_hThread = m_hFlag = 0;
  #elif __linux__
    m_pFlag = NULL;
  #endif
}


CThread::~CThread()
{
  //++
  // Destroy this thread object ...
  //--
  if (IsRunning()) ForceExit();
#ifdef _WIN32
  if (m_hFlag != 0) CloseHandle((HANDLE) m_hFlag);
  m_hFlag = 0;
#elif __linux__
  if (m_pFlag != NULL) {
    sem_destroy(m_pFlag);  delete m_pFlag;
  }
  m_pFlag = NULL;
#endif
}


bool CThread::Begin()
{
  //++
  //   This method will begin execution of this thread.  This method returns
  // immediately and the new thread runs autonomously until either End()
  // method is called OR the thread routine returns.
  //--
  LOGS(DEBUG, "starting thread for " << GetName());
  m_fExitRequested = false;
#ifdef _WIN32
  //   On Linux the thread routine must return a void * pointer, but on Windows
  // there is no return value.  In reality Windows doesn't care what we return,
  // so we simply re-cast the pointer to make the compiler happy ...
  m_hThread = _beginthread((void (__cdecl *)(void *)) m_pRoutine, 0, (void *) this);
  m_idThread = GetThreadId((HANDLE) m_hThread);
  if ((m_hThread == -1L) || (m_idThread == 0)) {
    LOGS(ERROR, "unable to create thread for " << GetName());  return false;
  }
  // If an event flag is required then create it now ...
  if ((m_nFlags > 0) && (m_hFlag == 0)) {
    m_hFlag = (intptr_t) CreateEvent(NULL, false, true, NULL);
    if (m_hFlag == 0) {
      LOGS(ERROR, "unable to create event flag for " << GetName());  return false;
    }
  }
#elif __linux__
  // Create the child thread ....
  int err = pthread_create(&m_idThread, NULL, m_pRoutine, (void *) this);
  if (err != 0) {
    LOGS(ERROR, "error " << err << " creating thread for " << GetName());  return false;
  }
  // And if a semaphore is needed, now is the time to create it ...
  if ((m_nFlags > 0) && (m_pFlag == NULL)) {
    m_pFlag = DBGNEW sem_t;
    err = sem_init(m_pFlag, 0, -1);
    if (err != 0) {
      LOGS(ERROR, "error " << err << " creating semaphore for " << GetName());  return false;
    }
  }
#endif
  //   This delay isn't really necessary, but it gives the new thread a chance
  // to print out any initial debugging and startup messages before the background
  // starts the user interface.
  _sleep_ms(100);
  return true;
}


void *CThread::End (void *pResult)
{
  //++
  //   This method should be called from the thread's main procedure as the
  // very last step before returning.  All it does is to tell this CThread
  // object that the thread is finished, and to clean up accordingly.  This
  // method doesn't actually _force_ the thread to exit - it's still up to
  // the thread to return from it's main procedure.  It also cannot be called
  // from another thread to force this thread to terminate (you should use one
  // of the RequestExit(), WaitExit() or ForceExit() methods for that).
  //
  //   The return value of this procedure is just simply it's parameter, or
  // NULL if none is specified.  Why do that?  Because Linux insists that a
  // thread procecure must return a void * value, and this makes it easy to
  // write something like
  //
  //        return pMyThread->End();
  //
  // as the last line in the thread procedure.  That's all...
  //--
#ifdef _WIN32
  _endthread();
#elif __linux__
  pthread_exit(pResult);
#endif

  // This is just to make the compiler happy - it never gets executed!
  return pResult;
}


void CThread::Wait()
{
  //++
  //   This method will wait until the thread terminates.  There is no timeout,
  // so it can potentially wait forever.  You've been warned!
  //--
  if (!IsRunning()) return;
  LOGS(DEBUG, "waiting for " << GetName() << " thread to exit");
#ifdef _WIN32
  WaitForSingleObject((HANDLE) m_hThread, INFINITE);
#elif __linux__
  void *pResult;
  int err = pthread_join(m_idThread, &pResult);
  if (err != 0)
    LOGS(ERROR, "error " << err << " in join for " << GetName());
#endif
}


void CThread::WaitExit()
{
  //++
  //   This method simply does a RequestExit() followed by a Wait() ...
  // It's the simplest way to cleanly and voluntarily ask a thread to exit.
  //--
  if (IsRunning()) {
    RequestExit();  Wait();
  }
}


bool CThread::IsRunning() const
{
  //++
  //   This method returns TRUE if this thread is still running, and FALSE if
  // it has exited (or has never been started).
  //--
#ifdef _WIN32
  return m_hThread != 0;
#elif __linux__
  return m_idThread != 0;
#endif
}


void CThread::SetBackgroundPriority()
{
  //++
  //   This routine will attempt to set the priority of this thread to below
  // normal or background level.  Exactly what result this might have is system
  // specific, but we'll do our best...
  //--
#ifdef _WIN32
  assert(m_hThread != 0);
  if (GetPriorityClass(GetCurrentProcess()) == NORMAL_PRIORITY_CLASS)
    SetThreadPriority((HANDLE) m_hThread, THREAD_PRIORITY_BELOW_NORMAL);
  LOGS(DEBUG, GetName() << " thread running at priority " << GetThreadPriority((HANDLE) m_hThread));
#elif __linux__
   //TBA NYI TODO!!
  // use pthread_setschedprio()
  // but what priority???
  assert(false);
#endif
}


void CThread::ForceExit()
{
  //++
  // Force a thread to terminate, regardless of what it's doing ...
  //--
#ifdef _WIN32
#elif __linux__
  // could use pthread_kill() ???
#endif
}


void CThread::RaiseFlag(uint32_t nFlag)
{
  //++
  //   This method will raise the flag, and if this thread is currently
  // blocked by a call to WaitForFlag(), it will resume execution.  The
  // flag stays in the logical "raised" state until the next call to the
  // WaitForFlag() method.  If the thread isn't currently blocked, then
  // the next call to WaitForFlag() will return immediately.
  //
  //   In theory we could have more than one flag, but presently only one is
  // actually implemented.  The nFlag parameters specifies which flag we're
  // talking about, and it currently must be zero.
  //--
  assert(nFlag == 0);
#ifdef _WIN32
  assert(m_hFlag != 0);
  SetEvent((HANDLE) m_hFlag);
#elif __linux__
  assert(m_pFlag != NULL);
  int err = sem_post(m_pFlag);
  if (err != 0)
    LOGS(ERROR, "error " << err << " in post for " << GetName());
#endif
}

 
bool CThread::WaitForFlag (uint32_t nTimeout, uint32_t nFlag)
{
  //++
  //   This method will block this thread until the flag is raised by another
  // process.  If the flag is already in the raised state then this call will
  // return immediately, but otherwise we'll wait.  This method always returns
  // with the flag in a cleared state so that we're ready to use it again
  // next time.
  //
  //   The nTimeout parameter specifies a maximum time, in milliseconds, to
  // wait.  If this is not zero and the time elapses with no flag, then this
  // routine returns FALSE.  If the timeout is zero, then we'll wait forever.
  //
  //   The potential exists to have more than one flag, and the nFlag parameter
  // specified the flag to use.  Presently only one is implemented, however,
  // and nFlag must always be zero.
  //--
  assert(nFlag == 0);
#ifdef _WIN32
  assert(m_hFlag != 0);
  return WaitForSingleObject((HANDLE) m_hFlag, nTimeout) != WAIT_TIMEOUT;
#elif __linux__
  assert(m_pFlag != NULL);
  int err = sem_wait(m_pFlag);
  if (err != 0)
    LOGS(ERROR, "error " << err << " in wait for " << GetName());
  return err == 0;
#endif
}


THREAD_ID CThread::GetCurrentThreadID()
{
  //++
  //   This method is identical to GetID() except that it always returns the
  // ID of the CURRENT thread, not any CThread child thread.  This is a static
  // method and isn't associated with any particular CThread object ...
  //--
#ifdef _WIN32
  return GetCurrentThreadId();
#elif __linux__
  return pthread_self();
#endif
}


PROCESS_ID CThread::GetCurrentProcessID()
{
  //++
  //   This method is identical to GetCurrentThreadID() except that it returns
  // PROCESS ID for this entire group of threads.  This is used to uniquely
  // identify one instance, for example, of the MBS program from another
  // instance of the same program running on this machine.
  //--
#ifdef _WIN32
  return GetCurrentProcessId();
#elif __linux__
  return getpid();
#endif
}
