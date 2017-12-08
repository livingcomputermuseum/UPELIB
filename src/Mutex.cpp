//++
// Mutex.cpp -> Platform independent critical section interlock ...
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
//   This class implements a simple critical section interlock to guarantee
// exclusive access when multiple threads use the same shared memory location.
// In most circles this is known as a mutex, and indeed under Linux this is
// exactly a mutex from the pthreads library.  However under Windows a mutex
// is slightly different (it's cross process and system wide on Windows) and
// instead this corresponds to what Windows calls a "critical section".  Under
// Windows there's actually an StlLock.h library file that does exactly what
// we want, but unfortunately there's no equivalent to that on Linux.
//
//   So we have this source file to absorb the differences between Linux and
// Windows.  I'm sure you'll notice that all these routines are trivial in the
// extreme and could have been written out inline in Mutex.hpp.  Don't do it!
// Putting the code in the header file means that the header will also have to
// include Windows.h, and that will cause  no end of problems!!
//
// Bob Armstrong <bob@jfcl.com>   [30-MAY-2017]
//
// REVISION HISTORY:
// 30-MAY-17  RLA   New file.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>           // CRITICAL_SECTION obect, et al ...
#elif __linux__
#include <sys/types.h>          // pthread_mutex_t and others ...
#include <unistd.h>             // ...
#include <pthread.h>            // all pthread_mutex_xyz ...
#endif
#include "UPELIB.hpp"           // UPE library definitions
#include "Mutex.hpp"            // declarations for this module

//   These macros cast the void* m_pMutex member into the appropriate type.
// They save typing!
#ifdef _WIN32
#define PMUTEX(p) ((LPCRITICAL_SECTION) p)
#elif __linux__
#define PMUTEX(p) ((pthread_mutex_t *) p)
#endif


CMutex::CMutex() 
{
  //++
  // Allocate and initialize a mutex object ...
  //--
#ifdef _WIN32
  m_pMutex = DBGNEW CRITICAL_SECTION;
  InitializeCriticalSection(PMUTEX(m_pMutex));
#elif __linux__
  m_pMutex = DBGNEW pthread_mutex_t;
  pthread_mutex_init(PMUTEX(m_pMutex), NULL);
#endif
}


CMutex::~CMutex() 
{
  //++
  // Delete the mutex and return its resources to the OS ...
  //--
  if (m_pMutex != NULL) {
#ifdef _WIN32
    DeleteCriticalSection(PMUTEX(m_pMutex));
#elif __linux__
    pthread_mutex_destroy(PMUTEX(m_pMutex));
    delete PMUTEX(m_pMutex);
#endif
  }
  m_pMutex = NULL;
}


void CMutex::Enter()
{
  //++
  // Acquire the mutex, and block (maybe forever!) if it's busy ...
  //--
  assert(m_pMutex != NULL);
#ifdef _WIN32
  EnterCriticalSection(PMUTEX(m_pMutex));
#elif __linux__
  pthread_mutex_lock(PMUTEX(m_pMutex));
#endif
}


void CMutex::Leave()
{
  //++
  // Release the mutex, assuming we currently have it ...
  //--
  assert(m_pMutex != NULL);
#ifdef _WIN32
  LeaveCriticalSection(PMUTEX(m_pMutex));
#elif __linux__
  pthread_mutex_unlock(PMUTEX(m_pMutex));
#endif
}
