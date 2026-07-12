# AArch64（ARMv8-A）后端设计报告

> 第二个后端。目标：把已经跑通竞赛全部样例的 rv64 后端框架，复用到 AArch64（ARMv8-A，LP64）。
> 复用同一套 MIR、同一套目标无关框架（TargetContext/Inst/Block/Function、图着色分配器核心、机器级 pass 框架、RegisterInfo 接口），只新增 AArch64 目标特化层。
> 约束沿用 rv64：**保证通过全部测试样例、尽可能覆盖 SysY 更多情况、保持接口一致、保持代码风格一致、不牺牲优化力度去 debug、不使用特殊判断去 debug。**

---

## 0. 现状盘点（起点）

- rv64 后端已完成并通过竞赛全量样例（functional 100/100、h_functional 40/40、riscv_performance 57/57、riscv_finalperformance 60/60，-O0/-O1/-O2 全绿）。
- 目标无关框架已就位于 `backend/targets/utils/`：
  - `context/TargetContext`：拥有全部 TargetInst/Operand/Block/Function 的工厂（`new`+`unique_ptr` 持有）。
  - `inst/TargetInst`、`operand/TargetOperand`（VirtualReg/PhysicalReg/IntLiteral/Label/Symbol）、`struct/TargetBlock`、`struct/TargetFunction`。
  - `regalloc/RegisterInfo`：目标寄存器文件描述接口（可分配集合、caller/callee-saved）。
  - `pass/MachineFunctionPass` + `MachinePassManager`：机器级 pass 框架（跑到不动点、按 -O 等级开关）。
  - `operand/TargetPrinter`：物理寄存器名/汇编渲染接口。
- 图着色分配器（Iterated Register Coalescing）目前**具体绑定在 `RISCVInst`** 上，位于 `backend/targets/riscv/regalloc/`。复用到 AArch64 需要先做一次**小重构**，把与目标相关的 3 处钩子抽出去（见 §3）。
- 中端 LLVM-IR→MIR 的降级、phi 消除（含并行拷贝、关键边拆分）都是目标无关的，AArch64 直接复用，**无需改中端**。

### 关键前提：AArch64 与 rv64 的宽度约定一致

MIR 里 int 是 32 位、指针/地址是 64 位、float 是 32 位单精度。这套约定在 AArch64 上**天然成立**：

- 32 位整数 → `w` 寄存器视图（`w0..w30`），32 位运算自动零扩展进 `x`。
- 64 位指针/地址 → `x` 寄存器视图（`x0..x30`）。
- 单精度 float → `s` 寄存器视图（`s0..s31`，即 `v0..v31` 的低 32 位）。

因此 **MIR 的操作数宽度（DWord=32 / QWord=64）无需任何改动**即可映射到 AArch64（这正是选 AArch64 而非 ARMv7-32 的工程红利：ARMv7 是 32 位指针，会牵动 MIR/ISel 的地址宽度假设）。

---

## 1. 总体架构：目标无关框架 + AArch64 特化

```
                MIR (虚拟寄存器, 目标无关)
                        │  （phi 消除已在中端 lower 完成：并行拷贝 + 关键边拆分）
        ┌───────────────┴───────────────┐
        │        目标无关 codegen 框架     │  backend/targets/utils/
        │  - TargetInst/Block/Function    │
        │  - 图着色 IRC 核心（抽出钩子后） │  ← §3 复用
        │  - RegisterInfo 接口            │
        │  - MachineFunctionPass 框架     │
        └──────┬────────────────┬────────┘
               │实例化          │实例化
   ┌───────────┴───┐    ┌───────┴────────────┐
   │ RISC-V 特化    │    │ AArch64 特化（新增）│  backend/targets/aarch64/
   │ backend/…/riscv│    │ - AArch64Reg        │  x0..x30/sp/xzr, v0..v31
   └───────────────┘    │ - AArch64Inst       │  真实指令 + 语法形式表
                        │ - AArch64ISel       │  MIR→AArch64Inst
                        │ - AArch64RegisterInfo
                        │ - AArch64Frame      │  AAPCS64 序言/尾声
                        │ - AArch64Printer    │  寄存器名/汇编
                        │ - aarch64 peephole  │  复用 pass 框架
                        └─────────┬───────────┘
                                  │
                            AArch64 汇编 (.s)
```

