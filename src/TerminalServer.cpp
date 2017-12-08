//++
// TerminalServer.cpp -> TELNET Terminal Server methods
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
//   This class, plus the CTerminalLine class, implements a simple but complete
// TELNET server. It may be used in conjunction with a terminal multiplexer
// emulation to implement Internet connectivity even for hosts that are too old
// to be "TCP/IP aware". This CTerminalServer class is concerned mainly with
// handling the TCP/IP connections and sending and receiving raw data.  It
// supports an arbitrary number of connections (called "lines" here) and uses
// an asynchronous event programming model.
//
//   The host machine multipexer emulation should create a CTerminalServer
// object and then call the StartServer() method to start listening for TELNET
// connection requests.  When we're done with TELNET, call the StopServer()
// method and then destroy this object.  Individual connections are identified
// by a small integer index called the "line number" which ranges from zero to
// m_nMaxLines-1.  This line number usually corresponds to a TTY line on the
// associated terminal multiplexer.  One CTerminalLine object is associated
// with each connection/line, and this class handles creating and destroying
// those objects automatically as TELNET sessions connect and disconnect.
//
// CALLBACKS:
//   The CTerminalServer object passes asynchronous events back to the host
// terminal multiplexer emulation via three callbacks -
//
//    CONNECT_CALLBACK - this method is called when the TELNET server receives
// a new connection request, and it allows the host multiplexer software to
// decide whether the connection should be accept or rejected.  The host may
// also want to do certain housekeeping tasks, such as asserting DATA TERMINAL
// ready on the corresponding terminal line.
//
//   DISCONNECT_CALLBACK - this method is called when an existing TELNET link
// is disconnected.  It allows the host multiplexer software to do any required
// housekeeping, such as clearing DATA TERMINAL READY on the associated terminal
// line.
//
//   RECEIVE_DATA - this is the primary callback and is invoked any time our
// TELNET thread receives terminal data from the remote end.  Note that all
// terminal data is ASCII and is limited to 7 bits - this server currently
// doesn't implement the 8 bit or binary protocol extensions.
//
//   WARNING!!!  Remember that the TELNET server runs on its own thread, and
// these callback routines are invoked on that thread.  The caller's software
// MUST BE THREAD SAFE, at least as far as these three callbacks go.  Also,
// callback routines should avoid blocking or doing anything potentially time
// consuming as that would intefere with communications on any other TELNET
// connections.
//
// ARCHITECTURE:
//   This server uses a single threaded, asynchronous I/O, programming model.
// All network I/O for all terminals is handled by a single thread created by
// the CTerminalServer object, and this thread traps all WM_SOCKET messages.
// Unfortunately that requires us to create a WNDPROC windows message handler
// procedure, and that requires us to create a dummy window class and even a
// dummy window.  The latter is an invisible window, but so far as I know
// there's no way to get out of creating one.  That part gets a bit messy, but
// the asynchronous I/O model is the most scalable for large number of remote
// sessions and has the fewest problems with resource locking.
//
//   The WNDPROC for the invisible window then handles FD_CONNECT, FD_READ and
// FD_CLOSE messages.  The CONNECT message creates a new CTerminalLine object,
// invokes the CONNECT_CALLBACK method, and connects object to the remote NVT.
// FD_READ reads from the remote NVT and passes the data thru the CTerminalLine
// object to the RECEIVE_DATA callback routine. FD_CLOSE invokes the DISCONNECT
// _CALLBACK and then destroys the CTerminalLine object.
//
//   Notice that FD_WRITE is not used.  When the host terminal multiplexer
// wants to send data to the NVT, it calls the CTerminalLine::Send() method.
// This invokes the winsock send() method directly, however since the socket
// is in asynchronous mode this call never blocks.  If the remote NVT is not
// able to accept data right now then send() returns failure, and we return
// that failure back to the multiplexer emulator.  It's up to that software
// to figure out how to handle it - usually it simply signals the host that
// the multiplexer is still busy transmitting the last character.
//
//   Right now no attempt is made to buffer data, either for sending or for
// receiving, but that could certainly be added in the future.  Implementing
// some kind of circular buffer in the CTerminalLine object is the obvious
// way to go.  If this is done for sending to the remote NVT, then we would
// want to start handling the FD_WRITE events and use them to unbuffer data
// and transmit it.
//
// NOTES:
//   Note that CTerminalServer is a "modified" Singleton class - only one 
// instance per application can be created, however the constructor must be
// explicitly called, once, to create that instance.  Subsequent calls to the
// constructor will cause assertion failures.  A pointer to the original
// instance may be retrieved at any time by calling GetServer().  BTW, the
// only reason for this singleton restriction is because of the WindowProc,
// ServerWindowProcedure(), that we need for the asynchronous I/O model.
// This window procedure needs a way to find the associated CTerminalServer
// object, and the simplest way to do that is with a singleton object.  If
// you care to implement a more sophisticated system, then it would be easy
// to have more than one TELNET server per application.
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
//
// REVISION HISTORY:
// 17-MAY-16  RLA   New file.
// 13-JUL-16  RLA   Add lParam to all callback routines.
// 28-FEB-17  RLA   Make 64 bit clean.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>               // exit(), system(), etc ...
#include <stdint.h>	          // uint8_t, uint32_t, etc ...
#include <assert.h>               // assert() (what else??)
#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <process.h>              // needed for _beginthread(), et al ...
#include <windows.h>              // WaitForSingleObject(), et al ...
#include <winsock2.h>             // socket interface declarations ...
#include <ws2tcpip.h>             // newer TCP/IP functions for winsock v2
#pragma comment(lib,"Ws2_32.lib") // force linking with ws2_32 ...
#include "Mutex.hpp"            // CMutex critical section lock
#include "UPELIB.hpp"             // global declarations for this library
#include "LogFile.hpp"            // message logging facility
#include "TerminalLine.hpp"       // CTerminalLine declarations
#include "TerminalServer.hpp"     // declarations for this module

