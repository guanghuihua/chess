param(
    [string]$PythonPath = "E:\Anaconda\envs\chess\python.exe"
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$engineDirectory = Join-Path $projectRoot "engines\pikafish"
$temporaryDirectory = Join-Path ([System.IO.Path]::GetTempPath()) ("pikafish-install-" + [guid]::NewGuid())
$archivePath = Join-Path $temporaryDirectory "Pikafish.2026-01-02.7z"
$extractedPath = Join-Path $temporaryDirectory "extracted"
$downloadUrl = "https://github.com/official-pikafish/Pikafish/releases/download/Pikafish-2026-01-02/Pikafish.2026-01-02.7z"

try {
    if (-not (Test-Path -LiteralPath $PythonPath)) {
        throw "Python was not found at $PythonPath"
    }

    New-Item -ItemType Directory -Force -Path $temporaryDirectory, $extractedPath, $engineDirectory | Out-Null
    Write-Host "Downloading the official Pikafish release..."
    Invoke-WebRequest -Uri $downloadUrl -OutFile $archivePath

    & $PythonPath -c "import py7zr" 2>$null
    if ($LASTEXITCODE -ne 0) {
        & $PythonPath -m pip install py7zr
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to install py7zr"
        }
    }

    & $PythonPath -m py7zr x $archivePath $extractedPath
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to extract the Pikafish release"
    }

    Copy-Item -LiteralPath (Join-Path $extractedPath "Windows\pikafish-avx2.exe") `
              -Destination (Join-Path $engineDirectory "pikafish.exe") -Force
    Copy-Item -LiteralPath (Join-Path $extractedPath "pikafish.nnue") `
              -Destination (Join-Path $engineDirectory "pikafish.nnue") -Force
    Copy-Item -LiteralPath (Join-Path $extractedPath "Copying.txt") `
              -Destination (Join-Path $engineDirectory "Copying.txt") -Force
    Copy-Item -LiteralPath (Join-Path $extractedPath "NNUE-License.md") `
              -Destination (Join-Path $engineDirectory "NNUE-License.md") -Force

    Write-Host "Pikafish was installed in $engineDirectory"
}
finally {
    if (Test-Path -LiteralPath $temporaryDirectory) {
        Remove-Item -LiteralPath $temporaryDirectory -Recurse -Force
    }
}
