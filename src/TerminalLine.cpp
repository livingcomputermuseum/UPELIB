//++
// TerminalLine.cpp -> TELNET protocol implementation methods
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
//   This class, together with the CTerminalServer class, implements a simple
// but complete TELNET server package.  It may be used in conjunction with a
// host terminal multiplexer emulation to implement Internet connectivity even
// for hosts that are too old to be "TCP/IP aware".  The CTerminalLine class
// is primarily concerned with implementing the TELNET protocol for a single
// line, and the CTerminalServer class manages the network connections and
// sending and receiving TCP/UIP data.  One CTerminalLine object is created
// for each active TELNET connection.
//
//   This class, CTerminalLine, implements the actual TELNET protocol.  Right
// now our implementation is very limited but completely conforms to the TELNET
// RFC854 et al, specification.  This server doesn't implement ANY options, so
// if the other NVT sends us any "IAC DO xyz" sequences, we will always respond
// with "IAC WON'T xyz".  Likewise we don't offer any options, so we never send
// "IAC WILL xyz".
//
//   We know how to negotiate exactly two things, local echo and suppress go
// ahead (aka SGA) mode and we're able to handle the responses associated with
// those.  Anything else is an error and if the other end offers to do something
// by sending us a "IAC WILL xyz", then we always decline by responding with
// "IAC DON'T xyz".
//
//   We don't implement, and in fact we can't even parse, suboption negotiation
// sequences.  OTOH, we won't agree to any option that requires subnegotiation
// either, so the other end should never try to start a subnegotiation and it's
// screwed up on that end if it does.
//
// Bob Armstrong <bob@jfcl.com>   [17-MAY-2016]
//
// REFERENCES:
//   https://tools.ietf.org/html/rfc854
//   https://tools.ietf.org/html/rfc855
//   https://tools.ietf.org/html/rfc857
//   https://tools.ietf.org/html/rfc858
//   http://www.iana.org/assignments/telnet-options/telnet-options.xml
//   http://www.softpanorama.net/Net/Application_layer/telnet.shtml
//   http://www.tcpipguide.com/free/t_TelnetOptionsandOptionNegotiation-2.htm
//   https://support.microsoft.com/en-us/kb/231866
//
// REVISION HISTORY:
// 17-MAY-16  RLA   New file.
// 12-Jul-16  RLA   Finish up.
// 28-FEB-17  RLA   Make 64 bit clean.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>               // exit(), system(), etc ...
#include <stdint.h>               // uint8_t, uint32_t, etc ...
#include <assert.h>               // assert() (what else??)
#include <winsock2.h>             // socket interface declarations ...
#include <ws2tcpip.h>             // newer TCP/IP functions for winsock v2
#include "UPELIB.hpp"             // global declarations for this library
#include "LogFile.hpp"            // message logging facility
#include "ImageFile.hpp"          // needed for CTextInputFile
#include "TerminalLine.hpp"       // declarations for this module
#include "TerminalServer.hpp"     // TELNET server class declarations


CTerminalLine::CTerminalLine (uint32_t nLine, intptr_t skt, CTerminalServer &Server)
  : m_Server(Server)
{
  //++
  // The constructor just initializes all the members ...
  //--
  m_nLine = nLine;  m_ClientSocket = skt;
  m_lClientIP = 0;  m_nClientPort = 0;
  m_staCurrent = STA_NORMAL;
  m_optLocalEcho = OPT_ENABLED;
  m_optLocalSGA = m_optRemoteSGA = OPT_DISABLED;

  // Attempt to get the IP and port of our remote client ...
  sockaddr_in sin;  int cb = sizeof(sockaddr_in);
  if (getpeername(m_ClientSocket, (sockaddr *) &sin, &cb) != 0) {
    LOGF(WARNING, "TELNET getpeername() failed (d) for line %d", WSAGetLastError(), m_nLine);
  } else {
    m_lClientIP = ntohl(sin.sin_addr.s_addr);
    m_nClientPort = ntohs(sin.sin_port);
  }
}