// Initialize the pointer to the one and only CTerminalServer instance ...
CTerminalServer *CTerminalServer::m_pServer = NULL;


CTerminalServer::CTerminalServer (RECEIVE_CALLBACK *pReceive, CONNECT_CALLBACK *pConnect, DISCONNECT_CALLBACK *pDisconnect, intptr_t lCallbackParam, uint32_t nMaxLines)
{
  //++
  //   The constructor initializes everything and establishes the maximum
  // number of simultaneous connections allowed, and TCP port we would use for
  // listening.  It DOES NOT start the TELNET server however - you must call
  // the StartServer() method explicitly!
  //
  //   Lastly, note that the CTerminalServer is a singleton object - only one
  // instance ever exists and we keep a pointer to that one in a static member,
  // m_pServer.  Even though CTerminalServer is a singleton, this constructor
  // must still be called, exactly once, to create that instance.  Subsequent
  // calls to this constructor will cause assert() failures...
  //--
  assert(m_pServer == NULL);
  assert((nMaxLines > 0)  &&  (nMaxLines <= DEFAULT_LINES));
  assert(pReceive != NULL);
  m_pServer = this;  m_nMaxLines = nMaxLines;  m_pReceiveCallback = pReceive;
  m_pConnectCallback = pConnect;  m_pDisconnectCallback = pDisconnect;
  m_lCallbackParam = lCallbackParam;
  //   Note that the server port can be set via the StartServer() call.
  // Here we just set it to the default ...
  m_nServerPort = DEFAULT_PORT;  m_lServerIP = INADDR_ANY;
  m_hServerWindow = NULL;  m_hServerThread = NULL;  m_idServerThread = 0;
  m_pWSAdata = NULL;  m_hServerSocket = INVALID_SOCKET;
  m_pLock = DBGNEW CMutex;
  m_apLines.resize(m_nMaxLines);  m_mapSocket.clear();
  for (uint32_t i = 0;  i < m_nMaxLines;  ++i)  m_apLines[i] = NULL;
}


CTerminalServer::~CTerminalServer()
{
  //++
  // Delete all child Line objects, stop the server thread, and quit ...
  //--
  assert(m_pServer == this);
  if (IsServerRunning()) StopServer();
  delete m_pWSAdata;  delete m_pLock;
  //   Reset the pointer to this Singleton object. In theory this would allow
  // another CTerminalServer instance to be created, but that's not likely to
  // be useful.
  m_pServer = NULL;
}


