# 代码设计

所有代码设计统一维护在根目录 `design.md`，后续的类定义、接口、事件协议、状态机、调度与联机实现说明都更新在此。游戏规则见 [README.md](README.md)，卡牌参考资料见 [设计调研](docs/function-card-research.md)。

本文为 C++17 接口与架构草案；示例通过语法检查不表示事件循环、业务操作或网络服务已经实现。当前命名统一为 `Event`、`Condition`、`Context`、`Handle_Event`、`FunctionCards`、`Poker` 和 `EventListener`。

事件数据模型已生成在 [include/Event.hpp](include/Event.hpp)，包含 `Event` 抽象基类、四类中间基类、固定类型通知模板及 132 个具体事件，并保留参数与事件注释。头文件只带上事件字段所需的枚举、ID 和轻量引用；其他模块的代码仍留在本文设计草案中。包含此头文件即可使用事件类型，无需链接单独的 `.cpp` 文件。

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

## 1. 总体架构

```text
网络操作 / 定时器 / 连接回调 / 业务动作
  → EventListener 归一输入或发布可信事件
  → Condition 接收并校验：商店请求排队，局内普通操作直接进入当前结算
  → 更新本房间共享 Context，向选定的 Handle_Event 提供只读视图
  → 返回一个新 Event，或 nullptr
  → Condition 执行合法效果，生成后续事件并继续调度
  → 完成结算，向相关客户端同步结果
```

每个房间拥有一套 `Condition` 和权威 `GameState`，商店、开局、回合、技能、局末和结束均进入同一条事件处理路径。功能卡拥有处理器，状态机负责选择和调用。业务模块提交事件，阶段和操作权限的判断集中在状态机。

`GameState` 统一保存所有玩家的手牌、分数、金币、功能卡和状态；`Condition` 保存流程阶段、商店请求队列、当前结算栈、待处理交互与派发进度。商店队列缓存并行到达的操作；结算栈保存当前操作产生的内部事件及恢复位置，局内不缓存后续普通操作。历史事件记录另行管理，服务端决定操作是否成立。

## 2. 事件与状态机

本轮统一采用用户最新命名：事件类 `Event`、状态机类 `Condition`、技能事件处理器类 `Handle_Event`。本文定义接口与责任，具体对局引擎尚未实现。

### 2.1 调用关系

```text
Event
  → Condition 接收，检查当前状态并更新共享 Context
  → 选择功能卡中的 Handle_Event，调用 handle_event(context)
  → 返回一个新 Event，或 nullptr
  → Condition 将新 Event 纳入当前结算链，执行效果或继续派发
  → 当前事件链完成后继续原事件、后续阶段与状态转移
```

`Handle_Event` 返回事件，`Condition` 负责继续驱动。处理器不自行调用 `Condition::on_event` 或递归调用其他处理器，也不直接修改游戏资源；效果用返回事件表达。`nullptr` 表示本处理器没有输出，状态机继续处理其他候选，当前操作的全部结算帧完成后结束该次结算。

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

