//++
// BitStream.hpp -> CBitStream (Xilinx FPGA bit stream file) class
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
// 21-SEP-15  RLA   New file.
//--
#pragma once
#include <string>               // C++ std::string class, et al ...
#include <iostream>             // C++ style output for LOGS() ...
#include <sstream>              // C++ std::stringstream, et al ...
using std::string;              // ...
using std::ostream;             // ...
using std::ostringstream;       // ...


class CBitStream {
  //++
  //--

  // Constructor and destructor ...
public:
  CBitStream (const string &sFileName = "");
  virtual ~CBitStream() {Clear();}

  // Properties ...
public:
  // Get filename associated with this object ...
  string GetFileName() const {return m_sFileName;}
  // Return true if a bitstream has been successfully loaded ...
  bool IsLoaded() const {return m_pabBits != NULL;}
  // Get the design information from the bit stream header ...
  string GetDesignName() const {assert(IsLoaded());  return m_sDesignName;}
  string GetDesignDate() const {assert(IsLoaded());  return m_sDesignDate;}
  string GetDesignTime() const {assert(IsLoaded());  return m_sDesignTime;}
  string GetPartName()   const {assert(IsLoaded());  return m_sPartName;  }
  // Return the size of the configuration bit stream ...
  size_t GetBitStreamSize()   const {assert(IsLoaded());  return m_cbBits;}
  // Return a pointer to the actual bit stream data ...
  const uint8_t *GetBitStream() const {assert(IsLoaded());  return m_pabBits;}

  // Bitstream data access methods ...
public:
  // Swap the order of the bits in a single byte ...
  static const uint8_t m_abSwapBits[256];
  static uint8_t SwapBits (uint8_t &b)  {return b = m_abSwapBits[b];}
  // Swap bytes in a word ...
  static uint16_t SwapBytes (uint16_t &w)
    {return w = MKWORD(LOBYTE(w), HIBYTE(w));}

  // Public methods ...
public:
  // Clear the current loaded bitstream (if any) ...
  void Clear();
  // Open (and load) a bitstream file ...
  bool Open (const string &sFileName = "");

  
  // Local methods ...
protected:
  // Print a file related error message ...
  bool Error (const char *pszMsg, int nError=0) const;
  // Read one or more bytes from the bit stream file ...
  bool ReadBytes (void *pData, size_t cbData);
  // Read a string of bytes preceeded by a 16 bit count word ...
  size_t ReadBlock (void *pData, size_t cbMaxData);
  // Read an ASCII string from the bit stream file ...
  bool ReadString (string &str);
  // Read the bitstream data ...
  bool ReadFile();

  // Local members ...
protected:
  FILE    *m_pFile;           // bitstream file handle
  string   m_sFileName;       // name of the bitstream file
  string   m_sDesignName;     // original design name from the header
  string   m_sDesignDate;     // date the design was compiled ...
  string   m_sDesignTime;     // time  "     "    "    "   "  ....
  string   m_sPartName;       // target Xilinx part name
  size_t   m_cbBits;          // number of bytes in comfiguration bit stream
  uint8_t *m_pabBits;         // and the actual configuration bit stream
};


//   This inserter allows you to send the file name directly to an I/O
// stream for error messages ...
inline ostream& operator << (ostream &os, const CBitStream &bs)
  {os << bs.GetFileName();  return os;}
