#ifndef X86MCHADEAN_H
#define X86MCHADEAN_H

#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCStreamer.h"

namespace llvm {

class HadeanExpander
{
private:
  // The branch target register reserved for indirect jumps
  static const unsigned BTR;

  const MCSubtargetInfo subtargetInfo;
  const uint64_t xorValue;

  void emitHadeanRet(MCStreamer &out);
  void emitHadeanJump(MCStreamer &out, MCInst::const_iterator opStart, MCInst::const_iterator opEnd);
  void emitHadeanCall(MCStreamer &out, MCInst::const_iterator opStart, MCInst::const_iterator opEnd);
  void emitPUSH64r(MCStreamer &out, unsigned reg);
  void emitPOP64r(MCStreamer &out, unsigned reg);
  void emitPUSHXMMr(MCStreamer &out, unsigned reg);
  void emitPOPXMMr(MCStreamer &out, unsigned reg);
  void emitMOV64ri(MCStreamer &out, unsigned reg, uint64_t value);
  void emitROL64ri(MCStreamer &out, unsigned reg, uint64_t value);
  void emitMOV64rr(MCStreamer &out, unsigned dst, unsigned src);
  void emitXOR64rr(MCStreamer &out, unsigned destReg, unsigned opReg);
  void emitAND64rr(MCStreamer &out, unsigned destReg, unsigned opReg);
  void emitCMP64rr(MCStreamer &out, unsigned r1, unsigned r2);
  void emitMOV64r(MCStreamer& out, unsigned reg, MCInst::const_iterator opStart, MCInst::const_iterator opEnd);
  void emitValidatedJump(MCStreamer &out);
  MCOperand buildExternalSymbolOperand(MCStreamer &out, const std::string &name);
  void addMemoryReference(MCStreamer &out, MCInst& instr, const unsigned baseReg,
    const unsigned indexReg, const unsigned scale, const int displacement);
  //void emitValidate();

public:
  HadeanExpander(const MCSubtargetInfo &info);
  bool expandInstruction(MCStreamer& out, const MCInst &instr);
};

}

#endif
