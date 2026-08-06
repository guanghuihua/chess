param(
    [string]$PackyKeyFile = ""
)

$ErrorActionPreference = "Stop"

$sourceDirectory = Join-Path $PSScriptRoot "agent-kit"
$installDirectory = Join-Path $env:LOCALAPPDATA "MindDuet\AgentKit"
$codexDirectory = Join-Path $env:USERPROFILE ".codex"
$packyProfilePath = Join-Path $codexDirectory "packy.config.toml"

if (-not (Test-Path -LiteralPath $sourceDirectory)) {
    Write-Error "Agent Kit source directory was not found: $sourceDirectory"
    exit 1
}

New-Item -ItemType Directory -Path $installDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $codexDirectory -Force | Out-Null

$kitFiles = @(
    "MindDuetCredential.ps1",
    "mindduet-agent.ps1",
    "mindduet-init.ps1",
    "mindduet-agent.cmd",
    "mindduet-init.cmd"
)
foreach ($fileName in $kitFiles) {
    Copy-Item -LiteralPath (Join-Path $sourceDirectory $fileName) -Destination (Join-Path $installDirectory $fileName) -Force
}

if (-not (Test-Path -LiteralPath $packyProfilePath)) {
    Copy-Item -LiteralPath (Join-Path $sourceDirectory "packy.config.toml") -Destination $packyProfilePath
    Write-Output "Created Packy Codex profile"
} else {
    Write-Output "Kept existing Packy Codex profile"
}

. (Join-Path $installDirectory "MindDuetCredential.ps1")
if (-not [string]::IsNullOrWhiteSpace($PackyKeyFile)) {
    $resolvedKeyFile = (Resolve-Path -LiteralPath $PackyKeyFile).Path
    $packyApiKey = (Get-Content -LiteralPath $resolvedKeyFile -Raw).Trim()
    if ([string]::IsNullOrWhiteSpace($packyApiKey)) {
        Write-Error "The Packy key file is empty."
        exit 1
    }
    Set-MindDuetCredential -Target "MindDuet.AgentKit.PackyApiKey" -UserName "Packy API" -Secret $packyApiKey
    $packyApiKey = $null
    Write-Output "Saved Packy key in Windows Credential Manager"
}

$userPath = [Environment]::GetEnvironmentVariable("Path", "User")
$pathEntries = @($userPath -split ";" | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
if ($pathEntries -notcontains $installDirectory) {
    $newUserPath = (($pathEntries + $installDirectory) -join ";")
    [Environment]::SetEnvironmentVariable("Path", $newUserPath, "User")
    Write-Output "Added Agent Kit to the user PATH; restart terminals to use the short commands"
} else {
    Write-Output "Agent Kit is already in the user PATH"
}

Write-Output "Installed MindDuet Agent Kit to: $installDirectory"
