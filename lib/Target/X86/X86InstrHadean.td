let Defs = [EFLAGS] in {
  let isBranch = 1, isTerminator = 1, isBarrier = 1, isIndirectBranch = 1 in {
    def HAD_JMP64r : I<0, MRM4r, (outs), (ins GR64:$dst), "hadjmp\t{*}$dst",
                       [(brind GR64:$dst)], IIC_JMP_REG>, Requires<[In64BitMode, IsHadean]>,
                     Sched<[WriteJump]>;

    let isCodeGenOnly = 1 in {
      def HAD_JMP64m : I<0, Pseudo, (outs), (ins i64mem:$dst), "",
                         [(brind (loadi64 addr:$dst))], IIC_JMP_MEM>,
                       Requires<[In64BitMode, IsHadean]>, Sched<[WriteJumpLd]>;
    }
  }
}
