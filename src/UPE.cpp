//++
// upe.cpp -> FPGA (UPE) interface methods for UPE library 
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
//   A CUPE object is an interface to one FPGA board running the UPE Xilinx
// bitstream.  The actual physical interface between the two is via the PLX
// 9054 PCI to localbus bridge chip, and you'll notice that this module is
// mostly based on the PLX API library.
//
//   Our interface to the the FPGA is actually fairly simple  - there's only a
// shared memory window which is used for all communications and an interrupt.
// That's it - no I/O ports are used.  The memory window is application (e.g.
// MBS, CPS, SDE, etc) specific but generally contains a command queue, a
// sector/record buffer, and a few miscellaneous control registers. In this
// code we treat the shared memory as a void * and it's up to you to write a
// wrapper class for CUPE that implements the specific memory map and any
// functions that go along with it.
//
//   A PC may have more than one FPGA/UPE card installed and this application
// may talk to more than one of them, but it doesn't necessarily talk to all of
// them.  Some of the FPGA boards may be used by other applications.  The main
// program will create a single instance of the CUPEs object to encapsulate the
// entire collection of FPGA boards.  The Enumerate() method will discover all
// FPGA/UPE boards installed and populate the collection with CUPE objects for
// each one, however this does NOT ACTUALLY CONNECT TO THE UPE NOR DOES IT 
// DISTURB THE UPE STATE IN ANY WAY.  This allows us to discover UPEs which may
// (or may not) be in use by other programs.  This process doesn't take over
// control of the FPGA/UPE board until the Open() method is explicitly called.
//
//   It's also possible to run this application on a PC which has no FPGA/UPE
// boards at all.  In that case the CUPEs collection would be empty and so we
// create a special, "offline" CUPE instance and put it in CUPEs.  An offline
// UPE has no physical FPGA/UPE board associated with it and by defintion can't
// do anything useful, however it is REALLY handy for testing!
//
// Bob Armstrong <bob@jfcl.com>   [20-MAY-2015]
//
// REVISION HISTORY:
// 20-May-15  RLA   Adapted from MBS.
//  3-Jun-15  RLA   Make pBuffer to BarSpaceWrite a const
// 22-Sep-15  RLA   Add FPGA configuration stuff
// 28-FEB-17  RLA   Update for PLXLIB v7.24 and x64
//  2-JUN-17  RLA   Linux port.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include <string.h>             // strcpy(), memset(), strerror(), etc ...
#include <PlxApi.h>             // PLX 9054 PCI interface API declarations
#include "UPELIB.hpp"           // global declarations for this library
#include "SafeCRT.h"		// replacements for Microsoft "safe" CRT functions
#include "LogFile.hpp"          // message logging facility
#include "BitStream.hpp"        // Xilinx FPGA bit stream object
#include "MESA.h"               // MESA 5I22 card definitions
#include "UPE.hpp"              // and declarations for this module
#ifdef _WIN32
#pragma comment(lib, "PlxApi.lib")   // force the PLX library to be loaded
#endif


// Private members ...
//   This gets a bit ugly, but we need private copies of these PLX API data
// structures for each FPGA/UPE instance.  We could include these as local
// members directly in the CUPE class, but that'd mean that every source file
// that includes upe.h would also need to include PlxApi.h.  We don't want to
// do that, so instead we define a private structure here to contain all the
// instance specific PLX library data. We only need to store a generic pointer
// to the structure in the class definition, and the actual memory can be
// allocated in the Open() method and released in Close(). It's a kludge, but
// it gets the job done...
struct _UPE_PLXLIB_DATA {
  PLX_DEVICE_KEY    plxKey;       // PLX library key for this device
  PLX_DEVICE_OBJECT plxDevice;    // PCI device handle
  PLX_INTERRUPT     plxInterrupt; // localbus interrupt
  PLX_NOTIFY_OBJECT plxNotify;    // PCI interrupt semaphore
  uint16_t          wIObase;      // PCI I/O base port address
  uint16_t          wCSRport;     // PCI9054 CNTRL (control/status) port
};


///////////////////////////////////////////////////////////////////////////////
/////////////   C U P E s   C O L L E C T I O N   M E T H O D S   /////////////
///////////////////////////////////////////////////////////////////////////////


CUPE *NewUPE (PLX_DEVICE_KEY *pplxKey)
{
  //++
  //   Create a new generic CUPE object.  AFAIK this is NEVER used - we always
  // create an application specific UPE instance (e.g. a CDECUPE, CCDCUPE or a
  // CSDEUPE) instead.
  //--
  return (CUPE *) DBGNEW CUPE(pplxKey);
}


