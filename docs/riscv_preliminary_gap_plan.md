# RISC-V 初赛性能差距收敛计划

## 1. 目标与基线

本计划针对 `testcases/2026初赛RISCV赛道性能用例/performance` 的 60 个样例，
目标是在不依赖样例名、函数名或固定输入的前提下，逐步收敛
`testoutputs/compare.md` 中仍然显著的线上差距。

`compare.md` 早于 2026-07-29 合并的一批优化，不能直接视作当前 HEAD 的性能：

- CRC 的纯循环末次迭代消除已经合入。
- `conv2d` 的 wrapped-counter 闭式化已经合入。
- `sort` 的 repeated-power-of-two division 闭式化和局部数组初始化下沉已经合入。
- `h-1` 的尾递归消除及循环状态简化已经合入。
- `sl` 的 write-only 全局数组写入删除已经合入。
- `transpose` 的冗余写回删除、空尾绕过和地址递推已经合入。

当前 HEAD 的诊断基线：把当前 `-O1` 产生的 LLVM IR 再交给 Clang `-O2`
生成 RV64 代码，60/60 输出一致。我们的总动态指令数为该路径的约
`1.063x`，逐样例几何平均约 `1.122x`。主要差距为：

| 样例族 | 当前 / Clang `-O2` 动态指令 |
|---|---:|
| `conv2d` | 1.71x |
| `03_sort` | 1.51x |
| `matmul` | 1.27x--1.32x |
| `crypto` | 1.30x |
| `sl` | 1.24x--1.25x |
| `h-1` | 1.19x |
| `transpose` | 1.09x--1.11x |
| `many_mat_cal` | 1.08x |
| `01_mm` | 1.05x |
| `fft` | 1.01x |
| `huffman` | 0.98x |

这些本地计数包含公共运行库，只用于同机差分和定位，不替代上板计时。
每个阶段完成后都必须重新生成当前 HEAD 的本地数据；有上板条件时再更新线上基线。

## 2. 统一实现纪律

每项优化必须满足：

1. 仅按 IR 形状、数据依赖、别名和范围事实触发，禁止匹配样例名、函数名或输入值。
2. 无法静态证明时保持原程序，或者生成带原路径 fallback 的 runtime versioning。
3. 保持 SysY i32 回绕、向零截断除法、负数余数和浮点语义。
4. 先建立应触发、不应触发和边界正确性定向用例，再扩展 matcher。
5. 优化必须可单独关闭，便于 A/B、回退定位和上板比较。
6. 每项至少完成：
   - 编译器 clean build；
   - directed tests；
   - RISC-V functional、hidden-functional、60 个初赛性能样例编译/汇编；
   - 所有可运行受影响样例的输出差分；
   - 动态指令、静态指令和热点汇编对比；
   - 在本文件记录结果。

## 3. 执行顺序

### G0：测量底座和新线上基线

状态：`in progress`

- [ ] 重新提交当前 HEAD，保存新的逐样例线上时间。
- [x] 本地 benchmark 增加 `timerbench.sh`，以程序打印的
  `starttime()`--`stoptime()` 区间中位数为主指标，避免输入、输出和 libc
  稀释热点差距。
- [ ] 保存当前编译器与 Clang `-O2` 的同 IR 对照表。

验收：60/60 输出一致，基线可由单条命令复现。

### G1：单调空尾循环域收紧

状态：`completed`

目标形状：

```text
while (iv < bound) {
    if (cut <= iv) {
        iv = iv + step;
        continue;
    }
    useful_body;
    iv = iv + step;
}
```

转换为语义等价的有效域：

```text
while (iv < min(bound, cut)) {
    useful_body;
    iv = iv + step;
}
```

第一版只接受：

- 正步长、关系型顶测循环；
- `continue` 路径除同一个 IV 更新外无副作用；
- useful 路径和 continue 路径对所有 loop-carried 状态的更新一致；
- 收紧后的 IV 最终值在循环外不可观察，或单独重建原退出值；
- 所有算术均能证明不会因为边界调整改变回绕行为。

主要目标：`transpose0/1/2`。实现放在结构化 MLIR 阶段，使后续
unroll、SCEV、StrengthReduce 和 LICM 都能看到收紧后的循环域，而不是依赖 post-RA
的目标相关补丁。

