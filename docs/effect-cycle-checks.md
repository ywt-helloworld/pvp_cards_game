# 功能牌效果循环检查

目的：提交代码时，拒绝可能不断自动触发的卡牌设计，不必先启动游戏碰到该组合。入口是 [检查脚本](../tools/check_effect_cycles.py)，独立的 GitHub Actions 作业叫 **Effect design safety**，结果也纳入 **PR checks**。

## 1. 从技能得到事件连线

JSON 已经声明触发事件 `event` 和原子操作 `op`，检查器根据原子操作的真实接口推导后续事件：

| 操作 | 可能立即产生的事件 |
|---|---|
| `add_score` | `ScoreChanged` |
| `gain_coins` | `CoinsChanged` |
| `add_pending_score` | 无；只修改待结算快照 |
| `add_pending_coins` | 无；只修改待结算快照 |
| `reduce_shop_cost` | 无；只修改本次报价快照 |

例如“出牌后加分”是一条 `PlayCommitted → ScoreChanged`。读取全部 JSON 定义和手写效果摘要后，把这些连线合并成同一张有向图，所以两张分别安全、组合后成环的牌也会被检查。

新增原子操作时必须补全 `ACTION_OUTPUTS`。映射与生成器支持的操作不一致时直接失败，不能把未知操作当成“不产生事件”。

## 2. 查找环并报错

`cycle_path()` 使用深度优先搜索。每个事件有“尚未访问、当前路径中、已经完成”三种状态。走到当前路径中的事件说明已经闭环；走到已经完成的事件是正常汇合。

例如 A 牌“金币变化后加分”，B 牌“分数变化后加金币”，会报告：

```text
Potential automatic effect cycle (card design rejected):
  CoinsChanged -- A / ability 1 (cards/function/a.json) --> ScoreChanged
  ScoreChanged -- B / ability 1 (cards/function/b.json) --> CoinsChanged
```

CLI 返回退出码 1，导致本次检查失败。错误包含完整环路、牌名、技能序号和定义文件；检查器不删除牌、不改数值、不自动修复。

先分析事件图，再执行 JSON 的完整字段和事件白名单校验。因此即使某种反馈事件当前尚未开放给 JSON，错误示例也可以报告具体环路；通过环检查并不意味着该事件已被导入器支持。

## 3. 手写 C++ 的边界

任意 C++ 回调的所有分支、循环和终止条件无法靠这个检查器可靠推断。手写实现的可能输出记录在 [native_effects.json](../cards/native_effects.json)，包括 Demo 牌、强化附加技能、系统资源牌、系统发牌牌和系统流程的即时效果摘要。这里列出的是可能边，不是一次运行观察到的边；条件成立才触发的输出也应包含。

该文件的 `source_sha256` 绑定 `include/`、`src/` 下全部非生成 C++ 源码及头文件和 JSON 生成器。源码内容改变、新增或删除文件而未复核摘要时，检查失败；CRLF 与 LF 统一后计算散列，不受 Windows 检出方式影响。JSON 生成文件由现有的生成一致性检查负责。

修改手写技能时，先审查所有可能触发和输出、更新摘要并补充测试，再更新相关源码的散列。计算单个文件散列可用：

```text
python -c "import hashlib,pathlib;print(hashlib.sha256(pathlib.Path('src/SystemCards.cpp').read_text(encoding='utf-8-sig').encode('utf-8')).hexdigest())"
```

散列只强制进行复核，不能证明摘要与 C++ 实现完全一致；错误或遗漏的摘要仍可能漏报。不要只更新散列来消除报错。

系统摘要包含交易、结算和阶段衔接的即时事件。`TurnStart` 中断线玩家自动进入超时处理的分支是既有调度器的受审查进度边界：它依赖有限出牌、巡末摸牌和局数推进，仍由原生完整对局测试及运行时预算保护，不把这条状态推进边当作无限卡牌反馈。新增会改变这些进度假设的手写效果必须重新审查该边界。

## 4. 保守规则与限制

第一版拒绝模型中所有自动事件环，不按价格、最低商店等级、剩余槽位、玩家归属、概率或“最终会溢出”消除边。它检查可能的危险设计，不声称已证明每个环在实际状态下一定无限运行。

“每局最多一次”“耗尽计数后停止”等有界技能也不自动豁免；当前 JSON 尚无可验证的次数约束。未来若支持这类设计，需要把终止条件接入模型并验证实际扣减，不能仅添加 `safe: true` 跳过检查。

正常游戏需要玩家的新操作才能继续时，检查器不会凭空添加“玩家下一次点击”的连线。检查只针对声明的即时事件模型；不分析任意 C++ 函数内部的 `while` 死循环，也不替代源码审查和运行时事件预算。

## 5. 本地验证与 PR

在仓库根目录执行：

```text
python tools/check_effect_cycles.py
python tests/effect_cycles_test.py
```

第一条检查当前卡池；第二条在临时目录构造坏设计，验证它们确实被拒绝，同时验证正常设计可以通过。当前回归覆盖自循环、两牌和三牌组合、JSON 与手写牌组合、系统资源请求中转、正常汇合、快照修改、新操作遗漏、C++ 源码变化和 CLI 的非零退出码。

`python tools/check_pr.py --suite native` 和完整检查也会执行设计检查，CTest 中还会显示 `EffectCycleContracts`。新建或更新 PR、向 main 推送会自动运行 **Effect design safety**；该作业失败时，汇总 **PR checks** 也失败。此流程不更新线上游戏实例。
