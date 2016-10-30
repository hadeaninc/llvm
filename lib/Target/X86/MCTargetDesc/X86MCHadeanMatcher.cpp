#include "X86MCHadeanMatcher.h"
#include "X86MCHadeanExpander.h"
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
  //FIXME: This needs to be made *much* smarter.
  if (ref.getOpcode() != provided.getOpcode())
    return false;

  if (ref.getNumOperands() != provided.getNumOperands())
    return false;

  return true;
}

HadeanMatcher::HadeanMatcher(const Triple &_triple) : triple(_triple) {
  std::string error;

  const Target *target = TargetRegistry::lookupTarget(triple.getTriple(), error);
  assert(target != nullptr);

  MCRegisterInfo *MRI = target->createMCRegInfo(triple.getTriple());
  assert(MRI != nullptr);

  MCAsmInfo *MAI = target->createMCAsmInfo(*MRI, triple.getTriple());
  assert(MAI != nullptr);

  MCContext *context = new MCContext(MAI, MRI, nullptr);
  assert(context != nullptr);

  holder.reset(new Holder(context));
  HadeanExpander expander;
  expander.emitValidatedJump(*holder);
  assert(holder->numInstructions() != 0);
}

void HadeanMatcher::feedInstruction(const MCInst &instr) {
  current.push_back(instr);

  if (current.size() > holder->numInstructions())
    current.pop_front();
}

bool HadeanMatcher::isValidatedJump() const {
  if (current.size() == holder->numInstructions()) {
    for(size_t i = 0; i < current.size(); ++i) {
      if (!matches(holder->getInstruction(i), current[i]))
        return false;
    }
    return true;
  } else {
    return false;
  }
}

}
