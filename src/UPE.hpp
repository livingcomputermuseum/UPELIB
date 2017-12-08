//++
// upe.hpp -> CUPE (FPGA/UPE) interface class declarations
//            CUPEs UPE collection class declarations
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
//   The CUPE class encapsulates all the methods for interfacing to the UPE
// ("Universal Peripheral Emulator", aka the FPGA board!).  The CUPEs class
// is a collection of UPE objects for all FPGA/UPE boards installed in this
// computer.
//
// Bob Armstrong <bob@jfcl.com>   [20-MAY-2015]
//
// REVISION HISTORY:
// 20-MAY-15  RLA   Adapted from MBS.
//  5-APR-16  RLA   Add template for CUPEs::ReOpen() ...
//--
#pragma once
#include <string>               // C++ std::string class, et al ...
#include <iostream>             // C++ style output for LOGS() ...
#include <sstream>              // C++ std::stringstream, et al ...
#include <vector>               // C++ std::vector template
using std::string;              // ...
using std::ostream;             // ...
using std::vector;              // ...
#include "Thread.hpp"           // needed for PROCESS_ID ...
#include "BitStream.hpp"        // needed to compile the CUPE class
#include "LogFile.hpp"          // needed to compile the CUPEs::ReOpen() template
extern class CUPEs *g_pUPEs;    // needed to compile the CUPEs::ReOpen() template


//   This little hack allows files other than upe.cpp to include this header
// without also needing to include the entire PLX library API headers!
typedef struct _UPE_PLXLIB_DATA UPE_PLXLIB_DATA;
typedef struct _PLX_DEVICE_KEY PLX_DEVICE_KEY;


// CUPE class definition ...
class CUPE {
  //++
  // The CUPE class describes the interface to a single FPGA/UPE board ...
  //--

  // Constants and parameters ...
public:
  enum {
    SHARED_MEMORY_SIZE = 65536  // the expected size of the UPE window
  };

  // Constructor and destructor ...
public:
  CUPE (const PLX_DEVICE_KEY *pplxKey);
  virtual ~CUPE();

  // Public properties for the UPE ...
public:
  // Test the status of this UPE ...
  bool IsOffline() const {return m_pplxData == NULL;}
  bool IsOpen() const {return m_pWindow != NULL;}
  // Return details about the associated PCI card ...
  uint8_t GetPCIBus() const;
  uint8_t GetPCISlot() const;
  uint16_t GetPLXChip() const;
  uint8_t GetPLXRevision() const;
  const PLX_DEVICE_KEY *GetPLXDeviceKey() const;
  // Convert a PCI address to BDF ("bus:domain.function") notation ...
  static string GetBDF (uint8_t nBus, uint8_t nSlot, uint8_t nFunction=0);
  static string GetBDF (PLX_DEVICE_KEY *pplxKey);
  string GetBDF() const {return GetBDF(GetPCIBus(), GetPCISlot());}
  // Return details about the PLX library we're using ...
  static string GetSDKVersion();
  // Return the address of the shared memory window ...
  volatile void *GetWindow() const {return m_pWindow;}

  //   All (well, most) of the UPE implementations have a version number for
  // the VHDL code stored somewhere in the shared memory map.  It'd be nice
  // if it was in the same place for all of them, but it isn't, so the code to
  // extract the VHDL revision depends on the application specific memory map.
  // The generic version just returns zeros.
  virtual uint16_t GetRevision() const {return 0;}

  //   The FPGA locking facility allows us to record in the FPGA's shared
  // memory the PID of the process that currently owns that FPGA.  This
  // prevents two instances of MBS, CPS, etc from stepping on each other
  // should they accidentally be connected to the same FPGA board.  The
  // algorithm for doing this is independent of the application, but it
  // does unfortunately require knowledge of the specific FPGA memory
  // layout.  The generic library version here never locks anything and
  // requires that each specific implementation override the GetOwner()
  // and SetOwner() functions.
  virtual PROCESS_ID GetOwner() const {return 0;}
  virtual void SetOwner (PROCESS_ID nPID) {};
  // Get OUR current PID (whether we're the owner or not!) ...
  static PROCESS_ID GetOurPID() {return CThread::GetCurrentProcessID();}
  // Return TRUE if this UPE is locked to our process ...
  bool IsLocked() const
    {return IsOpen() ? (GetOwner() == GetOurPID()) : false;}

