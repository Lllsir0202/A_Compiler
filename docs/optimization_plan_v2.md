# 优化规划 v2：向前沿看齐（参考 Gnalc / UESTC）

> 背景：Tier 1/2 已完成（B1–B8、LocalCSE、B7 loop-weighted spill、M6 unroll 4→8、M3 LICM div/mod、magic div…）。
> 本轮以 **Gnalc**（电子科大 T202510614205710-2983，一个非常完整的高分 SysY 编译器）为参照，规划下一阶段。
> 排序标准：**对我们这批热点样例的期望收益优先**（不排斥高难度），effort/risk 作次级参考。
> 纪律不变：逐项实现、两后端 -O0/-O1 全绿（functional 100 + h_functional 40）、生成汇编为准、动态指令数主筛 + 墙钟/llvm-mca 辅（延迟置换型）、可开关、提交写清 bench 数。

---

## 0. 差距盘点（Gnalc 有、我们没有 / 较弱）

我们已有：mem2reg、SCCP、GVN、ADCE、DSE、LICM(+div/mod)、SCEV、强度削弱、全/部分展开、内联、尾递归、alias/memorySSA、magic-div、后端 LocalCSE/addr-fold/frame/stp-ldp、图着色 RA(Iterated Coalescing)+loop-weighted spill。

Gnalc 领先我们、且我们**缺失**的关键项（按类别）：

| 类别 | Gnalc 有 | 我们 | 缺口价值 |
|---|---|---|---|
| **循环重构** | Loop Interchange、Relayout(数组转置)、Loop Fusion、Loop Unswitch、Loop Rotate、Loop Parallel、While→For | 无 | **极高**（matmul/transpose 是 cache-bound，重构可数倍） |
| **向量化** | SLP Vectorizer(bottom-up)、Loop Parallel | 无 | 高（但硬件受限，见 §caveat） |
| **冗余消除深化** | GVN-PRE、Reassociate、Redundant Load Elim、Range Analysis | 仅 GVN | 中高 |
| **控制流** | If-Conversion(→select/csel)、CodeLayout(块重排)、BCE、Code Sink | 无 | 中高 |
| **指令级** | PostRA Scheduling(AArch64)、GenericPeephole、MachineLICM、LoadEli | 部分(peephole/LocalCSE) | 中（乱序核上折扣） |
| **过程间/杂项** | Memoization(纯递归)、DAE、Internalize/Globalize、Tree Shaking、Gep Flatten | 部分(inline/尾递归/getptr flatten) | 低–中(局部大) |

---

## ⚠ 两个战略前提（先验证，别盲目投入）

1. **向量化的硬件可用性**：ARM(Ubuntu)几乎必有 NEON(ARMv8 强制)；**RISC-V 板是 FPGA 上的 BOOM，大概率没有 RVV 向量扩展**。RV 是我们的主板目标(-rv 分支/上板)。→ **投向量化前，先确认板子 ISA**。若 RV 无 RVV：向量化只做 NEON(仅 ARM 目标受益)，或降级为"标量 SLP 打包 + 展开配合"。
2. **乱序核削弱静态调度价值**：两台评测机都是乱序核(BOOM + Neoverse/A72 级)，硬件有 ROB 动态重排 → **PostRA 指令调度的收益打折**（不像顺序核那样关键）。仍有价值(填发射槽/减假依赖)，但优先级相应下调。

---

## 1. 优先级排序（P1 最高）

### 【Tier A：最大期望收益 —— 直击 matmul/transpose/crypto 热点】

#### P1. Loop Interchange（循环交换）★ 首选
- **是什么**：对完美嵌套循环，交换内外层顺序，使最内层沿 stride-1 方向访问（如 matmul `i-j-k` → `i-k-j`，让 `B[k][j]` 连续）。
- **为什么**：matmul/transpose/gemm 类是 **cache-bound**，交换后 L1 命中率剧变，实测常 2–5×（这是顶尖编译器拉开差距的核心）。直击 `01_mm1/2/3`、`transpose0/1/2` 及 finalperf 的 gemm 类。
- **依赖**：已有 SCEV + LoopInfo；需补**方向向量/依赖分析**判定交换合法性（无循环携带依赖阻止）。
- **难度/风险**：高 / 中（合法性判定要严谨，否则出错）。**收益极高，作为 P1。**

#### P2. Array Relayout（数组转置重排）
- **是什么**：当某热点 2D 全局/大数组的访问模式跨 stride，物理转置该数组 + 改写所有访问下标，使内层 stride-1。
- **为什么**：Interchange 的补充/替代——当循环因依赖不能交换、但数组可转置时同样拿到 cache 收益。transpose 系列样例本质就是这件事。
- **难度/风险**：高 / 中高（要全程序改写某数组的所有访问，且证明安全）。与 P1 共享依赖分析基础设施。

