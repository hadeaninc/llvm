#ifndef X86MCHADEAN_H
#define X86MCHADEAN_H

#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCStreamer.h"

namespace llvm {

class HadeanExpander
{
private:
  const MCSubtargetInfo subtargetInfo;
  const uint64_t xorValue;

  void emitHadeanRet(MCStreamer &out);
  void emitHadeanJump(MCStreamer &out, const MCOperand &op);
  void emitHadeanCall(MCStreamer &out, const MCOperand &op);
  void emitPUSH64r(MCStreamer &out, unsigned reg);
  void emitCall(MCStreamer &out, unsigned destReg, const MCExpr &returnAddress, const unsigned scratch);
  void emitPOP64r(MCStreamer &out, unsigned reg);
  void emitMOV64ri(MCStreamer &out, unsigned reg, uint64_t value);
  void emitXOR64rr(MCStreamer &out, unsigned destReg, unsigned opReg);
  void emitCMP64rr(MCStreamer &out, unsigned r1, unsigned r2);
  MCOperand buildExternalSymbolOperand(MCStreamer &out, const std::string &name);
  //void emitValidate();

public:
  HadeanExpander(const MCSubtargetInfo &info);
  bool expandInstruction(MCStreamer& out, const MCInst &instr);
};

}

#endif
