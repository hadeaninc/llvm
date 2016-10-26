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
#include "MCTargetDesc/X86MCHadean.h"
#include "MCTargetDesc/X86MCTargetDesc.h"

namespace llvm {

HadeanExpander::HadeanExpander(const MCSubtargetInfo &_subtargetInfo) :
  // subtargetInfo(_subtargetInfo), xorValue(0) {
  subtargetInfo(_subtargetInfo), xorValue(0xf0f0f0f0f0f0f0f0) {
}

bool HadeanExpander::expandInstruction(MCStreamer &out, const MCInst &instr) {
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

void HadeanExpander::emitHadeanCall(MCStreamer &out, const MCInst::const_iterator opBegin, const MCInst::const_iterator opEnd) {
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
    out.EmitInstruction(movBuilder, subtargetInfo);
  }

  // Move return address to stack
  emitPUSH64r(out, BTR);

  // Move obfuscated target to BTR
  emitMOV64r(out, BTR, opBegin, opEnd);

  // Decode, validate and jump to target
  emitValidatedJump(out);

  // Return from function call
  out.EmitLabel(labelReturn);
}

void HadeanExpander::emitHadeanRet(MCStreamer &out) {
  emitPOP64r(out, BTR);
  emitValidatedJump(out);
}

void HadeanExpander::emitHadeanJump(MCStreamer &out, const MCInst::const_iterator opBegin, const MCInst::const_iterator opEnd) {
  emitMOV64r(out, BTR, opBegin, opEnd);
  emitValidatedJump(out);
}

void HadeanExpander::emitMOV64r(MCStreamer& out, unsigned reg, const MCInst::const_iterator opBegin, const MCInst::const_iterator opEnd)
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
    out.EmitInstruction(movBuilder, subtargetInfo);
  } else {
    llvm_unreachable("Unhandled operand type for indirect jump target.");
  }
}

void HadeanExpander::emitValidatedJump(MCStreamer &out) {
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
    out.EmitInstruction(builder, subtargetInfo);
  }
  emitCMP64rr(out, scratch, BTR);
  {
    MCInstBuilder builder(X86::JL_1);
    builder.addExpr(labelFailExpr);
    out.EmitInstruction(builder, subtargetInfo);
  }

  // Load text end
  {
    MCInstBuilder builder(X86::MOV64ri);
    builder.addReg(scratch);
    builder.addOperand(buildExternalSymbolOperand(out, "__etext"));
    out.EmitInstruction(builder, subtargetInfo);
  }
  emitCMP64rr(out, scratch, BTR);
  {
    MCInstBuilder builder(X86::JGE_1);
    builder.addExpr(labelFailExpr);
    out.EmitInstruction(builder, subtargetInfo);
  }

  // Restore scratch
  emitPOP64r(out, scratch);

  // The indirect jump
  MCInstBuilder jumpBuilder(X86::JMP64r);
  jumpBuilder.addReg(BTR);
  out.EmitInstruction(jumpBuilder, subtargetInfo);

  // Failure
  out.EmitLabel(labelFail);

  const bool useExitFunction = false;
  if (useExitFunction) {
    MCInstBuilder movBuilder(X86::MOV32ri);
    movBuilder.addReg(X86::EDI);
    movBuilder.addImm(13);
    out.EmitInstruction(movBuilder, subtargetInfo);

    MCInstBuilder callBuilder(X86::CALL64pcrel32);
    callBuilder.addOperand(buildExternalSymbolOperand(out, "exit"));
    out.EmitInstruction(callBuilder, subtargetInfo);
  } else {
    MCInstBuilder jumpBuilder(X86::JMP_4);
    jumpBuilder.addOperand(buildExternalSymbolOperand(out, "__hadean_invalid_jump"));
    out.EmitInstruction(jumpBuilder, subtargetInfo);
  }
}

void HadeanExpander::emitXOR64rr(MCStreamer &out, unsigned destReg, unsigned opReg) {
  MCInstBuilder builder(X86::XOR64rr);
  builder.addReg(destReg);
  builder.addReg(destReg);
  builder.addReg(opReg);
  out.EmitInstruction(builder, subtargetInfo);
}

void HadeanExpander::emitAND64rr(MCStreamer &out, unsigned destReg, unsigned opReg) {
  MCInstBuilder builder(X86::AND64rr);
  builder.addReg(destReg);
  builder.addReg(destReg);
  builder.addReg(opReg);
  out.EmitInstruction(builder, subtargetInfo);
}

void HadeanExpander::emitMOV64rr(MCStreamer &out, unsigned dst, unsigned src) {
  MCInstBuilder builder(X86::MOV64rr);
  builder.addReg(dst);
  builder.addReg(src);
  out.EmitInstruction(builder, subtargetInfo);
}

void HadeanExpander::emitPUSH64r(MCStreamer &out, const unsigned reg) {
  MCInstBuilder builder(X86::PUSH64r);
  builder.addReg(reg);
  out.EmitInstruction(builder, subtargetInfo);
}

void HadeanExpander::emitPOP64r(MCStreamer& out, const unsigned reg) {
  MCInstBuilder builder(X86::POP64r);
  builder.addReg(reg);
  out.EmitInstruction(builder, subtargetInfo);
}

void HadeanExpander::emitCMP64rr(MCStreamer &out, unsigned r1, unsigned r2) {
  MCInstBuilder builder(X86::CMP64rr);
  builder.addReg(r2);
  builder.addReg(r1);
  out.EmitInstruction(builder, subtargetInfo);
}

void HadeanExpander::emitMOV64ri(MCStreamer &out, const unsigned reg, const uint64_t immediate) {
  MCInstBuilder builder(X86::MOV64ri);
  builder.addReg(reg);
  builder.addImm(immediate);
  out.EmitInstruction(builder, subtargetInfo);
}

void HadeanExpander::emitROL64ri(MCStreamer &out, const unsigned reg, const uint64_t immediate) {
  MCInstBuilder builder(X86::ROL64ri);
  builder.addReg(reg);
  builder.addReg(reg);
  builder.addImm(immediate);
  out.EmitInstruction(builder, subtargetInfo);
}

MCOperand HadeanExpander::buildExternalSymbolOperand(MCStreamer &out, const std::string &name) {
  MCContext &context = out.getContext();
  MCSymbol *sym = context.getOrCreateSymbol(name);
  assert(sym != nullptr);
  const MCSymbolRefExpr *symExpr = MCSymbolRefExpr::create(sym, context);
  assert(symExpr != nullptr);
  out.EmitSymbolAttribute(sym, MCSA_ELF_TypeNoType);
  return MCOperand::createExpr(symExpr);
}

void HadeanExpander::addMemoryReference(MCStreamer &out, MCInst& instr, const unsigned baseReg,
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
