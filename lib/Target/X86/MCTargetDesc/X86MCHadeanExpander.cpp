#ifndef X86_HADEAN_H
#define X86_HADEAN_H

#include <cstdio>
#include <cassert>
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCInstBuilder.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "MCTargetDesc/X86MCHadeanExpander.h"
#include "MCTargetDesc/X86MCTargetDesc.h"

namespace llvm {

class MCOutputTargetStreamer : public MCOutputTarget {
private:
  MCStreamer &out;
  MCSubtargetInfo &subtargetInfo;

public:
  MCOutputTargetStreamer(MCStreamer& out, MCSubtargetInfo &subtargetInfo);
  virtual void emitInstruction(const MCInst& instruction) override;
  virtual void emitLabel(MCSymbol *symbol) override;
  virtual MCContext &getContext() override;
};

MCOutputTargetStreamer::MCOutputTargetStreamer(MCStreamer &_out, MCSubtargetInfo &_subtargetInfo) :
  out(_out), subtargetInfo(_subtargetInfo) {
}

void MCOutputTargetStreamer::emitInstruction(const MCInst& instr) {
  out.EmitInstruction(instr, subtargetInfo);
}

void MCOutputTargetStreamer::emitLabel(MCSymbol *sym) {
  out.EmitLabel(sym);
}

MCContext &MCOutputTargetStreamer::getContext() {
  return out.getContext();
}

HadeanExpander::HadeanExpander() :
  xorValue(0xf0f0f0f0f0f0f0f0) {
  //xorValue(0) {
}

MCOutputTarget *HadeanExpander::createMCStreamerOutput(MCStreamer &streamer, MCSubtargetInfo &info) {
  return new MCOutputTargetStreamer(streamer, info);
}

bool HadeanExpander::expandInstruction(MCOutputTarget &out, const MCInst &instr) {
  const unsigned opcode = instr.getOpcode();

  switch (opcode)
  {
    case X86::HAD_CALL64r:
    case X86::HAD_CALL64m: {
      emitHadeanCall(out, instr.begin(), instr.end());
      return true;
    }
    case X86::HAD_RET64: {
      emitHadeanRet(out);
      return true;
    }
    case X86::HAD_JMP64r:
    case X86::HAD_JMP64m: {
      emitHadeanJump(out, instr.begin(), instr.end());
      return true;
    }
    default:
      return false;
  }
}

void HadeanExpander::emitHadeanCall(MCOutputTarget &out, const MCInst::const_iterator opBegin, const MCInst::const_iterator opEnd) {
  MCContext &context = out.getContext();

  // Create return label
  MCSymbol *labelReturn = context.createTempSymbol();
  assert(labelReturn != nullptr);
  const MCExpr *labelReturnExpr = MCSymbolRefExpr::create(labelReturn, context);
  assert(labelReturnExpr != NULL);

  // Move return address to BTR
  {
    MCInstBuilder movBuilder(X86::MOV64ri);
    movBuilder.addReg(BTR);
    movBuilder.addExpr(labelReturnExpr);
    out.emitInstruction(movBuilder);
  }

  // Move return address to stack
  emitPUSH64r(out, BTR);

  // Move obfuscated target to BTR
  emitMOV64r(out, BTR, opBegin, opEnd);

  // Decode, validate and jump to target
  emitValidatedJump(out);

  // Return from function call
  out.emitLabel(labelReturn);
}

void HadeanExpander::emitHadeanRet(MCOutputTarget &out) {
  emitPOP64r(out, BTR);
  emitValidatedJump(out);
}

void HadeanExpander::emitHadeanJump(MCOutputTarget &out, const MCInst::const_iterator opBegin, const MCInst::const_iterator opEnd) {
  emitMOV64r(out, BTR, opBegin, opEnd);
  emitValidatedJump(out);
}

void HadeanExpander::emitMOV64r(MCOutputTarget& out, unsigned reg, const MCInst::const_iterator opBegin, const MCInst::const_iterator opEnd)
{
  if (opBegin == opEnd) {
    llvm_unreachable("No operand!");
  } else if (std::next(opBegin) == opEnd && opBegin->isReg()) {
    emitMOV64rr(out, BTR, opBegin->getReg());
  } else if (opEnd - opBegin == 5) { // Memory accesses have 5 operands
    MCInstBuilder movBuilder(X86::MOV64rm);
    movBuilder.addReg(BTR);
    for(MCInst::const_iterator opIter = opBegin; opIter != opEnd; ++opIter)
      movBuilder.addOperand(*opIter);
    out.emitInstruction(movBuilder);
  } else {
    llvm_unreachable("Unhandled operand type for indirect jump target.");
  }
}

void HadeanExpander::emitValidatedJump(MCOutputTarget &out) {
  MCContext &context = out.getContext();

  const unsigned scratch = X86::RAX;

  // Backup scratch
  emitPUSH64r(out, scratch);

  const bool smarterObfuscation = false;
  if (!smarterObfuscation) {
    // Decode the branch address
    emitMOV64ri(out, scratch, xorValue);
    emitXOR64rr(out, BTR, scratch);
  } else {
    const unsigned maskReg = scratch;
    const unsigned maskedReg = X86::RBX;
    assert(BTR != maskedReg);
    assert(BTR != maskReg);
    assert(maskReg != maskedReg);

    emitPUSH64r(out, maskedReg);
    emitMOV64ri(out, maskReg, 0x5555555555555555ull);

    for(int round = 0; round < 5; ++round) {
      const int shift = (2 << round) - 1;
      for(int i = 0; i < 2; ++i) {
        emitMOV64rr(out, maskedReg, BTR);
        emitAND64rr(out, maskedReg, maskReg);
        emitROL64ri(out, BTR, shift);
        emitXOR64rr(out, BTR, maskedReg);
      }
    }

    emitPOP64r(out, maskedReg);
  }

  // Create failure label
  MCSymbol *labelFail = context.createTempSymbol();
  assert(labelFail != nullptr);
  const MCExpr *labelFailExpr = MCSymbolRefExpr::create(labelFail, context);
  assert(labelFailExpr != NULL);

  // Load text start
  {
    MCInstBuilder builder(X86::MOV64ri);
    builder.addReg(scratch);
    builder.addOperand(buildExternalSymbolOperand(out, "__executable_start"));
    out.emitInstruction(builder);
  }
  emitCMP64rr(out, scratch, BTR);
  {
    MCInstBuilder builder(X86::JL_1);
    builder.addExpr(labelFailExpr);
    out.emitInstruction(builder);
  }

  // Load text end
  {
    MCInstBuilder builder(X86::MOV64ri);
    builder.addReg(scratch);
    builder.addOperand(buildExternalSymbolOperand(out, "__etext"));
    out.emitInstruction(builder);
  }
  emitCMP64rr(out, scratch, BTR);
  {
    MCInstBuilder builder(X86::JGE_1);
    builder.addExpr(labelFailExpr);
    out.emitInstruction(builder);
  }

  // Restore scratch
  emitPOP64r(out, scratch);

  // The indirect jump
  MCInstBuilder jumpBuilder(X86::JMP64r);
  jumpBuilder.addReg(BTR);
  out.emitInstruction(jumpBuilder);

  // Failure
  out.emitLabel(labelFail);

  const bool useExitFunction = false;
  if (useExitFunction) {
    MCInstBuilder movBuilder(X86::MOV32ri);
    movBuilder.addReg(X86::EDI);
    movBuilder.addImm(13);
    out.emitInstruction(movBuilder);

    MCInstBuilder callBuilder(X86::CALL64pcrel32);
    callBuilder.addOperand(buildExternalSymbolOperand(out, "exit"));
    out.emitInstruction(callBuilder);
  } else {
    emitPUSH64r(out, BTR);
    MCInstBuilder jumpBuilder(X86::JMP_4);
    jumpBuilder.addOperand(buildExternalSymbolOperand(out, "__hadean_invalid_jump"));
    out.emitInstruction(jumpBuilder);
  }
}

void HadeanExpander::emitXOR64rr(MCOutputTarget &out, unsigned destReg, unsigned opReg) {
  MCInstBuilder builder(X86::XOR64rr);
  builder.addReg(destReg);
  builder.addReg(destReg);
  builder.addReg(opReg);
  out.emitInstruction(builder);
}

void HadeanExpander::emitAND64rr(MCOutputTarget &out, unsigned destReg, unsigned opReg) {
  MCInstBuilder builder(X86::AND64rr);
  builder.addReg(destReg);
  builder.addReg(destReg);
  builder.addReg(opReg);
  out.emitInstruction(builder);
}

void HadeanExpander::emitMOV64rr(MCOutputTarget &out, unsigned dst, unsigned src) {
  MCInstBuilder builder(X86::MOV64rr);
  builder.addReg(dst);
  builder.addReg(src);
  out.emitInstruction(builder);
}

void HadeanExpander::emitPUSH64r(MCOutputTarget &out, const unsigned reg) {
  MCInstBuilder builder(X86::PUSH64r);
  builder.addReg(reg);
  out.emitInstruction(builder);
}

void HadeanExpander::emitPOP64r(MCOutputTarget& out, const unsigned reg) {
  MCInstBuilder builder(X86::POP64r);
  builder.addReg(reg);
  out.emitInstruction(builder);
}

void HadeanExpander::emitCMP64rr(MCOutputTarget &out, unsigned r1, unsigned r2) {
  MCInstBuilder builder(X86::CMP64rr);
  builder.addReg(r2);
  builder.addReg(r1);
  out.emitInstruction(builder);
}

void HadeanExpander::emitMOV64ri(MCOutputTarget &out, const unsigned reg, const uint64_t immediate) {
  MCInstBuilder builder(X86::MOV64ri);
  builder.addReg(reg);
  builder.addImm(immediate);
  out.emitInstruction(builder);
}

void HadeanExpander::emitROL64ri(MCOutputTarget &out, const unsigned reg, const uint64_t immediate) {
  MCInstBuilder builder(X86::ROL64ri);
  builder.addReg(reg);
  builder.addReg(reg);
  builder.addImm(immediate);
  out.emitInstruction(builder);
}

MCOperand HadeanExpander::buildExternalSymbolOperand(MCOutputTarget &out, const std::string &name) {
  MCContext &context = out.getContext();
  MCSymbol *sym = context.getOrCreateSymbol(name);
  assert(sym != nullptr);
  const MCSymbolRefExpr *symExpr = MCSymbolRefExpr::create(sym, context);
  assert(symExpr != nullptr);
  //out.EmitSymbolAttribute(sym, MCSA_ELF_TypeNoType);
  return MCOperand::createExpr(symExpr);
}

void HadeanExpander::addMemoryReference(MCOutputTarget &out, MCInst& instr, const unsigned baseReg,
    const unsigned indexReg, const unsigned scale, const int displacement) {
  const unsigned segReg = 0;
  instr.addOperand(MCOperand::createReg(baseReg));
  instr.addOperand(MCOperand::createImm(scale));
  instr.addOperand(MCOperand::createReg(indexReg));
  instr.addOperand(MCOperand::createImm(displacement));
  instr.addOperand(MCOperand::createReg(segReg));
}

// This is reserved by Hadean. Don't change it!
const unsigned HadeanExpander::BTR = X86::R11;

}

#endif