#### P3. Auto-Vectorization（SLP + 循环向量化）
- **是什么**：SLP 从相邻 store 自底向上打包同构标量为向量；循环向量化按 VF 拓宽循环体。目标 NEON(ARM) / RVV(RV，若有)。
- **为什么**：matmul/crypto/fft 的原始吞吐最大来源，理论 2–4×。
- **⚠ 先决**：见 §caveat 1，先验证 RV 板 RVV。无 RVV 则 NEON-only。
- **难度/风险**：**很高** / 高（正确性+对齐+归约模式+硬件门槛）。收益高但门槛也高，排 P3。

### 【Tier B：广谱、稳健收益】

#### P4. If-Conversion（分支转 select/csel）
- **是什么**：把小 `if` 的两路合并为 `select`（ARM `csel`；RV 无原生 → 用无分支算术 `(mask&a)|(~mask&b)` 或 `czero`(若 Zicond)）。
- **为什么**：消除难预测分支——乱序核上分支误预测惩罚仍很重(前端冲刷)。直击 sort/fft/条件循环；顺便可缓解 M6 在 `03_sort3` 的展开回退。
- **难度/风险**：中 / 中（RV 无 csel 要谨慎选型；只转"两路都便宜且无副作用"的）。ROI 好。

#### P5. GVN-PRE（部分冗余消除）
- **是什么**：比现有 GVN 更强——消除"部分路径冗余"(某些前驱算过、某些没算)的表达式，在缺失路径插入计算。
- **为什么**：循环里跨迭代/跨路径的部分冗余是我们 GVN 抓不到的；补齐冗余消除深度。
- **难度/风险**：中高 / 中。我们已有 GVN/memorySSA/CFG 基础，可扩展。

#### P6. Reassociate（表达式重结合）
- **是什么**：重结合 + 常量折叠，把 `a*i + a*j` → `a*(i+j)`、`(inv+iv)+inv2` → `iv+(inv+inv2)`，暴露更多不变量/CSE/强度削弱机会。
- **为什么**：我们 M3/M4 多次"空收益"部分是因为不变量没被暴露；Reassociate 是这些 pass 的**放大器**。广谱小收益 + 使能。
- **难度/风险**：中 / 低中。性价比好，且是 P5/LICM/强度削弱的前置增益。

### 【Tier C：定向 / 使能 / 低成本】

#### P7. Loop Unswitch（循环去分支）
把循环内的**循环不变** `if` 提到循环外（复制两份循环）。中等难度，命中"循环体里有不变条件判断"的样例。

#### P8. PostRA Instruction Scheduling（列表调度）
按流水线资源 + 依赖做拓扑排序，填发射槽/减假依赖。**乱序核上打折**（P8）；参考 Gnalc 只在 AArch64 做。可先只做 AArch64，用**墙钟/llvm-mca**判定（指令数不变）。中高难度 / 中收益。

#### P9. CodeLayout（基本块重排）
按频率把热路径排成 fall-through + 冷块下沉，改善 i-cache 与分支方向。低中难度、稳定小收益、ROI 高，适合穿插做。

#### P10. Loop Rotate（while→do-while）
旋转循环使判定在底部，减少每迭代一次分支 + 更好地配合 LICM/展开。低中难度、使能型。

#### P11. Memoization（纯递归函数记忆化）
对无副作用递归函数缓存结果。**niche 但局部可数量级**——若 h_functional/finalperf 里有重递归样例(如朴素 fib/组合)收益爆炸。先扫样例确认有目标再做。

### 【Tier D：清理/次要】
Loop Fusion、Dead Arg Elim、Internalize/Globalize、Tree Shaking、Code Sink、Range Analysis(SysY 无越界检查，收益主要来自使能简化)。按需穿插。

---

## 2. 推荐执行序列

1. **先验证向量硬件**（决定 P3 走向）+ **扫样例**确认 P11 是否有目标（低成本情报）。
2. **P6 Reassociate**（低成本、使能，先铺路）→ **P4 If-Conversion**（广谱稳收益）。
3. **P1 Loop Interchange**（最大单点收益，主攻）→ 视情况 **P2 Relayout**。
4. **P5 GVN-PRE** 深化冗余；**P9 CodeLayout** 穿插。
5. **P3 向量化**（若 RV 有 RVV 或专攻 ARM NEON）—— 压轴大工程。
6. 其余（P7/P8/P10/P11/Tier D）按实测缺口补。

每项仍走：基线 → 实现(可开关) → 两后端全绿 → bench 几何平均 → 提交。空收益的（像 M4/full-unroll）如实回退；正确但本地零收益的能力项（像 B7/M3）按既定偏好按需保留。
