//++
// BitStream.cpp -> CBitStream (Xilinx FPGA bit stream file) methods
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
//   A CBitStream object encapsulates a Xilinx bitstream (.BIT) file that's
// used for programming the FPGA on the MESA I/O (aka the UPE) board.  Most
// of the Xilinx bitstream is encrypted and is a deep, dark secret but the
// header format is known.  With that it's possible to extract the actual
// configuration bits that need to be sent to the FPGA, and that's enough to
// enable FPGA programming via the MESA/UPE card.  
//
// Bob Armstrong <bob@jfcl.com>   [21-SEP-2015]
//
// REVISION HISTORY:
// 21-SEP-15  RLA   New file.
//  1-JUN-17  RLA   Linux port.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include <string.h>             // strcpy(), memset(), etc ...
#include "UPELIB.hpp"           // UPE library definitions
#include "SafeCRT.h"		// replacements for Microsoft "safe" CRT functions
#include "LogFile.hpp"          // UPE library message logging facility
#include "CommandParser.hpp"    // UPE library command line parsing methods
#include "BitStream.hpp"        // declarations for this module

//   Xilinx FPGAs are actually programmed bit serially, and in some designs
// it's necessary to reverse the order of the bits in each byte so that the
// serial bit stream comes out in the right order.  The MESA card is one of
// these, so we use this table and the Swap() routine to swap the bits in
// a byte.
const uint8_t CBitStream::m_abSwapBits[256] = {
  0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
  0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
  0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
  0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
  0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
  0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
  0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
  0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
  0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
  0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
  0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
  0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
  0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
  0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
  0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
  0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
};


CBitStream::CBitStream (const string &sFileName)
{
  //++
  //   The constructor can optionally initialize the file name, but it does
  // NOT attempt to open the file - the caller must sometime later call the
  // Open() method to do that.  The main reason for this is that constructors
  // have no good way to fail, and there's no clean way to handle the case
  // where the Open() fails.
  //--
  m_sFileName = sFileName; 
  m_sDesignName = m_sDesignDate = m_sDesignTime = m_sPartName = "";
  m_cbBits = 0;  m_pabBits = NULL;
}


void CBitStream::Clear()
{
  //++
  // Initialize everything (and clear any currently loaded bitstream) ...
  //--
  // Notice that this explicitly DOES NOT clear the filename!!
  m_sDesignName = m_sDesignDate = m_sDesignTime = m_sPartName = "";
  if (m_pabBits != NULL) delete[]m_pabBits;
  m_cbBits = 0;  m_pabBits = NULL;
}


bool CBitStream::Error (const char *pszMsg, int nError) const
{
  //++
  //   Print a file related error message and then always return false.  Why
  // always return false?  So we can say something like
  //
  //    if ... return Error("fail", errno);
  //--
  char sz[80];
  if (nError > 0) {
    LOGS(ERROR, "error (" << nError << ") " << pszMsg << " " << m_sFileName);
    strerror_s(sz, sizeof(sz), nError);
    LOGS(ERROR, sz);
  } else {
    LOGS(ERROR, pszMsg << " - " << m_sFileName);
  }
  return false;
}


bool CBitStream::ReadBytes (void *pData, size_t cbData)
{
  //++
  //   This method loads cbData bytes from the the bit stream file.  If it
  // fails for any reason, it prints a message and returns false.
  //--
  if (fread(pData, 1, cbData, m_pFile) != cbData)
    return Error("reading", errno);
  return true;
}


size_t CBitStream::ReadBlock (void *pData, size_t cbMaxData)
{
  //++
  //   This routine will load a "counted block" of data where the first two
  // bytes are a count of the data bytes that follow.  cbMaxData is the longest
  // block we can accomodate, and the return value is the actual size of the
  // block.  Remember that Xilinx files are always big endian, so be sure to
  // swap bytes in the length word!
  //--
  uint16_t wLength;
  if (!ReadBytes(&wLength, 2)) return 0;
  if (SwapBytes(wLength) > cbMaxData) return 0;
  if (!ReadBytes(pData, wLength)) return 0;
  return wLength;
}


bool CBitStream::ReadString (string &str)
{
  //++
  //    This method reads a counted ASCII string (the same block format as
  // the ReadBlock() routine!) and converts it to a C++ style string.
  //--
  size_t cb;   char sz[512];
  memset(sz, 0, sizeof(sz));  str.clear();
  cb = ReadBlock(sz, sizeof(sz)-1);
  if (cb == 0) return false;
  str = sz;  return true;
}