CUPEs::~CUPEs()
{
  //++
  // The destructor deletes any and all CUPE objects in this collection ...
  //--
  for (iterator it = begin();  it != end();  ++it)  {
    CUPE *pUPE = *it;
    assert(pUPE != NULL);
    //   Note that we MUST explicity close the UPE first, before destroying
    // it, even though the CUPE destructor also closes the UPE.  See the note
    // before the Close() routine for more details.
    if (pUPE->IsOpen())  pUPE->Close();
    delete pUPE;
  }
}


void CUPEs::Enumerate()
{
  //++
  //   This routine will enumerate all the PCI FPGA/UPE boards in this PC and
  // populate this collection with one CUPE object for each one.  This method
  // should be called just once, at startup, and that's enough for anybody.
  //--
  PLX_DEVICE_KEY plxKey;
  for (uint32_t i = 0;  ;  ++i) {
    // (re)Initialize the PLX_DEVICE_KEY structure ...
    memset(&plxKey, PCI_FIELD_IGNORE, sizeof(PLX_DEVICE_KEY));
    plxKey.VendorId = PLX_PCI_VENDOR_ID_PLX;  plxKey.DeviceId = PLX_PCI_DEVICE_ID_PLX;

    // Try to find the next device - if we fail, then there are no more...
    if (PlxPci_DeviceFind(&plxKey, i) != ApiSuccess) return;

    //   The PLX library has a nasty habit of returning the same device more
    // than once.  I think it's a bug, but whatever it is we have to make
    // sure this device isn't already on the list!
    if (Find(plxKey.bus, plxKey.slot) != NULL) continue;

    // This is a good UPE - keep it ...
    LOGS(DEBUG, "FPGA/UPE board found at PCI address " << CUPE::GetBDF(&plxKey)
           << " (bus " << plxKey.bus << " slot " << plxKey.slot << ")");
    Add(&plxKey);
  }
}


CUPE *CUPEs::Find (uint8_t nBus, uint8_t nSlot) const
{
  //++
  //   This routine searches thru the installed FPGA/UPE boards to find one
  // which matches the PCI bus and slot number specified.  If a match is found
  // the corresponding CUPE object will be returned; if no match can be found
  // the NULL is returned instead.  Note that this assumes you've called the
  // Enumerate() method first!
  //
  //   Note that technically there are four parts to a physical PCI address -
  // not just the bus and slot, but also the domain and function.  We don't
  // support PCI domains (I've never seen them implemented) and our FPGA
  // board only has function 0, so those two are currently ignored.
  //--
  for (const_iterator it = begin();  it != end();  ++it)
    if (   ((*it)->GetPCIBus()  == nBus)
        && ((*it)->GetPCISlot() == nSlot)) return *it;
  return NULL;
}


bool CUPEs::Open (uint8_t nBus, uint8_t nSlot, CUPE *&pUPE)
{
  //++
  //   This method will search for the UPE instance matching the PCI address
  // given, open it, and return address of the CUPE object.  If the PCI address
  // is zero, then an offline UPE instance is created, opened, and returned.
  //
  //   WARNING - there is a templated version of this function, ReOpen(), in
  // the UPE.hpp header file.  If you make any changes to this, you might want
  // to review that one too!
  //--
  if ((nSlot == 0)  &&  (nBus == 0)) {
    // Create and open an offline UPE ...
    pUPE = m_NewUPE((PLX_DEVICE_KEY *) NULL);
    Add(pUPE);
    return pUPE->OpenOffline();
  } else {
    // Open a real, physical, UPE ...
    if ((pUPE = Find(nBus, nSlot)) == NULL) {
      LOGS(ERROR, "no UPE found at PCI address " << CUPE::GetBDF(nBus, nSlot));  return false;
    }
    if (pUPE->IsOpen()) {
      LOGS(ERROR, "UPE " << *pUPE << " is in use");  return false;
    }
    return pUPE->Open();
  }
}


