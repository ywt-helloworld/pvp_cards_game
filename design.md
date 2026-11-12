# 代码设计

所有代码设计统一维护在根目录 `design.md`，后续的类定义、接口、事件协议、状态机、调度与联机实现说明都更新在此。快速规则见 [README.md](README.md)，完整规则与草案见 [详细规则](docs/game-rules.md)，卡牌参考资料见 [设计调研](docs/function-card-research.md)。

当前已实现事件模型、Context / Condition、回调监听、62 个原子操作、玩家与功能牌、动态 Register、按牌缓冲的 HandleEvent，以及四人 TCP 原型服务器。玩家与系统功能牌共用同一条结算路径。默认卡池明确标记为 Demo，正式卡池可通过工厂接口替换；卡包、E 系列拓展牌与 U06 的业务尚待接入。

| 模块 | 实现文件 |
|---|---|
| 公共定义 | [Common/enums.hpp](include/Common/enums.hpp)、[Common/types.hpp](include/Common/types.hpp)、[Common/network_types.hpp](include/Common/network_types.hpp) |
| 事件与上下文 | [Event.hpp](include/Event.hpp)、[Context.hpp](include/Context.hpp)、[Condition.hpp](include/Condition.hpp)、[Condition.cpp](src/Condition.cpp) |
| 玩家与扑克牌 | [Player.hpp](include/Player.hpp)、[Poker.hpp](include/Poker.hpp)、[Poker.cpp](src/Poker.cpp) |
| 原子操作 | [Abilities.hpp](include/Abilities.hpp)、[Abilities.cpp](src/Abilities.cpp) |
| 功能牌与系统牌 | [FunctionCards.hpp](include/FunctionCards.hpp)、[FunctionCards.cpp](src/FunctionCards.cpp)、[SystemCards.hpp](include/SystemCards.hpp)、[SystemCards.cpp](src/SystemCards.cpp) |
| 订阅与派发 | [Register.hpp](include/Register.hpp)、[Register.cpp](src/Register.cpp)、[HandleEvent.hpp](include/HandleEvent.hpp)、[HandleEvent.cpp](src/HandleEvent.cpp) |
| 对局调度 | [Scheduler.hpp](include/Scheduler.hpp)、[Scheduler.cpp](src/Scheduler.cpp) |
| 网络与监听 | [ListenEvent.hpp](include/network/ListenEvent.hpp)、[ListenEvent.cpp](src/network/ListenEvent.cpp)、[Protocol.hpp](include/network/Protocol.hpp)、[Protocol.cpp](src/network/Protocol.cpp)、[Server.hpp](include/network/Server.hpp)、[Server.cpp](src/network/Server.cpp) |
| 启动与构建 | [main.cpp](src/main.cpp)、[CMakeLists.txt](CMakeLists.txt) |
| 验证 | [ListenEvent.test.cpp](tests/ListenEvent.test.cpp)、[ContextCondition.test.cpp](tests/ContextCondition.test.cpp)、[CardPipeline.test.cpp](tests/CardPipeline.test.cpp)、[server_smoke.py](tests/server_smoke.py) |

公共类型和领域对象位于 cardgame，原子函数位于 cardgame::abilities，网络模块位于 cardgame::network。所有枚举与类型别名统一放在 include/Common/；网络、监听和协议代码放在 include/network/ 与 src/network/。头文件不引入全局 using namespace。

## 目录

