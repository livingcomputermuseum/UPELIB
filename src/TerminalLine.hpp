//++
// TerminalLine.hpp -> Simple TELNET protocol definitions
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
//   This CTerminalLine class primarily implements the actual TELNET protocol.
// The CTerminalServer class is concerned mainly with handling the TCP/IP
// connections and sending and receiving raw data.  
//
// Bob Armstrong <bob@jfcl.com>   [17-MAY-2016]
//
// REVISION HISTORY:
// 16-MAY-16  RLA   New file.
// 12-Jul-16  RLA   Finish up.
// 28-FEB-17  RLA   Make 64 bit clean.
//--
#pragma once
#include <string>               // C++ std::string class, et al ...
using std::string;              // ...
class CTerminalServer;          // forward reference this class


// CTerminalLine class definition ...
class CTerminalLine {
  //++
  //   This class handles a single TELNET connection.  The complete TELNET
  // server is implemented by the CTerminalServer class, which is a collection
  // of these objects...
  //--

  // Constants ...
public:
  enum {
    // TELNET IAC ("interpret as command") commands ...
    //   Not all of these are implemented, and in fact the majority of them are
    // not, but they're all defined here for future expansion.
    IAC_EOF           = 236,  // end of file
    IAC_SP            = 237,  // suspend process
    IAC_ABORT         = 238,  // abort process
    IAC_EOR           = 239,  // end of record
    IAC_SE            = 240,  // end of subnegotiation parameters
    IAC_NOP           = 241,  // no operation
    IAC_DM            = 242,  // data mark
    IAC_BRK           = 243,  // break
    IAC_IP            = 244,  // suspend
    IAC_AO            = 245,  // abort output
    IAC_AYT           = 246,  // are you there
    IAC_EC            = 247,  // erase character
    IAC_EL            = 248,  // erase line
    IAC_GA            = 249,  // go ahead
    IAC_SB            = 250,  // subnegotiation of the indicated option follows
    IAC_WILL          = 251,  // we want to do something
    IAC_WONT          = 252,  // we refuse to do something
    IAC_DO            = 253,  // we want the other end to do something
    IAC_DONT          = 254,  // we want the other end to stop doing something
    IAC               = 255,  // interpet as command prefix
    // TELNET options for the IAC_SB command ...
    //   As above, most of these are not currently implemented!
    OPT_BINARY        =   0,  // transmit binary
    OPT_ECHO          =   1,  // echo (RFC857)
    OPT_RECONNECT     =   2,  // reconnection 
    OPT_SGA           =   3,  // suppress go ahead (RFC858)
    OPT_AMSN          =   4,  // approx message size
    OPT_STATUS        =   5,  // status
    OPT_TIMINGMARK    =   6,  // timing mark (RFC860)
    OPT_RCTE          =   7,  // remote transmission and echo
    OPT_OUTLINEWID    =   8,  // output line width
    OPT_OUTPAGESIZ    =   9,  // output page size
    OPT_NAOCRD        =  10,  // output carriage return disposition
    OPT_NAOHTS        =  11,  // output horizontal tab stops
    OPT_NAOHTD        =  12,  // output horizontal tab stop disposition
    OPT_NAOFFD        =  13,  // output formfeed disposition
    OPT_NAOVTS        =  14,  // output vertical tabstops
    OPT_NAOVTD        =  15,  // output vertical tab disposition
    OPT_NAOLFD        =  16,  // output linefeed disposition
    OPT_EXTENDASC     =  17,  // extended ascii
    OPT_LOGOUT        =  18,  // logout
    OPT_BM            =  19,  // byte macro
    OPT_DET           =  20,  // data entry terminal
    OPT_SUPDUP        =  21,  // SUPDUP display protocol
    OPT_SUPDUPOUT     =  22,  // SUPDUP output
    OPT_SENDLOC       =  23,  // send location
    OPT_TERMTYPE      =  24,  // terminal type (RFC1091)
    OPT_EOR           =  25,  // end of record
    OPT_TUID          =  26,  // user id
    OPT_OUTMRK        =  27,  // output marking
    OPT_TTYLOC        =  28,  // terminal location number
    OPT_3270REGIME    =  29,  // 3270 regime
    OPT_X3PAD         =  30,  // X.3-PAD
    OPT_NAWS          =  31,  // window size (RFC1073)
    OPT_TERMSPEED     =  32,  // terminal speed (RFC1079)
    OPT_REMFLOWCTL    =  33,  // toggle flow control (RFC1372)
    OPT_LINEMODE      =  34,  // linemode (RFC1184)
    OPT_XDISPLOC      =  35,  // X display location
    OPT_ENVIRON       =  36,  // telnet environment (RFC1408)
    OPT_AUTHEN        =  37,  // authentication
    OPT_ENCRYPT       =  38,  // encryption option
    OPT_NEWENVIRON    =  39,  // new environment
    OPT_TN3270E       =  40,  // TN3270E
    OPT_XAUTH         =  41,  // *XAUTH
    OPT_CHARSET       =  42,  // CHARSET
    OPT_RSP           =  43,  // remote serial port
    OPT_COMMPORT      =  44,  // com port control option
    OPT_SUPPECHO      =  45,  // suppress local echo
    OPT_STARTTLS      =  46,  // start TLP
    OPT_KERMIT        =  47,  // KERMIT
    OPT_SENDURL       =  48,  // send URL
    OPT_FORWARDX      =  49,  // forward X
    OPT_EXOPL         = 255,  // extended-options-list
  };

