#ifndef X86MCHADEANEXPANDER_H
#define X86MCHADEANEXPANDER_H

#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCStreamer.h"
#include "X86Subtarget.h"

namespace llvm {

class HadeanExpander {
public:
  HadeanExpander(MCSubtargetInfo& STI) : STI_(STI), emitRaw_(false) {}
  bool expandInstruction(MCStreamer& out, const MCInst &instr);

  static constexpr uint32_t kBundleSizeInBits = 5;
  static constexpr uint32_t kBundleSizeInBytes = (1u << kBundleSizeInBits);

  static constexpr unsigned kCodeBaseRegister = X86::R15;

private:
  MCSubtargetInfo& STI_;

  // Emitting instruction recursively attempts to expand it.
  // When `emitRaw_` is set to true, skip re-expansion.
  bool emitRaw_;

  void EmitDirectCall(MCStreamer &out, const MCInst &inst);
  void EmitIndirectCall(MCStreamer &out, const MCInst &inst);
  void EmitJump(MCStreamer &out, const MCInst &inst);
  void EmitReturn(MCStreamer &out, const MCInst &inst);

  void EmitSafeBranch(MCStreamer &out, const MCInst &inst, unsigned targetReg, bool isCall);
};

}

#endif
