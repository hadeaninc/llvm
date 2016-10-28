#ifndef X86MCHADEANMATCHER_H
#define X86MCHADEANMATCHER_H

#include <llvm/ADT/Triple.h>
#include <llvm/MC/MCInst.h>
#include "X86MCHadeanExpander.h"

namespace llvm {

namespace {
  class Holder;
}

class HadeanMatcher {
private:
  Triple triple;
  std::unique_ptr<Holder> holder;
  size_t index;
  bool matches(const MCInst &ref, const MCInst &provided);

public:
  HadeanMatcher(const Triple& triple);
  void feedInstruction(const MCInst &instr);
  bool isValidatedJump() const;
};

}

#endif
