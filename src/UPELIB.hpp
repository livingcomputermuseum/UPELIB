//++
// UPELIB.hpp -> Global declarations for the LCM UPE library
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
//   This file contains global constants and universal macros for the LCM's
// Universal Peripheral Emulator library.  It's used by the MBS, CPS and
// SDE programs.
//
// Bob Armstrong <bob@jfcl.com>   [20-MAY-2015]
//
// REVISION HISTORY:
// 20-MAY-15  RLA   split from MBS/CPS.
//--
#pragma once
#include <string>             // C++ std::string class, et al ...
using std::string;            // this is used EVERYWHERE!

// Constants and parameters ...
#define UPEVER         48     // UPE library version number ...

// ASCII character codes used by the console window and TELNET classes ...
#define CHNUL        0x00     // null
#define CHBEL        0x07     // bell
#define CHBSP        0x08     // backspace
#define CHTAB        0x09     // horizontal tab
#define CHFFD        0x0C     // form feed
#define CHLFD        0x0A     // line feed
#define CHCRT        0x0D     // carriage return
#define CHEOF        0x1A     // control-Z
#define CHESC        0x1B     // escape
#define CHDEL        0x7F     // delete key for some terminals

//   Sadly, windef.h defines these macros too - fortunately Microsoft's
// definition is compatible with ours, so it's not a fatal problem.
// Unfortunately, Microsoft's header doesn't check to see whether these
// things are defined before redefining them, and that leads to a
// compiler error message.
#if !(defined(_WIN32) && defined(LOBYTE))
#define LOBYTE(x) 	((uint8_t)  ((x) & 0xFF))
#define HIBYTE(x) 	((uint8_t)  (((x) >> 8) & 0xFF))
#define LOWORD(x) 	((uint16_t  ( (x) & 0xFFFF)))
#define HIWORD(x)	((uint16_t) (((x) & 0xFFFF0000L) >> 16) )
#endif

// Assemble and disassemble nibbles, bytes, words and longwords...
#define MASK8(x)        ((x) & 0xFF)
#define MASK12(x)       ((x) & 0xFFF)
#define MASK16(x)       ((x) & 0xFFFF)
#define MASK32(x)       ((x) & 0xFFFFFFFFUL)
#define LONIBBLE(x)	((x) & 0x0F)
#define HINIBBLE(x)	(((x) >> 4) & 0x0F)
#define MKBYTE(x)       ((uint8_t) ((x) & 0xFF))
#define MKWORD(h,l)	((uint16_t) ((((h) & 0xFF) << 8) | ((l) & 0xFF)))
#define MKLONG(h,l)	((uint32_t) (( (uint32_t) ((h) & 0xFFFF) << 16) | (uint32_t) ((l) & 0xFFFF)))
#define MKQUAD(h,l)	((uint64_t) (( (uint64_t) ((h) & 0xFFFFFFFF) << 32) | (uint64_t) ((l) & 0xFFFFFFFF)))

//   This macro converts a 64 bit quantity (e.g. a size_t) to a 32 bit integer
// when we're targeting a 64 bit platform.  This is unfortunately necessary
// for some legacy cases (e.g. the second argument to fgets()).
#define MKINT32(x)      ((int) ((x) & 0xFFFFFFFF))

// Bit set, clear and test macros ...
#define SETBIT(x,b)	x |=  (b)
#define CLRBIT(x,b)	x &= ~(b)
#define CPLBIT(x,b)     x ^=  (b)
#define ISSET(x,b)	(((x) & (b)) != 0)

// Useful arithmetic macros ...
#define MAX(a,b)  ((a) > (b) ? (a) : (b))
#define MIN(a,b)  ((a) < (b) ? (a) : (b))
#define ISODD(a)  (((a) & 1) != 0)
#define ISEVEN(a) (((a) & 1) == 0)

// Useful shorthand for string comparisons ...
#define STREQL(a,b)     (strcmp(a,b) == 0)
#define STRNEQL(a,b,n)  (strncmp(a,b,n) == 0)
#define STRIEQL(a,b)    (_stricmp(a,b) == 0)
#define STRNIEQL(a,b,n) (_strnicmp(a,b,n) == 0)

//   Define the C++ new operator to use the debug version. This enables tracing
// of memory leaks via _CrtDumpMemoryLeaks(), et al.  It'd be really nice if
// Visual Studio was smart enough to just do this for us, but it isn't ...
#if defined(_DEBUG) && defined(_MSC_VER)
#define DBGNEW new( _NORMAL_BLOCK, __FILE__, __LINE__)
#else
#define DBGNEW new
#endif

// Prototypes for routines declared in upelib.cpp ...
extern "C" void _sleep_ms (uint32_t nDelay);
extern "C" void CheckAffinity (void);
extern "C" void SetTimerResolution (unsigned int uResolution);
extern "C" void RestoreTimerResolution (void);
extern bool SplitPath (const char *pszPath, string &sDrive, string &sDirectory, string &sFileName, string &sExtension);
extern string MakePath (const char *pszDrive, const char *pszDirectory, const char *pszFileName, const char *pszExtension);
extern string FullPath (const char *pszRelativePath);
extern bool FileExists (const char *pszPath);
extern bool ParseIPaddress (const char *pszAddr, uint32_t &lIP, uint16_t &nPort);
extern string FormatIPaddress (uint32_t lIP, uint16_t nPort=0);
extern string FormatIPaddress (const struct sockaddr_in *p);
