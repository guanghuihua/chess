param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$CodexArguments
)

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

    throw "Codex CLI was not found in PATH, the Codex desktop installation, or the npm global bin directory."
}

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$keyFile = Join-Path $projectRoot "APIKEY"

if (-not (Test-Path -LiteralPath $keyFile)) {
    Write-Error "Packy API key file not found: $keyFile"
    Write-Host "Create APIKEY in the project root and put only the API key in that file."
    exit 1
}

$packyApiKey = (Get-Content -LiteralPath $keyFile -Raw).Trim()
if ([string]::IsNullOrWhiteSpace($packyApiKey)) {
    Write-Error "The APIKEY file is empty."
    exit 1
}

$codexExecutable = Find-CodexExecutable
$previousPackyApiKey = $env:PACKY_API_KEY
try {
    $env:PACKY_API_KEY = $packyApiKey
    & $codexExecutable -p packy -C $projectRoot @CodexArguments
    $codexExitCode = $LASTEXITCODE
} finally {
    if ($null -eq $previousPackyApiKey) {
        Remove-Item Env:PACKY_API_KEY -ErrorAction SilentlyContinue
    } else {
        $env:PACKY_API_KEY = $previousPackyApiKey
    }
    $packyApiKey = $null
}

exit $codexExitCode