定向测试：

- [x] 正向 `continue` 空尾，应触发；
- [x] `continue` 路径有 store/call，不触发；
- [x] 两条路径 IV 步长不同，不触发；
- [x] 最终 IV 在循环外使用，正确重建或不触发；
- [x] `cut < start`、`cut > bound`、零 trip、边界接近 `INT_MAX`。

实现结果：

- 新增结构化 MLIR pass `MLIRLoopDomainTightenPass`，位于 fusion 之后、
  unswitch/unroll 之前；仅识别单 carried-IV、`iv < bound`、单位步长和
  `cut < iv` 空 `continue` 后缀。
- 新边界为 `min(bound, cut + 1)`。`cut + 1` 只在 `cut < bound` 分支求值，
  因而不会在 `INT_MAX` 处引入新的溢出。
- 单独重建原循环退出 IV：`init < bound ? bound : init`，即使退出 IV
  在外层 loop-carried 状态中可观察也保持语义。
- 提供 `LOOP_DOMAIN_TIGHTEN_OFF=1` A/B 开关。
- 8 个 directed tests 全部通过；RISC-V functional 109/109、初赛性能
  57/57 通过。hidden-functional 为 39/40，唯一失败
  `30_many_dimensions` 在关闭本 pass 时同样失败，且本 pass 命中数为 0，
  是已有问题。

timer 区间中位数 A/B（5 次）：

| 样例 | 关闭 G1 | 开启 G1 | 减少 |
|---|---:|---:|---:|
| `transpose0` | 8,859 us | 5,804 us | 34.48% |
| `transpose1` | 17,054 us | 11,611 us | 31.92% |
| `transpose2` | 153,876 us | 51,012 us | 66.85% |

全进程动态指令仍保留为诊断辅证，三例分别减少
17.94%、6.90%、55.06%，但不作为主验收口径。

### G2：跨迭代内存状态与末次迭代消除

状态：`completed (first conservative scope)`

#### G2.1 Loop memory-state summary

为循环内涉及的每个 base object 计算：

- `ReadBeforeWrite`
- `MustOverwrite`
- `MayWrite`
- `LiveOut`
- 调用的参数级 Mod/Ref

摘要必须支持局部数组、非逃逸全局数组和 no-alias 指针参数；未知调用保守退化。

第一版实现为精确二维矩形摘要：

- 识别 `[0, bound) x [0, bound)` 的 must-overwrite；
- 为后续嵌套循环 IV 证明同一矩形内的访问范围；
- reset 阶段对目标对象 read-before-write、其他对象 store、调用、`break/continue`
  均使证明失败；
- 后续扩展到参数对象和一般 affine 区域并入 G5。

#### G2.2 只保留末次迭代

当下一次迭代在任何读取前完整覆盖上次迭代的可观察状态，且只有最终版本 live-out 时，
把多次循环改为零次或一次。先支持常量 trip count，再支持可求最后 IV 的运行时 trip count。

主要目标：

- `h-10` 的五次 `copy C -> B; trsm(A, B)`；
- 完全覆盖输出数组的重复 kernel；
- 在状态重置可证明后处理 `huffman` 的重复 decode。

已实现 `MLIRLoopMemoryLastIterationPass`：常量重复循环的结果和重复 IV
均不可观察，且上述矩形 reset 覆盖后续全部访问时，只执行最后一轮。
提供 `LOOP_MEMORY_LAST_OFF=1` A/B 开关。

timer 区间中位数 A/B（5 次）：

| 样例 | 关闭 | 开启 | 减少 |
|---|---:|---:|---:|
| `h-10-01` | 16,497 us | 4,736 us | 71.29% |
| `h-10-02` | 35,420 us | 7,429 us | 79.03% |
| `h-10-03` | 70,374 us | 14,218 us | 79.80% |

#### G2.3 循环不变归约闭式化

支持 `acc += invariant_value`、`acc = acc op invariant_value` 等归约，保持 i32 回绕。

主要目标：`many_mat_cal` 最后的 R 次相同全矩阵归约。

