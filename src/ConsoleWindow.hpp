//++
// ConsoleWindow.hpp -> CConsoleWindow (WIN32 console window) definitions
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
//   The CConsolewindow class implements an interface to the WIN32 console
// window for console applications. It handles WIN32 specific console things
// like changing the window title, changing the window size and/or position,
// and trapping control-C and Windows Shutdown and LogOff events.  
//
//    This is a "modified" Singleton class - only one instance per application
// should ever be created, however the constructor must be explicitly called
// once to create that instance.  Subsequent calls to the constructor will
// cause assertion failures, and a pointer to the original instance may be
// retrieved at any time by calling GetConsole().
//
//    Note that the implementation of this class can be found in either the
// WindowsConsole.cpp file OR the LinuxConsole.cpp file.  The differences in
// the console implementation between the two operating systems were just too
// great to put everything in one big file, so instead there are two.  Both
// files share this same header file, however, and the interface presented to
// the rest of the code is the same.
//
// Bob Armstrong <bob@jfcl.com>   [11-JUN-2015]
//
// REVISION HISTORY:
// 11-JUN-15  RLA   New file.
//--
#pragma once
#include <string>               // C++ std::string class, et al ...
using std::string;              // ...

// Invisible window constants ....
#ifdef _WIN32
#define INVISIBLE_WINDOW_CLASS    L"InvisibleWindow"
#define INVISIBLE_WINDOW_TITLE    L"InvisibleWindow"
#define INVISIBLE_WINDOW_ICON     L"InvisibleWindow"
#endif


// CConsoleWindow class definition ...
class CConsoleWindow {
  //++
  //--

  // Constants ...
public:
  enum {
    // CGA color bits for SetColors() ...
    BLACK         = 0x0,        //
    DARK_BLUE     = 0x1,        // NAVY
    DARK_GREEN    = 0x2,        // GREEN
    DARK_CYAN     = 0x3,        // TEAL
    DARK_RED      = 0x4,        // MAROON
    DARK_MAGENTA  = 0x5,        // PURPLE
    ORANGE        = 0x6,        // BROWN
    LIGHT_GRAY    = 0x7,        // SILVER
    GRAY          = 0x8,        //
    BLUE          = 0x9,        //
    GREEN         = 0xA,        // LIME
    CYAN          = 0xB,        // AQUA
    RED           = 0xC,        // PINK
    MAGENTA       = 0xD,        // FUCHSIA
    YELLOW        = 0xE,        //
    WHITE         = 0xF,        //
  };

  // Public CConsoleWindow properties ...
public:
  // Return a pointer to the one and only CConsoleWindow object ...
  static CConsoleWindow *GetConsole() {assert(m_pConsole != NULL);  return m_pConsole;}
  // Get and set the title of this window ...
  string GetTitle() const;
  void SetTitle (const char *pszTitle, ...);
  // Control the foreground and background colors ...
  bool GetColors (uint8_t &bForeground, uint8_t &bBackground);
  void SetColors (uint8_t bForeground, uint8_t bBackground);
  // Control the window size (in rows and columns) ...
  bool GetWindowSize (uint16_t &nColumns, uint16_t &nRows);
  bool SetWindowSize (uint16_t nColumns, uint16_t nRows, int32_t nX=-1, int32_t nY=-1);
  // Return true if there are no more console commands coming ...
  bool IsForcedExit() const  {return m_fForceExit;}
  void SetForcedExit(bool fSet=true)  {m_fForceExit = fSet;}
#ifdef _WIN32
  // Get the console window position (in screen coordinates) ...
  void GetWindowPosition (int32_t &X, int32_t &Y);
  // Set the icon for this window ...
  bool SetIcon(uint32_t nIcon);
  // Control the buffer size ...
  bool GetBufferSize(uint16_t &nColumns, uint16_t &nRows);
  bool SetBufferSize(uint16_t nColumns, uint16_t nRows);
  // Return true if this application is being forcibly shut down ...
  bool IsSystemShutdown() const {return m_fSystemShutdown;}
  void SetSystemShutdown(bool fSet=true)  {m_fSystemShutdown = m_fForceExit = fSet;}
#endif

  // Public CConsoleWindow methods ...
public:
  // Constructor and destructor ...
  CConsoleWindow (const char *pszTitle = NULL);
  virtual ~CConsoleWindow();
  //   Write a string to the console window.  Note that the only difference
  // between Write() and WriteLine() is that the latter appends a CRLF ...
  void Write (const char *pszText);
  void Write (const string &str)  {Write(str.c_str());}
  void WriteLine (const char *pszLine=NULL);
  void WriteLine (const string &str) {WriteLine(str.c_str());}
  // Send formatted (i.e. printf() style) output to the console ...
  void Print (const char *pszFormat, ...);
  // Read a single line from the console ...
  bool ReadLine (const char *pszPrompt, char *pszBuffer, size_t cbBuffer);
  bool ReadLine (const char *pszPrompt, string &sBuffer);

  //   These methods really should be private, however they need to be called
  // from C language WINAPI routines and hence have to be delcared public.
  // They're not used anywhere outside of the CConsoleWindow implementation
  // and are not present in the Linux version!
public:
#ifdef _WIN32
  // Detach the current console and create a new one ...
  static bool CreateNewConsole();
  bool AttachNewConsole();
  // Handle Window Close and System Shutdown events ...
  void HandleWindowClosed();
  void HandleSystemShutdown();
  // Force a keystroke into the console input buffer ...
  void SendConsoleKey(char chKey=CHCRT, uint16_t vkKey=CHCRT, bool fControl=false);
#endif

  // Private CConsoleWindow methods ...
private:
#ifdef _WIN32
  // "Invisible" window procedures ...
  void BeginInvisibleThread();
  void EndInvisibleThread();
  // Set or clear console mode bits ...
  void SetMode(void *hBuffer, uint32_t dwSet);
  void ClearMode(void *hBuffer, uint32_t dwClear);
  // "Attach" this class instance to the current console window ..
  void AttachCurrentConsole();
  void DetachCurrentConsole();
#endif

  // Local members ...
private:
  // This stuff works on both Linux and Windows ...
  bool     m_fForceExit;            // true to force EOF on next ReadLine()
#ifdef _WIN32
  //   Notice that there's a little bit of funny stuff going on here.  The
  // handles for the console window, input buffer and output buffer should be
  // of type HANDLE.  Likewise, dwOriginalMode should be a DWORD.  The problem
  // is that these are unique Windows types, defined in windows.h, and if we
  // use them here anything that includes this header will also need to drag
  // in all of Windows!  Rather than do that, we'll cheat a little by knowing
  // (or pretending to know) the underlying types for HANDLE and DWORD.
  void    *m_hWindow;               // handle for the console window
  void    *m_hInput;                // handle for console input buffer
  void    *m_hOutput;               //   "     "     "    output   "
  void    *m_hInvisibleThread;      // thread for the "invisible" window
  bool     m_fSystemShutdown;       // true if the system is being shut down
  uint32_t m_dwOriginalMode;        // original console mode
  uint16_t m_wOriginalWindowWidth;  // original window width (characters)
  uint16_t m_wOriginalWindowHeight; //   "   "     "    height (lines)
  uint16_t m_wOriginalBufferWidth;  // original buffer width (characters)
  uint16_t m_wOriginalBufferHeight; //   "   "     "    height (lines)
  uint8_t  m_bOriginalForeground;   // original foreground color
  uint8_t  m_bOriginalBackground;   //   "   "  background   "
#endif

  // Static data ...
private:
  static CConsoleWindow *m_pConsole; // the one and only CConsoleWindow instance
};
