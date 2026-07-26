# 后端代码生成规划（RISC-V 优先，图着色寄存器分配）

> 目标：在现有 MIR（目标无关机器 IR）之上，构建一个**健壮、快速**的后端，把 MIR 一路降到 RISC-V (rv64) 汇编，通过全部 functional / performance 测试样例。
> 原则：**接口一致、代码风格一致、不用特判 debug、不为通过样例牺牲优化力度**。ARM(AArch64) 作为第二目标复用同一套目标无关框架。

---

## 0. 现状盘点（起点）

当前流水线：

```
source → lexer → parser → AST → MLIR → BASEIR(SCEV/SR/GVN/CSE/SCCP…) → LLVM IR
       → MIR(目标无关机器IR) → 【本规划从这里开始】→ RISC-V 汇编(.s)
```

已有：
- **MIR 层**（`backend/mir/`）已相当完整：指令、操作数、module/function/block 结构、从 LLVM IR 的 lowering（`lowerhelper.cpp`）、`FrameInfo` 骨架、文本输出。
- **`backend/targets/utils/`** 只有基类骨架：`TargetInst / TargetBlock / TargetFunction / TargetOperand`（虚/物理寄存器、立即数、FrameIndex）、`TargetPrinter` 接口、`TargetRegClass/Size`。
- `-S`/`-s` 已在 `main.cpp` 的 `STAGES` 集合里声明，但**无代码路径**。
- 构建：`Makefile` 用 `find backend -name '*.cpp'` 自动收集源文件，新增文件即自动编译；`CXXFLAGS` 无 `-Werror`。

缺失（本规划要建的东西）：**指令选择、寄存器分配、栈帧/ABI、汇编输出、机器级 peephole**。

---

## 1. 总体架构：目标无关框架 + 目标特化

分层，把"能复用的"和"每架构一份的"彻底切开，保证以后加 AArch64 只写目标特化部分。

```
                MIR (虚拟寄存器, 目标无关)
                        │
        ┌───────────────┴───────────────┐
        │        目标无关 codegen 框架     │  backend/targets/utils/
        │  - TargetInst/Block/Function    │
        │  - GraphColoringAllocator       │  ← 图着色（复用）
        │  - RegisterInfo 接口            │  ← 目标提供寄存器文件描述
        │  - Frame/PEI 通用骨架           │
        │  - Peephole 框架                │
        └───────────────┬───────────────┘
                        │ 实例化
        ┌───────────────┴───────────────┐
        │   RISC-V 目标特化              │  backend/targets/riscv/
        │  - RISCVReg   (x0..x31/f0..f31)│
        │  - RISCVInst  (真实指令)        │
        │  - RISCVISel  (MIR→RISCVInst)  │
        │  - RISCVRegisterInfo           │
        │  - RISCVFrame (ABI/序言尾声)    │
        │  - RISCVPrinter (寄存器名/汇编) │
        └───────────────┬───────────────┘
                        │
                    RISC-V 汇编 (.s)
```

**代码生成主流水线**（`RISCVBackend::Run()`）：

```
1. Instruction Selection   MIR ──► RISCVInst（仍是虚拟寄存器）
2. (可选) Machine-level opt  局部窥孔、常量物化下沉、addressing 折叠
3. Register Allocation      图着色：build→coalesce→spill→color，虚→物理
4. Frame Lowering / PEI     栈帧定案、FrameIndex→sp偏移、序言/尾声、callee-saved 保存
5. Post-RA Peephole         mv 消除、冗余 load/store、分支化简
6. Assembly Emission        RISCVPrinter 输出 .s
```

> 关键点：**指令选择在寄存器分配之前**，选择阶段只用虚拟寄存器；物理寄存器约束（如除法、调用参数 a0–a7/fa0–fa7、返回值 a0）通过"预着色/固定物理寄存器 + 复制"的方式表达，交给分配器去合并（coalesce）。这样既不特判也不牺牲质量。

---

