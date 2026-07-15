# 优化规划 v3：中端循环专项（内存 × 副作用 × 覆盖补强）

> 背景：v2 的 Tier 基本盘（GVN-PRE、部分展开增强等）已落地一部分。本轮聚焦**中端循环相关优化**：
> ① 已实现的 pass 补测试覆盖、把每种情况处理得更好；② 未实现的逐步补齐，**优先内存/副作用相关**——
> 这是当前"要么太保守、要么没有"的重灾区。
>
> 纪律不变：逐项实现、**Backend-tmp 与 Backend-tmp-rv 两分支同步公共代码**（当前两分支仅 main.cpp
> 默认 target_arch 一行之差，中端完全一致，每项做完后 cherry-pick 同步、保留该差异行）、
> functional 100 + h_functional 40 两后端 -O0/-O1 全绿、bench 动态指令数主筛 + 墙钟辅、
> 可开关、提交写清 bench 数。

---

## 0. 现状盘点（本次代码审查结论，含精确保守点）

### 已实现（循环/内存相关）
LoopInfo、LoopSimplify（preheader 规范化）、SCEV（AddRec 链 + no-wrap + Div/SMax/UMin）、
LICM（load 提升 + 纯运算提升，div/mod 可投机）、StrengthReduce（SCEV 驱动乘→加 IV）、
DeadLoopElim（闭式求值删循环）、全展开 + 运行时部分展开（MLIR 层）、
AliasAnalysis（base object + 逃逸 + 常量下标）、MemorySSA、GVN（MemorySSA 驱动 load/store 转发）、
GVN-PRE、DSE（块内 + 跨块路径覆盖）、ADCE、SCCP、内联、尾递归。

### 保守点定位（file:line 为当前 HEAD）

| # | 位置 | 保守行为 | 连锁受害者 |
|---|---|---|---|
| C1 | `alias.cpp:215` `CallModRef` | **完全忽略 callee**：任何 call 被视为可能读写一切（除未逃逸局部） | LICM（循环含任一 call 时全局/参数 load 全不能提）、DSE（call 杀死所有 store）、MemorySSA/GVN（call = clobber-all，`memoryssa.cpp:30`）、ADCE、DeadLoopElim |
| C2 | `alias.cpp:72` `ComputeEscape` | 指针传给**任何** call 即逃逸——即使 `putarray`（只读）、`getarray`（写但不保存指针）、或摘要可证不保存指针的自定义函数 | 同上，且 SysY 无指针存储，源语言层面指针根本无法被保存，逃逸大多是虚警 |
| C3 | `alias.cpp:177` `SameObjectAlias` | 下标链长度不同 → MayAlias；下标非常量 → MayAlias，即使 SCEV 可证 `a[i]` vs `a[i+1]`、`a[2i]` vs `a[2i+1]` 不相交 | LICM load 提升、DSE、GVN 转发在数组循环中大量失效 |
| C4 | `licm.cpp` | **纯 call 不提升**。（勘误：scalar promotion **已实现**——`PromotionPass` 就是通用版 load 提头 + store 沉出口，含条件 store 的 SSA 重建；它的保守点在"循环内有 call 即 CallModRef 阻断"= C1，以及 must-alias 判定 = C3） | 含 call 的归约循环无法寄存器化 |
| C5 | `deadloopelim.h:31` | 要求**无 store 无 call** 且 **常量 trip count** | 含纯 call 或符号 trip count（闭式 `n*(n-1)/2`）的死循环删不掉 |
| C6 | `dse.cpp` | 无循环感知（本迭代 store 被下一迭代必然覆盖的情况不删）；call 处理依赖 C1 | 循环内冗余 store |
| C7 | `mlirlooppartialunroll.h:33` | `bound_main = bound-(K-1)*step` 有**溢出风险**（bound 接近 INT_MAX）；只支持常量加性步长的 counted while | 边界正确性 + 覆盖面 |
| C8 | 无 Loop Rotate / Unswitch / Interchange / Fusion / Reassociate | v2 的 P1/P6/P7/P10 尚未开工 | 结构性收益缺失 |