CTerminalLine::~CTerminalLine()
{
  //++
  //--
}


string CTerminalLine::GetClientAddress() const
{
  //++
  // Return the IP address (as a string) of our client ...
  //--
  return FormatIPaddress(GetClientIP(), GetClientPort());
}


bool CTerminalLine::SocketWrite (const uint8_t abData[], size_t cbData)
{
  //++
  //  This method sends raw data to the other end and it's used only in this
  // module to send escape sequences.  To send actual terminal text to the
  // other end, use the Send() method instead.
  //
  //   Note that this invokes the winsock send() method directly, and since
  // the socket is in asynchronous mode this call never blocks.  If the remote
  // NVT is not able to accept data right now then send() returns failure, and
  // we return FALSE.  NO BUFFERING IS CURRENTLY IMPLEMENTED, although it would
  // be easy enough to add some ...
  //--
  int cb = send(m_ClientSocket, (char *) abData, MKINT32(cbData), 0);
  return (cb == cbData);
}


bool CTerminalLine::Send (const uint8_t abData[], size_t cbData)
{
  //++
  //   This method is called by the terminal mux code to send text, maybe just
  // a single character and maybe more, to the client.  Really we should handle
  // binary data here, stuff NULs after CRs (that's actually required by the
  // TELNET protocol), and escape any IAC bytes in the data, but right now we
  // do none of those.  You can always add them later if they're needed.
  //--
  return SocketWrite(abData, cbData);
}


bool CTerminalLine::SendFile (const string &sFileName)
{
  //++
  //   This method will open the specified file (presumably an ASCII text file)
  // and send the entire contents to the remote end.  It's currently used to
  // implement a simple "message of the day" (aka "MOTD") function and allows
  // the system status and other news to be displayed when a new connection is
  // established.  If any errors occur we return FALSE and just give up - we
  // don't try very hard to recover.
  //
  //   Two last subtle points - it's possible that this routine might be called
  // more than once, simultaneously, on multiple threads for different TELNET
  // connections.  It's important that we always open the file for read only
  // access and with the "deny none" share mode to prevent conflicts.  Also
  // notice that this method blocks the current thread until the entire file
  // is sent.  The assumption is that the file is short and this won't be a
  // problem, but if you send a very long file then you might have to be more
  // careful.
  //--
  CTextInputFile in;  char sz[256];
  if (!in.Open(sFileName, true, _SH_DENYNO)) return false;
  while (in.ReadLine(sz, sizeof(sz)-1)) {
    if (!(Send(sz) && Send("\r\n"))) return false;
  }
  return true;
}


void CTerminalLine::SendCommand (uint8_t nCommand, uint8_t nOption)
{
  //++
  // Send a TELNET escape sequence to the remote end ...
  //--
  uint8_t abEscape[3] = {IAC, nCommand, nOption};
  if (!SocketWrite(abEscape, sizeof(abEscape))) {
    LOGS(WARNING, "TELNET failed to send command " << DecodeCommand(nCommand) << " " << DecodeOption(nOption));
  } else {
    LOGS(TRACE, "TELNET sent command " << DecodeCommand(nCommand) << " " << DecodeOption(nOption));
  }
}


void CTerminalLine::HandleWill (uint8_t nOption)
{
  //++
  //   This method is called when we receive an "IAC WILL option" sequence from
  // the other end.  This could be a response to our request that the other end
  // do something (e.g. ECHO or LINEMODE) or it could be an unsolicited offer
  // from the other end to enable an option (e.g. SUPPRESS GA).
  //--
  LOGS(TRACE, "TELNET received WILL " << DecodeOption(nOption));
  switch (nOption) {
    // SUPPRESS GO AHEAD (aka SGA) is good, so tell him that we agree!
    case OPT_SGA:
      LOGS(TRACE, "TELNET client SUPPRESS GO AHEAD accepted");
      if (m_optRemoteSGA != OPT_WAITING) SendDo(OPT_SGA);
      m_optRemoteSGA = OPT_ENABLED;  break;

    // Anything else we don't expect or understand ...
    default:
      LOGS(TRACE, "TELNET client " << DecodeOption(nOption) << " declined");
      SendDont(nOption);  break;
  }
}


