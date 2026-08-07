# Chinese Chess / 中国象棋针对性训练系统

这是一个使用 Qt 6、C++、Python、SQLite、Pikafish 和大语言模型构建的中国象棋个性化训练原型。它不仅能够进行人机对弈，还会保存个人对局、自动评价红方走法并积累长期训练数据。

## 核心理念

> 引擎负责“算得准”，大语言模型负责“讲得懂”，个人数据库负责“看得远”。

- **Pikafish**计算最佳走法、局面评分和推荐变化。
- **AI 教练（DeepSeek Chat）**基于引擎证据解释失误、提出训练任务和复盘问题。
- **SQLite 个人数据库**长期保存每盘棋、每一步、思考时间和复盘结果。

```text
记录对局 → 引擎复盘 → AI 解释 → 更新个人档案 → 生成针对训练 → 检验提升效果
```

这个项目也是未来个性化数学教育系统的实验原型：规则和计算工具保证判断准确，大语言模型负责教学解释，学习者数据库负责长期诊断。

## MindDuet 项目文档

- [从中国象棋训练到个性化数学教育](docs/xiangqi_training_vision.md)
- [MindDuet Math：高中数学个性化学习系统计划](docs/mindduet_math_plan.md)
- [针对性训练与动态用户画像设计](docs/personalized_training_and_user_profile.md)
- [MindDuet Chess 开发复盘与后续路线](docs/mindduet_chess_development_retrospective.md)

## 当前可运行功能

- 绘制标准棋盘和中文棋子
- 用高对比度起终点圆环和加粗箭头标记最后一步走法，便于观察对手刚才的移动
- 鼠标控制红方，Pikafish 控制黑方，并支持入门、初级、中级、高级四档难度
- 支持悔棋一回合；当前棋谱会回退，但被撤销的用户着法会单独保存为学习证据，包含局面、引擎推荐、损失和错误等级
- 将“AI 教练”和“引擎分析”组织在统一的专业分析工作区；每个关键步骤使用独立原生卡片展示诊断、局面依据、训练动作和落子前自问，界面统一使用适合中文阅读的无衬线字体；每局结束后自动生成、保存并在独立窗口显示整盘建议（未配置模型时使用本地引擎数据总结）
- 提供历史对局时间轴复盘：逐步查看棋盘、最后一步箭头、中文着法、引擎评价、推荐变化、当步教练意见和整盘建议
- 可在历史对局复盘窗口删除故意测试或无效对局；相关棋谱、分析、AI 建议和训练记录会一并删除，并重新计算个人画像
- 历史列表使用当前有效对局的连续序号，删除测试局后会自动重排；数据库内部仍保留稳定 ID 用于关联数据
- 引擎分析与 AI 教练合并在同一工作区，并可针对当前着法或整盘复盘连续追问；对话按用户、对局和步数保存在数据库中
- 悔棋会永久保存被撤销的原着、局面、引擎推荐、损失、推荐变化和 AI 诊断；当前分析卡片不会被删除，而会灰化标记为“已撤销分支”，迟到的分析结果也会自动回填并生成分支卡片，在复盘的“悔棋记录”页单独展示
- 棋盘使用可缩放的 Qt 矢量绘制，选中棋子后显示合法落点和可吃目标；AI 输出限制为具体战术因果、对手惩罚、正确思路和可执行检查规则，避免泛泛鼓励与重复评分
- 支持主动认输；认输对局按黑方胜利和 `resignation` 结束原因保存，并继续进入个人统计与整盘复盘
- 从个人历史失误、画像变式和经 Pikafish 验证的 AI 原创题生成专项训练；题库支持浏览、切题、去重、间隔复习，并以三题为目标维护未练过 AI 原创题的后台库存
- 从评分、阶段、思考用时和悔棋中生成诊断标签，形成带证据、置信度、状态和趋势的动态能力画像
- 自动生成阶段训练计划并优先抽取当前弱项的真实历史局面；每道题说明推荐原因，提供三级提示并记录提示依赖
- 支持创建和切换多个用户，每个用户拥有独立对局、画像、错题和训练记录
- 每位用户每完成 10 盘有效对局，自动生成一份阶段表现总结和训练建议
- 用数据卡片、胜负环形图、失误柱状图和最近十盘趋势图展示个人记录
- 校验全部棋子规则、将军、将帅照面和胜负
- 自动记录双方每一步、吃子、思考时间和走前/走后局面
- 使用 SQLite 保存全部对局数据
- 红方每走一步，Pikafish 自动进行两次 UCI 搜索
- 比较最佳评分和实际落子评分，计算局面损失
- 主界面只显示实战着、推荐着、错误等级和评价下降，隐藏冗长文字变化；可点击“在棋盘上查看推荐着”，在独立窗口用箭头直观显示走法
- 累计统计优秀、轻微失误、明显失误和严重失误
- 可选接入 DeepSeek Chat：对局损失超过 30 时先展示 Pikafish 的实战惩罚线与推荐应对线，再依据双线证据和用户留下的落子思路生成结构化讲解；每张建议可记录“说清楚了 / 太抽象 / 不理解变化”反馈
- 对局结束并等待皮卡鱼完成全部逐步分析后，自动整理完整着法、阶段表现、关键转折点和全部悔棋证据，生成并保存整盘 AI 复盘；所有悔棋都必须逐项解释
- 支持随时开始新对局

