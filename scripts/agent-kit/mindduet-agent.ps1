param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidateSet("packy", "deepseek")]
    [string]$Provider,

    [string]$ProjectRoot = (Get-Location).Path,

    [switch]$FullAccess,

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$AgentArguments
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "MindDuetCredential.ps1")

$packyCredentialTarget = "MindDuet.AgentKit.PackyApiKey"
$deepSeekCredentialTarget = "GuanghuiEducationLab.XiangqiTraining.DeepSeekApiKey"
$resolvedProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path

function Find-CodexExecutable {
    $pathCommand = Get-Command "codex.exe" -ErrorAction SilentlyContinue
    if ($null -ne $pathCommand) {
        return $pathCommand.Source
    }

    $desktopBinRoot = Join-Path $env:LOCALAPPDATA "OpenAI\Codex\bin"
    if (Test-Path -LiteralPath $desktopBinRoot) {
        $desktopCandidate = Get-ChildItem -LiteralPath $desktopBinRoot -Recurse -Filter "codex.exe" -File -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTime -Descending |
            Select-Object -First 1
        if ($null -ne $desktopCandidate) {
            return $desktopCandidate.FullName
        }
    }

    $npmCandidate = Join-Path $env:APPDATA "npm\codex.cmd"
    if (Test-Path -LiteralPath $npmCandidate) {
        return $npmCandidate
    }

    throw "Codex CLI was not found. Install Codex or the Codex desktop application first."
}

function Find-AiderExecutable {
    $pathCommand = Get-Command "aider.exe" -ErrorAction SilentlyContinue
    if ($null -ne $pathCommand) {
        return $pathCommand.Source
    }

    $userCandidate = Join-Path $env:USERPROFILE ".local\bin\aider.exe"
    if (Test-Path -LiteralPath $userCandidate) {
        return $userCandidate
    }

    throw "Aider was not found. Install it with the official aider-install package first."
}

function Invoke-PackyAgent {
    $packyApiKey = (Get-MindDuetCredential -Target $packyCredentialTarget)
    if ([string]::IsNullOrWhiteSpace($packyApiKey)) {
        throw "Packy credential is missing. Run the MindDuet Agent Kit installer with -PackyKeyFile."
    }

    $packyProfile = Join-Path $env:USERPROFILE ".codex\packy.config.toml"
    if (-not (Test-Path -LiteralPath $packyProfile)) {
        throw "Packy Codex profile is missing: $packyProfile"
    }

    $codexExecutable = Find-CodexExecutable
    $previousPackyApiKey = $env:PACKY_API_KEY
    try {
        $env:PACKY_API_KEY = $packyApiKey.Trim()
        $codexBaseArguments = @("-p", "packy", "-C", $resolvedProjectRoot)
        if ($FullAccess) {
            $codexBaseArguments += @("-s", "danger-full-access", "-a", "never")
        }
        & $codexExecutable @codexBaseArguments @AgentArguments
        $script:mindDuetAgentExitCode = $LASTEXITCODE
    } finally {
        if ($null -eq $previousPackyApiKey) {
            Remove-Item Env:PACKY_API_KEY -ErrorAction SilentlyContinue
        } else {
            $env:PACKY_API_KEY = $previousPackyApiKey
        }
        $packyApiKey = $null
    }
}

function Invoke-DeepSeekAgent {
    if ($FullAccess) {
        throw "-FullAccess is supported only by the Packy Codex provider."
    }

    $deepSeekApiKey = (Get-MindDuetCredential -Target $deepSeekCredentialTarget)
    if ([string]::IsNullOrWhiteSpace($deepSeekApiKey)) {
        throw "DeepSeek credential is missing. Save it in MindDuet Chess first."
    }

    $aiderExecutable = Find-AiderExecutable
    $previousDeepSeekApiKey = $env:DEEPSEEK_API_KEY
    $previousPythonIoEncoding = $env:PYTHONIOENCODING
    $previousPythonUtf8 = $env:PYTHONUTF8
    $previousOutputEncoding = [Console]::OutputEncoding
    try {
        $env:DEEPSEEK_API_KEY = $deepSeekApiKey.Trim()
        $env:PYTHONIOENCODING = "utf-8"
        $env:PYTHONUTF8 = "1"
        [Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)

        $aiderBaseArguments = @(
            "--model", "deepseek/deepseek-v4-flash",
            "--no-auto-commits",
            "--no-analytics",
            "--no-check-update",
            "--no-gitignore",
            "--no-pretty"
        )
        $agentsFile = Join-Path $resolvedProjectRoot "AGENTS.md"
        if (Test-Path -LiteralPath $agentsFile) {
            $aiderBaseArguments += @("--read", $agentsFile)
        }

        & $aiderExecutable @aiderBaseArguments @AgentArguments
        $script:mindDuetAgentExitCode = $LASTEXITCODE
    } finally {
        if ($null -eq $previousDeepSeekApiKey) {
            Remove-Item Env:DEEPSEEK_API_KEY -ErrorAction SilentlyContinue
        } else {
            $env:DEEPSEEK_API_KEY = $previousDeepSeekApiKey
        }
        if ($null -eq $previousPythonIoEncoding) {
            Remove-Item Env:PYTHONIOENCODING -ErrorAction SilentlyContinue
        } else {
            $env:PYTHONIOENCODING = $previousPythonIoEncoding
        }
        if ($null -eq $previousPythonUtf8) {
            Remove-Item Env:PYTHONUTF8 -ErrorAction SilentlyContinue
        } else {
            $env:PYTHONUTF8 = $previousPythonUtf8
        }
        [Console]::OutputEncoding = $previousOutputEncoding
        $deepSeekApiKey = $null
    }
}

try {
    $script:mindDuetAgentExitCode = 1
    if ($Provider -eq "packy") {
        Invoke-PackyAgent
    } else {
        Invoke-DeepSeekAgent
    }
} catch {
    Write-Error $_.Exception.Message
    exit 1
}

exit $script:mindDuetAgentExitCode