void CTerminalLine::HandleWont (uint8_t nOption)
{
  //++
  //   This method is called when we receive an "IAC WON'T option" sequence
  // from the other end.  This is a refusal to do something and (I think) can
  // only occur in respose to a previous IAC DO request that we have sent.
  // I don't believe the other end should ever generate an unsolicited WONT
  // message.  That means the only options we can expect to see here are ones
  // that we actually know how to request!
  //--
  LOGS(WARNING, "TELNET received WON'T " << DecodeOption(nOption));
}


void CTerminalLine::HandleDo (uint8_t nOption)
{
  //++
  //   This method is called when we receive an "IAC DO option" sequence from
  // the other end.  This is a request from the remote client that start doing
  // something.
  //--
  LOGS(TRACE, "TELNET received DO " << DecodeOption(nOption));
  switch (nOption) {
    // SUPPRESS GO AHEAD (aka SGA) is good, so tell him that we agree!
    case OPT_SGA:
      LOGS(TRACE, "TELNET local SUPPRESS GO AHEAD enabled");
      if (m_optLocalSGA != OPT_WAITING) SendWill(OPT_SGA);
      m_optLocalSGA = OPT_ENABLED;  break;

    //   See SetLocalEcho() for more details on this one, but be careful -
    // we only change the ECHO option if we sent the request.  If the other
    // end sends us an unsolicited "DO ECHO" then we either refuse or accept
    // depending on our current state!
    case OPT_ECHO:
      switch (m_optLocalEcho) {
        case OPT_WAITING:
          LOGS(TRACE, "TELNET client local echo disabled");
          m_optLocalEcho = OPT_DISABLED;  break;
        // Remember that WILL and WON'T are backwards - see SetLocalEcho() ...
        case OPT_ENABLED:   SendWont(OPT_ECHO);             break;
        case OPT_DISABLED:  SendWill(OPT_ECHO);             break;
      }
      break;

    // Anything else we don't expect or understand ...
    default:
      LOGS(WARNING, "TELNET received unexpected DO " << DecodeOption(nOption));
      SendWont(nOption);  break;
  }
}


void CTerminalLine::HandleDont (uint8_t nOption)
{
  //++
  //   This method is called when we receive either an "IAC DON'T option"
  // sequence from the other end.  This is a request from the client that we
  // stop doing something.
  //--
  LOGS(TRACE, "TELNET received DON'T " << DecodeOption(nOption));
  switch (nOption) {
    //   The suppress go ahead option is required in the sense that we don't
    // know how to work without it.  If we get a negative response, then it's
    // a fatal error.
    case OPT_SGA:
      LOGS(WARNING, "TELNET SUPPRESS GO AHEAD option declined by client");  break;

    // See SetLocalEcho() for more details on this one ...
    case OPT_ECHO:
      LOGS(TRACE, "TELNET client local echo enabled");
      m_optLocalEcho = OPT_ENABLED;  break;

    // Anything else we don't expect or understand ...
    default:
      LOGS(WARNING, "TELNET received unexpected DON'T " << DecodeOption(nOption));  break;
  }
}


