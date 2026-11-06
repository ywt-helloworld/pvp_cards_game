# 卡牌技能设计

本文说明卡牌技能的设计，以及使用 C++17 抽象基类 `Handle_Event` 实现这些设计。`Handle_Event` 作为事件处理器，统一通过 `handle_event(const Context&)` 读取上下文，返回一个新 `Event` 或空值，由状态机 `Condition` 继续处理。功能牌的持有者、实例、技能容器与牌级状态见 [功能卡说明](function-cards.md)。`Event`、`Condition` 与 `Context` 的定义见 [事件与状态机说明](event-system.md)。本文为接口草案，尚未实现对局引擎。

## 1. 时机与行为主体分别记录

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

## 2. 第一版时机表

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

## 3. 结算顺序与边界

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

## 4. 主动、被动和状态

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

## 5. C++17 接口草案

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

此接口依赖 [事件与状态机说明中的公共定义](event-system.md#5-c17-接口草案)。`Players` 表示固定座位，只遍历实际参局玩家；时机枚举与布尔集合下标仍沿用 §2 的 34 个取值。

技能的公共入口为 `handle_event(const Context&)`。处理器通过只读上下文获取所需参数，以 `std::optional<Event>` 表达有无后续事件；效果由 `Condition` 继续执行。基类提供虚析构函数，时机集合初始化为 `TOTAL_TIMES` 个 false。

技能由功能牌拥有；技能容器、添加、删除、废除及临时禁用的管理规则见 [功能卡说明](function-cards.md#3-技能管理与生命周期)。

## 6. Context 与派发流程

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

空值表示没有后续事件，状态机继续其他候选；新事件表示请求执行后续处理，实际成功后再记录成功次数和处理消耗。报价预览、事件来源、队列与循环边界见 [事件与状态机说明](event-system.md#4-队列返回值与结算)。

## 7. 事件处理示例

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
