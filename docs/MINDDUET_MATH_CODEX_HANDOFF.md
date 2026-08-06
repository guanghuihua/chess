# MindDuet Math：交给 Codex 的项目开发说明

> 本文件是一份独立、可直接复制到新项目目录的开发交接文档。阅读本文件的 Codex 应先检查所在目录中的 TeX、PDF 和已有文件，然后直接开始建立可运行的最小版本。

## 1. 项目发起人的背景

项目发起人是一名中国高中数学教师，已经积累了自己的教学经验总结和题库。现有教学资料主要由 TeX 源文件和 PDF 文件组成。

项目发起人希望利用 AI 为每名学生建立独立的学习数据库，根据学生长期作答数据识别稳定的薄弱点，生成个性化诊断、训练任务和阶段报告，最终形成教师与 AI 协作的高中数学学习系统。

项目名称：**MindDuet Math**。

MindDuet 的长期目标不是让 AI 取代教师，而是探索人与 AI 如何共同观察、共同判断和持续改进。

## 2. 项目的来源与已经验证的经验

MindDuet Math 来源于一个已经可以运行的中国象棋个性化训练原型 MindDuet Xiangqi。象棋系统已经实现或验证了：

- 多用户及相互独立的个人数据；
- 保存每盘对局和每一步行为；
- 使用可靠引擎评价实际行为；
- 使用大语言模型解释引擎证据；
- 从历史错误中生成训练题；
- 间隔复习、训练记录和掌握度更新；
- 每完成一定数量的有效任务生成阶段报告；
- 使用卡片、分布图和趋势图展示个人画像；
- API 密钥与源码分离；
- 数据库迁移和自动化测试。

数学项目应继承这个闭环：

```text
学生作答
→ 答案与步骤判定
→ 知识点和错因诊断
→ 保存诊断证据
→ 更新个人画像
→ 生成针对性训练
→ 延迟复习与再次检测
→ 生成学生报告和教师报告
```

## 3. 核心思想

数学系统由四部分协作：

1. **判题器、标准答案、评分规则和数学工具**负责基础判断准确。
2. **大语言模型**负责解释错因、分层提示、反思提问和训练建议。
3. **个人学习数据库**负责保存长期证据、更新学生画像和观察发展趋势。
4. **教师**负责教学设计、审核重要结论和处理复杂或不确定的情况。

必须坚持以下边界：

- 大语言模型不能单独决定答案是否正确。
- 不得根据一次错误断言学生已经形成某种固定缺陷。
- 所有重要诊断必须能够追溯到具体题目、答案和解题步骤。
- AI 不确定时必须明确表示不确定，并交给教师审核。
- 教师能够修改诊断、评分、知识点标签和训练安排。
- AI 生成的新题和答案在用于正式教学前必须经过教师审核。

## 4. 当前资料目录的处理原则

阅读本文件的 Codex 应首先递归检查当前项目目录中的：

- `*.tex`
- `*.pdf`
- 图片、表格和 TeX 引用的其他资源
- 已有目录结构、README、Git 状态和构建脚本

处理教学资料时遵守：

1. 原始 TeX/PDF 是教师资产，不得批量覆盖、移动或删除。
2. 优先把 TeX 作为可解析的内容来源，PDF 作为渲染结果或补充参考。
3. 第一阶段不要试图一次导入全部资料。
4. 先统计文件、主题、题目数量、宏命令和常见题目结构。
5. 从资料最完整、边界最清晰的一个小专题建立首个样本题库。
6. 如果题目结构无法可靠自动识别，先生成导入报告和待人工确认清单，不要猜测。
7. 每个导入条目记录源文件相对路径、内容哈希和必要的行号/定位信息，以便追溯。
8. 自动生成的数据放在独立目录或数据库中，不污染原始资料目录。

建议先生成以下只读分析产物：

```text
data/import_reports/source_inventory.json
data/import_reports/tex_structure_report.md
data/import_reports/topic_candidates.json
```

## 5. 第一版范围

第一版不得覆盖整个高中数学。应从当前资料中选择一个资料完整的小专题，例如：

