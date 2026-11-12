# 用 JSON 新增功能牌

辅助工具：[tools/import_function_card.py](../tools/import_function_card.py)。只需要 Python 3.10 或更新版本，不需要安装 Python 第三方包。工具在开发时读取 JSON，生成 C++ 文件并加入卡池；运行中的服务器不热加载 JSON。

## 1. 准备 JSON

参考完整示例 [稳健收益](../cards/function/steady_income.json)。每个 `abilities` 条目只描述一个原子技能，加分与获得金币分成两项：

```json
{
  "version": 1,
  "type": "function",
  "id": 2002,
  "key": "play_reward",
  "name": "出牌奖励",
  "price": 4,
  "minimum_shop_level": 1,
  "description": "自己出牌后获得 2 分和 1 金币。",
  "abilities": [
    {"event": "PlayCommitted", "scope": "self", "op": "add_score", "amount": 2},
    {"event": "PlayCommitted", "scope": "self", "op": "gain_coins", "amount": 1}
  ]
}
```

| 字段 | 说明 |
|---|---|
| `version` | 固定为 `1` |
| `type` | 固定为 `"function"`；当前工具只生成持续功能牌 |
| `id` | 唯一正整数，最大 `9007199254740991`；不得使用已有 JSON 或内置牌编号 |
| `key` | 文件和生成类的稳定标识，只用小写英文字母、数字、单个下划线，以字母开头，最多 64 字符 |
| `name` | 展示名称，1～128 字符 |
| `price` | 非负整数金币价格，最大为有符号 64 位整数上限 |
| `minimum_shop_level` | 可选，默认 `1`，范围 `1～3` |
| `description` | 单行说明，1～2048 字符；请与实际技能参数保持一致 |
| `abilities` | 1～64 个原子技能，按数组顺序结算 |

每个技能必须包含 `event`、`op`、正整数 `amount`。`scope` 可省略，默认为 `self`。数据要求 UTF-8，可带 BOM；文件最大 256 KiB。未知字段、重复 JSON 字段、布尔数值、负数、溢出和非法文件名都会报错。

## 2. 事件与技能

### 成功通知

以下事件支持 `add_score` 和 `gain_coins`，直接给牌的持有者加分或金币：

| 事件 | 触发时机 |
|---|---|
| `PlayCommitted` | 成功出牌后 |
| `PassCommitted` | 确认不出后 |
| `ShopEnter` / `ShopLeave` | 进入 / 结束购物 |
| `TurnStart` / `TurnEnd` | 玩家回合开始 / 结束 |
| `DrawAfter` | 非初始实际摸牌后 |
| `RoundStart` / `RoundEnd` | 每局开始 / 结束 |
| `TrickStart` / `TrickEnd` | 每巡开始 / 结束 |
| `GameEnd` | 整场结束通知；发生在冠军已确定后，不用于追加影响冠军的分数 |

对于带玩家的事件，`scope: "self"` 只响应持有者的操作；`scope: "any_player"` 响应任意玩家，但资源仍给持有者。例如“其他玩家或自己出牌后，自己 +1 分”可用 `any_player`。目前没有单独的“仅其他玩家”条件。

局、巡、整场事件是全局通知，使用 `self`，表示持有者在每次全局通知获得一次效果；不按事件的 actor 推断赢家或先手。

### 待结算奖励

`PlayRewardBefore`、`RoundRewardBefore` 支持 `add_pending_score`、`add_pending_coins`，修改当前出牌或局末奖励；不会当场给玩家增加资源。

```json
{"event": "PlayRewardBefore", "op": "add_pending_score", "amount": 3}
```

### 商店报价

`ShopBuyQuote`、`ShopRefreshQuote` 支持 `reduce_shop_cost`，可带非负整数 `minimum`，默认 `0`：

```json
{"event": "ShopRefreshQuote", "op": "reduce_shop_cost", "amount": 1, "minimum": 1}
```

预览只修正报价或待结算奖励，不实际发放资源。不允许在报价事件使用 `gain_coins` 或 `add_score`，避免失败购买或反复查询产生资源。客户端请求、资源变化等其他事件和其他原子操作暂不接受；需要在生成工具中补充明确的参数与事件映射后使用。此版本生成被动事件牌，不生成主动选目标、条件表达式或任意 C++ 代码。

## 3. 导入和编译

在项目根目录运行，输入文件可放在任意可读位置：

```text
python tools/import_function_card.py path/to/play_reward.json
cmake --build build --parallel 4
```

工具自动维护以下文件：

```text
cards/function/play_reward.json                 # 项目中的标准 JSON 定义
include/cards/generated/play_reward.hpp         # FunctionCards 派生类与工厂声明
src/cards/generated/play_reward.cpp             # 技能实现
src/CardCatalog.cpp                            # 所有导入卡牌的工厂注册
```

CMake 会发现 `src/cards/generated/` 中新增的实现文件。默认卡池会包含导入的牌，商店根据最低等级生成；手工指定 `SchedulerConfig.catalog` 时仍以调用方的显式卡池为准，可以自行合并 `generated_function_cards()`。

生成牌沿用 `Register`、`HandleEvent` 和 `CardEffects`：一张牌的全部原子技能先结算，再处理产生的后续事件。数值强化可增加每个对应得分或金币技能的收益；没有对应数值技能的牌会拒绝该种数值强化。附加新技能仍使用已有强化接口。

## 4. 检查和更新

```text
python tools/import_function_card.py path/to/play_reward.json --check
python tools/import_function_card.py path/to/play_reward.json --force --check
python tools/import_function_card.py path/to/play_reward.json --force
```

`--check` 仅校验并列出将修改的文件，不写入。重复导入完全相同的定义不会重复注册，也不重写文件。更新既有牌需要 `--force`，编号和 key 保持不变；新牌应使用新的 key 和编号。生成文件会保留身份标记以阻止误改实例定义身份。

请编辑 JSON 后重新生成，保留 `// Generated ...` 文件交给工具管理。工具拒绝覆盖没有生成标记的手写 C++ 文件；普通校验失败不会修改输出文件，文件替换失败时会尝试恢复已替换的内容。该工具不删除卡牌、也不负责发布服务器；重新编译后按原部署流程发布本地产物。

内部实现与扩展约定统一记录在 [design.md 第 13 节](../design.md#13-json-功能牌导入工具)。
