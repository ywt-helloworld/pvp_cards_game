#pragma once
#include "Scheduler.hpp"
namespace cardgame {
/// 由 tools/import_function_card.py 维护注册实现；返回 JSON 导入的功能牌工厂。
std::vector<SchedulerConfig::CardDefinition> generated_function_cards();
} // namespace cardgame