## 2. 指令选择（Instruction Selection）

### 2.1 策略
起步用**逐 MIR 指令的宏展开式选择（macro expansion / "简单但正确"）**，配合若干**模式匹配窥孔**做局部优化。不上 DAG/BURG，理由：MIR 已经是三地址、类型清晰、且中端已做了大量优化（SCEV/GVN/SR/SCCP），ISel 再上 tree pattern 收益递减、复杂度陡增、容易出 bug。参考 Cranelift 早期、教学编译器（北大编译实践、清华 miniSysY）的做法。

### 2.2 逐条降级规则（RISC-V rv64）
SysY 的 int 是 32 位、指针/地址运算 64 位、float 是 32 位单精度。**约定：int 用 `addw/subw/mulw/divw/remw/sllw/...` 的 W 变体保证 32 位回绕语义；地址/指针用 64 位 `add/addi`；float 用 `.s` 单精度指令**。这是健壮性的核心之一（避免 32/64 位混用导致的溢出错误）。

| MIR | RISC-V 选择 | 备注 |
|---|---|---|
| `Add/Sub`(int) | `addw/subw`，rhs 是小立即数→`addiw` | 立即数范围 [-2048,2047] |
| `Mul`(int) | `mulw` | 常量乘法在窥孔里换移位/加（`x*2^k`,`x*(2^k±1)`） |
| `Div/Mod`(int) | `divw/remw` | 常量除法用 magic number（见 §2.4） |
| `And/Or/Xor` | `and/or/xor`，立即数→`andi/ori/xori` | |
| `Shl/LShr/AShr` | `sllw/srlw/sraw`，立即数→`slliw/...` | |
| `ICmp` | `slt/sltu/slti` + `seqz/snez` 组合 | 与分支融合，见 §2.3 |
| `AddF/SubF/MulF/DivF` | `fadd.s/fsub.s/fmul.s/fdiv.s` | |
| `FCmp` | `feq.s/flt.s/fle.s` | |
| `Load/Store` | `lw/sw`（int/ptr 元素）/`flw/fsw`（float）/`ld/sd`（64位指针） | 地址=base+imm 折叠 |
| `Zext`(i1→i32) | 通常空操作（比较结果已是0/1） | |
| `Int2Float` | `fcvt.s.w` | |
| `Float2Int` | `fcvt.w.s`（rtz 截断，SysY 语义） | |
| `Br` | `j label` | |
| `CondBr` | 与前驱 `ICmp` 融合成 `beq/bne/blt/bge/bltu/bgeu` | 见 §2.3 |
| `Call` | 参数搬到 a0–a7/fa0–fa7 + 栈；`call sym`；返回值从 a0/fa0 取回 | ABI，见 §4 |
| `Ret` | 结果→a0/fa0；`j .epilogue` | |
| `GlobalDef` | `.data/.bss/.section` + `la` 取址 | 见 §5 |
| `alloca`(已在 MIR 化为 Frame) | 分配 FrameIndex 槽 | |
| `Phi` | 保留为 machine-SSA `PHI` pseudo，寄存器分配前统一消除（见 §3.4） | incoming 常量/地址按 CFG edge 延迟物化 |

### 2.3 比较/分支融合（重要，影响质量）
RISC-V 没有条件码。若把 `ICmp` 单独物化成 0/1 再 `bnez` 会多一条指令。做法：ISel 时若 `ICmp` 的**唯一 use 是紧跟的 `CondBr`**，则直接选成 `blt/bge/beq/bne/bltu/bgeu`（浮点用 `feq/flt/fle`+`bnez`）。否则物化成 0/1。判断"唯一 use 且相邻"用 MIR 的 use-list，不特判具体样例。