void CTerminalServer::LockServer()
{
  //++
  //   Acquire our critical section lock for the server thread ...
  // Note that the only reason this is here, rather than being a one liner
  // in the header file, is so that we don't have to include StlLock.h in 
  // every file that uses this class.
  //--
  assert(m_pLock != NULL);
  m_pLock->Enter();
}


void CTerminalServer::UnlockServer()
{
  //++
  // Release the server thread's critical section lock ...
  //--
  assert(m_pLock != NULL);
  m_pLock->Leave();
}


void CTerminalServer::Disconnect (uint32_t nLine)
{
  //++
  //   This method will completely disconnet a particular TELNET link.  It
  // first onvokes the DISCONNECT_CALLBACK via the CTerminalLine object,
  // then it deletes the CTerminalLine object, closes the socket and, finally,
  // removes the line from this collection.
  //--
  CTerminalLine *pLine = Line(nLine);
  assert(pLine != NULL);

  // First, invoke the DISCONNECT_CALLBACK ...
  if (m_pDisconnectCallback != NULL) (*m_pDisconnectCallback)(m_lCallbackParam, nLine);

  // Delete the CTerminalLine object ...
  SOCKET sktClient = pLine->GetSocket();
  delete pLine;

  // Close the socket ...
  closesocket(sktClient);

  // Remove the line from both m_apLines and m_mapSockets ...
  m_apLines[nLine] = NULL;
  m_mapSocket.erase(sktClient);
  UnlockServer();
  LOGF(TRACE, "TELNET line %d disconnected", nLine);
}


void CTerminalServer::SocketAccept()
{
  //++
  //   This routine is called when we recceive a connection request.  It will
  // do an accept() on our master socket (which is guaranteed not to block at
  // this point!), get the new socket, and then create new CTerminalLine object
  // to connect it to.  At that point the CONNECT_CALLBACK is invoked and the
  // host emulator has the option of refusing the connection.  If we're out of
  // terminal lines, or if the CONNECT_CALLBACK refuses the connection, then
  // new connection is closed and the CTerminalLine is deleted immediately.
  // But otherwise, the new line is all ready to receive data.
  //--
  uint32_t nLine = m_nMaxLines;  CTerminalLine *pLine = NULL;

  //   First, accept the incoming connection.  This should never block and it
  // should never fail, but if it does fail then just give up...
  SOCKET sktClient = accept(m_hServerSocket, NULL, NULL);
  if (sktClient == INVALID_SOCKET) {
    LOGF(WARNING, "TELNET accept failed (%d)", WSAGetLastError());  return;
  }

  // From this point on, lock the server database so there are no races ...
  LockServer();

  // Set the client socket to async mode too ...
  int nResult = WSAAsyncSelect(sktClient, (HWND) m_hServerWindow, WM_SOCKET, (FD_CLOSE|FD_READ));
  if (nResult != 0) {
    LOGF(WARNING, "TELNET client async select failed (%d)", WSAGetLastError());  goto failed;
  }

  // Find a free line number.  If there are no free lines, just give up ...
  for (nLine = 0;  nLine < m_nMaxLines;  ++nLine)
    if (Line(nLine) == NULL) break;
  if (nLine >= m_nMaxLines) {
    LOGF(WARNING, "TELNET accept failed - no more lines");  goto failed;
  }

  // Create a new CTerminalLine object and attach it to the socket ...
  pLine = DBGNEW CTerminalLine(nLine, sktClient, *this);

  // Call the CONNECT_CALLBACK and give it a chance to refuse ...
  //   There's a bit of kludge here because we have to put the new line into
  // the m_apLines array BEFORE we invoke the callback.  That's because the
  // callback might want to use our Line() method to find the CTerminalLine
  // pointer, and we want to make sure he can if he needs to.
  m_apLines[nLine] = pLine;
  if ((m_pConnectCallback != NULL) && !(*m_pConnectCallback)(m_lCallbackParam, nLine)) {
    LOGF(WARNING, "TELNET accept failed - connect callback refused");  goto failed;
  }

  // Success!  Update both the line array and socket map, and we're done ...
  m_apLines[nLine] = pLine;  m_mapSocket[sktClient] = nLine;
  UnlockServer();
  LOGF(TRACE, "TELNET connection to line %d accepted from %s", nLine, pLine->GetClientAddress().c_str());
  return;

  // Here if the connection fails for any reason ...
failed:
  if (nLine < m_nMaxLines) m_apLines[nLine] = NULL;
  delete pLine;  closesocket(sktClient);  UnlockServer();
}


