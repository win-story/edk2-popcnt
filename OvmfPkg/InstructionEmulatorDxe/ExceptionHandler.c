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
    case 8: BaseValue = ContextRecord->R8; break; // REX.B
    case 9: BaseValue = ContextRecord->R9; break; // REX.B
    case 10: BaseValue = ContextRecord->R10; break; // REX.B
    case 11: BaseValue = ContextRecord->R11; break; // REX.B
    case 12: BaseValue = ContextRecord->R12; break; // REX.B
    case 13: BaseValue = ContextRecord->R13; break; // REX.B
    case 14: BaseValue = ContextRecord->R14; break; // REX.B
    case 15: BaseValue = ContextRecord->R15; break; // REX.B
    default: return 0;
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

// Helper function to calculate instruction length with REX prefix handling
UINTN
GetInstructionLength(
  UINT8 *Rip,
  BOOLEAN HasRexPrefix
) {
  UINT8 ModRM = HasRexPrefix ? Rip[4] : Rip[3];
  UINT8 Mod = (ModRM >> 6) & 0x3;
  UINT8 Rm = ModRM & 0x7;
  UINTN Length = HasRexPrefix ? 5 : 4; // Base length: F3 0F B8 /r with optional REX prefix

  if (Mod == 0x01) {
    Length += 1; // 8-bit displacement
  } else if (Mod == 0x02) {
    Length += 4; // 32-bit displacement
  }

  if (Rm == 4 && Mod != 0x3) {
    // SIB byte present
    Length += 1;
  }

  return Length;
}

VOID
EFIAPI
ExceptionHandler(
  IN EFI_EXCEPTION_TYPE ExceptionType,
  IN EFI_SYSTEM_CONTEXT SystemContext
) {
  if (ExceptionType != EXCEPT_X64_INVALID_OPCODE) {
    return;
  }

  UINT8 *Rip = (UINT8 *)(SystemContext.SystemContextX64->Rip - 1);

  // Check for relevant prefixes
  UINT8 *InstructionPtr = Rip;
  BOOLEAN HasRexPrefix = FALSE;
  UINT8 REX = 0;

  while (*InstructionPtr == 0xF3 || *InstructionPtr == 0xF2 || *InstructionPtr == 0x66 || 
         (*InstructionPtr & 0xF0) == 0x40) {
    if ((*InstructionPtr & 0xF0) == 0x40) {
      REX = *InstructionPtr; // REX prefix found
      HasRexPrefix = TRUE;
    }
    InstructionPtr++;
  }

  // Check if the instruction is POPCNT (opcode: 0F B8 /r)
  if (*InstructionPtr == 0x0F && *(InstructionPtr + 1) == 0xB8) {
    DEBUG((EFI_D_INFO, "POPCNT instruction detected at 0x%p\n", Rip));

    UINT8 ModRM = HasRexPrefix ? *(InstructionPtr + 2) : *(InstructionPtr + 2);
    UINT8 Mod = (ModRM >> 6) & 0x3;
    UINT8 Reg = (ModRM >> 3) & 0x7;
    UINT8 Rm = ModRM & 0x7;

    // Adjust register encoding based on REX prefix
    if (HasRexPrefix) {
      if (REX & 0x01) Rm += 8; // REX.B
      if (REX & 0x04) Reg += 8; // REX.R
    }

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
        case 8: Operand = SystemContext.SystemContextX64->R8; break;  // REX.B
        case 9: Operand = SystemContext.SystemContextX64->R9; break;  // REX.B
        case 10: Operand = SystemContext.SystemContextX64->R10; break; // REX.B
        case 11: Operand = SystemContext.SystemContextX64->R11; break; // REX.B
        case 12: Operand = SystemContext.SystemContextX64->R12; break; // REX.B
        case 13: Operand = SystemContext.SystemContextX64->R13; break; // REX.B
        case 14: Operand = SystemContext.SystemContextX64->R14; break; // REX.B
        case 15: Operand = SystemContext.SystemContextX64->R15; break; // REX.B
        default: return;
      }
    } else {
      // Handle memory operand
      Operand = ReadMemoryOperand(SystemContext.SystemContextX64, InstructionPtr + 2, Rip);
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
      case 8: SystemContext.SystemContextX64->R8 = Result; break;  // REX.R
      case 9: SystemContext.SystemContextX64->R9 = Result; break;  // REX.R
      case 10: SystemContext.SystemContextX64->R10 = Result; break; // REX.R
      case 11: SystemContext.SystemContextX64->R11 = Result; break; // REX.R
      case 12: SystemContext.SystemContextX64->R12 = Result; break; // REX.R
      case 13: SystemContext.SystemContextX64->R13 = Result; break; // REX.R
      case 14: SystemContext.SystemContextX64->R14 = Result; break; // REX.R
      case 15: SystemContext.SystemContextX64->R15 = Result; break; // REX.R
      default: return;
    }

    // Adjust RIP to point to the next instruction
    UINTN InstructionLength = (UINTN)(InstructionPtr - Rip) + 2 + GetInstructionLength(InstructionPtr, HasRexPrefix);
    SystemContext.SystemContextX64->Rip = (UINT64)(Rip + InstructionLength);

    return;
  }
}