### 2.4 常量强度削弱（窥孔阶段）
- 乘常量：`x*C` → 移位/加/减序列（C 为 `2^k`、`2^k±1` 时最优；否则保留 `mulw`）。
- 除/模常量：有符号除以常量用 magic number 乘法（Granlund-Montgomery，见 Hacker's Delight 第 10 章 / LLVM `TargetLowering::BuildSDIV`）。这是 performance 样例拉分的关键。
- `addi` 立即数越界（|imm|>2047）→ `li` 到临时寄存器再 `add`。**立即数合法化（immediate legalization）统一在一个 helper 里做，任何指令构造都过它**，杜绝"某条指令忘了处理大立即数"这类 bug。

---

## 3. 寄存器分配（图着色，本次选定）

参考：Appel《Modern Compiler Implementation》第 11 章（Iterated Register Coalescing, George & Appel 1996）、LLVM `RegAllocGreedy` 思路、以及 Briggs 的乐观着色。

### 3.1 输入/输出
- 输入：Phi 消除后的 `TargetFunction`，指令操作数为虚拟寄存器（`TargetVirtualReg`）+ 少量预着色物理寄存器（参数/返回/除法约束）。
- 输出：全部替换为 `TargetPhysicalReg`；必要处插入 spill 的 load/store 与 FrameIndex 槽。

### 3.2 两套独立寄存器文件
整型 GPR 与浮点 FPR **分两张干扰图、各自独立着色**（reg class 不同不会互相干扰）。`RegisterInfo` 接口提供每个 class 的可分配物理寄存器集合。

RISC-V 可分配集合（ABI）：
- GPR 可分配：`t0–t6`, `a0–a7`, `s1–s11`（`s0=fp` 若用作帧指针则保留；`ra/sp/gp/tp/x0` 保留）。
- FPR 可分配：`ft0–ft11`, `fa0–fa7`, `fs0–fs11`。
- caller-saved（调用者保存，跨 call 失效）：`t*`, `a*`, `ft*`, `fa*`。
- callee-saved（被调用者保存）：`s*`, `fs*`, `ra`, `fp`。

### 3.3 图着色主循环（Iterated Register Coalescing）
```
loop:
  Build                  # 活跃分析 → 干扰图（含 move 边）
  MkWorklist             # 分 simplify/freeze/spill 三个工作表
  repeat:
    Simplify             # 移除低度数、非 move 相关结点
    Coalesce             # 保守合并 move（Briggs/George 判据）→ 消 mv
    Freeze               # 冻结低度数 move 相关结点
    SelectSpill          # 挑高度数结点乐观压栈
  until worklists empty
  AssignColors           # 出栈着色；着不上→实际 spill
  if actualSpills:
    RewriteProgram       # 为 spill 变量插 load/store，生成新短命虚寄存器
    goto loop
```
要点：
- **合并（coalescing）消除 `mv`**：把 phi 消除产生的 copy、ABI 参数 copy 尽量合并掉——这直接决定"不牺牲优化力度"。
- **溢出代价** = 使用/定义次数 / 度数，循环内乘 10^深度（用中端已有的 loop info / 深度）。
- **预着色结点**（物理寄存器）度数视为 ∞，不可 simplify/spill，只能被 coalesce 或作为约束。

### 3.4 Phi 消除（在寄存器分配之前）
LLVM IR → MIR → ISel 全程保留 `Phi`，让选择后的虚拟寄存器维持单定义；机器 SSA 优化完成后、寄存器分配之前，再把 `PHI` 用**并行复制（parallel copy）**在 CFG edge 上展开：
- 同一前驱多个 phi 的复制是**并行语义**，要处理相互覆盖 → 用经典的"并行复制串行化 + 必要时借临时寄存器打破环"（见 Boissinot 等 *Revisiting Out-of-SSA Translation*）。
- 关键边（critical edge）先拆分，避免复制放错块。
- incoming 常量、全局地址等不能提前放进整个前驱块；将物化指令绑定到 `(pred, succ)`，拆边后放入 edge block，避免在未走该边时推测执行。
- 复制统一走 `Copy`/`mv`，由分配器合并消除，不特判。
> 记忆点：项目里 `scf.while` 的 cond/body 共享 loop-carried arg（见 memory `while-shared-block-arg`），phi/复制处理要按块重建 use-list，避免踩到同类坑。