- 函数的单调性与最值；
- 基本不等式；
- 数列通项与求和；
- 三角函数图像与性质；
- 立体几何中的线面关系。

如果教师没有提前指定专题，Codex 应根据 TeX 资料覆盖情况选择最适合建立闭环的最小专题，并在 README 中写明选择依据。

建议首个专题包含：

- 10～15 个知识点；
- 首批 20 道可完整运行的题目；
- 后续扩充到 50～100 道教师审核题目；
- 5～10 种常见错误类型。

## 6. 最小可行产品（MVP）

第一版必须跑通下面这个完整场景：

> 教师创建或导入一个专题和题目。一名学生登录后完成 10 道题。系统保存最终答案、解题步骤、用时、信心和提示使用情况，完成基础判题并标记相关知识点。教师可以查看和修正诊断。系统随后生成 5 道针对性训练任务。训练完成后，系统显示掌握度变化，并分别生成学生报告和教师报告。

MVP 功能：

1. 创建、切换学生，每名学生数据严格隔离。
2. 教师查看知识点、题目、标准答案、评分规则和来源。
3. 学生查看题目，提交最终答案和文字/LaTeX 解题步骤。
4. 记录作答时间、自评信心、提示次数和修改次数。
5. 客观题直接判定；代数式使用 SymPy 检查等价性。
6. 解答题先根据教师评分点给出辅助评价，再由教师确认。
7. 保存概念、运算、审题、分类讨论、推理条件、方法选择和表达等错误类型。
8. 展示知识点掌握度、错误分布、训练正确率和最近表现趋势。
9. 根据历史证据生成小型训练包并安排间隔复习。
10. 每完成 10～20 道有效题目生成一次阶段报告。

## 7. 第一版不要做的事情

- 不训练大语言模型。
- 不直接使用强化学习推荐题目。
- 不一开始使用微服务架构。
- 不一开始引入 Redis、消息队列和 Kubernetes。
- 不先做覆盖整个高中数学的大型知识图谱。
- 不允许 AI 自动生成大量未经教师审核的正式题目。
- 不为了界面效果而跳过数据追溯、判题和测试。

第一阶段使用透明、可解释的训练推荐规则：

```text
推荐优先级 =
知识点薄弱程度
× 错误重复程度
× 知识点重要程度
× 遗忘风险
× 题目难度适配度
```

## 8. 推荐开发环境

目标开发电脑运行 Windows 和 WSL 2，当前没有 Qt。数学项目不需要安装 Qt，应开发为浏览器访问的 Web 应用。

推荐环境：

- WSL 2 + Ubuntu
- Windows 版 VS Code + WSL 扩展
- Python 3.12
- `uv` 管理 Python、虚拟环境和依赖锁
- FastAPI
- Jinja2 + HTMX 或少量原生 JavaScript
- MathJax 或 KaTeX 显示数学公式
- SQLite
- SQLAlchemy + Alembic
- SymPy
- Pydantic
- pytest + Ruff

第一阶段不需要 Docker。开始使用 PostgreSQL、Redis，或者准备部署时再加入 Docker Compose。

项目源代码应保存在 WSL Linux 文件系统，例如：

```text
~/projects/mindduet-math
```

不要把主要开发目录放在 `/mnt/c/...`。

建议初始化命令：

```bash
sudo apt update
sudo apt install -y git curl build-essential sqlite3 libsqlite3-dev

curl -LsSf https://astral.sh/uv/install.sh | sh

uv init
uv add fastapi "uvicorn[standard]" sqlalchemy alembic pydantic \
    sympy jinja2 python-multipart openai
uv add --dev pytest pytest-asyncio httpx ruff
```

## 9. 建议代码结构

Codex 可以根据已有目录调整，但推荐保持如下边界：

```text
mindduet-math/
├── README.md
├── AGENTS.md
├── pyproject.toml
├── uv.lock
├── .env.example
├── .gitignore
├── app/
│   ├── main.py
│   ├── config.py
│   ├── database.py
│   ├── models/
│   ├── schemas/
│   ├── repositories/
│   ├── services/
│   │   ├── grading.py
│   │   ├── mastery.py
│   │   ├── recommendation.py
│   │   ├── reporting.py
│   │   └── ai_coach.py
│   ├── routes/
│   ├── templates/
│   └── static/
├── content/
│   ├── importers/
│   └── normalized/
├── data/
│   └── import_reports/
├── migrations/
├── scripts/
├── tests/
└── docs/
```