  // States for the TELNET engine ...
private:
  enum _TELNET_STATES {
    STA_NORMAL        =  0,   // the next byte is nothing special
    STA_IAC_RCVD      =  1,   // next byte is a command code
    STA_WILL_RCVD     =  2,   // IAC WILL  received, next byte is the option
    STA_WONT_RCVD     =  3,   // IAC WON'T   "   "     "   "    "  "    "
    STA_DO_RCVD       =  4,   // IAC DO      "   "     "   "    "  "    "
    STA_DONT_RCVD     =  5,   // IAC DON'T   "   "     "   "    "  "    "
    STA_CR_LAST       =  6,   // CR (carriage return) received last
  };
  typedef enum _TELNET_STATES TELNET_STATE;

  // States for option negotiation ...
private:
  enum _OPTION_STATES {
    OPT_DISABLED      =  0,   // this option is disabled
    OPT_ENABLED       =  1,   // this option is enabled
    OPT_WAITING       =  3,   // waiting for a response from the client
  };
  typedef enum _OPTION_STATES OPTION_STATE;

  // Constructor and destructor ...
public:
  CTerminalLine(uint32_t nLine, intptr_t skt, CTerminalServer &Server);
  virtual ~CTerminalLine();
  // Disallow copy and assignment operations with CTerminalLine objects...
private:
  CTerminalLine(const CTerminalLine &e) = delete;
  CTerminalLine& operator= (const CTerminalLine &e) = delete;

  // Public CTerminalLine properties ...
public:
  // Return the associated CTerminalServer object, line number or socket ...
  CTerminalServer *GetServer() const {return &m_Server;}
  uint32_t GetLine() const {return m_nLine;}
  intptr_t GetSocket() const {return m_ClientSocket;}
  // Return the remote NVT client's IP address and/or port ...
  uint32_t GetClientIP() const {return m_lClientIP;}
  uint16_t GetClientPort() const {return m_nClientPort;}
  string GetClientAddress() const;

  // Public CTerminalLine methods ...
public:
  // Handle characters received from the remote NVT ...
  void Receive(uint8_t ch);
  // Send text to the remote NVT ...
  bool Send (const uint8_t abData[], size_t cbData);
  bool Send (const char *psz) {return Send((uint8_t *) psz, strlen(psz));}
  bool Send (char ch)         {return Send((uint8_t *) &ch, 1);}
  bool Send (string str)      {return Send(str.c_str());}
  // Send an entire text file to the remote NVT ...
  bool SendFile (const string &sFileName);
  // Control the remote ECHO option ...
  void SetLocalEcho (bool fEcho=true);
  // Enable SUPPRESS GO AHEAD mode for both client and server ...
  void SuppressGoAhead();

  // Private CTerminalLine methods ...
private:
  // Run the TELNET state machine ...
  TELNET_STATE NextState (uint8_t ch);
  // An IAC WILL is an either offer or an agreement to do something ...
  void HandleWill (uint8_t nOption);
  // An IAC WONT is a refusal to do something ...
  void HandleWont (uint8_t nOption);
  // An IAC DO is a request that we start doing something ...
  void HandleDo (uint8_t nOption);
  // An IAC DONT is a demand that we stop doing something ...
  void HandleDont (uint8_t nOption);
  // Send various IAC escape sequences to the remote end ...
  void SendCommand  (uint8_t nCommand, uint8_t nOption);
  void SendWill (uint8_t nOption) {SendCommand(IAC_WILL, nOption);}
  void SendWont (uint8_t nOption) {SendCommand(IAC_WONT, nOption);}
  void SendDo   (uint8_t nOption) {SendCommand(IAC_DO,   nOption);}
  void SendDont (uint8_t nOption) {SendCommand(IAC_DONT, nOption);}
  // Send raw data to the other end ...  
  bool SocketWrite (const uint8_t abData[], size_t cbData);
#ifdef _DEBUG
  // Special methods for debugging ...
  static string DecodeCommand (uint8_t nCmd);
  static string DecodeOption (uint8_t nOpt);
#endif

  // Local CTerminalLine members ...
private:
  CTerminalServer &m_Server;        // the TELNET server that owns us
  uint32_t         m_nLine;         // our line number on this server
  intptr_t         m_ClientSocket;  // socket used by our client
  uint32_t         m_lClientIP;     // IP address of the remote end
  uint16_t         m_nClientPort;   // port used by the remote end
  TELNET_STATE     m_staCurrent;    // current telnet engine state
  OPTION_STATE     m_optLocalEcho;  // the client echos locally
  OPTION_STATE     m_optLocalSGA;   // local (server) SUPPRESS GO AHEAD state
  OPTION_STATE     m_optRemoteSGA;  // remote (client)  "   "   "   "     "
};