**主流水线**（`AArch64Backend::Run()`，与 `RISCVBackend::Run()` 一一对应）：

```
1. Instruction Selection   MIR ──► AArch64Inst（虚拟寄存器）
2. Machine-level opt (-O≥1) 窥孔/强度削弱/DCE/分支折叠（复用 pass 框架）
3. Register Allocation      图着色 IRC（复用核心，AArch64RegisterInfo 提供寄存器文件）
4. Frame Lowering / PEI     AAPCS64 栈帧、FrameIndex→sp偏移、序言/尾声、callee-saved 保存
5. Assembly Emission        AArch64Printer 输出 .s
```

同 rv64：**ISel 在寄存器分配之前，只用虚拟寄存器**；物理约束（除法/调用参数 x0–x7/v0–v7、返回值 x0/v0）用"预着色物理寄存器 + 复制"表达，交给分配器合并（coalesce）。不特判、不牺牲质量。

---

## 2. 指令选择（AArch64）

### 2.1 策略
沿用 rv64：**逐 MIR 指令的宏展开式选择 + 若干模式匹配窥孔**。MIR 已三地址化、类型清晰、中端已充分优化，ISel 求"简单但正确"，把优化留给窥孔和中端。

### 2.2 w/x 视图与符号扩展（健壮性核心）
- int（32 位）运算一律用 `w` 视图：`add/sub/mul/sdiv w..`，结果零扩展进 `x`，32 位回绕语义天然正确。
- 地址/指针运算用 `x` 视图：`add/ldr/str x..`。
- **int 索引参与 64 位地址计算时必须符号扩展**：`sxtw x_idx, w_idx`（对应 rv64 里靠 `.w` 指令天然 sign-extend 的位置）。这是 AArch64 与 rv64 最需要小心的差异点——rv64 的 `addw` 自带符号扩展，AArch64 的 `w` 运算是零扩展进 x，做地址运算前要显式 `sxtw`。
- 在寄存器**类/大小**上区分（沿用 rv64 经验：MIR 字面量无类型，靠 vreg 的 class/size 判定 int/float、32/64），而非 MIR 操作数类型。

### 2.3 逐条降级规则

| MIR | AArch64 选择 | 备注 |
|---|---|---|
| `Add/Sub`(int) | `add/sub w`，小立即数→`add/sub w,#imm12`（可 <<12） | imm∈[0,4095]，负数或超范围→`mov` 到寄存器再运算 |
| `Mul`(int) | `mul w` | 常量乘在窥孔换移位（`x*2^k`→`lsl`；`x*(2^k±1)`→`add/sub+lsl`） |
| `Div`(int) | `sdiv w` | 常量除用 magic number（后续窥孔，见 §6） |
| `Mod`(int) | `sdiv wq, a, b` + `msub wr, wq, b, a`（r = a − q*b） | AArch64 **无取模指令**，用 sdiv+msub 合成 |
| `And/Or/Xor` | `and/orr/eor w`，立即数→bitmask-imm 或先 `mov` 到寄存器 | 逻辑立即数是特殊 bitmask 编码，非任意值；不合法就物化到寄存器（不特判、通用） |
| `Shl/LShr/AShr` | `lsl/lsr/asr w`，立即数→`lsl/lsr/asr w,#k`(k∈0..31) | |
| `ICmp` | `cmp w,w`/`cmp w,#imm` 置 NZCV，再 `cset w,cond` 物化 0/1 | 与分支融合时不物化，直接接 `b.cond`（见 §2.4） |
| `AddF/SubF/MulF/DivF` | `fadd/fsub/fmul/fdiv s` | 单精度 |
| `FCmp` | `fcmp s,s` + `cset w,cond`（EQ/NE/LT/LE/GT/GE→eq/ne/mi/ls/gt/ge 等） | 融合时接 `b.cond` |
| `Load/Store` | int→`ldr/str w`；float→`ldr/str s`；ptr→`ldr/str x`；地址 base+imm 或 `[base, idx, lsl #shift]` | 富寻址模式，GEP 可折叠成带比例变址的一条 ldr/str |
| `Zext`(i1→i32) | 通常空操作（比较结果已是 0/1） | |
| `Int2Float` | `scvtf s, w` | |
| `Float2Int` | `fcvtzs w, s` | 向零截断，SysY 语义 |
| `Br` | `b label` | |
| `CondBr` | 与前驱 `ICmp/FCmp` 融合成 `cmp/fcmp` + `b.cond`；否则 `cbnz/cbz w, label` | 见 §2.4 |
| `Call` | 参数→x0–x7/v0–v7 + 栈；`bl sym`；返回值从 x0/v0 取回 | AAPCS64，见 §4 |
| `Ret` | 结果→x0/v0；`b .epilogue`（或直接 ret，帧层统一 ret） | |
| `GlobalDef` | `.data/.bss/.section` + `adrp x,sym; add x,x,:lo12:sym` 取址 | 见 §5 |
| `Phi` | **不在此处**——中端已做 phi 消除 | 复用 rv64 已修好的并行拷贝 + 关键边拆分 |

