# MindDuet Chess Agent Guide

## Communication

- Communicate with the user primarily in Chinese.
- When the user writes English, briefly correct important grammar or wording mistakes while still answering the request.
- Lead with the result, then explain the cause, changes, and verification in plain language.

## Project Purpose

- This repository is **MindDuet Chess**, a personalized Chinese-chess training system.
- Its central architecture is: Pikafish calculates accurately, the AI coach explains clearly, and the personal database tracks long-term patterns.
- Preserve the distinction between engine evidence, AI interpretation, and user learning history.
- The long-term goal is targeted training based on real games, mistakes, undo branches, reviews, and repeated decision patterns.

## Technology and Structure

- The desktop application uses C++17, Qt 6 Widgets, CMake, SQLite, and Qt Network.
- `xiangqi_game.*` owns Xiangqi rules, legality, turns, check, and game termination.
- `xiangqi_board_widget.*` owns board rendering, interaction, engine turns, and the analysis workspace.
- `pikafish_analyzer.*` owns UCI communication and engine evaluation.
- `deepseek_coach.*` owns Packy/DeepSeek requests and AI response handling.
- `game_database.*` owns persistent games, moves, analyses, undo evidence, profiles, and training data.
- `engine_py/` is an educational Python engine and must not silently replace Pikafish in the production path.

## Implementation Rules

- Read `README.md` and the relevant design document in `docs/` before making architectural changes.
- Keep Xiangqi rule validation in `XiangqiGame`; do not duplicate partial rule logic in UI or AI code.
- Never trust a move returned by an engine or model without validating it through the game rules.
- Keep engine scoring, AI prose, and database updates asynchronous so the UI thread remains responsive.
- Preserve undone moves as learning evidence. Undoing the live board must not delete the original move, engine analysis, or coaching record.
- AI advice must be concrete and grounded in the current position, full move history, engine evidence, and undo branches. Avoid generic encouragement and repetitive boilerplate.
- Keep UI text professional, concise, and readable in Chinese. Prefer native Qt drawing and layouts over fragile fixed coordinates.
- Maintain backward compatibility for existing SQLite databases; schema changes require migrations or defensive `ALTER TABLE` logic.

## Security and Privacy

- Never place API keys, access tokens, passwords, or private user data in source code, logs, screenshots, commits, or documentation examples.
- Read Packy credentials from `APIKEY` or supported environment variables, and DeepSeek credentials from Windows Credential Manager or environment variables.
- `APIKEY`, `.env*`, databases, build outputs, downloaded engines, and release artifacts must remain untracked.
- Send only the minimum relevant game or learning context to external AI services.

## Build and Verification

- Preferred local kit: Qt 6.10.1 MinGW 64-bit with CMake and Ninja.
- Build the existing Qt Creator build directory with:

```powershell
E:\QT\Tools\CMake_64\bin\cmake.exe --build E:\Chess\build\Desktop_Qt_6_10_1_MinGW_64_bit-Debug --target all
```

- Run tests after behavior or database changes:

```powershell
E:\QT\Tools\CMake_64\bin\ctest.exe --test-dir E:\Chess\build\Desktop_Qt_6_10_1_MinGW_64_bit-Debug --output-on-failure
```

- If that build directory does not exist on another computer, configure through Qt Creator first instead of hard-coding a different compiler toolchain.
- Add or update a focused regression test for rule, database, JSON parsing, undo, profile, or training behavior changes.
- For visual changes, build the application and inspect the affected screen; compilation alone is not sufficient verification.

## Git Workflow

- Preserve unrelated user changes and inspect `git status` and `git diff` before editing or committing.
- Do not commit generated build directories, executables, databases, credentials, downloaded engines, or temporary screenshots.
- After completing and verifying requested repository changes, create a focused commit and push the current branch unless the user explicitly asks not to push.
- Report the commit hash, verification performed, and any remaining limitation.