bool CUPEs::Replace (CUPE *pOld, CUPE *pNew)
{
  //++
  //   This method will find the UPE instance referenced by pOld, close it,
  // delete it (!!), and then replace its entry in m_vecUPEs with pNew.  TRUE
  // is returned if we are successful, and FALSE if pOld cannot be found in
  // the current UPE collection.
  //
  //   Why do this??  Well, it's messy...  Most of the applications use a
  // single application specific UPE object for everything (e.g MBS has
  // CDECUPE, CPS has CCDCUPE, etc) but SDE is different. SDE uses a unique
  // FPGA bitstream for every peripheral (e.g. disk has one bitstream, card
  // reader has another, printer has a third, etc).  The memory map of each
  // bitstream is different, the functions of the bitstream are different, and
  // everything is different.  The only sensible way to handle that is to have
  // a different SDE specific CUPE derived object for each peripheral.  So, SDE
  // has not one CUPE derived object but several, CDiskUPE, CPrinterUPE, etc.  
  //
  //   For SDE the CUPEs::Enumerate() method just creates a generic CSDEUPE
  // object (yes, SDE has one of those too!) for every FPGA board found.  It
  // CAN'T create a peripheral specific object, not even if it tried, because
  // we won't know what peripheral a specific FPGA board is used for until
  // after the bitstream has been loaded.  
  //
  //    We could just cast the generic CSDEUPE object into something specific,
  // like a CDiskUPE, but that's dangerous on several levels.  Bad idea - don't
  // go there!  The only safe thing is, once we know what a specific FPGA board
  // is to be used for, to delete the old generic CSDEUPE object and replace it
  // with a new, peripheral specific object, that refers to the same PCI slot.
  // And that's exactly what SDE does.  It's ugly, but it's safe and it works.
  //--
  assert((pOld != NULL) && (pNew != NULL) && (pOld != pNew));
  for (uint32_t i = 0;  i < m_vecUPEs.size();  ++i) {
    if (m_vecUPEs.at(i) == pOld) {
      m_vecUPEs.at(i) = pNew;
      if (pOld->IsOpen())  pOld->Close();
      delete pOld;
      return true;
    }
  }
  return false;
}


///////////////////////////////////////////////////////////////////////////////
//////////////////////////  C U P E   M E T H O D S   /////////////////////////
///////////////////////////////////////////////////////////////////////////////


/*static*/ string CUPE::GetSDKVersion()
{
  //++
  // Return the version number of the PLX SDK library ...
  //--
  uint8_t bMajor, bMinor, bRevision;  char szBuffer[10];
  PlxPci_ApiVersion(&bMajor, &bMinor, &bRevision);
  sprintf_s(szBuffer, sizeof(szBuffer), "%d.%d%d", bMajor, bMinor, bRevision);
  return string(szBuffer);
}


CUPE::CUPE (const PLX_DEVICE_KEY *pplxKey)
{
  //++
  //   The constructor simply initializes the variables and copies the PLX
  // library key for the corresponding PLX PCI device.  It DOES NOT attempt
  // to connect to the PLX PCI chip - you must call Open() explicitly to do
  // that.  There are two reasons for this two step process - one is that we
  // might not want to open this particular PLX device (it might be used by 
  // another instance, for example).  Another reason is simply that there
  // is no good way for a constructor to fail and report errors if anything
  // goes wrong while opening.
  //
  //   Note that the pplxKey pointer MAY BE NULL, and that condition opens
  // an "offline" UPE instance.  
  //--
  m_pWindow = NULL;  m_pplxData = NULL;
  if (pplxKey == NULL) return;
  
  // Allocate the PLXLIB data, initialize it, and copy the key...
  m_pplxData = (UPE_PLXLIB_DATA *) calloc(1, sizeof(UPE_PLXLIB_DATA));
  if (m_pplxData == NULL) return;
  memcpy(&m_pplxData->plxKey, pplxKey, sizeof(PLX_DEVICE_KEY));
}


CUPE::~CUPE()
{
  //++
  // The destructor just closes the PLX interface - there isn't much else to do.
  //--
  if (IsOpen()) Close();
  if (m_pplxData != NULL) free(m_pplxData);
  m_pWindow = NULL;  m_pplxData = NULL;
}


uint8_t CUPE::GetPCIBus() const
{
  //++
  //   Return the PCI bus number associated with this FPGA/UPE instance.
  // This method can't simply be inline because we don't want to expose the
  // details of the UPE_PLXLIB_DATA structure...
  //--
  return IsOffline() ? 0 : m_pplxData->plxKey.bus;
}


uint8_t CUPE::GetPCISlot() const
{
  //++
  // Return the PCI slot number associated with this FPGA/UPE instance.
  //--
  return IsOffline() ? 0 : m_pplxData->plxKey.slot;
}