### 2.4 比较/分支融合（与 rv64 的最大差异）

rv64 是"比较即分支"（`blt rs,rt,L`）；AArch64 是**条件标志模型**：先 `cmp` 置 NZCV，再 `b.cond`。融合方式：

- `ICmp` 结果**只被一个 `CondBr` 使用**时，不物化布尔，直接：
  - `cmp w_lhs, w_rhs`（或 `cmp w_lhs, #imm`），后接 `b.<cc> L_true; b L_false`。
  - 谓词映射：EQ→eq、NE→ne、SLT→lt、SGE→ge、SGT→gt、SLE→le（有符号）。
- 与 0 比较可用 `cbz/cbnz w, L`（省一条 cmp）。
- `FCmp+CondBr`：`fcmp s,s` + `b.<fcc>`（浮点条件码：lt→mi/lt、le→ls/le、gt→gt、ge→ge、eq→eq、ne→ne，注意 NaN 语义；SysY 无 NaN 输入，取标准整型化条件码即可）。
- 未融合的 `ICmp/FCmp`（结果被多次用/被存储）：`cmp/fcmp` + `cset w, cond` 物化成 0/1。

沿用 rv64 的 fusion 判定：函数级 def-use 统计，**单一使用且使用者是 CondBr** 才融合（安全、无特判）。

### 2.5 地址计算与 GEP
AArch64 寻址模式比 RISC-V 强，GEP 的行主序线性化（沿用 rv64 里已修好的：`-1` 哨兵维区分局部/全局数组 vs 指针形参、右侧维乘积做 stride）产出的 `base + idx*elem` 可折叠为：
- 常量偏移：`ldr w, [base, #off]`。
- 变址：`ldr w, [base, x_idx, lsl #2]`（elem=4 时 shift=2）。
省去 rv64 那种 `slli+add` 的显式地址算式。此折叠放窥孔阶段做，ISel 先产出朴素 `add` 再折叠，保证正确优先。

---

## 3. 寄存器分配（复用图着色 IRC）

### 3.1 复用策略：抽出 3 处目标钩子
图着色核心（liveness / build 干涉图 / simplify / coalesce / freeze / select-spill / assign-colors / rewrite-spill）**完全目标无关**，只操作 `TargetInst`/`TargetOperand` 和 `RegisterInfo`。当前实现里仅 3 处碰了 `RISCVInst`：

1. `IsMove(inst, class, &dst, &src)`：识别寄存器间移动（RV 的 `MV`/`FMV_S`）→ 供 coalesce。
2. `DefsUses(inst, class)` 里的**调用识别与 clobber**（RV 的 `CALL` → caller-saved 全部作为合成 def；实参寄存器作为 use）。
3. `EmitReload/EmitSpill`：溢出的 load/store（RV 的 `LW/SW/FLW/FSW`，含超范围偏移的地址物化）。

