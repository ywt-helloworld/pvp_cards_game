# 贡献指南：添加功能牌

普通功能牌通过 **JSON 定义 → 生成代码 → 检查 → 提交 PR** 接入。
先 Fork 或克隆仓库，在项目根目录操作；环境要求见[检查说明](docs/pr-checks.md#2-本地先跑一次)。

## 1. 定义卡牌

```text
git switch -c cards/play-reward
```

参考[现有示例](cards/function/steady_income.json)，新建 `cards/function/play_reward.json`：

- `id`、`key` 必须唯一，`key` 与文件名一致。
- 每项 `abilities` 只做一个原子操作，例如加分与加金币分开写。
- 名称、说明应与实际效果和数值一致。字段与可用范围见 [JSON 格式](docs/function-card-json.md)；完整名称见[事件表](docs/event-system.md#完整事件表)和[技能表](docs/ability-system.md#完整技能表)。

## 2. 生成代码

```text
python tools/import_function_card.py cards/function/play_reward.json
```

工具会生成头文件、实现文件并更新卡池。修改已有牌时保留 `id` 和 `key`，命令加 `--force`；生成的 C++ 文件由工具维护。

## 3. 运行检查

```text
python tools/check_effect_cycles.py
python tools/check_pr.py
```

为新效果补充测试，覆盖正确触发、无关事件不触发，以及强化后的行为（如有）。新增事件或技能机制时，先扩展实现与生成器。

## 4. 提交 PR

将 JSON、生成代码和相关测试一起提交：

```text
git add cards/function/play_reward.json include/cards/generated/play_reward.hpp src/cards/generated/play_reward.cpp src/CardCatalog.cpp
git commit -m "feat: add play reward card"
git push -u origin cards/play-reward
```

新增的测试文件也要 `git add`。向 `main` 创建 PR，说明卡牌效果与验证结果；检查通过后由维护者合并。合并后需重新构建部署，新卡才会在线上生效。