void CTerminalServer::SocketRead (intptr_t skt)
{
  //++
  //   This method is called when Windows tells us that a socket has data
  // ready for reading - what this really means is that a recv() call will
  // not block.  We read as many bytes as we can from the connection and pass
  // them along to the CTerminalLine object, which will either interpret them
  // as TELNET escape sequences or pass them along in turn to the multiplexer
  // emulation.  Note that the CTerminalLine::Receive() method only handles
  // one character at a time - normally this isn't a problem, since TELNET
  // is character oriented anyway, but if we happen to receive multiple
  // characters here then we'll divide them up and call Receive() many times.
  //--
  uint8_t szBuffer[MAXRECV];
  uint32_t nLine = SocketToLine(skt);
  CTerminalLine *pLine = Line(nLine);
  assert(pLine != NULL);

  int cbBuffer = recv(skt, (char *) &szBuffer, MAXRECV, 0);
  if (cbBuffer == SOCKET_ERROR) {
    LOGF(WARNING, "TELNET error (%d) reading socket for line %d", WSAGetLastError(), nLine);
  } else if (cbBuffer == 0) {
    LOGF(WARNING, "TELNET unexpeccted disconnect for line %d", nLine);
    Disconnect(nLine);
  } else {
    for (int32_t i = 0;  i < cbBuffer;  ++i)  pLine->Receive(szBuffer[i]);
  }
}


static LRESULT CALLBACK ServerWindowProcedure (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  //++
  //   This is the window procedure for the server's invisible window.  All it
  // does is watch for WM_SOCKET messages and pass those along to SocketEvent()
  // - everything else is unimportant.
  //--
  uint16_t wEvent, wError;   SOCKET sktClient;  CTerminalServer *pServer;
  switch (msg) {

    // Handle socket events - that's why we're here!!
    case WM_SOCKET:
      wEvent    = WSAGETSELECTEVENT(lParam);
      wError    = WSAGETSELECTERROR(lParam);
      sktClient = (SOCKET) wParam;
      pServer   =  CTerminalServer::GetServer();
      if (wError != 0) {
        LOGF(WARNING, "TELNET WM_SOCKET error (%d) for event %d", wError, wEvent);
        return FALSE;
      }
      switch (wEvent) {
        case FD_ACCEPT:  pServer->SocketAccept();                                 break;
        case FD_READ:    pServer->SocketRead(sktClient);                          break;
        case FD_CLOSE:   pServer->Disconnect(pServer->SocketToLine(sktClient));   break;
        default:
          LOGF(WARNING, "TELNET unexpected WM_SOCKET event %d", wEvent);
      }
      return TRUE;

    // Handle other miscellaneous Windows events ...
    case WM_DESTROY:
      PostQuitMessage(0);  return TRUE;
    default:
      return DefWindowProc(hwnd, msg, wParam, lParam);
  }
}


bool CTerminalServer::SetServerPort  (uint16_t nPort)
{
  //++
  //   This method sets the port number used by the TELNET server to listen
  // for incoming connections.  It can only be used before the server is
  // started - if the server is already running, then it returns FALSE and
  // does nothing.
  //--
  if (IsServerRunning()) return false;
  m_nServerPort = nPort;
  return true;
}


bool CTerminalServer::SetServerIP (uint32_t lIP)
{
  //++
  //   And this method sets the interface used by the TELNET server to listen
  // for incoming connections.  The interface is specified by its assigned IP
  // address and not the name (e.g. 172.16.1.1 and not eth0).  The default
  // interface is INADDR_ANY, which means to listen on all interfaces.
  //
  //   Like SetServerPort(), this can only be called before the server is
  // started.
  //--
  if (IsServerRunning()) return false;
  m_lServerIP = lIP;
  return true;
}