**重构方案**：定义目标无关接口 `TargetRegAllocHooks`（放 `backend/targets/utils/regalloc/`），含 `IsMove / IsCall / CallClobbers(class) / EmitReload / EmitSpill`。把 `GraphColorRegAlloc` 的核心搬到 utils 下成 `GraphColoring`（模板方法/持有 hooks 指针），RISC-V 与 AArch64 各实现一份 hooks。**重构时以 rv64 全量样例回归为验收**——核心逻辑一字不改，只把 3 处替换为钩子调用，确保 RV 不回退。

> 若重构风险偏高，退路是先复制一份 `AArch64GraphColorRegAlloc`（与 RV 版并存）跑通 AArch64，再合并去重。但优先做干净抽取（符合"保持接口一致"）。

### 3.2 AArch64 寄存器文件（`AArch64RegisterInfo`）
- **GPR**（x0..x30 + sp/xzr）：
  - 可分配集合：x0–x15、x19–x28（约 26 个）。**保留**：x16/x17（IP0/IP1，链接器/PLT veneer 会用，且做溢出地址 scratch）、x18（平台寄存器，Linux 上保守保留）、x29（fp，帧指针）、x30（lr）、sp、xzr。参照 rv64 里排除 fp/t3 的做法。
  - caller-saved：x0–x15（含 x9–x15 临时、x0–x7 参数）。
  - callee-saved：x19–x28。
- **FPR/SIMD**（v0..v31，用 s 视图）：
  - 可分配集合：v0–v31（32 个，全可用）。
  - caller-saved：v0–v7、v16–v31。
  - callee-saved：v8–v15（**仅低 64 位 d8–d15 需保存**；单精度 s8–s15 落在其中，帧层按 d 宽度保存/恢复）。
- 溢出/帧地址 scratch：保留 x16（对应 rv64 的 t3）。

### 3.3 调用 clobber 与预着色（沿用 rv64 经验）
- `bl` 处把该类**全部 caller-saved 物理寄存器作为合成 def**（跨调用活跃的值自动进 callee-saved 或溢出）。
- 实参寄存器（x0–x7/v0–v7）作为 `bl` 的**额外操作数**记录（printer 忽略），让 liveness 看到它们活到调用点——这是 rv64 上踩过并修好的关键点（否则溢出重载会复用 x0 覆盖 arg0）。
- **绝不把值 coalesce 进保留寄存器**（sp/xzr/fp/lr）——rv64 上修过的 latent bug（`allocatable_ids_` 白名单），核心逻辑复用即自动生效。

---

## 4. 栈帧与调用约定（AAPCS64，LP64）

### 4.1 栈帧布局（与 rv64 对称）
```
高地址
  ┌──────────────────────────┐  ← 调用者栈
  │ 入参溢出区（第 9+ 个实参） │  经 fp(x29) 以 fp+k*8 访问，与帧大小无关
  ├──────────────────────────┤  ← 序言后 sp = 本函数 fp
  │ 保存的 x29(fp)、x30(lr)    │
  │ 保存的 callee-saved（用到的）│  x19–x28 / d8–d15
  │ 溢出槽（分配器决定）        │
  │ alloca（数组/取址局部）     │
  │ 出参溢出区（本函数最宽调用） │  sp+k*8
  └──────────────────────────┘  ← sp（16 字节对齐）
低地址
```
16 字节对齐（AAPCS64 在公共接口处要求 sp 16 对齐）。沿用 rv64：出参区在帧底、经 sp 访问；入参溢出区经 fp 访问（帧大小无关）；所有 sp 偏移做立即数合法化（AArch64 ldr/str 的 scaled-imm12 或 unscaled-imm9 超范围时经 x16 物化地址）。