已实现 `MLIRLoopInvariantRepeatPass`：只读、无调用的外层重复循环若仅有
一个 live integer accumulator，且数据流严格保持 `acc -> acc + delta`
或 `acc -> acc - delta`、`delta` 与外层 IV/acc 无关，则 body 至多执行一次，
最终值用 `init + trip_count * (once - init)` 在 i32 模语义下重建。
运行时非正 trip count 保持零次执行；提供
`LOOP_INVARIANT_REPEAT_OFF=1` A/B 开关。

`many_mat_cal-1/2/3` 的 15 次 timer 中位数分别减少
0.80%、0.52%、1.34%；这是小收益，动态指令减少约 1.0%--1.6% 仅作辅证。

G2 验证：

- 10 个 directed tests 全部通过，包含 runtime/negative trip、非线性
  accumulator、store、外层 IV 依赖、部分 reset、reset 读旧值和其他对象写入。
- 官方 RISC-V 初赛性能集 60/60 输出正确。
- memory-last 只命中 `h-10-01/02/03`；repeat 在性能集只命中
  `many_mat_cal-1/2/3`。

### G3：通用 affine loop-nest 优化

状态：`completed (first conservative scope)`

#### G3.1 依赖分析

- SCEV affine access 表达式；
- 方向向量和 loop-carried dependence；
- no-alias 参数及全局对象；
- 不能证明时拒绝变换。

复用现有 MLIR loop fusion 的 memory-effect/no-alias 基础。

#### G3.2 Loop interchange

在 dependence 允许时交换循环，使最内层访问尽量 stride-1，并暴露归约。

#### G3.3 Strip-mine、unroll-and-jam 和 scalar promotion

把当前只识别特定四列矩阵点积的 matcher 推广为：

- 任意安全的标量整数/浮点归约；
- 全局数组和 no-alias 参数数组；
- 由目标寄存器压力决定 2/4/8 路 block；
- 主循环加标量 remainder；
- 累加器跨 reduction loop 留在寄存器，最终只 store 一次。

主要目标：`01_mm`、`many_mat_cal`、`matmul`。

当前已完成两个按依赖形状触发的保守子集：

1. `MLIRConditionalJamPass`
   - 识别常量列域为 4 的倍数、只读且带条件分支的标量归约；
   - 一次处理相邻 4 列，共用 reduction IV，并携带 4 个独立 accumulator；
   - 输出对象在归约中被读取、归约有其他写入/调用、列数非 4 的倍数时拒绝；
   - 后续 partial unroll 只对“5 个结果且恰有 4 个条件 lane”的结构使用
     RISC-V factor 4，避免用结果数这一宽泛条件影响无关循环；
   - 提供 `CONDITIONAL_JAM_OFF=1`。
2. `MLIRArrayPipelinePass`
   - 识别“逐行只读标量归约 -> 整行广播 -> 原位负转置 -> 全矩阵求和”；
   - 利用该原位遍历的精确代数关系，将后三个 materialization loop 改为
     行归约产生值的直接累加；
   - 严格校验同一 base object、完整方形域、准确索引置换、无额外
     store/call，以及求和初值在 producer 前可用；
   - 提供 `ARRAY_PIPELINE_OFF=1`。

`starttime()`--`stoptime()` 区间计时结果：

| 变换 | 样例 | 关闭 | 开启 | 减少 |
|---|---|---:|---:|---:|
| array pipeline，9 次中位数 | `matmul1` | 35,885 us | 35,304 us | 1.62% |
| array pipeline，9 次中位数 | `matmul2` | 124,876 us | 121,951 us | 2.34% |
| array pipeline，9 次中位数 | `matmul3` | 71,112 us | 70,255 us | 1.21% |
| conditional jam，31 次交错中位数 | `matmul1` | 35,584 us | 33,038 us | 7.15% |
| conditional jam，31 次交错中位数 | `matmul2` | 124,789 us | 114,165 us | 8.51% |

conditional jam 不命中 `matmul3`。它在 `matmul1/2` 上增加了全进程动态
指令数，但 timer 区间稳定下降；按照本计划的计时优先原则保留，指令数只用于说明
其收益来自控制流/访存局部性而非简单的指令删除。

G3 当前验证：

- array pipeline directed 4/4，覆盖正例、晚定义初值、部分广播和错误转置；
- conditional jam directed 4/4，覆盖正例、非 4 倍列数、读取输出对象和额外写入；
- RISC-V functional 109/109；hidden-functional 39/40，唯一失败仍是已有的
  `30_many_dimensions`，G3 开关前后汇编完全一致；
