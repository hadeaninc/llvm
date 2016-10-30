#ifndef X86MCHADEANEXPANDER_H
#define X86MCHADEANEXPANDER_H

#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCStreamer.h"

namespace llvm {

class MCOutputTarget {
public:
  virtual void emitInstruction(const MCInst& instruction) = 0;
  virtual void emitLabel(MCSymbol *symbol) = 0;
  virtual MCContext &getContext() = 0;
  virtual ~MCOutputTarget() {}
};

class MCOutputTargetStreamer : public MCOutputTarget {
private:
  MCStreamer &out;
  MCSubtargetInfo &subtargetInfo;

public:
  static MCOutputTargetStreamer *create(MCStreamer& streamer, MCSubtargetInfo &info);

  MCOutputTargetStreamer(MCStreamer& out, MCSubtargetInfo &subtargetInfo);
  virtual void emitInstruction(const MCInst& instruction) override;
  virtual void emitLabel(MCSymbol *symbol) override;
  virtual MCContext &getContext() override;
};

class HadeanExpander
{
private:
  // The branch target register reserved for indirect jumps
  static const unsigned BTR;

  const uint64_t xorValue;

  void emitHadeanRet(MCOutputTarget &out);
  void emitHadeanJump(MCOutputTarget &out, MCInst::const_iterator opStart, MCInst::const_iterator opEnd);
  void emitHadeanCall(MCOutputTarget &out, MCInst::const_iterator opStart, MCInst::const_iterator opEnd);
  void emitPUSH64r(MCOutputTarget &out, unsigned reg);
  void emitPOP64r(MCOutputTarget &out, unsigned reg);
  void emitPUSHXMMr(MCOutputTarget &out, unsigned reg);
  void emitPOPXMMr(MCOutputTarget &out, unsigned reg);
  void emitMOV64ri(MCOutputTarget &out, unsigned reg, uint64_t value);
  void emitROL64ri(MCOutputTarget &out, unsigned reg, uint64_t value);
  void emitMOV64rr(MCOutputTarget &out, unsigned dst, unsigned src);
  void emitXOR64rr(MCOutputTarget &out, unsigned destReg, unsigned opReg);
  void emitAND64rr(MCOutputTarget &out, unsigned destReg, unsigned opReg);
  void emitCMP64rr(MCOutputTarget &out, unsigned r1, unsigned r2);
  void emitMOV64r(MCOutputTarget& out, unsigned reg, MCInst::const_iterator opStart, MCInst::const_iterator opEnd);
  MCSymbol *buildExternalSymbol(MCOutputTarget &out, const std::string &name);
  const MCExpr *buildExternalSymbolExpr(MCOutputTarget &out, const std::string &name);
  void addMemoryReference(MCOutputTarget &out, MCInst& instr, const unsigned baseReg,
    const unsigned indexReg, const unsigned scale, const int displacement);

public:
  HadeanExpander();
  bool expandInstruction(MCOutputTarget& out, const MCInst &instr);
  void emitValidatedJump(MCOutputTarget &out);
};

}

#endif
