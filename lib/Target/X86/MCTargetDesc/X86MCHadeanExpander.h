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
      frames_({ WorkFrame(static_cast<Stage>(kStartStage + 1)) }) {}

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

  enum Stage {
    kStartStage = 0,
    kHadeanJumpStage,
    kMpxMemAccessStage,
    kMpxStackPtrStage,
    kCfiStage,
    kSyscallStage,
    kEmitStage,
  };

  typedef SmallVector<MCInst, 2> PrefixVector;
  struct PrefixInst {
    PrefixInst(const MCInst &inst, const PrefixVector &prefixes)
      : inst_(inst), prefixes_(prefixes) {}
    PrefixInst(const MCInst &inst, const PrefixInst &other)
      : inst_(inst), prefixes_(other.prefixes_) {}

    MCInst inst_;
    PrefixVector prefixes_;
  };

  struct WorkFrame {
    WorkFrame(Stage stage) : stage_(stage), prefixes_() {}

    Stage stage_;
    PrefixVector prefixes_;
  };
  SmallVector<WorkFrame, kEmitStage> frames_;

  void EmitWithPrefixes(MCStreamer &out, const PrefixInst &inst);

  bool HandleMPX_MemoryAccess(MCStreamer &out, const PrefixInst &inst);
  bool HandleMPX_StackPtrUpdate(MCStreamer &out, const PrefixInst &inst);
  bool HandleCFI(MCStreamer &out, const PrefixInst &inst);
  bool HandleHadeanJump(MCStreamer &out, const PrefixInst &inst);
  bool HandleSyscall(MCStreamer &out, const PrefixInst &inst);

  void EmitDirectCall(MCStreamer &out, const PrefixInst &inst);
  void EmitIndirectCall(MCStreamer &out, const PrefixInst &inst);
  void EmitJump(MCStreamer &out, const PrefixInst &inst);
  void EmitReturn(MCStreamer &out, const PrefixInst &inst);

  void EmitSafeBranch(MCStreamer &out,
                      const PrefixInst &inst,
                      unsigned opcode,
                      unsigned targetReg,
                      bool alignToEnd);

  const MCInstrDesc &GetDesc(const MCInst &inst);
};

}

#endif