所有事件继承抽象基类 `Event`，按职责分为 `RequestEvent`、`SignalEvent`、`EffectEvent`、`ControlEvent` 四类中间基类，再为每个具体事件定义具名派生类。完整清单见 [全部事件清单与派生类](#27-全部事件清单与派生类)。

- Request：玩家意图或查询，先验证，不代表行为已经成功。
- Signal：服务端确认的业务事实，或已获准的一次主动技能调用，按时机匹配处理器。
- Effect：业务流程或处理器提出的具体效果请求，由 Condition 校验并执行。
- Control：连接、计时器、拒绝和同步结果，不直接当作成功行为触发功能卡。

`EventType` 是可被技能订阅的业务通知类型编号，每一项对应一个具体 `SignalEvent`；`Event` 保存这次事件的玩家、目标和其他参数。技能通过 `event_type()` 和 `eventTypes` 掩码订阅通知，`EventKind` 继续区分 Request、Signal、Effect、Control 四类职责。

原有 34 个 `EventType` 编号保持不变，新增 29 个业务通知追加在末尾，`TOTAL_EVENT_TYPES = 63`。只有 Signal 参与技能事件类型匹配；阶段 `GamePhase` 仍由 Condition 集中维护。请求、控制和效果各自拥有专属参数，不再把所有参数塞进一个平铺 Event。

每个房间的 Condition 只创建一份 `Context` 并反复更新；单房间运行时总共只有一份。它是该房间全局共享的调用现场，通过 `Condition::context()` 提供只读访问，不使用跨房间的进程级可变全局对象。

Condition 在调用每个处理器之前，直接更新上下文的当前事件、接收技能、待结算奖励与报价快照。处理器只能接收 `const Context&`，不能改业务状态；前一个处理器返回的效果执行后，再刷新同一个 Context 给下一个处理器。

Context 的对象生命周期属于 Condition，内容绑定只在当前一次处理器调用期间有效。处理器返回后清理绑定；等待选择时保存事件、交互 ID 与结算进度，恢复时重新填充上下文，不保存旧 Context 引用。父事件在子链完成后重新绑定，避免玩家、事件和目标串用。

主动请求使用 `ActivateAbilityRequest`，服务端验证后根据窗口生成 `ShopAbilityActivationEvent` 或 `TurnAbilityActivationEvent`，只调用选定技能。效果完成后产生的新业务通知默认是被动通知，来源技能记录在事件元数据中。

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

`Condition::on_event(EventPtr)` 是统一接收入口，按事件来源与当前状态采用以下接收方式：

| 输入 | 接收方式 |
|---|---|
| 商店的购买、刷新等普通操作 | 初步检查来源后进入商店队列，出队时再校验窗口、目标、余额与报价 |
| 局内出牌、不出牌或主动技能操作 | 仅在当前玩家的对应窗口开放时接受，立即启动当前结算，不保存到下一回合再执行 |
| 当前操作尚未完成时到达的其他普通出牌请求 | 拒绝并同步当前状态，不缓存为之后的行动 |
| 目标选择、超时、掉线与重连等控制事件 | 按当前待处理交互和状态版本校验，允许恢复或结束对应流程 |
| 处理器返回的内部事件 | 纳入当前操作的结算链，先完成后续效果，再恢复原事件 |

商店使用请求队列，是因为多个玩家的操作可以同时到达；局内普通操作采用“接受一个，完整结算一个”，不建立未来行动的请求队列。请求已经入商店队列也不表示交易已成功，实际执行时必须重新校验。

当前操作内部的事件仍需要顺序和恢复位置。使用循环与显式结算栈：同一通知按玩家、槽位、牌内处理器顺序执行，先保存父通知的处理位置；某处理器返回事件后压入子结算帧，优先执行子链，结束后弹出并继续父通知。这个栈只服务于当前操作的连锁，不保存其他玩家尚未获准的操作，也不依赖 C++ 递归调用。

`nullptr` 只表示当前处理器没有输出，不能据此跳过同一通知中的其他候选。只有当前操作的全部事件、候选处理器和必要交互完成，才按规则重新开放操作窗口或推进回合。结算需要等待选择时，保存相应帧和交互信息；挂起与恢复结构待实现时补齐。

`Handle_Event::handle_event(const Context&)` 返回 `EventPtr`：

- 空值：没有输出事件，继续其他候选。
- 新事件指针：Condition 接管唯一所有权，补入事件 ID、父事件 ID、根事件 ID与具体处理器来源，再校验和处理。
- 返回了事件但效果执行失败：不视为成功发动，不发奖或扣成功次数；是否采用“尝试即消耗”由具体技能规则明确。

`Condition` 根据效果真正成功的结果记录次数、处理消耗，并在允许时产生 `AbilityResolved` 等后续通知。返回事件本身不代表效果已到账。`Effect` 只在相应阶段可执行，例如 `MultiplyPendingScoreEvent` 必须指向仍在处理的奖励结算；报价查询中不得接受直接发放金币或分数的效果。

真实事件 ID 用于去重，后续事件带来源链。同一事件对同一处理器至多派发一次；因果链不得重新进入尚未完成的处理器。状态机需设置有限的事件链处理预算，检测到循环或超限时停止该链并记录原因，具体上限待实际卡牌确定。

### 2.5 C++17 接口草案

公共枚举、事件参数、上下文和状态机集中定义于此；技能和功能卡接口在后续章节引用它们。

```cpp
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

enum class Players : std::uint8_t { P1, P2, P3, P4 };
enum class AbilityType : std::uint8_t { Active, Passive };
enum class PlayerScope : std::uint8_t { Self, Others, Any };

enum class EventType : std::size_t {
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
    ShopSlotLocked = 34,
    ShopSlotUnlocked = 35,
    BoosterPackOpened = 36,
    BoosterPackChoiceCompleted = 37,
    BoosterPackSkipped = 38,
    ExpansionCardUsed = 39,
    HandPatternUpgraded = 40,
    FacilityPurchased = 41,
    CoinsChanged = 42,
    ScoreChanged = 43,
    PlayerStatusChanged = 44,
    AbilityAdded = 45,
    AbilityRemoved = 46,
    AbilityAbolished = 47,
    AbilityUsabilityChanged = 48,
    ChoiceOpened = 49,
    ChoiceResolved = 50,
    ChoiceExpired = 51,
    RoundWinnerLocked = 52,
    DrawPileExhausted = 53,
    AllShopsClosed = 54,
    FunctionCardUsabilityChanged = 55,
    FunctionCardAbolished = 56,
    AbilityStateChanged = 57,
    DelayedEffectScheduled = 58,
    DelayedEffectTriggered = 59,
    DelayedEffectExpired = 60,
    CoatingApplied = 61,
    CardVisibilityChanged = 62,
    Count = 63
};

inline constexpr std::size_t TOTAL_EVENT_TYPES =
    static_cast<std::size_t>(EventType::Count);

using EventId = std::uint64_t;
using RequestId = std::uint64_t;
using RoomId = std::uint64_t;
using ShopId = std::uint64_t;
using SlotId = std::uint64_t;
using ProductId = std::uint64_t;
using PackId = std::uint64_t;
using ChoiceId = std::uint64_t;
using ChoiceOptionId = std::uint64_t;
using CardId = std::uint64_t;
using DefinitionId = std::uint64_t;
using StatusId = std::uint64_t;
using CounterId = std::uint64_t;
using DelayedEffectId = std::uint64_t;
using RoundId = std::uint64_t;
using TrickId = std::uint64_t;
using TurnId = std::uint64_t;
using PlayedHandId = std::uint64_t;
using SettlementId = std::uint64_t;
using RoundResultId = std::uint64_t;
using SnapshotId = std::uint64_t;
using PreviewId = std::uint64_t;
using ScoreValue = std::int64_t;
using CoinValue = std::int64_t;

enum class GamePhase : std::uint8_t {
    NotStarted, Shopping, RoundStarting,
    Playing, RoundEnding, Finished
};

// 原 Signal/Effect 的数值保留，新增类别追加。
enum class EventKind : std::uint8_t {
    Signal = 0, Effect = 1, Request = 2, Control = 3
};

enum class EventOrigin : std::uint8_t { Server, Client, Timer, Connection };
enum class ExecutionMode : std::uint8_t { Commit, Preview };
enum class HandPattern : std::uint8_t { Single, Pair, Straight, Bomb, StraightFlush };
enum class Suit : std::uint8_t { Spades, Hearts, Clubs, Diamonds };
enum class PassReason : std::uint8_t { Chosen, Timeout, Disconnected };
enum class DrawReason : std::uint8_t { TrickEnd, Skill, Swap };
enum class RemovalReason : std::uint8_t { Sold, Replaced, Consumed, Deleted };
enum class ChoiceKind : std::uint8_t { Booster, Wildcard, Target, Option };
enum class ChoiceReason : std::uint8_t { Manual, Timeout, Disconnected };
enum class CoatingType : std::uint8_t { Gold, Laser };
enum class ContinueMode : std::uint8_t { ExtraThree, Endless };
enum class ShopOperation : std::uint8_t { Purchase, Sale, Refresh, HandUpgrade, Facility };
enum class RefreshMode : std::uint8_t { Paid, Free };
enum class Lifetime : std::uint8_t { Event, Turn, Trick, Round, Shop, Game };
enum class RewardComponent : std::uint8_t { Base, Extra };
enum class RoundingRule : std::uint8_t { Floor, Ceil, Nearest };
enum class CounterOperation : std::uint8_t { Add, Set };

struct CardRef {
    Players owner = Players::P1;
    std::size_t seq = 0;
};

struct AbilityRef {
    CardRef card;
    std::size_t seq = 0;  // 牌内稳定技能编号，不是 vector 下标
};

struct WildcardAssignment {
    CardId cardId = 0;
    int rank = 0;
    Suit suit = Suit::Spades;
};

struct TargetSpec {
    std::vector<Players> players;
    std::vector<CardId> cards;
    std::vector<AbilityRef> abilities;
    std::optional<int> rank;
    std::optional<Suit> suit;
};

struct RewardData {
    SettlementId settlementId = 0;
    ScoreValue score = 0;
    CoinValue coins = 0;
};

struct ShopData {
    SettlementId settlementId = 0;
    CoinValue baseCost = 0;
    CoinValue finalCost = 0;
    std::uint64_t priceVersion = 0;
};

struct EventMeta {
    RoomId roomId = 0;
    EventId eventId = 0;
    std::optional<EventId> parentEventId;
    EventId rootEventId = 0;
    std::optional<Players> actor;  // 行为发起者；全局事件可以为空
    std::optional<AbilityRef> originAbility;
    std::uint64_t stateVersion = 0;
    EventOrigin origin = EventOrigin::Server;
    ExecutionMode mode = ExecutionMode::Commit;
};

class Event {
public:
    virtual ~Event() = default;
    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;

    virtual EventKind kind() const noexcept = 0;
    virtual std::optional<EventType> event_type() const noexcept {
        return std::nullopt;
    }
    const char* name() const noexcept { return name_; }

    EventMeta meta;  // 由可信入口和 Condition 填写，不信任客户端自报值

protected:
    explicit Event(const char* name) : name_(name) {}

private:
    const char* name_;  // 派生类传入固定字符串
};  // 所有事件的抽象基类，统一提供元数据、事件类别和虚析构接口。

using EventPtr = std::unique_ptr<Event>;

class RequestEvent : public Event {
public:
    EventKind kind() const noexcept final { return EventKind::Request; }
    RequestId requestId = 0;
    std::uint64_t expectedVersion = 0;

protected:
    explicit RequestEvent(const char* name) : Event(name) {}
};  // 操作请求的中间基类，携带请求编号和预期版本，由 Condition 校验后处理。

class SignalEvent : public Event {
public:
    EventKind kind() const noexcept final { return EventKind::Signal; }
    virtual std::optional<EventType> event_type() const noexcept override = 0;
    AbilityType source() const noexcept { return source_; }

protected:
    explicit SignalEvent(const char* name,
                         AbilityType source = AbilityType::Passive)
        : Event(name), source_(source) {}

private:
    AbilityType source_;
};  // 业务通知的中间基类，提供事件类型与主动、被动来源，供技能匹配。

// 只复用固定事件类型的实现；每个具体通知仍是独立的具名派生类。
template<EventType Type>
class TypedSignalEvent : public SignalEvent {
public:
    std::optional<EventType> event_type() const noexcept final { return Type; }

protected:
    explicit TypedSignalEvent(const char* name,
                              AbilityType source = AbilityType::Passive)
        : SignalEvent(name, source) {}
};  // 固定类型通知模板，复用 event_type() 实现；每种业务通知仍有独立派生类。

class EffectEvent : public Event {
public:
    EventKind kind() const noexcept final { return EventKind::Effect; }

protected:
    explicit EffectEvent(const char* name) : Event(name) {}
};  // 效果请求的中间基类，由 Condition 校验并执行，成功后产生对应业务通知。

class ControlEvent : public Event {
public:
    EventKind kind() const noexcept final { return EventKind::Control; }

protected:
    explicit ControlEvent(const char* name) : Event(name) {}
};  // 控制事件的中间基类，承载连接、计时器和服务端反馈，不直接作为技能成功行为。

class GameState;  // 视为已有业务模型，不在本次重新实现
class Condition;

class Context {
public:
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    const GameState& game() const noexcept { return game_; }
    const Event* event() const noexcept { return event_; }
    std::optional<Players> owner() const noexcept {
        if (!receiver_) return std::nullopt;
        return receiver_->card.owner;
    }
    const AbilityRef* receiver() const noexcept {
        return receiver_ ? &*receiver_ : nullptr;
    }
    const RewardData* reward() const noexcept {
        return reward_ ? &*reward_ : nullptr;
    }
    const ShopData* shop() const noexcept {
        return shop_ ? &*shop_ : nullptr;
    }

    template<class T>
    const T* event_as() const noexcept {
        return dynamic_cast<const T*>(event_);
    }

private:
    friend class Condition;
    explicit Context(const GameState& game) : game_(game) {}

    const GameState& game_;
    const Event* event_ = nullptr;
    std::optional<AbilityRef> receiver_;
    std::optional<RewardData> reward_;
    std::optional<ShopData> shop_;
};

struct ResolutionFrame {
    EventPtr event;
    std::vector<AbilityRef> candidates;  // 本通知冻结后的有序候选
    std::size_t nextHandler = 0;
};

class Condition {
public:
    explicit Condition(GameState& game) : game_(game), context_(game) {}

    // 商店普通请求排队；局内合法操作直接启动结算。
    // 控制或授权选择单独校验；内部效果属于当前结算链。
    void on_event(EventPtr event);
    GamePhase phase() const { return phase_; }
    const Context& context() const noexcept { return context_; }

private:
    bool can_accept(const Event& event) const;
    void resolve(EventPtr event);
    void handle_request(const RequestEvent& event);
    void handle_control(const ControlEvent& event);
    void bind_context(const Event& event, const AbilityRef& receiver);
    void clear_context() noexcept;
    void dispatch_next_handler();
    bool apply_effect(const EffectEvent& event);
    void finish_event(const Event& event);

    GameState& game_;
    Context context_;  // 每个房间仅此一份，Condition 直接更新
    GamePhase phase_ = GamePhase::NotStarted;
    std::deque<EventPtr> shopEvents_;  // 只保存商店普通 Request
    std::vector<ResolutionFrame> resolutionStack_;
    bool processing_ = false;
};
```

`Condition::on_event(EventPtr)` 接管事件所有权。`resolve` 推进当前结算栈；`handle_request` 校验操作；`handle_control` 处理连接、计时器和反馈；`apply_effect` 执行效果；`finish_event` 完成当前帧并按规则推进流程。具体业务对象视为已有，只约定接入接口，不在本次实现它们。

`bind_context` 由 Condition 直接更新唯一 Context 的私有字段，填入当前帧的 Event 指针、接收技能与最新待结算快照；`clear_context` 在处理器返回后清空事件、接收者和可选参数。Context 禁止复制，其他模块通过只读访问器读取，不能把调用现场保存为以后使用的状态。

Event 由队列或结算帧中的 `unique_ptr` 拥有，即使容器移动，其派生对象地址也保持稳定。释放或弹出事件帧前必须先清理上下文绑定。所有返回事件按值保存 ID、目标和参数，不能包含指向当前 Context 的指针或引用。

`Event` 是抽象基类，不能使用 `EventPtr` 之外的旧式 `optional<Event>` 或 `deque<Event>` 按值承载派生对象；返回空指针表示没有输出。Event 的虚析构保证基类指针正确销毁派生对象。`processing_` 只表示调度循环正在运行，不能代替操作窗口和挂起交互检查。

### 2.6 示例链

“自己的本手出牌分数翻倍”的处理器流程为：

```text
Condition 收到 PlayRewardBefore 通知
  → 更新共享 Context，包含当前本手分数
  → DoubleScoreHandler 返回 MultiplyPendingScoreEvent，倍率为 2/1
  → Condition 校验目标与结算阶段，将待结算分数乘 2
  → 刷新同一个 Context 给下一个处理器，继续该手结算
  → 所有处理完成后，按最终金额发放本手奖励
```

处理器示例代码见 [技能说明](#37-事件处理示例)。正式卡牌及数值尚未确定。

### 2.7 全部事件清单与派生类

本清单以当前已经确定的游戏规则和通用技能能力为边界，共 **132 个具体事件**。新增机制继续追加清单和派生类，不把尚未发生的成功行为当作客户端请求。玩家、牌局、牌型判定、商店、随机、选择交互等模块均视为已有；这里设计它们之间的事件接口。

| 类别 | 数量 | 主要来源 | 是否直接触发功能卡 |
|---|---:|---|---|
| Request | 21 | 玩家操作和只读查询 | 先校验，再产生对应 Signal |
| Signal | 63 | Condition 确认的业务事实或允许的主动调用 | 按 EventType 与主动／被动类型匹配 |
| Effect | 38 | 已校验业务流程或 Handle_Event 返回值 | 先执行，成功后产生对应 Signal |
| Control | 10 | 连接、计时器和服务端反馈 | 不直接作为成功行为触发卡牌 |

事件的公共字段在 `EventMeta` 中，客户端请求额外带 `requestId` 和预期版本；下表只列专属参数。ID、引用及枚举为接口约定，已有同名定义时直接复用，不重复构建玩家等业务类。默认构造仅便于创建，必需 ID 和参数仍由 Condition 校验，零值不代表合法对象。

每个表项都对应下面的独立派生类。共同的四类父类和 `TypedSignalEvent` 只消除重复代码；事件参数仍在各个具体类中声明。没有使用统一的 `void*` 或任意类型参数包。

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
class BuyShopItemRequest final : public RequestEvent {
public:
    BuyShopItemRequest() : RequestEvent("BuyShopItemRequest") {}
    ShopId shopId{};  // 商店会话
    SlotId slotId{};  // 货位
    ProductId productId{};  // 商品
};  // 玩家请求：购买普通货架商品；商店队列；价格由服务端计算。

class BuyBoosterPackRequest final : public RequestEvent {
public:
    BuyBoosterPackRequest() : RequestEvent("BuyBoosterPackRequest") {}
    ShopId shopId{};  // 商店会话
    PackId packId{};  // 展示中的卡包实例
};  // 玩家请求：购买并立即打开卡包；商店队列；验证空槽与包限购，扣款后立即开包。

class SellFunctionCardRequest final : public RequestEvent {
public:
    SellFunctionCardRequest() : RequestEvent("SellFunctionCardRequest") {}
    ShopId shopId{};  // 商店会话
    CardRef card{};  // 功能卡实例
};  // 玩家请求：出售功能卡；商店队列；出售成本和成长清理按既有规则。

class RefreshShopRequest final : public RequestEvent {
public:
    RefreshShopRequest() : RequestEvent("RefreshShopRequest") {}
    ShopId shopId{};  // 商店会话
};  // 玩家请求：刷新普通货架；商店队列；出队时检查刷新上限及当前报价。

class LockShopSlotRequest final : public RequestEvent {
public:
    LockShopSlotRequest() : RequestEvent("LockShopSlotRequest") {}
    ShopId shopId{};  // 商店会话
    SlotId slotId{};  // 货位
};  // 玩家请求：锁定货位；由 Condition 校验当前状态、权限和参数后处理。

class UnlockShopSlotRequest final : public RequestEvent {
public:
    UnlockShopSlotRequest() : RequestEvent("UnlockShopSlotRequest") {}
    ShopId shopId{};  // 商店会话
    SlotId slotId{};  // 货位
};  // 玩家请求：解锁货位；由 Condition 校验当前状态、权限和参数后处理。

class ChooseBoosterCardRequest final : public RequestEvent {
public:
    ChooseBoosterCardRequest() : RequestEvent("ChooseBoosterCardRequest") {}
    ChoiceId choiceId{};  // 待处理交互
    ChoiceOptionId optionId{};  // 选中的候选
};  // 玩家请求：选择卡包候选；授权选择回复，不作为下一次普通购物排队。

class SkipBoosterPackRequest final : public RequestEvent {
public:
    SkipBoosterPackRequest() : RequestEvent("SkipBoosterPackRequest") {}
    ChoiceId choiceId{};  // 待处理交互
};  // 玩家请求：放弃已打开卡包；授权选择回复；不退已支付包价。

class UpgradeHandPatternRequest final : public RequestEvent {
public:
    UpgradeHandPatternRequest() : RequestEvent("UpgradeHandPatternRequest") {}
    ShopId shopId{};  // 商店会话
    HandPattern pattern{};  // 牌型
};  // 玩家请求：付费加强牌型；由 Condition 校验当前状态、权限和参数后处理。

class BuyFacilityRequest final : public RequestEvent {
public:
    BuyFacilityRequest() : RequestEvent("BuyFacilityRequest") {}
    ShopId shopId{};  // 商店会话
    DefinitionId facilityId{};  // 设施定义
};  // 玩家请求：购买设施；由 Condition 校验当前状态、权限和参数后处理。

class FinishShoppingRequest final : public RequestEvent {
public:
    FinishShoppingRequest() : RequestEvent("FinishShoppingRequest") {}
    ShopId shopId{};  // 商店会话
};  // 玩家请求：完成本次购物；由 Condition 校验当前状态、权限和参数后处理。

class PlayCardsRequest final : public RequestEvent {
public:
    PlayCardsRequest() : RequestEvent("PlayCardsRequest") {}
    TurnId turnId{};  // 所属回合
    std::vector<CardId> cards{};  // 实际扑克牌实例
    std::vector<WildcardAssignment> wildcards{};  // 赖子指定方案
};  // 玩家请求：确认出牌；局内直接；合法提交后才产生出牌成功通知。

class PassRequest final : public RequestEvent {
public:
    PassRequest() : RequestEvent("PassRequest") {}
    TurnId turnId{};  // 所属回合
};  // 玩家请求：主动不出牌；局内直接；先手不能因此跳过领出。

class ActivateAbilityRequest final : public RequestEvent {
public:
    ActivateAbilityRequest() : RequestEvent("ActivateAbilityRequest") {}
    AbilityRef ability{};  // 目标技能
    TargetSpec selection{};  // 目标与方案
};  // 玩家请求：发动主动技能；校验技能归属、窗口、费用与次数；商店和局内依窗口处理。

class SubmitChoiceRequest final : public RequestEvent {
public:
    SubmitChoiceRequest() : RequestEvent("SubmitChoiceRequest") {}
    ChoiceId choiceId{};  // 待处理交互
    std::vector<ChoiceOptionId> options{};  // 选中的合法选项
};  // 玩家请求：提交一般目标或方案；按交互授权接受，允许非当前行动玩家回复。

class ResolveWildcardRequest final : public RequestEvent {
public:
    ResolveWildcardRequest() : RequestEvent("ResolveWildcardRequest") {}
    ChoiceId choiceId{};  // 待处理交互
    std::vector<WildcardAssignment> wildcards{};  // 指定方案
};  // 玩家请求：确认赖子指定；按赖子交互授权接受，确认后仍需校验跟牌。

class ContinueGameRequest final : public RequestEvent {
public:
    ContinueGameRequest() : RequestEvent("ContinueGameRequest") {}
    ContinueMode mode{};  // 续玩模式
};  // 玩家请求：请求加赛或无尽模式；游戏结束后的模式请求；多人一致性规则待既有房间模块确定。

class StartNewGameRequest final : public RequestEvent {
public:
    StartNewGameRequest() : RequestEvent("StartNewGameRequest") {}
};  // 玩家请求：请求开始新一场；按房间流程验证，新游戏重新初始化资源。

class StateSyncRequest final : public RequestEvent {
public:
    StateSyncRequest() : RequestEvent("StateSyncRequest") {}
    std::uint64_t lastSeenVersion{};  // 客户端已知版本
};  // 玩家请求：请求状态同步；只读同步通道，不受出牌请求忙碌门禁限制。

class PreviewPlayRequest final : public RequestEvent {
public:
    PreviewPlayRequest() : RequestEvent("PreviewPlayRequest") {}
    TurnId turnId{};  // 所属回合
    std::vector<CardId> cards{};  // 实际扑克牌实例
};  // 玩家请求：查询选牌的合法方案；只读预览；不提交出牌、不扣次数、不推进实际随机序列。

class QuoteShopOperationRequest final : public RequestEvent {
public:
    QuoteShopOperationRequest() : RequestEvent("QuoteShopOperationRequest") {}
    ShopId shopId{};  // 商店会话
    ShopOperation operation{};  // 报价业务
    std::optional<ProductId> productId{};  // 购买商品
    std::optional<CardRef> card{};  // 出售实例
    std::optional<HandPattern> pattern{};  // 加强牌型
    std::optional<DefinitionId> facilityId{};  // 设施
};  // 玩家请求：查询商店操作报价；只读预览；不因重复查询消耗免费额度。
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
class GameStartedEvent final : public TypedSignalEvent<EventType::GameStart> {
public:
    GameStartedEvent() : TypedSignalEvent<EventType::GameStart>("GameStartedEvent") {}
    std::vector<Players> participants{};  // 实际参局玩家
    int roundCount{};  // 标准局数
};  // 业务通知：整场开始，由 Condition 在对应时机发布，供被动技能响应。

class ShopEnteredEvent final : public TypedSignalEvent<EventType::ShopEnter> {
public:
    ShopEnteredEvent() : TypedSignalEvent<EventType::ShopEnter>("ShopEnteredEvent") {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
};  // 业务通知：进入局前商店，由 Condition 在对应时机发布，供被动技能响应。

class ShopAbilityActivationEvent final : public TypedSignalEvent<EventType::ShopActionWindow> {
public:
    ShopAbilityActivationEvent() : TypedSignalEvent<EventType::ShopActionWindow>("ShopAbilityActivationEvent", AbilityType::Active) {}
    AbilityRef ability{};  // 目标技能
    TargetSpec selection{};  // 目标与方案
};  // 主动调用通知：商店窗口内的主动技能调用，由 Condition 验证请求后只派发给指定技能。

class BeforeShopRefreshEvent final : public TypedSignalEvent<EventType::ShopRefreshQuote> {
public:
    BeforeShopRefreshEvent() : TypedSignalEvent<EventType::ShopRefreshQuote>("BeforeShopRefreshEvent") {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    SettlementId settlementId{};  // 待结算事务
    ShopData initialQuote{};  // 初始报价
};  // 业务通知：刷新报价前，由 Condition 在对应时机发布，供被动技能响应。

class ShopRefreshedEvent final : public TypedSignalEvent<EventType::ShopRefreshAfter> {
public:
    ShopRefreshedEvent() : TypedSignalEvent<EventType::ShopRefreshAfter>("ShopRefreshedEvent") {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    std::vector<SlotId> slots{};  // 实际替换的货位
    CoinValue paid{};  // 实际费用
};  // 业务通知：刷新成功后，由 Condition 在对应时机发布，供被动技能响应。

class BeforeShopPurchaseEvent final : public TypedSignalEvent<EventType::ShopBuyQuote> {
public:
    BeforeShopPurchaseEvent() : TypedSignalEvent<EventType::ShopBuyQuote>("BeforeShopPurchaseEvent") {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    SettlementId settlementId{};  // 待结算事务
    ProductId productId{};  // 商品
    ShopData initialQuote{};  // 初始报价
};  // 业务通知：购买报价前，由 Condition 在对应时机发布，供被动技能响应。

class ShopPurchasedEvent final : public TypedSignalEvent<EventType::ShopBuyAfter> {
public:
    ShopPurchasedEvent() : TypedSignalEvent<EventType::ShopBuyAfter>("ShopPurchasedEvent") {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    ProductId productId{};  // 商品
    CoinValue paid{};  // 实际费用
};  // 业务通知：购买交付成功后，由 Condition 在对应时机发布，供被动技能响应。

class BeforeShopSaleEvent final : public TypedSignalEvent<EventType::ShopSellQuote> {
public:
    BeforeShopSaleEvent() : TypedSignalEvent<EventType::ShopSellQuote>("BeforeShopSaleEvent") {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    CardRef card{};  // 功能卡实例
    SettlementId settlementId{};  // 待结算事务
    ShopData initialQuote{};  // 初始报价
};  // 业务通知：出售报价前，由 Condition 在对应时机发布，供被动技能响应。

class ShopSoldEvent final : public TypedSignalEvent<EventType::ShopSellAfter> {
public:
    ShopSoldEvent() : TypedSignalEvent<EventType::ShopSellAfter>("ShopSoldEvent") {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    CardRef card{};  // 功能卡实例
    CoinValue received{};  // 实际收入
};  // 业务通知：出售成功后，由 Condition 在对应时机发布，供被动技能响应。

class ShopLeftEvent final : public TypedSignalEvent<EventType::ShopLeave> {
public:
    ShopLeftEvent() : TypedSignalEvent<EventType::ShopLeave>("ShopLeftEvent") {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    bool timedOut{};  // 是否商店超时
};  // 业务通知：离店，由 Condition 在对应时机发布，供被动技能响应。

class RoundStartedEvent final : public TypedSignalEvent<EventType::RoundStart> {
public:
    RoundStartedEvent() : TypedSignalEvent<EventType::RoundStart>("RoundStartedEvent") {}
    RoundId roundId{};  // 所属局
    Players firstPlayer{};  // 本局先手
};  // 业务通知：本局开始，由 Condition 在对应时机发布，供被动技能响应。

class BeforeInitialDealEvent final : public TypedSignalEvent<EventType::InitialDealBefore> {
public:
    BeforeInitialDealEvent() : TypedSignalEvent<EventType::InitialDealBefore>("BeforeInitialDealEvent") {}
    RoundId roundId{};  // 所属局
    int cardsPerPlayer = 7;  // 既有规则起手数量
};  // 业务通知：初始发牌前，由 Condition 在对应时机发布，供被动技能响应。

class InitialCardsDealtEvent final : public TypedSignalEvent<EventType::InitialDealAfter> {
public:
    InitialCardsDealtEvent() : TypedSignalEvent<EventType::InitialDealAfter>("InitialCardsDealtEvent") {}
    RoundId roundId{};  // 所属局
};  // 业务通知：全体初始发牌完成，由 Condition 在对应时机发布，供被动技能响应。

class TrickStartedEvent final : public TypedSignalEvent<EventType::TrickStart> {
public:
    TrickStartedEvent() : TypedSignalEvent<EventType::TrickStart>("TrickStartedEvent") {}
    RoundId roundId{};  // 所属局
    TrickId trickId{};  // 所属巡
    Players leader{};  // 领出玩家
};  // 业务通知：本巡开始，由 Condition 在对应时机发布，供被动技能响应。

class TurnStartedEvent final : public TypedSignalEvent<EventType::TurnStart> {
public:
    TurnStartedEvent() : TypedSignalEvent<EventType::TurnStart>("TurnStartedEvent") {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
};  // 业务通知：当前回合开始，由 Condition 在对应时机发布，供被动技能响应。

class TurnAbilityActivationEvent final : public TypedSignalEvent<EventType::PlayActionWindow> {
public:
    TurnAbilityActivationEvent() : TypedSignalEvent<EventType::PlayActionWindow>("TurnAbilityActivationEvent", AbilityType::Active) {}
    TurnId turnId{};  // 所属回合
    AbilityRef ability{};  // 目标技能
    TargetSpec selection{};  // 目标与方案
};  // 主动调用通知：出牌窗口内的主动技能调用，由 Condition 验证请求后只派发给指定技能。

class PlayValidationEvent final : public TypedSignalEvent<EventType::PlayValidate> {
public:
    PlayValidationEvent() : TypedSignalEvent<EventType::PlayValidate>("PlayValidationEvent") {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
    std::vector<CardId> cards{};  // 实际扑克牌实例
    std::vector<WildcardAssignment> wildcards{};  // 指定方案
};  // 业务通知：验证选牌及跟牌，由 Condition 在对应时机发布，供被动技能响应。

class CardsPlayedEvent final : public TypedSignalEvent<EventType::PlayCommitted> {
public:
    CardsPlayedEvent() : TypedSignalEvent<EventType::PlayCommitted>("CardsPlayedEvent") {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
    PlayedHandId handId{};  // 已提交的一手牌
    std::vector<CardId> cards{};  // 实际扑克牌实例
    HandPattern pattern{};  // 最终牌型
};  // 业务通知：成功提交一手牌，由 Condition 在对应时机发布，供被动技能响应。

class BeforePlayRewardEvent final : public TypedSignalEvent<EventType::PlayRewardBefore> {
public:
    BeforePlayRewardEvent() : TypedSignalEvent<EventType::PlayRewardBefore>("BeforePlayRewardEvent") {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
    PlayedHandId handId{};  // 对应出牌
    SettlementId settlementId{};  // 待结算事务
    RewardData initialReward{};  // 原始奖励
};  // 业务通知：计算本手出牌奖励前，由 Condition 在对应时机发布，供被动技能响应。

class PlayRewardGrantedEvent final : public TypedSignalEvent<EventType::PlayRewardAfter> {
public:
    PlayRewardGrantedEvent() : TypedSignalEvent<EventType::PlayRewardAfter>("PlayRewardGrantedEvent") {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
    PlayedHandId handId{};  // 对应出牌
    RewardData reward{};  // 实际到账
};  // 业务通知：本手奖励到账后，由 Condition 在对应时机发布，供被动技能响应。

class PlayerPassedEvent final : public TypedSignalEvent<EventType::PassCommitted> {
public:
    PlayerPassedEvent() : TypedSignalEvent<EventType::PassCommitted>("PlayerPassedEvent") {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
    PassReason reason{};  // 主动、超时或掉线
};  // 业务通知：确认不出牌，由 Condition 在对应时机发布，供被动技能响应。

class TurnEndedEvent final : public TypedSignalEvent<EventType::TurnEnd> {
public:
    TurnEndedEvent() : TypedSignalEvent<EventType::TurnEnd>("TurnEndedEvent") {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
};  // 业务通知：当前回合结算结束，由 Condition 在对应时机发布，供被动技能响应。

class TrickEndedEvent final : public TypedSignalEvent<EventType::TrickEnd> {
public:
    TrickEndedEvent() : TypedSignalEvent<EventType::TrickEnd>("TrickEndedEvent") {}
    TrickId trickId{};  // 所属巡
    Players leader{};  // 牌权获得者
};  // 业务通知：获得牌权并结束本巡，由 Condition 在对应时机发布，供被动技能响应。

class BeforeCardsDrawEvent final : public TypedSignalEvent<EventType::DrawBefore> {
public:
    BeforeCardsDrawEvent() : TypedSignalEvent<EventType::DrawBefore>("BeforeCardsDrawEvent") {}
    Players player{};  // 相关玩家
    SettlementId settlementId{};  // 待结算事务
    int requestedCount{};  // 请求摸牌数
    DrawReason reason{};  // 摸牌原因
};  // 业务通知：非初始摸牌前，由 Condition 在对应时机发布，供被动技能响应。

class CardsDrawnEvent final : public TypedSignalEvent<EventType::DrawAfter> {
public:
    CardsDrawnEvent() : TypedSignalEvent<EventType::DrawAfter>("CardsDrawnEvent") {}
    Players player{};  // 相关玩家
    std::vector<CardId> cards{};  // 实际扑克牌实例
    DrawReason reason{};  // 摸牌原因
};  // 业务通知：实际摸牌完成，由 Condition 在对应时机发布，供被动技能响应。

class TrickDrawingCompletedEvent final : public TypedSignalEvent<EventType::TrickDrawsAfter> {
public:
    TrickDrawingCompletedEvent() : TypedSignalEvent<EventType::TrickDrawsAfter>("TrickDrawingCompletedEvent") {}
    TrickId trickId{};  // 所属巡
    int actualTotal{};  // 全体实际摸牌总数
};  // 业务通知：巡末摸牌步骤完成，由 Condition 在对应时机发布，供被动技能响应。

class RoundEndedEvent final : public TypedSignalEvent<EventType::RoundEnd> {
public:
    RoundEndedEvent() : TypedSignalEvent<EventType::RoundEnd>("RoundEndedEvent") {}
    RoundId roundId{};  // 所属局
    RoundResultId resultId{};  // 已锁定结果
};  // 业务通知：本局结束且最后回合结算完成，由 Condition 在对应时机发布，供被动技能响应。

class BeforeRoundRewardEvent final : public TypedSignalEvent<EventType::RoundRewardBefore> {
public:
    BeforeRoundRewardEvent() : TypedSignalEvent<EventType::RoundRewardBefore>("BeforeRoundRewardEvent") {}
    RoundId roundId{};  // 所属局
    Players player{};  // 相关玩家
    SettlementId settlementId{};  // 待结算事务
    RewardData initialReward{};  // 基础待结算奖励
};  // 业务通知：计算玩家局末奖励前，由 Condition 在对应时机发布，供被动技能响应。

class RoundRewardsGrantedEvent final : public TypedSignalEvent<EventType::RoundRewardAfter> {
public:
    RoundRewardsGrantedEvent() : TypedSignalEvent<EventType::RoundRewardAfter>("RoundRewardsGrantedEvent") {}
    RoundId roundId{};  // 所属局
    RoundResultId resultId{};  // 实际结算记录
};  // 业务通知：全体局末奖励与扣款完成，由 Condition 在对应时机发布，供被动技能响应。

class GameEndedEvent final : public TypedSignalEvent<EventType::GameEnd> {
public:
    GameEndedEvent() : TypedSignalEvent<EventType::GameEnd>("GameEndedEvent") {}
    std::vector<Players> winners{};  // 并列冠军
    std::uint64_t finalVersion{};  // 最终状态版本
};  // 业务通知：整场结束与冠军确认，由 Condition 在对应时机发布，供被动技能响应。

class PlayingCardsChangedEvent final : public TypedSignalEvent<EventType::CardChanged> {
public:
    PlayingCardsChangedEvent() : TypedSignalEvent<EventType::CardChanged>("PlayingCardsChangedEvent") {}
    Players player{};  // 相关玩家
    std::vector<CardId> cards{};  // 实际扑克牌实例
    std::string reason{};  // 变化原因
};  // 业务通知：技能实际改变牌面或手牌后，由 Condition 在对应时机发布，供被动技能响应。

class AbilityResolvedEvent final : public TypedSignalEvent<EventType::AbilityResolved> {
public:
    AbilityResolvedEvent() : TypedSignalEvent<EventType::AbilityResolved>("AbilityResolvedEvent") {}
    AbilityRef ability{};  // 目标技能
    EventId triggerEventId{};  // 触发事件
};  // 业务通知：技能效果成功结算后，由 Condition 在对应时机发布，供被动技能响应。

class FunctionCardEquippedEvent final : public TypedSignalEvent<EventType::FunctionCardEquipped> {
public:
    FunctionCardEquippedEvent() : TypedSignalEvent<EventType::FunctionCardEquipped>("FunctionCardEquippedEvent") {}
    CardRef card{};  // 功能卡实例
    std::size_t slot{};  // 实际槽位
};  // 业务通知：功能卡进入槽位，由 Condition 在对应时机发布，供被动技能响应。

class FunctionCardRemovedEvent final : public TypedSignalEvent<EventType::FunctionCardRemoved> {
public:
    FunctionCardRemovedEvent() : TypedSignalEvent<EventType::FunctionCardRemoved>("FunctionCardRemovedEvent") {}
    CardRef card{};  // 功能卡实例
    RemovalReason reason{};  // 移除原因
};  // 业务通知：功能卡离开槽位，由 Condition 在对应时机发布，供被动技能响应。

class ShopSlotLockedEvent final : public TypedSignalEvent<EventType::ShopSlotLocked> {
public:
    ShopSlotLockedEvent() : TypedSignalEvent<EventType::ShopSlotLocked>("ShopSlotLockedEvent") {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    SlotId slotId{};  // 货位
};  // 业务通知：货位锁定成功，由 Condition 在对应时机发布，供被动技能响应。

class ShopSlotUnlockedEvent final : public TypedSignalEvent<EventType::ShopSlotUnlocked> {
public:
    ShopSlotUnlockedEvent() : TypedSignalEvent<EventType::ShopSlotUnlocked>("ShopSlotUnlockedEvent") {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    SlotId slotId{};  // 货位
};  // 业务通知：货位解锁成功，由 Condition 在对应时机发布，供被动技能响应。

class BoosterPackOpenedEvent final : public TypedSignalEvent<EventType::BoosterPackOpened> {
public:
    BoosterPackOpenedEvent() : TypedSignalEvent<EventType::BoosterPackOpened>("BoosterPackOpenedEvent") {}
    Players player{};  // 相关玩家
    PackId packId{};  // 包实例
    ChoiceId choiceId{};  // 待处理交互
};  // 业务通知：卡包已付费打开，由 Condition 在对应时机发布，供被动技能响应。

class BoosterPackChoiceCompletedEvent final : public TypedSignalEvent<EventType::BoosterPackChoiceCompleted> {
public:
    BoosterPackChoiceCompletedEvent() : TypedSignalEvent<EventType::BoosterPackChoiceCompleted>("BoosterPackChoiceCompletedEvent") {}
    Players player{};  // 相关玩家
    PackId packId{};  // 包实例
    std::vector<ChoiceOptionId> options{};  // 实际选项
};  // 业务通知：卡包候选选取完成，由 Condition 在对应时机发布，供被动技能响应。

class BoosterPackSkippedEvent final : public TypedSignalEvent<EventType::BoosterPackSkipped> {
public:
    BoosterPackSkippedEvent() : TypedSignalEvent<EventType::BoosterPackSkipped>("BoosterPackSkippedEvent") {}
    Players player{};  // 相关玩家
    PackId packId{};  // 包实例
};  // 业务通知：卡包已放弃，由 Condition 在对应时机发布，供被动技能响应。

class ExpansionCardUsedEvent final : public TypedSignalEvent<EventType::ExpansionCardUsed> {
public:
    ExpansionCardUsedEvent() : TypedSignalEvent<EventType::ExpansionCardUsed>("ExpansionCardUsedEvent") {}
    Players player{};  // 相关玩家
    DefinitionId expansionId{};  // 拓展牌定义
    TargetSpec selection{};  // 目标与方案
};  // 业务通知：拓展牌获得即使用完成，由 Condition 在对应时机发布，供被动技能响应。

class HandPatternUpgradedEvent final : public TypedSignalEvent<EventType::HandPatternUpgraded> {
public:
    HandPatternUpgradedEvent() : TypedSignalEvent<EventType::HandPatternUpgraded>("HandPatternUpgradedEvent") {}
    Players player{};  // 相关玩家
    HandPattern pattern{};  // 牌型
    int beforeLevel{};  // 原等级
    int afterLevel{};  // 新等级
};  // 业务通知：牌型加强完成，由 Condition 在对应时机发布，供被动技能响应。

class FacilityPurchasedEvent final : public TypedSignalEvent<EventType::FacilityPurchased> {
public:
    FacilityPurchasedEvent() : TypedSignalEvent<EventType::FacilityPurchased>("FacilityPurchasedEvent") {}
    Players player{};  // 相关玩家
    DefinitionId facilityId{};  // 设施定义
};  // 业务通知：设施购买生效，由 Condition 在对应时机发布，供被动技能响应。

class CoinsChangedEvent final : public TypedSignalEvent<EventType::CoinsChanged> {
public:
    CoinsChangedEvent() : TypedSignalEvent<EventType::CoinsChanged>("CoinsChangedEvent") {}
    Players player{};  // 相关玩家
    CoinValue before{};  // 变化前
    CoinValue after{};  // 变化后
    std::string reason{};  // 收入或支出原因
};  // 业务通知：金币余额实际变化，由 Condition 在对应时机发布，供被动技能响应。

class ScoreChangedEvent final : public TypedSignalEvent<EventType::ScoreChanged> {
public:
    ScoreChangedEvent() : TypedSignalEvent<EventType::ScoreChanged>("ScoreChangedEvent") {}
    Players player{};  // 相关玩家
    ScoreValue before{};  // 变化前
    ScoreValue after{};  // 变化后
};  // 业务通知：累计分数实际变化，由 Condition 在对应时机发布，供被动技能响应。

class PlayerStatusChangedEvent final : public TypedSignalEvent<EventType::PlayerStatusChanged> {
public:
    PlayerStatusChangedEvent() : TypedSignalEvent<EventType::PlayerStatusChanged>("PlayerStatusChangedEvent") {}
    Players player{};  // 相关玩家
    StatusId statusId{};  // 状态实例
    std::string reason{};  // 增加、移除或层数变化
};  // 业务通知：玩家状态实际变化，由 Condition 在对应时机发布，供被动技能响应。

class AbilityAddedEvent final : public TypedSignalEvent<EventType::AbilityAdded> {
public:
    AbilityAddedEvent() : TypedSignalEvent<EventType::AbilityAdded>("AbilityAddedEvent") {}
    AbilityRef ability{};  // 目标技能
};  // 业务通知：技能添加完成，由 Condition 在对应时机发布，供被动技能响应。

class AbilityRemovedEvent final : public TypedSignalEvent<EventType::AbilityRemoved> {
public:
    AbilityRemovedEvent() : TypedSignalEvent<EventType::AbilityRemoved>("AbilityRemovedEvent") {}
    AbilityRef ability{};  // 目标技能
};  // 业务通知：技能逻辑移除完成，由 Condition 在对应时机发布，供被动技能响应。

class AbilityAbolishedEvent final : public TypedSignalEvent<EventType::AbilityAbolished> {
public:
    AbilityAbolishedEvent() : TypedSignalEvent<EventType::AbilityAbolished>("AbilityAbolishedEvent") {}
    AbilityRef ability{};  // 目标技能
};  // 业务通知：技能被永久废除，由 Condition 在对应时机发布，供被动技能响应。

class AbilityUsabilityChangedEvent final : public TypedSignalEvent<EventType::AbilityUsabilityChanged> {
public:
    AbilityUsabilityChangedEvent() : TypedSignalEvent<EventType::AbilityUsabilityChanged>("AbilityUsabilityChangedEvent") {}
    AbilityRef ability{};  // 目标技能
    bool usable{};  // 当前开关
};  // 业务通知：技能临时可用开关变化，由 Condition 在对应时机发布，供被动技能响应。

class ChoiceOpenedEvent final : public TypedSignalEvent<EventType::ChoiceOpened> {
public:
    ChoiceOpenedEvent() : TypedSignalEvent<EventType::ChoiceOpened>("ChoiceOpenedEvent") {}
    Players player{};  // 相关玩家
    ChoiceId choiceId{};  // 待处理交互
    ChoiceKind choiceKind{};  // 交互类型
};  // 业务通知：交互选择窗口建立，由 Condition 在对应时机发布，供被动技能响应。

class ChoiceResolvedEvent final : public TypedSignalEvent<EventType::ChoiceResolved> {
public:
    ChoiceResolvedEvent() : TypedSignalEvent<EventType::ChoiceResolved>("ChoiceResolvedEvent") {}
    Players player{};  // 相关玩家
    ChoiceId choiceId{};  // 待处理交互
    std::vector<ChoiceOptionId> options{};  // 实际选择
    ChoiceReason reason{};  // 手动、超时或掉线
};  // 业务通知：交互选择处理完成，由 Condition 在对应时机发布，供被动技能响应。

class ChoiceExpiredEvent final : public TypedSignalEvent<EventType::ChoiceExpired> {
public:
    ChoiceExpiredEvent() : TypedSignalEvent<EventType::ChoiceExpired>("ChoiceExpiredEvent") {}
    Players player{};  // 相关玩家
    ChoiceId choiceId{};  // 待处理交互
};  // 业务通知：交互期限已到并完成默认处理，由 Condition 在对应时机发布，供被动技能响应。

class RoundWinnerLockedEvent final : public TypedSignalEvent<EventType::RoundWinnerLocked> {
public:
    RoundWinnerLockedEvent() : TypedSignalEvent<EventType::RoundWinnerLocked>("RoundWinnerLockedEvent") {}
    RoundId roundId{};  // 所属局
    Players winner{};  // 赢家
    RoundResultId resultId{};  // 冻结结果
};  // 业务通知：出空时锁定赢家与手牌快照，由 Condition 在对应时机发布，供被动技能响应。

class DrawPileExhaustedEvent final : public TypedSignalEvent<EventType::DrawPileExhausted> {
public:
    DrawPileExhaustedEvent() : TypedSignalEvent<EventType::DrawPileExhausted>("DrawPileExhaustedEvent") {}
    RoundId roundId{};  // 所属局
};  // 业务通知：牌堆由非空变为空，由 Condition 在对应时机发布，供被动技能响应。

class AllShopsClosedEvent final : public TypedSignalEvent<EventType::AllShopsClosed> {
public:
    AllShopsClosedEvent() : TypedSignalEvent<EventType::AllShopsClosed>("AllShopsClosedEvent") {}
    RoundId roundId{};  // 所属局
};  // 业务通知：全体商店会话关闭且交易结清，由 Condition 在对应时机发布，供被动技能响应。

class FunctionCardUsabilityChangedEvent final : public TypedSignalEvent<EventType::FunctionCardUsabilityChanged> {
public:
    FunctionCardUsabilityChangedEvent() : TypedSignalEvent<EventType::FunctionCardUsabilityChanged>("FunctionCardUsabilityChangedEvent") {}
    CardRef card{};  // 功能卡实例
    bool usable{};  // 当前开关
};  // 业务通知：整张功能卡临时开关变化，由 Condition 在对应时机发布，供被动技能响应。

class FunctionCardAbolishedEvent final : public TypedSignalEvent<EventType::FunctionCardAbolished> {
public:
    FunctionCardAbolishedEvent() : TypedSignalEvent<EventType::FunctionCardAbolished>("FunctionCardAbolishedEvent") {}
    CardRef card{};  // 功能卡实例
};  // 业务通知：整张功能卡被废除，由 Condition 在对应时机发布，供被动技能响应。

class AbilityStateChangedEvent final : public TypedSignalEvent<EventType::AbilityStateChanged> {
public:
    AbilityStateChangedEvent() : TypedSignalEvent<EventType::AbilityStateChanged>("AbilityStateChangedEvent") {}
    AbilityRef ability{};  // 目标技能
    CounterId counterId{};  // 已定义的计数状态
    int before{};  // 变化前
    int after{};  // 变化后
};  // 业务通知：技能成长或蓄力状态变化，由 Condition 在对应时机发布，供被动技能响应。

class DelayedEffectScheduledEvent final : public TypedSignalEvent<EventType::DelayedEffectScheduled> {
public:
    DelayedEffectScheduledEvent() : TypedSignalEvent<EventType::DelayedEffectScheduled>("DelayedEffectScheduledEvent") {}
    Players player{};  // 相关玩家
    DelayedEffectId delayedEffectId{};  // 标记实例
};  // 业务通知：延迟效果标记建立，由 Condition 在对应时机发布，供被动技能响应。

class DelayedEffectTriggeredEvent final : public TypedSignalEvent<EventType::DelayedEffectTriggered> {
public:
    DelayedEffectTriggeredEvent() : TypedSignalEvent<EventType::DelayedEffectTriggered>("DelayedEffectTriggeredEvent") {}
    Players player{};  // 相关玩家
    DelayedEffectId delayedEffectId{};  // 标记实例
};  // 业务通知：延迟标记实际执行完成，由 Condition 在对应时机发布，供被动技能响应。

class DelayedEffectExpiredEvent final : public TypedSignalEvent<EventType::DelayedEffectExpired> {
public:
    DelayedEffectExpiredEvent() : TypedSignalEvent<EventType::DelayedEffectExpired>("DelayedEffectExpiredEvent") {}
    Players player{};  // 相关玩家
    DelayedEffectId delayedEffectId{};  // 标记实例
};  // 业务通知：延迟标记结束或失效，由 Condition 在对应时机发布，供被动技能响应。

class CoatingAppliedEvent final : public TypedSignalEvent<EventType::CoatingApplied> {
public:
    CoatingAppliedEvent() : TypedSignalEvent<EventType::CoatingApplied>("CoatingAppliedEvent") {}
    CardRef card{};  // 功能卡实例
    CoatingType coating{};  // 镀层
};  // 业务通知：镀层施加完成，由 Condition 在对应时机发布，供被动技能响应。

class CardVisibilityChangedEvent final : public TypedSignalEvent<EventType::CardVisibilityChanged> {
public:
    CardVisibilityChangedEvent() : TypedSignalEvent<EventType::CardVisibilityChanged>("CardVisibilityChangedEvent") {}
    Players viewer{};  // 获得信息的玩家
    std::vector<CardId> cards{};  // 实际扑克牌实例
};  // 业务通知：查看权限或公开信息变化，由 Condition 在对应时机发布，供被动技能响应。
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
class AddScoreEvent final : public EffectEvent {
public:
    AddScoreEvent() : EffectEvent("AddScoreEvent") {}
    Players target{};  // 效果目标玩家
    ScoreValue amount{};  // 增加数量
};  // 效果请求：增加实际分数；由 Condition 校验并执行。

class AddCoinsEvent final : public EffectEvent {
public:
    AddCoinsEvent() : EffectEvent("AddCoinsEvent") {}
    Players target{};  // 效果目标玩家
    CoinValue amount{};  // 增加数量
};  // 效果请求：增加实际金币；由 Condition 校验并执行。

class SpendCoinsEvent final : public EffectEvent {
public:
    SpendCoinsEvent() : EffectEvent("SpendCoinsEvent") {}
    Players target{};  // 效果目标玩家
    CoinValue amount{};  // 扣款数量
    std::string reason{};  // 扣款用途
};  // 效果请求：扣除实际金币；由 Condition 校验并执行。

class AddPendingScoreEvent final : public EffectEvent {
public:
    AddPendingScoreEvent() : EffectEvent("AddPendingScoreEvent") {}
    Players target{};  // 效果目标玩家
    SettlementId settlementId{};  // 待结算事务
    ScoreValue delta{};  // 有符号增减值
};  // 效果请求：修正待结算分数；由 Condition 校验并执行。

class MultiplyPendingScoreEvent final : public EffectEvent {
public:
    MultiplyPendingScoreEvent() : EffectEvent("MultiplyPendingScoreEvent") {}
    Players target{};  // 效果目标玩家
    SettlementId settlementId{};  // 待结算事务
    int numerator = 1;  // 倍率分子
    int denominator = 1;  // 倍率分母
    RoundingRule rounding{};  // 取整规则
};  // 效果请求：缩放待结算分数；由 Condition 校验并执行。

class AddPendingCoinsEvent final : public EffectEvent {
public:
    AddPendingCoinsEvent() : EffectEvent("AddPendingCoinsEvent") {}
    Players target{};  // 效果目标玩家
    SettlementId settlementId{};  // 待结算事务
    CoinValue delta{};  // 有符号增减值
    RewardComponent component{};  // 基础或额外奖励
    CoinValue minimum{};  // 该部分最低金额
};  // 效果请求：修正待结算金币；由 Condition 校验并执行。

class ReduceShopCostEvent final : public EffectEvent {
public:
    ReduceShopCostEvent() : EffectEvent("ReduceShopCostEvent") {}
    Players target{};  // 效果目标玩家
    SettlementId settlementId{};  // 待结算事务
    CoinValue amount{};  // 减费数
    CoinValue minimum{};  // 卡牌规定的最低价
};  // 效果请求：降低本次商店报价；由 Condition 校验并执行。

class GrantFreeRefreshEvent final : public EffectEvent {
public:
    GrantFreeRefreshEvent() : EffectEvent("GrantFreeRefreshEvent") {}
    Players target{};  // 效果目标玩家
    ShopId shopId{};  // 商店会话
    int uses{};  // 额度数量
};  // 效果请求：增加本店免费刷新额度；由 Condition 校验并执行。

class RefreshShopEvent final : public EffectEvent {
public:
    RefreshShopEvent() : EffectEvent("RefreshShopEvent") {}
    Players target{};  // 效果目标玩家
    ShopId shopId{};  // 商店会话
    RefreshMode mode{};  // 付费或免费
};  // 效果请求：执行一次货架刷新；由 Condition 校验并执行，仍检查本店刷新总上限，免费不推进付费价格序号。

class OpenBoosterPackEvent final : public EffectEvent {
public:
    OpenBoosterPackEvent() : EffectEvent("OpenBoosterPackEvent") {}
    Players target{};  // 开包玩家
    ShopId shopId{};  // 商店会话
    PackId packId{};  // 卡包实例
    SettlementId settlementId{};  // 已验证的购买或授予事务
};  // 效果请求：执行已获准的开包；由 Condition 校验并执行，与购包事务关联，不跨输入等待；完成后发布开包通知并建立选择。

class LockShopSlotEvent final : public EffectEvent {
public:
    LockShopSlotEvent() : EffectEvent("LockShopSlotEvent") {}
    Players target{};  // 效果目标玩家
    ShopId shopId{};  // 商店会话
    SlotId slotId{};  // 货位
};  // 效果请求：执行货位锁定；由 Condition 校验并执行。

class UnlockShopSlotEvent final : public EffectEvent {
public:
    UnlockShopSlotEvent() : EffectEvent("UnlockShopSlotEvent") {}
    Players target{};  // 效果目标玩家
    ShopId shopId{};  // 商店会话
    SlotId slotId{};  // 货位
};  // 效果请求：执行货位解锁；由 Condition 校验并执行。

class GrantFunctionCardEvent final : public EffectEvent {
public:
    GrantFunctionCardEvent() : EffectEvent("GrantFunctionCardEvent") {}
    Players target{};  // 效果目标玩家
    DefinitionId definitionId{};  // 牌种
    CoinValue purchaseCost{};  // 实例出售成本
};  // 效果请求：生成并装备功能卡；由 Condition 校验并执行，检查空槽；免费生成成本为零。

class RemoveFunctionCardEvent final : public EffectEvent {
public:
    RemoveFunctionCardEvent() : EffectEvent("RemoveFunctionCardEvent") {}
    CardRef card{};  // 功能卡实例
    RemovalReason reason{};  // 移除原因
};  // 效果请求：移除功能卡；由 Condition 校验并执行。

class ReplaceFunctionCardEvent final : public EffectEvent {
public:
    ReplaceFunctionCardEvent() : EffectEvent("ReplaceFunctionCardEvent") {}
    CardRef card{};  // 功能卡实例
    DefinitionId newDefinitionId{};  // 新牌种
    bool preservePurchaseCost = true;  // 是否沿用出售成本
};  // 效果请求：替换功能卡；由 Condition 校验并执行。

class AddAbilityEvent final : public EffectEvent {
public:
    AddAbilityEvent() : EffectEvent("AddAbilityEvent") {}
    CardRef card{};  // 功能卡实例
    DefinitionId abilityDefinitionId{};  // 技能定义
};  // 效果请求：给功能卡添加技能；由 Condition 校验并执行。

class RemoveAbilityEvent final : public EffectEvent {
public:
    RemoveAbilityEvent() : EffectEvent("RemoveAbilityEvent") {}
    AbilityRef ability{};  // 目标技能
};  // 效果请求：逻辑删除技能；由 Condition 校验并执行。

class AbolishAbilityEvent final : public EffectEvent {
public:
    AbolishAbilityEvent() : EffectEvent("AbolishAbilityEvent") {}
    AbilityRef ability{};  // 目标技能
};  // 效果请求：永久废除技能；由 Condition 校验并执行。

class SetAbilityUsableEvent final : public EffectEvent {
public:
    SetAbilityUsableEvent() : EffectEvent("SetAbilityUsableEvent") {}
    AbilityRef ability{};  // 目标技能
    bool usable{};  // 新开关
};  // 效果请求：设置技能临时开关；由 Condition 校验并执行。

class SetFunctionCardUsableEvent final : public EffectEvent {
public:
    SetFunctionCardUsableEvent() : EffectEvent("SetFunctionCardUsableEvent") {}
    CardRef card{};  // 功能卡实例
    bool usable{};  // 新开关
};  // 效果请求：设置整牌临时开关；由 Condition 校验并执行。

class AbolishFunctionCardEvent final : public EffectEvent {
public:
    AbolishFunctionCardEvent() : EffectEvent("AbolishFunctionCardEvent") {}
    CardRef card{};  // 功能卡实例
};  // 效果请求：永久废除整张牌；由 Condition 校验并执行。

class ApplyCoatingEvent final : public EffectEvent {
public:
    ApplyCoatingEvent() : EffectEvent("ApplyCoatingEvent") {}
    CardRef card{};  // 功能卡实例
    CoatingType coating{};  // 镀层
};  // 效果请求：给功能卡施加镀层；由 Condition 校验并执行。

class DrawCardsEvent final : public EffectEvent {
public:
    DrawCardsEvent() : EffectEvent("DrawCardsEvent") {}
    Players target{};  // 效果目标玩家
    int count{};  // 数量
    DrawReason reason{};  // 摸牌原因
};  // 效果请求：按规则执行非初始摸牌；由 Condition 校验并执行，锁定本局结束后不得借此进行额外摸牌；牌堆不足按规则处理。

class SwapCardsWithDeckEvent final : public EffectEvent {
public:
    SwapCardsWithDeckEvent() : EffectEvent("SwapCardsWithDeckEvent") {}
    Players target{};  // 效果目标玩家
    std::vector<CardId> cards{};  // 实际扑克牌实例
};  // 效果请求：将指定手牌换到牌堆底并等量摸顶；由 Condition 校验并执行，保持原牌身份与公共牌组总数。

class ChangeCardRankEvent final : public EffectEvent {
public:
    ChangeCardRankEvent() : EffectEvent("ChangeCardRankEvent") {}
    CardId cardId{};  // 扑克牌实例
    int rank{};  // 目标点数
    Lifetime lifetime{};  // 持续范围
};  // 效果请求：临时改变普通牌点数；由 Condition 校验并执行。

class ChangeCardSuitEvent final : public EffectEvent {
public:
    ChangeCardSuitEvent() : EffectEvent("ChangeCardSuitEvent") {}
    CardId cardId{};  // 扑克牌实例
    Suit suit{};  // 目标花色
    Lifetime lifetime{};  // 持续范围
};  // 效果请求：临时改变花色；由 Condition 校验并执行。

class MakeCardJokerEvent final : public EffectEvent {
public:
    MakeCardJokerEvent() : EffectEvent("MakeCardJokerEvent") {}
    CardId cardId{};  // 扑克牌实例
    Lifetime lifetime{};  // 持续范围
};  // 效果请求：将指定牌临时变为王；由 Condition 校验并执行。

class RevealCardsEvent final : public EffectEvent {
public:
    RevealCardsEvent() : EffectEvent("RevealCardsEvent") {}
    Players viewer{};  // 查看者
    Players target{};  // 被查看玩家
    std::vector<CardId> cards{};  // 实际扑克牌实例
};  // 效果请求：向指定玩家展示合法信息；由 Condition 校验并执行，只向有权限的客户端同步牌面。

class ApplyPlayerStatusEvent final : public EffectEvent {
public:
    ApplyPlayerStatusEvent() : EffectEvent("ApplyPlayerStatusEvent") {}
    Players target{};  // 效果目标玩家
    DefinitionId statusDefinitionId{};  // 状态定义
    int stacks = 1;  // 层数
    Lifetime lifetime{};  // 期限
};  // 效果请求：施加玩家状态；由 Condition 校验并执行。

class RemovePlayerStatusEvent final : public EffectEvent {
public:
    RemovePlayerStatusEvent() : EffectEvent("RemovePlayerStatusEvent") {}
    Players target{};  // 效果目标玩家
    StatusId statusId{};  // 状态实例
};  // 效果请求：移除玩家状态；由 Condition 校验并执行。

class UpgradeHandPatternEvent final : public EffectEvent {
public:
    UpgradeHandPatternEvent() : EffectEvent("UpgradeHandPatternEvent") {}
    Players target{};  // 效果目标玩家
    HandPattern pattern{};  // 牌型
    int levels = 1;  // 加强级数
};  // 效果请求：执行牌型加强；由 Condition 校验并执行。

class PurchaseFacilityEvent final : public EffectEvent {
public:
    PurchaseFacilityEvent() : EffectEvent("PurchaseFacilityEvent") {}
    Players target{};  // 效果目标玩家
    DefinitionId facilityId{};  // 设施
};  // 效果请求：执行设施购买与生效；由 Condition 校验并执行。

class UseExpansionCardEvent final : public EffectEvent {
public:
    UseExpansionCardEvent() : EffectEvent("UseExpansionCardEvent") {}
    Players target{};  // 效果目标玩家
    DefinitionId expansionId{};  // 拓展牌
    TargetSpec selection{};  // 目标与方案
};  // 效果请求：执行获得即使用的拓展牌；由 Condition 校验并执行。

class AddDelayedEffectEvent final : public EffectEvent {
public:
    AddDelayedEffectEvent() : EffectEvent("AddDelayedEffectEvent") {}
    Players target{};  // 效果目标玩家
    DefinitionId effectDefinitionId{};  // 延迟效果定义
    EventType triggerType{};  // 未来触发的通知类型
    TargetSpec selection{};  // 目标与方案
    Lifetime lifetime{};  // 有效期
};  // 效果请求：建立延迟效果标记；由 Condition 校验并执行。

class OpenChoiceEvent final : public EffectEvent {
public:
    OpenChoiceEvent() : EffectEvent("OpenChoiceEvent") {}
    Players target{};  // 效果目标玩家
    ChoiceKind choiceKind{};  // 选择类型
    std::vector<ChoiceOptionId> options{};  // 合法选项
    std::uint32_t timeoutMs{};  // 超时长度
    bool optional{};  // 是否可放弃
};  // 效果请求：建立选择窗口并挂起相关流程；由 Condition 校验并执行。

class ResolveChoiceEvent final : public EffectEvent {
public:
    ResolveChoiceEvent() : EffectEvent("ResolveChoiceEvent") {}
    ChoiceId choiceId{};  // 待处理交互
    std::vector<ChoiceOptionId> options{};  // 实际选项
    ChoiceReason reason{};  // 手动、超时或掉线
};  // 效果请求：执行选择或默认方案；由 Condition 校验并执行。

class ChangeAbilityCounterEvent final : public EffectEvent {
public:
    ChangeAbilityCounterEvent() : EffectEvent("ChangeAbilityCounterEvent") {}
    AbilityRef ability{};  // 目标技能
    CounterId counterId{};  // 计数项
    CounterOperation operation{};  // 加值或设值
    int value{};  // 操作值
};  // 效果请求：改变已定义的技能成长或蓄力计数；由 Condition 校验并执行。

class EventBatch final : public EffectEvent {
public:
    EventBatch() : EffectEvent("EventBatch") {}
    std::vector<EventPtr> events{};  // 有序子效果
};  // 效果请求：有序原子组合多个效果；由 Condition 校验并执行，只允许 Effect 子事件；先校验后提交，不能跨玩家输入等待。
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
class PlayerJoinedControl final : public ControlEvent {
public:
    PlayerJoinedControl() : ControlEvent("PlayerJoinedControl") {}
    Players player{};  // 相关玩家
};  // 控制事件：玩家加入房间，由 Condition 更新房间流程，满足满员条件后自动开局。

class PlayerDisconnectedControl final : public ControlEvent {
public:
    PlayerDisconnectedControl() : ControlEvent("PlayerDisconnectedControl") {}
    Players player{};  // 相关玩家
    std::uint64_t connectionVersion{};  // 连接代次
};  // 控制事件：连接断开，保留原座位与资源，按对应超时规则继续处理。

class PlayerReconnectedControl final : public ControlEvent {
public:
    PlayerReconnectedControl() : ControlEvent("PlayerReconnectedControl") {}
    Players player{};  // 相关玩家
    std::uint64_t connectionVersion{};  // 新连接代次
};  // 控制事件：原玩家重连，恢复原座位并同步有权查看的当前状态。

class ShoppingDeadlineControl final : public ControlEvent {
public:
    ShoppingDeadlineControl() : ControlEvent("ShoppingDeadlineControl") {}
    ShopId shopId{};  // 商店会话
    std::uint64_t expectedVersion{};  // 对应商店版本
};  // 控制事件：商店计时到期，由 Condition 检查商店会话与版本后执行结束流程。

class TurnDeadlineControl final : public ControlEvent {
public:
    TurnDeadlineControl() : ControlEvent("TurnDeadlineControl") {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
    std::uint64_t expectedVersion{};  // 对应回合版本
};  // 控制事件：出牌计时到期，由 Condition 检查回合与版本后执行既定超时操作。

class ChoiceDeadlineControl final : public ControlEvent {
public:
    ChoiceDeadlineControl() : ControlEvent("ChoiceDeadlineControl") {}
    ChoiceId choiceId{};  // 待处理交互
    std::uint64_t expectedVersion{};  // 对应交互版本
};  // 控制事件：选择计时到期，由 Condition 检查交互与版本后执行默认方案。

class RequestRejectedControl final : public ControlEvent {
public:
    RequestRejectedControl() : ControlEvent("RequestRejectedControl") {}
    RequestId requestId{};  // 被拒绝请求
    std::string reason{};  // 拒绝原因
    std::uint64_t currentVersion{};  // 当前版本
};  // 控制反馈：操作请求未获接受，只反馈原因，不视为成功行为或成功类收益来源。

class StateSnapshotReadyControl final : public ControlEvent {
public:
    StateSnapshotReadyControl() : ControlEvent("StateSnapshotReadyControl") {}
    Players player{};  // 相关玩家
    SnapshotId snapshotId{};  // 按权限过滤的快照
};  // 控制反馈：同步快照已经准备，只向指定玩家发送其有权查看的信息。

class PlayOptionsReadyControl final : public ControlEvent {
public:
    PlayOptionsReadyControl() : ControlEvent("PlayOptionsReadyControl") {}
    Players player{};  // 相关玩家
    PreviewId previewId{};  // 预览结果
    std::vector<ChoiceOptionId> options{};  // 合法方案
};  // 查询反馈：合法出牌或赖子方案已经计算，表示预览结果，尚未成功出牌。

class ShopQuoteReadyControl final : public ControlEvent {
public:
    ShopQuoteReadyControl() : ControlEvent("ShopQuoteReadyControl") {}
    Players player{};  // 相关玩家
    PreviewId previewId{};  // 预览结果
    ShopData quote{};  // 当前报价
};  // 查询反馈：商店报价已经计算，本身不扣款或消耗免费刷新额度。
```

#### 2.7.5 参数、来源与重复触发约定

- `Request` 表示意图，实际成功后才发布事实通知。例如 `PlayCardsRequest` 通过后发布 `CardsPlayedEvent`，失败只返回 `RequestRejectedControl`。
- 一次出牌只发布一次成功出牌通知，自己、他人和任意玩家由处理器相对 `Context.owner()` 判断。购买、金币变化和装备完成可以是不同事实，但不能把同一事实重复发布。
- `ShopAbilityActivationEvent` 与 `TurnAbilityActivationEvent` 分别使用原窗口时机，只表示一次已验证的主动调用；窗口保持开放时不会反复产生通知。
- `PlayingCardsChangedEvent` 使用原因和实际牌 ID 区分换牌、改点数、改花色或临时变王；查看信息使用 `CardVisibilityChangedEvent`，不冒充物理牌面变化。
- `EventBatch` 让一个处理器返回一个事件，同时表达多个有序效果。子项仅允许 Effect，整体成功只计一次技能成功；先整体校验再提交，失败回滚，不能把等待玩家输入的交互放进跨等待事务。
- `QuoteShopOperationRequest` 和 `PreviewPlayRequest` 使用 Preview 模式。允许临时报价或合法性结果，禁止扣费、消耗额度、发放实际奖励、推进持久随机序列和发布技能成功通知。
- 分数和金币的“待结算修正”与“实际余额变化”分开；`AddPendingCoinsEvent` 指定基础或额外奖励，保证 E17 的扣款只作用于规则规定的部分。
- `Effect` 中的目标玩家与 `EventMeta.actor`、当前处理器持有者分别记录。涉及私有牌面的通知按查看权限同步。
- 所有 Request、效果和通知仍需满足房间当前状态、牌权、结束锁定和既有游戏规则；事件类存在不等于对应操作已经获准。续玩的一致性流程、事务实现和各类效果落地调用复用既有业务模块，具体接线后续完善。

## 3. 技能事件处理

本文说明卡牌技能的设计，以及使用 C++17 抽象基类 `Handle_Event` 实现这些设计。`Handle_Event` 作为事件处理器，统一通过 `handle_event(const Context&)` 读取上下文，返回一个新 `Event` 或空值，由状态机 `Condition` 继续处理。功能牌的持有者、实例、技能容器与牌级状态见 [功能卡说明](#4-功能卡)。`Event`、`Condition` 与 `Context` 的定义见 [事件与状态机说明](#2-事件与状态机)。本文为接口草案，尚未实现对局引擎。

### 3.1 时机与行为主体分别记录

`eventTypes[i] == true` 只说明技能订阅了第 i 种业务通知，不表示条件满足、目标合法或次数充足。另用 `PlayerScope` 限制行为主体：`Self` 为持有者、`Others` 为其他实际参局玩家、`Any` 为任意主体。

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

`context.event()->meta.actor` 表示行为发起者，`context.owner()` 表示接收处理器所属玩家；效果目标由具体事件类记录，三者允许不同。局开始、局结束等全局事件可以没有 `actor`，此时不按 `Self/Others` 匹配，使用 `Any` 并根据事件中的赢家等字段判断。

### 3.2 第一版时机表

区分整场游戏（Game）、一局（Round）、一巡（Trick）、一名玩家的一次行动回合（Turn）。以下 EventType 编号直接作为技能订阅布尔集合的下标。原有 0～33 保留，新增 34～62 追加，当前共 63 个；旧掩码扩展时新增位为 false。只有 Signal 参与事件类型匹配，`Count` 仅用于确定订阅掩码长度。

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

`context.event()->meta.eventId` 标识一次真实事件，网络重试不生成新事件。每个技能对同一事件默认至多执行一次；派生事件携带来源链，默认不得重新进入链中尚未完成的同一技能。复制和重复结算若开放，必须另外规定可复制范围、次数上限和是否产生新事件，不通过递归再次发布原始出牌来实现。

### 3.4 主动、被动和状态

- 主动技能：玩家提交 `ActivateAbilityRequest`，Condition 验证后生成商店或回合的主动调用通知，只选择目标技能；未主动请求不执行，超时默认放弃。
- 被动技能：业务通知继承 `SignalEvent`，默认 `source() == Passive`，按既定顺序匹配处理器；触发后需要选择目标仍属于被动技能。
- 同一牌可有多个技能，主动发动只调用所选技能，不把该牌的其他主动技能一起执行；被动事件只遍历被动技能。

| 字段 | 本轮接口约定 |
|---|---|
| `available` | 对象仍然有效；废除、消耗或移除后设为 false，不用重置次数恢复它 |
| `usable` | 对象当前允许执行的开关；临时封禁设为 false，解除后可以恢复 |
| `eventTypes` | 固定长度事件类型掩码；只过滤通知类型，不代替主体、次数和目标判断 |
| `type` | `Active` 或 `Passive` |
| `scope` | 响应自己的、他人的或任意主体的行为 |

`Condition` 统一调用 `handle_event(context)`。状态机核对持有关系、牌级状态和请求目标，处理器读取上下文并检查自己的 `available/usable`、事件 `source` 与自身 `type`、事件类型掩码、主体范围、目标、费用、次数及具体条件；满足后返回效果事件，由 `Condition` 校验并执行。`usable == true` 不表示可以忽略时机直接发动。

次数上限、已使用次数、重置范围、消耗策略和持续期限在逐张设计时明确；布尔值不能表达这些数值与生命周期。临时效果结束时撤销对应修正，不自动退款或回滚已经发放的收益。

### 3.5 C++17 接口草案

```cpp
class Handle_Event {
public:
    virtual ~Handle_Event() = default;

    std::size_t seq = 0;  // 由功能卡分配的稳定技能编号
    bool available = true;
    bool usable = true;
    AbilityType type = AbilityType::Passive;
    PlayerScope scope = PlayerScope::Any;
    std::vector<bool> eventTypes = std::vector<bool>(TOTAL_EVENT_TYPES, false);

    virtual EventPtr handle_event(const Context& context) = 0;
};
```

公共入口返回 `EventPtr`，支持返回任意具体效果派生类；`nullptr` 表示没有后续事件。Condition 校验返回值只允许 Effect，成功后发布相应 Signal。处理器不能返回伪造的玩家 Request、连接 Control 或未经业务提交的成功通知。

技能需要读取特定参数时，通过 `context.event_as<BeforePlayRewardEvent>()` 等具名类型取得对应事件；类型不匹配返回空指针，不使用不安全的强制转换。一个技能包含多个效果时返回 `EventBatch`。

`seq` 是牌内稳定编号，联机和事件使用 `AbilityRef`，容器下标仅用于内部管理。技能由功能卡拥有，生命周期见 [功能卡管理](#43-技能管理与生命周期)。

### 3.6 Context 与派发流程

同一房间始终复用 `Condition::context_`，每次调用前更新，不为每个事件创建另一份全局 Context。一个进程有多个房间时，各自拥有一份上下文；同一房间在调度器上串行访问。

| 只读接口 | 信息 |
|---|---|
| `event()` | 当前事件基类指针，可读取元数据和类别 |
| `event_as<T>()` | 取得对应具体派生类及其专属参数；不匹配或未绑定时为空 |
| `owner() / receiver()` | 当前处理器所属玩家和稳定技能引用 |
| `reward() / shop()` | 当前待结算奖励或报价快照；不适用时为空 |
| `game()` | 已有 GameState 的只读查询接口 |

```text
当前事件帧
  → Condition 清理旧绑定，更新同一份 Context
  → Handle_Event 读取 Context，返回 EventPtr 或 nullptr
  → 清理 Context 绑定，再执行新事件或恢复父帧
  → 下一处理器调用前按最新状态重新填充
```

父子事件和不同玩家之间复用同一对象，不代表共用旧参数。Condition 每次完整填充接收者、事件、奖励和报价，没有的参数必须清空。等待选择时只保存事件、交互和结算进度，恢复时重新绑定；处理器不得异步持有 Context 引用。

### 3.7 事件处理示例

以下仅为接口示意，不代表正式卡牌：“自己的出牌奖励计算前，将本手待结算分数翻倍”。处理器读取共享上下文，并返回 `MultiplyPendingScoreEvent` 的唯一所有权指针。

```cpp
class DoubleScoreHandler final : public Handle_Event {
public:
    DoubleScoreHandler() {
        scope = PlayerScope::Self;
        eventTypes[static_cast<std::size_t>(EventType::PlayRewardBefore)] = true;
    }

    EventPtr handle_event(const Context& context) override {
        const auto* event = context.event_as<BeforePlayRewardEvent>();
        const auto owner = context.owner();
        if (!event || !owner || !available || !usable ||
            event->source() != type) {
            return nullptr;
        }

        const auto eventType = event->event_type();
        if (!eventType || static_cast<std::size_t>(*eventType) >= eventTypes.size() ||
            !eventTypes[static_cast<std::size_t>(*eventType)] ||
            event->meta.actor != owner || !context.reward()) {
            return nullptr;
        }

        auto next = std::make_unique<MultiplyPendingScoreEvent>();
        next->target = *owner;
        next->settlementId = event->settlementId;
        next->numerator = 2;
        next->denominator = 1;
        next->rounding = RoundingRule::Floor;
        return next;
    }
};
```

`Condition` 校验并执行返回事件，修改当前待结算分数；实际到账仍由奖励阶段完成。处理器返回的事件 ID 与来源由状态机统一补齐，不能把它当作一次新的实际出牌。

具体状态转移、商店请求队列、当前结算栈、派发游标、成功计次、受控业务操作、交互挂起和事务处理待引擎实现时补齐。

## 4. 功能卡

功能卡（下文也称功能牌）是放入玩家槽位并持续持有的卡牌实例，由 C++17 抽象基类 `FunctionCards` 表示。一张功能卡可以包含多个技能，也可以同时包含主动与被动技能。

技能分类、生效时机、事件参数、事件处理规则和 `Handle_Event` 接口统一见 [技能说明](#3-技能事件处理)。本文为设计草案，尚未实现功能卡与对局引擎。

### 4.1 功能卡与技能的关系

功能卡负责记录“谁持有这张牌、这张牌是哪一个实例、包含哪些技能、整张牌当前是否有效”；各个技能分别记录自己的发动方式、时机、状态和具体效果。

- 功能卡保存技能事件处理器，由 Condition 选择处理器并更新共享 Context。
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

`seq` 是持有者范围内单调分配、不重复使用的牌实例编号，`(player, seq)` 标识一张实例；移动槽位不改变编号。牌种编号 `cardId`、名称、说明、稀有度、基础价格、实际购入价格、可选基数和倍率等仍需保留，后续并入 `FunctionCards` 的卡牌元信息，不放进事件类型掩码。

`Players` 的固定座位定义见 [事件系统中的公共枚举](#25-c17-接口草案)。2 人或 3 人房间只遍历实际参局座位；枚举存在不表示该座位已加入游戏。

### 4.3 技能管理与生命周期

使用 `unique_ptr` 明确技能由功能牌拥有，添加时转移所有权，删除时由容器释放，避免裸指针的释放责任不清。`deleteAbilities(index)` 表示逻辑移除并在安全点释放；`abolishAbilities(index)` 表示将技能 `available` 设为 false，保留实例用于显示和记录；临时禁用使用 `usable = false`。整张牌废除时设置牌的 `available = false`，所有技能统一被门禁挡住。

| 操作 | 接口 | 说明 |
|---|---|---|
| 事件调度 | 由 `Condition` 负责 | 选择牌内处理器，更新共享 `Context`，执行返回事件后再继续下一个处理器 |
| 添加技能 | `addAbilities(ability)` | 转移技能所有权，加入容器 |
| 删除技能 | `deleteAbilities(index)` | 逻辑移除，并在结算安全点释放 |
| 废除技能 | `abolishAbilities(index)` | 将该技能的 `available` 设为 false，保留实例用于显示与记录 |
| 临时禁用 | 设置 `usable = false` | 可作用于整张牌或某个技能，解除后可以恢复 |
| 废除整牌 | 设置牌的 `available = false` | 所有技能统一被牌级检查挡住 |

派发时冻结候选列表，结算中新增技能从后续事件开始参与；已被禁用、废除或移除的候选跳过。物理删除需延迟到安全点，详见 [技能结算规则](#33-结算顺序与边界)。

管理接口中的 `index` 只用于当前容器操作，网络、事件和交互统一使用 `AbilityRef` 的稳定技能编号并检查状态版本；Condition 查找编号后再映射到当前下标。实际调度、稳定 ID、次数与费用管理、交互挂起和事务处理待引擎实现时补齐。


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

Condition 先检查牌级状态和持有关系，按当前接收技能刷新唯一 Context。被动通知按顺序选取处理器；主动请求按 `AbilityRef` 核对牌、技能、调用玩家、窗口和版本。返回事件链完成后再继续下一候选，删除或废除后的稳定编号不复用。

### 4.5 逐张功能卡的说明模板

```text
名称／牌种编号：
价格／稀有度／出售成本规则：
技能 A：主动或被动
  响应事件／事件类型掩码：
  Context 只读参数与返回 EventPtr：
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

`std::optional<int>` 表示花色与点数可以为空，空值使用 `nullptr`。成员初始值、花色编码及倍率与基数的具体计算规则待后续确定。

## 6. 商店、回合与跨玩家效果

商店和对局共用 `Event → Condition → Context → Handle_Event → Event` 流程。商店的并行表示多个玩家同时拥有可操作会话；出牌的顺序表示只有当前玩家拥有出牌权限。状态机继续记录这些状态，业务模块无需分别维护两套事件机制。

| 状态范围 | 保存内容与调度要求 |
|---|---|
| 房间流程 | 当前局、巡、回合、阶段、结束锁定与状态版本 |
| 各玩家商店会话 | 货架、刷新次数、锁定货位、完成标记与待选卡包 |
| 当前回合 | 行动玩家、出牌窗口、待结算动作与技能链 |
| 待处理交互 | 请求者、合法目标或选项、关联事件、超时时间与恢复位置 |

商店的外部购买、刷新等操作先入 `shopEvents_`，由房间调度器依次取出，每次完成该操作及其内部效果链。P1 等待开包选择时，保存 P1 的结算进度与等待状态，允许其他玩家继续进行不依赖该待选结果的购物操作；等待客户端输入时不占住处理线程。涉及已经变化的余额、目标或报价时，后续操作必须根据当前状态重新校验。

局内普通操作不进入商店队列，也不缓存下一玩家的出牌。接受当前玩家的一次合法操作后关闭对应操作窗口，完成该动作的结算链，再按规则重新开放当前玩家窗口或切换下一玩家。目标玩家的被动效果或授权选择可以在他人的回合执行；选择回复、超时、掉线与重连仍由状态机处理，出牌权限、技能持有者和效果目标分别判断。

跨玩家效果通过返回事件的目标字段执行。例如 P1 的处理器返回目标为 P2 的 `AddScoreEvent`，`Condition` 修改 P2 的权威分数，再按规则产生相关通知。P2 的状态发生变化不会自动赋予 P2 出牌权限。

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

    void publish(EventPtr event) {
        condition_.on_event(std::move(event));
    }

private:
    Condition& condition_;
};
```

`EventListener` 负责适配输入，`Condition` 负责状态与调度。监听入口本身不更改手牌、金币、分数或结算上下文；Context 由 Condition 创建一次，并在每次调用处理器之前根据最新状态更新。

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
- 每个 Condition 复用唯一 Context，只绑定当前事件和小型快照；Event 通过 unique_ptr 移交所有权，不复制整个牌局或派生对象。虚调用、类型识别和分配成本在实现后测量，确有需要时再采用对象池。
- 事件处理路径避免同步等待磁盘、数据库或外部网络；这些工作通过独立任务完成，并按规则接收结果。
- 检测事件循环、控制连锁工作量，重点测量复杂牌型判断和具体效果的成本。不同房间可以独立调度，同一房间的共享状态由统一入口修改。
- 定时器用于明确的超时规则；用固定周期扫描所有状态会额外引入接近一个扫描周期的等待，应优先使用实际业务回调。

在实现后记录输入到达、商店请求入队（如适用）、开始处理、事件链完成和结果发出的时间，分别统计服务端调度等待、商店排队等待、连锁耗时与端到端耗时；关联 `eventId`、`rootEventId`，便于定位慢技能和重复链。当前没有运行中的对局引擎或实测数据，不能给出可靠的毫秒数；仅凭增加这些类不能判断最终延迟。

## 9. 后续实现事项

| 项目 | 待补齐内容 |
|---|---|
| 既有业务接入 | 玩家、GameState、手牌、牌型判定、商店、随机、交互等视为已有，仅完成事件到既有接口的连接 |
| 输入协议 | 操作请求、认证来源、合法性校验、状态版本、超时与掉线回调 |
| 事件调度 | 商店请求队列、局内请求互斥、当前结算栈、去重、来源链、派发游标、暂停／恢复及循环预算 |
| 事务与成功标准 | 效果执行、费用、计次、消耗、失败处理和结果同步 |
| 稳定标识与协议 | 复用房间、玩家、牌实例、技能、事件及交互 ID；定义 132 个事件名的序列化白名单与版本，不使用 typeid 哈希作为网络协议 |
| 数据与计算 | 花色点数编码、基数倍率、舍入规则，以及与付费加强的接入 |
| 验证与性能 | 真正实现后检查典型连锁、并发购物、跨玩家效果、重试、重连和延迟数据 |

后续的代码设计增补直接修改本文件。具体卡牌的玩法、价格与平衡数值仍按游戏规则和卡池设计审定。