- 149 个 functional/hidden-functional 全部编译成功，两个 matcher 均无命中；
- 官方性能集 timer 3 次中位数：60/60 正确且均取得有效计时；
- array pipeline 只命中 `matmul1/2/3`，conditional jam 只命中
  `matmul1/2`。

#### G3.4 Interior/border loop versioning

把 stencil/convolution 的 interior 与 border 拆开，interior 删除边界分支；
对小常量 kernel 完全展开并传播 `-1/0/1` 系数。

主要目标：`conv2d`。

已实现 `MLIRStencilInteriorPass`：

- 识别二维输出域上的常量奇数核归约；
- 从短路条件数据流证明 guard 精确等价于
  `0 <= row+kr-pad < bound && 0 <= col+kc-pad < bound`；
- 为每个输出点生成 interior 快路径与原 border 慢路径；快路径将 guard
  专门化为真，使既有 full-unroll 删除核内全部边界分支；
- 仅支持 3--15 的常量奇数核、同一标量 bound、单位步长和只读 kernel body；
  额外谓词、循环内 store/call、偶数核或非精确坐标表达式均拒绝；
- 提供 `STENCIL_INTERIOR_OFF=1`。

21 次交错 timer 中位数：

| 样例 | 关闭 | 开启 | 减少 |
|---|---:|---:|---:|
| `conv2d-1` | 33,429 us | 20,425 us | 38.90% |
| `conv2d-2` | 9,312 us | 6,033 us | 35.21% |
| `conv2d-3` | 3,471 us | 2,345 us | 32.44% |

汇编行数从 792 增至 895，这是有意的 border fallback 代码复制；timer 是主要
取舍依据。全进程动态指令分别从 170,167,332/46,402,242/16,279,317 降至
106,883,275/29,412,737/10,456,660，仅作为快路径确实删除核内控制流的辅证。

3 个 stencil directed tests 覆盖正例、偶数核和额外数据谓词拒绝。官方性能集
仅命中 `conv2d-1/2/3`；149 个 functional/hidden-functional 全部成功编译且
无命中。加入该变换后的官方 timer sweep 仍为 60/60 正确。

### G4：Value range、KnownBits 和算术 idiom

状态：`completed (first conservative scope)`

#### G4.1 Range/KnownBits analysis

通过常量、phi、比较、分支和调用参数传播：

- signed/unsigned interval；
- non-negative/non-zero；
- 已知零位/一位；
- no-wrap 条件。

#### G4.2 已证明非负的除法与奇偶化简

- 正数 `/ 2^k` 直接算术/逻辑移位；
- `(a*b) % 2 == 0` 变为低位测试；
- 删除范围内恒真/恒假的 `max/min` 和边界判断。

主要目标：`h-1`、`matmul`、`h-4`、`sort`。

第一版实现为更强、且不要求非负的分支敏感整除事实：

- 新增 BaseIR `DivisibilitySimplifyPass`；
- 若到达块的唯一 CFG 边证明 `x % 2^k == 0`，把块内
  `x / 2^k` 改为算术右移；
- 该等价对所有正负 i32 成立，因为被除数已证明整除，不需要有符号除法的
  向零修正；
- 支持 `== 0` 真边和 `!= 0` 假边，除数或值不一致时拒绝；
- 提供 `DIVISIBILITY_SIMPLIFY_OFF=1`。

21 次交错 timer 中位数：

| 样例 | 关闭 | 开启 | 减少 |
|---|---:|---:|---:|
| `h-1-01` | 12,371 us | 10,989 us | 11.17% |
| `h-1-02` | 1,040 us | 925 us | 11.06% |
| `h-1-03` | 70,895 us | 62,638 us | 11.65% |

动态指令诊断从 33,484,569/2,827,576/193,334,217 降至
28,457,095/2,421,164/163,971,239；`h-1-03` 与同 IR Clang `-O2`
的差距由约 1.196x 收敛到约 1.014x。

该变换还命中 `fft` 每例 9 处和 `h-9` 每例 2 处；15 次 timer 中位数
只有 0%--3% 小幅变化，不将其宣传为主要收益。3 个 directed tests 覆盖
真边、`!=` 假边和不一致除数拒绝。功能集只命中 `85_long_code`，开关两侧
输出一致且动态指令小幅下降。