bool CTerminalServer::SetServerAddress (const char *pszAddress)
{
  //++
  //   Set the current server IP and/or port number, using any format string
  // accepted by ParseAddress().  This can only be called BEFORE the server
  // is actually started!
  //--
  if (IsServerRunning()) return false;
  return ParseIPaddress(pszAddress, m_lServerIP, m_nServerPort);
}


bool CTerminalServer::CreateServerSocket()
{
  //++
  //   This method will initialize the winsock library and the create the
  // socket we use to listen for TELNET connections.  This socket is always
  // set to async mode, so all future socket related events will generate
  // WM_SOCKET messages to the WNDPROC.
  //--

  // Initialize WinSock ...
  m_pWSAdata = DBGNEW(WSAData);
  if (WSAStartup(MAKEWORD(2, 2), m_pWSAdata) != 0) {
    LOGS(ERROR, "TELNET WSA Initialization failed!");  return false;
  }

  // Create a master TCP/IP socket for listening ...
  m_hServerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (m_hServerSocket == INVALID_SOCKET) {
    LOGF(ERROR, "TELNET server socket creation failed (%d)", WSAGetLastError());  return false;
  }

  //   Set the SO_EXCLUSIVEADDRUSE option for our socket. This prevents anybody
  // else, including us (!), from binding to the same port and socket.
  int iOptval = 1;
  if (setsockopt(m_hServerSocket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char *) &iOptval, sizeof(iOptval)) == SOCKET_ERROR) {
    LOGF(ERROR, "TELNET server set socket options failed (%d) !", WSAGetLastError());  return false;
  }

  // And bind our server port to that socket ...
  SOCKADDR_IN sin;
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = htonl(m_lServerIP);
  sin.sin_port = htons(m_nServerPort);
  if (bind(m_hServerSocket, (SOCKADDR *) (&sin), sizeof(sin)) == SOCKET_ERROR) {
    LOGF(ERROR, "TELNET server bind socket failed (%d) !", WSAGetLastError());  return false;
  }

  // Set the socket to async mode ...
  int nResult = WSAAsyncSelect(m_hServerSocket, (HWND) m_hServerWindow, WM_SOCKET, (FD_CLOSE|FD_ACCEPT|FD_READ));
  if (nResult != 0) {
    LOGF(ERROR, "TELNET server async select failed (%d)", WSAGetLastError());  return false;
  }

  // Start listening for connections on the socket and we're done ...
  if (listen(m_hServerSocket, SOMAXCONN) == SOCKET_ERROR) {
    LOGF(ERROR, "TELNET server listen failed (%d)", WSAGetLastError());  return false;
  }
  LOGS(TRACE, "TELNET server listening on " << GetServerAddress());
  return true;
}


bool CTerminalServer::CreateServerWindow()
{
  //++
  //   This method will create the invisible window used by the server thread.
  // This window exists only so that we can have a WNDPROC associated with it,
  // and that's there only so we can receive the WM_SOCKET messages!
  //--
  WNDCLASS wc;

  // Initialize and register the invisible window class ...
  memset(&wc, 0, sizeof(wc));
  wc.lpfnWndProc = (WNDPROC) ServerWindowProcedure;
  wc.hInstance = GetModuleHandle(NULL);
  wc.hIcon = LoadIcon(GetModuleHandle(NULL), SERVER_WINDOW_ICON);
  wc.lpszClassName = SERVER_WINDOW_CLASS;
  if ((RegisterClass(&wc) == 0) && (GetLastError() != ERROR_CLASS_ALREADY_EXISTS)) {
    LOGF(ERROR, "TELNET server failed to register window class (%d)", GetLastError());  return false;
  }

  // And then create the invisible window ...
  m_hServerWindow = CreateWindowEx(
    0, SERVER_WINDOW_CLASS, SERVER_WINDOW_TITLE, WS_OVERLAPPEDWINDOW,
    CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
    (HWND) NULL, (HMENU) NULL, GetModuleHandle(NULL), (LPVOID) NULL
    );
  if (m_hServerWindow == NULL) {
    LOGF(ERROR, "TELNET server failed to create window (%d)", GetLastError());  return false;
  }
  return true;
}


