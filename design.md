# 代码设计

所有代码设计统一维护在根目录 `design.md`，后续的类定义、接口、事件协议、状态机、调度与联机实现说明都更新在此。游戏规则见 [README.md](README.md)，卡牌参考资料见 [设计调研](docs/function-card-research.md)。

本文为 C++17 接口与架构草案；示例通过语法检查不表示事件循环、业务操作或网络服务已经实现。当前命名统一为 `Event`、`Condition`、`Context`、`Handle_Event`、`FunctionCards`、`Poker` 和 `EventListener`。

## 目录

- [1. 总体架构](#1-总体架构)
- [2. 事件与状态机](#2-事件与状态机)
- [3. 技能事件处理](#3-技能事件处理)
- [4. 功能卡](#4-功能卡)
- [5. 扑克牌 Poker](#5-扑克牌-poker)
- [6. 商店、回合与跨玩家效果](#6-商店回合与跨玩家效果)
- [7. 事件监听与输入入口](#7-事件监听与输入入口)
- [8. 延迟与执行方式](#8-延迟与执行方式)
- [9. 后续实现事项](#9-后续实现事项)

## 1. 总体架构

```text
网络操作 / 定时器 / 连接回调 / 业务动作
  → EventListener 归一输入或发布可信事件
  → Condition 接收并校验，按队列与当前状态调度
  → 生成只读 Context，调用选定的 Handle_Event
  → 返回一个新 Event，或 std::nullopt
  → Condition 执行合法效果，生成后续事件并继续调度
  → 完成结算，向相关客户端同步结果
```

每个房间拥有一套 `Condition` 和权威 `GameState`，商店、开局、回合、技能、局末和结束均进入同一条事件处理路径。功能卡拥有处理器，状态机负责选择和调用。业务模块提交事件，阶段和操作权限的判断集中在状态机。

`GameState` 统一保存所有玩家的手牌、分数、金币、功能卡和状态；`Condition` 保存流程阶段、队列、待处理交互与派发进度。事件队列保存待处理工作，历史事件记录另行管理。客户端展示和提交操作，服务端决定操作是否成立。

## 2. 事件与状态机

本轮统一采用用户最新命名：事件类 `Event`、状态机类 `Condition`、技能事件处理器类 `Handle_Event`。本文定义接口与责任，具体对局引擎尚未实现。

### 2.1 调用关系

```text
Event
  → Condition 接收，检查当前状态并生成 Context
  → 选择功能卡中的 Handle_Event，调用 handle_event(context)
  → 返回一个新 Event，或 std::nullopt
  → Condition 将新 Event 入队，执行效果或继续派发
  → 当前事件链完成后继续原事件、后续阶段与状态转移
```

`Handle_Event` 返回事件，`Condition` 负责继续驱动。处理器不自行调用 `Condition::on_event` 或递归调用其他处理器，也不直接修改游戏资源；效果用返回事件表达。`std::nullopt` 表示本处理器没有输出，状态机继续处理其他候选，工作队列清空后结束处理。

| 对象 | 职责 |
|---|---|
| `Event` | 记录发生了什么，或请求执行哪个效果，以及行为者、参数和来源 |
| `Condition` | 保存流程阶段、接收与校验事件、生成上下文、调度处理器、执行效果、推进流程 |
| `Context` | 为一次处理提供事件、处理器持有者和当前业务状态的只读视图 |
| `Handle_Event` | 读取上下文并判断条件，返回一个后续事件 |
| `FunctionCards` | 保存牌级状态并拥有处理器，提供添加、删除和废除操作 |
| `GameState` | 保存手牌、分数、金币和当前待结算数据等业务状态 |

技能接口和响应时机见 [技能说明](#3-技能事件处理)，功能卡实例与容器见 [功能卡说明](#4-功能卡)。

### 2.2 Event 与 Context

事件分为两类，均使用 `Event`：

- `Signal`：通知一个对局时机已经到达，或承载已校验的主动请求。`Condition` 为它选择候选处理器。
- `Effect`：处理器返回的具体效果请求，例如增加分数、修正待结算分数、修正报价。`Condition` 校验并执行，不把它再次当作原始出牌事件派发。

`Timing` 仍沿用现有的 34 个时机，取值不变。时机描述事件的精确边界；`GamePhase` 表示状态机当前所处的大阶段，两者分别记录。`EffectData.value` 的含义由 `EffectType` 决定，实际效果字段随卡牌补齐。

`Context` 包含只读的 `event`、`game`，以及当前处理器的 `owner`。需要事件类型、行为者或主动目标时，读取 `context.event`；当前待结算奖励和报价由 `Condition` 生成快照，分别放在 `context.reward`、`context.shop`。

原始事件及其参数在派发期间保持不变。每次轮到一个处理器，`Condition` 基于最新待结算数据重新生成上下文；前一个处理器返回的合法效果完成后，下一个处理器可以看到更新结果。不能让多个独立快照各自写回，覆盖其他处理器的结果。

主动技能请求必须来自该牌持有者，指定目标牌和技能，并处于允许窗口；`Condition` 验证后只调用选定处理器。效果执行后产生的新时机通知属于被动 `Signal`，原始主动请求的来源另外保留，不自动要求所有后续技能也为主动技。

### 2.3 状态与转移

下表规定流程骨架，所有转移在对应事件及其后续效果完成后进行；`Condition` 仍需检查真实游戏状态，不能仅凭一个事件名称跳转。

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

`Condition::on_event(Event)` 是统一接收入口。首次进入时启动处理循环；处理期间收到的新事件先入队，避免 C++ 递归调用不断加深。

同一通知按玩家、槽位、牌内处理器顺序执行。状态机为原通知保留派发进度：一个处理器返回事件后，优先完成其后续事件链，再生成下一个处理器的 `Context`。事件队列和待恢复的派发进度共同调度，后续阶段等待当前链完成。示例类的队列是接口骨架，派发游标与恢复结构待实现时补齐。

`Handle_Event::handle_event(const Context&)` 返回 `std::optional<Event>`：

- 空值：没有输出事件，继续其他候选。
- 新事件：`Condition` 补入事件 ID、父事件 ID、根事件 ID及具体处理器来源，再校验和处理。
- 返回了事件但效果执行失败：不视为成功发动，不发奖或扣成功次数；是否采用“尝试即消耗”由具体技能规则明确。

`Condition` 根据效果真正成功的结果记录次数、处理消耗，并在允许时产生 `AbilityResolved` 等后续通知。返回事件本身不代表效果已到账。`Effect` 只在相应阶段可执行，例如 `MultiplyPendingScore` 必须指向仍在处理的奖励结算；报价查询中不得接受直接发放金币或分数的效果。

真实事件 ID 用于去重，后续事件带来源链。同一事件对同一处理器至多派发一次；因果链不得重新进入尚未完成的处理器。状态机需设置有限的事件链处理预算，检测到循环或超限时停止该链并记录原因，具体上限待实际卡牌确定。

### 2.5 C++17 接口草案

公共枚举、事件参数、上下文和状态机集中定义于此；技能和功能卡接口在后续章节引用它们。

```cpp
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>

enum class Players : std::uint8_t { P1, P2, P3, P4 };
enum class AbilityType : std::uint8_t { Active, Passive };
enum class PlayerScope : std::uint8_t { Self, Others, Any };

enum class Timing : std::size_t {
    GameStart = 0,
    ShopEnter = 1,
    ShopActionWindow = 2,
    ShopRefreshQuote = 3,
    ShopRefreshAfter = 4,
    ShopBuyQuote = 5,
    ShopBuyAfter = 6,
    ShopSellQuote = 7,
    ShopSellAfter = 8,
    ShopLeave = 9,
    RoundStart = 10,
    InitialDealBefore = 11,
    InitialDealAfter = 12,
    TrickStart = 13,
    TurnStart = 14,
    PlayActionWindow = 15,
    PlayValidate = 16,
    PlayCommitted = 17,
    PlayRewardBefore = 18,
    PlayRewardAfter = 19,
    PassCommitted = 20,
    TurnEnd = 21,
    TrickEnd = 22,
    DrawBefore = 23,
    DrawAfter = 24,
    TrickDrawsAfter = 25,
    RoundEnd = 26,
    RoundRewardBefore = 27,
    RoundRewardAfter = 28,
    GameEnd = 29,
    CardChanged = 30,
    AbilityResolved = 31,
    FunctionCardEquipped = 32,
    FunctionCardRemoved = 33,
    Count = 34
};

inline constexpr std::size_t TOTAL_TIMES =
    static_cast<std::size_t>(Timing::Count);

enum class GamePhase : std::uint8_t {
    NotStarted, Shopping, RoundStarting,
    Playing, RoundEnding, Finished
};

enum class EventKind : std::uint8_t {
    Signal,  // 对局时机通知或已校验的主动技能请求
    Effect   // 请求 Condition 执行一个具体效果
};

enum class EffectType : std::uint8_t {
    AddScore,
    MultiplyPendingScore,
    ReducePendingShopCost
};

struct RewardData {
    int score = 0;
    int coins = 0;
};

struct ShopData {
    int baseCost = 0;
    int finalCost = 0;
};

struct EffectData {
    EffectType type = EffectType::AddScore;
    Players target = Players::P1;
    int value = 0;  // 例如加分数量、整数倍率或减费金额
};

class Event {
public:
    EventKind kind = EventKind::Signal;
    std::uint64_t eventId = 0;  // 新返回事件由 Condition 分配
    std::optional<std::uint64_t> parentEventId;
    std::uint64_t rootEventId = 0;

    // Signal 使用的事件描述；Effect 不使用 timing 进行技能派发。
    Timing timing = Timing::GameStart;
    std::optional<Players> actor;
    AbilityType source = AbilityType::Passive;
    std::optional<std::size_t> requestedCardSeq;
    std::optional<std::size_t> requestedAbilityIndex;
    std::optional<RewardData> reward;
    std::optional<ShopData> shop;

    // Effect 使用的效果请求。
    std::optional<EffectData> effect;
};

class GameState;  // 资源、手牌等业务状态，具体结构后续定义

struct Context {
    const GameState& game;
    const Event& event;
    Players owner;

    // Condition 从当前待结算状态生成的只读快照。
    std::optional<RewardData> reward;
    std::optional<ShopData> shop;
};

class Condition {
public:
    explicit Condition(GameState& game) : game_(game) {}

    // 接收并排队，串行处理；不递归执行新返回的事件。
    void on_event(Event event);
    GamePhase phase() const { return phase_; }

private:
    bool can_accept(const Event& event) const;
    Context make_context(const Event& event, Players owner) const;
    void dispatch_next_handler();
    void apply_effect(const Event& event);
    void finish_event(const Event& event);

    GameState& game_;
    GamePhase phase_ = GamePhase::NotStarted;
    std::deque<Event> events_;
    bool processing_ = false;
};
```

`Condition::make_context` 从当前状态生成临时只读快照；`dispatch_next_handler` 按功能卡中的处理器顺序调度；`apply_effect` 执行合法效果；`finish_event` 负责完成当前结算并在满足条件时推进状态。这些方法目前只声明责任，队列执行、业务操作、请求校验与循环检测尚未实现。

`Event` 返回值按值保存，`Context` 仅在当前处理期间有效；处理器不能保存其引用供之后使用。新事件不能持有指向该临时上下文的指针或引用。

### 2.6 示例链

“自己的本手出牌分数翻倍”的处理器流程为：

```text
Condition 收到 PlayRewardBefore 通知
  → 生成 Context，包含当前本手分数
  → DoubleScoreHandler 返回 MultiplyPendingScore 效果事件，value = 2
  → Condition 校验目标与结算阶段，将待结算分数乘 2
  → 生成下一个处理器的 Context，继续该手结算
  → 所有处理完成后，按最终金额发放本手奖励
```

处理器示例代码见 [技能说明](#37-事件处理示例)。正式卡牌及数值尚未确定。

## 3. 技能事件处理

本文说明卡牌技能的设计，以及使用 C++17 抽象基类 `Handle_Event` 实现这些设计。`Handle_Event` 作为事件处理器，统一通过 `handle_event(const Context&)` 读取上下文，返回一个新 `Event` 或空值，由状态机 `Condition` 继续处理。功能牌的持有者、实例、技能容器与牌级状态见 [功能卡说明](#4-功能卡)。`Event`、`Condition` 与 `Context` 的定义见 [事件与状态机说明](#2-事件与状态机)。本文为接口草案，尚未实现对局引擎。

### 3.1 时机与行为主体分别记录

`time[i] == true` 只说明技能允许在第 i 个时机执行，不表示条件满足、目标合法或次数充足。另用 `PlayerScope` 限制行为主体：`Self` 为持有者、`Others` 为其他实际参局玩家、`Any` 为任意主体。

例如：

| 描述 | 时机 | 主体范围 |
|---|---|---|
| 自己出牌时修改本手奖励 | `PlayRewardBefore` | `Self` |
| 任意玩家成功出牌后 | `PlayCommitted` | `Any` |
| 自己成功出牌后 | `PlayCommitted` | `Self` |
| 其他玩家成功出牌后 | `PlayCommitted` | `Others` |
| 自己出牌奖励到账后 | `PlayRewardAfter` | `Self` |
| 每巡结束后 | `TrickEnd` | `Any`，如需自己获得牌权则再检查牌权拥有者 |

同一次出牌只发布一次 `PlayCommitted`，不能再发布“任意玩家出牌后”“自己出牌后”“其他玩家出牌后”三个事件。主体范围由每个技能相对自己的持有者判断；主体范围也不是技能的效果目标。

`Context.event.actor` 表示发起当前行为的玩家，`Context.owner` 表示正在检查的技能持有者；效果目标由对应事件参数记录，三者允许不同。局开始、局结束等全局事件可以没有 `actor`，此时不按 `Self/Others` 匹配，使用 `Any` 并根据事件中的赢家等字段判断。

### 3.2 第一版时机表

区分整场游戏（Game）、一局（Round）、一巡（Trick）、一名玩家的一次行动回合（Turn）。以下编号是布尔集合下标，固定后新增时机追加在末尾，避免旧数据错位；`Count` 不是可触发时机。

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

商店开包、使用拓展牌、牌型加强与设施购买使用购买上下文中的商品类型和后续效果信息区分。未来出现必须单独监听的新业务边界时追加时机，不用“购买后”冒充“使用后”；当前没有为这些行为自动增加收益。

**一次事件与一个操作窗口不同。** `RoundStart` 等被动事件每次边界只派发一次；`ShopActionWindow`、`PlayActionWindow` 表示允许主动操作的持续区间，不通过反复派发事件执行被动技能。被动牌若需要在窗口开始时执行，分别订阅 `ShopEnter` 或 `TurnStart`。

**报价与校验可以重复进行，必须没有持久副作用。** `ShopRefreshQuote` 等只修改本次报价，`PlayValidate` 只提供合法性修正；UI 预览或失败请求不扣费、不扣次数、不重复发奖。一次性免费额度、使用次数和消耗仅在对应操作成功提交时确认，持久副作用放在成功后的事件中。

**持续效果不等于每帧执行技能。** 折扣、可用牌型、手牌上限等由引擎在报价或校验时查询有效技能；如果使用缓存，装备、移除、失效与恢复时必须更新缓存。常驻规则仍属于被动技能。

### 3.3 结算顺序与边界

一次普通出牌按如下顺序结算：

```text
TurnStart → PlayActionWindow
  → 选择牌与赖子方案 → PlayValidate → 最终合法性校验
  → 成功提交，关闭主动窗口 → PlayCommitted
  → PlayRewardBefore → 出牌奖励到账 → PlayRewardAfter
  → TurnEnd
```

在提交前修改了手牌、赖子方案或规则时必须重新校验；非法出牌不发布出牌成功、出牌奖励事件。成功出牌是每手一次，不按手中每张扑克牌重复派发。如果今后设计逐张计分，另加逐张时机。

不出牌走 `TurnStart → PlayActionWindow → PassCommitted → TurnEnd`，不进入出牌成功或奖励事件。“出牌后”与“回合结束后”因此不等价。每个阶段先处理其允许的派生效果，再进入下一阶段。

获得牌权且无人出空时，走 `TurnEnd → TrickEnd → 各玩家 DrawBefore/DrawAfter → TrickDrawsAfter → 下一巡 TrickStart`。牌堆空时不制造虚假的 `DrawAfter`；巡结束表示获得牌权，不表示每人恰好行动过一次。

保留现有“出空即结束”的基础规则：最后一手成功提交时锁定赢家及各玩家剩余手牌快照；该手的出牌后技能、牌型奖励、奖励后技能和 `TurnEnd` 仍正常处理，然后进入 `RoundEnd`。后置效果不能撤销已经锁定的胜利，局末名次和扣减按该快照计算；结束后不再开放出牌窗口、派发 `TrickEnd` 或进行巡末摸牌。要改变胜利条件的特殊卡牌需另行明确规则。

局末依次执行 `RoundEnd → RoundRewardBefore → 基础奖励到账及 E17 借款扣款 → RoundRewardAfter`，最后一局后再执行 `GameEnd`。金额只发放一次；后置技能新增的资源记录为额外收益。

同一时机先按本事件指定的起点沿逆时针排列玩家，再按功能牌槽位从左到右、牌内技能顺序结算；全局无行为主体的事件从本局先手起，整场初始化用 P1。`seq` 是牌实例编号，不是槽位位置。

派发时冻结候选列表，轮到每个技能时重新检查状态和条件。结算中新增的技能从后续事件开始参与；已被禁用、废除或移除的候选跳过。物理删除延迟到当前结算安全点，避免正在调用的对象被销毁。

`Context.event.eventId` 标识一次真实事件，网络重试不生成新事件。每个技能对同一事件默认至多执行一次；派生事件携带来源链，默认不得重新进入链中尚未完成的同一技能。复制和重复结算若开放，必须另外规定可复制范围、次数上限和是否产生新事件，不通过递归再次发布原始出牌来实现。

### 3.4 主动、被动和状态

- 主动技能：玩家在允许窗口发起请求，由服务端封装为主动 `Event`（`source = Active`），同时填写目标功能卡与技能标识；只派发给被选中的技能。未主动请求不执行；超时默认放弃可选主动技能。
- 被动技能：事件发生或规则被查询时，由系统提供被动 `Event`（`source = Passive`），按既定顺序派发给对应处理器。触发后选择目标仍属于被动技能，不因此改成主动技。
- 同一牌可有多个技能，主动发动只调用所选技能，不把该牌的其他主动技能一起执行；被动事件只遍历被动技能。

| 字段 | 本轮接口约定 |
|---|---|
| `available` | 对象仍然有效；废除、消耗或移除后设为 false，不用重置次数恢复它 |
| `usable` | 对象当前允许执行的开关；临时封禁设为 false，解除后可以恢复 |
| `time` | 固定长度时机掩码；只过滤时机，不代替主体、次数和目标判断 |
| `type` | `Active` 或 `Passive` |
| `scope` | 响应自己的、他人的或任意主体的行为 |

`Condition` 统一调用 `handle_event(context)`。状态机核对持有关系、牌级状态和请求目标，处理器读取上下文并检查自己的 `available/usable`、事件 `source` 与自身 `type`、时机掩码、主体范围、目标、费用、次数及具体条件；满足后返回效果事件，由 `Condition` 校验并执行。`usable == true` 不表示可以忽略时机直接发动。

次数上限、已使用次数、重置范围、消耗策略和持续期限在逐张设计时明确；布尔值不能表达这些数值与生命周期。临时效果结束时撤销对应修正，不自动退款或回滚已经发放的收益。

### 3.5 C++17 接口草案

```cpp
#include <optional>
#include <vector>

// 依赖事件与状态机说明中的 Context、Event 及公共枚举。
class Handle_Event {
public:
    virtual ~Handle_Event() = default;

    bool available = true;
    bool usable = true;
    AbilityType type = AbilityType::Passive;
    PlayerScope scope = PlayerScope::Any;
    std::vector<bool> time = std::vector<bool>(TOTAL_TIMES, false);

    // 读取上下文，返回新事件；无输出时返回 std::nullopt。
    virtual std::optional<Event> handle_event(const Context& context) = 0;
};
```

此接口依赖 [事件与状态机说明中的公共定义](#25-c17-接口草案)。`Players` 表示固定座位，只遍历实际参局玩家；时机枚举与布尔集合下标仍沿用 §3.2 的 34 个取值。

技能的公共入口为 `handle_event(const Context&)`。处理器通过只读上下文获取所需参数，以 `std::optional<Event>` 表达有无后续事件；效果由 `Condition` 继续执行。基类提供虚析构函数，时机集合初始化为 `TOTAL_TIMES` 个 false。

技能由功能牌拥有；技能容器、添加、删除、废除及临时禁用的管理规则见 [功能卡说明](#43-技能管理与生命周期)。

### 3.6 Context 与派发流程

`Condition` 接收 `Event`，根据当前流程状态校验事件，并为选定处理器生成 `Context`。处理器只读当前事件和业务状态，返回一个后续事件；状态机继续处理该事件链。

| 上下文信息 | 用途 |
|---|---|
| `event.timing / event.eventId` | 原通知的时机及唯一编号 |
| `owner / event.actor` | 当前处理器所属玩家与行为发起者 |
| `event.source` | 区分主动请求与被动通知 |
| `event.requestedCardSeq / event.requestedAbilityIndex` | 主动请求目标；被动通知为空 |
| `reward / shop` | 状态机生成的当前待结算奖励或报价快照 |
| `game` | 只读业务状态视图，具体查询接口后续定义 |

```text
Event → Condition → Context → Handle_Event::handle_event(context)
                                  ↓
                         新 Event / std::nullopt
                                  ↓
                         Condition 继续调度
```

每次生成上下文时使用最新的待结算数据；返回事件由 `Condition` 执行后，再继续下一个处理器。原始事件保持不变，处理器不直接修改游戏资源、快照或事件，也不自行递归派发。

空值表示没有后续事件，状态机继续其他候选；新事件表示请求执行后续处理，实际成功后再记录成功次数和处理消耗。报价预览、事件来源、队列与循环边界见 [事件与状态机说明](#24-队列返回值与结算)。

### 3.7 事件处理示例

以下仅为接口示意，不代表正式卡牌：“自己的出牌奖励计算前，将本手待结算分数翻倍”。处理器读取上下文，并返回 `MultiplyPendingScore` 效果事件。

```cpp
class DoubleScoreHandler final : public Handle_Event {
public:
    DoubleScoreHandler() {
        scope = PlayerScope::Self;
        time[static_cast<std::size_t>(Timing::PlayRewardBefore)] = true;
    }

    std::optional<Event> handle_event(const Context& context) override {
        if (!available || !usable || context.event.source != type) {
            return std::nullopt;
        }

        const auto index = static_cast<std::size_t>(context.event.timing);
        if (index >= time.size() || !time[index]) {
            return std::nullopt;
        }

        if (!context.event.actor ||
            *context.event.actor != context.owner || !context.reward) {
            return std::nullopt;
        }

        Event next;
        next.kind = EventKind::Effect;
        next.effect = EffectData{
            EffectType::MultiplyPendingScore, context.owner, 2
        };
        return next;
    }
};
```

`Condition` 校验并执行返回事件，修改当前待结算分数；实际到账仍由奖励阶段完成。处理器返回的事件 ID 与来源由状态机统一补齐，不能把它当作一次新的实际出牌。

具体状态转移、事件队列、派发游标、成功计次、受控业务操作、交互挂起和事务处理待引擎实现时补齐。

## 4. 功能卡

功能卡（下文也称功能牌）是放入玩家槽位并持续持有的卡牌实例，由 C++17 抽象基类 `FunctionCards` 表示。一张功能卡可以包含多个技能，也可以同时包含主动与被动技能。

技能分类、生效时机、事件参数、事件处理规则和 `Handle_Event` 接口统一见 [技能说明](#3-技能事件处理)。本文为设计草案，尚未实现功能卡与对局引擎。

### 4.1 功能卡与技能的关系

功能卡负责记录“谁持有这张牌、这张牌是哪一个实例、包含哪些技能、整张牌当前是否有效”；各个技能分别记录自己的发动方式、时机、状态和具体效果。

- 功能卡保存技能事件处理器，由 `Condition` 根据 `Event` 和牌级状态选择处理器并生成 `Context`。
- `Condition` 直接调用处理器的 `handle_event(const Context&)`；处理器返回一个新 `Event` 或空值，由状态机继续执行。
- 主动请求先核对请求玩家、目标牌与目标技能，只选择指定处理器；被动通知按既定顺序遍历。
- 整张牌的状态与技能自身的状态共同限制执行；牌级检查通过后仍需执行 [技能发动条件检查](#34-主动被动和状态)。
- 同时触发的牌按槽位顺序结算，牌内按技能顺序结算；完整事件顺序见 [技能结算规则](#33-结算顺序与边界)。

### 4.2 数据成员与卡牌元信息

| 成员 | 类型 | 含义 |
|---|---|---|
| `player` | `Players` | 当前持有者，使用固定座位枚举 |
| `seq` | `std::size_t` | 持有者范围内的稳定实例序号 |
| `abilities` | `std::vector<std::unique_ptr<Handle_Event>>` | 由这张牌拥有的技能，按顺序保存 |
| `available` | `bool`，初始 true | 整张牌仍然有效 |
| `usable` | `bool`，初始 true | 整张牌当前允许执行技能的开关 |

`seq` 是持有者范围内单调分配、不重复使用的牌实例编号，`(player, seq)` 标识一张实例；移动槽位不改变编号。牌种编号 `cardId`、名称、说明、稀有度、基础价格、实际购入价格、可选基数和倍率等仍需保留，后续并入 `FunctionCards` 的卡牌元信息，不放进时机掩码。

`Players` 的固定座位定义见 [事件系统中的公共枚举](#25-c17-接口草案)。2 人或 3 人房间只遍历实际参局座位；枚举存在不表示该座位已加入游戏。

### 4.3 技能管理与生命周期

使用 `unique_ptr` 明确技能由功能牌拥有，添加时转移所有权，删除时由容器释放，避免裸指针的释放责任不清。`deleteAbilities(index)` 表示逻辑移除并在安全点释放；`abolishAbilities(index)` 表示将技能 `available` 设为 false，保留实例用于显示和记录；临时禁用使用 `usable = false`。整张牌废除时设置牌的 `available = false`，所有技能统一被门禁挡住。

| 操作 | 接口 | 说明 |
|---|---|---|
| 事件调度 | 由 `Condition` 负责 | 选择牌内处理器，生成 `Context`，执行返回事件后再继续下一个处理器 |
| 添加技能 | `addAbilities(ability)` | 转移技能所有权，加入容器 |
| 删除技能 | `deleteAbilities(index)` | 逻辑移除，并在结算安全点释放 |
| 废除技能 | `abolishAbilities(index)` | 将该技能的 `available` 设为 false，保留实例用于显示与记录 |
| 临时禁用 | 设置 `usable = false` | 可作用于整张牌或某个技能，解除后可以恢复 |
| 废除整牌 | 设置牌的 `available = false` | 所有技能统一被牌级检查挡住 |

派发时冻结候选列表，结算中新增技能从后续事件开始参与；已被禁用、废除或移除的候选跳过。物理删除需延迟到安全点，详见 [技能结算规则](#33-结算顺序与边界)。

管理接口中的 `index` 以及 `Context.event.requestedAbilityIndex` 仅适用于服务端当前稳定的技能列表；联机请求需要稳定技能编号及状态版本，不能信任客户端缓存的可变下标。实际调度、稳定 ID、次数与费用管理、交互挂起和事务处理待引擎实现时补齐。


装备与移除分别使用 `FunctionCardEquipped`、`FunctionCardRemoved` 事件，具体边界见 [技能时机表](#32-第一版时机表)。持续效果的来源随装备、移除、失效和恢复更新。

### 4.4 C++17 接口草案

以下接口依赖 [事件系统的公共定义](#25-c17-接口草案)和 [技能处理器定义](#35-c17-接口草案)，代码应接在这些定义之后使用。所有接口在本文件内按依赖顺序维护。

```cpp
#include <cstddef>
#include <memory>
#include <vector>

class FunctionCards {
public:
    FunctionCards(Players owner, std::size_t sequence)
        : player(owner), seq(sequence) {}
    virtual ~FunctionCards() = default;

    Players player;
    std::size_t seq;
    std::vector<std::unique_ptr<Handle_Event>> abilities;
    bool available = true;
    bool usable = true;

    virtual bool addAbilities(std::unique_ptr<Handle_Event> ability) = 0;
    virtual bool deleteAbilities(std::size_t index) = 0;
    virtual bool abolishAbilities(std::size_t index) = 0;
};
```

基类提供虚析构函数，以便通过基类指针正确释放派生实例。功能卡负责拥有和管理处理器；`Condition` 负责事件调度并直接调用牌内处理器。添加、删除和废除属于实例管理操作，具体功能卡由派生类实现。

调度顺序：`Condition` 先检查牌仍被持有且 `available/usable` 为 true，为当前牌持有者生成 `Context`，被动通知按技能顺序选择处理器。主动请求要求 `event.actor == player`、`requestedCardSeq == seq` 且 `requestedAbilityIndex` 有效，只选择该位置的主动处理器；不合法时拒绝请求，不退回为批量派发。返回事件及其后续链处理完毕，再继续下一个候选。联机仍使用稳定技能标识和状态版本定位，再由服务端映射到当前下标。

### 4.5 逐张功能卡的说明模板

```text
名称／牌种编号：
价格／稀有度／出售成本规则：
技能 A：主动或被动
  响应事件／时机掩码：
  Context 所需参数与返回 Event：
  行为主体范围：Self / Others / Any
  效果目标：
  发动条件与具体效果：
  费用、次数、重置边界与消耗策略：
  成长／蓄力／衰减状态及保存期限：
  无合法目标、取消、超时与掉线处理：
技能 B（可选）：同上
联动对象／互斥或叠加规则：
最后一手与局末处理：
```

使用时机与效果实际生效的时机分别记录。次数限制需写明按技能、按卡牌实例还是按玩家统计。无次数限制、无需选择目标等情况也应明确注明。

功能卡进入空槽、实际购入成本、出售价格和卖出后清除成长等交易规则见 [游戏规则的商店章节](README.md#6-商店)。拓展牌仍遵循获得即使用、不能保存的规则，不与功能卡的持有方式混用。

第一批功能卡的样本和候选方向见 [卡牌设计调研](docs/function-card-research.md)；正式卡池、价格、稀有度与效果数值尚待逐张确定。

## 5. 扑克牌 Poker

扑克牌使用 C++ 类 `Poker` 表示，当前确定以下五个数据成员：

```cpp
#include <optional>

class Poker {
public:
    bool isJoker;              // 是否为王（赖子）
    std::optional<int> suit;  // 花色
    std::optional<int> rank;  // 点数
    int multiplier;          // 倍率
    int baseValue;           // 基数
};
```

| 含义 | 变量名 | 类型 |
|---|---|---|
| 是不是王 | `isJoker` | `bool` |
| 花色 | `suit` | `std::optional<int>` |
| 点数 | `rank` | `std::optional<int>` |
| 倍率 | `multiplier` | `int` |
| 基数 | `baseValue` | `int` |

`std::optional<int>` 表示花色与点数可以为空，空值使用 `std::nullopt`。成员初始值、花色编码及倍率与基数的具体计算规则待后续确定。

## 6. 商店、回合与跨玩家效果

商店和对局共用 `Event → Condition → Context → Handle_Event → Event` 流程。商店的并行表示多个玩家同时拥有可操作会话；出牌的顺序表示只有当前玩家拥有出牌权限。状态机继续记录这些状态，业务模块无需分别维护两套事件机制。

| 状态范围 | 保存内容与调度要求 |
|---|---|
| 房间流程 | 当前局、巡、回合、阶段、结束锁定与状态版本 |
| 各玩家商店会话 | 货架、刷新次数、锁定货位、完成标记与待选卡包 |
| 当前回合 | 行动玩家、出牌窗口、待结算动作与技能链 |
| 待处理交互 | 请求者、合法目标或选项、关联事件、超时时间与恢复位置 |

独立的购买、刷新等短操作由房间调度器依次完成。P1 等待开包选择时，保存 P1 的等待状态，允许其他玩家继续进行不依赖该待选结果的购物操作；等待客户端输入时不占住处理线程。涉及已经变化的余额、目标或报价时，后续操作必须根据当前状态重新校验。

游戏回合内先完成当前动作的结算链，再开放下一玩家行动。目标玩家的被动效果或授权选择可以在他人的回合执行；出牌权限、技能持有者和效果目标分别判断。

跨玩家效果通过返回事件的目标字段执行。例如 P1 的处理器返回目标为 P2 的 `AddScore` 请求，`Condition` 修改 P2 的权威分数，再按规则产生相关通知。P2 的状态发生变化不会自动赋予 P2 出牌权限。

所有玩家完成购物或商店超时后，先结清待处理操作并关闭购物窗口，再产生开局事件。已经关闭的购物窗口拒绝迟到或重复请求。开局发牌、巡与回合推进、局末结算继续遵循 [游戏规则](README.md)。

## 7. 事件监听与输入入口

`EventListener` 是输入适配入口，通过回调接收实际发生的变化。游戏逻辑在完成动作时直接产生对应事件，监听入口负责转交；不用持续扫描所有玩家状态来猜测是否发生了出牌或购买。

| 来源 | 产生事件或请求的时点 |
|---|---|
| 网络操作 | 收到已识别玩家的操作消息，提交待校验请求 |
| 定时器 | 对应计时器到期时回调，附带所属交互和状态版本 |
| 连接管理 | 掉线或重连回调时，关联原玩家与房间 |
| 游戏业务 | 出牌、购买、摸牌或效果真正完成时，由负责该动作的逻辑发布通知 |
| 技能处理器 | 返回一个后续 `Event`，由 `Condition` 继续处理 |

玩家请求和动作成功通知分别处理：收到“我要出牌”先交 `Condition` 验证，成功提交后才能产生 `PlayCommitted`。收到“我要购买”也必须完成权限、价格、余额和交付校验。客户端不能直接声明出牌成功、发奖或内部效果已经发生。请求协议及来源校验随网络层实现补齐。

下面是可信内部事件的转交接口示意，网络解析和定时器注册尚未实现：

```cpp
#include <utility>

class EventListener {
public:
    explicit EventListener(Condition& condition) : condition_(condition) {}

    void publish(Event event) {
        condition_.on_event(std::move(event));
    }

private:
    Condition& condition_;
};
```

`EventListener` 负责适配输入，`Condition` 负责状态与调度。监听入口本身不更改手牌、金币、分数或结算上下文；`Context` 由状态机在调用处理器之前根据最新状态生成。

## 8. 延迟与执行方式

事件驱动在同一服务进程中可以由函数调用和内存队列实现，不要求每个事件新建线程、等待下一次固定轮询，或在客户端与服务器之间再往返一次。

```text
端到端耗时
  = 输入传输
  + 房间队列等待
  + 校验与当前事件链结算
  + 结果同步
```

- 输入到达且当前没有处理任务时，立即启动或唤醒调度循环；处理期间的新事件入队，依照当前链及恢复顺序执行。
- 内部技能连锁在服务端内存中结算，再同步最终状态与必要的表现事件；玩家必须选择目标的交互会产生真实等待，应记录为等待状态。
- `Context` 引用只读的 `GameState` 与 `Event`，仅生成必要的小型参数快照，避免每次处理复制整个牌局。
- 事件处理路径避免同步等待磁盘、数据库或外部网络；这些工作通过独立任务完成，并按规则接收结果。
- 检测事件循环、控制连锁工作量，重点测量复杂牌型判断和具体效果的成本。不同房间可以独立调度，同一房间的共享状态由统一入口修改。
- 定时器用于明确的超时规则；用固定周期扫描所有状态会额外引入接近一个扫描周期的等待，应优先使用实际业务回调。

在实现后记录输入到达、入队、开始处理、事件链完成和结果发出的时间，分别统计排队等待、连锁耗时与端到端耗时；关联 `eventId`、`rootEventId`，便于定位慢技能和重复链。当前没有运行中的对局引擎或实测数据，不能给出可靠的毫秒数；仅凭增加这些类不能判断最终延迟。

## 9. 后续实现事项

| 项目 | 待补齐内容 |
|---|---|
| 权威业务状态 | `GameState`、玩家资源、手牌、牌权、局／巡／回合、商店会话与待结算数据 |
| 输入协议 | 操作请求、认证来源、合法性校验、状态版本、超时与掉线回调 |
| 事件调度 | 去重、来源链、队列优先顺序、派发游标、暂停／恢复及循环预算 |
| 事务与成功标准 | 效果执行、费用、计次、消耗、失败处理和结果同步 |
| 稳定标识 | 房间、玩家、牌实例、技能、事件及交互标识；客户端不直接使用可变下标 |
| 数据与计算 | 花色点数编码、基数倍率、舍入规则，以及与付费加强的接入 |
| 验证与性能 | 真正实现后检查典型连锁、并发购物、跨玩家效果、重试、重连和延迟数据 |

后续的代码设计增补直接修改本文件。具体卡牌的玩法、价格与平衡数值仍按游戏规则和卡池设计审定。
