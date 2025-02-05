#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include "ExceptionHandler.h"

static UINT64 CountSetBits(UINT64 value) {
  UINT64 count = 0;
  while (value) {
    count += value & 1;
    value >>= 1;
  }
  return count;
}

static UINT64 ReadRegister(UINT8 Reg, EFI_SYSTEM_CONTEXT_X64 *ContextRecord) {
  switch (Reg) {
    case 0: return ContextRecord->Rax;
    case 1: return ContextRecord->Rcx;
    case 2: return ContextRecord->Rdx;
    case 3: return ContextRecord->Rbx;
    case 4: return ContextRecord->Rsp;
    case 5: return ContextRecord->Rbp;
    case 6: return ContextRecord->Rsi;
    case 7: return ContextRecord->Rdi;
    case 8: return ContextRecord->R8;
    case 9: return ContextRecord->R9;
    case 10: return ContextRecord->R10;
    case 11: return ContextRecord->R11;
    case 12: return ContextRecord->R12;
    case 13: return ContextRecord->R13;
    case 14: return ContextRecord->R14;
    case 15: return ContextRecord->R15;
    default: return 0;
  }
}

static VOID WriteRegister(UINT8 Reg, EFI_SYSTEM_CONTEXT_X64 *ContextRecord, UINT64 Value) {
  switch (Reg) {
    case 0: ContextRecord->Rax = Value; break;
    case 1: ContextRecord->Rcx = Value; break;
    case 2: ContextRecord->Rdx = Value; break;
    case 3: ContextRecord->Rbx = Value; break;
    case 4: ContextRecord->Rsp = Value; break;
    case 5: ContextRecord->Rbp = Value; break;
    case 6: ContextRecord->Rsi = Value; break;
    case 7: ContextRecord->Rdi = Value; break;
    case 8: ContextRecord->R8 = Value; break;
    case 9: ContextRecord->R9 = Value; break;
    case 10: ContextRecord->R10 = Value; break;
    case 11: ContextRecord->R11 = Value; break;
    case 12: ContextRecord->R12 = Value; break;
    case 13: ContextRecord->R13 = Value; break;
    case 14: ContextRecord->R14 = Value; break;
    case 15: ContextRecord->R15 = Value; break;
  }
}

static UINT64 ReadMemoryOperand(EFI_SYSTEM_CONTEXT_X64 *ContextRecord, UINT8 *ModRM, UINT8 *Rip) {
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
    case 8: BaseValue = ContextRecord->R8; break;
    case 9: BaseValue = ContextRecord->R9; break;
    case 10: BaseValue = ContextRecord->R10; break;
    case 11: BaseValue = ContextRecord->R11; break;
    case 12: BaseValue = ContextRecord->R12; break;
    case 13: BaseValue = ContextRecord->R13; break;
    case 14: BaseValue = ContextRecord->R14; break;
    case 15: BaseValue = ContextRecord->R15; break;
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

static UINTN GetInstructionLength(UINT8 *Rip, BOOLEAN HasRexPrefix) {
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

static BOOLEAN CheckInstruction(UINT8 Opcode, BOOLEAN Extended, UINT8 *InstructionPtr, BOOLEAN *HasRexPrefix, UINT8 *REX) {
  *HasRexPrefix = FALSE;
  *REX = 0;

  while (*InstructionPtr == PREFIX_REP || *InstructionPtr == PREFIX_REPNE || *InstructionPtr == PREFIX_OP_SIZE_OVERRIDE || 
         (*InstructionPtr & 0xF0) == PREFIX_REX_BASE) {
    if ((*InstructionPtr & 0xF0) == PREFIX_REX_BASE) {
      *REX = *InstructionPtr; // REX prefix found
      *HasRexPrefix = TRUE;
    }
    InstructionPtr++;
  }

  if (Extended) {
    if (*InstructionPtr == OPCODE_EXTENDED && *(InstructionPtr + 1) == Opcode) {
      return TRUE;
    }
  } else {
    if (*InstructionPtr == Opcode) {
      return TRUE;
    }
  }

  return FALSE;
}

VOID EFIAPI ExceptionHandler(IN EFI_EXCEPTION_TYPE ExceptionType, IN EFI_SYSTEM_CONTEXT SystemContext) {
  if (ExceptionType != EXCEPT_X64_INVALID_OPCODE) {
    return;
  }

  UINT8 *Rip = (UINT8 *)SystemContext.SystemContextX64->Rip;

  BOOLEAN HasRexPrefix;
  UINT8 REX;
  if (CheckInstruction(OPCODE_POPCNT, TRUE, Rip, &HasRexPrefix, &REX)) {
    DEBUG((EFI_D_INFO, "POPCNT instruction detected at 0x%p\n", Rip));

    UINT8 *InstructionPtr = Rip + (HasRexPrefix ? 1 : 0);
    UINT8 ModRM = *(InstructionPtr + 2);
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
      Operand = ReadRegister(Rm, SystemContext.SystemContextX64);
    } else {
      Operand = ReadMemoryOperand(SystemContext.SystemContextX64, InstructionPtr + 2, Rip);
    }

    // Emulate POPCNT
    UINT64 Result = CountSetBits(Operand);

    // Store result in the destination register
    WriteRegister(Reg, SystemContext.SystemContextX64, Result);

    // Adjust RIP to point to the next instruction
    UINTN InstructionLength = (UINTN)(InstructionPtr - Rip) + 2 + GetInstructionLength(InstructionPtr, HasRexPrefix);
    SystemContext.SystemContextX64->Rip = (UINT64)(Rip + InstructionLength);

    return;
  }
  else
  {
    DEBUG((EFI_D_INFO, "Unknown instruction detected at 0x%p\n", Rip));
  }
}