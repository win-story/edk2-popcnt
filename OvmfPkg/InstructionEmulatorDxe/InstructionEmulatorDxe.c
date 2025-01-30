#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/Cpu.h>
#include "ExceptionHandler.h"

EFI_STATUS
EFIAPI
InstructionEmulatorDxeEntryPoint(
  IN EFI_HANDLE ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
) {
  EFI_STATUS Status;

  // Register the exception handler
  EFI_CPU_ARCH_PROTOCOL *Cpu;
  Status = gBS->LocateProtocol(&gEfiCpuArchProtocolGuid, NULL, (VOID**)&Cpu);
  if (EFI_ERROR(Status)) {
    DEBUG((EFI_D_ERROR, "Failed to locate CPU Arch protocol\n"));
    return Status;
  }

  Status = Cpu->RegisterInterruptHandler(Cpu, EXCEPT_IA32_INVALID_OPCODE, ExceptionHandler);
  if (EFI_ERROR(Status)) {
    DEBUG((EFI_D_ERROR, "Failed to register exception handler\n"));
    return Status;
  }

  DEBUG((EFI_D_INFO, "Instruction Emulation Driver Loaded\n"));
  return EFI_SUCCESS;
}