---

## 1. 实现清单（按执行顺序编号 L0–L6）

### L0. 定向测试底座（先做，保护后面所有改动）★低成本
- **L0.1** 建 `testcases/directed/`（或 `tests/midend/`）：每个循环 pass 一组最小 `.sy` 用例，
  三类：**应触发**（核对 IR dump 里转换确实发生）、**不应触发**（合法性边界，转换必须不发生）、
  **正确性边界**（触发后输出必须仍对拍）。
- **L0.2** 脚本：编译到 BASEIR/LLVM dump + grep 断言转换发生/不发生 + qemu 对拍输出，一条命令全跑。
- **L0.3** 用它先给**现有** pass 建档：LICM（嵌套循环、header phi、div/mod 投机、含 call/含
  may-alias store 的不可提场景）、StrengthReduce（多 IV、嵌套、getptr 地址递推）、
  DeadLoopElim（各阶闭式、no-wrap 边界）、全展开（trip 上限、嵌套）、部分展开（**C7 溢出用例**：
  bound 近 INT_MAX、负步长、`!=` 条件、非单位步长）、SCEV（`<=`/`!=`/负步长 trip count）、
  GVN 跨块 load 转发、DSE 路径覆盖。**发现的洞当场修**。
- 验收：新用例全绿 + 现有 140 功能样例全绿（两后端 ×2 优化级）。

### L1. 过程间副作用摘要（ModRef / FunctionAttrs）★本轮最高杠杆
一个分析同时解锁 C1+C2 的全部连锁受害者。
- **L1.1** 新 analysis pass `FunctionSideEffectPass`：沿 call graph 自底向上（SCC 内取不动点）算每函数摘要：
  - 读/写了哪些**全局**（精确到具体全局变量集合，封顶退化为 any）；
  - 每个**指针形参**是 read / write / none；
  - 是否**保存指针**（SysY 语言无指针存储 → 用户函数天然不保存；仅防 IR 层意外）；
  - runtime 库函数**打表**：`getint/getch/getfloat` 只读输入流不碰程序内存；`putint/putch/putfloat/putf`
    只写输出流；`getarray/getfarray` = 写 arg 数组不保存；`putarray/putfarray` = 读 arg 数组不保存；
    `starttime/stoptime` 无程序内存效应。**IO 序作为独立的"不可重排/不可删"位保留**（副作用 ≠ 内存效应，删/提/换序要分开判）。
- **L1.2** `AliasAnalysisPass::CallModRef(call, ptr)` 改用摘要 + 实参匹配：ptr 的 base 不在 callee
  的 mod/ref 集合 → NoModRef；区分 Mod 与 Ref（DSE 只怕 Ref，load 提升只怕 Mod）。
- **L1.3** `ComputeEscape` 改用"callee 可能保存该指针"判逃逸（配合打表 → SysY 下数组传参基本不再逃逸）。
- **L1.4** 消费者逐个放开（每个单独提交、单独 bench）：
  a) MemorySSA：call 仅在 Mod 时作 def（`memoryssa.cpp:30` 的 `IsMemoryWrite`）；
  b) LICM：load 越过只读 call 提升；
  c) DSE：store 越过不 Ref 该地址的 call；
  d) ADCE：结果无用且 readnone/readonly 且无 IO 的 call 可删；
  e) DeadLoopElim：循环含纯 call 也可删（C5 前半）；
  f) LICM 提升**循环不变纯 call**（实参不变 + readnone；readonly 需循环内无 Mod）。
- 验收：h_functional 里递归/多函数样例重点回归；bench 预期 LICM/GVN 命中率显著上升。

### L2. LICM Scalar Promotion（load 提头 + store 沉出口）★内存类主菜
- 条件：循环内对**同一 must-alias 地址**的所有访问，与循环内其余 store/call 不别名（用 L1 后的
  ModRef），地址循环不变，且 store 在每次迭代必执行（或该地址可证可解引用——SysY 数组/全局天然满足）。
