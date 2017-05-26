#include "MCTargetDesc/X86MCHadeanExpander.h"

#include <cstdio>
#include <cassert>

#include <llvm/Support/CommandLine.h>
#include <llvm/MC/MCInst.h>
#include <llvm/MC/MCExpr.h>
#include <llvm/MC/MCContext.h>
#include <llvm/MC/MCInstBuilder.h>
#include <llvm/MC/MCStreamer.h>
#include <llvm/MC/MCSubtargetInfo.h>
#include "MCTargetDesc/X86MCTargetDesc.h"

#define GET_INSTRINFO_OPERAND_TYPES_ENUM
#include "X86GenInstrInfo.inc"

namespace llvm {

cl::opt<bool> EnableCFI("hadean-cfi",
                        cl::desc("Hadean CFI assembly instrumentation"),
                        cl::init(false));

cl::opt<bool> EnableMPX("hadean-mpx",
                        cl::desc("Hadean MPX assembly instrumentation"),
                        cl::init(true));

// To enable, pass '-mllvm --hadean-debug-cfi' to Clang.
// TODO: This is enaled by default for now. Disable before putting into production!.
cl::opt<bool> DebugCFI("hadean-debug-cfi",
                       cl::desc("Debug instrumentation for Hadean CFI"),
                       cl::init(true));

extern const char X86InstrNameData[];
extern const unsigned X86InstrNameIndices[];

static constexpr unsigned kReadOnlyBoundsReg = X86::BND2;
static constexpr unsigned kReadWriteBoundsReg = X86::BND3;
static constexpr unsigned kStackOpBoundsReg = kReadWriteBoundsReg;

static const char *GetName(const MCInst &inst) {
  return &X86InstrNameData[X86InstrNameIndices[inst.getOpcode()]];
}

const MCInstrDesc &HadeanExpander::GetDesc(const MCInst &inst) {
  return II_->get(inst.getOpcode());
}

static inline bool IsMemoryOperand(const MCInstrDesc &desc, unsigned opIndexMC) {
  return desc.OpInfo[opIndexMC].OperandType == MCOI::OPERAND_MEMORY;
}

static inline bool IsStackRegisterOperand(const MCInst &inst, unsigned opIndexMC) {
  const MCOperand &operand = inst.getOperand(opIndexMC);
  return operand.isReg() && (operand.getReg() == X86::RSP ||
                             operand.getReg() == X86::ESP ||
                             operand.getReg() == X86::SP  ||
                             operand.getReg() == X86::SPL);
}

static inline bool IsPrefixInstruction(const MCInst &inst) {
  switch (inst.getOpcode()) {
  case X86::CS_PREFIX:
  case X86::DATA16_PREFIX:
  case X86::DS_PREFIX:
  case X86::ES_PREFIX:
  case X86::FS_PREFIX:
  case X86::GS_PREFIX:
  case X86::LOCK_PREFIX:
  case X86::REPNE_PREFIX:
  case X86::REP_PREFIX:
  case X86::REX64_PREFIX:
  case X86::SS_PREFIX:
  case X86::XACQUIRE_PREFIX:
  case X86::XRELEASE_PREFIX:
    return true;
  default:
    return false;
  }
}

class RawEmitLock {
public:
  RawEmitLock(bool *value) : value_(value) {
    assert(*value_ == false);
    *value_ = true;
  }

  ~RawEmitLock() { *value_ = false; }

private:
  bool *value_;
};

struct MemoryOperand {
  MemoryOperand() : valid(false) {}

  void FromInst(const MCInst &inst, unsigned opIndexMC, int64_t offset) {
    base = inst.getOperand(opIndexMC + 0).getReg();
    scale = inst.getOperand(opIndexMC + 1).getImm();
    index = inst.getOperand(opIndexMC + 2).getReg();
    displacement = inst.getOperand(opIndexMC + 3).getImm() + offset;
    segment = inst.getOperand(opIndexMC + 4).getReg();
    valid = true;
  }

