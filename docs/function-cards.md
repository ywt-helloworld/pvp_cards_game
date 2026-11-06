# 功能卡说明

功能卡（下文也称功能牌）是放入玩家槽位并持续持有的卡牌实例，由 C++17 抽象基类 `FunctionCards` 表示。一张功能卡可以包含多个技能，也可以同时包含主动与被动技能。

技能分类、生效时机、事件参数、事件处理规则和 `Handle_Event` 接口统一见 [技能说明](ability-system.md)。本文为设计草案，尚未实现功能卡与对局引擎。

## 1. 功能卡与技能的关系

功能卡负责记录“谁持有这张牌、这张牌是哪一个实例、包含哪些技能、整张牌当前是否有效”；各个技能分别记录自己的发动方式、时机、状态和具体效果。

- 功能卡保存技能事件处理器，由 `Condition` 根据 `Event` 和牌级状态选择处理器并生成 `Context`。
- `Condition` 直接调用处理器的 `handle_event(const Context&)`；处理器返回一个新 `Event` 或空值，由状态机继续执行。
- 主动请求先核对请求玩家、目标牌与目标技能，只选择指定处理器；被动通知按既定顺序遍历。
- 整张牌的状态与技能自身的状态共同限制执行；牌级检查通过后仍需执行 [技能发动条件检查](ability-system.md#4-主动被动和状态)。
- 同时触发的牌按槽位顺序结算，牌内按技能顺序结算；完整事件顺序见 [技能结算规则](ability-system.md#3-结算顺序与边界)。

## 2. 数据成员与卡牌元信息

| 成员 | 类型 | 含义 |
|---|---|---|
| `player` | `Players` | 当前持有者，使用固定座位枚举 |
| `seq` | `std::size_t` | 持有者范围内的稳定实例序号 |
| `abilities` | `std::vector<std::unique_ptr<Handle_Event>>` | 由这张牌拥有的技能，按顺序保存 |
| `available` | `bool`，初始 true | 整张牌仍然有效 |
| `usable` | `bool`，初始 true | 整张牌当前允许执行技能的开关 |

`seq` 是持有者范围内单调分配、不重复使用的牌实例编号，`(player, seq)` 标识一张实例；移动槽位不改变编号。牌种编号 `cardId`、名称、说明、稀有度、基础价格、实际购入价格、可选基数和倍率等仍需保留，后续并入 `FunctionCards` 的卡牌元信息，不放进时机掩码。

`Players` 的固定座位定义见 [事件系统中的公共枚举](event-system.md#5-c17-接口草案)。2 人或 3 人房间只遍历实际参局座位；枚举存在不表示该座位已加入游戏。

## 3. 技能管理与生命周期

使用 `unique_ptr` 明确技能由功能牌拥有，添加时转移所有权，删除时由容器释放，避免裸指针的释放责任不清。`deleteAbilities(index)` 表示逻辑移除并在安全点释放；`abolishAbilities(index)` 表示将技能 `available` 设为 false，保留实例用于显示和记录；临时禁用使用 `usable = false`。整张牌废除时设置牌的 `available = false`，所有技能统一被门禁挡住。

| 操作 | 接口 | 说明 |
|---|---|---|
| 事件调度 | 由 `Condition` 负责 | 选择牌内处理器，生成 `Context`，执行返回事件后再继续下一个处理器 |
| 添加技能 | `addAbilities(ability)` | 转移技能所有权，加入容器 |
| 删除技能 | `deleteAbilities(index)` | 逻辑移除，并在结算安全点释放 |
| 废除技能 | `abolishAbilities(index)` | 将该技能的 `available` 设为 false，保留实例用于显示与记录 |
| 临时禁用 | 设置 `usable = false` | 可作用于整张牌或某个技能，解除后可以恢复 |
| 废除整牌 | 设置牌的 `available = false` | 所有技能统一被牌级检查挡住 |

派发时冻结候选列表，结算中新增技能从后续事件开始参与；已被禁用、废除或移除的候选跳过。物理删除需延迟到安全点，详见 [技能结算规则](ability-system.md#3-结算顺序与边界)。

管理接口中的 `index` 以及 `Context.event.requestedAbilityIndex` 仅适用于服务端当前稳定的技能列表；联机请求需要稳定技能编号及状态版本，不能信任客户端缓存的可变下标。实际调度、稳定 ID、次数与费用管理、交互挂起和事务处理待引擎实现时补齐。


装备与移除分别使用 `FunctionCardEquipped`、`FunctionCardRemoved` 事件，具体边界见 [技能时机表](ability-system.md#2-第一版时机表)。持续效果的来源随装备、移除、失效和恢复更新。

## 4. C++17 接口草案

以下接口依赖 [事件系统的公共定义](event-system.md#5-c17-接口草案)和 [技能处理器定义](ability-system.md#5-c17-接口草案)，代码应接在这些定义之后使用。

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

## 5. 逐张功能卡的说明模板

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

功能卡进入空槽、实际购入成本、出售价格和卖出后清除成长等交易规则见 [游戏规则的商店章节](../README.md#6-商店)。拓展牌仍遵循获得即使用、不能保存的规则，不与功能卡的持有方式混用。

第一批功能卡的样本和候选方向见 [卡牌设计调研](function-card-research.md)；正式卡池、价格、稀有度与效果数值尚待逐张确定。
