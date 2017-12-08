//++
// Mutex.hpp -> Platform independent critical section interlock ...
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
// 30-MAY-17  RLA   New file.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
//--
#pragma once


class CMutex {
  //++
  //   This class is a simple mutex object.  The constructor creates the mutex
  // the destructor destroys it.  The Enter() method acquires the mutex and
  // will block if it is not available, and the Leave() method releases the
  // mutex.  You should call Enter() before starting a critical section and
  // then call Leave() when you're done. Couldn't be simpler!
  //--

  // Constructor and destructor ...
public:
  CMutex();
  ~CMutex();

  // Disallow copy and assignment operations with CMutex objects ...
private:
  CMutex(const CMutex &dw) = delete;
  CMutex& operator= (const CMutex &dw) = delete;

  // Mutex operations (there are only two!!) ...
public:
  void Enter();
  void Leave();

  // Local members ...
protected:
  void        *m_pMutex;      // whatever OS specific object we need to create
};
