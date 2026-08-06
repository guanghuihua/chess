param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$CodexArguments
)

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

$previousPackyApiKey = $env:PACKY_API_KEY
try {
    $env:PACKY_API_KEY = $packyApiKey
    & codex -p packy -C $projectRoot @CodexArguments
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