  void FromStackPtr(int64_t offset) {
    base = X86::RSP;
    scale = 1;
    index = 0;
    displacement = offset;
    segment = 0;
    valid = true;
  }

  void AppendTo(MCInstBuilder &builder) const {
    // ModRM can encode 1, 2 or 4 byte displacements.
    static constexpr int64_t kMinModRmDisplacement = std::numeric_limits<int32_t>::min();
    static constexpr int64_t kMaxModRmDisplacement = std::numeric_limits<int32_t>::max();

    if (displacement < kMinModRmDisplacement || kMaxModRmDisplacement < displacement) {
      // TODO: Handle large displacements.
      report_fatal_error("Displacement too large to encode");
    }

    builder.addReg(base);
    builder.addImm(scale);
    builder.addReg(index);
    builder.addImm(displacement);
    builder.addReg(segment);
  }

  bool IsValid() const { return valid; }

  bool valid;

  unsigned base;
  int64_t scale;
  unsigned index;
  int64_t displacement;
  unsigned segment;
};

static inline bool IsPush(const MCInst &inst, /* out */ int64_t *outSize = nullptr) {
  int64_t size;
  switch (inst.getOpcode()) {
    case X86::PUSH16r:
    case X86::PUSH16rmr: size = 2; break;

    case X86::PUSH64r:
    case X86::PUSH64rmr: size = 8; break;

    case X86::PUSH64i8:  size = 1; break;
    case X86::PUSH64i32: size = 4; break;

    case X86::PUSH16rmm:
    case X86::PUSH64rmm: report_fatal_error("PUSH from memory not valid on Hadean");

    case X86::PUSH32r:
    case X86::PUSH32rmr:
    case X86::PUSH32rmm: report_fatal_error("PUSH32 not valid on x86_64");

    default:             return false;
  }

  if (outSize != nullptr) {
    *outSize = size;
  }
  return true;
}

static inline bool IsPop(const MCInst &inst, /* out */ int64_t *outSize = nullptr) {
  int64_t size;
  switch (inst.getOpcode()) {
    case X86::POP16r:
    case X86::POP16rmr: size = 2; break;

    case X86::POP64r:
    case X86::POP64rmr: size = 8; break;

    case X86::POP16rmm:
    case X86::POP64rmm: report_fatal_error("POP from memory not valid on Hadean");

    case X86::POP32r:
    case X86::POP32rmr:
    case X86::POP32rmm: report_fatal_error("POP32 not valid on x86_64");

    default:          return false;
  }

  if (outSize != nullptr) {
    *outSize = size;
  }
  return true;
}

static inline unsigned GetMemoryOperandSize(const MCInst &inst, unsigned opIdxGen) {
  for (auto& entry : X86::OpTypes::InstOpInfoArray) {
    if (entry.Opcode == inst.getOpcode()) {
      switch (entry.OperandTypes[opIdxGen]) {
      case X86::OpTypes::i8mem:   return 1;
      case X86::OpTypes::i16mem:  return 2;
      case X86::OpTypes::f32mem:
      case X86::OpTypes::i32mem:  return 4;
      case X86::OpTypes::f64mem:
      case X86::OpTypes::i64mem:  return 8;
      case X86::OpTypes::f80mem:  return 10;
      case X86::OpTypes::f128mem:
      case X86::OpTypes::i128mem: return 16;
      case X86::OpTypes::f256mem:
      case X86::OpTypes::i256mem: return 32;
      case X86::OpTypes::f512mem:
      case X86::OpTypes::i512mem: return 64;
      default:
        report_fatal_error(std::string("Operand #") + std::to_string(opIdxGen) + " of instruction "
                           + GetName(inst) + " is not a recognized memory operand type");
      }
    }
  }
  report_fatal_error(std::string("Could not find operand type info for opcode ") + GetName(inst));
}

bool HadeanExpander::expandInstruction(MCStreamer &out, const MCInst &inst) {
  if (emitCompletelyRaw_) {
    return false;
  }

  if (!emitHadeanJump_) {
    RawEmitLock l(&emitHadeanJump_);
    if (HandleHadeanJump(out, inst)) {
      return true;
    }
  }

  if (prefixedInstruction_ == nullptr) {
    if (IsPrefixInstruction(inst)) {
      // Collect all prefix instructions so that we do not split them
      // from the main instruction.
      instructionPrefixes_.push_back(inst.getOpcode());
      // Return without emitting the prefix.
      return true;
    } else if (!instructionPrefixes_.empty()) {
      // This is the instruction that we have collected prefixes for.
      // Store a pointer to it so we can recognize it once it is being emitted.
      prefixedInstruction_ = &inst;
    }
  }

  if (EnableMPX && !emitWithoutMPX_MemoryAccess_) {
    RawEmitLock l(&emitWithoutMPX_MemoryAccess_);
    if (HandleMPX_MemoryAccess(out, inst)) {
      return true;
    }
  }

  if (EnableMPX && !emitWithoutMPX_StackPtrUpdate_) {
    RawEmitLock l(&emitWithoutMPX_StackPtrUpdate_);
    if (HandleMPX_StackPtrUpdate(out, inst)) {
      return true;
    }
  }

  if (EnableCFI && !emitWithoutCFI_) {
    RawEmitLock l(&emitWithoutCFI_);
    if (HandleCFI(out, inst)) {
      return true;
    }
  }

  // If we are here, we are about to emit `inst` raw.

  if (prefixedInstruction_ == &inst) {
    assert(!instructionPrefixes_.empty());

    // This instruction has prefixes which we have collected and
    // now need to emit them together with the instruction.

    RawEmitLock l(&emitCompletelyRaw_);

    out.EmitBundleLock(/* alignToEnd */ false);
    for (unsigned prefixOpcode : instructionPrefixes_) {
      out.EmitInstruction(MCInstBuilder(prefixOpcode), STI_);
    }
    out.EmitInstruction(inst, STI_);
    out.EmitBundleUnlock();

    instructionPrefixes_.clear();
    prefixedInstruction_ = nullptr;
    return true;
  }

  return false;
}

bool HadeanExpander::HandleHadeanJump(MCStreamer &out, const MCInst &inst) {
  switch (inst.getOpcode()) {
    case X86::HAD_JMP64r: {
      MCInstBuilder instJMP(X86::JMP64r);
      instJMP.addReg(inst.getOperand(0).getReg());
      out.EmitInstruction(instJMP, STI_);
      return true;
    }

    case X86::JMP64r:
    case X86::JMP64m:
      // This method is not invoked recursively on the lowered JMP instruction.
      // If we're seeing a JMP, it is coming from user's code.
      report_fatal_error("Found indirect 'jmp' instrution. Use 'hadjmp' instead");

    case X86::CALL64m:
      report_fatal_error("Found indirect 'call' with a memory operand. "
                         "Use the variant with a register operand instead");

    case X86::HAD_JMP64m:
      report_fatal_error("Found indirect 'hadjmp' with a memory operand. "
                         "Use the variant with a register operand instead");

    default:
      return false;
  }
}

bool HadeanExpander::HandleMPX_StackPtrUpdate(MCStreamer &out, const MCInst &inst) {
  const MCInstrDesc &desc = GetDesc(inst);

  bool foundStackPtrDef = false;
  for (unsigned i = 0; i < desc.getNumDefs(); ++i) {
    if (IsStackRegisterOperand(inst, i)) {
      assert(!foundStackPtrDef);
      foundStackPtrDef = true;
    }
  }

  if (foundStackPtrDef) {
    // Emit the original instruction, allowing it to update RSP, then check
    // the value is within bounds.

    out.EmitBundleLock(/* alignToEnd */ false);

    out.EmitInstruction(inst, STI_);

    MCInstBuilder instLower(X86::BNDCL64rr);
    instLower.addReg(kStackOpBoundsReg);
    instLower.addReg(X86::RSP);
    out.EmitInstruction(instLower, STI_);

    MCInstBuilder instUpper(X86::BNDCU64rr);
    instUpper.addReg(kStackOpBoundsReg);
    instUpper.addReg(X86::RSP);
    out.EmitInstruction(instUpper, STI_);

    out.EmitBundleUnlock();

    return true;
  } else {
    return false;
  }
}

bool HadeanExpander::HandleMPX_MemoryAccess(MCStreamer &out, const MCInst &inst) {
  const MCInstrDesc &desc = GetDesc(inst);
  if (!desc.mayLoad() && !desc.mayStore()) {
    return false;
  }

  int64_t size;
  unsigned boundsReg;
  MemoryOperand memAddrLower, memAddrUpper;

  if (IsPush(inst, &size)) {
    boundsReg = kStackOpBoundsReg;
    memAddrLower.FromStackPtr(-size);
  } else if (IsPop(inst, &size)) {
    boundsReg = kStackOpBoundsReg;
    memAddrUpper.FromStackPtr(size);
  } else {
    boundsReg = desc.mayStore() ? kReadWriteBoundsReg : kReadOnlyBoundsReg;
    // Iterate over all operands. Because memory operands are expanded to five
    // machine operands, we keep two indices, one into the high-level instruction
    // description and one into the MCInst operands list.
    for (unsigned idxMC = 0, idxGen = 0; idxMC < desc.getNumOperands(); ++idxMC, ++idxGen) {
      if (IsMemoryOperand(desc, idxMC)) {
        assert(!memAddrLower.IsValid() && !memAddrUpper.IsValid() && "Multiple memory operands");
        unsigned operandSize = GetMemoryOperandSize(inst, idxGen);
        memAddrLower.FromInst(inst, idxMC, 0);
        memAddrUpper.FromInst(inst, idxMC, operandSize - 1);
        idxMC += 4;
      }
    }
  }

  if (!memAddrLower.IsValid() && !memAddrUpper.IsValid()) {
    switch (inst.getOpcode()) {
    case X86::INT3:
    case X86::PAUSE:
    case X86::TRAP:
      return false;
    default:
      report_fatal_error(std::string("Instruction ") + GetName(inst) +
                         " with load/store semantics without a memory operand");
    }
  }

  out.EmitBundleLock(/* alignToEnd */ false);

  if (memAddrLower.IsValid()) {
    MCInstBuilder instLower(X86::BNDCL64rm);
    instLower.addReg(boundsReg);
    memAddrLower.AppendTo(instLower);
    out.EmitInstruction(instLower, STI_);
  }

  if (memAddrUpper.IsValid()) {
    MCInstBuilder instUpper(X86::BNDCU64rm);
    instUpper.addReg(boundsReg);
    memAddrUpper.AppendTo(instUpper);
    out.EmitInstruction(instUpper, STI_);
  }

  out.EmitInstruction(inst, STI_);
  out.EmitBundleUnlock();
  return true;
}

bool HadeanExpander::HandleCFI(MCStreamer &out, const MCInst &inst) {
  switch (inst.getOpcode()) {
    case X86::CALL64pcrel32: EmitDirectCall(out, inst);   break;
    case X86::CALL64r:       EmitIndirectCall(out, inst); break;
    case X86::JMP64r:
    case X86::TAILJMPr64:    EmitJump(out, inst);         break;
    case X86::RETQ:          EmitReturn(out, inst);       break;

    default:
      return false;
  }

  return true;
}

void HadeanExpander::EmitJump(MCStreamer &out, const MCInst& inst) {
  assert(inst.getOpcode() == X86::HAD_JMP64r || inst.getOpcode() == X86::TAILJMPr64);
  assert(inst.getNumOperands() == 1);
  assert(inst.getOperand(0).isReg());
  EmitSafeBranch(
      out, inst, /* opcode */ X86::JMP64r, inst.getOperand(0).getReg(), /* alignToEnd */ false);
}

void HadeanExpander::EmitDirectCall(MCStreamer &out, const MCInst &inst) {
  assert(inst.getOpcode() == X86::CALL64pcrel32);
  out.EmitBundleLock(/* alignToEnd */ true);  // CALLs must be at the end of a bundle
  out.EmitInstruction(inst, STI_);
  out.EmitBundleUnlock();
}

void HadeanExpander::EmitIndirectCall(MCStreamer &out, const MCInst &inst) {
  assert(inst.getOpcode() == X86::CALL64r);
  assert(inst.getNumOperands() == 1);
  assert(inst.getOperand(0).isReg());
  EmitSafeBranch(
      out, inst, /* opcode */ X86::CALL64r, inst.getOperand(0).getReg(), /* alignToEnd */ true);
}

void HadeanExpander::EmitReturn(MCStreamer &out, const MCInst &inst) {
  assert(inst.getOpcode() == X86::RETQ);
  static constexpr unsigned kTempReg = X86::R11;  // register not preserved across function calls

  MCInstBuilder instPOP(X86::POP64r);
  instPOP.addReg(kTempReg);
  out.EmitInstruction(instPOP, STI_);

  MCInstBuilder instJMP(X86::JMP64r);
  instJMP.addReg(kTempReg);
  EmitSafeBranch(out, instJMP, /* opcode */ X86::JMP64r, kTempReg, /* alignToEnd */ false);
}

void HadeanExpander::EmitSafeBranch(MCStreamer &out,
                                    const MCInst &inst,
                                    unsigned opcode,
                                    unsigned targetReg,
                                    bool alignToEnd) {
  if (DebugCFI) {
    // Do an AND $31 on `targetReg`. Trap if the bottom five bits are not zero.
    MCContext &context = out.getContext();
    MCSymbol *labelFine = context.createTempSymbol();

    // Backup the target register.
    MCInstBuilder instPUSH(X86::PUSH64r);
    instPUSH.addReg(targetReg);
    out.EmitInstruction(instPUSH, STI_);

    // Mask out all but the bottom bits that should be zero.
    MCInstBuilder instAND(X86::AND64ri32);
    instAND.addReg(targetReg);
    instAND.addReg(targetReg);
    instAND.addImm(kBundleSizeInBytes - 1);
    out.EmitInstruction(instAND, STI_);

    // Skip over the TRAP if the zero flag is set.
    MCInstBuilder instSKIP(X86::JE_4);
    instSKIP.addExpr(MCSymbolRefExpr::create(labelFine, context));
    out.EmitInstruction(instSKIP, STI_);

    // If not zero, trap.
    out.EmitInstruction(MCInstBuilder(X86::TRAP), STI_);

    // Target of skipping the TRAP instruction.
    out.EmitLabel(labelFine);

    // Restore the clobbered target register.
    MCInstBuilder instPOP(X86::POP64r);
    instPOP.addReg(targetReg);
    out.EmitInstruction(instPOP, STI_);
  }

  {
    out.EmitBundleLock(alignToEnd);

    // Mask out bottom bits of the pointer. The immediate will be replaced
    // by elf2hoff to mask out the top bits as well.
    MCInstBuilder instAND(X86::AND64ri32);
    instAND.addReg(targetReg);
    instAND.addReg(targetReg);
    instAND.addImm(-kBundleSizeInBytes);
    out.EmitInstruction(instAND, STI_);

    // Add the base from the reserved register.
    MCInstBuilder instLEA(X86::LEA64r);
    instLEA.addReg(targetReg);
    instLEA.addReg(kCodeBaseRegister);
    instLEA.addImm(1);
    instLEA.addReg(targetReg);
    instLEA.addImm(0);
    instLEA.addReg(0);
    out.EmitInstruction(instLEA, STI_);

    // Emit the branch instruction
    MCInstBuilder instBranch(opcode);
    instBranch.addReg(targetReg);
    out.EmitInstruction(instBranch, STI_);

    out.EmitBundleUnlock();
  }
}

}  // namespace llvm
