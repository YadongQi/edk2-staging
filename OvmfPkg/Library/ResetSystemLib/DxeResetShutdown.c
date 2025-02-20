/** @file
  DXE Reset System Library Shutdown API implementation for OVMF.

  Copyright (C) 2020, Red Hat, Inc.
  Copyright (c) 2006 - 2019, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Base.h>                   // BIT13

#include <Library/BaseLib.h>        // CpuDeadLoop()
#include <Library/DebugLib.h>       // ASSERT()
#include <Library/IoLib.h>          // IoOr16()
#include <Library/PcdLib.h>         // PcdGet16()
#include <Library/ResetSystemLib.h> // ResetShutdown()
#include <OvmfPlatforms.h>          // PIIX4_PMBA_VALUE

STATIC UINT16 mAcpiPmBaseAddress;

EFI_STATUS
EFIAPI
DxeResetInit (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  UINT16 HostBridgeDevId;

  HostBridgeDevId = PcdGet16 (PcdOvmfHostBridgePciDevId);
  switch (HostBridgeDevId) {
  case INTEL_82441_DEVICE_ID:
    mAcpiPmBaseAddress = PIIX4_PMBA_VALUE;
    break;
  case INTEL_Q35_MCH_DEVICE_ID:
    mAcpiPmBaseAddress = ICH9_PMBASE_VALUE;
    break;
  case CH_VIRT_HOST_DEVICE_ID:
    break;
  default:
    ASSERT (FALSE);
    CpuDeadLoop ();
    return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

/**
  Calling this function causes the system to enter a power state equivalent
  to the ACPI G2/S5 or G3 states.

  System shutdown should not return, if it returns, it means the system does
  not support shut down reset.
**/
VOID
EFIAPI
ResetShutdown (
  VOID
  )
{
  if (mAcpiPmBaseAddress == 0) {
    IoWrite8 (CH_ACPI_SHUTDOWN_IO_ADDRESS, 5 << 2 | 1 << 5);
  } else {
    IoBitFieldWrite16 (mAcpiPmBaseAddress + 4, 10, 13, 0);
    IoOr16 (mAcpiPmBaseAddress + 4, BIT13);
  }
  CpuDeadLoop ();
}