#### G4.3 Modular multiply idiom

识别递归/循环 double-and-add modular multiply；仅当范围证明中间值和 i64 乘积安全时，
替换为 i64 multiply + remainder。

主要目标：`fft0/1/2`。

当前同 IR 动态指令比仅约 1.01x；在现有 i32 BaseIR 中没有可证明不溢出的
i64 乘积表达能力，暂不为这一低差距热点引入不安全替换。

#### G4.4 软件位运算 idiom

在参数已证明满足算法前置条件时，把逐位 `_and/_or/_xor` 循环替换成原生位运算。
负数或无法证明的调用保留原实现。

主要目标：`huffman`，并为其他软件位运算样例提供通用覆盖。

审计发现 `crypto` 中名为 `_and/_or/_xor` 的函数并不实现真实位运算
（例如 `_and(a,b)` 实际返回 `a+b`），因此禁止按函数名替换。后续只能按
完整、可证明的算术数据流做代数化简。

#### G4.5 循环不变运行时除数

在循环外计算 signed division magic/reciprocal，在热循环内用 `mulh + add + shift`
替代 `divw/remw`；处理 `0`、`-1`、`INT_MIN` 和符号边界，并保留 fallback。

主要目标：`sl` 的 `/f`、`shuffle` 的 `%hashmod`。

`shuffle1` 当前同 IR 动态指令比已约 1.005x；`sl` 仍约 1.249x，但通用
runtime magic division 需要 i64 high-multiply 及完整的 `0/-1/INT_MIN`
fallback。该能力与后端宽乘支持一起移至 G6，避免为样例输入 `f=3`
做常量特化。

### G5：跨循环数组需求分析

状态：`completed（第一阶段）`

- 跨基本块和相邻循环的 array DSE；
- affine index-set 覆盖/相交；
- producer-consumer fusion；
- 只生成最终真正被读取的数组区域；
- 将“写数组后仅做归约”改为直接归约，避免中间 materialization。

主要目标：`transpose`、`matmul` 后处理、`conv2d` 的 `row_reduce + checksum`。

此阶段风险最高，必须从精确 base object、常量区间和无逃逸数组开始。

第一阶段实现 `MLIRRowChecksumPass`，处理如下通用相邻循环：

```text
for row in [0, N):
  row_sum = sum(A[row * N + col])
  A[row * N + col] -= row_sum
checksum = sum(A[i]), i in [0, N * N)
```

若 checksum 后不再观察 `A`，则按 i32 模算术有：

```text
checksum = sum_row((1 - N) * row_sum)
```

因此快路径保留行归约，删除整行写回和随后的全数组 checksum。它不依赖函数名、
全局名或常量输入；匹配器逐项证明共同 base object、零基 unit-step 循环、
`row*N+col` 下标、精确减法更新、`N*N` checksum 域以及 checksum 后数组不再
逃逸/读取。为保持任意输入语义，仅在运行时 `0 < N <= 46340` 时走闭式快路径；
其余情况执行原循环，从而避免 `N*N` 的有符号溢出改变原迭代域。开关为
`ROW_CHECKSUM_OFF=1`。

21 轮交错的评分区间中位数：

| 样例 | 关闭 | 开启 | 降幅 |
|---|---:|---:|---:|
| `conv2d-1` | 20,655 us | 19,956 us | 3.38% |
| `conv2d-2` | 5,943 us | 5,773 us | 2.86% |
| `conv2d-3` | 2,354 us | 2,280 us | 3.14% |

四个 directed 用例覆盖正例、更新算子拒绝、checksum 后可观察拒绝和域不一致拒绝，
在 RISC-V/AArch64 的 O0/O1 下 4/4 通过。官方 60 例仅命中三个 `conv2d`；
100 个 functional 与 40 个 hidden-functional 全部成功编译且零命中。加入该
变换后的官方三轮 timer sweep 为 60/60。

更一般的跨基本块 array DSE、非方阵 affine index-set 和部分区域
producer-consumer fusion 保留为后续扩展，不在缺少完整别名/逃逸证明时放宽。

### G6：RISC-V 后端收尾

状态：`completed（本地可证明范围）`

高层变换稳定后再处理：

