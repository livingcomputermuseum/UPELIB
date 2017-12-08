//++
// TerminalServer.hpp -> Simple TELNET server definitions
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
//   The CTerminalServer class is concerned mainly with handling the TCP/IP
// connections and sending and receiving raw data.
//
//   Note that CTerminalServer is a "modified" Singleton class - only one 
// instance per application can be created, however the constructor must be
// explicitly called, once, to create that instance.  Subsequent calls to the
// constructor will cause assertion failures.  A pointer to the original
// instance may be retrieved at any time by calling GetServer().
//
// Bob Armstrong <bob@jfcl.com>   [17-MAY-2016]
//
// REVISION HISTORY:
// 16-MAY-16  RLA   New file.
// 28-FEB-17  RLA   Make 64 bit clean.
//--
#pragma once
#include <string>               // C++ std::string class, et al ...
#include <vector>               // C++ std::vector template
#include <unordered_map>        // C++ std::unordered_map template
#include "UPELIB.hpp"           // needed for FormatIPaddress()
using std::string;              // ...
class CTerminalLine;            // forward reference this class
class CMutex;                   // and this one
struct WSAData;                 // and this
struct sockaddr_in;             // and this

// Server invisible window constants ....
#define SERVER_WINDOW_CLASS    L"ServerWindow"
#define SERVER_WINDOW_TITLE    L"ServerWindow"
#define SERVER_WINDOW_ICON     L"ServerWindow"
#define WM_SOCKET              (WM_USER+100)


// CTerminalServer class definition ...
class CTerminalServer {
  //++
  //   This class is a collection of CTerminalLine objects.  This class
  // implements the code necessary to listen on the teLNET port and create
  // new connections, but most of the actual work needed to talk to the
  // remote NVT happens in the CTerminalLine object.
  //--

  // Constants ...
public:
  enum {
    DEFAULT_PORT  =   23,     // default port to listen on
    DEFAULT_LINES =   64,     // default number of lines to support
    MAXRECV       = 1024,     // maximum number of bytes in one recv() call
  };

  // Callback routine prototypes ...
public:
  typedef bool CONNECT_CALLBACK    (intptr_t lParam, uint32_t nLine);
  typedef void DISCONNECT_CALLBACK (intptr_t lParam, uint32_t nLine);
  typedef void RECEIVE_CALLBACK    (intptr_t lParam, uint32_t nLine, uint8_t ch);

  // Constructor and destructor ...
public:
  CTerminalServer (RECEIVE_CALLBACK *pReceive, CONNECT_CALLBACK *pConnect=NULL, DISCONNECT_CALLBACK *pDisconnect=NULL, intptr_t lCallbackParam=0, uint32_t nMaxLines=DEFAULT_LINES);
  virtual ~CTerminalServer();
  // Disallow copy and assignment operations with CTerminalServer objects...
private:
  CTerminalServer(const CTerminalServer &e) = delete;
  CTerminalServer& operator= (const CTerminalServer &e) = delete;

  // Define the terminal line collection, addressing and iterators ...
public:
  typedef std::vector<CTerminalLine *> TERMINAL_ARRAY;
  // Define array style addressing, where the index is the line number ...
  //   Notice that we use Line() instead of at(), and notice that Line() will
  // return a pointer to any line, or NULL if that line doesn't currently
  // exist. However array style addressing returns a reference and only works
  // for lines which actually exist.  Array addressing will assert if used to
  // access a non-existent line.
  bool IsLineConnected (uint32_t n)  const {return m_apLines[n] != NULL;}
  CTerminalLine *Line (uint32_t n) {return m_apLines.at(n);}
  const CTerminalLine *Line (uint32_t n) const {return m_apLines.at(n);}
  CTerminalLine& operator[] (uint32_t n)
    {CTerminalLine *p = Line(n);  assert(p != NULL);  return *p;}
  const CTerminalLine& operator[] (uint32_t n) const
    {const CTerminalLine *p = Line(n);  assert(p != NULL);  return *p;}
  // Define our own iterators for enumerating terminal lines...
  //   Well, that would be nice and we could just delegate to the vector
  // type iterators, but what we really want is an iterator that skips
  // over unused lines.  That's a little harder because we'd have to define
  // our own special CTerminalArrayIterator class so that we could overload
  // the ++ operator and make it do that.  And we'd probably want two of
  // those - one for const and one for non-const.  It's a lot of extra work
  // and I'll just skip it for now...

