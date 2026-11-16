# 完整原子技能索引

**62 个公开接口**均位于 [`cardgame::abilities`](../include/Abilities.hpp)，不包含内部 `detail` 辅助函数。下表列出中文名称、准确的函数名、参数顺序和返回通知；参数类型与默认值点击函数名查看。完整设计见 [design.md](../design.md#3-技能事件处理)。

当前 [JSON 生成器](function-card-json.md#2-json-可用事件与技能)只开放其中 **5 个操作**，其 `op` 名称在表中标出。“—”表示不能直接填入 JSON，需要 C++ 组合或扩展生成器。完整事件名称见[事件索引](event-system.md)。

## 调用与返回

- 参数含 `operation` 的模板接口要求业务模块提供回调，真正执行操作并填写事实数据；`deal_initial_cards` 同样依赖 `deal` 回调。声明接口不表示当前测试服务器已实现所有卡包、状态等玩法。
- `EventPtr` 表示返回一个通知，未执行或没有变化时部分接口会返回空指针；`EventList` 表示多个通知。返回值不会自动派发，调用方使用 `CardEffects::emit` 缓冲，整张牌的技能执行完后才处理后续事件。
- `bool` 表示是否改变数值或快照，这些接口不生成通知。收益计算、报价修改和资源到账是不同操作。

示例：`effects.emit(abilities::add_score(player, 2), 1);` 中的 `1` 是牌内技能序号；本次加分产生 `ScoreChanged`，它本身不是技能名称。

## 完整技能表

### 分数与金币

| 技能名称 | C++ 函数与参数顺序 | 返回或后续事件 | JSON op |
|---|---|---|---|
| 增加分数 | [add_score](../include/Abilities.hpp#L12)<br>`add_score(player, amount)` | `ScoreChanged` | `add_score` |
| 扣除分数 | [subtract_score](../include/Abilities.hpp#L13)<br>`subtract_score(player, amount)` | `ScoreChanged` | — |
| 设置分数 | [set_score](../include/Abilities.hpp#L14)<br>`set_score(player, value)` | `ScoreChanged` | — |
| 获得金币 | [gain_coins](../include/Abilities.hpp#L15)<br>`gain_coins(player, amount, reason)` | `CoinsChanged` | `gain_coins` |
| 消耗金币 | [spend_coins](../include/Abilities.hpp#L16)<br>`spend_coins(player, amount, reason)` | `CoinsChanged` | — |
| 设置金币 | [set_coins](../include/Abilities.hpp#L17)<br>`set_coins(player, value, reason)` | `CoinsChanged` | — |

### 待结算奖励与商店报价

| 技能名称 | C++ 函数与参数顺序 | 返回或后续事件 | JSON op |
|---|---|---|---|
| 增加待结算分数 | [add_pending_score](../include/Abilities.hpp#L18)<br>`add_pending_score(reward, delta)` | bool；不生成事件 | `add_pending_score` |
| 缩放待结算分数 | [multiply_pending_score](../include/Abilities.hpp#L19)<br>`multiply_pending_score(reward, numerator, denominator, rounding)` | bool；不生成事件 | — |
| 修正待结算金币 | [add_pending_coins](../include/Abilities.hpp#L21)<br>`add_pending_coins(component, delta)` | bool；不生成事件 | `add_pending_coins` |
| 降低商店报价 | [reduce_shop_cost](../include/Abilities.hpp#L22)<br>`reduce_shop_cost(quote, amount, minimum)` | bool；不生成事件 | `reduce_shop_cost` |

### 商店、设施与卡包

| 技能名称 | C++ 函数与参数顺序 | 返回或后续事件 | JSON op |
|---|---|---|---|
| 增加免费刷新次数 | [grant_free_refresh](../include/Abilities.hpp#L23)<br>`grant_free_refresh(credits, count)` | bool；不生成事件 | — |
| 解锁功能牌槽位 | [grant_function_slot](../include/Abilities.hpp#L24)<br>`grant_function_slot(player)` | bool；不生成事件 | — |
| 升级商店 | [upgrade_shop](../include/Abilities.hpp#L25)<br>`upgrade_shop(player, amount)` | `ShopLevelChanged` | — |
| 扩容货架 | [expand_shop_slots](../include/Abilities.hpp#L54)<br>`expand_shop_slots(player, amount, operation)` | `ShopSlotsChanged` | — |
| 刷新商店 | [refresh_shop](../include/Abilities.hpp#L59)<br>`refresh_shop(player, shop, mode, operation)` | `ShopRefreshAfter` | — |
| 打开卡包 | [open_booster_pack](../include/Abilities.hpp#L64)<br>`open_booster_pack(player, shop, pack, operation)` | `BoosterPackOpened` | — |
| 锁定货位 | [lock_shop_slot](../include/Abilities.hpp#L69)<br>`lock_shop_slot(player, shop, slot, operation)` | `ShopSlotLocked` | — |
| 解锁货位 | [unlock_shop_slot](../include/Abilities.hpp#L74)<br>`unlock_shop_slot(player, shop, slot, operation)` | `ShopSlotUnlocked` | — |
| 加强牌型 | [upgrade_hand_pattern](../include/Abilities.hpp#L149)<br>`upgrade_hand_pattern(player, pattern, levels, operation)` | `HandPatternUpgraded` | — |
| 使设施生效 | [purchase_facility](../include/Abilities.hpp#L154)<br>`purchase_facility(player, facility, operation)` | `FacilityPurchased` | — |

### 功能牌与技能管理

| 技能名称 | C++ 函数与参数顺序 | 返回或后续事件 | JSON op |
|---|---|---|---|
| 强化功能牌数值或追加技能 | [enhance_function_card](../include/Abilities.hpp#L27)<br>`enhance_function_card(registry, player, slot, enhancement)` | `FunctionCardEnhanced` | — |
| 装备功能牌 | [equip_function_card](../include/Abilities.hpp#L34)<br>`equip_function_card(registry, player, slot, card)` | `FunctionCardEquipped` | — |
| 移除功能牌 | [remove_function_card](../include/Abilities.hpp#L35)<br>`remove_function_card(registry, player, slot, reason)` | `FunctionCardRemoved` | — |
| 设置功能牌可用状态 | [set_function_card_usable](../include/Abilities.hpp#L32)<br>`set_function_card_usable(card, usable)` | `FunctionCardUsabilityChanged` | — |
| 废除功能牌 | [abolish_function_card](../include/Abilities.hpp#L33)<br>`abolish_function_card(card)` | `FunctionCardAbolished` | — |
| 添加技能 | [add_ability](../include/Abilities.hpp#L79)<br>`add_ability(card, definition, operation)` | `AbilityAdded` | — |
| 移除技能 | [remove_ability](../include/Abilities.hpp#L84)<br>`remove_ability(ability, operation)` | `AbilityRemoved` | — |
| 废除技能 | [abolish_ability](../include/Abilities.hpp#L89)<br>`abolish_ability(ability, operation)` | `AbilityAbolished` | — |
| 设置技能可用状态 | [set_ability_usable](../include/Abilities.hpp#L31)<br>`set_ability_usable(ability, usable, replacement)` | `AbilityUsabilityChanged` | — |
| 增加技能计数 | [add_ability_counter](../include/Abilities.hpp#L29)<br>`add_ability_counter(ability, counter, value, delta)` | `AbilityStateChanged` | — |
| 设置技能计数 | [set_ability_counter](../include/Abilities.hpp#L30)<br>`set_ability_counter(ability, counter, value, replacement)` | `AbilityStateChanged` | — |
| 施加功能牌镀层 | [apply_coating](../include/Abilities.hpp#L94)<br>`apply_coating(card, coating, operation)` | `CoatingApplied` | — |

### 发牌、摸牌与牌面修改

| 技能名称 | C++ 函数与参数顺序 | 返回或后续事件 | JSON op |
|---|---|---|---|
| 给全部参与玩家发初始手牌 | [deal_initial_cards](../include/Abilities.hpp#L37)<br>`deal_initial_cards(round, players, count, deal)` | `InitialDealAfter` | — |
| 给指定玩家摸牌 | [draw_cards](../include/Abilities.hpp#L99)<br>`draw_cards(player, count, reason, operation)` | `DrawAfter` | — |
| 给全部参与玩家摸牌 | [draw_to_all](../include/Abilities.hpp#L249)<br>`draw_to_all(players, count, reason, operation)` | EventList：多个 DrawAfter | — |
| 将指定手牌放到牌堆底并补摸顶牌 | [swap_cards_with_deck](../include/Abilities.hpp#L104)<br>`swap_cards_with_deck(player, cards, operation)` | `CardChanged` | — |
| 修改牌点数 | [change_card_rank](../include/Abilities.hpp#L109)<br>`change_card_rank(card, rank, lifetime, operation)` | `CardChanged` | — |
| 修改牌花色 | [change_card_suit](../include/Abilities.hpp#L114)<br>`change_card_suit(card, suit, lifetime, operation)` | `CardChanged` | — |
| 临时变为赖子 | [make_card_joker](../include/Abilities.hpp#L119)<br>`make_card_joker(card, lifetime, operation)` | `CardChanged` | — |
| 清除临时赖子标记 | [clear_card_joker](../include/Abilities.hpp#L124)<br>`clear_card_joker(card, operation)` | `CardChanged` | — |
| 授予指定手牌的查看权限 | [reveal_cards](../include/Abilities.hpp#L129)<br>`reveal_cards(viewer, target, cards, operation)` | `CardVisibilityChanged` | — |

### 玩家状态

| 技能名称 | C++ 函数与参数顺序 | 返回或后续事件 | JSON op |
|---|---|---|---|
| 施加玩家状态 | [apply_player_status](../include/Abilities.hpp#L134)<br>`apply_player_status(player, definition, stacks, lifetime, operation)` | `PlayerStatusChanged` | — |
| 移除玩家状态 | [remove_player_status](../include/Abilities.hpp#L139)<br>`remove_player_status(player, status, operation)` | `PlayerStatusChanged` | — |
| 改变状态层数 | [add_status_stacks](../include/Abilities.hpp#L144)<br>`add_status_stacks(player, status, delta, operation)` | `PlayerStatusChanged` | — |

### 延迟效果与选择交互

| 技能名称 | C++ 函数与参数顺序 | 返回或后续事件 | JSON op |
|---|---|---|---|
| 建立延迟效果 | [schedule_delayed_effect](../include/Abilities.hpp#L159)<br>`schedule_delayed_effect(player, definition, trigger, selection, lifetime, operation)` | `DelayedEffectScheduled` | — |
| 触发延迟效果 | [trigger_delayed_effect](../include/Abilities.hpp#L164)<br>`trigger_delayed_effect(player, effect, operation)` | `DelayedEffectTriggered` | — |
| 使延迟效果失效 | [cancel_delayed_effect](../include/Abilities.hpp#L169)<br>`cancel_delayed_effect(player, effect, operation)` | `DelayedEffectExpired` | — |
| 打开选择交互 | [open_choice](../include/Abilities.hpp#L174)<br>`open_choice(player, kind, options, timeoutMs, operation)` | `ChoiceOpened` | — |
| 提交选择结果 | [resolve_choice](../include/Abilities.hpp#L179)<br>`resolve_choice(choice, options, reason, operation)` | `ChoiceResolved` | — |
| 使选择交互到期 | [expire_choice](../include/Abilities.hpp#L184)<br>`expire_choice(choice, operation)` | `ChoiceExpired` | — |

### 系统流程与出牌提交

| 技能名称 | C++ 函数与参数顺序 | 返回或后续事件 | JSON op |
|---|---|---|---|
| 开始一局 | [begin_round](../include/Abilities.hpp#L189)<br>`begin_round(round, first, operation)` | `RoundStart` | — |
| 开始一巡 | [begin_trick](../include/Abilities.hpp#L194)<br>`begin_trick(round, trick, leader, operation)` | `TrickStart` | — |
| 开放行动回合 | [begin_turn](../include/Abilities.hpp#L199)<br>`begin_turn(turn, player, operation)` | `TurnStart` | — |
| 结束行动回合 | [end_turn](../include/Abilities.hpp#L204)<br>`end_turn(turn, player, operation)` | `TurnEnd` | — |
| 结束一巡 | [finish_trick](../include/Abilities.hpp#L209)<br>`finish_trick(trick, leader, operation)` | `TrickEnd` | — |
| 锁定本局赢家 | [lock_round_winner](../include/Abilities.hpp#L214)<br>`lock_round_winner(round, winner, operation)` | `RoundWinnerLocked` | — |
| 结束一局 | [finish_round](../include/Abilities.hpp#L219)<br>`finish_round(round, result, operation)` | `RoundEnd` | — |
| 结束整场游戏 | [finish_game](../include/Abilities.hpp#L224)<br>`finish_game(operation)` | `GameEnd` | — |
| 关闭玩家商店 | [finish_shopping](../include/Abilities.hpp#L229)<br>`finish_shopping(player, shop, operation)` | `ShopLeave` | — |
| 关闭全部玩家商店 | [close_all_shops](../include/Abilities.hpp#L234)<br>`close_all_shops(round, operation)` | `AllShopsClosed` | — |
| 提交已验证的出牌 | [commit_play](../include/Abilities.hpp#L239)<br>`commit_play(player, turn, cards, wildcards, operation)` | `PlayCommitted` | — |
| 提交合法不出 | [commit_pass](../include/Abilities.hpp#L244)<br>`commit_pass(player, turn, reason, operation)` | `PassCommitted` | — |

## 参数约定

`player` 通常是玩家对象或玩家编号；`card`、`cards` 是指定的功能牌引用或扑克牌 ID，按具体签名区分；`amount`、`count` 是数量，`delta` 是增量，`value`、`replacement` 是替换后的值。`lifetime` 是效果有效范围，`selection` 是目标集合，`registry` 是功能牌注册表。

`FunctionCardEnhancement` 可以包含数值增量和新增技能，具体规则见[消耗牌与功能牌强化](../design.md#12-消耗牌与功能牌强化)。技能应保持原子性，例如加分和加金币分别调用两个函数；一张功能牌负责组合这些操作。

## 设计章节

### 1. 时机与行为主体分别记录

见 [design.md 对应章节](../design.md#31-时机与行为主体分别记录)。

### 2. 第一版时机表

见 [design.md 对应章节](../design.md#32-第一版时机表)。

### 3. 结算顺序与边界

见 [design.md 对应章节](../design.md#33-结算顺序与边界)。

### 4. 主动、被动和状态

见 [design.md 对应章节](../design.md#34-主动被动和状态)。

### 5. C++17 接口草案

见 [design.md 对应章节](../design.md#35-c17-接口草案)。

### 6. Context 与派发流程

见 [design.md 对应章节](../design.md#36-context-与派发流程)。

### 7. 事件处理示例

见 [design.md 对应章节](../design.md#37-事件处理示例)。