  // Public UPE methods ...
public:
  // Open and close the PLX FPGA interface ...
  bool Open();
  void Close();
  // Open and close special offline connections ...
  bool OpenOffline();
  void CloseOffline();
  // Lock the UPE to this process (and unlock it later) ...
  bool Lock (bool fForce=false);
  void Unlock();
  // Initialize the UPE's state ...
  virtual bool Initialize()  {return true;}
  // Wait for interrupts from the FPGA ...
  uint32_t RegisterInterrupt();
  uint32_t EnableInterrupt();
  uint32_t WaitInterrupt(uint32_t lTimeout);
  // Direct memory space access (memcpy to the bar space) ...
  bool BarSpaceWrite (uint32_t cbOffset, const void *pBuffer, uint32_t cbBuffer, uint8_t nAccess=2 /*BitSize32*/, bool fLocalAddr=true);
  bool BarSpaceRead (uint32_t cbOffset, void *pBuffer, uint32_t cbBuffer, uint8_t nAccess=2 /*BitSize32*/, bool fLocalAddr=true);
  // Load the FPGA bit stream ...
  virtual bool LoadConfiguration (const CBitStream *pBits);

  // Protected methods ...
protected:
  // Report PLXLIB errors ...
  bool PLXError (const char *pszMsg, uint32_t nRet=0) const;
  // Modify bits in the PCI9054 CNTRL register ...
  bool ModifyControlRegister (uint32_t lClear, uint32_t lSet);
  bool SetControlBit (uint32_t lSet)  {return ModifyControlRegister(0, lSet);}
  bool ClearControlBit (uint32_t lClear)  {return ModifyControlRegister(lClear, 0);}
  // Return TRUE if the FPGA DONE output is asserted ...
  bool IsProgramDone() const;
  // Enable the PCI9054 user I/O pins ...
  bool SetupUserIOpins();
  // Reset the FPGA and enable programming mode ...
  bool EnableProgramMode();
  // Send configuration data to the FPGA ...
  bool WriteConfigurationData (uint8_t bData);

  // Disallow copy and assignment operations with CUPE objects...
  //   There's locally allocated data in m_pplxData and m_pUPE, and it's not
  // worth the trouble of actually making this work correctly!
private:
  CUPE(const CUPE&) = delete;
  CUPE& operator= (const CUPE&) = delete;

  // Private member data ...
protected:
  volatile void    *m_pWindow;  // pointer to the FPGA/UPE shared memory
private:
  UPE_PLXLIB_DATA  *m_pplxData; // all instance specific PLX library data
};


//   This handy little inserter allows you to send an UPE's identification
// (it actually just prints the BDF PCI address) to an I/O stream for error
// and debug messages ...
inline ostream& operator << (ostream &os, const CUPE &upe)
  {os << upe.GetBDF();  return os;}


// This routine is the dummy generic CUPE "object factory".
extern "C" CUPE *NewUPE (const PLX_DEVICE_KEY *pplxKey);


// CUPEs collection class definition ...
class CUPEs
{
  //++
  //   The CUPEs class is a collection of all FPGA/UPE boards installed.  The
  // implementation is just an STL std::vector of pointers to CUPE objects.
  // The Enumerate() method should be called once, shortly after startup, to
  // populate the collection with all the UPEs installed in this PC.  Note
  // that this class "owns" the CUPE objects in the collection - they're
  // created by Enumerate() (or the Add() method) and all the CUPE objects
  // are destroyed when this object is destroyed.
  //
  //   For convenience, this class exposes some of the STL vector behaviors,
  // most notably iterators...
  //
  //   There is, however, a small problem (how many times have you heard that
  // before??).  The trouble is, we don't really want a collection of CUPE
  // objects - what we want is a collection of CMDEUPE objects, or CCDCUPE or
  // CSDEUPE or whatever class this application derives from CUPE. CUPE objects
  // aren't really used; it's always an application specific class that's
  // derived from it.
  //
  //   "No problem", I hear you saying, "Make CUPEs a templated class. That's
  // what templates are for, after all."  It's true that you can solve the
  // problem with templates - I actually coded it all up and tried - but there
  // is another issue.  With templated member functions, the compiler needs to
  // have the source code for the templated method available when it compiles
  // a reference.  The net effect of that is that member functions in this
  // class that have real code, like Find(), Enumerate() and Open() would have
  // inlined and put in this header file.  Uggh - not good.
  //
  //   BUT, there is another way.  The only reason any of the code in this
  // class needs to care what kind of objects (CUPE or some derived class) are
  // stored in this collection is because there are some functions (e.g. Add(),
  // Open(), etc) that do new operations to create new object instances.  If
  // an application is using a CUPE derived class, then it's critical that we
  // create an instance of the appropriate derived object and not a base class
  // instance.  That's all - otherwise none of this code cares whether the
  // collection contains pointers to CUPE objects or any class derived from that.
  //
  //   So the answer is easy - we just require the caller to supply us with an
  // "object factory" method that creates new instances of any CUPE derived
  // class.  We use that object factory anytime we need a new CUPE object, and
  // everybody is happy.  Problem solved!
  //--