- 变换：preheader 载入寄存器 phi，循环内读写全部改 phi，**每个 exit block 沉一条 store**。
  需要 exit block 唯一化（LoopSimplify 补 dedicated exits —— 一并实现）。
- 直击归约模式 `g_sum += a[i]`、`b[j] = b[j] op ...`。
- 验收：定向用例（含"store 在 if 里不必执行→不促进"的反例）+ bench。

### L3. Alias 精度：SCEV 驱动的数组下标分析
- **L3.1** 下标链长度不同：按公共前缀逐维比较，前缀已证不相交即 NoAlias（`a[1]` 的行指针 vs `a[2][j]`）。
- **L3.2** 非常量下标交给 SCEV：同一循环的两个 AddRec，`{c1,+,s}` vs `{c2,+,s}` 且 `|c1-c2| < |s|`
  且 no-wrap → NoAlias（区分 `a[i]` vs `a[i+1]`）；步长/余数分析区分 `a[2i]` vs `a[2i+1]`。
  先做同层循环、常量 start 差 & 常量步长这一档，够用且稳。
- **L3.3** 接口：AliasAnalysis 增加可选 SCEV 后端（依赖注入，避免 pass 循环依赖）。
- 验收：定向用例 + LICM/DSE/GVN 在 matmul/stencil 类 bench 的命中变化。

### L4. DeadLoopElim / DSE 放宽（收尾 C5、C6）
- **L4.1** DeadLoopElim：符号 trip count 也接受（闭式直接用 SCEV 表达式展开成 IR，如 `n*(n-1)/2`，
  注意除 2 的地板语义与 no-wrap 前提）；store 到"未逃逸且此后不再读"的局部也允许（配合 DSE 思路）。
- **L4.2** DSE 循环感知：store 的地址是 AddRec 且下一迭代同一地址在任何读之前必被覆盖 → 删
  （典型：`a[i]=x; a[i]=y` 跨迭代变体）。优先级低于 L2（promotion 已覆盖大头），视 bench 决定做不做。

### L5. 已实现 pass 的"处理得更好"（伴随 L0 发现逐个修，独立可并行）
- **L5.1** 部分展开：修 C7 溢出（把 `bound_main` 计算换成防溢出形式或加范围前置条件）；
  支持负步长 / `!=` 条件；展开因子按体积自适应。
- **L5.2** LoopSimplify：header 多外部前驱时**构造** preheader（现在部分场景直接放弃 → LICM 整个不跑）；
  补 dedicated exit（L2 依赖）。
- **L5.3** SCEV：trip count 覆盖 `<=`、`!=`、负步长；smax/umin 化简完备性。
- **L5.4** StrengthReduce：IV 去重（多个同步长 IV 合并）、getptr 地址递推（多维数组行指针递推）、
  线性函数测试替换（LFTR，退出条件改用现成 IV，消掉原 IV）。
- **L5.5** GVN-PRE：跨迭代（loop-carried）部分冗余的用例验证，不足则补。

### L6. 循环结构类（衔接 v2，另立专项）
按 v2 既定顺序：Reassociate（使能，先）→ Loop Rotate → Loop Unswitch → Loop Interchange（大件，
依赖 L3 的依赖分析基础）→ Fusion。**本轮 L1–L5 做完后再开**，L3 的 SCEV 下标分析是 Interchange
合法性判定的直接地基。

---

## 1.5 L0 首轮结果（2026-07-15）

底座：`script/directed/directed.py` + `testcases/directed/`（21 用例，licm/promotion/strengthreduce/
deadloopelim/unroll/gvn/dse 七组；`//!` 指令做 IR 断言，宿主 gcc 生成 golden，qemu 双后端 ×-O0/-O1 对拍）。
另加 `AC_DUMP_AFTER_PASS=<dir>` 环境变量（passmanager.cpp）逐 pass dump IR，用于二分定位。