不要为了满足该结构而移动教师原始 TeX/PDF；可以通过配置指定原始资料目录。

## 10. 核心数据模型

至少设计以下实体：

### `users`

- `id`
- `name`
- `role`：student/teacher/admin
- `created_at`

### `student_profiles`

- `user_id`
- 当前摘要
- 最近更新时间

### `knowledge_points`

- `id`
- `code`
- `name`
- `parent_id`
- `description`
- `importance`

### `questions`

- `id`
- `title`
- `statement_tex`
- `answer_tex`
- `solution_tex`
- `question_type`
- `difficulty`
- `grading_policy`
- `review_status`
- `source_path`
- `source_hash`

### `question_knowledge`

- `question_id`
- `knowledge_point_id`
- `weight`

### `attempts`

- `id`
- `student_id`
- `question_id`
- `final_answer`
- `started_at`
- `submitted_at`
- `duration_ms`
- `confidence_before`
- `confidence_after`
- `hint_count`
- `revision_count`
- `auto_score`
- `final_score`
- `review_status`

### `solution_steps`

- `attempt_id`
- `step_index`
- `content_tex`
- `duration_ms`

### `diagnoses`

- `attempt_id`
- `knowledge_point_id`
- `error_type`
- `evidence`
- `confidence`
- `source`：rule/model/teacher
- `review_status`

### `training_tasks`

- `student_id`
- `question_id`
- `reason`
- `source_diagnosis_id`
- `scheduled_at`
- `completed_at`

### `mastery_history`

- `student_id`
- `knowledge_point_id`
- `mastery`
- `evidence_count`
- `recorded_at`

### `stage_reports`

- `student_id`
- `range_start`
- `range_end`
- `student_summary`
- `teacher_summary`
- `evidence_json`
- `created_at`

### `teacher_reviews`

- `teacher_id`
- `target_type`
- `target_id`
- `original_value`
- `revised_value`
- `comment`
- `created_at`

## 11. 判题与诊断设计

判题采用分层策略：

1. 选择题、判断题：确定性比较。
2. 数值答案：规范化单位和允许误差后比较。
3. 代数式：SymPy 解析、化简和等价性检查；必须处理解析失败。
4. 区间、集合、方程组：转换为结构化对象后比较。
5. 解答题：教师定义评分点；规则程序先匹配可确定部分。
6. 大语言模型：只能基于题目、评分点和学生步骤提出辅助判断与证据。
7. 教师审核：决定不确定或高影响评价的最终结果。

错误类型第一版可以使用：

- `concept_error`
- `calculation_error`
- `reading_error`
- `missing_case`
- `missing_condition`
- `method_selection`
- `reasoning_gap`
- `expression_issue`
- `unknown`

## 12. 个人画像设计

个人画像必须是随时间变化的证据汇总，而不是固定标签。至少展示：

- 各知识点掌握度与证据数量；
- 高频错误类型；
- 平均作答时间及趋势；
- 提示依赖程度；
- 自评信心和实际正确率的偏差；
- 训练完成率和训练正确率；
- 最近阶段进步与仍需关注的问题。

画像指标要设置合理边界，避免单个异常分数、超长时间或错误数据导致平均值溢出或失真。所有聚合查询都应测试空数据、异常值和历史数据兼容性。

## 13. AI 教练接口

AI 教练必须与核心判题解耦。即使未配置 API、请求失败或输出不合法，答题、判题、数据库和教师审核仍然正常工作。

### PackyAPI 配置