CTerminalLine::TELNET_STATE CTerminalLine::NextState (uint8_t ch)
{
  //++
  //   This is the TELNET state machine engine - it looks at the current state,
  // in m_staCurrent, and the byte received (in ch) and decides what we should
  // do next.  The new state is returned.
  //--
  switch (m_staCurrent) {

    //   If we receive an IAC in the normal state, then switch to the IAC_RCVD
    // state and wait for more.  If the state is normal and the byte is NOT an
    // IAC then we really shouldn't be here at all!
    case STA_NORMAL: return (ch == IAC) ? STA_IAC_RCVD : STA_NORMAL;

    //   If the last character received was CR then see if this is an LF or
    // a NUL.  If it's either of those then just drop it.  Otherwise pass it
    // along as a normal character (see the comments above Receive()!).  And
    // in either case, the next state is always back to STA_NORMAL...
    case STA_CR_LAST:
      if ((ch != CHNUL) && (ch != CHLFD)) m_Server.ReceiveCallback(m_nLine, ch);
      return STA_NORMAL;

    //   If the last byte received was an IAC, then the next byte should be one
    // of WILL/WONT/DO/DONT.  Actually many others are possible according to the
    // protocol, but those are the only ones we understand!  Well, almost - an
    // escaped IAC (i.e. "IAC IAC") sends a single 255 to the host.
    case STA_IAC_RCVD:
      switch (ch) {
        case IAC_WILL: return STA_WILL_RCVD;
        case IAC_WONT: return STA_WONT_RCVD;
        case IAC_DO:   return STA_DO_RCVD;
        case IAC_DONT: return STA_DONT_RCVD;
        case IAC: m_Server.ReceiveCallback(m_nLine, ch);  return STA_NORMAL;
        default:
          LOGS(WARNING, "TELNET received unimplemented command " << DecodeCommand(ch) << " received");
          return STA_NORMAL;
      }

    //  "IAC WILL xyz" and "IAC WON'T xyz" are both either a response to our
    // request for an option (e.g. ECHO or LINEMODE) or an unsolicited offer
    // to enable an option.  Either way, the current byte is the option.
    case STA_WILL_RCVD:  HandleWill(ch);  return STA_NORMAL;
    case STA_WONT_RCVD:  HandleWont(ch);  return STA_NORMAL;

    //   "IAC DO xyz" and "IAC DON'T xyz" are both requests from the other end
    // that we either start doing something or stop doing something.  Either
    // way, the current byte is the requested option.
    case STA_DO_RCVD:    HandleDo  (ch);  return STA_NORMAL;
    case STA_DONT_RCVD:  HandleDont(ch);  return STA_NORMAL;

    //   Anything else means that somebody has added a new state without adding
    // a corresponding state transition to this table.  Shame on you!!
    default: assert(false);  return STA_NORMAL;
  }
}


void CTerminalLine::Receive (uint8_t ch)
{
  //++
  //   This routine is called by the CTerminalServer method when we receive
  // data from the remote NVT.  If the byte is part of a command then we need
  // to run the TELNET state machine, but otherwise it's just data that we
  // pass along to the terminal multiplexer emulation via the RECEIVE_CALLBACK.
  //
  //   Note that this code doesn't do any buffering - if the multiplexer isn't
  // ready to handle the data right now, then that's too bad.  The data will
  // just be lost.  That works well enough for now but it might be a little
  // overly restrictive, so soomebody could implement some kind of buffering
  // here sometime...
  //
  //   One last word of advice - TELNET is somewhat conflicted about how it
  // handles carriage return (^M, ASCII 0x0D) characters.  Some TELNET clients
  // view the "ENTER" key as meaning literally "end of line" and will send
  // both CR and LF for that key.  Other implementations view ENTER as meaning
  // carriage return and send either CR (0x0D) alone, or CR NUL (0x0D 0x00)
  // instead.  The NUL is sent because a literal interpretation of the TELNET
  // specification requires it, but it's not necessary on modern systems.
  //
  //   AFAIK, suprisingly there isn't actually a TELNET option to control this
  // behavior, so we'll have to deal with it in our state machine.  ASCII NUL
  // characters (0x00) are simply always ignored, and LF (0x0A) characters are
  // ignored if they were immediately preceeded by a CR.
  //--
  //LOGF(TRACE, "TELNET received 0x%02X on line %d", ch, m_nLine);
  if ((m_staCurrent == STA_NORMAL) && (ch != IAC)) {
    //   This is normal text, with two special cases - NUL characters are
    // always ignored, and if this is a CR then the next state becomes
    // STA_CR_LAST instead of STA_NORMAL!
    if (ch != CHNUL) m_Server.ReceiveCallback(m_nLine, ch);
    if (ch == CHCRT) m_staCurrent = STA_CR_LAST;
  } else
    // For any state other than normal, just run the state machine!
    m_staCurrent = NextState(ch);
}