bool CBitStream::ReadFile()
{
  //++
  //   This routine reads the entire Xilinx bitstream file, including the
  // header information (design name, date time, FPGA part number, etc) as well
  // as the raw configuration bit stream.  It returns true if all is well.
  //--

  //   The Xilinx bit stream always starts with a header block exactly 9 bytes
  // long.  The first eight bytes repeat the pattern 0x0F 0xF0 ... and the last
  // byte is always zero...
  uint8_t ab[9]; 
  if (ReadBlock(&ab, sizeof(ab)) != sizeof(ab)) goto NotXilinx;
  if ((ab[0]!=0x0F) || (ab[2]!=0x0F) || (ab[4]!=0x0F) || (ab[6]!=0x0F)) goto NotXilinx;
  if ((ab[1]!=0xF0) || (ab[3]!=0xF0) || (ab[5]!=0xF0) || (ab[7]!=0xF0)) goto NotXilinx;
  if (ab[8] != 0) goto NotXilinx;

  //  The next block is the letter 'a' followed by the design name (this seems
  // to be the name of the NCD file, FWIW).  Oddly, this this time alone the
  // 'a' is preceeded by a length field (always 1, of course) effectively making
  // it a counted string.  None of the other fields have this so perhaps I
  // misunderstand something, but there it is...
  uint8_t bToken;
  if (ReadBlock(&bToken, sizeof(bToken)) != sizeof(bToken)) goto NotXilinx;
  if (bToken != 'a') goto NotXilinx;
  if (!ReadString(m_sDesignName)) goto NotXilinx;

  //   Next are the tokens 'b', 'c' and 'd' followed by the Xilinx part name,
  // the design compilation date and time.  In this case there is no length
  // field preceeding the letters - only the argument strings...
  if (!ReadBytes(&bToken, sizeof(bToken))) goto NotXilinx;
  if (bToken != 'b') goto NotXilinx;
  if (!ReadString(m_sPartName)) goto NotXilinx;
  if (!ReadBytes(&bToken, sizeof(bToken))) goto NotXilinx;
  if (bToken != 'c') goto NotXilinx;
  if (!ReadString(m_sDesignDate)) goto NotXilinx;
  if (!ReadBytes(&bToken, sizeof(bToken))) goto NotXilinx;
  if (bToken != 'd') goto NotXilinx;
  if (!ReadString(m_sDesignTime)) goto NotXilinx;

  //   And next is the actual bit stream data.  This is preceeded by a four
  // byte, longword, length field (instead of the usual two byte length used
  // by the others) and as always we have to be careful of the byte ordering.
  uint16_t cbBits0, cbBits1;
  if (!ReadBytes(&bToken, sizeof(bToken))) goto NotXilinx;
  if (bToken != 'e') goto NotXilinx;
  if (!(ReadBytes(&cbBits0, 2) && ReadBytes(&cbBits1, 2))) goto NotXilinx;
  m_cbBits = MKLONG(SwapBytes(cbBits0), SwapBytes(cbBits1));

  // Finally there is the raw bit stream data.
  m_pabBits = DBGNEW uint8_t[m_cbBits];
  if (!ReadBytes(m_pabBits, m_cbBits)) goto NotXilinx;

  // That's it - this is a good file!
  return true;

  // Here if the file format isn't right for a Xilinx bit stream ...
NotXilinx:
  LOGS(ERROR, m_sFileName << " does not look like a Xilinx bit stream");
  return false;
}


bool CBitStream::Open (const string &sFileName)
{
  //++
  //   This method will attempt to open the bitstream file.  It returns true
  // if all is well and false, along with logging an error message, if anything
  // goes wrong.  The filenae may be passed to either this routine or to the
  // constructor, but if neither was given then the Open() will fail. 
  //
  //   If we are successful opening the file, then this routine will attempt
  // to verify that it is indeed a Xilinx bitstream.  If that goes well, we'll
  // attempt to read the file header (which contains known information such as
  // the design name, part type, date compiled, etc) and then we'll load the
  // actual FPGA programming bits into memory.  If anything goes wrong during
  // this process (bad format, not a Xilinx file, etc) then we'll log an error
  // message and return false.
  //
  //   That's right - everything of importance from the file is loaded into
  // memory, incuding the actual FPGA configuration bits.  For the Xilinx parts
  // used on the MESA boards this runs to about 500K - a fair amount of space,
  // but we can afford it.  Besides, it makes life much easier later to just
  // keep all this stuff around in memory.
  //
  //   This means that once we're done loading the file we can close it, and
  // in fact Open() does exactly that - closes the file before it returns!
  // There is no explicit close operation fro CBitStream() objects.  It sounds
  // a bit odd, but now you know why...
  //--
  Clear();
  if (!sFileName.empty()) m_sFileName = sFileName;
  if (m_sFileName.empty()) return false;
  m_sFileName = CCmdParser::SetDefaultExtension(m_sFileName, ".bit");
  int err = fopen_s(&m_pFile, m_sFileName.c_str(), "rb");
  if (err != 0) return Error("opening", err);
  if (!ReadFile()) return false;
  fclose(m_pFile);  m_pFile = NULL;
  return true;
}
