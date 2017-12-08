//++
// CheckpointFiles.hpp -> CCheckpointFiles (flush to disk thread) class
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
// 29-OCT-15  RLA   New file.
// 13-NOV-15  RLA   Rewrite to allow checkpointing specific files only.
//  1-JUN-17  RLA   Linux port.
//--
#pragma once
#include <unordered_set>        // C++ std::unordered_set (a simple list) template
using std::pair;                // ...
using std::unordered_set;       // ...
#include "Thread.hpp"           // needed for THREAD_ATTRIBUTES ...


class CCheckpointFiles {
  //++
  //--

  // Constants and defaults ...
public:
  enum {
    DEFAULT_INTERVAL  = 60      // default checkpoint interval, in seconds
  };

  // Constructor and destructor ...
public:
  CCheckpointFiles (uint32_t dwInterval=DEFAULT_INTERVAL);
  virtual ~CCheckpointFiles();
  // Disallow copy and assignment operations with CCheckpointFiles objects...
private:
  CCheckpointFiles(const CCheckpointFiles &dw) = delete;
  CCheckpointFiles& operator= (const CCheckpointFiles &dw) = delete;

  //   The FILE_SET is a simple unordered set of all the file streams that we
  // want to checkpoint.  Files must be added to this set with the AddFile()
  // method before we will checkpoint them.
public:
  typedef unordered_set<FILE *> FILESET;
  typedef FILESET::iterator iterator;
  iterator begin() {return m_setFiles.begin();}
  iterator end()   {return m_setFiles.end();}

  // Properties ...
public:
  // Return TRUE if file checkpointing has been enabled ...
  static bool IsEnabled() {return m_pCheckpoint != NULL;}
  // Return a pointer to the one and only instance of this object ...
  static CCheckpointFiles *GetCheckpoint()
    {assert(IsEnabled());  return m_pCheckpoint;}
  // TRUE if the background thread is running now ...
  bool IsRunning() const {return m_CheckpointThread.IsRunning();}
  // Get or set the checkpoint interval (in seconds!) ...
  uint32_t GetInterval() const {return m_dwInterval/1000;}
  void SetInterval (uint32_t dwInterval);
  // Return true if a file is already being checkpointed ...
  bool IsCheckpointed (FILE *f) {return m_setFiles.find(f) != end();}

  // Public methods ...
public:
  // Start and stop the background thread ...
  bool Start() {return m_CheckpointThread.Begin();}
  void Stop() {m_CheckpointThread.WaitExit();}
  // Add or remove files from the collection ...
  pair<iterator, bool> AddFile (FILE *f);
  void RemoveFile (FILE *f) {m_setFiles.erase(f);}

  
  // Local methods ...
protected:
  // Checkpoint just one file ...
  static bool Checkpoint(FILE *f);
  // The background checkpointing thread ...
  static void* THREAD_ATTRIBUTES CheckpointThread (void *pParam);

  // Local members ...
protected:
  uint32_t  m_dwInterval;       // checkpoint interval, in milliseconds
  FILESET   m_setFiles;         // files we want to checkpoint
  CThread   m_CheckpointThread; // background thread to do the checkpoints
  static CCheckpointFiles *m_pCheckpoint; // the one and CCheckpointFiles instance
};
