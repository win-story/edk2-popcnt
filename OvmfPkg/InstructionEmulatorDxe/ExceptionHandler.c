#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>

UINT64
CountSetBits(
  UINT64 value
) {
  UINT64 count = 0;
  while (value) {
    count += value & 1;
    value >>= 1;
  }
  return count;
}

UINT64
ReadMemoryOperand(
  EFI_SYSTEM_CONTEXT_X64 *ContextRecord,
  UINT8 *ModRM, UINT8 *Rip
) {
  UINT64 Operand = 0;
  UINT8 Mod = (*ModRM >> 6) & 0x3;
  UINT8 Rm = *ModRM & 0x7;

  UINT64 BaseValue = 0;
  INT32 Disp32 = 0;
  INT8 Disp8 = 0;

  switch (Rm) {
    case 0: BaseValue = ContextRecord->Rax; break;
    case 1: BaseValue = ContextRecord->Rcx; break;
    case 2: BaseValue = ContextRecord->Rdx; break;
    case 3: BaseValue = ContextRecord->Rbx; break;
    case 4: BaseValue = ContextRecord->Rsp; break;
    case 5: BaseValue = (Mod == 0) ? (UINT64)(Rip + 4 + *(INT32 *)(ModRM + 1)) : ContextRecord->Rbp; break;
    case 6: BaseValue = ContextRecord->Rsi; break;
    case 7: BaseValue = ContextRecord->Rdi; break;
  }

  if (Mod == 0x01) {
    Disp8 = *(INT8 *)(ModRM + 1);
    Operand = *(UINT64 *)(BaseValue + Disp8);
  } else if (Mod == 0x02) {
    Disp32 = *(INT32 *)(ModRM + 1);
    Operand = *(UINT64 *)(BaseValue + Disp32);
  } else if (Mod == 0x00 && Rm == 5) {
    Operand = *(UINT64 *)(Rip + 4 + *(INT32 *)(ModRM + 1));
  } else {
    Operand = *(UINT64 *)(BaseValue);
  }

  return Operand;
}

VOID
EFIAPI
ExceptionHandler(
  IN EFI_EXCEPTION_TYPE ExceptionType,
  IN EFI_SYSTEM_CONTEXT SystemContext
) {
  if (ExceptionType != EXCEPT_IA32_INVALID_OPCODE) {
    return;
  }

  UINT8 *Rip = (UINT8 *)SystemContext.SystemContextX64->Rip;

  // Check if the instruction is POPCNT (opcode: F3 0F B8 /r)
  if (Rip[0] == 0xF3 && Rip[1] == 0x0F && Rip[2] == 0xB8) {
    DEBUG((EFI_D_INFO, "POPCNT instruction detected at 0x%p\n", Rip));

    // Decode ModR/M byte to get the operand
    UINT8 ModRM = Rip[3];
    UINT8 Mod = (ModRM >> 6) & 0x3;
    UINT8 Reg = (ModRM >> 3) & 0x7;
    UINT8 Rm = ModRM & 0x7;

    UINT64 Operand = 0;
    if (Mod == 0x3) {
      // Operand is a register
      switch (Rm) {
        case 0: Operand = SystemContext.SystemContextX64->Rax; break;
        case 1: Operand = SystemContext.SystemContextX64->Rcx; break;
        case 2: Operand = SystemContext.SystemContextX64->Rdx; break;
        case 3: Operand = SystemContext.SystemContextX64->Rbx; break;
        case 4: Operand = SystemContext.SystemContextX64->Rsp; break;
        case 5: Operand = SystemContext.SystemContextX64->Rbp; break;
        case 6: Operand = SystemContext.SystemContextX64->Rsi; break;
        case 7: Operand = SystemContext.SystemContextX64->Rdi; break;
        default: return;
      }
    } else {
      // Handle memory operand
      Operand = ReadMemoryOperand(SystemContext.SystemContextX64, Rip + 3, Rip);
    }

    // Emulate POPCNT
    UINT64 Result = CountSetBits(Operand);

    // Store result in the destination register
    switch (Reg) {
      case 0: SystemContext.SystemContextX64->Rax = Result; break;
      case 1: SystemContext.SystemContextX64->Rcx = Result; break;
      case 2: SystemContext.SystemContextX64->Rdx = Result; break;
      case 3: SystemContext.SystemContextX64->Rbx = Result; break;
      case 4: SystemContext.SystemContextX64->Rsp = Result; break;
      case 5: SystemContext.SystemContextX64->Rbp = Result; break;
      case 6: SystemContext.SystemContextX64->Rsi = Result; break;
      case 7: SystemContext.SystemContextX64->Rdi = Result; break;
      default: return;
    }

    // Adjust RIP to point to the next instruction
    SystemContext.SystemContextX64->Rip += 4; // Length of POPCNT instruction

    return;
  }
}