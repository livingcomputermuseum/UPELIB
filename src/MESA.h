//++
// MESA.hpp -> MESA 5i22 board definitions
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
//   This header file defines some constants and macros specific to the MESA
// 5i22 FPGA board.  Actually, most of them are for the PLX PCI9054 chip that
// board uses...
//
// Bob Armstrong <bob@jfcl.com>   [21-SEP-2015]
//
// REVISION HISTORY:
// 21-Sep-15  RLA   New file.
//--
#pragma once

// PLX PCI9054 chip ID constants (for finding it on the PCI bus) ....
#ifndef PLX_PCI_VENDOR_ID_PLX       // use PLX_PCI_VENDOR_ID_PLX from plx.h! 
#define PLX_PCI_VENDOR_ID_PLX 0x10B5// vendor ID for PCI9054 chip
#endif
#define PLX_PCI_DEVICE_ID_PLX 0x9054// device ID  "   "   "   "    "
  
// PLX PCI9054 BAR, I/O and PCI register offsets ...
#define PLX_REG_DATAOFFSET     0    // first data register offset
#define PLX_REG_CSROFFSET 0x006C    // control/status (CNTRL) register offset
#define PLX_BAR_LCLCFG         1    // local configuration BAR
#define PLX_BAR_IO32           2    // FPGA I/O window BAR
#define PLX_BAR_CFGMEM         3    // configuration memory BAR
#define PLX_BAR_SHAREDMEM      3    // BAR used for shared FPGA memory
#define PLX_IRQ_MASK           3    // mask for PLX_INTERRUPT structure
#define PCIBAR2OFF(r) (0x10 + ((r) * 4))
#define PLX_REG_LCLCFG PCIBAR2OFF(PLX_BAR_LCLCFG) // local config I/O port
#define PLX_REG_IO32   PCIBAR2OFF(PLX_BAR_IO32)   // first I/O window port

// User I/O bits in the control/status register ...
//  (These at least are specific to the MESA 5I22 board!)
#define PLX_CSR_USERO    0x80000    // enable USERo pin
#define PLX_CSR_USERI    0x40000    // enable USERi pin
#define MESA_CSR_DONE    0x20000    // FPGA done bit (USERi pin)
#define MESA_CSR_PROGRAM 0x10000    // FPGA /PROGRAM biit (USERo pin)

