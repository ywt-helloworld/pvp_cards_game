# 事件与状态机说明

本轮统一采用用户最新命名：事件类 `Event`、状态机类 `Condition`、技能事件处理器类 `Handle_Event`。本文定义接口与责任，具体对局引擎尚未实现。

## 1. 调用关系

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

技能接口和响应时机见 [技能说明](ability-system.md)，功能卡实例与容器见 [功能卡说明](function-cards.md)。

## 2. Event 与 Context

事件分为两类，均使用 `Event`：

- `Signal`：通知一个对局时机已经到达，或承载已校验的主动请求。`Condition` 为它选择候选处理器。
- `Effect`：处理器返回的具体效果请求，例如增加分数、修正待结算分数、修正报价。`Condition` 校验并执行，不把它再次当作原始出牌事件派发。

`Timing` 仍沿用现有的 34 个时机，取值不变。时机描述事件的精确边界；`GamePhase` 表示状态机当前所处的大阶段，两者分别记录。`EffectData.value` 的含义由 `EffectType` 决定，实际效果字段随卡牌补齐。

`Context` 包含只读的 `event`、`game`，以及当前处理器的 `owner`。需要事件类型、行为者或主动目标时，读取 `context.event`；当前待结算奖励和报价由 `Condition` 生成快照，分别放在 `context.reward`、`context.shop`。

原始事件及其参数在派发期间保持不变。每次轮到一个处理器，`Condition` 基于最新待结算数据重新生成上下文；前一个处理器返回的合法效果完成后，下一个处理器可以看到更新结果。不能让多个独立快照各自写回，覆盖其他处理器的结果。

主动技能请求必须来自该牌持有者，指定目标牌和技能，并处于允许窗口；`Condition` 验证后只调用选定处理器。效果执行后产生的新时机通知属于被动 `Signal`，原始主动请求的来源另外保留，不自动要求所有后续技能也为主动技。

## 3. 状态与转移

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

最后一手出空时仍锁定赢家与手牌快照，完成该手对应事件链后进入局末，不再摸牌或开始下一巡；规则见 [技能结算顺序](ability-system.md#3-结算顺序与边界)。

## 4. 队列、返回值与结算

`Condition::on_event(Event)` 是统一接收入口。首次进入时启动处理循环；处理期间收到的新事件先入队，避免 C++ 递归调用不断加深。

同一通知按玩家、槽位、牌内处理器顺序执行。状态机为原通知保留派发进度：一个处理器返回事件后，优先完成其后续事件链，再生成下一个处理器的 `Context`。事件队列和待恢复的派发进度共同调度，后续阶段等待当前链完成。示例类的队列是接口骨架，派发游标与恢复结构待实现时补齐。

`Handle_Event::handle_event(const Context&)` 返回 `std::optional<Event>`：

- 空值：没有输出事件，继续其他候选。
- 新事件：`Condition` 补入事件 ID、父事件 ID、根事件 ID及具体处理器来源，再校验和处理。
- 返回了事件但效果执行失败：不视为成功发动，不发奖或扣成功次数；是否采用“尝试即消耗”由具体技能规则明确。

`Condition` 根据效果真正成功的结果记录次数、处理消耗，并在允许时产生 `AbilityResolved` 等后续通知。返回事件本身不代表效果已到账。`Effect` 只在相应阶段可执行，例如 `MultiplyPendingScore` 必须指向仍在处理的奖励结算；报价查询中不得接受直接发放金币或分数的效果。

真实事件 ID 用于去重，后续事件带来源链。同一事件对同一处理器至多派发一次；因果链不得重新进入尚未完成的处理器。状态机需设置有限的事件链处理预算，检测到循环或超限时停止该链并记录原因，具体上限待实际卡牌确定。

## 5. C++17 接口草案

公共枚举、事件参数、上下文和状态机集中定义于此；技能和功能卡接口在各自文档中引用它们。

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

## 6. 示例链

“自己的本手出牌分数翻倍”的处理器流程为：

```text
Condition 收到 PlayRewardBefore 通知
  → 生成 Context，包含当前本手分数
  → DoubleScoreHandler 返回 MultiplyPendingScore 效果事件，value = 2
  → Condition 校验目标与结算阶段，将待结算分数乘 2
  → 生成下一个处理器的 Context，继续该手结算
  → 所有处理完成后，按最终金额发放本手奖励
```

处理器示例代码见 [技能说明](ability-system.md#7-事件处理示例)。正式卡牌及数值尚未确定。