void CTerminalLine::SetLocalEcho (bool fEcho)
{
  //++
  //   The goal of this function is to enable or disable local echo on the
  // remote client.  Sounds simple, but the way this works in TELNET is not at
  // all obvious.  You might think we could just send a "IAC DO ECHO" or "IAC
  // DONT ECHO" to the other end, but no - that's completely wrong.  Turns out
  // that sending a "DO ECHO" to the client actually asks it to echo our output
  // back to us!!
  //
  //   It has nothing to do with whether the client echos local keyboard input
  // back to its own screen or not.  In fact, there is no explicit way to tell
  // the client whether to locally echo input or not.  Yes, believe it or not -
  // read RFC857 if you don't believe me!  There is good news, however, because
  // it turns out that most clients will disable local echo if we tell them
  // that we want to handle the echo ourselves.
  //
  //   So, if we want the client to stop local echo then we send them an "IAC
  // WILL ECHO" indicating that we offer to handle the echo ourselves.  With
  // any luck, the client will respond with "IAC DO ECHO" and stop echoing
  // locally.  Likewise, to get the client to start echoing locally we need to
  // send them a "IAC WONT ECHO" indicating we're not handling echo anymore.
  // It's all a bit odd, but it makes sense to somebody ...
  //
  //   One last thing - note that the TELNET default is "DONT ECHO" on both
  // ends.  That means that most clients will start off in local echo mode by
  // default until we tell them otherwise.
  //--

  //   If we've already sent an ECHO offer recently and we're still waiting
  // for a response, then don't send another ...
  if (m_optLocalEcho == OPT_WAITING) return;

  //   Tell the other end to change state and then wait for a response.  Note
  // that the answer gets handled by the HandleDo() or HandleDont() methods ...
  if (fEcho) {
    if (m_optLocalEcho == OPT_ENABLED) return;
    SendWont(OPT_ECHO);
  } else {
    if (m_optLocalEcho == OPT_DISABLED) return;
    SendWill(OPT_ECHO);
  }
  m_optLocalEcho = OPT_WAITING;
}


void CTerminalLine::SuppressGoAhead()
{
  //++
  //   This method will attempt to negotiate TELNET SUPPRESS GO AHEAD mode for
  // both the client and the server.  GO AHEAD mode uses the "IAC GA" command
  // and is a holdover from the days of half duplex terminals.  It's pretty
  // much never used anymore, but it remains the TELNET default mode and so we
  // need to change it.  FWIW, this code is not even capable of operating in
  // half duplex mode so if the remote end declines SUPPRESS GO AHEAD mode
  // (we receive either "IAC WONT SGA" or "IAC DONT SGA") then it's a fatal
  // error.  Fortunate all modern clients support full duplex mode now.
  //
  //   This is basically a simplified case of what we did to negotiate the
  // local echo mode, but in this case we only do it once, at startup, and we
  // never change the SGA option after that.  In this case, though, there are
  // two SGA options we're interested in - one for our end (LocalSGA) and one
  // for the client (RemoteSGA) end.  And some clients may be way ahead of us
  // too, and either offer to or ask for SGA mode before we get to it.  We
  // have to be careful to avoid negotiation loops in those instances.
  //
  //   FWIW, PuTTY will both offer to ("IAC WILL SGA") and ask that we ("IAC
  // DO SGA") suppress go ahead when it starts up.  The Windows TELNET client
  // says nothing about either by default, but will respond to both if we ask
  // first.  The Multinet TELNET client offers to suppress go ahead on its end
  // ("WILL SGA") but doesn't ask us to by default.  It'll also do both if we
  // ask nicely.
  //--
  if (m_optLocalSGA  != OPT_ENABLED) {
    SendWill(OPT_SGA);  m_optLocalSGA = OPT_WAITING;
  }
  if (m_optRemoteSGA != OPT_ENABLED) {
    SendDo(OPT_SGA);  m_optRemoteSGA = OPT_WAITING;
  }
}