string CUPE::GetBDF (uint8_t nBus, uint8_t nSlot, uint8_t nFunction)
{
  //++
  // Return this FPGA/UPE's address in the standard PCI BDF notation ...
  //--
  char szBuffer[10];
  sprintf_s(szBuffer, sizeof(szBuffer), "%02X:%02X.%1X", nBus, nSlot, nFunction);
  return string(szBuffer);
}


string CUPE::GetBDF (PLX_DEVICE_KEY *pplxKey)
{
  //++
  // Same as before, but extract the PCI address from the PLX_DEVICE_KEY ...
  //--
  assert(pplxKey != NULL);
  return GetBDF(pplxKey->bus, pplxKey->slot, pplxKey->function);
}


uint16_t CUPE::GetPLXChip() const
{
  //++
  // Return the type (e.g. 9054) of the PLX chip on this FPGA board ...
  //--
  return IsOffline() ? 0 : m_pplxData->plxKey.PlxChip;
}


uint8_t CUPE::GetPLXRevision() const
{
  //++
  // Return the version (e.g. AC) of the PLX chip on this FPGA board ...
  //--
  return IsOffline() ? 0 : m_pplxData->plxKey.PlxRevision;
}


const PLX_DEVICE_KEY *CUPE::GetPLXDeviceKey() const
{
  //++
  // Return a pointer to the PLX_DEVICE_KEY structure for this UPE ...
  //--
  return IsOffline() ? NULL : &(m_pplxData->plxKey);
}


bool CUPE::PLXError (const char *pszMsg, uint32_t nError) const
{
  //++
  //   Print a PLXLIB related error message and then always return false.  Why
  // always return false?  So we can say something like
  //
  //    if (... bad-stuff ...) return Error("fail", errno);
  //--
  if (nError == 0) {
    LOGS(ERROR, "error " << pszMsg << " on " << *this);
  } else {
    LOGS(ERROR, "error (" << nError << ") " << pszMsg << " on " << *this);
    switch (nError) {
      case ApiNullParam:             LOGS(ERROR, "Null Parameter");  break;
      case ApiUnsupportedFunction:   LOGS(ERROR, "Unsupported Function");  break;
      case ApiNoActiveDriver:        LOGS(ERROR, "No Active Driver");  break;
      case ApiConfigAccessFailed:    LOGS(ERROR, "Config Access Failed");  break;
      case ApiInvalidDeviceInfo:     LOGS(ERROR, "Invalid Device Info");  break;
      case ApiInvalidDriverVersion:  LOGS(ERROR, "Invalid Driver Version");  break;
      case ApiInvalidOffset:         LOGS(ERROR, "Invalid Offset");  break;
      case ApiInvalidData:           LOGS(ERROR, "Invalid Data");  break;
      case ApiInvalidSize:           LOGS(ERROR, "Invalid Size");  break;
      case ApiInvalidAddress:        LOGS(ERROR, "Invalid Address");  break;
      case ApiInvalidAccessType:     LOGS(ERROR, "Invalid Access Type");  break;
      case ApiInvalidPowerState:     LOGS(ERROR, "Invalid Power State");  break;
//    case ApiInvalidIndex:          LOGS(ERROR, "Invalid Index");  break;
//    case ApiInvalidIopSpace:       LOGS(ERROR, "Invalid Iop Space");  break;
//    case ApiInvalidHandle:         LOGS(ERROR, "Invalid Handle");  break;
//    case ApiInvalidPciSpace:       LOGS(ERROR, "Invalid PCI Space");  break;
//    case ApiInvalidBusIndex:       LOGS(ERROR, "Invalid Bus Index");  break;
      case ApiInsufficientResources: LOGS(ERROR, "Insufficient Resources");  break;
      case ApiWaitTimeout:           LOGS(ERROR, "Wait Timeout");  break;
      case ApiWaitCanceled:          LOGS(ERROR, "Wait Canceled");  break;
      case ApiPowerDown:             LOGS(ERROR, "Power Down");  break;
      case ApiDeviceInUse:           LOGS(ERROR, "Device In Use");  break;
      case ApiDeviceDisabled:        LOGS(ERROR, "Device Disabled");  break;
      default:                       LOGS(ERROR, "PLXLIB unknown error");  break;
    }
  }
  return false;
}


