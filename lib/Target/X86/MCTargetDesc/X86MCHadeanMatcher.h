#ifndef X86MCHADEANMATCHER_H
#define X86MCHADEANMATCHER_H

#include <llvm/ADT/Triple.h>
#include <llvm/MC/MCInst.h>
#include <memory>
#include <deque>
#include "X86MCHadeanExpander.h"

namespace llvm {

namespace {

class Holder : public MCOutputTarget {
private:
  std::unique_ptr<MCContext> context;
  std::vector<MCInst> instructions;

public:
  Holder(MCContext *context);
  void emitInstruction(const MCInst& instruction) override;
  void emitLabel(MCSymbol *symbol) override;
  MCContext &getContext() override;
  size_t numInstructions() const;
  const MCInst& getInstruction(const size_t index) const;
};

}

class HadeanMatcher {
private:
  Triple triple;
  std::unique_ptr<Holder> holder;
  std::deque<MCInst> current;
  bool matches(const MCInst &ref, const MCInst &provided) const;

public:
  HadeanMatcher(const Triple& triple);
  void feedInstruction(const MCInst &instr);
  bool isValidatedJump() const;
};

}

#endif