#ifdef _DEBUG
string CTerminalLine::DecodeCommand (uint8_t nCmd)
{
  //++
  // Translate a TELNET command to an ASCII string ...
  //--
  char sz[64];
  switch (nCmd) {
    case IAC:       sprintf_s(sz, sizeof(sz), "IAC");           break;
    case IAC_SE:    sprintf_s(sz, sizeof(sz), "SE");            break;
    case IAC_NOP:   sprintf_s(sz, sizeof(sz), "NOP");           break;
    case IAC_DM:    sprintf_s(sz, sizeof(sz), "DM");            break;
    case IAC_BRK:   sprintf_s(sz, sizeof(sz), "BRK");           break;
    case IAC_IP:    sprintf_s(sz, sizeof(sz), "IP");            break;
    case IAC_AO:    sprintf_s(sz, sizeof(sz), "AO");            break;
    case IAC_AYT:   sprintf_s(sz, sizeof(sz), "AYT");           break;
    case IAC_EC:    sprintf_s(sz, sizeof(sz), "EC");            break;
    case IAC_EL:    sprintf_s(sz, sizeof(sz), "EL");            break;
    case IAC_GA:    sprintf_s(sz, sizeof(sz), "GA");            break;
    case IAC_SB:    sprintf_s(sz, sizeof(sz), "SB");            break;
    case IAC_WILL:  sprintf_s(sz, sizeof(sz), "WILL");          break;
    case IAC_WONT:  sprintf_s(sz, sizeof(sz), "WONT");          break;
    case IAC_DO:    sprintf_s(sz, sizeof(sz), "DO");            break;
    case IAC_DONT:  sprintf_s(sz, sizeof(sz), "DONT");          break;
    default:        sprintf_s(sz, sizeof(sz), "0x%02X", nCmd);  break;
  }
  return string(sz);
}