bool CUPE::Open()
{
  //++
  //   This method opens the connecton to the PLX PCI chip identified by the
  // PLX key specified.  Once the PLX chip is connected, we map the shared
  // memory window and set up notify (aka interrupt) events.  If anything goes
  // wrong during any of this, then an error message is printed and FALSE is
  // returned.
  //
  //   IMPORTANT - there are some cases (e.g. to obtain the VHDL version number
  // or the current UPE owner PID) where we have to map the shared memory
  // region, even though the FPGA may BE IN USE BY ANOTHER PROCESS!  That means
  // it's important that this routine not do anything that might disturb the
  // state of another instance that might be using this same FPGA.In particular
  // there should be no initialization of FPGA registers here.  That kind of
  // thing should belong in the Clear() method instead.
  //--
  PLX_STATUS ret;  PLX_PCI_BAR_PROP plxBarProperties;
  volatile void *pUPE;

  //   If there's no PLXLIB data then that means the constructor wasn't
  // given a PLX_DEVICE_KEY, and that means an "offline" connection.
  if (m_pplxData == NULL) return OpenOffline();

  // Open the selected device ...
  ret = PlxPci_DeviceOpen(&m_pplxData->plxKey, &m_pplxData->plxDevice);
  if (ret != ApiSuccess)
    return PLXError("opening PLX PCI device", ret);
  PlxPci_DeviceReset(&m_pplxData->plxDevice);

  //   Figure the I/O port address of the PLX chip's configuration port.
  // We only need this to configure the MESA card GPIO bits for programming, 
  // but we go ahead and compute it here anyway just in case we want it later.
  uint32_t lCSR = PlxPci_PciRegisterRead(m_pplxData->plxKey.bus,
    m_pplxData->plxKey.slot, m_pplxData->plxKey.function, PLX_REG_LCLCFG, &ret);
  if (ret != ApiSuccess)
    return PLXError("reading configuration registers", ret);
  m_pplxData->wCSRport = (lCSR & ~0x3) + PLX_REG_CSROFFSET;

  //   And compute the base address for all the PLX chip's I/O ports.  Once
  // again, we only use this for FPGA programming but just in case...
  uint32_t lIO = PlxPci_PciRegisterRead(m_pplxData->plxKey.bus,
    m_pplxData->plxKey.slot, m_pplxData->plxKey.function, PLX_REG_IO32, &ret);
  if (ret != ApiSuccess)
    return PLXError("reading configuration registers", ret);
  m_pplxData->wIObase = (lIO & ~0x3) + PLX_REG_DATAOFFSET;
  LOGF(DEBUG, "PCI9054 CNTRL port at 0x%04lX; I/O base at 0x%04lX", m_pplxData->wCSRport, m_pplxData->wIObase);

  // Map the UPE's window into our virtual address space ...
  ret = PlxPci_PciBarMap(&m_pplxData->plxDevice, PLX_BAR_SHAREDMEM, (void **) &pUPE);
  if (ret == ApiSuccess)
    ret = PlxPci_PciBarProperties(&m_pplxData->plxDevice, PLX_BAR_SHAREDMEM, &plxBarProperties);
  if ((ret != ApiSuccess) || (pUPE == NULL))
    return PLXError("mapping window", ret);
  LOGF(DEBUG, "PCI9054 %dK memory window mapped at 0x%p",
    (((uint32_t)plxBarProperties.Size) >> 10), (void *) pUPE);
  if (plxBarProperties.Size != SHARED_MEMORY_SIZE)
    return PLXError("memory window size mismatch");

  //   Success! Note that now (and only now) do we set the class member m_pWindow
  // to the address of the shared window - that's because setting this member
  // makes this object be "open" and we can't do that until we're sure every-
  // thing has worked...
  m_pWindow = pUPE;
  return true;
}


bool CUPE::OpenOffline()
{
  //++
  //   This method will open an "offline" PLX connection.  An offline UPE
  // has no physical FPGA/UPE board associated with it and by defintion can't
  // do anything useful, however it is _really_ handy for testing!  Notice
  // that in this case we allocate a "fake" memory window which is just
  // ordinary memory - that allows things to still work without crashing.
  //--
  m_pWindow = calloc(1, SHARED_MEMORY_SIZE);
  if (m_pWindow == NULL) return false;
  return true;
}


