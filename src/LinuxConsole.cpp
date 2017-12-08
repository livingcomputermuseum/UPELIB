//++
// LinuxConsole.cpp -> CConsoleWindow implementation for Linux/ANSI
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
// NOTE:
//   This file implements the CConsoleWindow class for the Linux platform using
// ANSI escape sequences and the ncurses library.  The Microsoft Windows version
// is implemented by the WindowsConsole.cpp file.  Both share the same 
// ConsoleWindow.hpp header and class definitions.
//
// DESCRIPTION:
//   One upon a time when MBS, CPS, SDE, et al ran only on Windows PCs there
// were requests to do WIN32 specific console things like set the window title,
// change the window size and/or reposition it on the screen and, ugliest of
// all, trap control-C and Windows Shutdown and LogOff events.  Hence we now
// have this class - one of the first things the main program does is to create
// a CConsoleWindow object and from then on it takes over management of the
// console, including all console I/O. 
//
//   Unfortunately with the Linux port this became a bit trickier, because the
// Linux assumes and ordinary text console and can even be run remotely using
// telnet or ssh.  The Linux version therefore uses ANSI escape sequences and
// the ncurses library to the same effect.  We can still do a lot of things
// this way, like set the window size, colors and title, but some of things we
// can do on WIN32 are just not possible this way.
//
//   It's probably worth reading the comments at the beginning of the
// WindowsConsole.cpp file because many of the things said there will also
// apply here.  Both files share exactly the same header file and class
// definitions in the file ConsoleWindow.hpp.  This implementation is also
// a modified Singleton object that can be instanciated only once, and so
// on.
//
// Bob Armstrong <bob@jfcl.com>   [5-JUN-2017]
//
// REVISION HISTORY:
//  5-JUN-17  RLA   Split from WindowsConsole.cpp
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#ifdef __linux__                // THIS ENTIRE FILE IS ONLY FOR LINUX!
#include <stdio.h>              // fputs(), fprintf(), all the usual stuff ...
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include <stdarg.h>             // va_start(), va_end(), et al ...
#include <string.h>             // strcpy(), memset(), strerror(), etc ...
#include "UPELIB.hpp"           // global declarations for this library
#include "ConsoleWindow.hpp"    // declarations for this module
#include "LogFile.hpp"          // message logging facility


// Initialize the pointer to the one and only CConsoleWindow instance ...
CConsoleWindow *CConsoleWindow::m_pConsole = NULL;


CConsoleWindow::CConsoleWindow (const char *pszTitle)
{
  //++
  //   Note that the CConsoleWindow is a singleton object - only one instance
  // ever exists and we keep a pointer to that one in m_pConsole.
  //--
  assert(m_pConsole == NULL);
  m_pConsole = this;
  m_fForceExit = false;
  if (pszTitle != NULL) SetTitle(pszTitle);
}


CConsoleWindow::~CConsoleWindow()
{
  //++
  //   The destructor closes the handles for the console input and output
  // buffers, and restores the original console mode, just in case we've
  // changed it to something funny...
  //--
  //   Reset the pointer to this Singleton object. In theory this would allow
  // another CConsoleWindow instance to be created, but that's not likely to
  // be useful.
  assert(m_pConsole == this);
  m_pConsole = NULL;
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////


void CConsoleWindow::Write (const char *pszText)
{
  //++
  //   Write a string to the console window.  This is pretty easy and for now
  // the Linux version just does an fputs() to stdout.  Ideally we'd like send
  // error messages to stderr and regular messages to stdout, but the Windows
  // version doesn't distinguish those two and that information isn't available
  // here.  Maybe some day we'll add it, but not today...
  //--
  assert(pszText != NULL);
  fputs(pszText, stdout);
}


void CConsoleWindow::WriteLine (const char *pszLine)
{
  //++
  // Write a string followed by a newline ...
  //--
  if (pszLine != NULL) Write(pszLine);
  Write("\n");
}


void CConsoleWindow::Print (const char *pszFormat, ...)
{
  //++
  // Send printf() style formatted output to the console.
  //--
  assert(pszFormat != NULL);
  va_list args;
  va_start(args, pszFormat);
  vfprintf(stdout, pszFormat, args);
  va_end(args);
}


bool CConsoleWindow::ReadLine (const char *pszPrompt, char *pszBuffer, size_t cbBuffer)
{
  //++
  //   This routine will read one line of input from the console window.  Once
  // again, the Linux version is pretty simple for now and just reads from
  // stdin.  
  //
  //   Also note that Windows will return the carriage return and line feed
  // characters ("\r\n") at the end of the buffer.  We strip those out and
  // the string returned to the caller has no special line terminator.
  //--
  assert((pszBuffer != NULL) && (cbBuffer > 0));
  if (m_fForceExit) return false;
  if (pszPrompt != NULL) fputs(pszPrompt, stdout);
  fflush(stdout);
  if (fgets(pszBuffer, cbBuffer, stdin) == NULL) return false;
  char *psz = strrchr(pszBuffer, '\n');
  if (psz != NULL) *psz = '\0';
  return true;
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////


void CConsoleWindow::SetTitle (const char *pszTitle, ...)
{
  //++
  //   This method sets the title string of the console window.  It's pretty
  // simple, but notice that it allows for extra printf() style arguments.
  // This makes it easy to include the version number, build date, disk status
  // or whatever else, in the title bar.
  //--
  assert((pszTitle != NULL) && (strlen(pszTitle) > 0));
}


string CConsoleWindow::GetTitle() const
{
  //++
  // Return the current title string for the console window ...
  //--
  return string("");
}


void CConsoleWindow::SetColors (uint8_t bForeground, uint8_t bBackground)
{
  //++
  //   This routine will set the foreground (text) and background (window)
  // colors for the console.  The console palette is very limited and 
  // essentially emulates the CGA display with a range of only 16 colors.
  // Both bForeground and bBackground are four bit values where the value
  // 1 is blue, 2 is green, 4 is red and 8 is the intensify bit.  
  //--
}


bool CConsoleWindow::GetColors (uint8_t &bForeground, uint8_t &bBackground)
{
  //++
  //   Return the current console window colors (see the SetColors() method
  // for more details on the color set).
  //--
  bForeground = WHITE;  bBackground = BLACK;
  return false;
}


bool CConsoleWindow::SetWindowSize (uint16_t nColumns, uint16_t nRows, int32_t nX, int32_t nY)
{
  //++
  //--
  return false;
}


bool CConsoleWindow::GetWindowSize (uint16_t &nColumns, uint16_t &nRows)
{
  //++
  // Return the current console window size, in rows and columns ...
  //--
  nColumns = 80;  nRows = 24;
  return false;
}


#endif      // end of #ifdef __linux__ from the very top of this file!