## 项目结构

```text
xiangqi_game.*           象棋规则、胜负判断和标准走棋记录
xiangqi_board_widget.*   棋盘绘制、鼠标交互和分级 Pikafish 对弈
game_database.*          SQLite 对局、走法、分析结果和长期统计
pikafish_analyzer.*      Pikafish UCI 通信、FEN/坐标转换和评价
deepseek_coach.*         DeepSeek Chat 异步请求、提示词、JSON 校验和本地引擎降级处理
credential_store.*       Windows Credential Manager 密钥安全存储
engine_py/               自己编写的 Python 实验引擎（保留用于学习和后续训练）
tests/                   端到端自动化测试
scripts/                 环境安装脚本
docs/                    项目愿景与设计说明
```

## 环境

- Qt 6.10.1（MinGW 64-bit，包含 Qt SQL）
- CMake
- Conda 环境 `chess`
- Python 路径优先使用 `E:/Anaconda/envs/chess/python.exe`

## 安装 Pikafish

当前电脑已经安装完成。如果重新克隆项目，可以在 PowerShell 中运行：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/install_pikafish.ps1
```

脚本会从官方 GitHub 发布页下载 Pikafish 2026-01-02，将 AVX2 引擎、NNUE 网络和许可证放入 `engines/pikafish/`。Pikafish 使用 GPL v3，NNUE 文件许可见该目录中的 `NNUE-License.md`。

也可以通过环境变量指定其他皮卡鱼程序：

```powershell
$env:PIKAFISH_PATH = "D:/engines/pikafish.exe"
```

## 构建和运行

在 Qt Creator 中打开 `CMakeLists.txt`，选择 Desktop Qt MinGW Kit，然后点击 **Build** 和 **Run**。

程序启动后玩家执红方。每次红方合法移动后，黑方自动应对，右侧训练面板随后显示皮卡鱼评价。

数据库默认保存在 Windows 用户应用数据目录：

```text
%APPDATA%/GuanghuiEducationLab/XiangqiTraining/xiangqi_training.db
```

程序界面底部也会显示实际数据库路径。

## 启用 AI 教练

在程序的 **AI 设置** 中输入 DeepSeek API 密钥，或设置 `DEEPSEEK_API_KEY` 环境变量。程序通过 DeepSeek 的 Chat Completions 接口调用 `deepseek-chat`，用于单步讲解、追问、整盘复盘和 AI 原创训练题。密钥不会进入 Git、数据库或发布包；复制到另一台电脑时需要单独配置。

程序不会把 API 密钥写入源码、配置文件或数据库。也可以在程序的 **AI 设置** 中输入新的 DeepSeek 密钥：

1. 启动象棋程序。
2. 点击右侧的 **AI 设置**。
3. 输入新密钥。
4. 点击 **保存并测试连接**。

密钥会保存在 Windows Credential Manager 中，以后启动程序会自动读取。设置窗口不会回显已经保存的密钥，并支持替换或删除。

环境变量 `DEEPSEEK_API_KEY` 可用于开发和临时运行，并且优先于 Windows 凭据：

```powershell
$env:DEEPSEEK_API_KEY = "这里填写新密钥"
```

在同一个 PowerShell 窗口中启动 Qt Creator，或者在 Qt Creator 的项目运行环境中添加该变量。不要把真实密钥提交到 Git。

## 在 VS Code 中启动 Packy Codex Agent

本机的官方 Codex 登录与 Packy API 使用独立配置，互不覆盖。Packy 和 DeepSeek 密钥统一保存在 Windows Credential Manager 中，不会进入 Git。

在 VS Code 中按 `Ctrl+Shift+P`，选择 **Tasks: Run Task**，然后运行：

```text
Start Packy Codex Agent
```

也可以在项目的 PowerShell 终端中运行：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/codex-packy.ps1
```

