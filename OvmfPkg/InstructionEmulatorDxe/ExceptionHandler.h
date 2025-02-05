#ifndef _INSTRUCTION_EXCEPTION_HANDLER_H_
#define _INSTRUCTION_EXCEPTION_HANDLER_H_

#include <Uefi.h>

#define PREFIX_OP_SIZE_OVERRIDE 0x66
#define PREFIX_REPNE 0xF2
#define PREFIX_REP 0xF3
#define PREFIX_REX_BASE 0x40
#define OPCODE_POPCNT 0xB8
#define OPCODE_EXTENDED 0x0F

static UINT64 CountSetBits(UINT64 value);
static UINT64 ReadRegister(UINT8 Reg, EFI_SYSTEM_CONTEXT_X64 *ContextRecord);
static VOID WriteRegister(UINT8 Reg, EFI_SYSTEM_CONTEXT_X64 *ContextRecord, UINT64 Value);
static UINT64 ReadMemoryOperand(EFI_SYSTEM_CONTEXT_X64 *ContextRecord, UINT8 *ModRM, UINT8 *Rip);
static UINTN GetInstructionLength(UINT8 *Rip, BOOLEAN HasRexPrefix);
static BOOLEAN CheckInstruction(UINT8 Opcode, BOOLEAN Extended, UINT8 *InstructionPtr, BOOLEAN *HasRexPrefix, UINT8 *REX);

VOID
EFIAPI
ExceptionHandler(
  IN EFI_EXCEPTION_TYPE ExceptionType,
  IN EFI_SYSTEM_CONTEXT SystemContext
);

#endif
