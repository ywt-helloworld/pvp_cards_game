# 贡献指南

## 1 添加功能牌

添加普通功能牌只需要按格式完成 **JSON 定义**，后续操作通过程序自动完成。

### 1.1 Fork 和环境配置
先 Fork 或克隆仓库，在项目根目录操作；环境要求见[检查说明](docs/pr-checks.md#2-本地先跑一次)。

### 1.2 定义卡牌

```text
git switch -c cards/play-reward
```

参考[现有示例](cards/function/steady_income.json)，新建 `cards/function/play_reward.json`：

- `id`、`key` 必须唯一，`key` 与文件名一致。
- 每项 `abilities` 只做一个原子操作，例如加分与加金币分开写。
- 名称、说明应与实际效果和数值一致。字段与可用范围见 [JSON 格式](docs/function-card-json.md)；完整名称见[事件表](docs/event-system.md#完整事件表)和[技能表](docs/ability-system.md#完整技能表)。

### 1.3 生成代码

在项目根目录下执行：

- 创建新的功能牌（注意不要跟其他原有卡使用同样的 `id` 或 `key`）：
  ```text
  python tools/import_function_card.py cards/function/play_reward.json
  ```
- 修改已有功能牌牌（注意保留原来的 `id` 和 `key`），命令加 `--force`：
  ```text
  python tools/import_function_card.py cards/function/play_reward.json --force
  ```

工具会生成头文件、实现文件并更新卡池，生成的 C++ 文件由工具维护。

### 1.4 运行检查

在项目根目录下执行：

```text
python tools/check_effect_cycles.py
python tools/check_pr.py
```

### 1.5 提交 PR

确认正确后在项目根目录下执行：

```text
git add cards/function/play_reward.json include/cards/generated/play_reward.hpp src/cards/generated/play_reward.cpp src/CardCatalog.cpp
git commit -m "feat: add play reward card"
git push -u origin cards/play-reward
```

