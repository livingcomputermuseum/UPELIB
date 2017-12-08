//++
// SafeCRT.c -> Microsoft "Safe C Run Time" replacements
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
//   Microsoft defines a number of "safe" C runtime functions - e.g. all those
// that end in "_s", strcat_s, strcpy_s, etc.  Unfortunately there are no glib
// equivalents for these, although may be someday (it's been proposed for the
// C17 standard).  In the meantime, this header and the associated C file
// define implementations for these functions that will allow us to compile
// under Linux.
//
//   In addition to the Safe CRT functions, there's another class of C library
// routines where Microsoft is arbitrarily different from the usual C standard.
// This includes things like _snprintf() vs snprintf(), _strerr() vs strerr(),
// and so on.  Usually it's just a slight difference in the order of the
// parameters, but it's enough to make the Windows code fail to work under
// Linux/gcc/glib.  This module also contains various wrappers, shims and fixes
// for those as well...
//
//   Most of these things can be accomplished by clever #defines and the
// occasional inline routine, and all of that appears in the SafeCRT.h header
// file.  There are a few, however, that require non-trivial amounts of code
// and you'll find them here.
//
//   As a rule I've tried to reproduce the "safeness" of the Microsoft routines
// as well as the functionality, but that's not always true.  There are a few
// replacements here which are functionally equivalent to the MS versions but
// do not have all the same error checking features.  This allows us to keep
// the safe versions and associated checking under Windows while still porting
// to Linux with relative ease.
//
// Bob Armstrong <bob@jfcl.com>   [26-MAY-2017]
//
// REVISION HISTORY:
// 26-MAY-17  RLA   New file.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#ifndef _MSC_VER                // don't need any of this for Microsoft C!
#define _GNU_SOURCE             // need the GNU version of strerror_r()
#include <stdio.h>              // FILE, et al ...
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	            // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include <string.h>             // memcpy(), memset(), etc ...
#include <errno.h>              // ENOENT, EACCESS, etc ...
#include "SafeCRT.h"            // and the declarations for this module

int strcpy_s (char *pszDst, size_t cbDst, const char *pszSrc)
{
  //++
  //   All we really want from strcpy_s() is a safe string copy that #1, won't
  // overflow the destination, and #2, always guarantees that the destination
  // will be null terminated.  I read comments from lots of people bad mouthing
  // MS for inventing their own non-standard string functions, but Microsoft has
  // a routine that meets these requirements when nobody else does!!
  //
  //   One point worth mentioning is that the real MS strcpy_s function will
  // invoke the invalid parameter handler if any of the parameters passed is
  // invalid, but we don't don't bother with that and just assert() instead.
  //--
  assert((pszDst != NULL) && (pszSrc != NULL) && (cbDst > 0));
  size_t nSrcLen = strlen(pszSrc);
  size_t nBytesToCopy = (nSrcLen < cbDst) ? nSrcLen : cbDst-1;
  memcpy(pszDst, pszSrc, nBytesToCopy);
  pszDst[nBytesToCopy] = '\0';
  return (nBytesToCopy < nSrcLen) ? STRUNCATE : 0;
}


int strcat_s (char *pszDst, size_t cbDst, const char *pszSrc)
{
  //++
  //   Once we have a safe strcpy_s(), a safe strcat_s() is pretty easy - we
  // just have to figure out how many bytes in the destination are actually
  // free, and then we can use strcpy_s().
  //--
  assert((pszDst != NULL) && (pszSrc != NULL) && (cbDst > 0));
  size_t nDstLen = strlen(pszDst);
  if ((nDstLen+1) >= cbDst) return STRUNCATE;
  size_t nFreeBytes = cbDst-nDstLen;
  return strcpy_s(pszDst+nDstLen, nFreeBytes, pszSrc);
}


int strerror_s (char *pszBuffer, size_t cbBuffer, int nError)
{
  //++
  //   Linux recently added a strerr() function with bounds checking, but they
  // didn't do it for the purpose of preventing buffer overflows - they did it
  // to be thread safe.  The original strerr() used a static buffer and wasn't
  // thread safe, but strerror_r() in theory uses a caller supplied buffer and
  // is safe.  I say "in theory" because in reality strerror_r() is also
  // allowed to totally ignore the caller's buffer and return a pointer to its
  // own internal string, so long as that string is immutable.  Since there's
  // usually an array of error messages somewhere, that's pretty easy to do and
  // all strerror_r does is return a pointer to one of those constant strings.
  // If that happens we have to copy the string to the caller's buffer!
  //--
  memset(pszBuffer, 0, cbBuffer);
  char *pszResult = strerror_r(nError, pszBuffer, cbBuffer);
  if (pszResult != pszBuffer) strcpy_s(pszBuffer, cbBuffer, pszResult);
  return 0;
}


#endif	// #ifndef _MSC_VER (from the very top of this file!)