void CUPE::Close()
{
  //++
  //   The Close() method closes the FPGA interface, releases the shared memory
  // window, and free up any associated resources.  Note that there are plenty
  // of errors that can occur during this process, but in this case we simply
  // ignore them and keep going...
  //
  //   Be warned about calling this method from the CUPE destructor - remember
  // that in C++ virtual function calls in the destructor refer to those of
  // the _base_ class, even if the actual object is (or rather, was) of a
  // derived class.  That's especially relevant here to Unlock(), which calls
  // GetOwner(), which is a virtual function.  What all this means that that
  // the Unlock() won't work if this method is called via the destructor.
  // The best way to deal with that is to explictly Close() the UPE first,
  // before destroying it!
  //--
  PLX_STATUS ret;
  if (!IsOpen()) return;
  if (IsLocked()) Unlock();
  if (IsOffline()) {CloseOffline();  return;}
  // Unmap the shared memory window ...
  ret = PlxPci_PciBarUnmap(&m_pplxData->plxDevice, (void **) &m_pWindow);
  if (ret != ApiSuccess) PLXError("unmapping window", ret);
  // Cancel the interrupt notification ...
  if (m_pplxData->plxNotify.IsValidTag != 0) {
    ret = PlxPci_NotificationCancel(&m_pplxData->plxDevice, &m_pplxData->plxNotify);
    if (ret != ApiSuccess) PLXError("cancelling interrupt notification", ret);
  }
  // Release the PCI device ...
  ret = PlxPci_DeviceClose(&m_pplxData->plxDevice);
  if (ret != ApiSuccess) PLXError("closing PLX device", ret);
  LOGS(DEBUG, "UPE PCI interface closed for " << *this);
  m_pWindow = NULL;
}


void CUPE::CloseOffline()
{
  //++
  //   This method will close an offline connection. If we're offline, there
  // are no PCI/PLX resources to be freed and the shared memory is just
  // ordinary heap. 
  //--
  // Note that free() doesn't like the volatile attribute of m_pWindow !
  if (m_pWindow != NULL) free((void *) m_pWindow);
  m_pWindow = NULL;
}


bool  CUPE::Lock (bool fForce)
{
  //++
  //   This method will check the current owner PID of the FPGA board and,
  // assuming it's zero, it will store our PID as the owner.  This PID is
  // stored in a 32 bit register that's actually implemented by the FPGA,
  // so it's visible to anybody who maps the shared memory region.
  //
  //   If the current FPGA owner isn't zero (and assuming it's not the same
  // as our PID) then the FPGA belongs to some other process.  We'll print
  // an error message and return FALSE in that case, UNLESS fForce is TRUE.
  // In that case we'll take ownership of the FPGA board anyway.  This is
  // useful in cases where the previous process terminated abnormally and
  // didn't clear the FPGA owner register before exiting
  //--
  assert(IsOpen());
  if (IsLocked()) return true;
  if (fForce || (GetOwner() == 0)) {
    SetOwner(GetOurPID());
    LOGF(DEBUG,"UPE %s locked to process %08X", GetBDF().c_str(), GetOwner());
    return true;
  }
  LOGF(ERROR, "UPE %s is already in use by process %08X", GetBDF().c_str(), GetOwner());
  return false;
}


void  CUPE::Unlock()
{
  //++
  //   Assuming we currently own this FPGA board, zero the owner PID.  This
  // makes it available to all.
  //--
  assert(IsOpen());
  if (GetOwner() == GetOurPID()) {
    LOGF(DEBUG, "UPE %s unlocked from process %08X", GetBDF().c_str(), GetOwner());
    SetOwner(0);
  }
}


uint32_t CUPE::RegisterInterrupt ()
{
  //++
  // Capture generic local bus -> PCI interrupts ...
  //--
  if (IsOffline()) return ApiSuccess;
  m_pplxData->plxInterrupt.LocalToPci = PLX_IRQ_MASK;
  PLX_STATUS ret = PlxPci_NotificationRegisterFor(&m_pplxData->plxDevice, &m_pplxData->plxInterrupt, &m_pplxData->plxNotify);
  if (ret != ApiSuccess) PLXError("registering interrupt", ret);
  return ret;
}


uint32_t CUPE::EnableInterrupt()
{
  //++
  //   Enable interrupts from the FPGA.  Note that the generic PLX driver will
  // disable interrupts after every interrupt (to prevent the PC from hanging
  // if there's no software there to service the interrupt).  That's good, but
  // it means that we have to re-enable interrupts every time thru!
  //
  //   Remember that RegisterInterrupt() must be called first!
  //--
  PLX_STATUS ret = PlxPci_InterruptEnable(&m_pplxData->plxDevice, &m_pplxData->plxInterrupt);
  if (ret != ApiSuccess) PLXError("enabling interrupts", ret); 
  return ret;
}


uint32_t CUPE::WaitInterrupt (uint32_t lTimeout)
{
  //++
  // Wait for an interrupt from the FPGA ...
  //--
  PLX_STATUS ret = PlxPci_NotificationWait(&m_pplxData->plxDevice, &m_pplxData->plxNotify, lTimeout);
  if ((ret == ApiWaitTimeout) || (ret == ApiWaitCanceled)) return ret;
  if (ret != ApiSuccess) PLXError("waiting for interrupt", ret);
  return ret;
}