  // Constructor and destructor ...
public:
  typedef CUPE* (&CUPE_FACTORY) (const PLX_DEVICE_KEY *pplxKey);
  CUPEs (CUPE_FACTORY of) : m_NewUPE(of) {};
  virtual ~CUPEs();

  // Public UPE collection properties ...
public:
  // Find a particular UPE by its BDF ...
  CUPE *Find (uint8_t nBus, uint8_t nSlot) const;

  // Delegate iterators for this collection ...
public:
  typedef vector<CUPE *> CUPE_VECTOR;
  typedef CUPE_VECTOR::iterator iterator;
  typedef CUPE_VECTOR::const_iterator const_iterator;
  iterator begin() {return m_vecUPEs.begin();}
  const_iterator begin() const {return m_vecUPEs.begin();}
  iterator end() {return m_vecUPEs.end();}
  const_iterator end() const {return m_vecUPEs.end();}

  // Delegate array style addressing for this collection ...
public:
  // Notice that we use UPE() instead of at() and Count() instead of size() ...
  size_t Count() const {return m_vecUPEs.size();}
  CUPE &UPE(uint32_t n) {return *m_vecUPEs.at(n);}
  const CUPE &UPE(uint32_t n) const {return *m_vecUPEs.at(n);}
  CUPE& operator[] (uint32_t n) {return UPE(n);}
  const CUPE& operator[] (uint32_t n) const {return UPE(n);}

  // Public interface functions for the UPE ...
public:
  // Add CUPE instances ...
  CUPE *Add(CUPE *pUPE) {m_vecUPEs.push_back(pUPE);  return pUPE;}
  CUPE *Add(PLX_DEVICE_KEY *pplxKey) {return Add(m_NewUPE(pplxKey));}
  CUPE *AddOffline() {return Add((PLX_DEVICE_KEY *) NULL);}
  // Enumerate all the FPGA/UPE devices on this PC ...
  void Enumerate();
  // Replace one UPE instance with another ...
  bool Replace(CUPE *pOld, CUPE *pNew);
  // Find a particular UPE instance and open it ...
  bool Open (uint8_t nBus, uint8_t nSlot, CUPE *&pUPE);

  template<class NEW_UPE_CLASS> bool ReOpen (uint8_t nBus, uint8_t nSlot, NEW_UPE_CLASS *&pUPE)
  {
    //++
    //   This nifty member template function does the equivalent of a call to
    // Open(), HOWEVER as a side effect the old generic CUPE object is deleted
    // and is replaced by a new, application specific CUPE subclass object, as
    // specified in the template.  This was originally used for SDE, which needs
    // a different CUPE derived class for every peripheral type (e.g. disk, card
    // reader, printer, etc).  It could, however, also be used to REPLACE THE
    // OBJECT FACTORY mechanism used in CPS and MBS.  That's a little radical
    // for now, bit maybe someday.
    //
    //   Sorry that all this code has to be inline, but since it is a template
    // the entire code must be visible to the module where it's used, otherwise
    // the compiler has no way to instanciate it for, say, SDE specific classes.
    // Open a UPE and convert it to a new CUPE subclass ...
    //--
    if ((nSlot == 0)  &&  (nBus == 0)) {
      // Create and open an offline UPE ...
      pUPE = DBGNEW NEW_UPE_CLASS ((PLX_DEVICE_KEY *) NULL);
      Add(pUPE);  return pUPE->OpenOffline();
    } else {
      // Open a real, physical, UPE ...
      CUPE *pOldUPE = g_pUPEs->Find(nBus, nSlot);
      if (pOldUPE == NULL) {
        LOGS(ERROR, "no UPE found at PCI address " << CUPE::GetBDF(nBus, nSlot));  return false;
      }
      pUPE = DBGNEW NEW_UPE_CLASS (pOldUPE->GetPLXDeviceKey());
      if (pUPE->IsOpen()) {
        LOGS(ERROR, "UPE " << *pUPE << " is in use");  return false;
      }
      Replace(pOldUPE, pUPE);  return pUPE->Open();
    }
  }

  // Disallow copy and assignment operations with CUPEs objects too...
private:
  CUPEs(const CUPEs&) = delete;
  CUPEs& operator= (const CUPEs&) = delete;

  // Private member data ...
private:
  CUPE_VECTOR    m_vecUPEs;     // collection of all FPGA/UPEs installed
  CUPE_FACTORY   m_NewUPE;      // routine to create new CUPE instances
};