---

## 4. 栈帧与调用约定（RISC-V lp64d ABI）

### 4.1 栈帧布局（高地址→低地址）
```
 ┌──────────────────────┐  ← 上一帧
 │  incoming args (>8)  │   调用者放的溢出参数
 ├──────────────────────┤  ← sp (进入函数时) / fp
 │  ra (返回地址)        │
 │  saved fp (s0)        │
 │  callee-saved s*/fs* │   本函数用到的被调用者保存寄存器
 ├──────────────────────┤
 │  spill slots          │   寄存器分配溢出
 │  local arrays/allocas │   alloca / 局部数组
 ├──────────────────────┤
 │  outgoing args (>8)  │   本函数调用别人时的溢出参数区
 └──────────────────────┘  ← sp (函数体内)
```
- 16 字节对齐（ABI 要求）。
- 帧大小在寄存器分配后才能定（spill 槽数已知）→ **序言/尾声在 PEI 阶段用占位、末尾回填**（RISC-V 里 `addi sp,sp,-frame` 的 frame 是常量，越界则用 `li`+`sub`）。

### 4.2 参数传递
- 整型/指针：`a0–a7`，超出走栈（8 字节槽）。
- 浮点：`fa0–fa7`，超出走栈。
- 返回值：int/ptr→`a0`，float→`fa0`。
- **调用点**：把实参 move 到对应 arg 寄存器（用 Copy，交给 coalesce）；标记所有 caller-saved 在 call 处被 clobber（活跃跨 call 的值自然被分配器分到 callee-saved 或 spill）。

### 4.3 与 sylib 的对接
`getint/putint/getch/putarray/...` 已有 `librv.a`（`make librv`）。声明走 `FuncDeclare`，调用完全按上面 ABI，无需特殊处理。`starttime/stoptime` 宏映射到 `_sysy_starttime/_sysy_stoptime`（performance 计时），注意传行号常量。

---

## 5. 全局量与只读数据

- 有初值的 int 数组 → `.data`；全 0 → `.bss`（`.zero`）。
- float 全局/常量 → 以 32 位 bit pattern 存 `.word`；函数内 float 立即数放 `.rodata` 用 `la`+`flw` 取（RISC-V 无法直接 li 浮点）。或用整型寄存器 `li` bit pattern + `fmv.w.x`（更省一次访存，二选一在窥孔决定）。
- 取址统一 `la reg, sym`（medany 模型，配合 `make librv` 的 `-mcmodel=medany`）。

---

## 6. 机器级优化（不牺牲力度的地方）

按 ROI 排序，逐步加，每个都能开关、可回归：

1. **窥孔（peephole）**：`mv x,x` 删除、`addi x,x,0` 删除、`li`+算术折叠、连续 load/store 合并、`beqz`/`bnez` 化简、冗余分支到 fallthrough。
2. **常量除法/取模 magic number**（§2.4）——performance 大头。
3. **地址计算折叠**：`getptr`/GEP 序列折叠进 load/store 的立即数偏移；循环内不变地址提升（中端 SCEV/SR 已做大半，后端补 base+offset 折叠）。
4. **块布局/分支反转**：把最可能的后继排到 fallthrough，减少 `j`；简单的基于循环深度的排序。
5. **（进阶）指令调度**：rv64 顺序核收益有限，放最后；若上，用表驱动延迟做 list scheduling。
6. **（进阶）乘加融合、`mv` 传播、窄化（w 指令）后的符号扩展消除**。

> 纪律：每个优化都**默认参与、由 `-O` 档位统一控制**，不以"某样例过不了"为由塞特判。过不了就在 IR/选择/分配层面找根因。

---

## 7. 健壮性工程（"通过全部样例"的保障）