bool CUPE::BarSpaceWrite (uint32_t cbOffset, const void *pBuffer, uint32_t cbBuffer, uint8_t nAccess, bool fLocalAddr)
{
  //++
  // Copy memory directly from our process address space to the PCI BAR space.
  //--
  PLX_STATUS ret = PlxPci_PciBarSpaceWrite(&m_pplxData->plxDevice, PLX_BAR_SHAREDMEM, cbOffset, (void *) pBuffer, cbBuffer, (PLX_ACCESS_TYPE) nAccess, fLocalAddr);
  return ret == ApiSuccess;
}


bool CUPE::BarSpaceRead (uint32_t cbOffset, void *pBuffer, uint32_t cbBuffer, uint8_t nAccess, bool fLocalAddr)
{
  //++
  // Copy memory directly from the PCI BAR space to our process address space.
  //--
  PLX_STATUS ret = PlxPci_PciBarSpaceRead(&m_pplxData->plxDevice, PLX_BAR_SHAREDMEM, cbOffset, pBuffer, cbBuffer, (PLX_ACCESS_TYPE) nAccess, fLocalAddr);
  return ret == ApiSuccess;
}


bool CUPE::ModifyControlRegister (uint32_t lClear, uint32_t lSet)
{
  //++
  //   This routine will modify bits in the PCI9054 chip CNTRL (control and
  // status) register.  This is used to control the user I/O bits for FPGA
  // programming.  The lSet parameter is a mask of bits to be set, and lClear
  // is a mask of bits to be cleared...  It returns TRUE if it succeeds, and
  // FALSE if it cannot access the PCI control register for some reason ...
  //--
  uint32_t lCtrl;  PLX_STATUS ret;
  ret = PlxPci_IoPortRead(&m_pplxData->plxDevice, m_pplxData->wCSRport, &lCtrl, 4, BitSize32);
  if (ret != ApiSuccess) goto NoAccess;
  lCtrl = (lCtrl & ~lClear) | lSet;
  ret = PlxPci_IoPortWrite(&m_pplxData->plxDevice, m_pplxData->wCSRport, &lCtrl, 4, BitSize32);
  if (ret != ApiSuccess) goto NoAccess;
  return true;

NoAccess:
  return PLXError("can't access PCI9054 CNTRL register", ret);
}


bool CUPE::IsProgramDone() const
{
  //++
  //   This routine returns TRUE if the FPGA DONE output is asserted and FALSE
  // otherwise.  The FPGA DONE output is connected to the PCI9054 USERi pin,
  // so all we have to do is read the 9054 control/status register and check.
  //--
  uint32_t lStatus;  PLX_STATUS ret;
  //   I don't completely understand why this delay is necessary, but 
  // empirically it won't work without it.  If you ever figure out why,
  // then fix it!
  _sleep_ms(50);
  ret = PlxPci_IoPortRead(&m_pplxData->plxDevice, m_pplxData->wCSRport, &lStatus, 4, BitSize32);
  if (ret != ApiSuccess) return false;
  return (lStatus & MESA_CSR_DONE) == MESA_CSR_DONE;
}


bool CUPE::SetupUserIOpins()
{
  //++
  //   This method sets up the PCI9054 user I/O bits for use in programming
  // the FPGA.  The 9054 has exactly two user I/O bits - USERi which is an
  // input connected to the FPGA DONE pin, and USERo which is an output pin
  // connected to the FPGA /PROGRAM input.  Note that the FPGA /PROGRAM input
  // is active low however the USERo pin is not, so the sense of that output
  // is inverted.
  //--
  return SetControlBit(PLX_CSR_USERI|PLX_CSR_USERO|MESA_CSR_PROGRAM);
}


bool CUPE::EnableProgramMode()
{
  //++
  //   This routine enables the FPGA programming mode by driving the /PROGRAM
  // pin low for a short time and then returning it back to the high state.
  // This should initialize the FPGA and also clear the DONE bit.
  //
  //   It's not clear what the minimum /PROGRAM pulse width is, but we can
  // assume that it's probably a very short time.  
  //
  //   BTW, remember that the /PROGRAM input is active low but the USERo pin
  // is not, so clearing the USERo status bit ASSERTS the /PROGRAM input!
  //--
  if (!ClearControlBit(MESA_CSR_PROGRAM)) return false;
  //   There's no actual delay here - we assume that the overhead of doing two
  // PLXLIB API calls is more than enough!
  return SetControlBit(MESA_CSR_PROGRAM);
}


