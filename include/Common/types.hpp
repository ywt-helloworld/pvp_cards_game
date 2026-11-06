#pragma once

/// @file types.hpp
/// @brief 公共整数别名、事件所有权与回调函数类型。

#include <cstdint>
#include <functional>
#include <memory>
#include "enums.hpp"
#include <bitset>
#include <vector>

namespace cardgame {

class Event;  // 只声明类型；创建和销毁具体事件时应包含 Event.hpp。

// 下列 ID 是整数类型别名，不拥有业务对象，也不提供运行时类型检查。
using EventId = std::uint64_t;  // 一次实际事件的实例编号；由外部调度记录管理，区别于操作种类 EventType。
using RequestId = std::uint64_t;  // 玩家请求的编号，用于关联反馈或由外部模块去重。
using RoomId = std::uint64_t;  // 房间实例编号。
using ShopId = std::uint64_t;  // 某名玩家本次商店会话的编号。
using SlotId = std::uint64_t;  // 商店货位编号。
using ProductId = std::uint64_t;  // 货架上某件商品实例的编号。
using PackId = std::uint64_t;  // 商店所展示卡包实例的编号。
using ChoiceId = std::uint64_t;  // 一次待处理选择交互的编号。
using ChoiceOptionId = std::uint64_t;  // 某次选择交互中的选项编号。
using CardId = std::uint64_t;  // 实际扑克牌实例编号，牌面变化后仍用于追踪同一张牌。
using DefinitionId = std::uint64_t;  // 卡牌、技能、设施或效果等静态定义的编号，具体种类由字段语义确定。
using StatusId = std::uint64_t;  // 玩家身上一项状态实例的编号。
using CounterId = std::uint64_t;  // 技能内一个已定义计数项的编号。
using DelayedEffectId = std::uint64_t;  // 已经建立的延迟效果实例编号。
using RoundId = std::uint64_t;  // 整场游戏中某一局的实例编号。
using TrickId = std::uint64_t;  // 某一局内一巡的实例编号。
using TurnId = std::uint64_t;  // 某名玩家一次行动回合的实例编号。
using PlayedHandId = std::uint64_t;  // 一次已提交出牌的实例编号。
using SettlementId = std::uint64_t;  // 一次奖励或报价结算记录的编号。
using RoundResultId = std::uint64_t;  // 本局结束时锁定的结果与手牌快照记录编号。
using SnapshotId = std::uint64_t;  // 一份按玩家权限生成的同步快照编号。
using PreviewId = std::uint64_t;  // 一次只读预览结果的编号。
using ScoreValue = std::int64_t;  // 有符号 64 位分数值；操作增量与实际累计分数由字段语义区分。
using CoinValue = std::int64_t;  // 有符号 64 位金币值；金额合法范围由具体操作规则检查。

/// 持有任意具体事件的唯一所有权；移动后由接收方负责其生命周期。
using EventPtr = std::unique_ptr<Event>;

/// 将一个事件的唯一所有权交给后续处理模块。
using EventReceiver = std::function<void(EventPtr)>;

/// 输入源提供的取消订阅函数；注册模块应保证它不抛异常。
using EventDisconnect = std::function<void()>;

struct Context;
struct CardEffects;
class FunctionCards;
class Player;
using EventMask = std::bitset<TOTAL_EVENT_TYPES>;
using TriggerMatrix = std::vector<EventMask>;
using EventList = std::vector<EventPtr>;
using PlayerOrder = std::vector<Players>;
using CardPtr = std::shared_ptr<FunctionCards>;
using FunctionCardFactory = std::function<CardPtr(Player&, std::size_t)>;
using CardHandler = std::function<void(const Context&, CardEffects&)>;
using InitialDealOperation = std::function<bool(RoundId, const PlayerOrder&, int)>;

}  // namespace cardgame