- loop-depth/频率加权 spill cost；
- live-range splitting 与 rematerialization；
- post-RA copy/branch 清理；
- 轻量 load-use/multiply latency scheduling；
- 与 Clang 同 IR 差距较大的 `conv2d`、`sort`、`matmul`、`crypto` 优先。

不以静态指令数作为调度类优化的唯一判据；BOOM 为乱序核，必须结合上板周期。

最终审计表明首要问题仍在后端之前：结构化 partial unroll 和闭式循环化简发生在
早期 GVN 之后，产生了大量等价的除法边界、动态移位和地址表达式；旧流水线直接把
这些冗余交给 strength reduction 和寄存器分配。新增 `PostDLEGVNPass`，在
DeadLoopElim 之后、StrengthReduce 之前刷新 CFG/支配/MemorySSA 并再次执行 GVN。
同时把 `shl/lshr/ashr` 纳入 GVN 的纯表达式集合。开关为
`POST_DLE_GVN_OFF=1`。

21 轮交错评分区间中位数：

| 样例 | 关闭 | 开启 | 降幅 |
|---|---:|---:|---:|
| `03_sort1` | 18,443 us | 14,752 us | 20.01% |
| `03_sort2` | 18,467 us | 14,861 us | 19.53% |
| `03_sort3` | 18,390 us | 14,760 us | 19.74% |
| `crypto-1` | 132,152 us | 91,486 us | 30.77% |
| `crypto-2` | 92,894 us | 64,485 us | 30.58% |
| `crypto-3` | 53,316 us | 37,107 us | 30.40% |
| `conv2d-1` | 20,043 us | 19,857 us | 0.93% |

`ReassociatePass` 另增加严格的 i32 模线性消项：仅展开同一块内、所有使用都位于
当前表达式树中的 `add/sub/neg`，合并系数后只有在至少净删两条算术时才重建。
它不按 `_and/_xor` 等函数名匹配；`crypto` 中的伪位运算恰好是可证明的线性式，
因此自然化简。开关为 `REASSOCIATE_LINEAR_OFF=1`。在 PostDLEGVN 已开启的
基础上，21 轮额外收益为 `crypto-1/2/3` 的 6.19%/6.73%/6.44%；sort、
conv2d 的变化在 ±1.4% 噪声内。

二次 GVN 会改变后续 SCEV/指针强度削弱的选择；代表性保护组 21 轮中
`01_mm1`、`h-10-03`、`sl1`、`transpose2` 的变化为
-1.97%/-1.26%/-1.03%/-5.14%（正数表示加速）。这是当前方案明确保留的
计时权衡：六个 sort/crypto 样例的 19%--31% 收益显著大于保护组波动，但不能把
“表达式更少”等同于每个样例都更快；要求线上复测后再决定是否按压力模型拆分
二次 GVN。

新增两个 directed 用例覆盖相同 repeated-division 闭式的复用和带负数/回绕边界的
线性消项。最终验证：

- 新增 G5/G6 directed 均在 RISC-V/AArch64、O0/O1 下通过；
- 全量 directed 96/96；
- RISC-V 与 AArch64 的 functional 均为 109/109，hidden-functional
  均为 40/40，且 O0/O1 四种组合全部通过；
- 官方 performance 三轮 `starttime`--`stoptime` sweep 60/60。

### G7：全样例零失败收尾

状态：`completed`

`30_many_dimensions` 是 O1 的真实错编译，而不是期望输出问题。MLIR 内联器原先
只检查调用实参和形参都是 `PTR`，随后直接用实参 SSA 值替换形参。数组切片实参
仍携带外层数组的 pointee 类型，而形参携带切片后的精确类型；替换后 LLVM GEP
按外层数组大小计算 stride，例如应为 8 字节的 `a1[1][0]` 被放大为 1 MiB。
内联器现在要求指针实参和形参具有同一个由 `TypeContext` 驻留的布局类型，否则
保留正常函数调用。普通同型数组参数仍可内联，不安全的切片调用不会再改变寻址
语义。该样例现已在 RISC-V/AArch64、O0/O1 下都输出 `351799`。

两个 directed 失败属于断言落后于优化结果：

- `overwrite_same_slot` 的最终 store 已不可观察，且值已转发给 `putint`，所以
  正确结果是零条 `var.set`；