- [1. 总体架构](#1-总体架构)
- [2. 事件与状态机](#2-事件与状态机)
- [2.7 全部事件清单与派生类](#27-全部事件清单与派生类)
- [3. 技能事件处理](#3-技能事件处理)
- [4. 功能卡](#4-功能卡)
- [5. 扑克牌 Poker](#5-扑克牌-poker)
- [6. 商店、回合与跨玩家效果](#6-商店回合与跨玩家效果)
- [7. 事件监听与输入入口](#7-事件监听与输入入口)
- [8. 延迟与执行方式](#8-延迟与执行方式)
- [9. 后续实现事项](#9-后续实现事项)
- [10. 服务器构建与协议](#10-服务器构建与协议)
- [11. 网页版与服务器部署](#11-网页版与服务器部署)
- [12. 消耗牌与功能牌强化](#12-消耗牌与功能牌强化)
- [13. JSON 功能牌导入工具](#13-json-功能牌导入工具)
- [14. PR 自动检查](#14-pr-自动检查)

## 1. 总体架构

```text
客户端请求 / 定时器 / 连接变化 / 业务回调
  → 网络会话绑定玩家，Protocol 与 ListenEvent 构造请求 Event
  → Scheduler 验证外部请求入口，交给 HandleEvent
  → Condition 将完整事件装入本房间唯一 Context
  → Register 按 EventType 所在列冻结响应功能牌
  → HandleEvent 逐张调用 FunctionCards::resolve(context, effects)
  → 功能牌分别调用所需的 abilities 原子函数
  → 后续事实事件先进入本张牌的 CardEffects 缓冲区
  → 本张牌结束后处理子事件，恢复父事件并继续下一张牌
```

四名玩家、公共牌堆、局／巡／回合状态和商店由 Scheduler 持有。Condition 管理事件到 Context 的结构转换；Register 管理槽位和订阅矩阵；HandleEvent 管理候选与因果链。发牌、资源请求和流程推进由不可见系统牌执行。

## 2. 事件与状态机

### 2.1 调用关系

| 对象 | 职责 |
|---|---|
| Event | 唯一操作类型、发起者及本操作参数 |
| Context | 完整当前事件、外部调度记录、接收者和工作快照 |
| Condition | 校验事件结构、更新、暂存及恢复同一个 Context |
| Register | 玩家与系统功能牌的槽位、订阅矩阵、冻结候选与实时有效性检查 |
| HandleEvent | 按牌调用、缓冲后续事件、驱动显式结算栈 |
| FunctionCards | 卡牌身份、潜在触发事件及一个卡牌级组合回调 |
| abilities | 按各操作所需参数提供普通函数，无统一 Ability 虚基类 |
| Scheduler | 权威玩家数据、对局阶段、业务校验和系统牌接线 |

### 2.2 Event 与 Context

所有具体事件直接继承抽象基类 `Event`，只读成员 `type: EventType` 唯一确定操作或通知，其他成员保存发起玩家和该操作所需参数。所有事件统一交给 Condition；具体操作如何执行、如何验证和是否派发技能由外部模块决定。完整清单见 [全部事件清单与派生类](#27-全部事件清单与派生类)。

- Request：玩家意图或查询，先验证，不代表行为已经成功。
- Signal：服务端确认的业务事实，或已获准的一次主动技能调用，按时机匹配处理器。
- Effect：业务流程或处理器提出的具体效果请求，由对应系统牌与业务模块校验并执行。
- Control：连接、计时器、拒绝和同步结果，不直接当作成功行为触发功能卡。

`EventType` 覆盖全部 136 个具体事件，每项对应一个具名事件类与确定的业务含义；`Event` 保存只读 `type` 和可选 `actor`，派生类保存目标与操作参数。Condition 按 `type` 查找处理规则，事件类只承载数据。

原有 63 个通知类型编号 0～62 保持不变，其余 69 个请求、效果与控制类型追加为 63～131；消耗牌与强化通知追加为 132～135，`TOTAL_EVENT_TYPES = 136`。Condition 校验 type 与根事件实际类；HandleEvent 和 Register 决定响应卡牌，Scheduler 与系统牌校验业务状态。主动、被动描述触发方式，同一原子操作可被不同触发方式复用。

每个房间的 Condition 只创建一份 `Context` 并反复更新；单房间运行时总共只有一份。它是该房间全局共享的调用现场，通过 `Condition::context()` 提供只读访问，不使用跨房间的进程级可变全局对象。

Condition 接收 EventPtr 后持有完整派生对象，所有专属字段、列表、可选值和 EventBatch 子事件均保留，调用方通过 event_as<T>() 读取。通用 type() 与 actor() 直接读取当前事件，避免维护重复副本。奖励和报价快照按对应事件字段复制，后续可由 set_reward / set_shop 修改工作快照，原始事件保持原值。

Context 的对象生命周期属于 Condition；on_event 和 clear_context 始终更新同一对象，其他模块读取 const Context&。事件和调度记录保存到下一次事件替换或显式清空；调用快照与接收技能指针还可能因对应 set 方法而失效。新事件清空旧接收者和无关快照；take_context / restore_context 移动保存父事件，HandleEvent 的结算栈负责恢复。

主动请求使用 `ActivateAbilityRequest`，调度器验证卡牌归属和窗口后生成 `ShopAbilityActivationEvent` 或 `TurnAbilityActivationEvent`，只调用选定技能。效果完成后产生的新业务通知按业务规则匹配技能，来源技能仅以引用记录在外部调度信息中；HandleEvent 只调用选中的玩家卡牌，具体技能序号、目标、费用与次数由该牌的代码核对。

### 2.3 状态与转移

下表的基础阶段转移已由 Scheduler 的系统功能牌接入；Condition 继续专注上下文转换。实际动作提交和子事件处理完成后，再推进相应阶段。

| 当前阶段 | 边界与条件 | 下一阶段 |
|---|---|---|
| `NotStarted` | 整场初始化及 `GameStart` 完成 | `Shopping` |
| `Shopping` | 全部玩家离店，本局准备就绪 | `RoundStarting` |
| `RoundStarting` | 初始发牌与开局效果完成 | `Playing` |
| `Playing` | 本局已锁定结束，最后一个回合结算完成 | `RoundEnding` |
| `RoundEnding` | 局末结算完成，还有下一局 | `Shopping` |
| `RoundEnding` | 最后一局结算完成 | `Finished`，再处理一次 `GameEnd` 通知 |

`Playing` 内的巡、回合、牌权和操作窗口继续按既有规则维护。某个 `Signal` 可以只触发技能而不改变阶段，例如同一回合中的出牌后通知。状态机通过当前阶段、局／巡／回合及操作窗口检查过期或不合法事件；具体转移代码待对局实现时补齐。

最后一手出空时仍锁定赢家与手牌快照，完成该手对应事件链后进入局末，不再摸牌或开始下一巡；规则见 [技能结算顺序](#33-结算顺序与边界)。

### 2.4 队列、返回值与结算

网络服务器在一个房间事件循环中串行接收客户端请求。商店允许四名玩家并行拥有可操作会话，网络输入按到达顺序处理；局内只接受当前玩家及当前 turnId 的行动。不会缓存未获授权的下一回合出牌。

一张牌调用 resolve 后，可以分别执行多项原子操作。每项产生的事实通知交给 CardEffects::emit，emit 只缓冲，不立即调用其他卡。当前牌返回后，按 FIFO 处理其通知；通知引发的子链完成后才处理下一条同级通知，再恢复父事件下一张卡。

例如：A 的“加分、加金币”依次更新资源，随后才处理 ScoreChanged 和 CoinsChanged；这两个通知的全部子链完成后，才轮到原事件的卡 B。实现使用 deque 保存一张牌的同级后续事件，vector 作为父事件与派发进度栈，不使用递归 C++ 调用。

当前卡的候选列表在事件开始时冻结。结算中加入的卡从后续事件开始参与；已卸下、被替换、禁用或废除的候选被跳过。移槽保留装备世代，卸下重装使用新世代；共享所有权保护正在结算的卡不被销毁。

原子函数先检查参数并为事实分配对象，再提交本次操作。单张牌由多次独立提交组成：后续技能异常时，DispatchReport.failures 记录故障，之前成功操作产生的事实仍会派发。不会把整张牌误称为可回滚事务。链事件预算默认 4096，对局调度器使用 65536，超限抛出 length_error 并停止该链；已提交操作不回滚。内部后续事件通过 CardEffects 提交，直接重入 HandleEvent 会被拒绝。

### 2.5 C++17 接口草案

当前接口以源文件为准，核心接线如下：

| 接口 | 行为 |
|---|---|
| Condition::on_event(EventPtr, DispatchMeta) | 校验 136 种根事件，更新并返回同一个 const Context& |
| Condition::set_receiver / set_reward / set_shop | 修改当前接收者或工作快照 |
| Condition::take_context() | 移出事件、调度记录和快照，清空共享 Context，返回可移动 ContextFrame |
| Condition::restore_context(ContextFrame) | 校验并恢复原事件和工作快照 |
| Register::matching(EventType, PlayerOrder) | 按矩阵列冻结响应候选，系统牌和玩家牌采用相同条目 |
| HandleEvent::handle_event(EventPtr, DispatchMeta) | 结算完整因果链，返回 DispatchReport |

ContextFrame 是待恢复的数据，不是第二份共享 Context。帧中保存原事件唯一所有权、接收者、工作快照和元数据；父子切换不会复制 EventBatch 或丢失派生字段。恢复后，下一张卡能看到前一张卡已经修改的工作快照。

Context::event_as<T>() 读取完整原始事件；reward() 和 shop() 是可独立修改的工作副本。空事件、未知类型、类型与对象不符会被拒绝，旧上下文保留。所有角色分别记录：actor 是发起者，具体事件 target / player 是目标，receiver 是本次接收卡／技能，originAbility 或 originSystemCard 是来源。

### 2.6 示例链

```text
BeforePlayRewardEvent
  → Condition 保存完整初始奖励，并复制工作快照
  → Register 选择修改奖励的玩家功能牌
  → 卡牌调用 abilities::multiply_pending_score(effects.reward(), 2, 1)
  → 同牌其他操作执行完成，工作快照保存到父事件帧
  → 后续系统流程牌读取新奖励，分别调用 add_score 与 gain_coins
  → 系统牌结束后派发 ScoreChanged、CoinsChanged 和奖励到账通知
```

原始 BeforePlayRewardEvent 中的 initialReward 仍保持原值。事件定义及全部参数继续列在下一节，实际类型位于 Common 和 Event.hpp。

### 2.7 全部事件清单与派生类

本清单以当前已经确定的游戏规则和通用技能能力为边界，共 **136 个具体事件**。新增机制继续追加清单和派生类，不把尚未发生的成功行为当作客户端请求。玩家、基础牌局、牌型、随机和部分商店／选择流程已实现；消耗牌与功能牌强化的新增事件和接口见第 12 节；其余业务通过对应系统牌和原子操作适配。

| 类别 | 数量 | 主要来源 | 是否直接触发功能卡 |
|---|---:|---|---|
| Request | 21 | 玩家操作和只读查询 | 先校验，再产生对应 Signal |
| Signal | 63 | Condition 确认的业务事实或允许的技能调用 | Condition 按 type 选取通知规则与目标技能 |
| Effect | 38 | 内部业务发起的操作请求 | 由对应系统牌提交后产生事实通知 |
| Control | 10 | 连接、计时器和服务端反馈 | 不直接作为成功行为触发卡牌 |

`Event` 统一提供只读 `type` 和可选的发起玩家 `actor`；派生类只声明本操作所需参数。请求去重编号、来源链和通用版本信息放在外部的 DispatchMeta 中，下表列出操作专属参数。ID、引用及枚举为接口约定，已有同名定义时直接复用，不重复构建玩家等业务类。默认构造仅便于创建，必需 ID 和参数仍由 Condition 校验，零值不代表合法对象。

每个表项都对应下面一个直接继承 `Event` 的独立派生类；具体构造函数固定 `type`，事件参数在各个具体类中声明。下面的分组用于阅读清单，所有事件共用一个基类和处理入口。

#### 2.7.1 操作请求（21 个）

| 事件派生类 | 含义 | 专属参数 | 时机或执行约束 |
|---|---|---|---|
| `BuyShopItemRequest` | 购买普通货架商品 | `shopId: ShopId`；`slotId: SlotId`；`productId: ProductId` | 商店队列；价格由服务端计算 |
| `BuyBoosterPackRequest` | 购买并立即打开卡包 | `shopId: ShopId`；`packId: PackId` | 商店队列；验证空槽与包限购，扣款后立即开包 |
| `SellFunctionCardRequest` | 出售功能卡 | `shopId: ShopId`；`card: CardRef` | 商店队列；出售成本和成长清理按既有规则 |
| `RefreshShopRequest` | 刷新普通货架 | `shopId: ShopId` | 商店队列；出队时检查刷新上限及当前报价 |
| `LockShopSlotRequest` | 锁定货位 | `shopId: ShopId`；`slotId: SlotId` | 按当前状态和来源校验 |
| `UnlockShopSlotRequest` | 解锁货位 | `shopId: ShopId`；`slotId: SlotId` | 按当前状态和来源校验 |
| `ChooseBoosterCardRequest` | 选择卡包候选 | `choiceId: ChoiceId`；`optionId: ChoiceOptionId` | 授权选择回复，不作为下一次普通购物排队 |
| `SkipBoosterPackRequest` | 放弃已打开卡包 | `choiceId: ChoiceId` | 授权选择回复；不退已支付包价 |
| `UpgradeHandPatternRequest` | 付费加强牌型 | `shopId: ShopId`；`pattern: HandPattern` | 按当前状态和来源校验 |
| `BuyFacilityRequest` | 购买设施 | `shopId: ShopId`；`facilityId: DefinitionId` | 按当前状态和来源校验 |
| `FinishShoppingRequest` | 完成本次购物 | `shopId: ShopId` | 按当前状态和来源校验 |
| `PlayCardsRequest` | 确认出牌 | `turnId: TurnId`；`cards: std::vector<CardId>`；`wildcards: std::vector<WildcardAssignment>` | 局内直接；合法提交后才产生出牌成功通知 |
| `PassRequest` | 主动不出牌 | `turnId: TurnId` | 局内直接；先手不能因此跳过领出 |
| `ActivateAbilityRequest` | 发动主动技能 | `ability: AbilityRef`；`selection: TargetSpec` | 校验技能归属、窗口、费用与次数；商店和局内依窗口处理 |
| `SubmitChoiceRequest` | 提交一般目标或方案 | `choiceId: ChoiceId`；`options: std::vector<ChoiceOptionId>` | 按交互授权接受，允许非当前行动玩家回复 |
| `ResolveWildcardRequest` | 确认赖子指定 | `choiceId: ChoiceId`；`wildcards: std::vector<WildcardAssignment>` | 按赖子交互授权接受，确认后仍需校验跟牌 |
| `ContinueGameRequest` | 请求加赛或无尽模式 | `mode: ContinueMode` | 游戏结束后的模式请求；多人一致性规则待既有房间模块确定 |
| `StartNewGameRequest` | 请求开始新一场 | 无额外参数 | 按房间流程验证，新游戏重新初始化资源 |
| `StateSyncRequest` | 请求状态同步 | `lastSeenVersion: std::uint64_t` | 只读同步通道，不受出牌请求忙碌门禁限制 |
| `PreviewPlayRequest` | 查询选牌的合法方案 | `turnId: TurnId`；`cards: std::vector<CardId>` | 只读预览；不提交出牌、不扣次数、不推进实际随机序列 |
| `QuoteShopOperationRequest` | 查询商店操作报价 | `shopId: ShopId`；`operation: ShopOperation`；`productId: std::optional<ProductId>`；`card: std::optional<CardRef>`；`pattern: std::optional<HandPattern>`；`facilityId: std::optional<DefinitionId>` | 只读预览；不因重复查询消耗免费额度 |

```cpp
namespace cardgame {

class BuyShopItemRequest final : public Event {
public:
    BuyShopItemRequest()
        : Event(EventType::BuyShopItemRequest) {}
    ShopId shopId{};  // 商店会话
    SlotId slotId{};  // 货位
    ProductId productId{};  // 商品
};  // 玩家请求：购买普通货架商品；商店队列；价格由服务端计算。

class BuyBoosterPackRequest final : public Event {
public:
    BuyBoosterPackRequest()
        : Event(EventType::BuyBoosterPackRequest) {}
    ShopId shopId{};  // 商店会话
    PackId packId{};  // 展示中的卡包实例
};  // 玩家请求：购买并立即打开卡包；商店队列；验证空槽与包限购，扣款后立即开包。

class SellFunctionCardRequest final : public Event {
public:
    SellFunctionCardRequest()
        : Event(EventType::SellFunctionCardRequest) {}
    ShopId shopId{};  // 商店会话
    CardRef card{};  // 功能卡实例
};  // 玩家请求：出售功能卡；商店队列；出售成本和成长清理按既有规则。

class RefreshShopRequest final : public Event {
public:
    RefreshShopRequest()
        : Event(EventType::RefreshShopRequest) {}
    ShopId shopId{};  // 商店会话
};  // 玩家请求：刷新普通货架；商店队列；出队时检查刷新上限及当前报价。

class LockShopSlotRequest final : public Event {
public:
    LockShopSlotRequest()
        : Event(EventType::LockShopSlotRequest) {}
    ShopId shopId{};  // 商店会话
    SlotId slotId{};  // 货位
};  // 玩家请求：锁定货位；由调度器与业务模块校验当前状态、权限和参数后处理。

class UnlockShopSlotRequest final : public Event {
public:
    UnlockShopSlotRequest()
        : Event(EventType::UnlockShopSlotRequest) {}
    ShopId shopId{};  // 商店会话
    SlotId slotId{};  // 货位
};  // 玩家请求：解锁货位；由调度器与业务模块校验当前状态、权限和参数后处理。

class ChooseBoosterCardRequest final : public Event {
public:
    ChooseBoosterCardRequest()
        : Event(EventType::ChooseBoosterCardRequest) {}
    ChoiceId choiceId{};  // 待处理交互
    ChoiceOptionId optionId{};  // 选中的候选
};  // 玩家请求：选择卡包候选；授权选择回复，不作为下一次普通购物排队。

class SkipBoosterPackRequest final : public Event {
public:
    SkipBoosterPackRequest()
        : Event(EventType::SkipBoosterPackRequest) {}
    ChoiceId choiceId{};  // 待处理交互
};  // 玩家请求：放弃已打开卡包；授权选择回复；不退已支付包价。

class UpgradeHandPatternRequest final : public Event {
public:
    UpgradeHandPatternRequest()
        : Event(EventType::UpgradeHandPatternRequest) {}
    ShopId shopId{};  // 商店会话
    HandPattern pattern{};  // 牌型
};  // 玩家请求：付费加强牌型；由调度器与业务模块校验当前状态、权限和参数后处理。

class BuyFacilityRequest final : public Event {
public:
    BuyFacilityRequest()
        : Event(EventType::BuyFacilityRequest) {}
    ShopId shopId{};  // 商店会话
    DefinitionId facilityId{};  // 设施定义
};  // 玩家请求：购买设施；由调度器与业务模块校验当前状态、权限和参数后处理。

class FinishShoppingRequest final : public Event {
public:
    FinishShoppingRequest()
        : Event(EventType::FinishShoppingRequest) {}
    ShopId shopId{};  // 商店会话
};  // 玩家请求：完成本次购物；由调度器与业务模块校验当前状态、权限和参数后处理。

class PlayCardsRequest final : public Event {
public:
    PlayCardsRequest()
        : Event(EventType::PlayCardsRequest) {}
    TurnId turnId{};  // 所属回合
    std::vector<CardId> cards{};  // 实际扑克牌实例
    std::vector<WildcardAssignment> wildcards{};  // 赖子指定方案
};  // 玩家请求：确认出牌；局内直接；合法提交后才产生出牌成功通知。

class PassRequest final : public Event {
public:
    PassRequest()
        : Event(EventType::PassRequest) {}
    TurnId turnId{};  // 所属回合
};  // 玩家请求：主动不出牌；局内直接；先手不能因此跳过领出。

class ActivateAbilityRequest final : public Event {
public:
    ActivateAbilityRequest()
        : Event(EventType::ActivateAbilityRequest) {}
    AbilityRef ability{};  // 目标技能
    TargetSpec selection{};  // 目标与方案
};  // 玩家请求：发动主动技能；校验技能归属、窗口、费用与次数；商店和局内依窗口处理。

class SubmitChoiceRequest final : public Event {
public:
    SubmitChoiceRequest()
        : Event(EventType::SubmitChoiceRequest) {}
    ChoiceId choiceId{};  // 待处理交互
    std::vector<ChoiceOptionId> options{};  // 选中的合法选项
};  // 玩家请求：提交一般目标或方案；按交互授权接受，允许非当前行动玩家回复。

class ResolveWildcardRequest final : public Event {
public:
    ResolveWildcardRequest()
        : Event(EventType::ResolveWildcardRequest) {}
    ChoiceId choiceId{};  // 待处理交互
    std::vector<WildcardAssignment> wildcards{};  // 指定方案
};  // 玩家请求：确认赖子指定；按赖子交互授权接受，确认后仍需校验跟牌。

class ContinueGameRequest final : public Event {
public:
    ContinueGameRequest()
        : Event(EventType::ContinueGameRequest) {}
    ContinueMode mode{};  // 续玩模式
};  // 玩家请求：请求加赛或无尽模式；游戏结束后的模式请求；多人一致性规则待既有房间模块确定。

class StartNewGameRequest final : public Event {
public:
    StartNewGameRequest()
        : Event(EventType::StartNewGameRequest) {}
};  // 玩家请求：请求开始新一场；按房间流程验证，新游戏重新初始化资源。

class StateSyncRequest final : public Event {
public:
    StateSyncRequest()
        : Event(EventType::StateSyncRequest) {}
    std::uint64_t lastSeenVersion{};  // 客户端已知版本
};  // 玩家请求：请求状态同步；只读同步通道，不受出牌请求忙碌门禁限制。

class PreviewPlayRequest final : public Event {
public:
    PreviewPlayRequest()
        : Event(EventType::PreviewPlayRequest) {}
    TurnId turnId{};  // 所属回合
    std::vector<CardId> cards{};  // 实际扑克牌实例
};  // 玩家请求：查询选牌的合法方案；只读预览；不提交出牌、不扣次数、不推进实际随机序列。

class QuoteShopOperationRequest final : public Event {
public:
    QuoteShopOperationRequest()
        : Event(EventType::QuoteShopOperationRequest) {}
    ShopId shopId{};  // 商店会话
    ShopOperation operation{};  // 报价业务
    std::optional<ProductId> productId{};  // 购买商品
    std::optional<CardRef> card{};  // 出售实例
    std::optional<HandPattern> pattern{};  // 加强牌型
    std::optional<DefinitionId> facilityId{};  // 设施
};  // 玩家请求：查询商店操作报价；只读预览；不因重复查询消耗免费额度。

}  // namespace cardgame
```

#### 2.7.2 业务通知（63 个）

| 事件派生类 | 含义 | 专属参数 | 时机或执行约束 |
|---|---|---|---|
| `GameStartedEvent` | 整场开始 | `participants: std::vector<Players>`；`roundCount: int` | `GameStart` |
| `ShopEnteredEvent` | 进入局前商店 | `player: Players`；`shopId: ShopId` | `ShopEnter` |
| `ShopAbilityActivationEvent` | 商店窗口内的主动技能调用 | `ability: AbilityRef`；`selection: TargetSpec` | `ShopActionWindow`；只对应一次已验证主动调用，不能作为每帧被动通知 |
| `BeforeShopRefreshEvent` | 刷新报价前 | `player: Players`；`shopId: ShopId`；`settlementId: SettlementId`；`initialQuote: ShopData` | `ShopRefreshQuote` |
| `ShopRefreshedEvent` | 刷新成功后 | `player: Players`；`shopId: ShopId`；`slots: std::vector<SlotId>`；`paid: CoinValue` | `ShopRefreshAfter` |
| `BeforeShopPurchaseEvent` | 购买报价前 | `player: Players`；`shopId: ShopId`；`settlementId: SettlementId`；`productId: ProductId`；`initialQuote: ShopData` | `ShopBuyQuote` |
| `ShopPurchasedEvent` | 购买交付成功后 | `player: Players`；`shopId: ShopId`；`productId: ProductId`；`paid: CoinValue` | `ShopBuyAfter` |
| `BeforeShopSaleEvent` | 出售报价前 | `player: Players`；`shopId: ShopId`；`card: CardRef`；`settlementId: SettlementId`；`initialQuote: ShopData` | `ShopSellQuote` |
| `ShopSoldEvent` | 出售成功后 | `player: Players`；`shopId: ShopId`；`card: CardRef`；`received: CoinValue` | `ShopSellAfter` |
| `ShopLeftEvent` | 离店 | `player: Players`；`shopId: ShopId`；`timedOut: bool` | `ShopLeave` |
| `RoundStartedEvent` | 本局开始 | `roundId: RoundId`；`firstPlayer: Players` | `RoundStart` |
| `BeforeInitialDealEvent` | 初始发牌前 | `roundId: RoundId`；`cardsPerPlayer: int` | `InitialDealBefore` |
| `InitialCardsDealtEvent` | 全体初始发牌完成 | `roundId: RoundId` | `InitialDealAfter` |
| `TrickStartedEvent` | 本巡开始 | `roundId: RoundId`；`trickId: TrickId`；`leader: Players` | `TrickStart` |
| `TurnStartedEvent` | 当前回合开始 | `turnId: TurnId`；`player: Players` | `TurnStart` |
| `TurnAbilityActivationEvent` | 出牌窗口内的主动技能调用 | `turnId: TurnId`；`ability: AbilityRef`；`selection: TargetSpec` | `PlayActionWindow`；只对应一次已验证主动调用，不能作为每帧被动通知 |
| `PlayValidationEvent` | 验证选牌及跟牌 | `turnId: TurnId`；`player: Players`；`cards: std::vector<CardId>`；`wildcards: std::vector<WildcardAssignment>` | `PlayValidate` |
| `CardsPlayedEvent` | 成功提交一手牌 | `turnId: TurnId`；`player: Players`；`handId: PlayedHandId`；`cards: std::vector<CardId>`；`pattern: HandPattern` | `PlayCommitted` |
| `BeforePlayRewardEvent` | 计算本手出牌奖励前 | `turnId: TurnId`；`player: Players`；`handId: PlayedHandId`；`settlementId: SettlementId`；`initialReward: RewardData` | `PlayRewardBefore` |
| `PlayRewardGrantedEvent` | 本手奖励到账后 | `turnId: TurnId`；`player: Players`；`handId: PlayedHandId`；`reward: RewardData` | `PlayRewardAfter` |
| `PlayerPassedEvent` | 确认不出牌 | `turnId: TurnId`；`player: Players`；`reason: PassReason` | `PassCommitted` |
| `TurnEndedEvent` | 当前回合结算结束 | `turnId: TurnId`；`player: Players` | `TurnEnd` |
| `TrickEndedEvent` | 获得牌权并结束本巡 | `trickId: TrickId`；`leader: Players` | `TrickEnd` |
| `BeforeCardsDrawEvent` | 非初始摸牌前 | `player: Players`；`settlementId: SettlementId`；`requestedCount: int`；`reason: DrawReason` | `DrawBefore` |
| `CardsDrawnEvent` | 实际摸牌完成 | `player: Players`；`cards: std::vector<CardId>`；`reason: DrawReason` | `DrawAfter` |
| `TrickDrawingCompletedEvent` | 巡末摸牌步骤完成 | `trickId: TrickId`；`actualTotal: int` | `TrickDrawsAfter` |
| `RoundEndedEvent` | 本局结束且最后回合结算完成 | `roundId: RoundId`；`resultId: RoundResultId` | `RoundEnd` |
| `BeforeRoundRewardEvent` | 计算玩家局末奖励前 | `roundId: RoundId`；`player: Players`；`settlementId: SettlementId`；`initialReward: RewardData` | `RoundRewardBefore` |
| `RoundRewardsGrantedEvent` | 全体局末奖励与扣款完成 | `roundId: RoundId`；`resultId: RoundResultId` | `RoundRewardAfter` |
| `GameEndedEvent` | 整场结束与冠军确认 | `winners: std::vector<Players>`；`finalVersion: std::uint64_t` | `GameEnd` |
| `PlayingCardsChangedEvent` | 技能实际改变牌面或手牌后 | `player: Players`；`cards: std::vector<CardId>`；`reason: std::string` | `CardChanged` |
| `AbilityResolvedEvent` | 技能效果成功结算后 | `ability: AbilityRef`；`triggerEventId: EventId` | `AbilityResolved` |
| `FunctionCardEquippedEvent` | 功能卡进入槽位 | `card: CardRef`；`slot: std::size_t` | `FunctionCardEquipped` |
| `FunctionCardRemovedEvent` | 功能卡离开槽位 | `card: CardRef`；`reason: RemovalReason` | `FunctionCardRemoved` |
| `ShopSlotLockedEvent` | 货位锁定成功 | `player: Players`；`shopId: ShopId`；`slotId: SlotId` | `ShopSlotLocked` |
| `ShopSlotUnlockedEvent` | 货位解锁成功 | `player: Players`；`shopId: ShopId`；`slotId: SlotId` | `ShopSlotUnlocked` |
| `BoosterPackOpenedEvent` | 卡包已付费打开 | `player: Players`；`packId: PackId`；`choiceId: ChoiceId` | `BoosterPackOpened` |
| `BoosterPackChoiceCompletedEvent` | 卡包候选选取完成 | `player: Players`；`packId: PackId`；`options: std::vector<ChoiceOptionId>` | `BoosterPackChoiceCompleted` |
| `BoosterPackSkippedEvent` | 卡包已放弃 | `player: Players`；`packId: PackId` | `BoosterPackSkipped` |
| `ExpansionCardUsedEvent` | 拓展牌获得即使用完成 | `player: Players`；`expansionId: DefinitionId`；`selection: TargetSpec` | `ExpansionCardUsed` |
| `HandPatternUpgradedEvent` | 牌型加强完成 | `player: Players`；`pattern: HandPattern`；`beforeLevel: int`；`afterLevel: int` | `HandPatternUpgraded` |
| `FacilityPurchasedEvent` | 设施购买生效 | `player: Players`；`facilityId: DefinitionId` | `FacilityPurchased` |
| `CoinsChangedEvent` | 金币余额实际变化 | `player: Players`；`before: CoinValue`；`after: CoinValue`；`reason: std::string` | `CoinsChanged` |
| `ScoreChangedEvent` | 累计分数实际变化 | `player: Players`；`before: ScoreValue`；`after: ScoreValue` | `ScoreChanged` |
| `PlayerStatusChangedEvent` | 玩家状态实际变化 | `player: Players`；`statusId: StatusId`；`reason: std::string` | `PlayerStatusChanged` |
| `AbilityAddedEvent` | 技能添加完成 | `ability: AbilityRef` | `AbilityAdded` |
| `AbilityRemovedEvent` | 技能逻辑移除完成 | `ability: AbilityRef` | `AbilityRemoved` |
| `AbilityAbolishedEvent` | 技能被永久废除 | `ability: AbilityRef` | `AbilityAbolished` |
| `AbilityUsabilityChangedEvent` | 技能临时可用开关变化 | `ability: AbilityRef`；`usable: bool` | `AbilityUsabilityChanged` |
| `ChoiceOpenedEvent` | 交互选择窗口建立 | `player: Players`；`choiceId: ChoiceId`；`choiceKind: ChoiceKind` | `ChoiceOpened` |
| `ChoiceResolvedEvent` | 交互选择处理完成 | `player: Players`；`choiceId: ChoiceId`；`options: std::vector<ChoiceOptionId>`；`reason: ChoiceReason` | `ChoiceResolved` |
| `ChoiceExpiredEvent` | 交互期限已到并完成默认处理 | `player: Players`；`choiceId: ChoiceId` | `ChoiceExpired` |
| `RoundWinnerLockedEvent` | 出空时锁定赢家与手牌快照 | `roundId: RoundId`；`winner: Players`；`resultId: RoundResultId` | `RoundWinnerLocked` |
| `DrawPileExhaustedEvent` | 牌堆由非空变为空 | `roundId: RoundId` | `DrawPileExhausted` |
| `AllShopsClosedEvent` | 全体商店会话关闭且交易结清 | `roundId: RoundId` | `AllShopsClosed` |
| `FunctionCardUsabilityChangedEvent` | 整张功能卡临时开关变化 | `card: CardRef`；`usable: bool` | `FunctionCardUsabilityChanged` |
| `FunctionCardAbolishedEvent` | 整张功能卡被废除 | `card: CardRef` | `FunctionCardAbolished` |
| `AbilityStateChangedEvent` | 技能成长或蓄力状态变化 | `ability: AbilityRef`；`counterId: CounterId`；`before: int`；`after: int` | `AbilityStateChanged` |
| `DelayedEffectScheduledEvent` | 延迟效果标记建立 | `player: Players`；`delayedEffectId: DelayedEffectId` | `DelayedEffectScheduled` |
| `DelayedEffectTriggeredEvent` | 延迟标记实际执行完成 | `player: Players`；`delayedEffectId: DelayedEffectId` | `DelayedEffectTriggered` |
| `DelayedEffectExpiredEvent` | 延迟标记结束或失效 | `player: Players`；`delayedEffectId: DelayedEffectId` | `DelayedEffectExpired` |
| `CoatingAppliedEvent` | 镀层施加完成 | `card: CardRef`；`coating: CoatingType` | `CoatingApplied` |
| `CardVisibilityChangedEvent` | 查看权限或公开信息变化 | `viewer: Players`；`cards: std::vector<CardId>` | `CardVisibilityChanged` |

```cpp
namespace cardgame {

class GameStartedEvent final : public Event {
public:
    GameStartedEvent()
        : Event(EventType::GameStart) {}
    std::vector<Players> participants{};  // 实际参局玩家
    int roundCount{};  // 标准局数
};  // 业务通知：整场开始，在对应操作提交后发布，供已注册功能牌响应。

class ShopEnteredEvent final : public Event {
public:
    ShopEnteredEvent()
        : Event(EventType::ShopEnter) {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
};  // 业务通知：进入局前商店，在对应操作提交后发布，供已注册功能牌响应。

class ShopAbilityActivationEvent final : public Event {
public:
    ShopAbilityActivationEvent()
        : Event(EventType::ShopActionWindow) {}
    AbilityRef ability{};  // 目标技能
    TargetSpec selection{};  // 目标与方案
};  // 主动调用通知：商店窗口内的主动技能调用，由调度器验证请求后只派发给指定功能牌。

class BeforeShopRefreshEvent final : public Event {
public:
    BeforeShopRefreshEvent()
        : Event(EventType::ShopRefreshQuote) {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    SettlementId settlementId{};  // 待结算事务
    ShopData initialQuote{};  // 初始报价
};  // 业务通知：刷新报价前，在对应操作提交后发布，供已注册功能牌响应。

class ShopRefreshedEvent final : public Event {
public:
    ShopRefreshedEvent()
        : Event(EventType::ShopRefreshAfter) {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    std::vector<SlotId> slots{};  // 实际替换的货位
    CoinValue paid{};  // 实际费用
};  // 业务通知：刷新成功后，在对应操作提交后发布，供已注册功能牌响应。

class BeforeShopPurchaseEvent final : public Event {
public:
    BeforeShopPurchaseEvent()
        : Event(EventType::ShopBuyQuote) {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    SettlementId settlementId{};  // 待结算事务
    ProductId productId{};  // 商品
    ShopData initialQuote{};  // 初始报价
};  // 业务通知：购买报价前，在对应操作提交后发布，供已注册功能牌响应。

class ShopPurchasedEvent final : public Event {
public:
    ShopPurchasedEvent()
        : Event(EventType::ShopBuyAfter) {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    ProductId productId{};  // 商品
    CoinValue paid{};  // 实际费用
};  // 业务通知：购买交付成功后，在对应操作提交后发布，供已注册功能牌响应。

class BeforeShopSaleEvent final : public Event {
public:
    BeforeShopSaleEvent()
        : Event(EventType::ShopSellQuote) {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    CardRef card{};  // 功能卡实例
    SettlementId settlementId{};  // 待结算事务
    ShopData initialQuote{};  // 初始报价
};  // 业务通知：出售报价前，在对应操作提交后发布，供已注册功能牌响应。

class ShopSoldEvent final : public Event {
public:
    ShopSoldEvent()
        : Event(EventType::ShopSellAfter) {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    CardRef card{};  // 功能卡实例
    CoinValue received{};  // 实际收入
};  // 业务通知：出售成功后，在对应操作提交后发布，供已注册功能牌响应。

class ShopLeftEvent final : public Event {
public:
    ShopLeftEvent()
        : Event(EventType::ShopLeave) {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    bool timedOut{};  // 是否商店超时
};  // 业务通知：离店，在对应操作提交后发布，供已注册功能牌响应。

class RoundStartedEvent final : public Event {
public:
    RoundStartedEvent()
        : Event(EventType::RoundStart) {}
    RoundId roundId{};  // 所属局
    Players firstPlayer{};  // 本局先手
};  // 业务通知：本局开始，在对应操作提交后发布，供已注册功能牌响应。

class BeforeInitialDealEvent final : public Event {
public:
    BeforeInitialDealEvent()
        : Event(EventType::InitialDealBefore) {}
    RoundId roundId{};  // 所属局
    int cardsPerPlayer = 7;  // 既有规则起手数量
};  // 业务通知：初始发牌前，在对应操作提交后发布，供已注册功能牌响应。

class InitialCardsDealtEvent final : public Event {
public:
    InitialCardsDealtEvent()
        : Event(EventType::InitialDealAfter) {}
    RoundId roundId{};  // 所属局
};  // 业务通知：全体初始发牌完成，在对应操作提交后发布，供已注册功能牌响应。

class TrickStartedEvent final : public Event {
public:
    TrickStartedEvent()
        : Event(EventType::TrickStart) {}
    RoundId roundId{};  // 所属局
    TrickId trickId{};  // 所属巡
    Players leader{};  // 领出玩家
};  // 业务通知：本巡开始，在对应操作提交后发布，供已注册功能牌响应。

class TurnStartedEvent final : public Event {
public:
    TurnStartedEvent()
        : Event(EventType::TurnStart) {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
};  // 业务通知：当前回合开始，在对应操作提交后发布，供已注册功能牌响应。

class TurnAbilityActivationEvent final : public Event {
public:
    TurnAbilityActivationEvent()
        : Event(EventType::PlayActionWindow) {}
    TurnId turnId{};  // 所属回合
    AbilityRef ability{};  // 目标技能
    TargetSpec selection{};  // 目标与方案
};  // 主动调用通知：出牌窗口内的主动技能调用，由调度器验证请求后只派发给指定功能牌。

class PlayValidationEvent final : public Event {
public:
    PlayValidationEvent()
        : Event(EventType::PlayValidate) {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
    std::vector<CardId> cards{};  // 实际扑克牌实例
    std::vector<WildcardAssignment> wildcards{};  // 指定方案
};  // 业务通知：验证选牌及跟牌，在对应操作提交后发布，供已注册功能牌响应。

class CardsPlayedEvent final : public Event {
public:
    CardsPlayedEvent()
        : Event(EventType::PlayCommitted) {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
    PlayedHandId handId{};  // 已提交的一手牌
    std::vector<CardId> cards{};  // 实际扑克牌实例
    HandPattern pattern{};  // 最终牌型
};  // 业务通知：成功提交一手牌，在对应操作提交后发布，供已注册功能牌响应。

class BeforePlayRewardEvent final : public Event {
public:
    BeforePlayRewardEvent()
        : Event(EventType::PlayRewardBefore) {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
    PlayedHandId handId{};  // 对应出牌
    SettlementId settlementId{};  // 待结算事务
    RewardData initialReward{};  // 原始奖励
};  // 业务通知：计算本手出牌奖励前，在对应操作提交后发布，供已注册功能牌响应。

class PlayRewardGrantedEvent final : public Event {
public:
    PlayRewardGrantedEvent()
        : Event(EventType::PlayRewardAfter) {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
    PlayedHandId handId{};  // 对应出牌
    RewardData reward{};  // 实际到账
};  // 业务通知：本手奖励到账后，在对应操作提交后发布，供已注册功能牌响应。

class PlayerPassedEvent final : public Event {
public:
    PlayerPassedEvent()
        : Event(EventType::PassCommitted) {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
    PassReason reason{};  // 主动、超时或掉线
};  // 业务通知：确认不出牌，在对应操作提交后发布，供已注册功能牌响应。

class TurnEndedEvent final : public Event {
public:
    TurnEndedEvent()
        : Event(EventType::TurnEnd) {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
};  // 业务通知：当前回合结算结束，在对应操作提交后发布，供已注册功能牌响应。

class TrickEndedEvent final : public Event {
public:
    TrickEndedEvent()
        : Event(EventType::TrickEnd) {}
    TrickId trickId{};  // 所属巡
    Players leader{};  // 牌权获得者
};  // 业务通知：获得牌权并结束本巡，在对应操作提交后发布，供已注册功能牌响应。

class BeforeCardsDrawEvent final : public Event {
public:
    BeforeCardsDrawEvent()
        : Event(EventType::DrawBefore) {}
    Players player{};  // 相关玩家
    SettlementId settlementId{};  // 待结算事务
    int requestedCount{};  // 请求摸牌数
    DrawReason reason{};  // 摸牌原因
};  // 业务通知：非初始摸牌前，在对应操作提交后发布，供已注册功能牌响应。

class CardsDrawnEvent final : public Event {
public:
    CardsDrawnEvent()
        : Event(EventType::DrawAfter) {}
    Players player{};  // 相关玩家
    std::vector<CardId> cards{};  // 实际扑克牌实例
    DrawReason reason{};  // 摸牌原因
};  // 业务通知：实际摸牌完成，在对应操作提交后发布，供已注册功能牌响应。

class TrickDrawingCompletedEvent final : public Event {
public:
    TrickDrawingCompletedEvent()
        : Event(EventType::TrickDrawsAfter) {}
    TrickId trickId{};  // 所属巡
    int actualTotal{};  // 全体实际摸牌总数
};  // 业务通知：巡末摸牌步骤完成，在对应操作提交后发布，供已注册功能牌响应。

class RoundEndedEvent final : public Event {
public:
    RoundEndedEvent()
        : Event(EventType::RoundEnd) {}
    RoundId roundId{};  // 所属局
    RoundResultId resultId{};  // 已锁定结果
};  // 业务通知：本局结束且最后回合结算完成，在对应操作提交后发布，供已注册功能牌响应。

class BeforeRoundRewardEvent final : public Event {
public:
    BeforeRoundRewardEvent()
        : Event(EventType::RoundRewardBefore) {}
    RoundId roundId{};  // 所属局
    Players player{};  // 相关玩家
    SettlementId settlementId{};  // 待结算事务
    RewardData initialReward{};  // 基础待结算奖励
};  // 业务通知：计算玩家局末奖励前，在对应操作提交后发布，供已注册功能牌响应。

class RoundRewardsGrantedEvent final : public Event {
public:
    RoundRewardsGrantedEvent()
        : Event(EventType::RoundRewardAfter) {}
    RoundId roundId{};  // 所属局
    RoundResultId resultId{};  // 实际结算记录
};  // 业务通知：全体局末奖励与扣款完成，在对应操作提交后发布，供已注册功能牌响应。

class GameEndedEvent final : public Event {
public:
    GameEndedEvent()
        : Event(EventType::GameEnd) {}
    std::vector<Players> winners{};  // 并列冠军
    std::uint64_t finalVersion{};  // 最终状态版本
};  // 业务通知：整场结束与冠军确认，在对应操作提交后发布，供已注册功能牌响应。

class PlayingCardsChangedEvent final : public Event {
public:
    PlayingCardsChangedEvent()
        : Event(EventType::CardChanged) {}
    Players player{};  // 相关玩家
    std::vector<CardId> cards{};  // 实际扑克牌实例
    std::string reason{};  // 变化原因
};  // 业务通知：技能实际改变牌面或手牌后，在对应操作提交后发布，供已注册功能牌响应。

class AbilityResolvedEvent final : public Event {
public:
    AbilityResolvedEvent()
        : Event(EventType::AbilityResolved) {}
    AbilityRef ability{};  // 目标技能
    EventId triggerEventId{};  // 触发事件
};  // 业务通知：技能效果成功结算后，在对应操作提交后发布，供已注册功能牌响应。

class FunctionCardEquippedEvent final : public Event {
public:
    FunctionCardEquippedEvent()
        : Event(EventType::FunctionCardEquipped) {}
    CardRef card{};  // 功能卡实例
    std::size_t slot{};  // 实际槽位
};  // 业务通知：功能卡进入槽位，在对应操作提交后发布，供已注册功能牌响应。

class FunctionCardRemovedEvent final : public Event {
public:
    FunctionCardRemovedEvent()
        : Event(EventType::FunctionCardRemoved) {}
    CardRef card{};  // 功能卡实例
    RemovalReason reason{};  // 移除原因
};  // 业务通知：功能卡离开槽位，在对应操作提交后发布，供已注册功能牌响应。

class ShopSlotLockedEvent final : public Event {
public:
    ShopSlotLockedEvent()
        : Event(EventType::ShopSlotLocked) {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    SlotId slotId{};  // 货位
};  // 业务通知：货位锁定成功，在对应操作提交后发布，供已注册功能牌响应。

class ShopSlotUnlockedEvent final : public Event {
public:
    ShopSlotUnlockedEvent()
        : Event(EventType::ShopSlotUnlocked) {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    SlotId slotId{};  // 货位
};  // 业务通知：货位解锁成功，在对应操作提交后发布，供已注册功能牌响应。

class BoosterPackOpenedEvent final : public Event {
public:
    BoosterPackOpenedEvent()
        : Event(EventType::BoosterPackOpened) {}
    Players player{};  // 相关玩家
    PackId packId{};  // 包实例
    ChoiceId choiceId{};  // 待处理交互
};  // 业务通知：卡包已付费打开，在对应操作提交后发布，供已注册功能牌响应。

class BoosterPackChoiceCompletedEvent final : public Event {
public:
    BoosterPackChoiceCompletedEvent()
        : Event(EventType::BoosterPackChoiceCompleted) {}
    Players player{};  // 相关玩家
    PackId packId{};  // 包实例
    std::vector<ChoiceOptionId> options{};  // 实际选项
};  // 业务通知：卡包候选选取完成，在对应操作提交后发布，供已注册功能牌响应。

class BoosterPackSkippedEvent final : public Event {
public:
    BoosterPackSkippedEvent()
        : Event(EventType::BoosterPackSkipped) {}
    Players player{};  // 相关玩家
    PackId packId{};  // 包实例
};  // 业务通知：卡包已放弃，在对应操作提交后发布，供已注册功能牌响应。

class ExpansionCardUsedEvent final : public Event {
public:
    ExpansionCardUsedEvent()
        : Event(EventType::ExpansionCardUsed) {}
    Players player{};  // 相关玩家
    DefinitionId expansionId{};  // 拓展牌定义
    TargetSpec selection{};  // 目标与方案
};  // 业务通知：拓展牌获得即使用完成，在对应操作提交后发布，供已注册功能牌响应。

class HandPatternUpgradedEvent final : public Event {
public:
    HandPatternUpgradedEvent()
        : Event(EventType::HandPatternUpgraded) {}
    Players player{};  // 相关玩家
    HandPattern pattern{};  // 牌型
    int beforeLevel{};  // 原等级
    int afterLevel{};  // 新等级
};  // 业务通知：牌型加强完成，在对应操作提交后发布，供已注册功能牌响应。

class FacilityPurchasedEvent final : public Event {
public:
    FacilityPurchasedEvent()
        : Event(EventType::FacilityPurchased) {}
    Players player{};  // 相关玩家
    DefinitionId facilityId{};  // 设施定义
};  // 业务通知：设施购买生效，在对应操作提交后发布，供已注册功能牌响应。

class CoinsChangedEvent final : public Event {
public:
    CoinsChangedEvent()
        : Event(EventType::CoinsChanged) {}
    Players player{};  // 相关玩家
    CoinValue before{};  // 变化前
    CoinValue after{};  // 变化后
    std::string reason{};  // 收入或支出原因
};  // 业务通知：金币余额实际变化，在对应操作提交后发布，供已注册功能牌响应。

class ScoreChangedEvent final : public Event {
public:
    ScoreChangedEvent()
        : Event(EventType::ScoreChanged) {}
    Players player{};  // 相关玩家
    ScoreValue before{};  // 变化前
    ScoreValue after{};  // 变化后
};  // 业务通知：累计分数实际变化，在对应操作提交后发布，供已注册功能牌响应。

class PlayerStatusChangedEvent final : public Event {
public:
    PlayerStatusChangedEvent()
        : Event(EventType::PlayerStatusChanged) {}
    Players player{};  // 相关玩家
    StatusId statusId{};  // 状态实例
    std::string reason{};  // 增加、移除或层数变化
};  // 业务通知：玩家状态实际变化，在对应操作提交后发布，供已注册功能牌响应。

class AbilityAddedEvent final : public Event {
public:
    AbilityAddedEvent()
        : Event(EventType::AbilityAdded) {}
    AbilityRef ability{};  // 目标技能
};  // 业务通知：技能添加完成，在对应操作提交后发布，供已注册功能牌响应。

class AbilityRemovedEvent final : public Event {
public:
    AbilityRemovedEvent()
        : Event(EventType::AbilityRemoved) {}
    AbilityRef ability{};  // 目标技能
};  // 业务通知：技能逻辑移除完成，在对应操作提交后发布，供已注册功能牌响应。

class AbilityAbolishedEvent final : public Event {
public:
    AbilityAbolishedEvent()
        : Event(EventType::AbilityAbolished) {}
    AbilityRef ability{};  // 目标技能
};  // 业务通知：技能被永久废除，在对应操作提交后发布，供已注册功能牌响应。

class AbilityUsabilityChangedEvent final : public Event {
public:
    AbilityUsabilityChangedEvent()
        : Event(EventType::AbilityUsabilityChanged) {}
    AbilityRef ability{};  // 目标技能
    bool usable{};  // 当前开关
};  // 业务通知：技能临时可用开关变化，在对应操作提交后发布，供已注册功能牌响应。

class ChoiceOpenedEvent final : public Event {
public:
    ChoiceOpenedEvent()
        : Event(EventType::ChoiceOpened) {}
    Players player{};  // 相关玩家
    ChoiceId choiceId{};  // 待处理交互
    ChoiceKind choiceKind{};  // 交互类型
};  // 业务通知：交互选择窗口建立，在对应操作提交后发布，供已注册功能牌响应。

class ChoiceResolvedEvent final : public Event {
public:
    ChoiceResolvedEvent()
        : Event(EventType::ChoiceResolved) {}
    Players player{};  // 相关玩家
    ChoiceId choiceId{};  // 待处理交互
    std::vector<ChoiceOptionId> options{};  // 实际选择
    ChoiceReason reason{};  // 手动、超时或掉线
};  // 业务通知：交互选择处理完成，在对应操作提交后发布，供已注册功能牌响应。

class ChoiceExpiredEvent final : public Event {
public:
    ChoiceExpiredEvent()
        : Event(EventType::ChoiceExpired) {}
    Players player{};  // 相关玩家
    ChoiceId choiceId{};  // 待处理交互
};  // 业务通知：交互期限已到并完成默认处理，在对应操作提交后发布，供已注册功能牌响应。

class RoundWinnerLockedEvent final : public Event {
public:
    RoundWinnerLockedEvent()
        : Event(EventType::RoundWinnerLocked) {}
    RoundId roundId{};  // 所属局
    Players winner{};  // 赢家
    RoundResultId resultId{};  // 冻结结果
};  // 业务通知：出空时锁定赢家与手牌快照，在对应操作提交后发布，供已注册功能牌响应。

class DrawPileExhaustedEvent final : public Event {
public:
    DrawPileExhaustedEvent()
        : Event(EventType::DrawPileExhausted) {}
    RoundId roundId{};  // 所属局
};  // 业务通知：牌堆由非空变为空，在对应操作提交后发布，供已注册功能牌响应。

class AllShopsClosedEvent final : public Event {
public:
    AllShopsClosedEvent()
        : Event(EventType::AllShopsClosed) {}
    RoundId roundId{};  // 所属局
};  // 业务通知：全体商店会话关闭且交易结清，在对应操作提交后发布，供已注册功能牌响应。

class FunctionCardUsabilityChangedEvent final : public Event {
public:
    FunctionCardUsabilityChangedEvent()
        : Event(EventType::FunctionCardUsabilityChanged) {}
    CardRef card{};  // 功能卡实例
    bool usable{};  // 当前开关
};  // 业务通知：整张功能卡临时开关变化，在对应操作提交后发布，供已注册功能牌响应。

class FunctionCardAbolishedEvent final : public Event {
public:
    FunctionCardAbolishedEvent()
        : Event(EventType::FunctionCardAbolished) {}
    CardRef card{};  // 功能卡实例
};  // 业务通知：整张功能卡被废除，在对应操作提交后发布，供已注册功能牌响应。

class AbilityStateChangedEvent final : public Event {
public:
    AbilityStateChangedEvent()
        : Event(EventType::AbilityStateChanged) {}
    AbilityRef ability{};  // 目标技能
    CounterId counterId{};  // 已定义的计数状态
    int before{};  // 变化前
    int after{};  // 变化后
};  // 业务通知：技能成长或蓄力状态变化，在对应操作提交后发布，供已注册功能牌响应。

class DelayedEffectScheduledEvent final : public Event {
public:
    DelayedEffectScheduledEvent()
        : Event(EventType::DelayedEffectScheduled) {}
    Players player{};  // 相关玩家
    DelayedEffectId delayedEffectId{};  // 标记实例
};  // 业务通知：延迟效果标记建立，在对应操作提交后发布，供已注册功能牌响应。

class DelayedEffectTriggeredEvent final : public Event {
public:
    DelayedEffectTriggeredEvent()
        : Event(EventType::DelayedEffectTriggered) {}
    Players player{};  // 相关玩家
    DelayedEffectId delayedEffectId{};  // 标记实例
};  // 业务通知：延迟标记实际执行完成，在对应操作提交后发布，供已注册功能牌响应。

class DelayedEffectExpiredEvent final : public Event {
public:
    DelayedEffectExpiredEvent()
        : Event(EventType::DelayedEffectExpired) {}
    Players player{};  // 相关玩家
    DelayedEffectId delayedEffectId{};  // 标记实例
};  // 业务通知：延迟标记结束或失效，在对应操作提交后发布，供已注册功能牌响应。

class CoatingAppliedEvent final : public Event {
public:
    CoatingAppliedEvent()
        : Event(EventType::CoatingApplied) {}
    CardRef card{};  // 功能卡实例
    CoatingType coating{};  // 镀层
};  // 业务通知：镀层施加完成，在对应操作提交后发布，供已注册功能牌响应。

class CardVisibilityChangedEvent final : public Event {
public:
    CardVisibilityChangedEvent()
        : Event(EventType::CardVisibilityChanged) {}
    Players viewer{};  // 获得信息的玩家
    std::vector<CardId> cards{};  // 实际扑克牌实例
};  // 业务通知：查看权限或公开信息变化，在对应操作提交后发布，供已注册功能牌响应。

}  // namespace cardgame
```

#### 2.7.3 效果请求（38 个）

| 事件派生类 | 含义 | 专属参数 | 时机或执行约束 |
|---|---|---|---|
| `AddScoreEvent` | 增加实际分数 | `target: Players`；`amount: ScoreValue` | 按当前状态和来源校验 |
| `AddCoinsEvent` | 增加实际金币 | `target: Players`；`amount: CoinValue` | 按当前状态和来源校验 |
| `SpendCoinsEvent` | 扣除实际金币 | `target: Players`；`amount: CoinValue`；`reason: std::string` | 按当前状态和来源校验 |
| `AddPendingScoreEvent` | 修正待结算分数 | `target: Players`；`settlementId: SettlementId`；`delta: ScoreValue` | 按当前状态和来源校验 |
| `MultiplyPendingScoreEvent` | 缩放待结算分数 | `target: Players`；`settlementId: SettlementId`；`numerator: int`；`denominator: int`；`rounding: RoundingRule` | 按当前状态和来源校验 |
| `AddPendingCoinsEvent` | 修正待结算金币 | `target: Players`；`settlementId: SettlementId`；`delta: CoinValue`；`component: RewardComponent`；`minimum: CoinValue` | 按当前状态和来源校验 |
| `ReduceShopCostEvent` | 降低本次商店报价 | `target: Players`；`settlementId: SettlementId`；`amount: CoinValue`；`minimum: CoinValue` | 按当前状态和来源校验 |
| `GrantFreeRefreshEvent` | 增加本店免费刷新额度 | `target: Players`；`shopId: ShopId`；`uses: int` | 按当前状态和来源校验 |
| `RefreshShopEvent` | 执行一次货架刷新 | `target: Players`；`shopId: ShopId`；`mode: RefreshMode` | 仍检查本店刷新总上限，免费不推进付费价格序号 |
| `OpenBoosterPackEvent` | 执行已获准的开包 | `target: Players`；`shopId: ShopId`；`packId: PackId`；`settlementId: SettlementId` | 与购包事务关联，不跨输入等待；完成后发布开包通知并建立选择 |
| `LockShopSlotEvent` | 执行货位锁定 | `target: Players`；`shopId: ShopId`；`slotId: SlotId` | 按当前状态和来源校验 |
| `UnlockShopSlotEvent` | 执行货位解锁 | `target: Players`；`shopId: ShopId`；`slotId: SlotId` | 按当前状态和来源校验 |
| `GrantFunctionCardEvent` | 生成并装备功能卡 | `target: Players`；`definitionId: DefinitionId`；`purchaseCost: CoinValue` | 检查空槽；免费生成成本为零 |
| `RemoveFunctionCardEvent` | 移除功能卡 | `card: CardRef`；`reason: RemovalReason` | 按当前状态和来源校验 |
| `ReplaceFunctionCardEvent` | 替换功能卡 | `card: CardRef`；`newDefinitionId: DefinitionId`；`preservePurchaseCost: bool` | 按当前状态和来源校验 |
| `AddAbilityEvent` | 给功能卡添加技能 | `card: CardRef`；`abilityDefinitionId: DefinitionId` | 按当前状态和来源校验 |
| `RemoveAbilityEvent` | 逻辑删除技能 | `ability: AbilityRef` | 按当前状态和来源校验 |
| `AbolishAbilityEvent` | 永久废除技能 | `ability: AbilityRef` | 按当前状态和来源校验 |
| `SetAbilityUsableEvent` | 设置技能临时开关 | `ability: AbilityRef`；`usable: bool` | 按当前状态和来源校验 |
| `SetFunctionCardUsableEvent` | 设置整牌临时开关 | `card: CardRef`；`usable: bool` | 按当前状态和来源校验 |
| `AbolishFunctionCardEvent` | 永久废除整张牌 | `card: CardRef` | 按当前状态和来源校验 |
| `ApplyCoatingEvent` | 给功能卡施加镀层 | `card: CardRef`；`coating: CoatingType` | 按当前状态和来源校验 |
| `DrawCardsEvent` | 按规则执行非初始摸牌 | `target: Players`；`count: int`；`reason: DrawReason` | 锁定本局结束后不得借此进行额外摸牌；牌堆不足按规则处理 |
| `SwapCardsWithDeckEvent` | 将指定手牌换到牌堆底并等量摸顶 | `target: Players`；`cards: std::vector<CardId>` | 保持原牌身份与公共牌组总数 |
| `ChangeCardRankEvent` | 临时改变普通牌点数 | `cardId: CardId`；`rank: int`；`lifetime: Lifetime` | 按当前状态和来源校验 |
| `ChangeCardSuitEvent` | 临时改变花色 | `cardId: CardId`；`suit: Suit`；`lifetime: Lifetime` | 按当前状态和来源校验 |
| `MakeCardJokerEvent` | 将指定牌临时变为王 | `cardId: CardId`；`lifetime: Lifetime` | 按当前状态和来源校验 |
| `RevealCardsEvent` | 向指定玩家展示合法信息 | `viewer: Players`；`target: Players`；`cards: std::vector<CardId>` | 只向有权限的客户端同步牌面 |
| `ApplyPlayerStatusEvent` | 施加玩家状态 | `target: Players`；`statusDefinitionId: DefinitionId`；`stacks: int`；`lifetime: Lifetime` | 按当前状态和来源校验 |
| `RemovePlayerStatusEvent` | 移除玩家状态 | `target: Players`；`statusId: StatusId` | 按当前状态和来源校验 |
| `UpgradeHandPatternEvent` | 执行牌型加强 | `target: Players`；`pattern: HandPattern`；`levels: int` | 按当前状态和来源校验 |
| `PurchaseFacilityEvent` | 执行设施购买与生效 | `target: Players`；`facilityId: DefinitionId` | 按当前状态和来源校验 |
| `UseExpansionCardEvent` | 执行获得即使用的拓展牌 | `target: Players`；`expansionId: DefinitionId`；`selection: TargetSpec` | 按当前状态和来源校验 |
| `AddDelayedEffectEvent` | 建立延迟效果标记 | `target: Players`；`effectDefinitionId: DefinitionId`；`triggerType: EventType`；`selection: TargetSpec`；`lifetime: Lifetime` | 按当前状态和来源校验 |
| `OpenChoiceEvent` | 建立选择窗口并挂起相关流程 | `target: Players`；`choiceKind: ChoiceKind`；`options: std::vector<ChoiceOptionId>`；`timeoutMs: std::uint32_t`；`optional: bool` | 按当前状态和来源校验 |
| `ResolveChoiceEvent` | 执行选择或默认方案 | `choiceId: ChoiceId`；`options: std::vector<ChoiceOptionId>`；`reason: ChoiceReason` | 按当前状态和来源校验 |
| `ChangeAbilityCounterEvent` | 改变已定义的技能成长或蓄力计数 | `ability: AbilityRef`；`counterId: CounterId`；`operation: CounterOperation`；`value: int` | 按当前状态和来源校验 |
| `EventBatch` | 有序原子组合多个效果 | `events: std::vector<EventPtr>` | 只允许 Effect 子事件；先校验后提交，不能跨玩家输入等待 |

```cpp
namespace cardgame {

class AddScoreEvent final : public Event {
public:
    AddScoreEvent()
        : Event(EventType::AddScore) {}
    Players target{};  // 效果目标玩家
    ScoreValue amount{};  // 增加数量
};  // 效果请求：增加实际分数；由对应系统牌与业务模块校验并执行。

class AddCoinsEvent final : public Event {
public:
    AddCoinsEvent()
        : Event(EventType::AddCoins) {}
    Players target{};  // 效果目标玩家
    CoinValue amount{};  // 增加数量
};  // 效果请求：增加实际金币；由对应系统牌与业务模块校验并执行。

class SpendCoinsEvent final : public Event {
public:
    SpendCoinsEvent()
        : Event(EventType::SpendCoins) {}
    Players target{};  // 效果目标玩家
    CoinValue amount{};  // 扣款数量
    std::string reason{};  // 扣款用途
};  // 效果请求：扣除实际金币；由对应系统牌与业务模块校验并执行。

class AddPendingScoreEvent final : public Event {
public:
    AddPendingScoreEvent()
        : Event(EventType::AddPendingScore) {}
    Players target{};  // 效果目标玩家
    SettlementId settlementId{};  // 待结算事务
    ScoreValue delta{};  // 有符号增减值
};  // 效果请求：修正待结算分数；由对应系统牌与业务模块校验并执行。

class MultiplyPendingScoreEvent final : public Event {
public:
    MultiplyPendingScoreEvent()
        : Event(EventType::MultiplyPendingScore) {}
    Players target{};  // 效果目标玩家
    SettlementId settlementId{};  // 待结算事务
    int numerator = 1;  // 倍率分子
    int denominator = 1;  // 倍率分母
    RoundingRule rounding{};  // 取整规则
};  // 效果请求：缩放待结算分数；由对应系统牌与业务模块校验并执行。

class AddPendingCoinsEvent final : public Event {
public:
    AddPendingCoinsEvent()
        : Event(EventType::AddPendingCoins) {}
    Players target{};  // 效果目标玩家
    SettlementId settlementId{};  // 待结算事务
    CoinValue delta{};  // 有符号增减值
    RewardComponent component{};  // 基础或额外奖励
    CoinValue minimum{};  // 该部分最低金额
};  // 效果请求：修正待结算金币；由对应系统牌与业务模块校验并执行。

class ReduceShopCostEvent final : public Event {
public:
    ReduceShopCostEvent()
        : Event(EventType::ReduceShopCost) {}
    Players target{};  // 效果目标玩家
    SettlementId settlementId{};  // 待结算事务
    CoinValue amount{};  // 减费数
    CoinValue minimum{};  // 卡牌规定的最低价
};  // 效果请求：降低本次商店报价；由对应系统牌与业务模块校验并执行。

class GrantFreeRefreshEvent final : public Event {
public:
    GrantFreeRefreshEvent()
        : Event(EventType::GrantFreeRefresh) {}
    Players target{};  // 效果目标玩家
    ShopId shopId{};  // 商店会话
    int uses{};  // 额度数量
};  // 效果请求：增加本店免费刷新额度；由对应系统牌与业务模块校验并执行。

class RefreshShopEvent final : public Event {
public:
    RefreshShopEvent()
        : Event(EventType::RefreshShop) {}
    Players target{};  // 效果目标玩家
    ShopId shopId{};  // 商店会话
    RefreshMode mode{};  // 付费或免费
};  // 效果请求：执行一次货架刷新；由对应系统牌与业务模块校验并执行，仍检查本店刷新总上限，免费不推进付费价格序号。

class OpenBoosterPackEvent final : public Event {
public:
    OpenBoosterPackEvent()
        : Event(EventType::OpenBoosterPack) {}
    Players target{};  // 开包玩家
    ShopId shopId{};  // 商店会话
    PackId packId{};  // 卡包实例
    SettlementId settlementId{};  // 已验证的购买或授予事务
};  // 效果请求：执行已获准的开包；由对应系统牌与业务模块校验并执行，与购包事务关联，不跨输入等待；完成后发布开包通知并建立选择。

class LockShopSlotEvent final : public Event {
public:
    LockShopSlotEvent()
        : Event(EventType::LockShopSlot) {}
    Players target{};  // 效果目标玩家
    ShopId shopId{};  // 商店会话
    SlotId slotId{};  // 货位
};  // 效果请求：执行货位锁定；由对应系统牌与业务模块校验并执行。

class UnlockShopSlotEvent final : public Event {
public:
    UnlockShopSlotEvent()
        : Event(EventType::UnlockShopSlot) {}
    Players target{};  // 效果目标玩家
    ShopId shopId{};  // 商店会话
    SlotId slotId{};  // 货位
};  // 效果请求：执行货位解锁；由对应系统牌与业务模块校验并执行。

class GrantFunctionCardEvent final : public Event {
public:
    GrantFunctionCardEvent()
        : Event(EventType::GrantFunctionCard) {}
    Players target{};  // 效果目标玩家
    DefinitionId definitionId{};  // 牌种
    CoinValue purchaseCost{};  // 实例出售成本
};  // 效果请求：生成并装备功能卡；由对应系统牌与业务模块校验并执行，检查空槽；免费生成成本为零。

class RemoveFunctionCardEvent final : public Event {
public:
    RemoveFunctionCardEvent()
        : Event(EventType::RemoveFunctionCard) {}
    CardRef card{};  // 功能卡实例
    RemovalReason reason{};  // 移除原因
};  // 效果请求：移除功能卡；由对应系统牌与业务模块校验并执行。

class ReplaceFunctionCardEvent final : public Event {
public:
    ReplaceFunctionCardEvent()
        : Event(EventType::ReplaceFunctionCard) {}
    CardRef card{};  // 功能卡实例
    DefinitionId newDefinitionId{};  // 新牌种
    bool preservePurchaseCost = true;  // 是否沿用出售成本
};  // 效果请求：替换功能卡；由对应系统牌与业务模块校验并执行。

class AddAbilityEvent final : public Event {
public:
    AddAbilityEvent()
        : Event(EventType::AddAbility) {}
    CardRef card{};  // 功能卡实例
    DefinitionId abilityDefinitionId{};  // 技能定义
};  // 效果请求：给功能卡添加技能；由对应系统牌与业务模块校验并执行。

class RemoveAbilityEvent final : public Event {
public:
    RemoveAbilityEvent()
        : Event(EventType::RemoveAbility) {}
    AbilityRef ability{};  // 目标技能
};  // 效果请求：逻辑删除技能；由对应系统牌与业务模块校验并执行。

class AbolishAbilityEvent final : public Event {
public:
    AbolishAbilityEvent()
        : Event(EventType::AbolishAbility) {}
    AbilityRef ability{};  // 目标技能
};  // 效果请求：永久废除技能；由对应系统牌与业务模块校验并执行。

class SetAbilityUsableEvent final : public Event {
public:
    SetAbilityUsableEvent()
        : Event(EventType::SetAbilityUsable) {}
    AbilityRef ability{};  // 目标技能
    bool usable{};  // 新开关
};  // 效果请求：设置技能临时开关；由对应系统牌与业务模块校验并执行。

class SetFunctionCardUsableEvent final : public Event {
public:
    SetFunctionCardUsableEvent()
        : Event(EventType::SetFunctionCardUsable) {}
    CardRef card{};  // 功能卡实例
    bool usable{};  // 新开关
};  // 效果请求：设置整牌临时开关；由对应系统牌与业务模块校验并执行。

class AbolishFunctionCardEvent final : public Event {
public:
    AbolishFunctionCardEvent()
        : Event(EventType::AbolishFunctionCard) {}
    CardRef card{};  // 功能卡实例
};  // 效果请求：永久废除整张牌；由对应系统牌与业务模块校验并执行。

class ApplyCoatingEvent final : public Event {
public:
    ApplyCoatingEvent()
        : Event(EventType::ApplyCoating) {}
    CardRef card{};  // 功能卡实例
    CoatingType coating{};  // 镀层
};  // 效果请求：给功能卡施加镀层；由对应系统牌与业务模块校验并执行。

class DrawCardsEvent final : public Event {
public:
    DrawCardsEvent()
        : Event(EventType::DrawCards) {}
    Players target{};  // 效果目标玩家
    int count{};  // 数量
    DrawReason reason{};  // 摸牌原因
};  // 效果请求：按规则执行非初始摸牌；由对应系统牌与业务模块校验并执行，锁定本局结束后不得借此进行额外摸牌；牌堆不足按规则处理。

class SwapCardsWithDeckEvent final : public Event {
public:
    SwapCardsWithDeckEvent()
        : Event(EventType::SwapCardsWithDeck) {}
    Players target{};  // 效果目标玩家
    std::vector<CardId> cards{};  // 实际扑克牌实例
};  // 效果请求：将指定手牌换到牌堆底并等量摸顶；由对应系统牌与业务模块校验并执行，保持原牌身份与公共牌组总数。

class ChangeCardRankEvent final : public Event {
public:
    ChangeCardRankEvent()
        : Event(EventType::ChangeCardRank) {}
    CardId cardId{};  // 扑克牌实例
    int rank{};  // 目标点数
    Lifetime lifetime{};  // 持续范围
};  // 效果请求：临时改变普通牌点数；由对应系统牌与业务模块校验并执行。

class ChangeCardSuitEvent final : public Event {
public:
    ChangeCardSuitEvent()
        : Event(EventType::ChangeCardSuit) {}
    CardId cardId{};  // 扑克牌实例
    Suit suit{};  // 目标花色
    Lifetime lifetime{};  // 持续范围
};  // 效果请求：临时改变花色；由对应系统牌与业务模块校验并执行。

class MakeCardJokerEvent final : public Event {
public:
    MakeCardJokerEvent()
        : Event(EventType::MakeCardJoker) {}
    CardId cardId{};  // 扑克牌实例
    Lifetime lifetime{};  // 持续范围
};  // 效果请求：将指定牌临时变为王；由对应系统牌与业务模块校验并执行。

class RevealCardsEvent final : public Event {
public:
    RevealCardsEvent()
        : Event(EventType::RevealCards) {}
    Players viewer{};  // 查看者
    Players target{};  // 被查看玩家
    std::vector<CardId> cards{};  // 实际扑克牌实例
};  // 效果请求：向指定玩家展示合法信息；由对应系统牌与业务模块校验并执行，只向有权限的客户端同步牌面。

class ApplyPlayerStatusEvent final : public Event {
public:
    ApplyPlayerStatusEvent()
        : Event(EventType::ApplyPlayerStatus) {}
    Players target{};  // 效果目标玩家
    DefinitionId statusDefinitionId{};  // 状态定义
    int stacks = 1;  // 层数
    Lifetime lifetime{};  // 期限
};  // 效果请求：施加玩家状态；由对应系统牌与业务模块校验并执行。

class RemovePlayerStatusEvent final : public Event {
public:
    RemovePlayerStatusEvent()
        : Event(EventType::RemovePlayerStatus) {}
    Players target{};  // 效果目标玩家
    StatusId statusId{};  // 状态实例
};  // 效果请求：移除玩家状态；由对应系统牌与业务模块校验并执行。

class UpgradeHandPatternEvent final : public Event {
public:
    UpgradeHandPatternEvent()
        : Event(EventType::UpgradeHandPattern) {}
    Players target{};  // 效果目标玩家
    HandPattern pattern{};  // 牌型
    int levels = 1;  // 加强级数
};  // 效果请求：执行牌型加强；由对应系统牌与业务模块校验并执行。

class PurchaseFacilityEvent final : public Event {
public:
    PurchaseFacilityEvent()
        : Event(EventType::PurchaseFacility) {}
    Players target{};  // 效果目标玩家
    DefinitionId facilityId{};  // 设施
};  // 效果请求：执行设施购买与生效；由对应系统牌与业务模块校验并执行。

class UseExpansionCardEvent final : public Event {
public:
    UseExpansionCardEvent()
        : Event(EventType::UseExpansionCard) {}
    Players target{};  // 效果目标玩家
    DefinitionId expansionId{};  // 拓展牌
    TargetSpec selection{};  // 目标与方案
};  // 效果请求：执行获得即使用的拓展牌；由对应系统牌与业务模块校验并执行。

class AddDelayedEffectEvent final : public Event {
public:
    AddDelayedEffectEvent()
        : Event(EventType::AddDelayedEffect) {}
    Players target{};  // 效果目标玩家
    DefinitionId effectDefinitionId{};  // 延迟效果定义
    EventType triggerType{};  // 未来触发的通知类型
    TargetSpec selection{};  // 目标与方案
    Lifetime lifetime{};  // 有效期
};  // 效果请求：建立延迟效果标记；由对应系统牌与业务模块校验并执行。

class OpenChoiceEvent final : public Event {
public:
    OpenChoiceEvent()
        : Event(EventType::OpenChoice) {}
    Players target{};  // 效果目标玩家
    ChoiceKind choiceKind{};  // 选择类型
    std::vector<ChoiceOptionId> options{};  // 合法选项
    std::uint32_t timeoutMs{};  // 超时长度
    bool optional{};  // 是否可放弃
};  // 效果请求：建立选择窗口并挂起相关流程；由对应系统牌与业务模块校验并执行。

class ResolveChoiceEvent final : public Event {
public:
    ResolveChoiceEvent()
        : Event(EventType::ResolveChoice) {}
    ChoiceId choiceId{};  // 待处理交互
    std::vector<ChoiceOptionId> options{};  // 实际选项
    ChoiceReason reason{};  // 手动、超时或掉线
};  // 效果请求：执行选择或默认方案；由对应系统牌与业务模块校验并执行。

class ChangeAbilityCounterEvent final : public Event {
public:
    ChangeAbilityCounterEvent()
        : Event(EventType::ChangeAbilityCounter) {}
    AbilityRef ability{};  // 目标技能
    CounterId counterId{};  // 计数项
    CounterOperation operation{};  // 加值或设值
    int value{};  // 操作值
};  // 效果请求：改变已定义的技能成长或蓄力计数；由对应系统牌与业务模块校验并执行。

class EventBatch final : public Event {
public:
    EventBatch()
        : Event(EventType::EventBatch) {}
    std::vector<EventPtr> events{};  // 有序子效果
};  // 效果请求：有序原子组合多个效果；由对应系统牌与业务模块校验并执行，只允许 Effect 子事件；先校验后提交，不能跨玩家输入等待。

}  // namespace cardgame
```

#### 2.7.4 控制事件（10 个）

| 事件派生类 | 含义 | 专属参数 | 时机或执行约束 |
|---|---|---|---|
| `PlayerJoinedControl` | 实际玩家加入房间 | `player: Players` | 连接或房间模块发出；满员后自动开始，无额外准备步骤 |
| `PlayerDisconnectedControl` | 连接断开 | `player: Players`；`connectionVersion: std::uint64_t` | 按当前状态和来源校验 |
| `PlayerReconnectedControl` | 恢复原座位连接 | `player: Players`；`connectionVersion: std::uint64_t` | 按当前状态和来源校验 |
| `ShoppingDeadlineControl` | 商店计时到期 | `shopId: ShopId`；`expectedVersion: std::uint64_t` | 按当前状态和来源校验 |
| `TurnDeadlineControl` | 出牌计时到期 | `turnId: TurnId`；`player: Players`；`expectedVersion: std::uint64_t` | 按当前状态和来源校验 |
| `ChoiceDeadlineControl` | 目标或赖子交互计时到期 | `choiceId: ChoiceId`；`expectedVersion: std::uint64_t` | 按当前状态和来源校验 |
| `RequestRejectedControl` | 操作未被接受 | `requestId: RequestId`；`reason: std::string`；`currentVersion: std::uint64_t` | 不作为卡牌可获利的成功行为广播 |
| `StateSnapshotReadyControl` | 为指定客户端准备同步结果 | `player: Players`；`snapshotId: SnapshotId` | 按当前状态和来源校验 |
| `PlayOptionsReadyControl` | 选牌或赖子的合法方案已计算 | `player: Players`；`previewId: PreviewId`；`options: std::vector<ChoiceOptionId>` | 纯预览不表示已经进入出牌成功阶段 |
| `ShopQuoteReadyControl` | 商店报价结果已计算 | `player: Players`；`previewId: PreviewId`；`quote: ShopData` | 按当前状态和来源校验 |

```cpp
namespace cardgame {

class PlayerJoinedControl final : public Event {
public:
    PlayerJoinedControl()
        : Event(EventType::PlayerJoinedControl) {}
    Players player{};  // 相关玩家
};  // 控制事件：玩家加入房间，由调度器更新房间流程，满足满员条件后自动开局。

class PlayerDisconnectedControl final : public Event {
public:
    PlayerDisconnectedControl()
        : Event(EventType::PlayerDisconnectedControl) {}
    Players player{};  // 相关玩家
    std::uint64_t connectionVersion{};  // 连接代次
};  // 控制事件：连接断开，保留原座位与资源，按对应超时规则继续处理。

class PlayerReconnectedControl final : public Event {
public:
    PlayerReconnectedControl()
        : Event(EventType::PlayerReconnectedControl) {}
    Players player{};  // 相关玩家
    std::uint64_t connectionVersion{};  // 新连接代次
};  // 控制事件：原玩家重连，恢复原座位并同步有权查看的当前状态。

class ShoppingDeadlineControl final : public Event {
public:
    ShoppingDeadlineControl()
        : Event(EventType::ShoppingDeadlineControl) {}
    ShopId shopId{};  // 商店会话
    std::uint64_t expectedVersion{};  // 对应商店版本
};  // 控制事件：商店计时到期，由调度器检查商店会话与版本后执行结束流程。

class TurnDeadlineControl final : public Event {
public:
    TurnDeadlineControl()
        : Event(EventType::TurnDeadlineControl) {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
    std::uint64_t expectedVersion{};  // 对应回合版本
};  // 控制事件：出牌计时到期，由调度器检查回合与版本后执行既定超时操作。

class ChoiceDeadlineControl final : public Event {
public:
    ChoiceDeadlineControl()
        : Event(EventType::ChoiceDeadlineControl) {}
    ChoiceId choiceId{};  // 待处理交互
    std::uint64_t expectedVersion{};  // 对应交互版本
};  // 控制事件：选择计时到期，由调度器检查交互与版本后执行默认方案。

class RequestRejectedControl final : public Event {
public:
    RequestRejectedControl()
        : Event(EventType::RequestRejectedControl) {}
    RequestId requestId{};  // 被拒绝请求
    std::string reason{};  // 拒绝原因
    std::uint64_t currentVersion{};  // 当前版本
};  // 控制反馈：操作请求未获接受，只反馈原因，不视为成功行为或成功类收益来源。

class StateSnapshotReadyControl final : public Event {
public:
    StateSnapshotReadyControl()
        : Event(EventType::StateSnapshotReadyControl) {}
    Players player{};  // 相关玩家
    SnapshotId snapshotId{};  // 按权限过滤的快照
};  // 控制反馈：同步快照已经准备，只向指定玩家发送其有权查看的信息。

class PlayOptionsReadyControl final : public Event {
public:
    PlayOptionsReadyControl()
        : Event(EventType::PlayOptionsReadyControl) {}
    Players player{};  // 相关玩家
    PreviewId previewId{};  // 预览结果
    std::vector<ChoiceOptionId> options{};  // 合法方案
};  // 查询反馈：合法出牌或赖子方案已经计算，表示预览结果，尚未成功出牌。

class ShopQuoteReadyControl final : public Event {
public:
    ShopQuoteReadyControl()
        : Event(EventType::ShopQuoteReadyControl) {}
    Players player{};  // 相关玩家
    PreviewId previewId{};  // 预览结果
    ShopData quote{};  // 当前报价
};  // 查询反馈：商店报价已经计算，本身不扣款或消耗免费刷新额度。

}  // namespace cardgame
```

#### 2.7.5 参数、来源与重复触发约定

- `Request` 表示意图，实际成功后才发布事实通知。例如 `PlayCardsRequest` 通过后发布 `CardsPlayedEvent`，失败只返回 `RequestRejectedControl`。
- 一次出牌只发布一次成功出牌通知，自己、他人和任意玩家由处理器相对 `Context.owner()` 判断。购买、金币变化和装备完成可以是不同事实，但不能把同一事实重复发布。
- `ShopAbilityActivationEvent` 与 `TurnAbilityActivationEvent` 分别使用原窗口时机，只表示一次已验证的主动调用；窗口保持开放时不会反复产生通知。
- `PlayingCardsChangedEvent` 使用原因和实际牌 ID 区分换牌、改点数、改花色或临时变王；查看信息使用 `CardVisibilityChangedEvent`，不冒充物理牌面变化。
- EventBatch 保留为组合请求数据结构；当前技能组合通过 CardEffects 连续缓冲结果。若业务模块要执行 EventBatch，应注册对应系统牌并实现其事务语义，调度器不会自动假定任意卡牌组合可回滚。
- `QuoteShopOperationRequest` 和 `PreviewPlayRequest` 使用 Preview 模式。允许临时报价或合法性结果，禁止扣费、消耗额度、发放实际奖励、推进持久随机序列和发布技能成功通知。
- 分数和金币的“待结算修正”与“实际余额变化”分开；`AddPendingCoinsEvent` 指定基础或额外奖励，保证 E17 的扣款只作用于规则规定的部分。
- `Effect` 中的目标玩家与 `Event.actor`、当前处理器持有者分别记录。涉及私有牌面的通知按查看权限同步。
- 所有 Request、效果和通知仍需满足房间当前状态、牌权、结束锁定和既有游戏规则；事件类存在不等于对应操作已经获准。续玩的一致性流程、事务实现和各类效果落地调用复用既有业务模块，具体接线后续完善。

## 3. 技能事件处理

abilities 是按操作提供不同参数的普通函数库，没有统一 Ability 基类、call_ability 或 handle_event 技能入口。调用者先根据 Context 准备玩家、数值、对象和目标，再调用对应原子函数。FunctionCards::resolve 是卡牌级组合入口，HandleEvent 是卡牌调度器。

### 3.1 时机与行为主体分别记录

Register 的 EventMask 只记录“可能响应哪些类型”。卡牌 resolve 继续判断主体、目标、次数、费用与具体条件。响应自己可比较事件中的 player 或 actor 与 context.owner()；来源、接收者和效果目标允许不同。全局事件可没有 actor。

同一种原子操作可用于主动或被动功能。主动调用使用 ShopAbilityActivationEvent / TurnAbilityActivationEvent，HandleEvent 只派发给指定 CardRef；牌内根据 ability.seq 决定调用哪个具体函数。普通被动通知按登记的事件类型响应。

### 3.2 第一版时机表

区分整场游戏（Game）、一局（Round）、一巡（Trick）、一名玩家的一次行动回合（Turn）。EventType 编号直接作为技能订阅布尔集合的下标，掩码总长度为 136。下表列出原有 0～62 号业务通知的触发边界；63～83 为操作请求，84～121 为效果，122～131 为控制事件，完整类型名见上面的事件枚举与清单。旧掩码扩展时新增位为 false。Register 与 HandleEvent 决定响应卡牌，`Count` 仅用于确定掩码长度。

| 下标 | 名称 | 精确边界与用途 |
|---|---|---|
| 0 | `GameStart` | 整场资源与座位初始化完成，首局商店开放前 |
| 1 | `ShopEnter` | 玩家进入局前商店，商店次数重置后、首次生成货架前 |
| 2 | `ShopActionWindow` | 商店可操作窗口；主动技能允许使用，不是循环发布的被动事件 |
| 3 | `ShopRefreshQuote` | 刷新报价时，扣款前计算减费或免费额度 |
| 4 | `ShopRefreshAfter` | 刷新校验通过、扣款与货架替换均成功后 |
| 5 | `ShopBuyQuote` | 购买报价时，扣款和交付商品前 |
| 6 | `ShopBuyAfter` | 购买成功，扣款与交付商品完成后 |
| 7 | `ShopSellQuote` | 出售报价时，移除卡牌和发放金币前 |
| 8 | `ShopSellAfter` | 出售成功，卡牌移除、金币到账后 |
| 9 | `ShopLeave` | 玩家确认完成或商店超时，关闭主动窗口前；全部离店后才能开局 |
| 10 | `RoundStart` | 所有玩家完成商店，重置本局计次、确定先手后，初始发牌前 |
| 11 | `InitialDealBefore` | 初始发牌方案确定前，用于修改发牌方案 |
| 12 | `InitialDealAfter` | 所有玩家的初始手牌发完后；“下一局发牌后”标记在此处理 |
| 13 | `TrickStart` | 本巡先手与牌权状态确定后，第一名玩家行动前 |
| 14 | `TurnStart` | 轮到当前玩家，开放其操作窗口前 |
| 15 | `PlayActionWindow` | 当前玩家出牌或不出牌前的主动窗口；发动技能不自动结束回合 |
| 16 | `PlayValidate` | 校验选牌、赖子指定和跟牌是否合法时；提供规则修正，尚未提交出牌 |
| 17 | `PlayCommitted` | 合法的一手牌已从手牌移到桌面，牌权跟踪已更新，出牌奖励尚未结算 |
| 18 | `PlayRewardBefore` | 最终牌型已确定，计算并修改本手分数与金币，到账前 |
| 19 | `PlayRewardAfter` | 本手出牌奖励已到账，且此前派生效果处理完毕后 |
| 20 | `PassCommitted` | 合法不出牌已确认；上下文区分主动选择、超时、掉线原因 |
| 21 | `TurnEnd` | 本回合出牌或不出牌及其派生效果全部结算后 |
| 22 | `TrickEnd` | 其余玩家均不出牌、最近出牌者重新获得牌权，巡末摸牌前 |
| 23 | `DrawBefore` | 某玩家即将实际执行非初始摸牌，调整此次摸牌方案前 |
| 24 | `DrawAfter` | 某玩家此次非初始摸牌实际完成后，包含实际牌和数量 |
| 25 | `TrickDrawsAfter` | 全体玩家按逆时针完成本巡摸牌步骤后；牌堆空而全部跳过时也发生一次 |
| 26 | `RoundEnd` | 本局结束已锁定，最后一个回合结算完毕，局末奖励计算前 |
| 27 | `RoundRewardBefore` | 计算局末奖励，到账前；按玩家分别提供待结算金额 |
| 28 | `RoundRewardAfter` | 全体玩家局末奖励及借款扣款处理完毕，资源汇总后 |
| 29 | `GameEnd` | 规定局数完成，所有局末效果处理完毕，确认累计分数与冠军时 |
| 30 | `CardChanged` | 牌面或手牌集合被技能实际改变后；记录变化前后、原因和来源 |
| 31 | `AbilityResolved` | 某技能成功结算后；用于镀层等监听，不是一次新的出牌 |
| 32 | `FunctionCardEquipped` | 功能牌进入槽位后，更新持续效果来源 |
| 33 | `FunctionCardRemoved` | 功能牌离开槽位时，撤销其持续效果来源；保留移除原因与来源快照 |
| 34 | `ShopSlotLocked` | 货位锁定成功，由服务端确认后发布 |
| 35 | `ShopSlotUnlocked` | 货位解锁成功，由服务端确认后发布 |
| 36 | `BoosterPackOpened` | 卡包已付费打开，由服务端确认后发布 |
| 37 | `BoosterPackChoiceCompleted` | 卡包候选选取完成，由服务端确认后发布 |
| 38 | `BoosterPackSkipped` | 卡包已放弃，由服务端确认后发布 |
| 39 | `ExpansionCardUsed` | 拓展牌获得即使用完成，由服务端确认后发布 |
| 40 | `HandPatternUpgraded` | 牌型加强完成，由服务端确认后发布 |
| 41 | `FacilityPurchased` | 设施购买生效，由服务端确认后发布 |
| 42 | `CoinsChanged` | 金币余额实际变化，由服务端确认后发布 |
| 43 | `ScoreChanged` | 累计分数实际变化，由服务端确认后发布 |
| 44 | `PlayerStatusChanged` | 玩家状态实际变化，由服务端确认后发布 |
| 45 | `AbilityAdded` | 技能添加完成，由服务端确认后发布 |
| 46 | `AbilityRemoved` | 技能逻辑移除完成，由服务端确认后发布 |
| 47 | `AbilityAbolished` | 技能被永久废除，由服务端确认后发布 |
| 48 | `AbilityUsabilityChanged` | 技能临时可用开关变化，由服务端确认后发布 |
| 49 | `ChoiceOpened` | 交互选择窗口建立，由服务端确认后发布 |
| 50 | `ChoiceResolved` | 交互选择处理完成，由服务端确认后发布 |
| 51 | `ChoiceExpired` | 交互期限已到并完成默认处理，由服务端确认后发布 |
| 52 | `RoundWinnerLocked` | 出空时锁定赢家与手牌快照，由服务端确认后发布 |
| 53 | `DrawPileExhausted` | 牌堆由非空变为空，由服务端确认后发布 |
| 54 | `AllShopsClosed` | 全体商店会话关闭且交易结清，由服务端确认后发布 |
| 55 | `FunctionCardUsabilityChanged` | 整张功能卡临时开关变化，由服务端确认后发布 |
| 56 | `FunctionCardAbolished` | 整张功能卡被废除，由服务端确认后发布 |
| 57 | `AbilityStateChanged` | 技能成长或蓄力状态变化，由服务端确认后发布 |
| 58 | `DelayedEffectScheduled` | 延迟效果标记建立，由服务端确认后发布 |
| 59 | `DelayedEffectTriggered` | 延迟标记实际执行完成，由服务端确认后发布 |
| 60 | `DelayedEffectExpired` | 延迟标记结束或失效，由服务端确认后发布 |
| 61 | `CoatingApplied` | 镀层施加完成，由服务端确认后发布 |
| 62 | `CardVisibilityChanged` | 查看权限或公开信息变化，由服务端确认后发布 |

商店开包、使用拓展牌、牌型加强与设施购买使用购买上下文中的商品类型和后续效果信息区分。未来出现必须单独监听的新业务边界时追加时机，不用“购买后”冒充“使用后”；当前没有为这些行为自动增加收益。

**一次事件与一个操作窗口不同。** `RoundStart` 等被动事件每次边界只派发一次；`ShopActionWindow`、`PlayActionWindow` 表示允许主动操作的持续区间，不通过反复派发事件执行被动技能。被动牌若需要在窗口开始时执行，分别订阅 `ShopEnter` 或 `TurnStart`。

**报价与校验可以重复进行，必须没有持久副作用。** `ShopRefreshQuote` 等只修改本次报价，`PlayValidate` 只提供合法性修正；UI 预览或失败请求不扣费、不扣次数、不重复发奖。一次性免费额度、使用次数和消耗仅在对应操作成功提交时确认，持久副作用放在成功后的事件中。

**持续效果不等于每帧执行技能。** 折扣、可用牌型、手牌上限等由引擎在报价或校验时查询有效技能；如果使用缓存，装备、移除、失效与恢复时必须更新缓存。常驻规则仍属于被动技能。

### 3.3 结算顺序与边界

每张功能牌内的原子操作按牌代码顺序执行，然后处理这张牌的后续事件。系统牌可以配置 BeforePlayers 或 AfterPlayers，玩家牌按当前事件 actor 起点及房间逆时针顺序、槽位从左到右结算。全局事件使用调度器配置的本局先手顺序。

一次成功出牌先提交手牌并在出空时锁定赢家和剩牌快照；完成出牌触发、奖励、奖励后触发与 TurnEnd 后才进入 RoundEnd。锁定结束后不进行巡末摸牌。局末奖励按各玩家独立结算，全部成功发放后才产生 RoundRewardsGranted 并开放下一局商店或结束整场。

### 3.4 主动、被动和状态

FunctionCards 保存 available 和 usable：废除使 available 永久失效，临时开关只改变 usable。候选调用前重新检查二者。原子函数不保存这些卡牌规则；冷却、条件计数和主动技能选择由对应牌代码或业务模块保存。

Preview 模式只执行显式声明 preview_safe 的卡；此类牌应只修改工作快照或计算只读结果。默认卡、资源系统牌和发牌系统牌不在预览中执行实际资源操作。服务器只将已识别的查询请求标记为 Preview，查询不推进游戏随机序列或权威状态版本。

### 3.5 C++17 接口草案

当前共 62 个公开原子函数。加分与加金币、支付与取得资源、添加与移除状态分别提供；转账、整张牌替换、扩展牌的组合效果由卡牌或业务模块分别调用所需函数，不增加复合 Ability。

直接实现的数值、卡槽与卡状态操作先检查溢出、余额和范围再修改。modify 类函数只修改指定值；无变化返回空事件或 false。multiply_pending_score 使用整数有理数及明确取整，Nearest 的中点向远离零方向取整；中间乘积超出范围则拒绝。

带 Operation 参数的函数调用既有业务对象：回调签名为 bool(该函数前面的业务参数..., 对应事实事件&)。回调负责一次规则事务、填写真实结果，false 表示未提交，true 才返回事实通知；抛异常时由调用者处理。函数在调用回调前创建事实对象。它们可直接适配现有商店、状态、交互与延迟模块，仓库不伪造这些模块的业务成功。

初始发牌接收 RoundId、PlayerOrder、每人数和 InitialDealOperation，整批修改共享牌堆与全体手牌后只发布 InitialCardsDealt。draw_to_all 一次提交整批摸牌，按 PlayerOrder 为真正摸到牌的玩家产生 CardsDrawn；空牌堆不伪造成功摸牌通知。

完整接口清单：

- `EventPtr add_score(Player& player, ScoreValue amount)`
- `EventPtr subtract_score(Player& player, ScoreValue amount)`
- `EventPtr set_score(Player& player, ScoreValue value)`
- `EventPtr gain_coins(Player& player, CoinValue amount, std::string reason = {})`
- `EventPtr spend_coins(Player& player, CoinValue amount, std::string reason = {})`
- `EventPtr set_coins(Player& player, CoinValue value, std::string reason = {})`
- `bool add_pending_score(RewardData& reward, ScoreValue delta)`
- `bool multiply_pending_score(RewardData& reward, ScoreValue numerator, ScoreValue denominator, RoundingRule rounding = RoundingRule::Floor)`
- `bool add_pending_coins(CoinValue& component, CoinValue delta)`
- `bool reduce_shop_cost(ShopData& quote, CoinValue amount, CoinValue minimum = 0)`
- `bool grant_free_refresh(std::size_t& credits, std::size_t count)`
- `bool grant_function_slot(Player& player)`
- `EventPtr add_ability_counter(AbilityRef ability, CounterId counter, int& value, int delta)`
- `EventPtr set_ability_counter(AbilityRef ability, CounterId counter, int& value, int replacement)`
- `EventPtr set_ability_usable(AbilityRef ability, bool& usable, bool replacement)`
- `EventPtr set_function_card_usable(FunctionCards& card, bool usable)`
- `EventPtr abolish_function_card(FunctionCards& card)`
- `EventPtr equip_function_card(Register& registry, Player& player, std::size_t slot, CardPtr card)`
- `EventPtr remove_function_card(Register& registry, Players player, std::size_t slot, RemovalReason reason)`
- `EventPtr deal_initial_cards(RoundId round, const PlayerOrder& players, int count, const InitialDealOperation& deal)`
- `EventPtr refresh_shop(Players player, ShopId shop, RefreshMode mode, Operation&& operation)`
- `EventPtr open_booster_pack(Players player, ShopId shop, PackId pack, Operation&& operation)`
- `EventPtr lock_shop_slot(Players player, ShopId shop, SlotId slot, Operation&& operation)`
- `EventPtr unlock_shop_slot(Players player, ShopId shop, SlotId slot, Operation&& operation)`
- `EventPtr add_ability(CardRef card, DefinitionId definition, Operation&& operation)`
- `EventPtr remove_ability(AbilityRef ability, Operation&& operation)`
- `EventPtr abolish_ability(AbilityRef ability, Operation&& operation)`
- `EventPtr apply_coating(CardRef card, CoatingType coating, Operation&& operation)`
- `EventPtr draw_cards(Players player, std::size_t count, DrawReason reason, Operation&& operation)`
- `EventPtr swap_cards_with_deck(Players player, const std::vector<CardId>& cards, Operation&& operation)`
- `EventPtr change_card_rank(CardId card, int rank, Lifetime lifetime, Operation&& operation)`
- `EventPtr change_card_suit(CardId card, Suit suit, Lifetime lifetime, Operation&& operation)`
- `EventPtr make_card_joker(CardId card, Lifetime lifetime, Operation&& operation)`
- `EventPtr clear_card_joker(CardId card, Operation&& operation)`
- `EventPtr reveal_cards(Players viewer, Players target, const std::vector<CardId>& cards, Operation&& operation)`
- `EventPtr apply_player_status(Players player, DefinitionId definition, int stacks, Lifetime lifetime, Operation&& operation)`
- `EventPtr remove_player_status(Players player, StatusId status, Operation&& operation)`
- `EventPtr add_status_stacks(Players player, StatusId status, int delta, Operation&& operation)`
- `EventPtr upgrade_hand_pattern(Players player, HandPattern pattern, int levels, Operation&& operation)`
- `EventPtr purchase_facility(Players player, DefinitionId facility, Operation&& operation)`
- `EventPtr schedule_delayed_effect(Players player, DefinitionId definition, EventType trigger, const TargetSpec& selection, Lifetime lifetime, Operation&& operation)`
- `EventPtr trigger_delayed_effect(Players player, DelayedEffectId effect, Operation&& operation)`
- `EventPtr cancel_delayed_effect(Players player, DelayedEffectId effect, Operation&& operation)`
- `EventPtr open_choice(Players player, ChoiceKind kind, const std::vector<ChoiceOptionId>& options, std::uint32_t timeoutMs, Operation&& operation)`
- `EventPtr resolve_choice(ChoiceId choice, const std::vector<ChoiceOptionId>& options, ChoiceReason reason, Operation&& operation)`
- `EventPtr expire_choice(ChoiceId choice, Operation&& operation)`
- `EventPtr begin_round(RoundId round, Players first, Operation&& operation)`
- `EventPtr begin_trick(RoundId round, TrickId trick, Players leader, Operation&& operation)`
- `EventPtr begin_turn(TurnId turn, Players player, Operation&& operation)`
- `EventPtr end_turn(TurnId turn, Players player, Operation&& operation)`
- `EventPtr finish_trick(TrickId trick, Players leader, Operation&& operation)`
- `EventPtr lock_round_winner(RoundId round, Players winner, Operation&& operation)`
- `EventPtr finish_round(RoundId round, RoundResultId result, Operation&& operation)`
- `EventPtr finish_game(Operation&& operation)`
- `EventPtr finish_shopping(Players player, ShopId shop, Operation&& operation)`
- `EventPtr close_all_shops(RoundId round, Operation&& operation)`
- `EventPtr commit_play(Players player, TurnId turn, const std::vector<CardId>& cards, const std::vector<WildcardAssignment>& wildcards, Operation&& operation)`
- `EventPtr commit_pass(Players player, TurnId turn, PassReason reason, Operation&& operation)`
- `EventList draw_to_all(const PlayerOrder& players, std::size_t count, DrawReason reason, Operation&& operation)`

### 3.6 Context 与派发流程

HandleEvent 为当前事件冻结候选，暂存父 ContextFrame，逐张恢复同一个 Context 并设置 receiver。CardEffects 保存本张牌的工作快照和 FIFO 通知队列；卡牌结束后更新快照，再处理子事件。Context 对象不移动、不复制，父事件的唯一所有权随帧暂存和恢复。

子事件自动填写新的 eventId、parentEventId、rootEventId，以及 originAbility 或 originSystemCard。调用方可为每次 emit 指定稳定技能序号，以记录一张牌中哪个原子操作产生了通知。系统牌使用定义编号，不冒用某个玩家身份。

普通后续通知默认继承发起者；全局的开局、局末、巡末等通知显式使用 emit 的 inheritActor=false，避免把最后操作的玩家误认为系统操作的发起者。

### 3.7 事件处理示例

```cpp
#include "SystemCards.hpp"
namespace cardgame::examples {
CardPtr make_two_operation_card(Player& player, std::size_t sequence) {
    return std::make_shared<CallbackFunctionCard>(CardRef{player.id, sequence}, 5001,
        "Score and coins example", FunctionCards::listens_to({EventType::PlayCommitted}),
        [&player](const Context& context, CardEffects& effects) {
            const auto* played = context.event_as<CardsPlayedEvent>();
            if (!played || played->player != player.id) return;
            effects.emit(abilities::add_score(player, 2), 1);
            effects.emit(abilities::gain_coins(player, 1, "card"), 2);
        });
}
} // namespace cardgame::examples
```

当得分通知的监听牌开始执行时，这张示例牌的两个原子操作都已完成；同级通知仍按得分、金币顺序派发。CardPipeline.test.cpp 验证了该顺序及父事件恢复。

## 4. 功能卡

### 4.1 功能卡与技能的关系

FunctionCards 是玩家牌与系统牌共同的抽象基类。只在卡牌层提供 resolve(const Context&, CardEffects&)；牌内按需要调用任意签名的原子函数，不存放统一 Ability 指针数组。新增牌可以派生该类，也可以使用 CallbackFunctionCard 组合回调。

### 4.2 数据成员与卡牌元信息

| 数据 | 含义 |
|---|---|
| optional<CardRef> reference | 玩家牌的 owner 和稳定 seq；系统牌为空 |
| definition / name | 牌种与可读名称 |
| EventMask subscriptions | 所有潜在响应事件，原始订阅不变；强化增加订阅时同步注册表 |
| available / usable | 永久有效与临时可执行开关 |
| SystemOrder | 系统牌前置或后置，玩家牌按槽位顺序 |
| preview_safe | 是否允许执行只读预览，默认 false |

Player 保存座位、名字、金币、分数、真实手牌 ID、连接状态、功能槽位上限、五类加强次数、设施和本局基础出牌收益。初始金币 6、分数 0、功能槽 4，最高 6；Register 保留每人 6 行物理位置，Scheduler 根据实际解锁数量限制购买。

### 4.3 技能管理与生命周期

Register 初始 24 行，row = playerIndex × 6 + slot，slot 为 0～5。系统牌追加到同一矩阵，列数始终为 136，所以行数为 24 + 系统行容量。系统牌卸下后保留空行稳定其他行号，后续可重用。

每行使用 bitset<TOTAL_EVENT_TYPES>，矩阵为 vector<EventMask>。装备时复制全部潜在触发位；移槽移动该行，卸下清空，替换刷新。matching 根据当前事件的列筛选，并检查牌的有效和可用状态。

公开接口包括 equip、replace、unequip、move、add_system、remove_system、at、player_cards、matrix、matching 和 current。Register 拒绝错误归属、重复装备、复用不同对象的历史 CardRef、超过第六槽等输入。候选包含共享所有权和装备世代；卸下重装或替换不会让旧候选误调用新牌，结算中新增的牌不插入当前冻结列表。

系统牌的 visible_to_players 为 false，player_cards 与网络玩家卡槽列表均不会包含它们。服务器默认注册资源系统牌 900001、流程系统牌 900003、发牌系统牌 900002，矩阵初始为 27×136。

### 4.4 C++17 接口草案

玩家牌构造需要 CardRef、definition、name、EventMask；系统牌构造需要 definition、name、EventMask 和可选 SystemOrder。CallbackFunctionCard 再接收一个 CardHandler。ResourceSystemCard 将 AddScore / AddCoins / SpendCoins 等请求转换为已提交事实；InitialDealSystemCard 响应 InitialDealBefore，在玩家的发牌前效果完成后调用整批发牌操作。

新正式卡池可通过 SchedulerConfig::catalog 注册 id、name、price、FunctionCardFactory(Player&, sequence)。工厂返回一张元数据一致的卡，商店按定义生成；不需要修改 HandleEvent 或 Register。未配置时使用明确命名为 Demo card 的演示卡池，演示价格与效果不代表正式平衡方案。

### 4.5 逐张功能卡的说明模板

每张牌应明确：定义编号与名称；潜在触发事件；主体与目标条件；牌内原子函数及先后顺序；主动技能序号；费用、次数和重置点；后续通知；预览是否安全；是否系统牌及前后置顺序。系统牌同样遵守这一模板，只是不进入玩家卡槽和商店。

## 5. 扑克牌 Poker

Poker 保存稳定 CardId、isJoker、可选 suit / rank、multiplier 和 baseValue。标准公共牌组包含 52 张普通牌与 2 张赖子；A 为 14，2 为 2，比较单张／对子／炸弹时将 2 视为 15，顺子分别尝试 A=1 与 A=14，不循环。

legal_moves 是纯计算函数，接受选中牌、可选上一手与可选赖子指定，返回 MoveOption 列表，不修改资源或推进随机数。它实现同型同张数只接下一档、单张／对 2 特殊规则、炸弹先比张数再比点数、普通顺子与同花顺同等压牌。赖子方案由玩家选择；待选超时从合法方案均匀随机。合法性与奖励都使用最终选择的同一方案。

## 6. 商店、回合与跨玩家效果

Scheduler 输入按 P1～P4 排列的四个 Player，注册所有系统牌后自动进入首局商店。每房间维护一个 Condition、Register 和 HandleEvent；客户端请求由服务器会话提供 actor。

每个玩家有独立 ShopState。已接入演示功能牌购买、出售、刷新、锁定、结束购物、五种牌型加强与 U01/U02/U03/U04/U05/U07/U08。付款前的报价事件先允许玩家功能牌修改工作报价，再由后置系统牌重新校验余额、商品和空槽。刷新计入五次总上限，免费额度不推进付费序号，锁定一位，已买空位不会被刷新补回。U06、卡包、E 系列效果需要额外业务适配，未注册的外部请求会明确拒绝。

所有商店关闭后初始化共享牌堆并洗牌，系统牌整批发 7 张。回合沿配置的逆时针座位序进行；其余玩家都不出牌，行动回到最近出牌者即结束一巡，从该玩家起每人依序摸一张，牌堆耗尽不补充。只接受当前 turnId 和当前行动者的出牌／不出牌；乱序请求不会预存到未来回合。

本局先手按首局随机起点的 1,2,3,4,4,3,2,1 循环。最后一手立即锁定赢家和剩牌快照，完成出牌奖励与回合末效果后发放局末奖励，之后进入下一局商店；第八局后比较累计分数。加赛三局或无尽模式需要四人选择一致；新游戏也需要四人一致请求，重置资源并保留连接身份。

出牌基础分为 1/3/5/8/8，金币为 1/2/3/5/5；分数乘该牌型的加强系数。第 r 局每人局末金币 8+5(r-1)，局末分数 max(0,16+5(r-1)-锁定剩牌数)。各玩家出牌收益只汇总、不重复到账；所有局末发放成功后才发布统一到账通知。

服务器使用实际 60 秒商店、30 秒行动与选择计时，可在启动参数中调整。跟牌超时自动不出，领出超时打出最小普通单张；仅剩赖子时采用最小单张指定。已经进入方案选择则均匀随机选择。掉线保留座位和资源并使用这些超时规则，不运行机器人策略；重连恢复原状态，已完成的操作不回滚。

## 7. 事件监听与输入入口

已实现的监听类为 cardgame::network::ListenEvent。输入源在操作、计时器或连接变化实际发生时触发回调，监听器立即构造对应的 Event 派生对象，映射操作参数，再把唯一所有权交给 EventReceiver。输入源、监听适配器和接收端分别负责产生事实、转换数据和后续处理。

| 接口 | 行为 |
|---|---|
| ListenEvent(EventReceiver) | 设置事件接收函数；拒绝空接收函数 |
| create_event(EventType, actor) | 按编号创建全部 136 种具体事件中的一种；仅填写类型和 actor，其他参数由调用方填写 |
| listen<EventT, Args...>(subscribe, translate) | 注册来源回调；每次回调创建 EventT，调用 translate(EventT&, Args...) 填入参数，再交付接收端 |
| stop() | 停止新事件交付并取消全部订阅；同一实例停止后不再启动 |
| listening() / subscription_count() | 查询是否仍接收输入及当前持有的订阅数 |

subscribe 的契约是接收 std::function<void(Args...)>，返回非空 EventDisconnect。取消函数由来源模块实现，应保证不抛异常。该契约可以适配现有回调接口、信号连接令牌或定时器回调，无需统一各来源的数据结构。translate 可以接收不同类型、不同数量的参数，也可以转移 move-only 数据的所有权。

| 来源 | 注册及转换的时点 |
|---|---|
| 网络操作 | 网络层解析并识别请求与玩家后触发回调，转换为相应请求事件 |
| 定时器 | 到期时提供所属局面、交互与版本，转换为对应超时事件 |
| 连接管理 | 加入、掉线或重连时提供玩家与连接代次，转换为对应连接事件 |
| 游戏业务 | 动作真实提交后触发业务回调，转换为对应事实通知 |
| 原子操作与功能牌 | 原子函数返回事实，CardEffects 缓冲，HandleEvent 驱动后续事件 |

网络层负责协议解析、玩家身份与允许的消息类型；不能把客户端任意声明的类型直接当作服务端成功通知或效果。ListenEvent 的工厂是内部对象构造接口，构造对象并不表示操作已通过校验。输入源注册和字段映射由来源模块提供，TCP 来源已由 Server 接入，截止时间由 Scheduler 维护；其他业务来源仍可使用同一订阅契约。

下面是可独立编译的接线示例，PlayInput 表示已有网络层提供的一种解析结果。实际项目可直接使用原有消息类型：

```cpp
#include "network/ListenEvent.hpp"

namespace cardgame::examples {

struct PlayInput {
    TurnId turnId;
    std::vector<CardId> cards;
    std::vector<WildcardAssignment> wildcards;
};

bool bind_play_input(
    network::ListenEvent& listener,
    const std::function<EventDisconnect(
        std::function<void(const PlayInput&)>)>& subscribe,
    Players authenticatedPlayer) {
    return listener.listen<PlayCardsRequest, const PlayInput&>(
        subscribe,
        [authenticatedPlayer](PlayCardsRequest& event, const PlayInput& input) {
            event.actor = authenticatedPlayer;
            event.turnId = input.turnId;
            event.cards = input.cards;
            event.wildcards = input.wildcards;
        });
}

}  // namespace cardgame::examples
```

接收端通过 EventReceiver 捕获房间执行器或 Condition，结合来源模块维护的 DispatchMeta 调用已实现的 Condition::on_event。多个线程可以产生来源回调，接收端必须把同一房间的事件投递到串行执行器；ListenEvent 只同步订阅集合，不自行维护商店队列或执行业务操作。参数映射器若包含可变共享状态，也应由调用方提供同步。

停止时先关闭事件交付，再在互斥锁外取消订阅，允许取消过程重入监听器。来源残留回调通过弱引用和停用标记安全返回；注册失败后的残留回调也会停用。已经进入接收函数的调用可以继续完成，销毁接收对象前应由来源模块或房间执行器等待这些调用结束。

参数映射或接收函数的异常传回来源调用方，未完成映射的事件不交付。显式 stop() 会尝试全部取消函数后重新抛出第一个异常；析构期间只执行清理。来源注册函数即使同步触发首个回调，或该回调中停止监听器，也会正确取消刚刚建立的订阅。

验证用例见 tests/ListenEvent.test.cpp，覆盖 136 种工厂映射、不同参数来源、无效类型、注册及映射失败、停止与析构、同步重入、并发来源和 move-only 参数。可在仓库根目录使用 C++17 编译器运行：

```text
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -pthread -Iinclude src/network/ListenEvent.cpp tests/ListenEvent.test.cpp -o ListenEvent.test.exe
./ListenEvent.test.exe
```

## 8. 延迟与执行方式

商店队列与局内直接结算的选择依据是输入的并行或顺序关系，并非已测得的延迟结论。事件驱动在同一服务进程中可由函数调用、商店队列和当前结算栈实现，不要求每个事件新建线程、等待固定轮询，或增加一次客户端与服务器往返。

```text
端到端耗时
  = 输入传输
  + 服务端调度等待（商店请求包含应用层排队）
  + 校验与当前事件链结算
  + 结果同步
```

- 商店普通请求入队后及时启动或唤醒调度循环；局内合法普通操作直接进入当前结算，忙碌或未获授权的出牌请求不缓存。当前动作的派生事件由循环与结算栈继续处理。
- 内部技能连锁在服务端内存中结算，再同步最终状态与必要的表现事件；玩家必须选择目标的交互会产生真实等待，应记录为等待状态。
- 每个 Condition 复用唯一 Context，持有完整当前事件与小型工作快照；Event 通过 unique_ptr 移交，不复制派生对象。当前结构转换已通过测试，端到端成本仍需对局接入后测量。
- 事件处理路径避免同步等待磁盘、数据库或外部网络；这些工作通过独立任务完成，并按规则接收结果。
- 检测事件循环、控制连锁工作量，重点测量复杂牌型判断和具体效果的成本。不同房间可以独立调度，同一房间的共享状态由统一入口修改。
- 定时器用于明确的超时规则；用固定周期扫描所有状态会额外引入接近一个扫描周期的等待，应优先使用实际业务回调。

在实现后记录输入到达、商店请求入队（如适用）、开始处理、事件链完成和结果发出的时间，分别统计服务端调度等待、商店排队等待、连锁耗时与端到端耗时；关联 `eventId`、`rootEventId`，便于定位慢技能和重复链。当前已验证本地四客户端 TCP 交互与完整基础八局流程；尚未做网络负载或生产端到端延迟测量。

## 9. 后续实现事项

当前已运行的范围是：原子操作与后端适配接口、玩家／系统功能牌、动态注册矩阵、牌级延迟派发、共享上下文栈、四人基础八局规则、商店基础服务、客户端请求校验、TCP 分帧、私有状态过滤、请求去重和会话重连。

后续业务重点为正式卡池与平衡、卡包和 E 系列拓展牌、U06 交互、更多具体主动牌的费用和次数，以及相应系统牌接入。原子函数接口已经覆盖这些操作，但带 Operation 的接口需要真实业务回调，并不会凭空完成未提供的状态模块。

当前服务器为单房间 TCP 原型；账户认证、TLS、持久化、多房间托管和部署由上层接入。测试覆盖四个客户端接线和完整基础对局，不代表正式卡池或所有未接入业务已经验证。

## 10. 服务器构建与协议

### 10.1 构建和启动

需要 C++17、CMake 与线程库，Windows 使用 Winsock，Linux 使用 POSIX socket；Python 可用于 TCP 集成测试。本机已使用 MinGW 构建：

```text
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
build/cardgame_server.exe --bind 127.0.0.1 --port 27345
```

其他平台可使用本机可用的 CMake 生成器。--bind 可指定服务地址，--port 0 会分配可用端口，--seed 固定游戏随机数；--shop-time 与 --turn-time 调整计时秒数。服务器输出 LISTENING address:port 后开始接受连接。

### 10.2 入座与请求

输入为 UTF-8 文本行，以换行结尾，支持 TCP 半包、粘包和 CRLF；响应为逐行 JSON。四个客户端分别发送：

```text
JOIN Alice
```

服务器返回 hello、welcome（含 seat 和私有 token）与 waiting；第四人入座后自动开放首局商店。seat 为 0～3，对应 P1～P4。重连新连接发送 RECONNECT token，服务器恢复同一座位并返回 lastRequest 与按权限过滤的 state。

已入座客户端每条请求格式为：递增正整数 requestId、空格、命令及参数。重复或倒序 requestId 返回 duplicate 与当前状态，不再次执行。重连继续使用大于 lastRequest 的编号。

| 命令 | 参数与含义 |
|---|---|
| STATE | 请求当前私有状态，不修改权威版本 |
| DONE shopId | 结束自己的本次购物 |
| BUY shopId slot productId | 买指定货架实例 |
| SELL shopId cardSeq | 出售自己的功能牌 |
| REFRESH shopId | 刷新未锁定货架 |
| LOCK / UNLOCK shopId slot | 锁定或解锁一个普通货位 |
| UPGRADE shopId pattern | 加强类型 0～4：单张、对子、顺子、炸弹、同花顺 |
| FACILITY shopId definitionId | 买当前可购设施 |
| PLAY turnId cardId... | 选择自己的手牌；需要指定时生成 choice |
| OPTIONS cardId... | 只计算合法方案，不提交出牌或推进随机数 |
| CHOOSE choiceId optionId | 确认当前合法方案，optionId 从 0 开始 |
| PASS turnId | 跟牌者不出牌，领出者不允许 |
| ABILITY cardSeq abilitySeq targetSeat... | 发动自己卡牌的主动规则，由卡牌核对技能序号和目标 |
| CONTINUE EXTRA3 / ENDLESS | 整场结束后的继续游戏投票 |
| NEW | 整场结束后请求开始新游戏 |

例：1 STATE、2 DONE 17、3 PLAY 42 7 8 53。所有 player / actor 都由连接会话生成，客户端不能提交加分、发牌成功或其他内部事实通知。无效命令、过期回合、越权玩家、无效手牌与余额不足会明确返回 error。

### 10.3 状态与生命周期

state 包含阶段、局、回合、当前行动者、公开玩家信息、牌堆数、桌面已出牌及已确定的赖子方案，并仅向本人发送真实手牌、私人货架、待选牌组与方案。商店阶段其他玩家的金币、分数与新购卡牌隐藏；系统牌始终不进入玩家卡槽展示。

服务器按房间单线程处理输入和截止时间，非阻塞套接字支持部分读写，限制输入行与积压输出。断线后保留 token 和资源，重连可替换旧连接；不是重新加入新玩家。不提供投降或机器人补位。

所有代码设计继续维护在本文件。正式规则见 README.md，运行说明中的 Demo 卡池只用于验证现有架构和基础流程。

## 11. 网页版与服务器部署

### 11.1 包装层与房间

浏览器通过同源 Socket.IO 连接 Node 20 网关；网关为每个房间启动一个现有 C++ `cardgame_server` 进程，绑定随机的本地端口。每个玩家对应一条私有 TCP 连接，继续使用第 10 节协议。外部仅开放 Nginx 的 80 端口；Node 固定监听 `127.0.0.1:3000`，规则进程也仅监听 loopback。

```mermaid
flowchart LR
    A[四位网页玩家] -->|HTTP / WebSocket| B[Nginx :80]
    B -->|静态文件| C[/var/www/cardgame/client]
    B -->|/socket.io/| D[Node + Socket.IO 127.0.0.1:3000]
    D -->|每人私有 TCP 连接| E[房间 A · C++ 权威规则进程]
    D -->|每人私有 TCP 连接| F[房间 B · C++ 权威规则进程]
```

`web/server/server.mjs` 管理六位房间号、四人座位、生命周期、进程和 Socket.IO；`commands.mjs` 将结构化操作白名单转为原协议；`main.mjs` 是独立启动入口，兼容 PM2 的模块加载方式。规则、发牌、随机数、事件队列和结算仍由 C++ 执行。客户端不能传入 actor 或内部事件；网关分配递增请求编号，C++ 再验证所属手牌、时机和费用。

房间人数固定为四人，第四人加入后自动进入商店。最多同时保留 12 个房间，所有玩家离线十分钟后回收房间进程。网关为每位玩家生成随机网页会话 token，另行私藏 C++ token；玩家状态只发给本人，禁止把私有状态广播到整个房间。名字用文本或 HTML 转义显示，输入限制长度和换行；消息上限 16 KiB，每连接操作限流 25 次/秒，浏览器连接检查同源。

网页 token 存在 `sessionStorage`，每个标签页独立；原标签页刷新后自动恢复。Socket.IO 连接中断给予五秒重连宽限，期间保留 TCP 连接；超过宽限后通知 C++ 掉线。开局前离线超过宽限释放座位并使 token 失效；开局后保留座位并按已有超时规则处理。已有在线连接不能被同一 token 抢占。网关或服务器重启会清空内存房间和会话；当前测试版没有持久化恢复。

### 11.2 网页客户端

`web/client/index.html`、`app.js`、`style.css` 提供房间创建/加入、四人状态、购物、出售、锁定/刷新、五种牌型加强、设施、手牌点选、预览、出牌/不出、赖子方案分页选择、八局结算、加赛/无尽/重开投票。卡面使用数字、花色与功能说明，未加入图片素材。

响应式页面支持手机横屏、触摸点选和手牌横向滚动，处理安全区；竖屏保留可用布局并提示横屏。全屏按钮在浏览器允许时请求横屏。纯 HTTP 下复制邀请链接可能不受浏览器支持，此时显示房间号，仍可手动分享网址。`remainingMs` 来自 C++ 截止时间，客户端只显示倒计时，不能决定超时。

协议新增本人专属 `self.functionSlots`、`self.upgrades`、`self.facilities`，以及商店的付费/免费刷新次数和折扣使用标记；不公开其他玩家的私有商店信息。测试卡池效果说明与当前默认十张 Demo 卡一致；接入正式卡池时应由卡牌定义下发说明。

### 11.3 本地开发与验证

先按第 10 节构建 C++。在 `web` 目录运行：

```text
npm ci
npm run build
npm test
npm run test:browser
npm start
```

Windows 默认使用 `build/cardgame_server.exe`；其他平台通过 `CARDGAME_ENGINE` 设置本机可执行文件绝对路径。Node 默认监听 `127.0.0.1:3000`，本地同时提供构建后的网页文件。浏览器测试使用本机 Edge，可通过 `BROWSER_PATH` 指向 Chrome；不在目标服务器安装浏览器。`TARGET_URL` 可以指向已部署实例。

`tests/gateway.test.mjs` 验证四人购物、资源扣费、私有数据过滤、发牌、出牌、越权拒绝、重连、跨房间隔离与在线座位保护。`tests/browser.mjs` 通过真实浏览器以 844×390 手机横屏完成四人入座、购买、发牌、出牌和刷新重连，并检查控制台错误与页面横向溢出。C++ 原有四组 CTest 继续验证全部事件、结算链、完整八局与原生 TCP 协议。

### 11.4 构建产物与部署

服务器使用 Ubuntu 22.04、Node 20、Nginx 和 PM2。**禁止在目标服务器运行 `npm install`、`npm ci`、`npm run build`，也不在服务器编译 C++。** 所有安装、前端打包、生产依赖裁剪和 C++ 编译都在工作站完成。

`deploy/build-release.py` 在本地使用 Zig 0.15.2 交叉编译 `x86_64-linux-musl` 静态 ELF，从而避免 Windows 二进制或不同 glibc 版本问题；再用 esbuild 打包网页，并在本地产生仅含运行依赖的 `node_modules`，排除开发依赖和可选原生加速模块。依赖版本与完整性由 `web/package-lock.json` 锁定。需要本机 Node、npm、Python 和 Zig：

```text
python deploy/build-release.py --zig /path/to/zig
```

可用 `--npm-cli /path/to/npm-cli.js` 指定本地 npm 入口，`--engine /path/to/prebuilt-linux-elf` 复用同一源码已构建的 Linux 文件。输出位于被忽略的 `deploy/artifacts/`，包括按 UTC 时间命名的压缩包与 SHA256 校验值。只上传压缩包和激活脚本：

```text
scp deploy/artifacts/RELEASE.tar.gz deploy/activate.sh cardgame:/var/tmp/
ssh cardgame 'bash /var/tmp/activate.sh RELEASE SHA256 /var/tmp/RELEASE.tar.gz'
```

`deploy/activate.sh` 校验压缩包，解压到 `/var/www/cardgame/releases/RELEASE`，检查 Linux/Node 入口，切换稳定路径的符号链接，检查 Nginx 配置，启动 PM2，完成健康检查后重载 Nginx，保存进程列表并启用 `pm2-root` 的 systemd 开机恢复。失败时恢复先前链接与 Nginx 配置。保留旧版本，避免覆盖既有真实目录。版本切换会重启网关，测试房间随之清空。

| 路径或服务 | 用途 |
|---|---|
| `/var/www/cardgame/client` | Nginx root，指向当前版本静态网页 |
| `/var/www/cardgame/server` | Node 入口、生产依赖与 `bin/cardgame_server` |
| `/var/www/cardgame/ecosystem.config.cjs` | PM2 配置，单进程 fork，180 MiB 网关内存重启阈值 |
| `/etc/nginx/sites-available/cardgame` | 指定 IP 的站点，不修改其他站点配置 |
| `/socket.io/` | 反代到 `127.0.0.1:3000`，HTTP/1.1，显式 Upgrade / Connection 头 |
| `/healthz` | HTTP 健康状态和当前房间数 |
| `cardgame-web` | PM2 应用名 |

WebSocket 配置采用 [Nginx 官方说明](https://nginx.org/en/docs/http/websocket.html) 与 [Socket.IO 官方反代说明](https://socket.io/docs/v4/reverse-proxy/)，反代读取超时 75 秒大于默认心跳窗口。运行检查：

```text
ssh cardgame 'pm2 status; systemctl is-enabled pm2-root; curl -fsS http://127.0.0.1:3000/healthz'
ssh cardgame 'pm2 logs cardgame-web --lines 50 --nostream'
```

公网测试入口为 [http://47.76.64.154/](http://47.76.64.154/)，只需 80 端口。当前不配置域名与 HTTPS。此发布是完整基础流程的可玩测试实例；正式卡包、E 系列拓展牌和 U06 仍保持未开放状态。

## 12. 消耗牌与功能牌强化

### 12.1 类型与生命周期

原有实现只接入设施、牌型加强服务和持续功能牌；本次增加通用消耗牌商品。规则见 README §6.8，以下是代码设计。

`ConsumableCards.hpp` 定义 `ConsumableCard` 商品与三个具名效果参数 `ExpandShopSlots`、`UpgradeShop`、`EnhanceFunctionCard`，用 `ConsumableEffect` 的 `std::variant` 保存其中一种。每个效果对应一个原子状态操作；新增相同机制的消耗牌只需配置定义编号、名称、价格、说明和效果参数。消耗牌不是持续订阅者，不继承 `FunctionCards`，不占功能槽或系统牌注册行，没有库存、装备和延后使用接口。

`ShopOffer.kind` 用公共枚举 `ShopProductKind` 区分功能牌和消耗牌；定义编号在两类商品之间唯一，实例编号 `ProductId` 标识本次具体出货。`SchedulerConfig.consumables` 可注入消耗牌定义，空列表使用四张 Demo 牌；`consumableChance` 配置混合货位的出货比例。普通功能牌工厂保持原有接口，新增 `minimumShopLevel` 限制出货等级。

### 12.2 购买与一次性执行

沿用 `BuyShopItemRequest` 与购买报价链，不再增加一个“存入背包后使用”的请求。强化牌在购买请求中携带 `optional<CardRef> targetCard`；经过 `BeforeShopPurchaseEvent` 原样传递，成交时重新核对当前商店、商品实例、余额、目标归属、装备状态、强化支持性和容量上限。

```text
BuyShopItemRequest(shop, slot, product, optional targetCard)
 → BeforeShopPurchaseEvent（报价）
 → 系统牌重新校验，并提交即时效果、扣款、清空商品和解除该货位锁定
 → 效果事实通知 / CoinsChanged / ShopPurchased / ConsumableUsed
```

上述操作与通知先进入同一系统牌的结算过程；该牌完成后才逐个处理后续事件，因此监听者看到已扣款、已使用和已清空的最终状态。余额不足、目标失效、目标属于别人、达到上限、过期商店或重复商品实例等业务失败不会扣款或消耗商品。既有原子操作约定继续适用：已经提交的操作不会因为后续监听牌报错而回滚。

购买消耗牌不检查功能槽是否已满；购买普通功能牌仍需空槽，且不接受强化目标。没有合法目标的强化牌不生成，锁定后目标发生变化的商品也必须在成交时重新检查。只读状态和报价不购买、不使用、不推进随机数。

### 12.3 扩容与商店升级

`Player.extraShopSlots` 保存消耗牌带来的扩容，实际货位数为 `min(6, 设施基础宽度 + extraShopSlots)`，设施基础宽度为 3/4/5。扩容只补新增货位，不替换原有商品、锁定项或已买空的位置。`fill_offers` 支持为临时货架生成新位置，成功后一起提交货架、容量、随机数状态和实例序列。功能牌槽仍由 `functionSlots` 独立管理。

`Player.shopLevel` 初始 1、Demo 上限 3，`abilities::upgrade_shop` 是独立原子操作。等级立即改变；原货架商品保留，之后的刷新、扩容补货和下局商店按新等级筛选候选。默认功能牌 1～6 / 7～8 / 9～10 分别从商店等级 1 / 2 / 3 解锁，暂不另加未审定的稀有度权重。扩容、等级和原有设施跨局及加赛保留；`StartNewGameRequest` 全票生效时一起清空。

### 12.4 强化数值与附加技能

`FunctionCardEnhancement` 包含 `scoreDelta`、`coinsDelta` 和 `vector<AddedCardSkill>`。这是传给目标牌的强化参数，没有恢复统一 Ability 基类。卡牌通过 `validate_bonus_upgrade` 和不抛异常的 `apply_bonus_upgrade` 解释自己的数值；`ResourceBonusCard` 已实现得分、金币增量与溢出检查，其他类型默认拒绝不支持的数值强化。

`AddedCardSkill` 保存订阅掩码、卡牌级处理回调、说明和预览安全标记。原有 `resolve` 继续只处理原始订阅；`HandleEvent` 改为调用 `resolve_all`，按“原有技能 → 附加技能顺序”共享同一个 `CardEffects` 缓冲区。新增“不出”技能不会把原先“出牌加分”也触发一次。技能列表按一次卡牌处理冻结，处理期间再强化自身只影响后续处理；技能对象保持共享存活，不丢失有状态回调的内部状态。

`Register::enhance(player, slot, enhancement)` 在所有校验和内存准备完成后更新同一实例，并同步矩阵订阅位。不更换 `CardRef`、槽位、指针或装备 token，也不伪造卸下/装备通知。`abilities::enhance_function_card` 对此提供原子技能接口。仅有纯技能强化也有效；系统牌、废除牌、不支持的数值、负增量、空技能、无效掩码或溢出会被拒绝。强化等级作为实例信息保留，出售该牌后不转移到新购同名实例。

### 12.5 新事件与协议

保留原 0～131 编号，追加四个纯事实事件，`Count` 和 `TOTAL_EVENT_TYPES` 更新为 136；Condition 和 ListenEvent 均接入新的具体派生类。默认矩阵为 27×136，消耗牌本身不增加行数。

| 编号 | EventType / 派生类 | 参数与含义 |
|---|---|---|
| 132 | ConsumableUsed / ConsumableUsedEvent | player、shopId、productId、definition、可选 targetCard；即时使用已经完成 |
| 133 | ShopSlotsChanged / ShopSlotsChangedEvent | player、before、after；普通货架容量已变化 |
| 134 | ShopLevelChanged / ShopLevelChangedEvent | player、before、after；商店等级已变化 |
| 135 | FunctionCardEnhanced / FunctionCardEnhancedEvent | card、beforeLevel、afterLevel；同一功能牌实例强化已完成 |

TCP 购买命令扩展为 `BUY shopId slot productId [targetSeq]`，旧的无目标购买仍兼容；owner 始终绑定当前连接玩家。Socket.IO `buy` 操作对应可选整数 `target`。内部成功事实仍不允许客户端伪造。

状态同步给本人增加 `self.shopLevel`、`self.shopSlots`，商品增加 `kind`、`description`、`needsTarget` 和合法目标序号列表 `targets`。功能牌增加服务端生成的 `description` 与 `enhancementLevel`；网页展示服务器当前效果，不用原始 Demo 常量覆盖强化后的数值。商店阶段其他玩家的私有购物资源仍隐藏。

### 12.6 验证

`tests/Consumables.test.cpp` 覆盖功能槽全满仍可购买消耗牌、扩容当场补货、跨局保留和新游戏重置、等级解锁、上限过滤、余额不足、无目标/越权/失效目标、重复商品、原实例数字强化、附加技能更新订阅、旧技能不误触发、溢出不留下部分强化，以及新协议与状态字段。原有监听和 Context 测试遍历全部 136 种事件。

Node 集成测试和手机横屏浏览器测试增加实际购买扩容、升级并确认即时状态、不装备消耗牌的场景；四人入座、正常购牌、出牌、重连和隐私回归继续运行。

## 13. JSON 功能牌导入工具

使用说明与输入格式见 [docs/function-card-json.md](docs/function-card-json.md)，完整可运行示例为 [steady_income.json](cards/function/steady_income.json)。工具位于 `tools/import_function_card.py`，仅依赖 Python 标准库，在开发阶段生成 C++，不向服务器新增运行时 JSON 解析依赖，也不热加载卡池。

### 13.1 文件与注册

工具读取单张功能牌 JSON，校验元信息、事件名和原子操作参数，将标准定义保存到 `cards/function/<key>.json`，生成 `include/cards/generated/<key>.hpp` 与 `src/cards/generated/<key>.cpp`。派生类实现 `FunctionCards::resolve`、说明与数值强化钩子，工厂接受 `Player&`、稳定实例序号，保持现有 `FunctionCardFactory` 签名。

`src/CardCatalog.cpp` 由所有标准 JSON 共同生成，`generated_function_cards()` 返回 `SchedulerConfig::CardDefinition` 列表。Scheduler 构造默认 Demo 卡池时追加导入工厂；显式提供的自定义卡池保持调用方的定义。CMake 通过 `CONFIGURE_DEPENDS` 发现新生成的实现文件，因此导入下一张牌不需要手工修改 Scheduler、Register 或 CMake。部署脚本原有的递归源文件收集也包含这些 C++ 文件。

卡池元信息新增 `description`，出货直接使用 JSON 说明；网页展示服务端的实际名称与说明，并保留 Demo 卡的原有默认展示。工厂在实际购买时才创建持有玩家引用的实例，读取商品说明不实例化牌，也不消耗实例序号。

### 13.2 事件与原子技能映射

JSON `abilities` 数组中的每一项对应一个原子操作，指定 event、scope、op、amount；数组位置从 1 开始作为牌内技能序号。加分和加金币分别调用 `abilities::add_score` 和 `abilities::gain_coins`，产生独立事实，并共享同一 `CardEffects`，保持“整张牌完成后再处理后续事件”的约定。

生成器使用明确的事件到具体类型/主体字段映射，不以编号或 actor 猜测操作主体。self 检查事件中的 player；any_player 响应任意主体，直接资源操作仍作用于持有者。全局通知不推断胜负或先手。第一版支持五种原子操作：直接加分、直接加金币、待结算加分、待结算加金币、商店报价减费；事件/操作配对使用白名单。

直接资源操作仅在事实通知与 Commit 模式执行；预览安全标记允许生成牌参与奖励和报价预览，但不会在 Preview 模式发放资源。报价/奖励修正只修改 `CardEffects` 的工作快照。客户端请求和 ScoreChanged/CoinsChanged 等反馈事件不开放给 JSON，避免请求重放发奖和直接的资源变化循环。复杂业务或新原子操作必须先扩展具名映射，不能把 JSON 文本作为 C++ 表达式插入。

### 13.3 强化与校验

生成牌记录得分、金币两类强化增量，分别叠加到每项对应的原子收益。强化前核对对应技能是否存在，并以最大基础增量检查有符号 64 位溢出；已有数值不支持时拒绝。新增技能仍走既有 `Register::enhance` 和统一订阅矩阵。

输入 schema version 固定为 1；严格拒绝未知/重复字段、错误数据类型、负数、越界、空技能、控制字符和不支持的事件/操作配对。卡牌编号还需在浏览器安全整数范围内，且不得与内置定义或其他标准 JSON 重复。key 限制为单层小写 ASCII 标识符，拒绝路径穿越和 Windows 保留设备名；输出路径解析后必须仍位于项目根目录内。名称、说明按 C++ 字符串字面量转义，网页渲染时再次按文本转义。

重复导入同一定义幂等。覆盖已生成牌需要 `--force`，即使直接编辑标准 JSON，也会根据已生成头文件的身份标记核对 key 和编号；不覆盖无生成标记的手写文件。所有内容先完成校验和生成，再写入临时文件，逐个原子替换；普通文件写入异常时尝试恢复之前版本。`--check` 只生成变更清单，不创建输出文件。

### 13.4 验证

`tests/function_card_importer_test.py` 覆盖真实文件导入、注册、重复导入、只读检查、强制更新、身份保护、非法输入、重复编号、手写文件保护、替换失败恢复，并用当前 C++ 编译器检查全部支持的事件/操作组合。`tests/JsonCards.test.cpp` 验证实际生成工厂、主体过滤、预览无资源副作用、同牌技能结算顺序、强化数值，以及默认商店生成和购买导入牌的完整路径。

## 14. PR 自动检查

操作教程见 [docs/pr-checks.md](docs/pr-checks.md)。`tools/check_pr.py` 是本地与 GitHub Actions 的统一入口，支持 native、web、all 三种检查集合；`.github/workflows/pr-checks.yml` 负责触发器、系统环境、日志留存和结果汇总。

原生检查先核对文档本地文件链接、JSON 定义及生成头文件/实现/卡池的一致性，然后用独立的 `build/pr-checks` 目录执行 CMake Debug 构建和 CTest。生成代码检查为只读，发现不同步、缺失或孤立文件会失败，不替提交者自动修复。`.gitattributes` 统一文本为 LF，避免 Windows 与 Linux 逐字节核对时发生换行误报。`tests/pr_checks_test.py` 验证失同步、删除 JSON 和失效链接确实导致失败且不修复源文件。

网页检查使用指定原生二进制，按锁文件安装 npm 依赖，构建并执行网关及真实浏览器测试。检查入口清除 TARGET_URL，确保测试只启动本地临时房间。Windows 使用本机 Edge 或 BROWSER_PATH；Linux runner 通过锁定的 playwright-core CLI 安装 Chromium，不使用 Windows 的浏览器路径。

GitHub 工作流在 pull_request、main push 和 workflow_dispatch 时运行。矩阵包含 Ubuntu 22.04 和 Windows 2022；Windows 准备 MSVC 环境以支持 CMake 与 Python 中的编译器语法测试。Linux 额外执行 Node 20 和手机横屏浏览器场景。最终名为 PR checks 的作业仅在所有平台成功时返回成功，供需要时配置为必过状态检查。

工作流采用只读 contents 权限，checkout 不保留凭据，Actions 固定提交 SHA，不引用项目 Secrets、不执行线上部署。相同 PR 的新提交取消旧运行；单个平台限时 20 分钟，汇总限时 2 分钟，测试日志与截图保留 7 天。分支保护、强制 PR 和人工审批属于独立仓库规则，本次工作流不修改它们。
