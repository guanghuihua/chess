param(
    [string]$ProjectRoot = (Get-Location).Path
)

$ErrorActionPreference = "Stop"

$resolvedProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
$projectName = Split-Path -Leaf $resolvedProjectRoot
$agentsPath = Join-Path $resolvedProjectRoot "AGENTS.md"
$gitignorePath = Join-Path $resolvedProjectRoot ".gitignore"
$vscodeDirectory = Join-Path $resolvedProjectRoot ".vscode"
$tasksPath = Join-Path $vscodeDirectory "tasks.json"

if (-not (Test-Path -LiteralPath $agentsPath)) {
    $agentsTemplate = @'
# __PROJECT_NAME__ Agent Guide

## Communication

- Communicate with the user primarily in Chinese.
- Briefly correct important English mistakes while still answering the request.
- Lead with the result, then explain changes and verification.

## Project Purpose

- Describe the purpose, users, and success criteria of this project here.
- Read the README and relevant design documents before architectural changes.

## Working Rules

- Preserve unrelated user changes.
- Keep changes focused and add proportionate verification.
- Explain important design decisions and likely pitfalls.
- Never put API keys, passwords, private data, build outputs, or local databases in Git.

## Verification

- Record the build, test, lint, or visual checks required by this project here.

## Git

- Inspect `git status` and `git diff` before committing.
- After completing and verifying requested repository changes, create a focused commit and push unless the user explicitly asks not to push.
'@
    $agentsTemplate = $agentsTemplate.Replace("__PROJECT_NAME__", $projectName)
    Set-Content -LiteralPath $agentsPath -Value $agentsTemplate -Encoding UTF8
    Write-Output "Created AGENTS.md"
} else {
    Write-Output "Kept existing AGENTS.md"
}

$ignoreEntries = @("/APIKEY", ".env", ".env.*", ".aider*")
$existingIgnore = if (Test-Path -LiteralPath $gitignorePath) {
    @(Get-Content -LiteralPath $gitignorePath -Encoding UTF8)
} else {
    @()
}
foreach ($entry in $ignoreEntries) {
    if ($existingIgnore -notcontains $entry) {
        Add-Content -LiteralPath $gitignorePath -Value $entry -Encoding UTF8
        $existingIgnore += $entry
    }
}

New-Item -ItemType Directory -Path $vscodeDirectory -Force | Out-Null
$packyTask = [ordered]@{
    label = "Start Packy Codex Agent"
    type = "shell"
    command = "powershell.exe"
    args = @(
        "-NoLogo", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
        '${env:LOCALAPPDATA}\MindDuet\AgentKit\mindduet-agent.ps1',
        "packy", "-ProjectRoot", '${workspaceFolder}'
    )
    problemMatcher = @()
    presentation = [ordered]@{ reveal = "always"; focus = $true; panel = "dedicated"; clear = $true }
    runOptions = [ordered]@{ instanceLimit = 1 }
}
$deepSeekTask = [ordered]@{
    label = "Start DeepSeek Agent (Cheap Tasks)"
    type = "shell"
    command = "powershell.exe"
    args = @(
        "-NoLogo", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
        '${env:LOCALAPPDATA}\MindDuet\AgentKit\mindduet-agent.ps1',
        "deepseek", "-ProjectRoot", '${workspaceFolder}'
    )
    problemMatcher = @()
    presentation = [ordered]@{ reveal = "always"; focus = $true; panel = "dedicated"; clear = $true }
    runOptions = [ordered]@{ instanceLimit = 1 }
}

if (-not (Test-Path -LiteralPath $tasksPath)) {
    $tasksDocument = [ordered]@{ version = "2.0.0"; tasks = @($packyTask, $deepSeekTask) }
    $tasksDocument | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $tasksPath -Encoding UTF8
    Write-Output "Created .vscode/tasks.json"
} else {
    try {
        $tasksDocument = Get-Content -LiteralPath $tasksPath -Raw -Encoding UTF8 | ConvertFrom-Json
        $tasks = @($tasksDocument.tasks)
        $changed = $false
        if ($tasks.label -notcontains $packyTask.label) {
            $tasks += [pscustomobject]$packyTask
            $changed = $true
        }
        if ($tasks.label -notcontains $deepSeekTask.label) {
            $tasks += [pscustomobject]$deepSeekTask
            $changed = $true
        }
        if ($changed) {
            $tasksDocument.tasks = $tasks
            $tasksDocument | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $tasksPath -Encoding UTF8
            Write-Output "Updated .vscode/tasks.json"
        } else {
            Write-Output "Kept existing MindDuet VS Code tasks"
        }
    } catch {
        Write-Warning "Existing tasks.json contains JSONC or invalid JSON, so it was not changed. Add the MindDuet tasks manually."
    }
}

Write-Output "MindDuet Agent Kit initialized for: $resolvedProjectRoot"