string CTerminalLine::DecodeOption (uint8_t nOpt)
{
  //++
  // Translate a TELNET option to an ASCII string ...
  //--
  char sz[64];
  switch (nOpt) {
    case OPT_BINARY:      sprintf_s(sz, sizeof(sz), "TRANSMIT-BINARY");       break;
    case OPT_ECHO:        sprintf_s(sz, sizeof(sz), "ECHO");                  break;
    case OPT_RECONNECT:   sprintf_s(sz, sizeof(sz), "RECONNECTION");          break;
    case OPT_SGA:         sprintf_s(sz, sizeof(sz), "SUPPRESS-GO-AHEAD");     break;
    case OPT_STATUS:      sprintf_s(sz, sizeof(sz), "STATUS");                break;
    case OPT_AMSN:        sprintf_s(sz, sizeof(sz), "AMSN");                  break;
    case OPT_TIMINGMARK:  sprintf_s(sz, sizeof(sz), "TIMING-MARK");           break;
    case OPT_RCTE:        sprintf_s(sz, sizeof(sz), "RCTE");                  break;
    case OPT_OUTLINEWID:  sprintf_s(sz, sizeof(sz), "OUTPUT-LINE-WIDTH");     break;
    case OPT_OUTPAGESIZ:  sprintf_s(sz, sizeof(sz), "OUTPUT-PAGE-SIZE");      break;
    case OPT_NAOCRD:      sprintf_s(sz, sizeof(sz), "NAOCRD");                break;
    case OPT_NAOHTS:      sprintf_s(sz, sizeof(sz), "NAOHTS");                break;
    case OPT_NAOHTD:      sprintf_s(sz, sizeof(sz), "NAOHTD");                break;
    case OPT_NAOFFD:      sprintf_s(sz, sizeof(sz), "NAOFFD");                break;
    case OPT_NAOVTS:      sprintf_s(sz, sizeof(sz), "NAOVTS");                break;
    case OPT_NAOVTD:      sprintf_s(sz, sizeof(sz), "NAOVTD");                break;
    case OPT_NAOLFD:      sprintf_s(sz, sizeof(sz), "NAOLFD");                break;
    case OPT_EXTENDASC:   sprintf_s(sz, sizeof(sz), "EXTEND-ASCII");          break;
    case OPT_LOGOUT:      sprintf_s(sz, sizeof(sz), "LOGOUT");                break;
    case OPT_BM:          sprintf_s(sz, sizeof(sz), "BM");                    break;
    case OPT_DET:         sprintf_s(sz, sizeof(sz), "DET");                   break;
    case OPT_SUPDUP:      sprintf_s(sz, sizeof(sz), "SUPDUP");                break;
    case OPT_SUPDUPOUT:   sprintf_s(sz, sizeof(sz), "SUPDUP-OUTPUT");         break;
    case OPT_SENDLOC:     sprintf_s(sz, sizeof(sz), "SEND-LOCATION");         break;
    case OPT_TERMTYPE:    sprintf_s(sz, sizeof(sz), "TERMINAL-TYPE");         break;
    case OPT_EOR:         sprintf_s(sz, sizeof(sz), "END-OF-RECORD");         break;
    case OPT_TUID:        sprintf_s(sz, sizeof(sz), "TUID");                  break;
    case OPT_OUTMRK:      sprintf_s(sz, sizeof(sz), "OUTMRK");                break;
    case OPT_TTYLOC:      sprintf_s(sz, sizeof(sz), "TTYLOC");                break;
    case OPT_3270REGIME:  sprintf_s(sz, sizeof(sz), "3270-REGIME");           break;
    case OPT_X3PAD:       sprintf_s(sz, sizeof(sz), "X.3-PAD");               break;
    case OPT_NAWS:        sprintf_s(sz, sizeof(sz), "NAWS");                  break;
    case OPT_TERMSPEED:   sprintf_s(sz, sizeof(sz), "TERMINAL-SPEED");        break;
    case OPT_REMFLOWCTL:  sprintf_s(sz, sizeof(sz), "TOGGLE-FLOW-CONTROL");   break;
    case OPT_LINEMODE:    sprintf_s(sz, sizeof(sz), "LINEMODE");              break;
    case OPT_XDISPLOC:    sprintf_s(sz, sizeof(sz), "X-DISPLAY-LOCATION");    break;
    case OPT_ENVIRON:     sprintf_s(sz, sizeof(sz), "ENVIRON");               break;
    case OPT_AUTHEN:      sprintf_s(sz, sizeof(sz), "AUTHENTICATION");        break;
    case OPT_ENCRYPT:     sprintf_s(sz, sizeof(sz), "ENCRYPT");               break;
    case OPT_NEWENVIRON:  sprintf_s(sz, sizeof(sz), "NEW-ENVIRON");           break;
    case OPT_TN3270E:     sprintf_s(sz, sizeof(sz), "TN3270E");               break;
    case OPT_XAUTH:       sprintf_s(sz, sizeof(sz), "XAUTH");                 break;
    case OPT_CHARSET:     sprintf_s(sz, sizeof(sz), "CHARSET");               break;
    case OPT_RSP:         sprintf_s(sz, sizeof(sz), "RSP");                   break;
    case OPT_COMMPORT:    sprintf_s(sz, sizeof(sz), "COM-PORT-OPTION");       break;
    case OPT_SUPPECHO:    sprintf_s(sz, sizeof(sz), "SUPPRESS-ECHO");         break;
    case OPT_STARTTLS:    sprintf_s(sz, sizeof(sz), "START-TLS");             break;
    case OPT_KERMIT:      sprintf_s(sz, sizeof(sz), "KERMIT");                break;
    case OPT_SENDURL:     sprintf_s(sz, sizeof(sz), "SEND-URL");              break;
    case OPT_FORWARDX:    sprintf_s(sz, sizeof(sz), "FORWARD-X");             break;
    case OPT_EXOPL:       sprintf_s(sz, sizeof(sz), "EXTENDED-OPTIONS-LIST"); break;
    default:              sprintf_s(sz, sizeof(sz), "0x%02X", nOpt);          break;
  }
  return string(sz);
}
#endif
