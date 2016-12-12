#include "X86MCHadeanMatcher.h"
#include "X86MCHadeanExpander.h"
#include "X86BaseInfo.h"
#include <llvm/Support/TargetRegistry.h>
#include <llvm/MC/MCContext.h>
#include <llvm/ADT/Triple.h>
#include <llvm/MC/MCInst.h>
#include <cassert>
#include <string>
#include <vector>
#include <memory>
#include <deque>

namespace llvm {

namespace {

Holder::Holder(MCContext *_context) : context(_context) {
}

void Holder::emitInstruction(const MCInst& instruction) {
  instructions.push_back(instruction);
}

void Holder::emitLabel(MCSymbol *symbol) {
  assert(symbol != nullptr);
}

MCContext &Holder::getContext() {
  return *context;
}

size_t Holder::numInstructions() const {
  return instructions.size();
}

const MCInst& Holder::getInstruction(const size_t index) const {
  assert(index < instructions.size());
  return instructions[index];
}

}

bool HadeanMatcher::matches(const MCInst &ref, const MCInst &provided) const {
  const unsigned relaxedRef = X86::getRelaxedOpcode(ref);
  const unsigned relaxedProvided = X86::getRelaxedOpcode(provided);

  if (relaxedRef != relaxedProvided)
    return false;

  if (ref.getNumOperands() != provided.getNumOperands())
    return false;

  for(size_t op = 0; op < ref.getNumOperands(); ++op) {
    if (!matches(ref.getOperand(op), provided.getOperand(op)))
        return false;
  }

  return true;
}

bool HadeanMatcher::matches(const MCOperand &ref, const MCOperand &provided) const {
  if (ref.isInst())
    return provided.isInst() && matches(*ref.getInst(), *provided.getInst());

  if (ref.isReg())
    return provided.isReg() && ref.getReg() == provided.getReg();

  if (ref.isFPImm())
    return provided.isFPImm() && ref.getFPImm() == provided.getFPImm();

  if (ref.isImm())
    return provided.isImm() && ref.getImm() == provided.getImm();

  //FIXME: This needs to resolve expressions to values
  if (ref.isExpr())
    return provided.isImm();

  assert(false && "Unhandled assembly operand in HadeanMatcher.");
  return false;
}

HadeanMatcher::HadeanMatcher(const Triple &_triple) : triple(_triple), progress(0) {
  std::string error;

  const Target *target = TargetRegistry::lookupTarget(triple.getTriple(), error); assert(target != nullptr);

  MCRegisterInfo *MRI = target->createMCRegInfo(triple.getTriple()); assert(MRI != nullptr);

  MCAsmInfo *MAI = target->createMCAsmInfo(*MRI, triple.getTriple()); assert(MAI != nullptr);

  MCContext *context = new MCContext(MAI, MRI, nullptr); assert(context != nullptr);

  holder.reset(new Holder(context));
  HadeanExpander expander;
  expander.emitValidatedJump(*holder);
  assert(holder->numInstructions() != 0);
}

enum HadeanMatcherState HadeanMatcher::feedInstruction(const MCInst &instr) {
  if (matches(holder->getInstruction(progress), instr)) {
    progress++;
    if (progress == holder->numInstructions()) {
      progress = 0;
      return HadeanMatcherStateValid;
    }
    return HadeanMatcherStateUnknown;
  } else {
    progress = 0;
    return HadeanMatcherStateInvalid;
  }
}

}