首轮抓到并**当场修复**的正确性 bug（两个都在共享中端，需同步两分支）：
1. **ADCE 遗留不可达块（段错误）**：`RewriteDeadTerminators` 绕过死区后不删不可达块；
   TrivialBlockMerge 随后折叠掉其中的空转发块，不可达块里的 phi incoming 悬空 → LLVM lowering 段错误。
   修复：ADCE 末尾新增 `RemoveUnreachableBlocks()`（按改写后的终结符重算可达性、摘块、清 use-list、
   修剪幸存 phi 的死 incoming）。触发用例 `dse/writeonly_local_gone.sy`。
2. **部分展开 bound_main 溢出（错编译，即 C7）**：`bound_main = bound-(K-1)*step` 32 位回绕后主循环
   多跑 ~2^31 次。修复：主循环包进 `scf.if(guard)`，guard 为 bound 与编译期常量阈值
   （升序 `bound >= INT_MIN+delta`、降序 `bound <= INT_MAX+delta`）的一次比较，失败则整段走 remainder；
   另拒绝 `|step| > INT_MAX/K`。触发用例 `unroll/partial_wrap_bound.sy`。

已确认的优化力度缺口（待修，按性价比排序）：
- **管线缺"循环后清理回合"**：DLE 删主循环并物化闭式常量后，remainder 成为零 trip 常量循环，
  但 SCCP/ADCE 都只在循环 pass 之前跑 → 死的 remainder 留下（`deadloopelim/const_tripcount_sum` 红）。
  方案：管线尾部加 PostLoop SCCP+ADCE 实例（或 DLE 循环至不动点）。
- **DLE 不处理高阶链**：`t += s; s += i`（三阶）不删（`deadloopelim/closed_form_second_order` 红）。
  与 SCEV EvaluateAtIteration/CanExpand 的覆盖有关，且注意 sum_wrap_boundary 的回绕语义约束。
- **GVNPRE 漏跨路径冗余**：guard-if 结构下主/remainder 两个 preheader 各 hoist 一份相同的
  invariant div，GVNPRE 未合并（licm/hoist_div_speculative 的 dump 可见）。
- **SCEV 常量折叠的编译器自身 UB**：`GetAddExpr`/`SplitAffineTerm` 等用裸 `int` 做
  `constant_sum +=`、`coefficient *=`，对抗性常量下是编译器进程内的有符号溢出 UB；且闭式含
  `Div`（二项式系数）时，"先回绕后除"与循环真实的"逐步回绕累加"可能不同值——需要系统梳理
  DLE 的 no-wrap 前置条件（sum_wrap_boundary 目前对拍通过，但要确认是"没删"还是"删对了"）。
  修法：SCEV 内部常量运算全部改 int64_t + 显式 wrap 截断，Div 闭式仅在证明无回绕时展开。

## 2. 推荐执行序列与理由

```
L0 测试底座（保护网，且立刻暴露存量 bug）
→ L1 副作用摘要 + 消费者放开（一次分析解五个 pass 的锁，期望收益最大）
→ L2 scalar promotion（内存类单点最大件，依赖 L1 的 ModRef 与 L5.2 的 dedicated exit）
→ L3 SCEV alias（数组循环全面受益，兼为 L6 Interchange 打地基）
→ L4 / L5 穿插收尾（小步快跑，每步一提交）
→ L6 结构类专项（回到 v2 的 P 序列）
```

## 3. 每项统一验收标准
1. 两分支同步：改动在 Backend-tmp 完成 → cherry-pick 到 Backend-tmp-rv（或反向），
   仅保留 main.cpp 的 target_arch 默认值差异；同步后两分支各自全绿。
2. 正确性：functional 100 + h_functional 40 + 定向用例，RV/AArch64 × -O0/-O1 全绿。
3. 性能：bench 动态指令数几何平均 + 回退样例明细；延迟置换型用墙钟/llvm-mca 复核。
4. 可开关（pass 级），commit message 写清优化项 + bench 数。