  //   But wait!  One collection is just not enough...  In addition to the
  // array of CTerminalLine pointers indexed by the line number, we keep a
  // secondary hash table collection that relates a SOCKET (the hash key)
  // to the line number.  This lets us quickly lookup the CTerminalLine
  // object from the socket when we receive an asynchronous windows event.
public:
  typedef std::unordered_map<intptr_t, uint32_t> SOCKET_HASH;
  // Return the CTerminalLine object associated with the specified socket ...
  bool IsSocketConnected (intptr_t skt) const
    {return m_mapSocket.find(skt) != m_mapSocket.end();}
  uint32_t SocketToLine (intptr_t skt) {return m_mapSocket[skt];}
  //   It's tempting to define an iterator here - after all, m_mapSocket
  // doesn't have any NULL entries - but that won't return the lines in line
  // number order, which we'd really like.

  // Public CTerminalServer properties ...
public:
  // Return a pointer to the one and only CTerminalserver object ...
  static CTerminalServer *GetServer() {assert(m_pServer != NULL);  return m_pServer;}
  // Return TRUE if the server is running ...
  bool IsServerRunning() const {return m_idServerThread != 0;}
  // Return the maximum number of lines or the number of active lines ...
  size_t MaximumLines() const {return m_apLines.size();}
  size_t ActiveLines() const {return m_mapSocket.size();}
  // Get or set the server port number ...
  uint16_t GetServerPort() const {return m_nServerPort;}
  bool SetServerPort (uint16_t nPort);
  // Get or set the IP/interface we're listening on ...
  uint32_t GetServerIP() const {return m_lServerIP;}
  bool SetServerIP (uint32_t lIP);
  string GetServerAddress() const {return FormatIPaddress(GetServerIP(), GetServerPort());}
  bool SetServerAddress (const char *pszAddress);

  // Public CTerminalServer methods ...
public:
  // Start and stop the server thread ...
  bool StartServer (uint16_t nPort=DEFAULT_PORT, uint32_t lIP=0);
  void StopServer();
  // Lock or release the server data ...
  void LockServer();
  void UnlockServer();
  // Disconnect a link ...
  void Disconnect (uint32_t nLine);
  // Call the RECEIVE_CALLBACK (used by CTerminalLine) ...
  void ReceiveCallback (uint32_t nLine, uint8_t ch)
    {assert(m_pReceiveCallback != NULL);  (*m_pReceiveCallback)(m_lCallbackParam,nLine,ch);}
  // Handle socket events from Windows ...
  //   BTW, these really aren't public but since they are called from the
  // WNDPROC, and that's not a member, they have to be declared public...
  void SocketAccept();
  void SocketRead (intptr_t skt);

  // Private CTerminalServer methods ...
private:
  // The TELNET server task thread ...
  static void __cdecl ServerWindowThread (void *lparam);
  // Create or destroy the socket we're listening to ....
  bool CreateServerSocket();
  void DeleteServerSocket();
  // Create or destroy the invisible server window ...
  bool CreateServerWindow();
  void DeleteServerWindow();

  // Local CTerminalServer members ...
private:
  uint32_t        m_nMaxLines;    // maximum simultaneous connections
  uint16_t        m_nServerPort;  // TCP port to listen on
  uint32_t        m_lServerIP;    // IP address to listen on
  TERMINAL_ARRAY  m_apLines;      // pointers to each CTerminalLine object
  SOCKET_HASH     m_mapSocket;    // map from socket to CTerminalLine
  //   This is kind of a kludge - m_hSocket really should be declared as a
  // SOCKET, however using that type would require us to include winsock2.h
  // (and by implication, windows.h) everywhere.  Instead, we cheat a bit by
  // knowing (or pretending to know) the underlying type for SOCKET. Likewise
  // there's no reason why we need a pointer to WSADATA rather than the
  // structure itself, except that the latter would require including all of
  // winsock.h/windows.h again.  
  intptr_t      m_hServerSocket;  // SOCKET structure for our server
  void         *m_hServerWindow;  // HWND for the server's invisible window
  WSAData      *m_pWSAdata;       // winsock library data
  intptr_t      m_hServerThread;  // server thread that services connections
  unsigned long m_idServerThread; // ditto ...
  CMutex       *m_pLock;          // CRITICAL_SECTION lock for server data
  //   Callback routine pointers.  Note that any or all of these are allowed
  // to be NULL except for the RECEIVE_CALLBACK.  After all, if you're not
  // going to handle received data, what's the point??
  CONNECT_CALLBACK    *m_pConnectCallback;    // connection request received
  DISCONNECT_CALLBACK *m_pDisconnectCallback; // existing line disconnected
  RECEIVE_CALLBACK    *m_pReceiveCallback;    // data received from remote NVT
  intptr_t             m_lCallbackParam;      // generic callback parameter

  // Static data ...
private:
  static CTerminalServer *m_pServer;// the one and only CTerminalServer instance
};
