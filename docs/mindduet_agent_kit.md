# MindDuet Agent Kit

MindDuet Agent Kit 为 Windows 上的不同项目提供统一的本地 AI Agent 入口：

- **Packy Codex**：用于复杂开发、跨模块修改、数据库迁移和最终审查；
- **DeepSeek Aider**：使用 `deepseek-v4-flash`，用于文案、解释、小脚本和局部低风险修改；
- **项目初始化器**：为新项目创建基础 `AGENTS.md`、安全忽略项和 VS Code 任务。

## 安装位置

工具安装在：

```text
%LOCALAPPDATA%\MindDuet\AgentKit
```

安装器会把该目录加入当前用户的 PATH。安装后需要重新打开 VS Code 或终端，短命令才会生效。

公开命令由 `.cmd` 包装器提供，内部 PowerShell 脚本使用 `*-core.ps1` 名称并由包装器以 `-ExecutionPolicy Bypass` 启动，因此不要求修改系统的全局 PowerShell 执行策略。

## 安装或更新

在 MindDuet Chess 仓库中运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/install_mindduet_agent_kit.ps1 -PackyKeyFile APIKEY
```

第一次安装时，`-PackyKeyFile` 会把 Packy 密钥迁移到 Windows Credential Manager。以后更新脚本时可以省略该参数：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/install_mindduet_agent_kit.ps1
```

安装器不会覆盖已经存在的 `%USERPROFILE%\.codex\packy.config.toml`。

## 在新项目中使用

先进入项目目录：

```powershell
cd E:\Guanghui\MyNewProject
mindduet-init
```

初始化器会：

1. 在不存在时创建基础 `AGENTS.md`；
2. 在 `.gitignore` 中补充 `APIKEY`、`.env*` 和 `.aider*`；
3. 在 `.vscode/tasks.json` 中加入 Packy 和 DeepSeek 两个任务；
4. 保留已有 `AGENTS.md`，不会用通用模板覆盖项目规则；
5. 如果已有 `tasks.json` 含 JSONC 注释而无法安全解析，则保留原文件并提示手工处理。

初始化后应当根据项目实际情况完善 `AGENTS.md`，至少写清楚：

- 项目目标和目标用户；
- 技术栈与重要目录；
- 构建、测试和运行命令；
- 数据与密钥安全边界；
- Git 提交和发布要求。

## 启动 Agent

在当前目录使用 Packy Codex：

```powershell
mindduet-agent packy
```

使用廉价 DeepSeek Agent：

```powershell
mindduet-agent deepseek
```

让 Packy Codex 在不逐次请求批准的情况下使用完整本机权限：

```powershell
mindduet-agent packy -FullAccess
```

该选项等价于 Codex 的 `sandbox = danger-full-access` 与 `approval = never`。Agent 可以在当前 Windows 用户权限范围内读写项目外文件并执行命令，失败时也不会弹出授权问题。它不会扩大任务本身的授权范围，也不会自动成为后台常驻进程。只应在可信项目、明确任务和可恢复的 Git 工作树中使用。

也可以在 VS Code 中按 `Ctrl+Shift+P`，运行 **Tasks: Run Task**，再选择对应 Agent。

VS Code 中的 **Start Packy Codex Agent (Full Access)** 对应上述完整权限模式。

指定其他项目目录：

```powershell
mindduet-agent packy -ProjectRoot E:\Guanghui\AnotherProject
mindduet-agent deepseek -ProjectRoot E:\Guanghui\AnotherProject
```

## 凭据管理

工具不会在项目中复制 API key：

- Packy：`MindDuet.AgentKit.PackyApiKey`；
- DeepSeek：`GuanghuiEducationLab.XiangqiTraining.DeepSeekApiKey`。

密钥存储在 Windows Credential Manager。启动器只把密钥注入子进程环境，并在 Agent 退出后恢复原环境变量。

如果 DeepSeek 凭据不存在，先在 MindDuet Chess 的 AI 设置窗口保存并测试 DeepSeek 密钥。

## 模型选择原则

| 任务 | 推荐入口 |
|---|---|
| 解释代码、文案、注释、小脚本 | DeepSeek |
| 单文件、低风险重构 | DeepSeek，完成后人工检查 diff |
| 多文件功能、复杂 Bug | Packy Codex |
| 数据库迁移、安全相关修改 | Packy Codex |
| 最终测试、审查、提交和推送 | Packy Codex |

DeepSeek 入口默认关闭 Aider 自动提交。完成修改后应检查 `git diff` 和测试结果，再决定是否提交。

## 迁移到另一台 Windows 电脑

1. 克隆包含 Agent Kit 安装器的 MindDuet Chess 仓库；
2. 安装 Codex 桌面应用或 Codex CLI；
3. 安装 Aider；
4. 运行 Agent Kit 安装器；
5. 在 Windows Credential Manager 中重新配置 Packy 和 DeepSeek 密钥；
6. 在每个项目中运行 `mindduet-init`。

API 密钥和 Windows 凭据不会通过 Git 同步，必须在每台设备上单独配置。