void CTerminalServer::DeleteServerSocket()
{
  //++
  //   Delete our socket and stop winsock.  Note that all child CTerminalLine
  // objects should be deleted before this is invoked!!!
  //--
  if (m_hServerSocket == INVALID_SOCKET) return;
  shutdown(m_hServerSocket, SD_BOTH);
  closesocket(m_hServerSocket);
  m_hServerSocket = INVALID_SOCKET;
  WSACleanup();
  delete m_pWSAdata;  m_pWSAdata = NULL;
}


void CTerminalServer::DeleteServerWindow()
{
  //++
  // Destroy the invisible window used by the server ...
  //--
  DestroyWindow((HWND) m_hServerWindow);
  UnregisterClass(SERVER_WINDOW_CLASS, GetModuleHandle(NULL));
  m_hServerWindow = NULL;
}


void __cdecl CTerminalServer::ServerWindowThread (void *lparam)
{
  //++
  //   And this is the background thread for the invisible server window.  It
  // initializes Winsock, binds a port for TELNET connections, and listens on
  // that port.  It also creates the invisible server window class and the
  // invisible server window, and finally runs the message loop for the window.
  //
  //   When the rest of the code wants us to quit, StopServer() posts a
  // WM_QUIT messsage to our thread.  This causes GetMessage() to fail and
  // exits the message loop.  After that, we clean up the Winsock connection
  // and the window class, and then we're done!  Simple, no??
  //
  //   Notice that this method is a static C procedure because that's what a
  // thread requires.  The lParam parameter we're passed is actually a pointer
  // to the CTerminalServer object that owns us.
  //
  //   One last thing - we probably should lock the server data while we're
  // doing the creation, but we don't bother because since the server hasn't
  // been started yet obviously no other thread is messing with our data.
  // Once the server is up and running, however, being thread safe is something
  // to consider.  That's up to the code in the WNDPROC to worry about.
  //--
  MSG msg;
  assert(lparam != NULL);
  CTerminalServer *pServer = static_cast<CTerminalServer *> (lparam);
  if (pServer->CreateServerWindow()) {
    if (pServer->CreateServerSocket()) {
      while (GetMessage(&msg, (HWND) NULL, 0, 0)) {
        TranslateMessage(&msg);  DispatchMessage(&msg);
      }
      pServer->DeleteServerSocket();
    }
    pServer->DeleteServerWindow();
  }
}


bool CTerminalServer::StartServer (uint16_t nPort, uint32_t lIP)
{
  //++
  //   Start the TELNET server thread... This call, thru the ServerWindowThread
  // procedure, will initialize winsock, create the initial TELNET socket, and
  // starts listening for incoming connecctions.
  //--
  if (IsServerRunning()) return true;
  assert(nPort > 0);
  m_nServerPort = nPort;  m_lServerIP = lIP;
  LOGS(DEBUG, "starting TELNET server thread");
  m_hServerThread = _beginthread(&ServerWindowThread, 0, (void *) this);
  m_idServerThread = GetThreadId((HANDLE) m_hServerThread);
  if ((m_hServerThread == NULL) || (m_idServerThread == 0)) {
    LOGS(ERROR, "unable to create TELNET server thread");  return false;
  }
  return true;
}


void CTerminalServer::StopServer()
{
  //++
  //   This method stops the server thread by posting a WM_QUIT message to the
  // invisible server window.  This causes the WNDPROC to exit as usual, and
  // then the ServerWindowThread will clean up the winsock connections.
  //--
  if (!IsServerRunning()) return;
  for (uint32_t i = 0; i < m_nMaxLines; ++i)
    if (IsLineConnected(i)) Disconnect(i);
  LOGS(DEBUG, "waiting for TELNET server thread to terminate");
  PostThreadMessage(m_idServerThread, WM_QUIT, 0, 0);
  WaitForSingleObject((HANDLE) m_hServerThread, INFINITE);
  m_hServerThread = NULL;  m_idServerThread = 0;
}
