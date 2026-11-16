# 贡献指南

## 1 添加功能牌

普通功能牌使用 JSON 定义，C++ 代码和卡池注册由工具生成。

### 1.1 Fork 和环境配置

以下步骤在自己的开发电脑上完成。初次参与项目时按顺序操作，已经配置好的工具可以直接使用。

#### 1.1.1 Fork 到自己的 GitHub 账号

1. 登录 GitHub，打开[原仓库](https://github.com/ywt-helloworld/pvp_cards_game)。
2. 点击右上角 **Fork**，Owner 选择自己的账号，仓库名保留 `pvp_cards_game`。
3. 点击 **Create fork**。完成后，你的仓库地址应为 `https://github.com/你的用户名/pvp_cards_game`。

Fork 是 GitHub 上属于你的仓库副本，后续将分支推送到这里，再向原仓库提交 PR。[Fork 官方说明](https://docs.github.com/en/pull-requests/how-tos/work-with-forks/fork-a-repo)

如果你是原仓库所有者，无需 Fork；下面克隆命令中的 `YOUR_GITHUB_NAME` 填 `ywt-helloworld`，并跳过添加 `upstream` 的步骤即可。

#### 1.1.2 安装开发工具

| 工具 | 版本与用途 | 安装入口 |
|---|---|---|
| Git | 克隆代码、创建分支、提交和推送 | [Git](https://git-scm.com/install/)；Windows 选择 Git for Windows |
| Python | 3.10+，运行卡牌生成与检查脚本；CI 使用 3.12 | [Python](https://www.python.org/downloads/) |
| CMake | 3.16+，配置与构建 C++ 项目 | [CMake](https://cmake.org/download/) |
| C++ 编译器 | 支持 C++17，Windows 可用 MSVC，Linux/macOS 可用 GCC/Clang | [Windows C++ 安装说明](https://learn.microsoft.com/en-us/cpp/build/vscpp-step-0-installation) |
| Node.js 与 npm | 网页构建及联机测试；项目要求 Node.js 20+，本地可安装 24 LTS，随附 npm | [Node.js](https://nodejs.org/en/download/) |
| 浏览器 | Windows 检查默认使用已安装的 Edge；其他配置见[检查说明](docs/pr-checks.md#2-本地先跑一次) | 使用本机浏览器 |

Windows 安装 C++ 工具时，选择 **使用 C++ 的桌面开发（Desktop development with C++）**，安装 MSVC、Windows SDK 和 C++ CMake 工具。后续编译检查在开始菜单中的 **Developer PowerShell for VS** 终端执行；已有 MinGW 环境的参数见[检查说明](docs/pr-checks.md#2-本地先跑一次)。

安装时将工具加入 PATH，然后重新打开终端，确认能输出版本号：

```text
git --version
python --version
cmake --version
node --version
npm.cmd --version
```

上面的 `npm.cmd` 用于 Windows；Linux/macOS 使用 `npm --version`。Windows 若只配置了 `py` 启动器，可用 `py -3` 代替下文的 `python`。卡牌脚本只用 Python 标准库；网页依赖由后面的检查命令按锁文件安装，首次完整检查需要联网。

#### 1.1.3 克隆到本地

在准备存放项目的父目录打开终端，将 `YOUR_GITHUB_NAME` 替换成你的 GitHub 用户名，再执行：

```text
git clone https://github.com/YOUR_GITHUB_NAME/pvp_cards_game.git
cd pvp_cards_game
git status
```

`git clone` 会新建 `pvp_cards_game` 文件夹，并建立名为 `origin` 的远程连接。`git status` 应显示当前在 `main` 分支，且没有待提交修改。以后编辑这个克隆目录中的文件；单独下载的 ZIP 不含 Git 提交记录。

#### 1.1.4 配置提交署名和登录

在刚进入的仓库目录执行，替换引号中的内容：

```text
git config user.name "你的提交署名"
git config user.email "你的 GitHub 已验证邮箱"
git var GIT_AUTHOR_IDENT
git var GIT_COMMITTER_IDENT
```

最后两条应显示你自己的署名和邮箱。这里配置的是提交记录中的身份，仅作用于当前仓库。

GitHub 登录决定你能向哪个仓库推送。Windows 安装 Git 时保留 Git Credential Manager；首次需要认证时，按浏览器提示登录 Fork 所属账号。公开仓库的克隆通常无需登录，首次推送时仍可能要求认证。[HTTPS 登录说明](https://docs.github.com/en/get-started/git-basics/caching-your-github-credentials-in-git?platform=windows)

#### 1.1.5 连接原仓库并验证

Fork 的贡献者再添加原仓库作为 `upstream`：

```text
git remote add upstream https://github.com/ywt-helloworld/pvp_cards_game.git
git remote -v
```

已有 `upstream` 时，直接用 `git remote -v` 核对地址即可。正确关系如下：

| 远程名称 | 指向 | 用途 |
|---|---|---|
| `origin` | `https://github.com/你的用户名/pvp_cards_game.git` | 推送你的分支 |
| `upstream` | `https://github.com/ywt-helloworld/pvp_cards_game.git` | 获取原仓库更新 |

验证本地目录与远程读取连接：

```text
git rev-parse --show-toplevel
git ls-remote --heads origin main
```

第一条应显示本地 `pvp_cards_game` 的绝对路径；第二条应返回一行提交哈希和 `refs/heads/main`。这表示可以读取远程仓库，推送权限仍由登录账号决定。[远程仓库配置说明](https://docs.github.com/en/pull-requests/how-tos/work-with-forks/configuring-a-remote-repository-for-a-fork)

Fork 的贡献者以后开始新功能前，可在本地 `main` 上同步原仓库：

```text
git fetch upstream
git switch main
git merge --ff-only upstream/main
```

直接克隆原仓库的维护者使用以下命令同步：

```text
git switch main
git pull --ff-only origin main
```

完成后继续 1.2，创建功能分支。***1.2～1.5 的命令均在项目根目录执行。***

### 1.2 定义卡牌

```text
git switch -c cards/play-reward
```

参考[现有示例](cards/function/steady_income.json)，新建 `cards/function/play_reward.json`：

- `id`、`key` 必须唯一，`key` 与 JSON 文件名（去掉 `.json`）一致。
- 每项 `abilities` 只做一个原子操作，例如加分与加金币分开写。
- 名称、说明应与实际效果和数值一致。字段与可用范围见 [JSON 格式](docs/function-card-json.md)；完整名称见[事件表](docs/event-system.md#完整事件表)和[技能表](docs/ability-system.md#完整技能表)。

### 1.3 生成代码

新建功能牌：

```text
python tools/import_function_card.py cards/function/play_reward.json
```

修改已有功能牌时，保留原来的 `id` 和 `key`，加上 `--force`：

```text
python tools/import_function_card.py cards/function/play_reward.json --force
```

工具会生成头文件、实现文件并更新卡池；生成的 C++ 文件由工具维护。

### 1.4 运行检查

```text
python tools/check_effect_cycles.py
python tools/check_pr.py
```

### 1.5 提交 PR

检查通过后，将 JSON 和生成代码一起提交；如有相关测试，也一并提交。

```text
git add cards/function/play_reward.json include/cards/generated/play_reward.hpp src/cards/generated/play_reward.cpp src/CardCatalog.cpp
git commit -m "feat: add play reward card"
git push -u origin cards/play-reward
```

推送后，从你的 Fork 分支向 `ywt-helloworld/pvp_cards_game` 的 `main` 创建 PR，说明卡牌效果与验证结果，等待自动检查通过后由维护者合并。