### 4.2 序言/尾声
- 序言：`sub sp, sp, #frame`（超范围→`mov`+`sub`）；`stp x29, x30, [sp, #fp_off]`；`add x29, sp, #fp_off`（fp 指向保存对，或指向 sp+frame 以对齐 rv64 的"fp=入参 sp"约定——二选一，保持整套一致）；保存用到的 callee-saved（`stp`/`str`）。
- 尾声：恢复 callee-saved；`ldp x29, x30`；`add sp, sp, #frame`；`ret`。
- `stp/ldp`（成对存取）是 AArch64 特色，可把两次 str 合一，属优化项；先用单条 `str/ldr` 求正确，窥孔再合并成 `stp/ldp`（对应 rv64 没有的额外码质量点）。

### 4.3 参数传递（AAPCS64）
- int 实参：x0–x7（w 视图传 32 位 int）；float 实参：v0–v7（s 视图）；**int 与 float 各自独立计数**（与 lp64d 对称）。
- 溢出实参按顺序压栈（出参区 sp+k*8）。
- 返回值：int→x0/w0，float→v0/s0。
- **memset/memcpy 内建**：沿用 rv64 的处理，只匹配 `llvm.memset`/`llvm.memcpy` 前缀（不误伤 `my_memset` 等用户函数），降级为 libc `memset/memcpy`（AArch64 上参数同为 x0/x1/x2）。

### 4.4 与 sylib 的对接
运行时库接口与平台无关（getint/putint/getfloat/putfloat/getarray/putarray/getch/putch、`_sysy_starttime/_sysy_stoptime`）。`starttime()/stoptime()` 由中端映射为 `_sysy_starttime(__LINE__)/_sysy_stoptime(__LINE__)`（rv64 已验证），AArch64 直接复用同一中端行为。

---

## 5. 全局量与只读数据
- 取址：`adrp x, sym` + `add x, x, :lo12:sym`（PC 相对页 + 页内偏移）。装载可折叠为 `adrp x, sym; ldr w, [x, :lo12:sym]`。
- 数据段：int/float 初值统一按 4 字节 `.word <bit pattern>`（float 存 IEEE-754 位型，沿用 rv64 的单一码路径）；全零→`.bss` `.zero`；`.align`。
- 段指示：`.data/.bss/.text/.globl/.align`（GNU as，AArch64 通用）。

---

## 6. 机器级优化（复用 pass 框架，不牺牲力度）
复用 `MachineFunctionPass`/`MachinePassManager`（按 -O≥1 开关，主驱动里组装，与中端 pass 管线一致）。AArch64 peephole（对应 rv64 的 `RISCVPeephole`）：
- 立即数折叠：`mov #C; add w,w,x` → `add w,w,#C`（imm12 范围内）；逻辑/比较同理（受 bitmask-imm 限制时保留寄存器形式）。
- 强度削弱：`mul` 常量→`lsl`/`add+lsl`；常量 `sdiv`→magic number（Hacker's Delight）。
- 地址折叠：GEP 的 `add base, idx<<k` 折进 `ldr/str [base, idx, lsl #k]`。
- `stp/ldp` 合并：相邻 `str/ldr` 成对合并（AArch64 专属码质量点）。
- MachineDCE（删无用纯指令）、BranchFold（删到下一块的冗余 `b`）：目标无关，直接复用/轻改（识别 AArch64 的 `b`/纯指令集合）。

窥孔的每条改写都是通用代数/目标恒等式，不针对任何样例形状（沿用 rv64 纪律）。

---

## 7. 健壮性工程（"通过全部样例"的保障）
- 立即数合法化：add/sub 的 imm12(<<12)、逻辑的 bitmask、ldr/str 的 scaled-imm12/unscaled-imm9、大常量的 movz/movk——超范围一律物化到 scratch（x16），**不特判**。
- 大栈帧：`sub/add sp` 超 imm 范围经 x16 物化；入参经 fp、出参经 sp，与帧大小解耦（rv64 同款）。
- 32/64 混用防线：int 用 w、地址用 x、int 索引进地址前 `sxtw`——AArch64 最易错处，ISel 里成规则而非个案。
- 验证：`aarch64-linux-gnu-gcc -static out.s libs/sylib.c -I libs -o elf` + `qemu-aarch64 elf`；期望 `.out` = 程序 stdout 后接退出码。GOTCHA（沿用 rv64）：qemu 退出码在任何管道**之前**捕获；比较用文件而非 `$(cat)`（避免吞尾部空行）；大用例 qemu 慢，别用短 timeout 误判死循环。
- A/B 判据：中端 IR 用 clang（`aarch64` triple）编译对拍，区分中端 bug 与后端 bug（rv64 上屡试不爽）。

