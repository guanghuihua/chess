# Chinese Chess / 中国象棋针对性训练系统

这是一个使用 Qt 6、C++、Python、SQLite、Pikafish 和 DeepSeek 构建的中国象棋个性化训练原型。它不仅能够进行人机对弈，还会保存个人对局、自动评价红方走法并积累长期训练数据。

## 核心理念

> 引擎负责“算得准”，大语言模型负责“讲得懂”，个人数据库负责“看得远”。

- **Pikafish**计算最佳走法、局面评分和推荐变化。
- **DeepSeek AI 教练**基于引擎证据解释失误、提出训练任务和复盘问题。
- **SQLite 个人数据库**长期保存每盘棋、每一步、思考时间和复盘结果。

```text
记录对局 → 引擎复盘 → AI 解释 → 更新个人档案 → 生成针对训练 → 检验提升效果
```

这个项目也是未来个性化数学教育系统的实验原型：规则和计算工具保证判断准确，大语言模型负责教学解释，学习者数据库负责长期诊断。

## 当前可运行功能

- 绘制标准棋盘和中文棋子
- 鼠标控制红方，Python 弱 AI 控制黑方
- 校验全部棋子规则、将军、将帅照面和胜负
- 自动记录双方每一步、吃子、思考时间和走前/走后局面
- 使用 SQLite 保存全部对局数据
- 红方每走一步，Pikafish 自动进行两次 UCI 搜索
- 比较最佳评分和实际落子评分，计算局面损失
- 显示中文着法、推荐着法、推荐变化和基础训练建议
- 累计统计优秀、轻微失误、明显失误和严重失误
- 可选接入 DeepSeek，为局面损失超过 30 的走法生成结构化个性化讲解并保存到数据库
- 支持随时开始新对局

## 项目结构

```text
xiangqi_game.*           象棋规则、胜负判断和标准走棋记录
xiangqi_board_widget.*   棋盘绘制、鼠标交互和弱 AI 对弈
game_database.*          SQLite 对局、走法、分析结果和长期统计
pikafish_analyzer.*      Pikafish UCI 通信、FEN/坐标转换和评价
deepseek_coach.*         DeepSeek 异步请求、提示词、JSON 校验和降级处理
credential_store.*       Windows Credential Manager 密钥安全存储
engine_py/               自己编写的 Python 弱引擎
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

## 启用 DeepSeek AI 教练

程序不会把 API 密钥写入源码、配置文件或数据库。请先在 DeepSeek 控制台创建一个新密钥，然后：

1. 启动象棋程序。
2. 点击右侧的 **DeepSeek 设置**。
3. 输入新密钥。
4. 点击 **保存并测试连接**。

密钥会保存在 Windows Credential Manager 中，以后启动程序会自动读取。设置窗口不会回显已经保存的密钥，并支持替换或删除。

环境变量 `DEEPSEEK_API_KEY` 仍作为开发和临时运行方式保留，并且优先于 Windows 凭据：

```powershell
$env:DEEPSEEK_API_KEY = "这里填写新密钥"
```

在同一个 PowerShell 窗口中启动 Qt Creator，或者在 Qt Creator 的项目运行环境中添加该变量。不要把真实密钥提交到 Git。

系统使用 `deepseek-v4-flash` 和 JSON 输出。为了控制费用，优秀着法使用本地说明，只有局面损失超过 30 时才请求 DeepSeek。发送给模型的内容仅包含走法、引擎评分、推荐变化、局面编码和匿名累计统计，不包含姓名等身份信息。没有设置密钥、网络失败或返回格式错误时，Pikafish 复盘与数据库功能仍会正常工作。

## 自动化验证

项目提供 `training_smoke_test`，验证完整链路：

```text
合法走棋 → 生成 MoveRecord → 写入 SQLite → Pikafish 分析 → 写入长期统计
```

可以在构建目录运行 `training_smoke_test.exe`。

## 数据库内容

- `games`：开始时间、结束时间和胜负
- `moves`：每一步坐标、棋子、吃子、思考时间和局面
- `analyses`：实际走法、推荐走法、评分损失、错误等级和推荐变化
- `coaching`：DeepSeek 诊断、证据、训练任务和复盘问题

## 下一阶段

- 对局历史列表和逐步回放
- 从严重失误局面自动生成训练题
- 更细致的错误类型：漏看威胁、子力保护、开局效率、残局等
- 汇总多盘 DeepSeek 建议，生成周度个性化复盘报告
- 追踪相同错误是否随着训练逐渐减少