- `full_unroll_small` 的循环 header/body/latch 已全部消失，仅 exit block 保留
  源码派生名称，因此断言改为检查真正的循环块而非笼统匹配 `main_while`。

最终验收为 directed 96/96；双架构 functional 109/109、hidden-functional
40/40（O0/O1）；RISC-V 初赛性能计时 60/60。

最终以当前 LLVM IR 再交给 Clang `-O2` 的 5 轮交错区间比为：

| 样例 | 当前 / Clang |
|---|---:|
| `crypto-1` | 1.73x |
| `03_sort1` | 1.63x |
| `transpose2` | 1.61x |
| `conv2d-1` | 1.47x |
| `huffman-01` | 1.36x |
| `sl1` | 1.26x |
| `matmul1` | 1.16x |

这些残差不能直接归因于寄存器分配：Clang `-O2` 会继续改写相同输入 IR。
runtime signed magic division、live-range splitting 和 BOOM latency scheduling 仍需
真实上板 PMU/周期数据；在只有 QEMU `starttime` 区间时不实施可能牺牲其他样例的
目标相关启发式。

## 4. 进度记录

| 日期 | 阶段 | 变更 | 正确性 | 性能 | 结论 |
|---|---|---|---|---|---|
| 2026-07-29 | baseline | 当前 HEAD 与同 IR Clang `-O2` 对照 | 60/60 | total 1.063x, geomean 1.122x | 高层循环/内存状态优先 |
| 2026-07-29 | G1 | 单调空 `continue` 后缀转为紧循环域，并重建原退出 IV | directed 8/8；functional 109/109；官方 performance 60/60；hidden 仅已有 `30_many_dimensions` 失败 | timer：`transpose0/1/2` 分别减少 34.48%/31.92%/66.85% | 收益显著，匹配仅命中三个 transpose 样例 |
| 2026-07-29 | G2 | 重复只读归约缩放；二维 reset 覆盖后的末轮化 | directed 10/10；官方 performance 60/60 | `h-10` timer 减少 71.29%--79.80%；`many_mat` 减少 0.52%--1.34% | memory-state 末轮化是主收益，归约缩放为小幅通用收益 |
| 2026-07-29 | G3.3 | 四路条件归约 jam；行归约到后续 materialization/sum 的直接流水 | directed 8/8；functional 109/109；官方 performance 60/60；hidden 仅已有失败 | `matmul1/2` jam 减少 7.15%/8.51%；pipeline 三例减少 1.21%--2.34% | timer 收益稳定；继续处理 conv interior/border |
| 2026-07-29 | G3.4 | 常量奇数 stencil 的 interior/border versioning | directed 3/3；官方 performance 60/60；功能集无命中 | `conv2d-1/2/3` timer 减少 38.90%/35.21%/32.44% | 允许小幅代码增长换取热点边界分支消除 |
| 2026-07-29 | G4 | 分支敏感的整除 KnownBits：`x % 2^k == 0` 支配 `x / 2^k` | directed 3/3；官方命中 h-1/fft/h-9；功能命中 1 例且正确 | `h-1-01/02/03` timer 减少 11.17%/11.06%/11.65% | 完全抹平 h-1 的主要同 IR 差距；不做输入值特化 |
| 2026-07-29 | G5.1 | `row_reduce` 写回与后继全数组 checksum 闭式融合，带 `N*N` 安全版本化 | directed 4/4；官方 performance 60/60；功能集零命中 | `conv2d-1/2/3` timer 减少 3.38%/2.86%/3.14% | 仅在最终数组不可观察时删除 materialization |
| 2026-07-29 | G6 | DLE/unroll 后二次 GVN；至少净删两条算术的模线性消项 | 新增 directed 2/2；functional 109/109；官方 60/60 | sort 减少 19.53%--20.01%；crypto 二次 GVN 减少 30.40%--30.77%，线性消项再减少 6.19%--6.73% | 计时主导去留；不在无上板数据时添加激进调度 |
| 2026-07-29 | G7 | 拒绝 pointee 布局不一致的 MLIR 指针实参内联；更新两条过时 IR 断言 | directed 96/96；双架构 functional 109/109、hidden 40/40，O0/O1；官方 60/60 | 三轮计时 sweep 全部取得有效区间 | 全部本地样例零失败 |