bool CUPE::WriteConfigurationData (uint8_t bData)
{
  //++
  //   This routine will send one byte of program data to the FPGA.  While the
  // MESA board is in program mode, we can simply write the data to any memory
  // address or to any I/O port that's mapped to this card.  Either works, but
  // writing to memory is faster.
  //
  //   Note that we have to reverse the order of the bits in the byte!
  //--
  CBitStream::SwapBits(bData);

  //   WARNING!!  Use one of the following ("write to memory space" or "write
  // to I/O space") but NOT BOTH!
  // Write to memory space ...
  *((volatile uint8_t *) m_pWindow) = bData;

  // Write data to I/O space ...
//PLX_STATUS ret;
//ret = PlxPci_IoPortWrite(&m_pplxData->plxDevice, m_pplxData->wIObase, &bData, 4, BitSize32);
//if (ret != ApiSuccess) 
//  return PLXError("can't write 9054 card data register", ret);

  return true;
}


bool CUPE::LoadConfiguration (const CBitStream *pBits)
{
  //++
  //   This method loads a Xilinx configuration bitstream into the FPGA on the
  // MESA 5I22 card.  Without the FPGA configured, of course, the card does
  // basically nothing, so this is an important step!  This is essentially the
  // same function as performed by the MESA SC9054W command line tool.
  //
  //   The argument is a pointer to a CBitStream object which contains the
  // actual FPGA configuration information.  Usually you'd create one of these
  // by loading a Xilinx .BIT file.  If the FPGA configuration fails for some
  // reason, this routine logs an error and returns false.
  //
  //   Note that the Open() method must be called on this FPGA before this
  // method can be used.  It's worth pointing out that configuring the FPGA has
  // no direct effect on the state of the PLX chip or the internal state of
  // this CUPE object.  Initializing the FPGA may, however, have a significant
  // effect on the state of some higher level MASSBUS, CDC channel, Sigma disk,
  // etc, interface.  That's basically the problem of any class that inherits
  // this one - this LoadConfiguration() method is virtual, so any superclass
  // can always define its own version and include any pre or post configuration
  // steps that are necessary.
  //--
  assert(IsOpen());
  if (IsOffline()) return true;

  // Configure the PCI9054 user I/O pins and put the FPGA in program mode ...
  if (!SetupUserIOpins()) return false;
  if (!EnableProgramMode()) return false;

  //   At this point the Xilinx configuration done bit should NOT be set - if
  // we find that it is, something's screwed up.  This isn't a really strong
  // diagnostic, but it's the best we can do.
  if (IsProgramDone())
    return PLXError("DONE bit stuck high before programming");

  // Send all the configuation bytes to the FPGA ...
  const uint8_t *pabBits = pBits->GetBitStream();
  size_t cbBits = pBits->GetBitStreamSize();
  while (cbBits-- != 0) {
    if (!WriteConfigurationData(*pabBits++)) return false;
  }

  //   The Xilinx needs "a few" (I think it's about 8, but I'm not really sure)
  // extra clocks during configuration to allow its internal state machine to
  // finish. The easiest way to generate these is just to send some extra dummy
  // bytes.  The actual data doesn't matter - the Xilinx will ignore it.
  for (uint32_t i = 0;  i < 32;  ++i) {
    if (!WriteConfigurationData(0)) return false;
  }

  //   And now we just need to wait for the FPGA to assert DONE. As a practical
  // this happens ridiculously fast (in fact, it should already ne done by
  // now!) but just to be safe we'll implement a very short timeout here ...
  // Note that once we find DONE asserted, we're literally done - there's 
  // nothing more to do!
  for (uint32_t i = 0;  i < 10000;  ++i) {
    if (IsProgramDone()) {
      //   Notice that we actually test DONE twice and require that we find two
      // one bits in a row.  This eliminates the possibility of a glitch.
      if (!IsProgramDone()) continue;
      LOGS(DEBUG, "FPGA part " << pBits->GetPartName() << " configured with " << pBits->GetBitStreamSize() << " bytes");
      return true;
    }
  }

  //   No DONE - something went wrong!  The most likely reason is that the 
  // configuration bitstream is either corrupted or is for a different FPGA
  // device.  Another possibility is that the hardware is broken, or even
  // that this software is screwed up!
  return PLXError("DONE failed to set after programming");
}
