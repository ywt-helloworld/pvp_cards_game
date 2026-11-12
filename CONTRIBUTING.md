# 贡献指南：添加功能牌

普通功能牌通过 **JSON 定义 → 生成 C++ → 本地检查 → 提交 PR** 接入，不需要手动修改商店调度或构建列表。

## 1. 准备环境与分支

需要 Python 3.10+、C++17 编译器、CMake；完整检查还需要 Node.js 20+、npm 和可用的测试浏览器。先在克隆的 Git 仓库中创建分支，外部贡献者可先 Fork：

```text
git switch -c cards/play-reward
```

## 2. 编写卡牌 JSON

参考 [现有示例](cards/function/steady_income.json)，新建 `cards/function/play_reward.json`：

```json
{
  "version": 1,
  "type": "function",
  "id": 2002,
  "key": "play_reward",
  "name": "出牌奖励",
  "price": 4,
  "minimum_shop_level": 1,
  "description": "自己出牌后获得 2 分和 1 金币。",
  "abilities": [
    {"event": "PlayCommitted", "scope": "self", "op": "add_score", "amount": 2},
    {"event": "PlayCommitted", "scope": "self", "op": "gain_coins", "amount": 1}
  ]
}
```

- `id` 和 `key` 必须唯一；示例编号使用前也应检查是否已被占用。`key` 与 JSON 文件名一致，用小写字母、数字和单个下划线，以字母开头。
- `name`、`description` 是展示文本，说明须与实际效果一致；`price` 为非负整数，`minimum_shop_level` 为 1～3。
- `abilities` 每项只做一个原子操作，例如加分和加金币分开写。`scope: "self"` 只响应自己的操作，`any_player` 响应任意玩家，收益仍给持有者。
- 当前支持 `add_score`、`gain_coins`、`add_pending_score`、`add_pending_coins`、`reduce_shop_cost`；必须配合支持的事件使用。完整字段、事件及限制见 [JSON 格式说明](docs/function-card-json.md)。

## 3. 生成卡牌代码

在仓库根目录执行：

```text
python tools/import_function_card.py cards/function/play_reward.json --check
python tools/import_function_card.py cards/function/play_reward.json
```

工具自动生成并注册：

```text
include/cards/generated/play_reward.hpp
src/cards/generated/play_reward.cpp
src/CardCatalog.cpp
```

JSON 与这些生成文件应一起提交。修改已有牌时，保留原编号和 key，重新运行导入命令并加 `--force`；不要直接编辑带有 `Generated` 标记的 C++ 文件。导入后需要重新编译，运行中的服务器不会热加载 JSON。

## 4. 检查效果

```text
python tools/check_pr.py
```

该命令检查文档链接、JSON/生成代码一致性、C++ 编译与测试、网页构建和联机流程。Windows MinGW、npm 不在 PATH 或浏览器安装等参数见 [本地与 PR 检查说明](docs/pr-checks.md#2-本地先跑一次)。

为新增卡牌补充有意义的效果测试，验证正确玩家和事件会触发、无关事件不会触发；涉及强化或报价时还应覆盖强化后的数值、预览与失败操作。使用现有原子操作组合即可；全新事件或技能机制需要先扩展实现和生成器，再补测试与设计说明，不能在 JSON 中写 C++ 代码。

## 5. 提交 PR

```text
git add cards/function/play_reward.json include/cards/generated/play_reward.hpp src/cards/generated/play_reward.cpp src/CardCatalog.cpp
git commit -m "feat: add play reward card"
git push -u origin cards/play-reward
```

新增的测试也要加入本次提交。向本仓库 `main` 创建 PR，说明卡牌名称、触发条件、效果、价格及验证结果。GitHub 会自动运行 **PR checks**；失败时在原分支修复并推送，即会重新检查。

新卡会进入默认卡池，达到最低商店等级后才可能随机出货。合并不会自动更新线上实例，发布由维护者另行构建部署。