全局启动器会临时读取 Windows 凭据，使用用户目录中的独立 `packy` Codex 配置档和 `gpt-5.6-sol`，并自动加载项目根目录的 `AGENTS.md`。退出 Agent 后不会在 PowerShell 环境中保留密钥，也不会改变 Codex 桌面应用原有的官方登录。

对于改文案、解释代码、局部重构和简单脚本等低风险任务，可以从同一任务列表运行：

```text
Start DeepSeek Agent (Cheap Tasks)
```

当前 Codex CLI 只支持 Responses 协议，而 DeepSeek 官方 API 使用 Chat Completions，因此廉价入口使用 Aider 作为 Agent 外壳，并选择 `deepseek/deepseek-v4-flash`。启动器从 MindDuet Chess 已保存的 Windows Credential Manager 凭据中临时读取 DeepSeek 密钥，加载 `AGENTS.md`，并关闭 Aider 自动提交。复杂功能、跨模块修改、数据库迁移和最终审查仍应使用 Packy Codex。

全局 Agent Kit 安装后，可以在任意项目目录运行：

```powershell
mindduet-init
mindduet-agent packy
mindduet-agent packy -FullAccess
mindduet-agent deepseek
```

`mindduet-init` 不会覆盖已有 `AGENTS.md`，会补充安全忽略规则，并在可安全解析的情况下加入 VS Code 任务。详细说明见 [MindDuet Agent Kit](docs/mindduet_agent_kit.md)。

AI 教练统一使用 DeepSeek 的 `deepseek-chat` 生成单步讲解、追问和整盘复盘；没有 DeepSeek 凭据时 AI 教练保持禁用。为了控制费用，优秀着法使用本地说明，只有局面损失超过 30 时才请求单步讲解。单步分析会携带必要棋局证据，整盘复盘会携带完整棋谱、胜负、阶段统计、关键转折点、思考时间和悔棋证据。AI 原创题仅接收压缩后的用户画像、当前训练计划、最多三条诊断证据和最近题目主题，生成后必须通过 XiangqiGame 与 Pikafish 验证才可入库。模型只负责解释和出题草案，不能修改引擎评分或编造未验证变化。

## 自动化验证

项目提供 `training_smoke_test`，验证完整链路：

```text
合法走棋 → 生成 MoveRecord → 写入 SQLite → Pikafish 分析 → 写入长期统计
```

可以在构建目录运行 `training_smoke_test.exe`。

## 数据库内容

- `games`：开始时间、结束时间、胜负和正常结束/认输等结束原因
- `users`：独立用户及创建时间
- `moves`：每一步坐标、棋子、吃子、思考时间和局面
- `analyses`：实际走法、推荐走法、评分损失、错误等级和推荐变化
- `coaching`：AI 诊断、证据、训练任务和复盘问题
- `coach_feedback`：用户对单步 AI 讲解的理解质量反馈
- `game_reviews`：对局结束后的整体评价、关键转折点、优点、重复思考模式、训练计划和复盘问题
- `undo_events`：不可随棋谱回退删除的悔棋证据，用于发现“落子后才意识到”的重复决策问题
- `diagnosis_tags`：由本地规则从引擎事实和用户行为中提取的受控错误标签
- `profile_snapshots`：每个阶段各能力维度的分数、置信度、状态、趋势和证据数量
- `training_plans` / `training_plan_items`：阶段重点、可检验诊断假设、训练任务和成功标准
- `training_positions`：从个人历史失误提取的训练局面、主题、掌握度和复习日期
- `training_attempts`：每次训练作答、正确性、用时和作答时间
- `profile_reports`：每完成 10 盘生成的个人阶段总结

## 下一阶段

- 对局历史列表和逐步回放
- 更细致的错误类型：漏看威胁、子力保护、开局效率、残局等
- 使用 Pikafish 对非首选但同样优秀的训练答案进行动态判分
- 汇总多盘 DeepSeek 建议，生成周度个性化复盘报告
- 追踪相同错误是否随着训练逐渐减少