本项目计划使用 [PackyAPI](https://docs.packyapi.ai/) 作为可配置的 AI 接口服务。其文档说明它兼容 OpenAI 接口协议，OpenAI 兼容 Base URL 为：

```text
https://www.packyapi.ai/v1
```

PackyAPI 的令牌分组决定该令牌能够使用哪些模型。创建令牌后，应在 PackyAPI 模型广场中确认当前分组的模型名称和费用，不在源码中写死未经验证的模型名称。参考文档：

- [购买额度](https://docs.packyapi.ai/docs/register/3-quota.html)
- [创建 API 令牌](https://docs.packyapi.ai/docs/register/4-token.html)
- [PackyAPI 快速开始与端点说明](https://docs.packyapi.ai/docs/register/)

开发环境使用 `.env`，只提交不含真实值的 `.env.example`：

```dotenv
MINDDUET_AI_ENABLED=false
MINDDUET_AI_PROVIDER=packyapi
MINDDUET_AI_BASE_URL=https://www.packyapi.ai/v1
MINDDUET_AI_API_KEY=
MINDDUET_AI_MODEL=
MINDDUET_AI_TIMEOUT_SECONDS=45
```

应用程序通过统一的 `AIProvider` 接口调用模型，不允许业务代码直接依赖 PackyAPI。至少实现：

```python
class AIProvider(Protocol):
    async def generate_structured(self, request: CoachingRequest) -> CoachingResult:
        ...
```

PackyAPI 适配器可以使用 OpenAI Python SDK，并把 `base_url` 指向环境变量：

```python
from openai import AsyncOpenAI

client = AsyncOpenAI(
    api_key=settings.ai_api_key,
    base_url=settings.ai_base_url,
)
```

不要在文档、示例或测试中填写真正的 PackyAPI Key。启动时只报告“已配置/未配置”，不得回显完整密钥。

建议为两个用途创建两个不同令牌：

1. **Codex 开发令牌**：仅供 Codex CLI/IDE 开发使用，选择 PackyAPI 的 Codex 分组。
2. **MindDuet Math 应用令牌**：仅供运行中的 AI 教练使用，选择实际所用模型支持的分组，并设置独立额度和名称。

两个令牌分开后，可以分别查看费用、限制额度和撤销权限。不要把 Codex 的 `~/.codex/auth.json` 当作应用密钥来源，也不要让应用读取 Codex 的配置目录。

由于 PackyAPI 是第三方中转服务，真实学生数据进入试验前，需要确认其隐私条款和数据处理方式。开发阶段只发送匿名或虚构数据；正式使用时只发送完成当前诊断所必需的匿名化内容。

要求：

- API Key 通过环境变量或系统密钥管理工具读取。
- `.env` 必须被 Git 忽略，只提交 `.env.example`。
- 不得在代码、测试、日志、数据库或提示词样例中放入真实密钥。
- 发送给外部模型前移除姓名、学号、联系方式等直接身份信息。
- 使用 Pydantic 校验模型 JSON 输出。
- 保存模型名称、提示词版本、输入证据标识和原始响应状态。
- 为请求设置超时、重试上限和取消机制。
- 学生撤回答案或切换用户后，过期异步响应不得写入当前任务。

推荐 AI 输出结构：

```json
{
  "diagnosis": "",
  "evidence": [""],
  "hint_levels": ["", "", ""],
  "reflection_question": "",
  "training_focus": "",
  "confidence": 0.0,
  "needs_teacher_review": true
}
```

## 14. 隐私和数据安全

- 只收集实现教学目标所需的最少数据。
- 开发样本优先使用匿名学生或虚构数据。
- 外部模型只接收匿名化后的必要内容。
- 数据库支持备份、导出和删除单个学生数据。
- 不在控制台输出学生完整答案和身份信息的组合。
- 正式邀请学生使用前，补充清晰的数据用途说明和授权流程。
- 教学原始资料、学生数据和 API 密钥不能进入公开仓库。

## 15. 从象棋项目吸取的工程教训

1. **先跑通闭环，再提升算法。** 弱算法但完整的数据闭环比没有数据的复杂模型更有价值。
2. **准确判断和语言解释分离。** 不让大语言模型替代可验证的计算与规则。
3. **每个结论保存证据。** 报告必须能返回原题、原答案和诊断来源。
4. **多用户隔离从第一天设计。** 所有作答、训练、报告和画像查询必须带用户范围。
5. **异步任务必须防止过期写入。** 用户切换、撤回或重新提交后要丢弃旧响应。
6. **异常值需要边界。** 平均分、损失、耗时和趋势图不能被单个异常数据破坏。
7. **阶段报告需要有效样本门槛。** 空记录、放弃任务和测试数据不应进入正式统计。
8. **自动化测试覆盖用户隔离。** 测试两个用户的数据不能相互污染。
9. **UI 显示最近发生了什么。** 学生应清楚看到刚提交的答案、系统判断和下一步任务。
10. **数据库迁移必须可重复。** 不能通过手工修改本地数据库维持开发。
11. **密钥永远不进入 Git。** 提交前自动扫描常见密钥格式。
12. **本地功能不能依赖 AI 在线可用。** AI 是增强层，不是系统单点故障。

## 16. 自动化测试最低要求

至少建立：

- 数据库初始化与重复迁移测试；
- 学生数据隔离测试；
- 题目导入幂等性测试；
- 客观题判题测试；
- SymPy 等价性和解析失败测试；
- 作答记录完整性测试；
- 教师修正覆盖自动评分测试；
- 掌握度更新边界测试；
- 训练推荐可追溯性测试；
- AI 未配置、超时和非法 JSON 降级测试；
- 关键页面冒烟测试；
- 完整闭环端到端测试。

第一条端到端测试应验证：

```text
创建教师和学生
→ 导入知识点与题目
→ 学生提交答案
→ 自动判题
→ 教师审核
→ 更新掌握度
→ 生成训练任务
→ 完成训练
→ 生成阶段报告
```

## 17. 开发阶段与验收标准

### 阶段 0：资料盘点

交付物：文件清单、TeX 结构报告、候选专题、风险说明。

验收：没有修改原始资料；报告能够追溯到源文件。

### 阶段 1：数据闭环

交付物：用户、知识点、题库、答题、判题、作答记录和教师查看页面。

验收：没有 AI API 也能完整运行；两个学生数据严格隔离。

### 阶段 2：个人画像与训练

交付物：掌握度、错误分布、训练推荐、间隔复习和可视化。

验收：每条训练任务能够说明“为什么推荐”和“来自哪次作答”。

### 阶段 3：AI 教练

交付物：分层提示、错因解释、反思问题和教师报告。

验收：结构化输出经过校验；失败时系统能够降级；教师可以修正。

### 阶段 4：小规模教学试验

交付物：匿名试验数据、训练前后比较、教师修正统计和学生反馈。

验收：不仅证明软件能运行，还要观察学生是否减少了同类错误。

## 18. Codex 接手后的第一轮任务

阅读本文件的 Codex 应按下面顺序直接行动：

1. 检查 Git 状态和目录结构，不覆盖用户已有修改。
2. 递归统计 TeX/PDF 和相关资源，读取足够样本理解实际格式。
3. 编写资料盘点报告，选择最小可行专题并说明依据。
4. 创建 README、AGENTS.md、`.gitignore`、`.env.example` 和 Python 项目骨架。
5. 建立数据库模型、Alembic 迁移和测试数据库。
6. 实现教师/学生、知识点、题目和作答的最小闭环。
7. 从选定专题导入首批少量题目，并保留来源追溯信息。
8. 实现确定性判题和 SymPy 等价性检查。
9. 实现教师查看与修正页面。
10. 运行测试并在 README 中写出 WSL 启动命令。

除非遇到会改变教学含义、破坏原始资料或涉及隐私授权的关键选择，否则不要停留在讨论和空架构阶段，应做出可以启动、可以测试、可以演示的第一版。

## 19. 第一轮完成定义

第一轮开发完成时，至少满足：

- 在 WSL 中执行一组明确命令即可启动系统；
- 浏览器能够打开教师端和学生端；
- 已导入一个小专题及若干真实题目；
- 可以创建两个学生并验证数据隔离；
- 学生能够提交答案和步骤；
- 系统能够完成至少一种确定性判题和一种 SymPy 判题；
- 教师能够查看并修正结果；
- 数据保存到 SQLite；
- 自动化测试通过；
- 原始 TeX/PDF 没有被破坏；
- README 说明下一阶段尚未实现的内容。

---

项目口号：

> **Human and AI, learning together.**

中文含义：人与 AI，共同学习，共同成长。
