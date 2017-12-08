//++
// CheckpointFiles.cpp -> CCheckpointFiles (flush to disk thread) methods
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
//   This super simple class creates a background thread which periodically
// does a fflush() followed by an _commit() call on selected files.  These
// two calls flush the CRTL buffers to Windows, and then flushes any Windows
// buffers to the physical disk.
//
//   The main program should create one instance of this object and then methods
// that want files checkpointed (e.g. CLogFile, CImageFile, etc) can "register"
// individual files with this object by calling our AddFile() method.  Every so
// often our background thread will automatically flush and then commit all
// registered files.  The flush thread defaults to running once per minute,
// although this can be changed by a call to the SetInterval() method.  The
// checkpoint thread can be stopped either by explicitly calling Stop(), or by
// the destructor.
//
//   NOTE - this object uses the LOGx() functions, so don't start the thread
// running until after the CLog object has been created!
//
// Bob Armstrong <bob@jfcl.com>   [28-OCT-2015]
//
// REVISION HISTORY:
// 28-OCT-15  RLA   New file.
// 15-NOV-15  RLA   Rewrite to keep an explicit set of files for flushing
//  1-JUN-17  RLA   Linux port.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include <errno.h>              // ENOENT, EACCESS, etc ...
#ifdef _WIN32
#include <io.h>                 // _commit(), _fileno(), etc...
#elif __linux__
#include <unistd.h>             // fsync(), fileno(), etc ...
#endif
#include "Thread.hpp"           // CThread portable thread library
#include "UPELIB.hpp"           // UPE library definitions
#include "SafeCRT.h"		// replacements for Microsoft "safe" CRT functions
#include "LogFile.hpp"          // UPE library message logging facility
#include "CheckpointFiles.hpp"  // declarations for this module


// Initialize the pointer to the one and only CCheckpointFiles instance ...
CCheckpointFiles *CCheckpointFiles::m_pCheckpoint = NULL;


CCheckpointFiles::CCheckpointFiles (uint32_t dwInterval)
  : m_CheckpointThread(&CCheckpointFiles::CheckpointThread, "checkpoint files")
{
  //++
  //   Initialize all the members, but DO NOT start the checkpoint files thread
  // running yet.  Somebody has to call the Start() method for that.  Remmeber
  // that this is a singleton object, so we keep a pointer to this (the one
  // and only) instance in m_pCheckpoint...
  //
  //   One slight annoyance - be sure to notice that the parameter to this
  // constructor is the checkpoint interval, IN SECONDS, but the m_dwInterval
  // member is the same interval, IN MILLISECONDS!  Sorry about that...
  //--
  assert((m_pCheckpoint == NULL) && (dwInterval > 0));
  m_pCheckpoint = this;  m_dwInterval = dwInterval*1000;  m_setFiles.clear();
  m_CheckpointThread.SetParameter(this);

}


CCheckpointFiles::~CCheckpointFiles()
{
  //++
  // Destroying this object will stop the thread (if it's running) ...
  //--
  assert(m_pCheckpoint == this);
  Stop();  m_pCheckpoint = NULL;
}


bool CCheckpointFiles::Checkpoint (FILE *f)
{
  //++
  // Flush and commit a single file ...
  //--
  if (fflush(f) != 0) return false;
#ifdef _WIN32
  _commit(_fileno(f));
#elif __linux__
  fsync(fileno(f));
#endif
  return true;
}


void* THREAD_ATTRIBUTES CCheckpointFiles::CheckpointThread (void *pParam)
{
  //++
  //   This is the background checkpoint thread and it couldn't be much
  // simpler.  It delays for the necessary interval, calls _flushall(), and
  // then repeats.  When the m_fExitThread member is set, it will exit.
  // That's all!
  //
  //   Note that the lparam passed to us by the Start() method is simply the
  // address of the corresponding CCheckpointFiles object.  That's how we can access
  // member data like the interval and the exit flag.
  //--
  assert(pParam != NULL);
  CThread *pThread = (CThread *) pParam;
  CCheckpointFiles *pThis = (CCheckpointFiles *) pThread->GetParameter();
  assert(pThis->m_dwInterval > 100);
  LOGF(DEBUG, "file checkpoint thread running at %d second intervals", pThis->m_dwInterval/1000);
  while (!pThread->IsExitRequested()) {
    //   The checkpoint interval is typically fairly long, like 60 seconds or
    // more, but we don't want to wait that long for the thread to exit when
    // this program shuts down.  So instead we sleep for 100ms at a time and
    // just count until we reach the required interval.
    for (uint32_t i=0; (i < pThis->m_dwInterval/100) && !pThread->IsExitRequested(); ++i) _sleep_ms(100);
    int nFiles = 0;
    for (iterator it = pThis->begin();  it != pThis->end();  ++it)
      if (Checkpoint(*it)) ++nFiles;
//  if (nFiles > 0) LOGF(TRACE, "checkpointed %d files", nFiles);
  }
  LOGS(DEBUG, "file checkpoint thread terminated");
  return pThread->End();
}


pair<CCheckpointFiles::iterator, bool> CCheckpointFiles::AddFile(FILE *f)
{
  //++
  //   Add a file to the checkpoint set and, if the background thread is not
  // already running, then start it...
  //--
  if (!IsRunning()) Start();
  return m_setFiles.insert(f);
}


void CCheckpointFiles::SetInterval (uint32_t dwInterval)
{
  //++
  //   This method will change the file checkpoint interval.  Note that this
  // does NOT stop or start the thread - if it's not running, this will change
  // the interval for the next time it is run but calling this method alone
  // won't start it.  If the thread is running, then this method will change 
  // the interval for all future checkpointing.  Note that we don't actually do
  // anything to tell the background thread about the change - just changing
  // m_dwInterval is enough.  The thread will pick up the change sooner or
  // later.
  //
  //   Also note that the interval must be greater than zero AND also notice
  // that the parameter to this method is IN SECONDS, but the m_dwInterval
  // member is the same interval, IN MILLISECONDS!  Sorry about that...
  //--
  assert(dwInterval > 0);
  m_dwInterval = dwInterval * 1000;
}