---

## 8. 里程碑（建议顺序）

- **A0**：`AArch64RegisterInfo` + `AArch64Reg`（寄存器编号/ABI 名/可分配集合/caller-callee 划分）+ `AArch64Printer`（w/x/s 名字、立即数、寻址渲染）+ `AArch64Inst`（opcode 枚举 + 语法形式表）。骨架能编译。
- **A1（ISel 整型子集）**：算术/移位/逻辑/`sdiv`/`msub` 取模、`cmp+cset`/融合分支、copy、call（≤8 int 参）、参数、return。输出仍是虚拟寄存器。
- **A2（寄存器分配）**：抽出图着色钩子接口，接上 `AArch64RegisterInfo`，跑出物理寄存器。以 rv64 全量回归验收重构。
- **A3（栈帧/内存/全局/数组）**：AAPCS64 序言尾声、FrameIndex→sp、`adrp/add` 全局、`ldr/str` 内存、GEP 地址。跑通整型 functional。
- **A4（float 单精度）**：`fadd.s` 等、`scvtf/fcvtzs`、float 参数 v0–v7、float 全局。
- **A5（>8 参数 + 立即数合法化收尾）**：出/入参溢出、大帧、大立即数。functional 全绿。
- **A6（机器级优化）**：peephole（立即数折叠/强度削弱/地址折叠/`stp-ldp`/magic 除法）、DCE、BranchFold，-O≥1 开。
- 每步以 `qemu-aarch64` 全量对拍验收，且 rv64 不回退。

---

## 9. 参考资料
- Arm Architecture Reference Manual for A-profile（ARMv8-A）；Arm A64 Instruction Set。
- Procedure Call Standard for the Arm 64-bit Architecture（AAPCS64）。
- LLVM AArch64 后端 / Cranelift aarch64；Compiler Explorer 对拍。
- Hacker's Delight（常量除法 magic number、强度削弱）。
- 本仓库 rv64 后端（`backend/targets/riscv/`）与 `docs/backend_codegen_plan.md`：一比一对照实现。

---

## 10. 目录约定（拟新增）
```
backend/targets/
  utils/                       # 目标无关（已存在，图着色核心将抽到 regalloc/）
    regalloc/
      registerinfo.h           # 已有
      targetregallochooks.h    # 新增：IsMove/IsCall/CallClobbers/EmitReload/EmitSpill
      graphcoloring.{h,cpp}    # 新增：从 riscv 抽出的目标无关 IRC 核心
    pass/                      # 已有，复用
  riscv/                       # 已有，重构后改用 utils 图着色核心 + RISCVRegAllocHooks
  aarch64/                     # 新增
    regs/aarch64reg.h          # x0..x30/sp/xzr、v0..v31、ABI 名、可分配集合、caller/callee
    regs/aarch64reginfo.h      # AArch64RegisterInfo : RegisterInfo
    inst/aarch64inst.{h,cpp}   # A64Opcode 枚举 + 语法形式表 + AArch64Inst
    isel/aarch64isel.{h,cpp}   # MIR → AArch64Inst
    regalloc/aarch64hooks.h    # AArch64RegAllocHooks : TargetRegAllocHooks
    frame/aarch64frame.{h,cpp} # AAPCS64 序言/尾声、立即数合法化
    printer/aarch64printer.h   # 寄存器名/汇编渲染
    pass/                      # aarch64 peephole 等
    aarch64.{h,cpp}            # AArch64Backend 驱动（Run/Emit），main.cpp 里按目标选择
```
驱动选择：main.cpp 现在固定用 `RISCVBackend`；引入一个 `-target {riscv,aarch64}`（或复用 stage）在两个后端间切换，默认 riscv（保持竞赛默认不变）。
