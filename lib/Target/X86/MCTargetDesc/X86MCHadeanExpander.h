#ifndef X86MCHADEANEXPANDER_H
#define X86MCHADEANEXPANDER_H

#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCStreamer.h"
#include "X86Subtarget.h"

namespace llvm {

class HadeanExpander {
public:
  HadeanExpander(MCSubtargetInfo& STI, std::unique_ptr<MCInstrInfo> &&II)
    : STI_(STI),
      II_(std::move(II)),
      prefixedInstruction_(nullptr),
      emitCompletelyRaw_(false),
      emitWithoutMPX_MemoryAccess_(false),
      emitWithoutMPX_StackPtrUpdate_(false),
      emitWithoutCFI_(false) {}

  bool expandInstruction(MCStreamer& out, const MCInst &instr);

  static constexpr uint32_t kBundleSizeInBits = 5;
  static constexpr uint32_t kBundleSizeInBytes = (1u << kBundleSizeInBits);

  static constexpr unsigned kCodeBaseRegister = X86::R15;

private:
  enum ValueSize {
    k8bit  = 1,
    k16bit = 2,
    k32bit = 4,
    k64bit = 8,
  };

  MCSubtargetInfo& STI_;
  std::unique_ptr<MCInstrInfo> II_;

  std::vector<unsigned> instructionPrefixes_;
  const MCInst *prefixedInstruction_;

  // Emitting instruction recursively attempts to expand it.
  // When these values are set to true, skip re-expansion.
  bool emitCompletelyRaw_;
  bool emitWithoutMPX_MemoryAccess_;
  bool emitWithoutMPX_StackPtrUpdate_;
  bool emitWithoutCFI_;

  bool HandleMPX_MemoryAccess(MCStreamer &out, const MCInst &inst);
  bool HandleMPX_StackPtrUpdate(MCStreamer &out, const MCInst &inst);
  bool HandleCFI(MCStreamer &out, const MCInst &inst);

  void EmitDirectCall(MCStreamer &out, const MCInst &inst);
  void EmitIndirectCall(MCStreamer &out, const MCInst &inst);
  void EmitJump(MCStreamer &out, const MCInst &inst);
  void EmitReturn(MCStreamer &out, const MCInst &inst);

  void EmitSafeBranch(MCStreamer &out,
                      const MCInst &inst,
                      unsigned opcode,
                      unsigned targetReg,
                      bool alignToEnd);

  const MCInstrDesc &GetDesc(const MCInst &inst);
};

}

#endif