1. **立即数合法化 helper 唯一入口**：任何 `addi/lw/sw/...` 的立即数越界都在一处集中处理，杜绝散落特判。
2. **Verifier**：在 Phi 消除前、Phi 消除后、寄存器分配后三个边界运行。检查 machine SSA 单定义、Phi 连续且 incoming block 为真实 CFG 前驱、opcode arity/form、立即数范围、终结指令顺序、分配后无残留虚拟寄存器。
3. **两级对拍**：
   - functional：本编译器 `.s` → `riscv64 gcc` 汇编链接 `librv.a` → `qemu-riscv64` 跑 → 比对 `.out` 与返回码（SysY 用低 8 位返回码 + stdout）。
   - performance：同上并计时（`_sysy_starttime/stoptime`）。
   - 交叉对拍：与 `-O0` 结果比对，任何优化档位输出必须一致（差异即 bug）。
4. **分档 bring-up**：先 `-O0`（关掉所有机器级优化、线性但正确）打通全部 functional；再逐个打开优化跑回归。**先绿，再快，且快不破绿**。
5. **测试脚本**：复用 `script/functional`、`testoutputs/` 目录，加 RISC-V 跑测目标。

---

## 8. 里程碑（建议顺序）

- **M0 骨架（本次已搭）**：目录/基类接口/`RISCVBackend` 驱动/`-S` 代码路径/`AsmStage` 打通，`-S` 能产出占位汇编。
- **M1 最小可运行**：整型子集（算术/比较/分支/局部变量/`return`/`getint/putint`）+ 朴素分配（先能跑），端到端过若干 functional。
- **M2 图着色 + ABI 完整**：图着色分配器（含 coalesce/spill）、完整调用约定、数组/全局/指针、phi 消除并行复制。跑通**全部 functional**。
- **M3 浮点**：float 全链路（`.s` 指令、fa/fs、fcvt、float 常量池）。
- **M4 优化**：magic number 除法、常量乘移位、窥孔、地址折叠、块布局。跑 performance 并计时优化。
- **M5 AArch64**：复用框架，只写 `backend/targets/arm/`（Reg/Inst/ISel/Frame/Printer）。

---

## 9. 参考资料

- Appel, *Modern Compiler Implementation in C*, ch.11（Iterated Register Coalescing）。
- George & Appel, *Iterated Register Coalescing*, TOPLAS 1996。
- Boissinot et al., *Revisiting Out-of-SSA Translation for Correctness, Code Quality, and Efficiency*（phi 消除并行复制）。
- Warren, *Hacker's Delight*, ch.10（常量除法 magic number）；LLVM `TargetLowering::BuildSDIV/BuildUDIV`。
- Wimmer & Franz, *Linear Scan Register Allocation on SSA Form*（对照方案）。
- Cranelift / LLVM `RegAllocGreedy`、Chibicc、北大编译实践讲义、清华 miniSysY 后端文档（RISC-V ISel/ABI 实操）。
- RISC-V ISA Manual + *psABI (lp64d)*（调用约定权威来源）。

---

## 10. 目录约定（骨架已按此建立）

```
backend/targets/
  utils/                       # 目标无关（已有基类 + 本次新增框架）
    regalloc/
      registerinfo.h           # 抽象寄存器文件描述（目标实现）
      graphcoloring.h          # 图着色分配器（复用）
    ...(已有 inst/operand/struct 基类)
  riscv/                       # RISC-V 目标特化
    riscv.h / riscv.cpp        # RISCVBackend 驱动（跑完整 codegen 流水线）
    regs/riscvreg.h            # 物理寄存器枚举/ABI名/类别/保存约定
    printer/riscvprinter.h     # TargetPrinter 实现（寄存器名/汇编输出）
    isel/riscvisel.h           # 指令选择（MIR→RISCVInst）
    frame/riscvframe.h         # 栈帧/序言尾声/ABI
    inst/                      # RISC-V 具体指令（后续填充）
stages/asm/asm_stage.h/.cpp    # -S/-s 输出 stage
```
