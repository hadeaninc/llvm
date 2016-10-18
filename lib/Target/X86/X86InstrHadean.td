class HadeanPI64<dag outs, dag ins, string asm>
  : I<0, CustomFrm, outs, ins, asm, []>, Requires<[IsHadean, In64BitMode]>;


let isCall = 1, isAsmParserOnly = 1, Uses = [RSP] in {
  def HAD_CALL64r       : HadeanPI64<(outs), (ins GR64:$dst),
                        "hadcall{q}\t{*}$dst">;
}

let isTerminator = 1, isReturn = 1, isBarrier = 1,
    hasCtrlDep = 1, FPForm = SpecialFP, isAsmParserOnly = 1 in {
  def HAD_RET64  : HadeanPI64<(outs), (ins), "hadret">;
}

let isBranch = 1, isTerminator = 1, isBarrier = 1, isIndirectBranch = 1, isAsmParserOnly = 1 in {
  def HAD_JMP64r : HadeanPI64<(outs), (ins GR64:$dst),
                    "hadjmp\t{$dst}">;
}

