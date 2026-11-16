# 完整事件索引

源码中共定义 **136 种事件**（编号 0～135），以 [EventType](../include/Common/enums.hpp) 和 [Event 派生类](../include/Event.hpp) 为准。事件只携带操作参数，构造事件不会自动执行操作。完整调度设计见 [design.md](../design.md#2-事件与状态机)。

其中 **16 种事件**可以用于当前 [JSON 功能牌](function-card-json.md#2-json-可用事件与技能)。表中“—”表示 JSON 尚未开放，不表示没有事件定义；已定义也不代表当前测试服务器接通了对应玩法。

事件、原子技能是两个层次：事件描述发生了什么，技能执行具体操作并可能返回新的事件。完整技能名称见[原子技能索引](ability-system.md)。`actor` 是操作发起者，`player`、`target` 等字段按各具体类解释；点击派生类可查看全部参数。

## 完整事件表

JSON 列中的“资源”表示可使用 `add_score`、`gain_coins`；“奖励”表示可使用 `add_pending_score`、`add_pending_coins`；“报价”表示可使用 `reduce_shop_cost`。`Count` 是总数哨兵，不是一种事件。

### 游戏、商店与出牌流程

| 编号 | EventType | 具体事件类 | 含义 | JSON |
|---|---|---|---|---|
| 0 | `GameStart` | [GameStartedEvent](../include/Event.hpp#L281) | 整场开始 | — |
| 1 | `ShopEnter` | [ShopEnteredEvent](../include/Event.hpp#L290) | 进入局前商店 | 资源 |
| 2 | `ShopActionWindow` | [ShopAbilityActivationEvent](../include/Event.hpp#L299) | 商店窗口内的主动技能调用 | — |
| 3 | `ShopRefreshQuote` | [BeforeShopRefreshEvent](../include/Event.hpp#L308) | 刷新报价前 | 报价 |
| 4 | `ShopRefreshAfter` | [ShopRefreshedEvent](../include/Event.hpp#L319) | 刷新成功后 | — |
| 5 | `ShopBuyQuote` | [BeforeShopPurchaseEvent](../include/Event.hpp#L330) | 购买报价前 | 报价 |
| 6 | `ShopBuyAfter` | [ShopPurchasedEvent](../include/Event.hpp#L343) | 购买交付成功后 | — |
| 7 | `ShopSellQuote` | [BeforeShopSaleEvent](../include/Event.hpp#L354) | 出售报价前 | — |
| 8 | `ShopSellAfter` | [ShopSoldEvent](../include/Event.hpp#L366) | 出售成功后 | — |
| 9 | `ShopLeave` | [ShopLeftEvent](../include/Event.hpp#L377) | 离店 | 资源 |
| 10 | `RoundStart` | [RoundStartedEvent](../include/Event.hpp#L387) | 本局开始 | 资源 |
| 11 | `InitialDealBefore` | [BeforeInitialDealEvent](../include/Event.hpp#L396) | 初始发牌前 | — |
| 12 | `InitialDealAfter` | [InitialCardsDealtEvent](../include/Event.hpp#L405) | 全体初始发牌完成 | — |
| 13 | `TrickStart` | [TrickStartedEvent](../include/Event.hpp#L413) | 本巡开始 | 资源 |
| 14 | `TurnStart` | [TurnStartedEvent](../include/Event.hpp#L423) | 当前回合开始 | 资源 |
| 15 | `PlayActionWindow` | [TurnAbilityActivationEvent](../include/Event.hpp#L432) | 出牌窗口内的主动技能调用 | — |
| 16 | `PlayValidate` | [PlayValidationEvent](../include/Event.hpp#L442) | 验证选牌及跟牌 | — |
| 17 | `PlayCommitted` | [CardsPlayedEvent](../include/Event.hpp#L453) | 成功提交一手牌 | 资源 |
| 18 | `PlayRewardBefore` | [BeforePlayRewardEvent](../include/Event.hpp#L465) | 计算本手出牌奖励前 | 奖励 |
| 19 | `PlayRewardAfter` | [PlayRewardGrantedEvent](../include/Event.hpp#L477) | 本手奖励到账后 | — |
| 20 | `PassCommitted` | [PlayerPassedEvent](../include/Event.hpp#L488) | 确认不出牌 | 资源 |
| 21 | `TurnEnd` | [TurnEndedEvent](../include/Event.hpp#L498) | 当前回合结算结束 | 资源 |
| 22 | `TrickEnd` | [TrickEndedEvent](../include/Event.hpp#L507) | 获得牌权并结束本巡 | 资源 |
| 23 | `DrawBefore` | [BeforeCardsDrawEvent](../include/Event.hpp#L516) | 非初始摸牌前 | — |
| 24 | `DrawAfter` | [CardsDrawnEvent](../include/Event.hpp#L527) | 实际摸牌完成 | 资源 |
| 25 | `TrickDrawsAfter` | [TrickDrawingCompletedEvent](../include/Event.hpp#L537) | 巡末摸牌步骤完成 | — |
| 26 | `RoundEnd` | [RoundEndedEvent](../include/Event.hpp#L546) | 本局结束且最后回合结算完成 | 资源 |
| 27 | `RoundRewardBefore` | [BeforeRoundRewardEvent](../include/Event.hpp#L555) | 计算玩家局末奖励前 | 奖励 |
| 28 | `RoundRewardAfter` | [RoundRewardsGrantedEvent](../include/Event.hpp#L566) | 全体局末奖励与扣款完成 | — |
| 29 | `GameEnd` | [GameEndedEvent](../include/Event.hpp#L575) | 整场结束与冠军确认 | 资源 |

### 结果与状态通知

| 编号 | EventType | 具体事件类 | 含义 | JSON |
|---|---|---|---|---|
| 30 | `CardChanged` | [PlayingCardsChangedEvent](../include/Event.hpp#L584) | 技能实际改变牌面或手牌后 | — |
| 31 | `AbilityResolved` | [AbilityResolvedEvent](../include/Event.hpp#L594) | 技能效果成功结算后 | — |
| 32 | `FunctionCardEquipped` | [FunctionCardEquippedEvent](../include/Event.hpp#L603) | 功能卡进入槽位 | — |
| 33 | `FunctionCardRemoved` | [FunctionCardRemovedEvent](../include/Event.hpp#L612) | 功能卡离开槽位 | — |
| 34 | `ShopSlotLocked` | [ShopSlotLockedEvent](../include/Event.hpp#L621) | 货位锁定成功 | — |
| 35 | `ShopSlotUnlocked` | [ShopSlotUnlockedEvent](../include/Event.hpp#L631) | 货位解锁成功 | — |
| 36 | `BoosterPackOpened` | [BoosterPackOpenedEvent](../include/Event.hpp#L641) | 卡包已付费打开 | — |
| 37 | `BoosterPackChoiceCompleted` | [BoosterPackChoiceCompletedEvent](../include/Event.hpp#L651) | 卡包候选选取完成 | — |
| 38 | `BoosterPackSkipped` | [BoosterPackSkippedEvent](../include/Event.hpp#L661) | 卡包已放弃 | — |
| 39 | `ExpansionCardUsed` | [ExpansionCardUsedEvent](../include/Event.hpp#L670) | 拓展牌获得即使用完成 | — |
| 40 | `HandPatternUpgraded` | [HandPatternUpgradedEvent](../include/Event.hpp#L680) | 牌型加强完成 | — |
| 41 | `FacilityPurchased` | [FacilityPurchasedEvent](../include/Event.hpp#L691) | 设施购买生效 | — |
| 42 | `CoinsChanged` | [CoinsChangedEvent](../include/Event.hpp#L700) | 金币余额实际变化 | — |
| 43 | `ScoreChanged` | [ScoreChangedEvent](../include/Event.hpp#L711) | 累计分数实际变化 | — |
| 44 | `PlayerStatusChanged` | [PlayerStatusChangedEvent](../include/Event.hpp#L721) | 玩家状态实际变化 | — |
| 45 | `AbilityAdded` | [AbilityAddedEvent](../include/Event.hpp#L731) | 技能添加完成 | — |
| 46 | `AbilityRemoved` | [AbilityRemovedEvent](../include/Event.hpp#L739) | 技能逻辑移除完成 | — |
| 47 | `AbilityAbolished` | [AbilityAbolishedEvent](../include/Event.hpp#L747) | 技能被永久废除 | — |
| 48 | `AbilityUsabilityChanged` | [AbilityUsabilityChangedEvent](../include/Event.hpp#L755) | 技能临时可用开关变化 | — |
| 49 | `ChoiceOpened` | [ChoiceOpenedEvent](../include/Event.hpp#L764) | 交互选择窗口建立 | — |
| 50 | `ChoiceResolved` | [ChoiceResolvedEvent](../include/Event.hpp#L774) | 交互选择处理完成 | — |
| 51 | `ChoiceExpired` | [ChoiceExpiredEvent](../include/Event.hpp#L785) | 交互期限已到并完成默认处理 | — |
| 52 | `RoundWinnerLocked` | [RoundWinnerLockedEvent](../include/Event.hpp#L794) | 出空时锁定赢家与手牌快照 | — |
| 53 | `DrawPileExhausted` | [DrawPileExhaustedEvent](../include/Event.hpp#L804) | 牌堆由非空变为空 | — |
| 54 | `AllShopsClosed` | [AllShopsClosedEvent](../include/Event.hpp#L812) | 全体商店会话关闭且交易结清 | — |
| 55 | `FunctionCardUsabilityChanged` | [FunctionCardUsabilityChangedEvent](../include/Event.hpp#L820) | 整张功能卡临时开关变化 | — |
| 56 | `FunctionCardAbolished` | [FunctionCardAbolishedEvent](../include/Event.hpp#L829) | 整张功能卡被废除 | — |
| 57 | `AbilityStateChanged` | [AbilityStateChangedEvent](../include/Event.hpp#L837) | 技能成长或蓄力状态变化 | — |
| 58 | `DelayedEffectScheduled` | [DelayedEffectScheduledEvent](../include/Event.hpp#L848) | 延迟效果标记建立 | — |
| 59 | `DelayedEffectTriggered` | [DelayedEffectTriggeredEvent](../include/Event.hpp#L857) | 延迟标记实际执行完成 | — |
| 60 | `DelayedEffectExpired` | [DelayedEffectExpiredEvent](../include/Event.hpp#L866) | 延迟标记结束或失效 | — |
| 61 | `CoatingApplied` | [CoatingAppliedEvent](../include/Event.hpp#L875) | 镀层施加完成 | — |
| 62 | `CardVisibilityChanged` | [CardVisibilityChangedEvent](../include/Event.hpp#L884) | 查看权限或公开信息变化 | — |

### 玩家操作请求

| 编号 | EventType | 具体事件类 | 含义 | JSON |
|---|---|---|---|---|
| 63 | `BuyShopItemRequest` | [BuyShopItemRequest](../include/Event.hpp#L91) | 购买普通货架商品 | — |
| 64 | `BuyBoosterPackRequest` | [BuyBoosterPackRequest](../include/Event.hpp#L102) | 购买并立即打开卡包 | — |
| 65 | `SellFunctionCardRequest` | [SellFunctionCardRequest](../include/Event.hpp#L111) | 出售功能卡 | — |
| 66 | `RefreshShopRequest` | [RefreshShopRequest](../include/Event.hpp#L120) | 刷新普通货架 | — |
| 67 | `LockShopSlotRequest` | [LockShopSlotRequest](../include/Event.hpp#L128) | 锁定货位 | — |
| 68 | `UnlockShopSlotRequest` | [UnlockShopSlotRequest](../include/Event.hpp#L137) | 解锁货位 | — |
| 69 | `ChooseBoosterCardRequest` | [ChooseBoosterCardRequest](../include/Event.hpp#L146) | 选择卡包候选 | — |
| 70 | `SkipBoosterPackRequest` | [SkipBoosterPackRequest](../include/Event.hpp#L155) | 放弃已打开卡包 | — |
| 71 | `UpgradeHandPatternRequest` | [UpgradeHandPatternRequest](../include/Event.hpp#L163) | 付费加强牌型 | — |
| 72 | `BuyFacilityRequest` | [BuyFacilityRequest](../include/Event.hpp#L172) | 购买设施 | — |
| 73 | `FinishShoppingRequest` | [FinishShoppingRequest](../include/Event.hpp#L181) | 完成本次购物 | — |
| 74 | `PlayCardsRequest` | [PlayCardsRequest](../include/Event.hpp#L189) | 确认出牌 | — |
| 75 | `PassRequest` | [PassRequest](../include/Event.hpp#L199) | 主动不出牌 | — |
| 76 | `ActivateAbilityRequest` | [ActivateAbilityRequest](../include/Event.hpp#L207) | 发动主动技能 | — |
| 77 | `SubmitChoiceRequest` | [SubmitChoiceRequest](../include/Event.hpp#L216) | 提交一般目标或方案 | — |
| 78 | `ResolveWildcardRequest` | [ResolveWildcardRequest](../include/Event.hpp#L225) | 确认赖子指定 | — |
| 79 | `ContinueGameRequest` | [ContinueGameRequest](../include/Event.hpp#L234) | 请求加赛或无尽模式 | — |
| 80 | `StartNewGameRequest` | [StartNewGameRequest](../include/Event.hpp#L242) | 请求开始新一场 | — |
| 81 | `StateSyncRequest` | [StateSyncRequest](../include/Event.hpp#L249) | 请求状态同步 | — |
| 82 | `PreviewPlayRequest` | [PreviewPlayRequest](../include/Event.hpp#L257) | 查询选牌的合法方案 | — |
| 83 | `QuoteShopOperationRequest` | [QuoteShopOperationRequest](../include/Event.hpp#L266) | 查询商店操作报价 | — |

### 效果操作

| 编号 | EventType | 具体事件类 | 含义 | JSON |
|---|---|---|---|---|
| 84 | `AddScore` | [AddScoreEvent](../include/Event.hpp#L895) | 增加实际分数 | — |
| 85 | `AddCoins` | [AddCoinsEvent](../include/Event.hpp#L904) | 增加实际金币 | — |
| 86 | `SpendCoins` | [SpendCoinsEvent](../include/Event.hpp#L913) | 扣除实际金币 | — |
| 87 | `AddPendingScore` | [AddPendingScoreEvent](../include/Event.hpp#L923) | 修正待结算分数 | — |
| 88 | `MultiplyPendingScore` | [MultiplyPendingScoreEvent](../include/Event.hpp#L933) | 缩放待结算分数 | — |
| 89 | `AddPendingCoins` | [AddPendingCoinsEvent](../include/Event.hpp#L945) | 修正待结算金币 | — |
| 90 | `ReduceShopCost` | [ReduceShopCostEvent](../include/Event.hpp#L957) | 降低本次商店报价 | — |
| 91 | `GrantFreeRefresh` | [GrantFreeRefreshEvent](../include/Event.hpp#L968) | 增加本店免费刷新额度 | — |
| 92 | `RefreshShop` | [RefreshShopEvent](../include/Event.hpp#L978) | 执行一次货架刷新 | — |
| 93 | `OpenBoosterPack` | [OpenBoosterPackEvent](../include/Event.hpp#L988) | 执行已获准的开包 | — |
| 94 | `LockShopSlot` | [LockShopSlotEvent](../include/Event.hpp#L999) | 执行货位锁定 | — |
| 95 | `UnlockShopSlot` | [UnlockShopSlotEvent](../include/Event.hpp#L1009) | 执行货位解锁 | — |
| 96 | `GrantFunctionCard` | [GrantFunctionCardEvent](../include/Event.hpp#L1019) | 生成并装备功能卡 | — |
| 97 | `RemoveFunctionCard` | [RemoveFunctionCardEvent](../include/Event.hpp#L1029) | 移除功能卡 | — |
| 98 | `ReplaceFunctionCard` | [ReplaceFunctionCardEvent](../include/Event.hpp#L1038) | 替换功能卡 | — |
| 99 | `AddAbility` | [AddAbilityEvent](../include/Event.hpp#L1048) | 给功能卡添加技能 | — |
| 100 | `RemoveAbility` | [RemoveAbilityEvent](../include/Event.hpp#L1057) | 逻辑删除技能 | — |
| 101 | `AbolishAbility` | [AbolishAbilityEvent](../include/Event.hpp#L1065) | 永久废除技能 | — |
| 102 | `SetAbilityUsable` | [SetAbilityUsableEvent](../include/Event.hpp#L1073) | 设置技能临时开关 | — |
| 103 | `SetFunctionCardUsable` | [SetFunctionCardUsableEvent](../include/Event.hpp#L1082) | 设置整牌临时开关 | — |
| 104 | `AbolishFunctionCard` | [AbolishFunctionCardEvent](../include/Event.hpp#L1091) | 永久废除整张牌 | — |
| 105 | `ApplyCoating` | [ApplyCoatingEvent](../include/Event.hpp#L1099) | 给功能卡施加镀层 | — |
| 106 | `DrawCards` | [DrawCardsEvent](../include/Event.hpp#L1108) | 按规则执行非初始摸牌 | — |
| 107 | `SwapCardsWithDeck` | [SwapCardsWithDeckEvent](../include/Event.hpp#L1118) | 将指定手牌换到牌堆底并等量摸顶 | — |
| 108 | `ChangeCardRank` | [ChangeCardRankEvent](../include/Event.hpp#L1127) | 临时改变普通牌点数 | — |
| 109 | `ChangeCardSuit` | [ChangeCardSuitEvent](../include/Event.hpp#L1137) | 临时改变花色 | — |
| 110 | `MakeCardJoker` | [MakeCardJokerEvent](../include/Event.hpp#L1147) | 将指定牌临时变为王 | — |
| 111 | `RevealCards` | [RevealCardsEvent](../include/Event.hpp#L1156) | 向指定玩家展示合法信息 | — |
| 112 | `ApplyPlayerStatus` | [ApplyPlayerStatusEvent](../include/Event.hpp#L1166) | 施加玩家状态 | — |
| 113 | `RemovePlayerStatus` | [RemovePlayerStatusEvent](../include/Event.hpp#L1177) | 移除玩家状态 | — |
| 114 | `UpgradeHandPattern` | [UpgradeHandPatternEvent](../include/Event.hpp#L1186) | 执行牌型加强 | — |
| 115 | `PurchaseFacility` | [PurchaseFacilityEvent](../include/Event.hpp#L1196) | 执行设施购买与生效 | — |
| 116 | `UseExpansionCard` | [UseExpansionCardEvent](../include/Event.hpp#L1205) | 执行获得即使用的拓展牌 | — |
| 117 | `AddDelayedEffect` | [AddDelayedEffectEvent](../include/Event.hpp#L1215) | 建立延迟效果标记 | — |
| 118 | `OpenChoice` | [OpenChoiceEvent](../include/Event.hpp#L1227) | 建立选择窗口并挂起相关流程 | — |
| 119 | `ResolveChoice` | [ResolveChoiceEvent](../include/Event.hpp#L1239) | 执行选择或默认方案 | — |
| 120 | `ChangeAbilityCounter` | [ChangeAbilityCounterEvent](../include/Event.hpp#L1249) | 改变已定义的技能成长或蓄力计数 | — |
| 121 | `EventBatch` | [EventBatch](../include/Event.hpp#L1260) | 有序原子组合多个效果 | — |

### 网络与计时控制

| 编号 | EventType | 具体事件类 | 含义 | JSON |
|---|---|---|---|---|
| 122 | `PlayerJoinedControl` | [PlayerJoinedControl](../include/Event.hpp#L1270) | 玩家加入房间 | — |
| 123 | `PlayerDisconnectedControl` | [PlayerDisconnectedControl](../include/Event.hpp#L1278) | 连接断开 | — |
| 124 | `PlayerReconnectedControl` | [PlayerReconnectedControl](../include/Event.hpp#L1287) | 原玩家重连 | — |
| 125 | `ShoppingDeadlineControl` | [ShoppingDeadlineControl](../include/Event.hpp#L1296) | 商店计时到期 | — |
| 126 | `TurnDeadlineControl` | [TurnDeadlineControl](../include/Event.hpp#L1305) | 出牌计时到期 | — |
| 127 | `ChoiceDeadlineControl` | [ChoiceDeadlineControl](../include/Event.hpp#L1315) | 选择计时到期 | — |
| 128 | `RequestRejectedControl` | [RequestRejectedControl](../include/Event.hpp#L1324) | 操作请求未获接受 | — |
| 129 | `StateSnapshotReadyControl` | [StateSnapshotReadyControl](../include/Event.hpp#L1334) | 同步快照已经准备 | — |
| 130 | `PlayOptionsReadyControl` | [PlayOptionsReadyControl](../include/Event.hpp#L1343) | 合法出牌或赖子方案已经计算 | — |
| 131 | `ShopQuoteReadyControl` | [ShopQuoteReadyControl](../include/Event.hpp#L1353) | 商店报价已经计算 | — |

### 消耗牌与强化通知

| 编号 | EventType | 具体事件类 | 含义 | JSON |
|---|---|---|---|---|
| 132 | `ConsumableUsed` | [ConsumableUsedEvent](../include/Event.hpp#L1363) | 消耗牌已购买并完成即时效果 | — |
| 133 | `ShopSlotsChanged` | [ShopSlotsChangedEvent](../include/Event.hpp#L1373) | 普通商店货位数量变化 | — |
| 134 | `ShopLevelChanged` | [ShopLevelChangedEvent](../include/Event.hpp#L1380) | 商店等级变化 | — |
| 135 | `FunctionCardEnhanced` | [FunctionCardEnhancedEvent](../include/Event.hpp#L1387) | 功能牌实例强化成功 | — |

## 设计章节

### 1. 调用关系

见 [design.md 对应章节](../design.md#21-调用关系)。

### 2. Event 与 Context

见 [design.md 对应章节](../design.md#22-event-与-context)。

### 3. 状态与转移

见 [design.md 对应章节](../design.md#23-状态与转移)。

### 4. 队列、返回值与结算

见 [design.md 对应章节](../design.md#24-队列返回值与结算)。

### 5. C++17 接口草案

见 [design.md 对应章节](../design.md#25-c17-接口草案)。

### 6. 示例链

见 [design.md 对应章节](../design.md#26-示例链)。
